/*
 * Copyright (C) 2006,2007 Felix Fietkau <nbd@openwrt.org>
 * Copyright (C) 2006,2007 Eugene Konev <ejka@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <asm/mach_avm.h>
#include <asm/mach-ur8/ur8.h>
#include <asm/mach-ur8/ur8int.h>
#include <asm/mach-ur8/hw_irq.h>
#include <asm/mach-ur8/ur8_pci.h>
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
#include <linux/avm_profile.h>
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 

#define EXCEPT_OFFSET	0x80
#define PACE_OFFSET	0xA0
#define CHNLS_OFFSET	0x200

#define REG_OFFSET(irq, reg)	((irq) / 32 * 0x4 + reg * 0x10)
#define SEC_REG_OFFSET(reg)	(EXCEPT_OFFSET + reg * 0x8)
#define SEC_SR_OFFSET		(SEC_REG_OFFSET(0))	/* 0x80 */
#define CR_OFFSET(irq)		(REG_OFFSET(irq, 1))	/* 0x10 */
#define SEC_CR_OFFSET		(SEC_REG_OFFSET(1))	/* 0x88 */
#define ESR_OFFSET(irq)		(REG_OFFSET(irq, 2))	/* 0x20 */
#define SEC_ESR_OFFSET		(SEC_REG_OFFSET(2))	/* 0x90 */
#define ECR_OFFSET(irq)		(REG_OFFSET(irq, 3))	/* 0x30 */
#define SEC_ECR_OFFSET		(SEC_REG_OFFSET(3))	/* 0x98 */
#define PIR_OFFSET		(0x40)
#define MSR_OFFSET		(0x44)
#define PM_OFFSET(irq)		(REG_OFFSET(irq, 5))	/* 0x50 */
#define TM_OFFSET(irq)		(REG_OFFSET(irq, 6))	/* 0x60 */

#define REG(addr) ((u32 *)(KSEG1ADDR(UR8_IRQ_CTRL_BASE) + addr))

#define CHNL_OFFSET(chnl) (CHNLS_OFFSET + (chnl * 4))

static int ur8_irq_base;
extern unsigned int ur8int_set_type(unsigned int irq, enum _ur8_interrupt_type_trigger level, enum _ur8_interrupt_type_polarity polarity);
static int ur8_set_type(unsigned int irq, unsigned int flow_type);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int irq_prio_stack[41], irq_prio_pointer = 0;

