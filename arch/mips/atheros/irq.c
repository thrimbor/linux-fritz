/*
 * General Interrupt handling for ATH soc
 */
//#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/kallsyms.h>

#include <asm/irq.h>
#include <asm/mipsregs.h>
//#include <asm/gdb-stub.h>

#include <atheros.h>
#include <asm/irq_cpu.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <linux/swap.h>
#include <linux/proc_fs.h>
#include <linux/pfn.h>
#include <linux/threads.h>
#include <asm/asm-offsets.h>

#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
#include <linux/avm_profile.h>
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
/*
 * dummy irqaction, so that interrupt controller cascading can work. Basically
 * when one IC is connected to another, this will be used to enable to Parent
 * IC's irq line to which the child IC is connected
 */

irqreturn_t ath_no_action(int cpl, void *dev_id)
{
	return IRQ_HANDLED;
}

static struct irqaction cascade = {
	.handler = no_action,
	.name = "cascade",
};

static void ath_dispatch_misc_intr(void);
void ath_dispatch_wlan_intr(void);
void ath_demux_usb_pciep_rc2(void);
static void ath_dispatch_gpio_intr(void);
static void ath_misc_irq_init(int irq_base);
extern pgd_t swapper_pg_dir[_PTRS_PER_PGD];
extern unsigned long pgd_current[NR_CPUS];

