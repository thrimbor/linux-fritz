/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2013 AVM GmbH <fritzbox_info@avm.de>
 *
 *   author: mbahr@avm.de
 *   description: yield-thread-interface mips34k
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*------------------------------------------------------------------------------------------*/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/compiler.h>
#include <linux/smp.h>
#include <linux/proc_fs.h>

#include <asm/atomic.h>
#include <asm/cacheflush.h>
#include <asm/cpu.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/hardirq.h>
#include <asm/mmu_context.h>
#include <asm/time.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/mips_mt.h>
#include <asm/yield_context.h>

#define MAX_YIELDSIGNALS        16
#define YIELDMASK               ((1 << MAX_YIELDSIGNALS) -1)

#define write_vpe_c0_yqmask(val)	mttc0(1, 4, val)
#define read_vpe_c0_yqmask(val)	    mftc0(1, 4)

#define YIELD_STAT

#define MAGIG_YIELD_STACK      0x595F5350 

#if defined(YIELD_STAT)
struct _generic_stat {
    unsigned long cnt; 
    unsigned long min;
    unsigned long max;
    unsigned long long avg;
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void init_generic_stat(struct _generic_stat *pgstat) {
    pgstat->min = LONG_MAX;
    pgstat->max = 0;
    pgstat->cnt = 0;
    pgstat->avg = 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void generic_stat(struct _generic_stat *pgstat, unsigned long val) {
    if(val > pgstat->max) pgstat->max = val;
    if(val < pgstat->min) pgstat->min = val;
    pgstat->avg += (unsigned long long)val;
    pgstat->cnt++;
}
#endif/*--- #if defined(YIELD_STAT) ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _yield_handler {
    /*--------------------------------------------------------------------------------*\
     * Funktion die im Yield-Kontext ausgefuehrt wird 
     * Achtung! kein Linux-Kontext, nur rudimentaere Zugriffe erlaubt!
    \*--------------------------------------------------------------------------------*/
    int (*yield_handler)(int signal, void *ref);
    void *ref;
    spinlock_t   progress;
    volatile unsigned long counter;
    volatile unsigned long unhandled;
    atomic_t   enable;
#if defined(YIELD_STAT)
    unsigned int last_access;
    struct _generic_stat consumption;
    struct _generic_stat latency;
#endif/*--- #if defined(YIELD_STAT) ---*/
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _yield_per_tc {
    volatile int yield_mask;
    int yield_tc;
    volatile int tc_init;
    volatile unsigned long yield_counter;
    struct thread_info yield_gp;
    unsigned int yield_sp[1024] __attribute__((aligned(8)));
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _yield_ctrl {
    volatile int yield_all_init;
    volatile int yield_all_mask;
    struct _yield_handler handler[MAX_YIELDSIGNALS]; 
    volatile struct _yield_per_tc *per_tc[YIELD_MAX_TC];
};

static struct _yield_ctrl yield_ctrl;

/*--------------------------------------------------------------------------------*\
  MIPS MT 34K-Specification:
    When the rs argument of the yield rs instruction is positive, the thread waits for a hardware condition; the thread
    will wait until the bitwise-and of rs and the hardware signal vector is non-zero. This is a cheap and efficient mecha-
    nism to get a thread to wait on the state of some input signal.

    Cores in the 34K family may have up to 16 external hardware signals attached. Because the yield instruction is
    available to user (low-privilege) software, you might not want it to have sight of all your hardware signals. The CP0
    register YQMask is a bit-field where a “1” bit marks an incoming signal as accessible to the yield instruction.
    In any OS running more threads than TCs you might want to reclaim a TC blocked on such a yield. If you need to
    do that while continuing to monitor the condition, then you’ll probably want your system integrator to ensure that the
    yield condition is also available as an interrupt, so you can get the OS’ attention when the condition happens.
    The OS can zero-out corresponding bits 0-15 of YQMask to prevent them being used - a yield rs which attempts
    to use one of those bits will result in an exception.

    In the two-operand form yield rd,rs the rd register gets a result, which is a bit-map with a 1 for every active
    yield input which is enabled by YQMask (bits which are zeroed in YQMask may return any value, don’t rely on them).
    The single-register form yield rs is really yield $0,rs.
\*--------------------------------------------------------------------------------*/
static inline unsigned int yield_events(unsigned int mask) {
	int res = 0;

	__asm__ __volatile__(
	"	.set	push						\n"
	"	.set	noreorder					\n"
	"	.set	noat						\n"
	"	.set	mips32r2					\n"
	"	nop                                 \n"
	"	yield   %0, %1                      \n"
	"	.set	pop						    \n"
	: "=r" (res)
	: "0" (mask)
    );
	return res;
}
/*--------------------------------------------------------------------------------*\
  MIPS MT 34K-Specification:
 There are very few extra instructions:

 fork rd,rs,rt: fires up a thread on a free TC (if available, see below). rs points to the instruction where the
    new thread is to start, and the new thread’s rd register gets the value from the existing thread’s rt.
    Some vital per-TC state is copied from the parent:

     TCStatus[TKSU]: whether you’re in kernel or user mode — the same as Status[KSU]);
     TCStatus[TASID]: what address space you’re part of — the same as EntryHi[ASID] ;
     UserLocal: some kind of kernel-maintained thread ID, see more in Section C.4.2 “The UserLocal register”.

    When the thread has finished its job it should use yield $0 to free up the TC again.
\*--------------------------------------------------------------------------------*/
static inline unsigned int fork(void *startaddr, void *arg) {
	int res = 0;

	__asm__ __volatile__(
	"	.set	push						\n"
	"	.set	noreorder					\n"
	"	.set	noat						\n"
	"	.set	mips32r2					\n"
	"	nop                                 \n"
	"	fork   %0, %1, %2                   \n"
	"	.set	pop						    \n"
	: "=r" (res)
	: "0" (startaddr), "r" (arg)
    );
	return res;
}
/*--------------------------------------------------------------------------------*\
 * start function in non-linux-yield-context
 *
 * ret: >= 0 number of registered signal < 0: errno
 *
 *  return of request_yield_handler() handled -> YIELD_HANDLED
\*--------------------------------------------------------------------------------*/
int request_yield_handler(int signal, int (*yield_handler)(int signal, void *ref), void *ref){
    struct _yield_ctrl *pyield_ctrl = &yield_ctrl;
    unsigned long flags;

    /*--- printk("%s: signal=%x func=%p ref=%p\n", __func__, signal, yield_handler, ref); ---*/
    if(pyield_ctrl->yield_all_init == 0) {
        return -ENODEV;
    }
    if((signal >= MAX_YIELDSIGNALS)) {
		printk(KERN_ERR "%s signal %d to large \n", __func__, signal);
        return -ERANGE;
    }
    if ((long)yield_handler < KSEG0 || (long)yield_handler >= KSEG1) {
		printk(KERN_ERR "%s only KSEG0/KSEG1 for yield_handler (%p) allowed\n", __func__, yield_handler);
        return -ERANGE;
    }
    if ((long)ref < KSEG0 || (long)ref >= KSEG1) {
		printk(KERN_ERR "%s only KSEG0/KSEG1 for ref (%p) allowed\n", __func__, ref);
        return -ERANGE;
    }
    if((pyield_ctrl->yield_all_mask & (1 << signal)) == 0)  {
		printk(KERN_ERR "%s signal %d in mask %04x not supported\n", __func__, signal, pyield_ctrl->yield_all_mask);
        return -ERANGE;
    }
    spin_lock_irqsave(&pyield_ctrl->handler[signal].progress, flags);
    if(pyield_ctrl->handler[signal].yield_handler) {
        spin_unlock_irqrestore(&pyield_ctrl->handler[signal].progress, flags);
		printk(KERN_ERR "%s signalhandler for signal %d already installed\n", __func__, signal);
        return -EBUSY;
    }
    pyield_ctrl->handler[signal].yield_handler   = yield_handler;
    pyield_ctrl->handler[signal].ref             = ref;
    pyield_ctrl->handler[signal].counter         = 0;
    pyield_ctrl->handler[signal].unhandled       = 0;
    atomic_set(&pyield_ctrl->handler[signal].enable, 1);
#if defined(YIELD_STAT)
    pyield_ctrl->handler[signal].last_access     = 0;
    init_generic_stat(&pyield_ctrl->handler[signal].consumption);
    init_generic_stat(&pyield_ctrl->handler[signal].latency);
#endif/*--- #if defined(YIELD_STAT) ---*/
    spin_unlock_irqrestore(&pyield_ctrl->handler[signal].progress, flags);
    return signal;
}
EXPORT_SYMBOL(request_yield_handler);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int free_yield_handler(int signal, void *ref){
    struct _yield_ctrl *pyield_ctrl = &yield_ctrl;
    unsigned long flags;

    if(pyield_ctrl->yield_all_init == 0) {
		printk(KERN_ERR "%s not initialized\n", __func__);
        return -ENODEV;
    }
    if((signal >= MAX_YIELDSIGNALS)) {
        return -ERANGE;
    }
    spin_lock_irqsave(&pyield_ctrl->handler[signal].progress, flags);
    if(pyield_ctrl->handler[signal].ref == ref) {
        pyield_ctrl->handler[signal].yield_handler = NULL;
        pyield_ctrl->handler[signal].ref           = NULL;
        spin_unlock_irqrestore(&pyield_ctrl->handler[signal].progress, flags);
        return 0;
    }
    spin_unlock_irqrestore(&pyield_ctrl->handler[signal].progress, flags);
    printk(KERN_ERR "%s false ref\n", __func__);
    return -ERANGE;
}
EXPORT_SYMBOL(free_yield_handler);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void disable_yield_handler(int signal){
    struct _yield_ctrl *pyield_ctrl = &yield_ctrl;
    if(pyield_ctrl->yield_all_init == 0) {
        return;
    }
    if((signal >= MAX_YIELDSIGNALS)) {
        return;
    }
    if(atomic_sub_return(1, &pyield_ctrl->handler[signal].enable) < 0){
		printk(KERN_ERR "%s warning unbalanced disable\n", __func__);
        dump_stack();
    }
}
EXPORT_SYMBOL(disable_yield_handler);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void enable_yield_handler(int signal){
    struct _yield_ctrl *pyield_ctrl = &yield_ctrl;
    if(pyield_ctrl->yield_all_init == 0) {
        return;
    }
    if((signal >= MAX_YIELDSIGNALS)) {
        return;
    }
    atomic_add(1, &pyield_ctrl->handler[signal].enable);
}
EXPORT_SYMBOL(enable_yield_handler);
extern void prom_printf(const char *, ...);    
/*--------------------------------------------------------------------------------*\
 * own non-Linux-YIELD-Kontext-Thread!
 * use __raw_spin_lock() because no error-output and error-handling allowed
\*--------------------------------------------------------------------------------*/
static void yield_context_thread(void) {
    struct _yield_handler *pyieldh;
    struct _yield_ctrl *pyield_ctrl = &yield_ctrl;
    struct _yield_per_tc *pyield_tc = NULL;
    unsigned int settings, mask, i;
    unsigned int first_signal = 0;
    unsigned int yield_predefmask;
    
#if defined(YIELD_STAT)
    unsigned int start_time;
#endif/*--- #if defined(YIELD_STAT) ---*/


    for(i = 0; i < YIELD_MAX_TC; i++) {
        if(pyield_ctrl->per_tc[i] && (pyield_ctrl->per_tc[i]->tc_init == 0)) {
            pyield_ctrl->per_tc[i]->tc_init = 1;
            pyield_tc = (struct _yield_per_tc *)pyield_ctrl->per_tc[i];
            break;
        }
    }
    /*--- prom_printf("%s %d", __func__, i); ---*/
    if(pyield_tc  == NULL) {
        /*--- big error ---*/
        yield_events(0);
        /*--- prom_printf("BIG ERROR");  ---*/
        return;
    }
    yield_predefmask = pyield_tc->yield_mask;
    mask = yield_predefmask;
    /*--- prom_printf("[%s tcstatus=%x tcbind=%x yqmask=%x(%x)]\n", __func__, read_tc_c0_tcstatus(), read_tc_c0_tcbind(), read_vpe_c0_yqmask(), mask);  ---*/
    first_signal = fls(yield_predefmask) - 1;
    for(;;) {
        unsigned int signal = first_signal;
        settings = (yield_events(mask) & yield_predefmask) >> signal;
        while(settings) {
            if(likely((settings & 0x1) == 0)) {
                signal++;
                settings >>= 1;
                continue;
            }
            pyieldh = &pyield_ctrl->handler[signal];
            pyieldh->counter++;
            __raw_spin_lock(&pyieldh->progress.raw_lock);
            if(unlikely(pyieldh->yield_handler == NULL)) {
                __raw_spin_unlock(&pyieldh->progress.raw_lock);
                signal++;
                settings >>= 1;
                continue;
            }
            if(atomic_read(&pyieldh->enable) <= 0) {
                __raw_spin_unlock(&pyieldh->progress.raw_lock);
                signal++;
                settings >>= 1;
                continue;
            }
#if defined(YIELD_STAT)
            start_time = get_cycles() | 1;
            if(pyieldh->last_access) {
                generic_stat(&pyieldh->latency, start_time - pyieldh->last_access);
            }
            pyieldh->last_access = start_time;
#endif/*--- #if defined(YIELD_STAT) ---*/
            if(pyieldh->yield_handler(signal, pyieldh->ref) != YIELD_HANDLED) {
                pyieldh->unhandled++;
            }
#if defined(YIELD_STAT)
            generic_stat(&pyieldh->consumption, get_cycles() - start_time);
#endif/*--- #if defined(YIELD_STAT) ---*/

            __raw_spin_unlock(&pyieldh->progress.raw_lock);
            signal++;
            settings >>= 1;
        }
        pyield_tc->yield_counter++;
    }
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int yield_proc_stat(char *buf, char **start, off_t offset, int count, int *eof, void *data) {
    char tmpbuf[256];
    struct _yield_ctrl *pyield_ctrl = &yield_ctrl;
    unsigned int i, tc, stack_used, *p;
    unsigned long flags;
	int idx = 0;
    if(buf == NULL) {
        buf = tmpbuf;
    }
    for(tc = 0; tc < YIELD_MAX_TC; tc++) {
        struct _yield_per_tc *pyield_tc = (struct _yield_per_tc *)pyield_ctrl->per_tc[tc];
        if(pyield_tc == NULL) {
            continue;
        }
        stack_used = sizeof(pyield_tc->yield_sp) / sizeof(unsigned int);
        p = pyield_tc->yield_sp;
        while(stack_used) {
            if(*p++ != MAGIG_YIELD_STACK) {
                break;
            }
            stack_used--;
        }
        idx += sprintf(buf+idx, "[cpu=%d tc=%d]yield: mask=0x%x trigger=%lu stack-used=%u from %u bytes%s\n", 
                pyield_tc->yield_gp.cpu,
                pyield_tc->yield_tc, pyield_tc->yield_mask, pyield_tc->yield_counter, 
                stack_used * sizeof(unsigned int), sizeof(pyield_tc->yield_sp),
                stack_used * sizeof(unsigned int) == sizeof(pyield_tc->yield_sp) ? "stack overflow!!!" : ""
                );
        if(buf == tmpbuf) printk(KERN_ERR"%s", buf), idx = 0;
        for(i = 0; i < MAX_YIELDSIGNALS; i++) {
            struct _yield_handler *pyieldh = &pyield_ctrl->handler[i]; 
#if defined(YIELD_STAT)
            struct _generic_stat *pstat;
            unsigned long cnt, max, min;
            unsigned long long avg64;
#endif/*--- #if defined(YIELD_STAT) ---*/
            if(pyieldh->yield_handler && (pyield_tc->yield_mask & (0x1 << i))) {
                idx += sprintf(buf+idx, "\t[%2d]handler: %pS enable=%d count=%lu unhandled=%lu\n",i, pyieldh->yield_handler,
                                                                           atomic_read(&pyieldh->enable),
                                                                           pyieldh->counter,
                                                                           pyieldh->unhandled
                              );
                if(buf == tmpbuf) printk(KERN_ERR"%s", buf), idx = 0;
#if defined(YIELD_STAT)
                pstat = &pyieldh->consumption;

                spin_lock_irqsave(&pyieldh->progress, flags);
                cnt = pstat->cnt;
                max = pstat->max;
                min = pstat->min;
                avg64 = pstat->avg;
                init_generic_stat(pstat);
                spin_unlock_irqrestore(&pyieldh->progress, flags);

                if(cnt) {
                    do_div(avg64, cnt);
                    idx += sprintf(buf+idx, "\t\t\tcycle-stat: [%lu]consumption: min=%lu max=%lu avg=%lu\n", cnt,
                                                   min, max, (unsigned long)avg64);
                    if(buf == tmpbuf) printk(KERN_ERR"%s", buf), idx = 0;
                }
                pstat = &pyieldh->latency;
                spin_lock_irqsave(&pyieldh->progress, flags);
                cnt = pstat->cnt;
                max = pstat->max;
                min = pstat->min;
                avg64 = pstat->avg;
                init_generic_stat(pstat);
                spin_unlock_irqrestore(&pyieldh->progress, flags);
                if(cnt) {
                    do_div(avg64, cnt);
                    idx += sprintf(buf+idx, "\t\t\tcycle-stat: [%lu]latency: min=%lu max=%lu avg=%lu\n", cnt,
                                                   min, max, (unsigned long)avg64);
                    if(buf == tmpbuf) printk(KERN_ERR"%s", buf), idx = 0;
                }
#endif/*--- #if defined(YIELD_STAT) ---*/
            }
        }
    }
	return idx;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void yield_context_dump(void){
    yield_proc_stat(NULL, NULL, 0, 0, NULL, NULL);
}
EXPORT_SYMBOL(yield_context_dump);
#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *g_yield_procdev;
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int yield_proc_init(void) {

	g_yield_procdev= proc_mkdir("yield", NULL);
	create_proc_read_entry("stat", 0, g_yield_procdev, yield_proc_stat, NULL);
	return 0;
}
#endif/*--- #ifdef CONFIG_PROC_FS ---*/

/*--------------------------------------------------------------------------------*\
TCSTATUS_TCU=S:28,31
TCSTATUS_TMX=S:27,27
TCSTATUS_RNST=S:24,23
TCSTATUS_TDS=S:21,21
TCSTATUS_DT=S:20,20
TCSTATUS_TCEE=S:17,17
TCSTATUS_DA=S:15,15
TCSTATUS_A=S:13,13
TCSTATUS_TKSU=S:12,11
TCSTATUS_IXMT=S:10,10
TCSTATUS_TASID=S:0,7


TCBIND_CurTC =S:21,28
TCBIND_CurVPE=S:17,17
TCBIND_CurVPE=S:0,3
\*--------------------------------------------------------------------------------*/
static void yield_context_init(int cpu, int yield_tc, unsigned int yield_mask) {
    struct _yield_ctrl *pyield_ctrl = &yield_ctrl;
    struct _yield_per_tc *pyield_tc;
	unsigned int long val, mvpval, old_tc, i, tc;
	unsigned long flags, time;

    printk(KERN_ERR "[%s] cpu=%x tc=%x mask=%x\n", __func__, cpu,yield_tc, yield_mask);
    if(!yield_mask) {
		printk(KERN_ERR "[%s] error yield_mask is zero\n", __func__);
		return;
	}
    if(yield_mask & pyield_ctrl->yield_all_mask) {
		printk(KERN_ERR "[%s] yield_mask over-crossed with other tc %x %x\n", __func__, yield_mask, pyield_ctrl->yield_all_mask);
		return;
	}
    for(tc = 0; tc < YIELD_MAX_TC; tc++) {
        pyield_tc = (struct _yield_per_tc *)pyield_ctrl->per_tc[tc];
        if(pyield_tc == NULL) {
            pyield_tc = kmalloc(sizeof(struct _yield_per_tc), GFP_ATOMIC);
            if(pyield_tc == NULL) {
                printk(KERN_ERR "[%s] memory error\n", __func__);
                return;
            }
            memset(pyield_tc, 0, sizeof(struct _yield_per_tc));
            for(i = 0; i < sizeof(pyield_tc->yield_sp) / sizeof(unsigned int); i++) {
                pyield_tc->yield_sp[i] =  MAGIG_YIELD_STACK;
            }
            break;
        } else {
            if(pyield_tc->yield_tc == yield_tc) {
                printk(KERN_ERR "[%s] error doubles yield_tc %d\n", __func__, yield_tc);
                return;
            }
            if(pyield_tc->yield_gp.cpu == cpu) {
                printk(KERN_ERR "[%s] error only one yield_tc per vpe %d\n", __func__, cpu);
                return;
            }
        }
    }
    if(tc == YIELD_MAX_TC) {
        printk(KERN_ERR "[%s] error no more tc-instances\n", __func__);
        return;
    }
    if(pyield_ctrl->yield_all_init == 0) {
        for(i = 0; i < MAX_YIELDSIGNALS; i++) {
            spin_lock_init(&pyield_ctrl->handler[i].progress);
        }
    }
	local_irq_save(flags);
	mips_ihb();
	val = read_c0_vpeconf0();
	if (!(val & VPECONF0_MVP)) {
		printk(KERN_ERR "[%s] error only Master VPE's are allowed to configure MT\n", __func__);
		local_irq_restore(flags);
        kfree(pyield_tc);
		return;
	}
    mvpval = dvpe();
    old_tc = read_c0_vpecontrol() & VPECONTROL_TARGTC;

    /* Yield-Thread-Context aufsetzen */    
	set_c0_mvpcontrol(MVPCONTROL_VPC);  /*--- make configuration registers writeable ---*/

	settc(yield_tc);
	write_tc_c0_tchalt(TCHALT_H);

    /*--- bind on master-vpe  vpe1 only possible if started form vpe1 ??? ---*/
    write_tc_c0_tcbind((read_tc_c0_tcbind() & ~TCBIND_CURVPE) | cpu);

	/* Write the address we want it to start running from in the TCPC register. */
	write_tc_c0_tcrestart((unsigned long)yield_context_thread);
	write_tc_c0_tccontext((unsigned long)0);

	/* stack pointer */
	write_tc_gpr_sp(((unsigned long)pyield_tc->yield_sp) + sizeof(pyield_tc->yield_sp));

    /*--- printk("%s:#-1 read_tc_c0_tcstatus=%lx\n", __func__, read_tc_c0_tcstatus()); ---*/

    pyield_tc->yield_gp.cpu = cpu;
	/* global pointer */
	write_tc_gpr_gp(&pyield_tc->yield_gp);
    
    /*--- set YieldQMask ---*/
    write_vpe_c0_yqmask(yield_mask);

    pyield_tc->yield_mask = yield_mask;
    pyield_tc->yield_tc   = yield_tc;

    pyield_ctrl->yield_all_mask |= yield_mask;
    pyield_ctrl->per_tc[tc]      = pyield_tc;

	val = read_tc_c0_tcstatus();

    /*--- printk(KERN_ERR"%s: yield_mask:%lx\n", __func__, read_vpe_c0_yqmask()); ---*/
#if 0
	val = (val & ~(TCSTATUS_A )) | TCSTATUS_DA | TCSTATUS_TMX | TCSTATUS_IXMT;
	write_tc_c0_tcstatus(val);
	clear_c0_mvpcontrol(MVPCONTROL_VPC); /*--- make configuration registers readonly ---*/
	settc(old_tc);
    fork((void *)yield_context_thread, (void *)pyield_tc);
#else
    /*--- Mark the not dynamically allocatable, TC as activated, DSP ase on, prevent interrupts ---*/
	val = (val & ~(TCSTATUS_DA )) | TCSTATUS_A | TCSTATUS_TMX | TCSTATUS_IXMT;
	write_tc_c0_tcstatus(val);
    /*--- write_c0_vpecontrol( read_c0_vpecontrol() | VPECONTROL_TE);  ---*//*--- multithreading enabled ---*/
	write_tc_c0_tchalt(read_tc_c0_tchalt() & ~TCHALT_H);
    
	/* finally out of configuration and into chaos */
	clear_c0_mvpcontrol(MVPCONTROL_VPC); /*--- make configuration registers readonly ---*/
	settc(old_tc);
#endif
	mips_ihb();

	evpe(mvpval);
    emt(EMT_ENABLE);
	mips_ihb();

    /*--- printk("%s:#1 vpecontrol=%x\n", __func__, read_c0_vpecontrol()); ---*/
    time = get_cycles();
    while(pyield_tc->tc_init == 0) {
        if((get_cycles() - time) > ((1000 /* ms */ *  1000) / 500 /* (@ 500 MHz) */ / 2 )) {
            panic("[%s] can't start tc %d\n", __func__, yield_tc);
        }
    }
    /*--- printk(KERN_INFO"[%s] tc=%d mask=0x%x done\n", __func__, yield_tc, yield_mask); ---*/
#if defined(CONFIG_PROC_FS)
    if(pyield_ctrl->yield_all_init == 0) {
        yield_proc_init();
    }
#endif/*--- #if defined(CONFIG_PROC_FS) ---*/
    pyield_ctrl->yield_all_init++;
	local_irq_restore(flags);
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _yield_on_work {
    unsigned int yield_tc;
    unsigned int yield_mask;
    struct semaphore sema;
    struct workqueue_struct *workqueue;
    struct work_struct work;
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void yield_on_startup_work(struct work_struct *data) {
    struct _yield_on_work *pwork = container_of(data, struct _yield_on_work, work);
    yield_context_init(smp_processor_id(), pwork->yield_tc, pwork->yield_mask);
    up(&pwork->sema);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline int workprocess(int cpu, struct _yield_on_work *pwork, const char *funcname) {

    sema_init(&pwork->sema, 0); /*--- nicht betreten ---*/
    if((pwork->workqueue = create_workqueue(funcname)) == NULL){
        return -ENOMEM;
    }
    INIT_WORK(&pwork->work, yield_on_startup_work);

	queue_work_on(cpu, pwork->workqueue, &pwork->work);
    down(&pwork->sema);
	destroy_workqueue(pwork->workqueue);
    return 0;
}
/*--------------------------------------------------------------------------------*\
 * cpu:      bind tc on this cpu
 * yield_tc: tc to use for yield
 * yield_mask: wich signal(s) would be catched
 *
 * actually YIELD_MAX_TC tc possible, no crossover of yield_mask allowed
\*--------------------------------------------------------------------------------*/
int yield_context_init_on(int cpu, unsigned int yield_tc, unsigned int yield_mask) {
    struct _yield_on_work yield_on_work;
    yield_on_work.yield_mask = yield_mask;
    yield_on_work.yield_tc   = yield_tc;
    return workprocess(cpu, &yield_on_work, "yield_w");
}