/*--- Index steht fuer Irqline - Wert steht fuer Irqchan(Prio) ---*/
static const int irqline_to_irqchan[40] = {
    0,  1,   2,   3,   4,   5,   6,   7,       /* Irq 0  -  7 */
   21,  9,  10,  11,  12,  13,  14,  15,       /* Irq 8  - 15 - uart-b(8+8) irq auf chan(prio) 21  */ 
   16, 17,  18,  19,  20,   8,  22,  23,       /* Irq 9  - 23 - c55-dsp-irq(21+8) auf chan(prio) 8 */ 
   24, 25,  26,  27,  28,  29,  30,  31,       /* Irq 24 - 31 */
   32, 33,  34,  35,  36,  37,  38,  39        /* Irq 32 - 39 */
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int find_irqline_by_irqchan(int prio) {
    int i;
    for(i = 0; i < sizeof(irqline_to_irqchan) / sizeof(irqline_to_irqchan[0]); i++) {
        if(irqline_to_irqchan[i] == prio) {
            /*--- printk(KERN_ERR"irq %d -> prio %d\n", i, prio); ---*/
            if(i != prio) {
                printk(KERN_ERR"[mips_cpu_irq_init]change irqline %d -> irqchan(prio) %d\n", i, prio);
            }
            return i;
        }
    }
    panic(KERN_ERR"[mips_cpu_irq_init]error: no irqline for irqchan(prio) %d\n", prio);
    return prio;
}
/*------------------------------------------------------------------------------------------*\
 * Diese Funktionen muessen geschuetzt aufgerufen werden !
\*------------------------------------------------------------------------------------------*/
static inline void priority_push(int actirqchan) {
    irq_prio_stack[irq_prio_pointer++] = actirqchan;
}
/*------------------------------------------------------------------------------------------*\
 * Diese Funktionen muesen geschuetzt aufgerufen werden !
\*------------------------------------------------------------------------------------------*/
static inline unsigned int priority_pop(void) {
    unsigned int prio;
    if(irq_prio_pointer == 0)
        prio = 40;
    else
        prio = irq_prio_stack[--irq_prio_pointer];
    return prio;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int mips_cpu_double_clear_mask[2];
static spinlock_t ur8_irq_lock = SPIN_LOCK_UNLOCKED;

static struct {
    unsigned char used;
    unsigned char irqline;
} ur8_pacing_list[4];
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ur8_unmask_irq(unsigned int irq) {
    int irqline = irq - ur8_irq_base;
    int irqchan = irqline_to_irqchan[irqline];    /*--- map irq to irqchan ---*/
	writel(1 << ((irqchan ) % 32), REG(ESR_OFFSET(irqchan)));
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void ur8_mask_irq(unsigned int irq) {
    int irqline = irq - ur8_irq_base;
    int irqchan = irqline_to_irqchan[irqline];    /*--- map irq to irqchan ---*/
	writel(1 << ((irqchan) % 32), REG(ECR_OFFSET(irqchan)));
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void ur8_ack_irq(unsigned int irq) {
    struct _irq_hw *IRQ = (struct _irq_hw *)UR8_IRQ_CTRL_BASE;
    unsigned long flags;
    int irqline = irq - ur8_irq_base;
    int irqchan = irqline_to_irqchan[irqline];    /*--- map irq to irqchan ---*/

	writel(1 << ((irqchan) % 32), REG(CR_OFFSET(irqchan)));
    if(irqchan <= 8) {
        local_irq_save(flags);
/*--- if(irq_prio_pointer) printk(KERN_ERR"<[%d]irq %d breaks %d:prio:%d>", irq_prio_pointer, irq, irqchan_to_irqline[IRQ->priority_mask_index_reg + 1] + MIPS_EXCEPTION_OFFSET, IRQ->priority_mask_index_reg); ---*/
        priority_push(IRQ->priority_mask_index_reg);
        IRQ->priority_mask_index_reg = irqline_to_irqchan[irqline] - 1;
        local_irq_restore(flags);
    }
    /*--- if(irqline != 7)printk(KERN_ERR"<ack-irq=%d/%d-%x>", irq, irq_prio_pointer, ((struct _irq_hw *)UR8_IRQ_CTRL_BASE)->priority_mask_index_reg); ---*/
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void ur8_end_irq(unsigned int irq) {
    struct _irq_hw *IRQ = (struct _irq_hw *)UR8_IRQ_CTRL_BASE;
    int irqline = irq - ur8_irq_base;
    int irqchan = irqline_to_irqchan[irqline];    /*--- map irq to irqchan ---*/

    if(irqchan <= 8) {
        unsigned long flags;
        local_irq_save(flags);
        IRQ->priority_mask_index_reg = priority_pop();
        local_irq_restore(flags);
    }
    /*--- if(irq != 15)printk(KERN_ERR"<end-irq=%d/%d-%x>", irq, irq_prio_pointer, ((struct _irq_hw *)UR8_IRQ_CTRL_BASE)->priority_mask_index_reg); ---*/
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void ur8_unmask_pci_irq(unsigned int irq) {
    pci_irq_enable(irq - UR8_INT_PCI_BEGIN);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void ur8_mask_pci_irq(unsigned int irq) {
    pci_irq_disable(irq - UR8_INT_PCI_BEGIN);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void ur8_unmask_sec_irq(unsigned int irq) {
	writel(1 << (irq - ur8_irq_base - 40), REG(SEC_ESR_OFFSET));
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void ur8_mask_sec_irq(unsigned int irq) {
	writel(1 << (irq - ur8_irq_base - 40), REG(SEC_ECR_OFFSET));
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void ur8_ack_sec_irq(unsigned int irq) {
	writel(1 << (irq - ur8_irq_base - 40), REG(SEC_CR_OFFSET));
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct irq_chip ur8_irq_type = {
	.name = "UR8",
	.unmask = ur8_unmask_irq,
	.mask = ur8_mask_irq,
	.ack = ur8_ack_irq,
    .set_type = ur8_set_type
};

static struct irq_chip ur8_sec_irq_type = {
	.name = "UR8",
	.unmask = ur8_unmask_sec_irq,
	.mask = ur8_mask_sec_irq,
	.ack = ur8_ack_sec_irq,
};

static struct irq_chip ur8_pci_irq_type = {
	.name = "UR8_PCI",
	.unmask = ur8_unmask_pci_irq,
	.mask = ur8_mask_pci_irq,
};

static struct irqaction ur8_cascade_action = {
	.handler = no_action,
	.name = "UR8 cascade interrupt"
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void __init ur8_irq_init(int base) {
	int i;
    struct _irq_hw *IRQ = (struct _irq_hw *)UR8_IRQ_CTRL_BASE;
	/*
	 * Disable interrupts and clear pending
	 */
	writel(0xffffffff, REG(ECR_OFFSET(0)));
	writel(0xff, REG(ECR_OFFSET(32)));
	writel(0xffffffff, REG(SEC_ECR_OFFSET));
	writel(0xffffffff, REG(CR_OFFSET(0)));
	writel(0xff, REG(CR_OFFSET(32)));
	writel(0xffffffff, REG(SEC_CR_OFFSET));

    IRQ->priority_mask_index_reg = 0;
	ur8_irq_base = base;

	for (i = 0; i < 41 ; i++) {
        irq_prio_stack[i] = 40;
    }
	for (i = 0; i < 40; i++) {
		writel(find_irqline_by_irqchan(i), REG(CHNL_OFFSET(i)));
		/* Primary IRQ's */
		set_irq_chip_and_handler(base + i, &ur8_irq_type,
					 handle_level_irq);
		/* Secondary IRQ's */
		if (i < 32)
			set_irq_chip_and_handler(base + i + 40,
						 &ur8_sec_irq_type,
						 handle_level_irq);
	}

	setup_irq(2, &ur8_cascade_action);
	setup_irq(ur8_irq_base, &ur8_cascade_action);
	set_c0_status(IE_IRQ0);
    ur8int_ctrl_irq_pacing_setup((4 * ur8_get_clock(avm_clock_id_vbus)) / 1000000);

    for(i = 0; i < 4; i++) {
    	set_irq_chip_and_handler(UR8_INT_PCI_BEGIN + i, &ur8_pci_irq_type, handle_level_irq);
    }
    IRQ->priority_mask_index_reg = 40;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void __init arch_init_irq(void) {
	/*--- printk("[%s]\n", __FUNCTION__); ---*/
	mips_cpu_irq_init();
	ur8_irq_init(MIPS_EXCEPTION_OFFSET);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ur8_cascade(void) {
	u32 status;
	int i, irq;

	/* Primary IRQ's */
	irq = (readl(REG(PIR_OFFSET)) >> 16) & 0x3f;
    /*--- if(irq != 7)printk("chan %d -> irqline %d\n", irq, (readl(REG(PIR_OFFSET)) >> 16) & 0x3f); ---*/
	if (irq) {
        irq += ur8_irq_base;
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
		do_IRQ(irq);
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_end, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
        ur8_end_irq(irq);
		return;
	}

	/* Secondary IRQ's are cascaded through primary '0' */
	writel(1, REG(CR_OFFSET(irq)));
	status = readl(REG(SEC_SR_OFFSET));
	for (i = 0; i < 32; i++) {
		if (status & 1) {
            int irq = ur8_irq_base + i + 40;
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
            avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
			do_IRQ(irq);
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
            avm_simple_profiling_log(avm_profile_data_type_hw_irq_end, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
			return;
		}
		status >>= 1;
	}

	spurious_interrupt();
}
/*------------------------------------------------------------------------------------------*\
 * Achtung!!!! Die Priorisierung (erst Hw-Irq, dann Timer-Irq) ist NICHT willkuerlich 
 * gewaehlt! 
 * Auf der 7270 konkurrieren Zerberus und C55-Irq miteinander. Durch zu lange Locks mit ungünstigen 
 * Verhältnis zum C55-Irq kann sonst ein Zerberus-Lock (Timer-Irq) dazu fuehren, dass C55-Irqs 
 * verloren gehen. 
 * Wenn wie hier nun geloest C55-Irq hoehere Prio, kann erst ein Lock > 8 msec zum Verlust von C55-Irqs
 * fuehren!
\*------------------------------------------------------------------------------------------*/
extern int cpu_wait_end(void);
asmlinkage void plat_irq_dispatch(void) {
	unsigned int pending = read_c0_status() & read_c0_cause() & ST0_IM;
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
    struct pt_regs regs;
    regs.cp0_epc = read_c0_epc();
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
    cpu_wait_end();
    if(pending & STATUSF_IP2) {		/* int0 hardware line */
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        int irq = 2;
        avm_simple_profiling(&regs, irq);
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
		ur8_cascade();
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_end, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
    } else if (pending & STATUSF_IP7) {		/* cpu timer */
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        int irq = 7;
        avm_simple_profiling(&regs, irq);
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
		do_IRQ(7);
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_end, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
    } else {
		spurious_interrupt();
    }
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ur8_set_type(unsigned int irq, unsigned int flow_type) {
    if((flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) == (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
        return -ENOSYS;
    }
    if((flow_type & (IRQF_TRIGGER_HIGH | IRQF_TRIGGER_LOW)) == (IRQF_TRIGGER_HIGH | IRQF_TRIGGER_LOW)) {
        return -EINVAL;
    }
    if(flow_type & IRQF_TRIGGER_PROBE) {
        return -ENOSYS;
    }
    if(flow_type & IRQF_TRIGGER_FALLING) {
        return ur8int_set_type(irq, ur8_interrupt_type_edge, ur8_interrupt_type_active_low);
    }
    if(flow_type & IRQF_TRIGGER_RISING) {
        return ur8int_set_type(irq, ur8_interrupt_type_edge, ur8_interrupt_type_active_high);
    }
    if(flow_type & IRQF_TRIGGER_LOW	) {
        return ur8int_set_type(irq, ur8_interrupt_type_level, ur8_interrupt_type_active_low);
    }
    if(flow_type & IRQF_TRIGGER_HIGH) {
        return ur8int_set_type(irq, ur8_interrupt_type_level, ur8_interrupt_type_active_high);
    }
    return -ENOSYS;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ur8int_set_type(unsigned int irq, enum _ur8_interrupt_type_trigger level, enum _ur8_interrupt_type_polarity polarity) {
    /*--- printk("irq:%d  level=%d polarity=%d\n", irq, level, polarity); ---*/
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    if(irq < MIPS_EXCEPTION_OFFSET) {
        return 1; /*--- kein setzen möglich ---*/
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    } else if(irq < UR8_INT_END_PRIMARY) {
        struct _irq_hw *IRQ = (struct _irq_hw *)UR8_IRQ_CTRL_BASE;
        irq = irqline_to_irqchan[irq - MIPS_EXCEPTION_OFFSET];    /*--- map irq to irqchan ---*/
        if(polarity == ur8_interrupt_type_active_low) {
            IRQ->polarity_reg[irq / 32] |=  (1 << (irq % 32));
        } else {
            IRQ->polarity_reg[irq / 32] &= ~(1 << (irq % 32));
        }
        if(level == ur8_interrupt_type_level) {
            IRQ->type_reg[irq / 32]     |=  (1 << (irq % 32));  /*--- level trigger ---*/
        } else {
            IRQ->type_reg[irq / 32]     &= ~(1 << (irq % 32));  /*--- edge trigger ----*/
        }
        if(level == ur8_interrupt_type_level) {
            mips_cpu_double_clear_mask[irq / 32] = (1 << (irq % 32));
        }

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    } else if(irq < UR8_INT_END_SECONDARY) {
        return 1; /*--- kein setzen möglich ---*/
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
#if defined(CONFIG_MIPS_VIRTUAL_IRQ)
    } else if(irq < UR8_INT_END_VIRTUAL) {
        irq -= UR8_INT_END_SECONDARY;
        vlynq_set_irq_type(irq, level, polarity);
#endif /*--- #if defined(CONFIG_MIPS_VIRTUAL_IRQ) ---*/
    }
    return 0;
}


void ur8int_ctrl_irq_pacing_setup(unsigned int prescale) {
    struct _irq_hw *IRQ = (struct _irq_hw *)UR8_IRQ_CTRL_BASE;
    unsigned int i;

    for(i = 0; i < 4; i++) {
        ur8_pacing_list[i].used    = 0;
        ur8_pacing_list[i].irqline = 0;
    }
    IRQ->pacing_prescale_reg.Bits.IPACEP = prescale;
}

/*------------------------------------------------------------------------------------------*\
 * Register the usage of pacing for an IRQ                                                  *
 *                                                                                          *
 * Argument: irq - The IRQ that should be paced                                             *
 *                                                                                          *
 * Return: >=0: Handle, with which to call the pacing related functions                     *
 *         -1 : IRQ is already registered                                                   *
 *         -2 : No more pacing registers available                                          *
 *         -3 : Thengiven IRQ can not be set and is therefor rejected                       *
\*------------------------------------------------------------------------------------------*/
int ur8int_ctrl_irq_pacing_register(unsigned int irq) {
    unsigned int i, handle = 99;
    unsigned long flags;

    if((irq < MIPS_EXCEPTION_OFFSET) || (irq >= UR8_INT_END_PRIMARY)) {
        printk(KERN_WARNING "[%s] This IRQ (%u) can not be set!\n",__FUNCTION__, irq);
        return -3;
    }
    irq -= MIPS_EXCEPTION_OFFSET;    /*--- map irq to irqchan ---*/

    spin_lock_irqsave(&ur8_irq_lock, flags);

    /* First check, whether the IRQ got registered already */
    for(i = 0; i < 4; i++) {
        if(ur8_pacing_list[i].used && (ur8_pacing_list[i].irqline == irq)) {
            spin_unlock_irqrestore(&ur8_irq_lock, flags);
            printk(KERN_WARNING "[%s] IRQ %u already registered!\n", __FUNCTION__,  irq);
            return -1;
        }
        if(!ur8_pacing_list[i].used)
            handle = i;
    }
    if(handle == 99) {
        spin_unlock_irqrestore(&ur8_irq_lock, flags);
        printk(KERN_WARNING "[%s] All IRQ pacing registers already used!\n",__FUNCTION__);
        return -2;
    }

    ur8_pacing_list[handle].used = 1;
    ur8_pacing_list[handle].irqline = irq;

    /* Configure pacing register */
    ur8int_ctrl_irq_pacing_set(handle, 0);

    spin_unlock_irqrestore(&ur8_irq_lock, flags);
    return handle;
}

/*------------------------------------------------------------------------------------------*\
 * Unregister the IRQ pacing for a given handle                                             *
 *                                                                                          *
 * Argument: handle - Handle returned at register time                                      *
\*------------------------------------------------------------------------------------------*/
void ur8int_ctrl_irq_pacing_unregister(int handle) {
    if((handle < 0) || (handle > 3) || !ur8_pacing_list[handle].used) {
        printk(KERN_ERR "[%s] Handle %u is invalid!\n",__FUNCTION__, handle);
        return;
    }
    
    /* Stop interrupt pacing for the given handle */
    ur8int_ctrl_irq_pacing_set(handle, 0);

    ur8_pacing_list[handle].used = 0;
}

/*------------------------------------------------------------------------------------------*\
 * Set the needed IRQ pacing                                                                *
 *                                                                                          *
 * Arguments: handle - Handle returned at registration time                                 *
 *            max    - Summarize at most 'max' interrupts per time slot                     *
 *                                                                                          *
 * Return: 0 on success, -1 on invalid handle                                               *
\*------------------------------------------------------------------------------------------*/
int ur8int_ctrl_irq_pacing_set(int handle, unsigned int max) {
    struct _irq_hw *IRQ = (struct _irq_hw *)UR8_IRQ_CTRL_BASE;
    unsigned int irq;

    if((handle < 0) || (handle > 3) || !ur8_pacing_list[handle].used) {
        printk(KERN_ERR "[%s] Handle %u is invalid!\n",__FUNCTION__, handle);
        return -1;
    }

    irq = ur8_pacing_list[handle].irqline;
    max &= (1 << 6) - 1;
    if(max == 1) max = 0;
    switch(handle) {
        case 0:
            if(max == 0) {
                IRQ->pacing_max_reg.Bits.IPACEN0 = 0;
                return 0;
            }
            IRQ->pacing_map_reg.Bits.IPACEMAP0 = irq;
            IRQ->pacing_max_reg.Bits.IPACEMAX0 = max;
            IRQ->pacing_max_reg.Bits.IPACEN0 = 1;
            break;
        case 1:
            if(max == 0) {
                IRQ->pacing_max_reg.Bits.IPACEN1 = 0;
                return 0;
            }
            IRQ->pacing_map_reg.Bits.IPACEMAP1 = irq;
            IRQ->pacing_max_reg.Bits.IPACEMAX1 = max;
            IRQ->pacing_max_reg.Bits.IPACEN1 = 1;
            break;
        case 2:
            if(max == 0) {
                IRQ->pacing_max_reg.Bits.IPACEN2 = 0;
                return 0;
            }
            IRQ->pacing_map_reg.Bits.IPACEMAP2 = irq;
            IRQ->pacing_max_reg.Bits.IPACEMAX2 = max;
            IRQ->pacing_max_reg.Bits.IPACEN2 = 1;
            break;
        case 3:
            if(max == 0) {
                IRQ->pacing_max_reg.Bits.IPACEN3 = 0;
                return 0;
            }
            IRQ->pacing_map_reg.Bits.IPACEMAP3 = irq;
            IRQ->pacing_max_reg.Bits.IPACEMAX3 = max;
            IRQ->pacing_max_reg.Bits.IPACEN3 = 1;
            break;
    }
    return 0;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

EXPORT_SYMBOL(ur8int_ctrl_irq_pacing_register);
EXPORT_SYMBOL(ur8int_ctrl_irq_pacing_unregister);
EXPORT_SYMBOL(ur8int_ctrl_irq_pacing_set);
EXPORT_SYMBOL(ur8int_set_type);





