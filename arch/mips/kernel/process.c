/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 1999, 2000 by Ralf Baechle and others.
 * Copyright (C) 2005, 2006 by Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 * Copyright (C) 2004 Thiemo Seufer
 */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/tick.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/personality.h>
#include <linux/sys.h>
#include <linux/user.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/kallsyms.h>
#include <linux/random.h>

#include <asm/asm.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/dsp.h>
#include <asm/fpu.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/mipsregs.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/elf.h>
#include <asm/isadep.h>
#include <asm/inst.h>
#include <asm/stacktrace.h>
#ifdef CONFIG_AVM_POWERMETER
#include <linux/avm_power.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
#include <atheros.h>
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/

/*--- #define STACK_TRC(args...) printk(KERN_INFO args) ---*/
#define STACK_TRC(args...)
extern unsigned int page_faultlink_max[NR_CPUS];
extern unsigned int page_faultlink_sum[NR_CPUS];
extern unsigned int page_faultlink_cnt[NR_CPUS];
/*--------------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------------*/
static struct _cpu_idlecount {
    unsigned int last_value;
    unsigned long last_value_done;
    unsigned int wait_count;
    unsigned int run_count;
    unsigned int sum_count;
    unsigned int show;
    unsigned int fullrun;
    unsigned int actrun;
    unsigned int fulldisp_jiffies;
} cpu_idlecount[NR_CPUS];

extern void r4k_wait_irqoff(void);
#define COUNT_DIFF(act, last)   ((act) - (last))
/*--------------------------------------------------------------------------------*\
 * liefert ob diese CPU im IDLE-Mode
\*--------------------------------------------------------------------------------*/
unsigned int cpu_idle_state(int vpe) {
    struct _cpu_idlecount *pcpuidle = &cpu_idlecount[vpe];
    return !pcpuidle->last_value_done;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void cpu_wait_start(void) {
    unsigned int count, tmp;
    struct _cpu_idlecount *pcpuidle = &cpu_idlecount[smp_processor_id()];

    count = get_cycles();
    if(pcpuidle->last_value) {
        tmp = COUNT_DIFF(count, pcpuidle->last_value);
        pcpuidle->run_count += tmp;
        pcpuidle->sum_count += tmp;
    }
    pcpuidle->last_value      = count;
    pcpuidle->last_value_done = 0;
    if(pcpuidle->fulldisp_jiffies == 0) {
        pcpuidle->fulldisp_jiffies = jiffies;
    }
}
#ifdef CONFIG_AVM_POWERMETER
static struct _hist_task_times {
    unsigned long time_last;
    unsigned int pid;
    unsigned int mark;
} hist_task_times[180];

#if defined(CONFIG_AR9)
#define CPU_TIMEOUT_10S  (394 / 2)
#elif defined(CONFIG_LANTIQ)
#define CPU_TIMEOUT_10S  (500 / 2)
#elif defined(CONFIG_MACH_ATHEROS)
#define CPU_TIMEOUT_10S  (700 / 2)
#else /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/
#define CPU_TIMEOUT_10S  (360 / 2)
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_MIPS_FUSIV) ---*/