static void ath_misc_irq_ack(unsigned int irq);
static void ath_misc_irq_enable(unsigned int irq);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __init arch_init_irq(void)
{
	/*
	 * initialize our interrupt controllers
	 */
	mips_cpu_irq_init();
	ath_misc_irq_init(ATH_MISC_IRQ_BASE);
	ath_gpio_irq_init(ATH_GPIO_IRQ_BASE);

#ifdef CONFIG_PCI
	ath_pci_irq_init(ATH_PCI_IRQ_BASE);
#endif /* CONFIG_PCI */

	/*
	 * enable cascades
	 */
	setup_irq(ATH_CPU_IRQ_MISC, &cascade);
	setup_irq(ATH_MISC_IRQ_GPIO, &cascade);

#ifdef CONFIG_PCI
	setup_irq(ATH_CPU_IRQ_PCI, &cascade);
#endif

    /*--------------------------------------------------------------------------------------*\
     * AVM Bug Fix ath_arch_init_irq() überschreibt die Interrupt-Controller Werte des CP0
    \*--------------------------------------------------------------------------------------*/
	/*--- ath_arch_init_irq(); ---*/

	set_c0_status(ST0_IM);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _misc_irq_tab_ {
    unsigned int irq;
    void (*irq_handler)(void);
} misc_irq_tab[32] = {
#define MIMR_BITS(INT) RST_MISC_INTERRUPT_STATUS_ ##INT ##_LSB
    [MIMR_BITS(TIMER_INT)] =          { .irq = ATH_MISC_IRQ_TIMER,        .irq_handler = NULL },			
    [MIMR_BITS(ERROR_INT)] =          { .irq = ATH_MISC_IRQ_ERROR,        .irq_handler = NULL },			
    [MIMR_BITS(GPIO_INT)] =           { .irq = ATH_MISC_IRQ_GPIO,         .irq_handler = ath_dispatch_gpio_intr },			
    [MIMR_BITS(UART_INT)] =           { .irq = ATH_MISC_IRQ_UART,         .irq_handler = NULL },			
    [MIMR_BITS(WATCHDOG_INT)] =       { .irq = ATH_MISC_IRQ_WATCHDOG,     .irq_handler = NULL },		
    [MIMR_BITS(PC_INT)] =             { .irq = ATH_MISC_IRQ_PERF_COUNTER, .irq_handler = NULL },	
#if defined(CONFIG_ATH_HS_UART) || defined(CONFIG_ATH_HS_UART_MODULE) || defined(CONFIG_SERIAL_AVM_ATH_HI) || defined(CONFIG_SERIAL_AVM_ATH_HI_MODULE)
    [MIMR_BITS(UART1_INT)] =          { .irq = ATH_MISC_IRQ_UART1,        .irq_handler = NULL },		
#else
    [MIMR_BITS(USB_OHCI_INT)] =       { .irq = ATH_MISC_IRQ_USB_OHCI,     .irq_handler = NULL },		
#endif
    [MIMR_BITS(MBOX_INT)] =           { .irq = ATH_MISC_IRQ_DMA,          .irq_handler = NULL },			
#if defined(CONFIG_MACH_AR934x) ||  defined(CONFIG_CONFIG_MACH_QCA955x)
    [MIMR_BITS(TIMER2_INT)] =         { .irq = ATH_MISC_IRQ_TIMER2,       .irq_handler = NULL },			
    [MIMR_BITS(TIMER3_INT)] =         { .irq = ATH_MISC_IRQ_TIMER3,       .irq_handler = NULL },			
    [MIMR_BITS(TIMER4_INT)] =         { .irq = ATH_MISC_IRQ_TIMER4,       .irq_handler = NULL },			
    [MIMR_BITS(DDR_PERF_INT)] =       { .irq = ATH_MISC_IRQ_DDR_PERF,     .irq_handler = NULL },			
#endif
#ifdef CONFIG_MACH_QCA955x
    [MIMR_BITS(SGMII_MAC_INT)] =      { .irq = ATH_MISC_IRQ_ENET_LINK,    .irq_handler = NULL },		
#else
    [MIMR_BITS(S26_MAC_INT)] =        { .irq = ATH_MISC_IRQ_ENET_LINK,    .irq_handler = NULL },		
#endif
#if defined(CONFIG_MACH_AR934x) ||  defined(CONFIG_CONFIG_MACH_QCA955x)
    [MIMR_BITS(LUTS_AGER_INT)] =      { .irq = ATH_MISC_IRQ_NAT_AGER,     .irq_handler = NULL },
    [MIMR_BITS(OTP_INT)] =            { .irq = ATH_MISC_IRQ_OTP,          .irq_handler = NULL },
    [MIMR_BITS(CHKSUM_ACC_INT)] =     { .irq = ATH_MISC_IRQ_CHKSUM_ACC,   .irq_handler = NULL },   
    [MIMR_BITS(DDR_SF_ENTRY)] =       { .irq = ATH_MISC_IRQ_DDR_SF_ENTRY, .irq_handler = NULL },   
    [MIMR_BITS(DDR_SF_EXIT)] =        { .irq = ATH_MISC_IRQ_DDR_SF_EXIT,  .irq_handler = NULL },   
    [MIMR_BITS(DDR_ACTIVITY_IN_SF)] = { .irq = ATH_MISC_IRQ_DDR_ACTIVITY_IN_SF, .irq_handler = NULL },    
    [MIMR_BITS(SLIC_INTR)] =          { .irq = ATH_MISC_IRQ_SLIC,         .irq_handler = NULL },   
    [MIMR_BITS(WOW_INTR)] =           { .irq = ATH_MISC_IRQ_WOW,          .irq_handler = NULL },   
    [MIMR_BITS(NANDF_INTR)] =         { .irq = ATH_MISC_IRQ_NANDF,        .irq_handler = NULL }   
#endif
#ifdef CONFIG_MACH_QCA955x
    [MIMR_BITS(PGPIO_INT)]     =      { .irq = ATH_MISC_IRQ_PGPIO,        .irq_handler = NULL },          
    [MIMR_BITS(FDC_INT)]       =      { .irq = ATH_MISC_IRQ_FDC,          .irq_handler = NULL },          
    [MIMR_BITS(I2C_INT)]       =      { .irq = ATH_MISC_IRQ_I2C,          .irq_handler = NULL },          
    [MIMR_BITS(USB1_PLL_LOCK)] =      { .irq = ATH_MISC_IRQ_USB1_PLL_LOCK,.irq_handler = NULL },             
    [MIMR_BITS(USB2_PLL_LOCK)] =      { .irq = ATH_MISC_IRQ_USB2_PLL_LOCK,.irq_handler = NULL },             
#endif
}; 

static void ath_dispatch_misc_intr(void) {

	int pending;
    int bit = 0;

    pending = ath_reg_rd(ATH_MISC_INT_STATUS) & ath_reg_rd(ATH_MISC_INT_MASK);
    /*--- pending &= ~0x100; ---*/

    while (1) {
        bit = ffs(pending) - 1;

        if (bit < 0)
            break;

        pending &= ~(1 << bit);

        if (misc_irq_tab[bit].irq) {
            if (misc_irq_tab[bit].irq_handler) {
                misc_irq_tab[bit].irq_handler();
            } else {
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
                avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin, (unsigned int)(irq_desc + misc_irq_tab[bit].irq), misc_irq_tab[bit].irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
                do_IRQ(misc_irq_tab[bit].irq);
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
                avm_simple_profiling_log(avm_profile_data_type_hw_irq_end, (unsigned int)(irq_desc + misc_irq_tab[bit].irq), misc_irq_tab[bit].irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
            }
        } else {
            printk( KERN_ERR "{%s} unknown Interrupt %d\n", __func__, bit);
        }
		/*--- ath_reg_rmw_clear(ATH_MISC_INT_STATUS, (1 << bit)); ---*/
    } 
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ath_dispatch_gpio_intr(void)
{
	int pending, i;

	pending = ath_reg_rd(ATH_GPIO_INT_PENDING) & ath_reg_rd(ATH_GPIO_INT_MASK);

    ath_misc_irq_ack(ATH_MISC_IRQ_GPIO);
	for (i = 0; i < ATH_GPIO_IRQ_COUNT; i++) {
		if (pending & (1 << i)) {
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
            unsigned int irq = ATH_GPIO_IRQn(i);
            avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
			do_IRQ(ATH_GPIO_IRQn(i));
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
            avm_simple_profiling_log(avm_profile_data_type_hw_irq_end, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
        }
	}
	ath_misc_irq_enable(ATH_MISC_IRQ_GPIO);
}

extern int cpu_wait_end(void);
/*------------------------------------------------------------------------------------------*\
 * Dispatch interrupts.
 * XXX: This currently does not prioritize except in calling order. Eventually
 * there should perhaps be a static map which defines, the IPs to be masked for
 * a given IP.

\*------------------------------------------------------------------------------------------*/
asmlinkage void plat_irq_dispatch(void)
{
	int pending;
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
    unsigned int first = 1;
    struct pt_regs regs;
    regs.cp0_epc = read_c0_epc();
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 

    cpu_wait_end(); /*--- auch wenn es r4k_wait_irqoff gibt: trotzdem aufrufen, um system-load-Ausgabe zu triggern  ---*/
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
    ath_workaround_nmi_check_cpugrant();   
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
#if 0
	if (!(pending & CAUSEF_IP7))
		printk("%s: in irq dispatch \n", __func__);
#endif
	pending = read_c0_status() & read_c0_cause();

	if (pending & CAUSEF_IP7) {
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        unsigned int irq = ATH_CPU_IRQ_TIMER;
        if(first) {
            avm_simple_profiling(&regs, irq);
            first = 0;
        }
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
        do_IRQ(ATH_CPU_IRQ_TIMER);
/*--- #ifdef CONFIG_MACH_QCA955x ---*/
/*--- ath_aphang_timer_fn(); ---*/
/*--- #endif ---*/
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_end, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
    } else if (pending & CAUSEF_IP2) {
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        unsigned int irq = ATH_CPU_IRQ_WLAN;
        if(first) {
            avm_simple_profiling(&regs, irq);
            first = 0;
        }
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
        ath_dispatch_wlan_intr();
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_end, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
    }

	else if (pending & CAUSEF_IP4) {
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        unsigned int irq = ATH_CPU_IRQ_GE0;
        if(first) {
            avm_simple_profiling(&regs, irq);
            first = 0;
        }
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
        do_IRQ(ATH_CPU_IRQ_GE0);
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
            avm_simple_profiling_log(avm_profile_data_type_hw_irq_end, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
    }
	else if (pending & CAUSEF_IP5) {
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        unsigned int irq = ATH_CPU_IRQ_GE1;
        if(first) {
            avm_simple_profiling(&regs, irq);
            first = 0;
        }
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
		do_IRQ(ATH_CPU_IRQ_GE1);
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_end, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
    }
	else if (pending & CAUSEF_IP3) {
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        unsigned int irq = ATH_CPU_IRQ_USB;
        if(first) {
            avm_simple_profiling(&regs, irq);
            first = 0;
        }
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_begin, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
#ifdef CONFIG_MACH_QCA955x
		ath_demux_usb_pciep_rc2();
#elif defined(CONFIG_MACH_AR934x)
		do_IRQ(ATH_CPU_IRQ_USB);
#endif
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
        avm_simple_profiling_log(avm_profile_data_type_hw_irq_end, (unsigned int)(irq_desc + irq), irq);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
    }
	else if (pending & CAUSEF_IP6) {
        ath_dispatch_misc_intr();
    }

	/*
	 * Some PCI devices are write to clear. These writes are posted and might
	 * require a flush (r8169.c e.g.). Its unclear what will have more
	 * performance impact - flush after every interrupt or taking a few
	 * "spurious" interrupts. For now, its the latter.
	 */
	/*else
	   printk("spurious IRQ pending: 0x%x\n", pending); */
}

#if 1
#define vpk(...)
#define vps(...)
#else
#define vpk     printk
#define vps     print_symbol
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ath_misc_irq_enable(unsigned int irq)
{
#if 0
	vpk("%s: %u ", __func__, irq);
	vps("%s\n", __builtin_return_address(0));
#endif
	ath_reg_rmw_set(ATH_MISC_INT_MASK, (1 << (irq - ATH_MISC_IRQ_BASE)));
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ath_misc_irq_disable(unsigned int irq)
{
#if 0
	vpk("%s: %u ", __func__, irq);
	vps("%s\n", __builtin_return_address(0));
#endif
	ath_reg_rmw_clear(ATH_MISC_INT_MASK, (1 << (irq - ATH_MISC_IRQ_BASE)));
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ath_misc_irq_startup(unsigned int irq)
{
#if 0
	vpk("%s: %u ", __func__, irq);
	vps("%s\n", __builtin_return_address(0));
#endif
	/*--- ath_reg_rmw_clear(ATH_MISC_INT_STATUS, (1 << (irq - ATH_MISC_IRQ_BASE))); ---*/
	ath_misc_irq_enable(irq);
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ath_misc_irq_shutdown(unsigned int irq)
{
#if 0
	vpk("%s: %u ", __func__, irq);
	vps("%s\n", __builtin_return_address(0));
#endif
	ath_misc_irq_disable(irq);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ath_misc_irq_ack(unsigned int irq)
{
#if 0
	vpk("%s: %u ", __func__, irq);
	vps("%s\n", __builtin_return_address(0));
#endif
	ath_misc_irq_disable(irq);
    ath_reg_rmw_clear(ATH_MISC_INT_STATUS, (1 << (irq - ATH_MISC_IRQ_BASE)));
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ath_misc_irq_end(unsigned int irq)
{
#if 0
	vpk("%s: %u ", __func__, irq);
	vps("%s\n", __builtin_return_address(0));
#endif
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		ath_misc_irq_enable(irq);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ath_misc_irq_set_affinity(unsigned int irq, const struct cpumask *dest)
{
	/*
	 * Only 1 CPU; ignore affinity request
	 */
	return 0;
}

struct irq_chip ath_misc_irq_controller = {
	.name = "ATH MISC",
	.startup = ath_misc_irq_startup,
	.shutdown = ath_misc_irq_shutdown,
	.enable = ath_misc_irq_enable,
	.disable = ath_misc_irq_disable,
	.ack = ath_misc_irq_ack,
	.end = ath_misc_irq_end,
	.eoi = ath_misc_irq_end,
	.set_affinity = ath_misc_irq_set_affinity,
};

/*------------------------------------------------------------------------------------------*\
 * Determine interrupt source among interrupts that use IP6
\*------------------------------------------------------------------------------------------*/
static void ath_misc_irq_init(int irq_base)
{
	int i;

	for (i = irq_base; i < irq_base + ATH_MISC_IRQ_COUNT; i++) {
		irq_desc[i].status = IRQ_DISABLED;
		irq_desc[i].action = NULL;
		irq_desc[i].depth = 1;
		set_irq_chip_and_handler(i, &ath_misc_irq_controller,
					 handle_percpu_irq);
	}
}