#define ARRAY_EL(a) (sizeof(a) / sizeof(a[0]))
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _hist_task_times *get_histtask_by_pid(unsigned int pid, unsigned int *cache_idx) {
    register unsigned int i;
    if(*cache_idx >= ARRAY_EL(hist_task_times)) {
        *cache_idx = 0;
    }
    for(i = *cache_idx; i < ARRAY_EL(hist_task_times); i++) {
        if(hist_task_times[i].pid == pid) {
            *cache_idx = i + 1;
            hist_task_times[i].mark = 2;
            return &hist_task_times[i];
        }
    }
    for(i = 0; i < *cache_idx; i++) {
        if(hist_task_times[i].pid == pid) {
            *cache_idx = i + 1;
            hist_task_times[i].mark = 2;
            return &hist_task_times[i];
        }
    }
    /*--- Create ---*/
    for(i = 0; i < ARRAY_EL(hist_task_times); i++) {
        if(hist_task_times[i].mark == 0) {
            hist_task_times[i].mark = 2;
            hist_task_times[i].time_last = 0;
            hist_task_times[i].pid  = pid;
            return &hist_task_times[i];
        } 
    }
    /*--- printk("<error pid %d>", pid); ---*/
    return NULL;
}
struct _accumulate_output {
    unsigned long sum_time;
    unsigned long curr_time;
    unsigned long maxrun_time;
    struct task_struct *maxrun_task;
    unsigned int tasks;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void accumulate_time(struct _accumulate_output *ao) {
    struct task_struct *g, *p, *maxrun_task = current;
    register unsigned int i;
    unsigned int cache_idx = 0;
    register unsigned long time_sum = 0;
    register unsigned long maxrun_time = 0;
    unsigned int tsks = 0;
    ao->maxrun_task = current;
    ao->curr_time   = 0;
	do_each_thread(g, p) {
        struct _hist_task_times *pht = get_histtask_by_pid(p->pid, &cache_idx);
        tsks++;
        if(pht) {
            unsigned long time_act = task_stime(p) + task_utime(p);
            time_sum += time_act - pht->time_last;
            if(maxrun_time < (time_act - pht->time_last)) {
                maxrun_time = (time_act - pht->time_last);
                maxrun_task = p;
            }
            if(unlikely(p == current)){
                ao->curr_time = time_act - pht->time_last;
            }
            pht->time_last = time_act;
        } else {
            ao->tasks    = tsks;
            ao->sum_time = 0;
            return;
        }
	} while_each_thread(g, p);

    for(i = 0; i < ARRAY_EL(hist_task_times); i++) {
        struct _hist_task_times *pht = &hist_task_times[i];
        if(pht->mark) {
            pht->mark--; /*--- um geloeschte Eintraege frei zu bekommen  ---*/
        }
    }
    ao->maxrun_task = maxrun_task;
    ao->maxrun_time = maxrun_time;
    ao->sum_time    = time_sum | 1;
    ao->tasks       = tsks;
}
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/
/*--------------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------------*/
int cpu_wait_end(void) {
    unsigned int cpu =  smp_processor_id();
    unsigned int cpu_run = 0;
    struct _cpu_idlecount *pcpuidle = &cpu_idlecount[cpu];

    if(test_and_set_bit(0, &pcpuidle->last_value_done) == 0) {
        int count, tmp;
        count = get_cycles();
        tmp = COUNT_DIFF(count, pcpuidle->last_value);
        pcpuidle->wait_count += tmp;
        pcpuidle->sum_count  += tmp;
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
        {
            tmp =  ath_workaround_nmi_wastetime();
            if(pcpuidle->wait_count > tmp) {
                pcpuidle->wait_count -= tmp;
            } else {
                pcpuidle->wait_count = 0;
            }
        }
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
        pcpuidle->last_value  = count;
        return 0;
    }
#ifdef CONFIG_AVM_POWERMETER
#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)

    if((COUNT_DIFF(get_cycles(), pcpuidle->last_value) > (1000 * 1000 /* usec */ * CPU_TIMEOUT_10S))) {
        register unsigned long j_diff;
        unsigned long curr_percent = 0, max_percent = 0, sum_percent = 0, pg_fault_cnt, pg_fault_avg, pg_fault_max; 
        pcpuidle->last_value       = get_cycles();
        if(pcpuidle->fullrun == 0) {
            unsigned int i;
            pcpuidle->actrun = 100;
            for(i = 0; i < NR_CPUS; i++) {
                struct _cpu_idlecount *pcpuidle = &cpu_idlecount[i];
                cpu_run |= pcpuidle->actrun << (i * 8);
            }
            PowerManagmentRessourceInfo(powerdevice_loadrate, cpu_run);
        }
        pcpuidle->fullrun++;
        j_diff = COUNT_DIFF(jiffies, pcpuidle->fulldisp_jiffies);
        if(j_diff >= (10 * HZ)) {
            struct _accumulate_output ao;
            unsigned long avnrun[3];
            pcpuidle->fulldisp_jiffies = jiffies;
            accumulate_time(&ao);
            get_avenrun(avnrun, FIXED_1/200, 0);
            if(ao.sum_time) {
                /*--- Promillewert im Verhaeltnis zu allen Tasks ---*/
                curr_percent = (ao.curr_time * 100)   / j_diff;
                max_percent  = (ao.maxrun_time * 100) / j_diff;
                sum_percent  = (ao.sum_time * 100)    / j_diff;
            }

            pg_fault_cnt = page_faultlink_cnt[cpu];
            pg_fault_max = page_faultlink_max[cpu];
            if(pg_fault_cnt) {
                pg_fault_avg       = (page_faultlink_sum[cpu] * 10) / pg_fault_cnt;
                page_faultlink_sum[cpu] = 0;
                page_faultlink_cnt[cpu] = 0;
                page_faultlink_max[cpu] = 0;
            } else {
                pg_fault_avg = 0;
            }
            printk(KERN_WARNING"[%u]system-load %d %s loadavg %lu.%lu %lu.%lu %lu.%lu - %d tasks:%lu %% curr:%s(%lu %%) max:%s(%lu %%, pid:%d), readytorun: %ld"
                    ", pgfault %lu/s (max %lu avg %lu.%lu)"
                    "\n", 
                    cpu,
                    pcpuidle->fullrun >= 10 ? 100 : pcpuidle->fullrun,
                    pcpuidle->fullrun >= 10 ? "%" : "",
                    LOAD_INT(avnrun[0]), LOAD_FRAC(avnrun[0]),
                    LOAD_INT(avnrun[1]), LOAD_FRAC(avnrun[1]),
                    LOAD_INT(avnrun[2]), LOAD_FRAC(avnrun[2]),
                    ao.tasks,
                    sum_percent,
                    current->comm,
                    curr_percent,
                    ao.maxrun_task->comm,
                    max_percent,
                    ao.maxrun_task->pid,
                    nr_running(),
                    pg_fault_cnt / (j_diff / HZ),
                    pg_fault_max,
                    pg_fault_avg / 10, pg_fault_avg % 10
                    );
            pcpuidle->fullrun = 0;
        }
        /*--- printk("<R>"); ---*/
    }
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/
    return 1;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void cpu_wait_info(void) {
    unsigned int cpu =  smp_processor_id();
    struct _cpu_idlecount *pcpuidle = &cpu_idlecount[cpu];
    register unsigned int sum_count =  pcpuidle->sum_count;
    unsigned int cpu_run = 0;
    if(sum_count > (1 << 29)) {
#if defined(CONFIG_AVM_POWERMETER)
        unsigned int i;
        int p_idle = ((pcpuidle->wait_count >> 8) * 100) / (pcpuidle->sum_count >> 8);
        pcpuidle->actrun = max(0, 100 - p_idle);

        for(i = 0; i < NR_CPUS; i++) {
            struct _cpu_idlecount *pcpuidle = &cpu_idlecount[i];
            cpu_run |= pcpuidle->actrun << (i * 8);
        }
        PowerManagmentRessourceInfo(powerdevice_loadrate, cpu_run);
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/
#if defined(CONFIG_SMP) & 0
        {
        int p_run  = ((pcpuidle->run_count >> 8) * 100) / (sum_count >> 8);
        int p_idle = ((pcpuidle->wait_count >> 8) * 100) / (pcpuidle->sum_count >> 8);
        printk(KERN_INFO"[idle:cpu%u]: %u%% (run %u%%) cpu_run %x\n", cpu, p_idle, p_run, cpu_run);
        }
#endif/*--- #if defined(CONFIG_SMP) ---*/
        pcpuidle->sum_count  >>= 8;
        pcpuidle->wait_count >>= 8;
        pcpuidle->run_count  >>= 8;
    }
}

/*
 * The idle thread. There's no useful work to be done, so just try to conserve
 * power and have a low exit latency (ie sit in a loop waiting for somebody to
 * say that they'd like to reschedule)
 */
void __noreturn cpu_idle(void)
{
	int cpu;

	/* CPU is going idle. */
	cpu = smp_processor_id();

	/* endless idle loop with no priority at all */
	while (1) {
		tick_nohz_stop_sched_tick(1);
		while (!need_resched() && cpu_online(cpu)) {
            if(cpu_wait != r4k_wait_irqoff) {
                /*--- in r4k_wait_irqoff ---*/
                cpu_wait_start();
            }
#if defined(CONFIG_AVM_POWERMETER) && defined(DECTSYNC_PATCH)
            PowerManagmentRessourceInfo(powerdevice_dectsync, 0);
#endif/*--- #if defined(CONFIG_AVM_POWERMETER) && defined(DECTSYNC_PATCH) ---*/
#ifdef CONFIG_MIPS_MT_SMTC
            {
                extern void smtc_idle_loop_hook(void);
                smtc_idle_loop_hook();
            }
#endif
			if (cpu_wait)
				(*cpu_wait)();
            if(cpu_wait != r4k_wait_irqoff) {
                /*--- in r4k_wait_irqoff ---*/
                cpu_wait_end();
            }
		}
#ifdef CONFIG_HOTPLUG_CPU
		if (!cpu_online(cpu) && !cpu_isset(cpu, cpu_callin_map) &&
		    (system_state == SYSTEM_RUNNING ||
		     system_state == SYSTEM_BOOTING))
			play_dead();
#endif
        cpu_wait_info();
		tick_nohz_restart_sched_tick();
		preempt_enable_no_resched();
		schedule();
		preempt_disable();
	}
}

asmlinkage void ret_from_fork(void);

void start_thread(struct pt_regs * regs, unsigned long pc, unsigned long sp)
{
	unsigned long status;

	/* New thread loses kernel privileges. */
	status = regs->cp0_status & ~(ST0_CU0|ST0_CU1|ST0_FR|KU_MASK);
#ifdef CONFIG_64BIT
	status |= test_thread_flag(TIF_32BIT_REGS) ? 0 : ST0_FR;
#endif
	status |= KU_USER;
	regs->cp0_status = status;
	clear_used_math();
	clear_fpu_owner();
	if (cpu_has_dsp)
		__init_dsp();
	regs->cp0_epc = pc;
	regs->regs[29] = sp;
	current_thread_info()->addr_limit = USER_DS;
}

void exit_thread(void)
{
}

void flush_thread(void)
{
}

int copy_thread(unsigned long clone_flags, unsigned long usp,
	unsigned long unused, struct task_struct *p, struct pt_regs *regs)
{
	struct thread_info *ti = task_thread_info(p);
	struct pt_regs *childregs;
	unsigned long childksp;
	p->set_child_tid = p->clear_child_tid = NULL;

	childksp = (unsigned long)task_stack_page(p) + THREAD_SIZE - 32;

	preempt_disable();

	if (is_fpu_owner())
		save_fp(p);

	if (cpu_has_dsp)
		save_dsp(p);

	preempt_enable();

	/* set up new TSS. */
	childregs = (struct pt_regs *) childksp - 1;
	/*  Put the stack after the struct pt_regs.  */
	childksp = (unsigned long) childregs;
	*childregs = *regs;
	childregs->regs[7] = 0;	/* Clear error flag */

	childregs->regs[2] = 0;	/* Child gets zero as return value */
	regs->regs[2] = p->pid;

	if (childregs->cp0_status & ST0_CU0) {
		childregs->regs[28] = (unsigned long) ti;
		childregs->regs[29] = childksp;
		ti->addr_limit = KERNEL_DS;
	} else {
		childregs->regs[29] = usp;
		ti->addr_limit = USER_DS;
	}
	p->thread.reg29 = (unsigned long) childregs;
	p->thread.reg31 = (unsigned long) ret_from_fork;

	/*
	 * New tasks lose permission to use the fpu. This accelerates context
	 * switching for most programs since they don't use the fpu.
	 */
	p->thread.cp0_status = read_c0_status() & ~(ST0_CU2|ST0_CU1);
	childregs->cp0_status &= ~(ST0_CU2|ST0_CU1);

#ifdef CONFIG_MIPS_MT_SMTC
	/*
	 * SMTC restores TCStatus after Status, and the CU bits
	 * are aliased there.
	 */
	childregs->cp0_tcstatus &= ~(ST0_CU2|ST0_CU1);
#endif
	clear_tsk_thread_flag(p, TIF_USEDFPU);

#ifdef CONFIG_MIPS_MT_FPAFF
	clear_tsk_thread_flag(p, TIF_FPUBOUND);
#endif /* CONFIG_MIPS_MT_FPAFF */

	if (clone_flags & CLONE_SETTLS)
		ti->tp_value = regs->regs[7];

	return 0;
}

/* Fill in the fpu structure for a core dump.. */
int dump_fpu(struct pt_regs *regs, elf_fpregset_t *r)
{
	memcpy(r, &current->thread.fpu, sizeof(current->thread.fpu));

	return 1;
}

void elf_dump_regs(elf_greg_t *gp, struct pt_regs *regs)
{
	int i;

	for (i = 0; i < EF_R0; i++)
		gp[i] = 0;
	gp[EF_R0] = 0;
	for (i = 1; i <= 31; i++)
		gp[EF_R0 + i] = regs->regs[i];
	gp[EF_R26] = 0;
	gp[EF_R27] = 0;
	gp[EF_LO] = regs->lo;
	gp[EF_HI] = regs->hi;
	gp[EF_CP0_EPC] = regs->cp0_epc;
	gp[EF_CP0_BADVADDR] = regs->cp0_badvaddr;
	gp[EF_CP0_STATUS] = regs->cp0_status;
	gp[EF_CP0_CAUSE] = regs->cp0_cause;
#ifdef EF_UNUSED0
	gp[EF_UNUSED0] = 0;
#endif
}

int dump_task_regs(struct task_struct *tsk, elf_gregset_t *regs)
{
	elf_dump_regs(*regs, task_pt_regs(tsk));
	return 1;
}

int dump_task_fpu(struct task_struct *t, elf_fpregset_t *fpr)
{
	memcpy(fpr, &t->thread.fpu, sizeof(current->thread.fpu));

	return 1;
}

/*
 * Create a kernel thread
 */
static void __noreturn kernel_thread_helper(void *arg, int (*fn)(void *))
{
	do_exit(fn(arg));
}

long kernel_thread(int (*fn)(void *), void *arg, unsigned long flags)
{
	struct pt_regs regs;

	memset(&regs, 0, sizeof(regs));

	regs.regs[4] = (unsigned long) arg;
	regs.regs[5] = (unsigned long) fn;
	regs.cp0_epc = (unsigned long) kernel_thread_helper;
	regs.cp0_status = read_c0_status();
#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX)
	regs.cp0_status = (regs.cp0_status & ~(ST0_KUP | ST0_IEP | ST0_IEC)) |
			  ((regs.cp0_status & (ST0_KUC | ST0_IEC)) << 2);
#else
	regs.cp0_status |= ST0_EXL;
#endif

	/* Ok, create the new process.. */
	return do_fork(flags | CLONE_VM | CLONE_UNTRACED, 0, &regs, 0, NULL, NULL);
}

/*
 *
 */
struct mips_frame_info {
	void		*func;
	unsigned long	func_size;
	int		frame_size;
	int		pc_offset;
};

static inline int is_ra_save_ins(union mips_instruction *ip)
{
	/* sw / sd $ra, offset($sp) */
	return (ip->i_format.opcode == sw_op || ip->i_format.opcode == sd_op) &&
		ip->i_format.rs == 29 &&
		ip->i_format.rt == 31;
}

static inline int is_jal_jalr_jr_ins(union mips_instruction *ip)
{
	if (ip->j_format.opcode == jal_op)
		return 1;
	if (ip->r_format.opcode != spec_op)
		return 0;
	return ip->r_format.func == jalr_op || ip->r_format.func == jr_op;
}

static inline int is_sp_move_ins(union mips_instruction *ip)
{
	/* addiu/daddiu sp,sp,-imm */
	if (ip->i_format.rs != 29 || ip->i_format.rt != 29)
		return 0;
	if (ip->i_format.opcode == addiu_op || ip->i_format.opcode == daddiu_op)
		return 1;
	return 0;
}
static inline int is_sp_addiu_ins(union mips_instruction *ip)
{
	/* addiu/daddiu sp,sp,-imm */
	if (ip->i_format.rs != 29 || ip->i_format.rt != 29)
		return 0;
	if (ip->i_format.opcode == addiu_op || ip->i_format.opcode == daddiu_op)
		return 1;
	return 0;
}
static inline int is_sp_save_ins(union mips_instruction *ip)
{
	/* move reg, sp */
	if (ip->r_format.opcode != spec_op) {
		return 0;
    }
	if (ip->r_format.func != addu_op) {
		return 0;
    }
	if ((ip->r_format.rt == 29) && (ip->r_format.rs == 0)) {
		return ip->r_format.rd;
    }
	if ((ip->r_format.rt == 0) && (ip->r_format.rs == 29)) {
		return ip->r_format.rd;
    }
	return 0;
}
static inline int is_sp_restore_ins(union mips_instruction *ip)
{
	/* move reg, sp */
	if (ip->r_format.opcode != spec_op) {
		return 0;
    }
	if (ip->r_format.rd != 29)
		return 0;
	if (ip->r_format.func == addu_op) {
		return ip->r_format.rt;
    }
	return 0;
}
/*--------------------------------------------------------------------------------*\
 * mbahr@avm.de: special handling for "again-modify-stackpointer-code" (e.g. if CONFIG_FTRACE=y)
 *
    80008614:	27bdffe8 	addiu	sp,sp,-24
    80008618:	afbe0010 	sw	s8,16(sp)   
    8000861c:	03a0f021 	move	s8,sp           <--- sp saved in local register !!!
    80008620:	afbf0014 	sw	ra,20(sp)
    80008624:	03e00821 	move	at,ra
    80008628:	0c0078f8 	jal	8001e3e0 <ftrace_caller>
    8000862c:	27bdfff8 	addiu	sp,sp,-8        <--- modified stackpointer again !!!
    
    
    ....  


    80008664:	03c0e821 	move	sp,s8           <--- restore sp form local register
    80008668:	8fbf0014 	lw	ra,20(sp)
    8000866c:	24020001 	li	v0,1
    80008670:	8fbe0010 	lw	s8,16(sp)
    80008674:	03e00008 	jr	ra
    80008678:	27bd0018 	addiu	sp,sp,24
\*--------------------------------------------------------------------------------*/

static int get_frame_info(struct mips_frame_info *info)
{
	union mips_instruction *ip = info->func;
	unsigned max_insns = info->func_size / sizeof(union mips_instruction);
	unsigned i;
    unsigned int saved_sp = 0, extra_sp_offset = 0;

	info->pc_offset = -1;
	info->frame_size = 0;


	if (!ip) {
        STACK_TRC("%s: err: %d\n", __func__, __LINE__);
		goto err;
    }
	if (max_insns == 0)
		max_insns = 128U;	/* unknown function size */
	max_insns = min(128U, max_insns);

	for (i = 0; i < max_insns; i++, ip++) {
        STACK_TRC("%s: instruction %lx: %08lx\n", __func__, (unsigned long)ip, *((unsigned long *)ip));
		if (is_jal_jalr_jr_ins(ip)) {
            STACK_TRC("%s: %d\n", __func__, __LINE__);
			break;
        }
		if (!info->frame_size) {
			if (is_sp_addiu_ins(ip)) {
				info->frame_size = - ip->i_format.simmediate;
                STACK_TRC("%s: set framsize=%d (simmediate=%d)\n", __func__,  info->frame_size, ip->i_format.simmediate);
            }
			continue;
		}
		if(info->frame_size && (saved_sp == 0) && (saved_sp = is_sp_save_ins(ip))) {
            /*--- if FTRACE configured arrrgh: stackpointer saved in local register, following code will again modify stack ... ---*/
            STACK_TRC("%s: %p:instruction=%08lx arrgh stackpointer saved in local register $%d\n", __func__, ip, *((unsigned long *)ip), saved_sp);
			continue;
        }
		if ((info->pc_offset == -1) && is_ra_save_ins(ip)) {
			info->pc_offset = ip->i_format.simmediate / sizeof(long);
            STACK_TRC("%s: pc_offset %d simmediate=%d\n", __func__, info->pc_offset, ip->i_format.simmediate);
            if(saved_sp == 0) {
                break;
            }
            continue;
		}
        if(saved_sp && is_sp_addiu_ins(ip)) {
            extra_sp_offset += -ip->i_format.simmediate;
            STACK_TRC("%s: stackpointer post-add : %d extra_sp_offset=%d\n", __func__, -ip->i_format.simmediate, extra_sp_offset);
            continue;
        }
        if(saved_sp && (saved_sp == is_sp_restore_ins(ip))) {
            STACK_TRC("%s: stackpointer restored\n", __func__);
            /*--- stackpointer restored ---*/
            extra_sp_offset = 0;
            saved_sp = 0;
        }
	}
    info->pc_offset  += extra_sp_offset / sizeof(long);
	info->frame_size += extra_sp_offset;
	if (info->frame_size && info->pc_offset >= 0) {/* nested */
        STACK_TRC("%s: nested: size %d offset %d extra_sp_offset %d\n", __func__, info->frame_size, info->pc_offset, extra_sp_offset);
		return 0;
    }
	if (info->pc_offset < 0) {/* leaf */
        STACK_TRC("%s: offset %d\n", __func__, info->pc_offset);
		return 1;
    }
	/* prologue seems boggus... */
err:
	return -1;
}

static struct mips_frame_info schedule_mfi __read_mostly;

static int __init frame_info_init(void)
{
	unsigned long size = 0;
#ifdef CONFIG_KALLSYMS
	unsigned long ofs;

	kallsyms_lookup_size_offset((unsigned long)schedule, &size, &ofs);
#endif
	schedule_mfi.func = schedule;
	schedule_mfi.func_size = size;

	get_frame_info(&schedule_mfi);

	/*
	 * Without schedule() frame info, result given by
	 * thread_saved_pc() and get_wchan() are not reliable.
	 */
	if (schedule_mfi.pc_offset < 0)
		printk(KERN_WARNING"Can't analyze schedule() prologue at %p\n", schedule);

	return 0;
}

arch_initcall(frame_info_init);

/*
 * Return saved PC of a blocked thread.
 */
unsigned long thread_saved_pc(struct task_struct *tsk)
{
	struct thread_struct *t = &tsk->thread;

	/* New born processes are a special case */
	if (t->reg31 == (unsigned long) ret_from_fork)
		return t->reg31;
	if (schedule_mfi.pc_offset < 0)
		return 0;
	return ((unsigned long *)t->reg29)[schedule_mfi.pc_offset];
}


#ifdef CONFIG_KALLSYMS
/* used by show_backtrace() */
unsigned long unwind_stack(struct task_struct *task, unsigned long *sp,
			   unsigned long pc, unsigned long *ra)
{
	unsigned long stack_page;
	struct mips_frame_info info;
	unsigned long size, ofs;
	int leaf;
	extern void ret_from_irq(void);
	extern void ret_from_exception(void);

    STACK_TRC("%s %d: %p sp=%lx pc=%lx ra=%lx\n", __FUNCTION__, __LINE__, task, *sp, pc, *ra);
    if(task == NULL) {
        stack_page = *sp; /*--- lazy safety ... ---*/
    } else {
        stack_page = (unsigned long)task_stack_page(task);
    }
	if (!stack_page) {
        STACK_TRC("%s %d: %p\n", __FUNCTION__, __LINE__, task);
		return 0;
    }
	/*
	 * If we reached the bottom of interrupt context,
	 * return saved pc in pt_regs.
	 */
	if (pc == (unsigned long)ret_from_irq ||
	    pc == (unsigned long)ret_from_exception) {
		struct pt_regs *regs;
		if (*sp >= stack_page &&
		    *sp + sizeof(*regs) <= stack_page + THREAD_SIZE - 32) {
			regs = (struct pt_regs *)*sp;
			pc = regs->cp0_epc;
			if (__kernel_text_address(pc)) {
				*sp = regs->regs[29];
				*ra = regs->regs[31];
                STACK_TRC("%s %d: %p sp=%lx pc=%lx ra=%lx\n", __FUNCTION__, __LINE__, task, *sp, pc, *ra);
				return pc;
			}
		}
        STACK_TRC("%s %d: %lx %d\n", __FUNCTION__, __LINE__, pc, __kernel_text_address(pc));
		return 0;
	}
	if (!kallsyms_lookup_size_offset(pc, &size, &ofs)) {
        STACK_TRC("%s %d:\n", __FUNCTION__, __LINE__);
		return 0;
    }
	/*
	 * Return ra if an exception occured at the first instruction
	 */
	if (unlikely(ofs == 0)) {
		pc = *ra;
		*ra = 0;
        STACK_TRC("%s %d:\n", __FUNCTION__, __LINE__);
		return pc;
	}

    STACK_TRC("%s %d: pc=%lx ofs=%lx\n", __FUNCTION__, __LINE__, pc, ofs);
	info.func = (void *)(pc - ofs);
	info.func_size = ofs;	/* analyze from start to ofs */
	leaf = get_frame_info(&info);
	if (leaf < 0) {
        STACK_TRC("%s %d:\n", __FUNCTION__, __LINE__);
		return 0;
    }
	if (*sp < stack_page ||
	    *sp + info.frame_size > stack_page + THREAD_SIZE - 32) {
        STACK_TRC("%s %d:\n", __FUNCTION__, __LINE__);
		return 0;
    }
	if (leaf) {
		/*
		 * For some extreme cases, get_frame_info() can
		 * consider wrongly a nested function as a leaf
		 * one. In that cases avoid to return always the
		 * same value.
		 */
		pc = pc != *ra ? *ra : 0;
    } else {
		pc = ((unsigned long *)(*sp))[info.pc_offset];
        STACK_TRC("%s %d: pc %lx pc_offset = %d\n", __FUNCTION__, __LINE__, pc, info.pc_offset);
    }
	*sp += info.frame_size;
	*ra = 0;
    STACK_TRC("%s %d: %lx %d\n", __FUNCTION__, __LINE__, pc, __kernel_text_address(pc));
	return __kernel_text_address(pc) ? pc : 0;
}
#endif

/*
 * get_wchan - a maintenance nightmare^W^Wpain in the ass ...
 */
unsigned long get_wchan(struct task_struct *task)
{
	unsigned long pc = 0;
#ifdef CONFIG_KALLSYMS
	unsigned long sp;
	unsigned long ra = 0;
#endif

	if (!task || task == current || task->state == TASK_RUNNING)
		goto out;
	if (!task_stack_page(task))
		goto out;

	pc = thread_saved_pc(task);

#ifdef CONFIG_KALLSYMS
	sp = task->thread.reg29 + schedule_mfi.frame_size;

	while (in_sched_functions(pc))
		pc = unwind_stack(task, &sp, pc, &ra);
#endif

out:
	return pc;
}

/*
 * Don't forget that the stack pointer must be aligned on a 8 bytes
 * boundary for 32-bits ABI and 16 bytes for 64-bits ABI.
 */
unsigned long arch_align_stack(unsigned long sp)
{
	if (!(current->personality & ADDR_NO_RANDOMIZE) && randomize_va_space)
		sp -= get_random_int() & ~PAGE_MASK;

	return sp & ALMASK;
}
