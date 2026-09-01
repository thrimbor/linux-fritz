/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
//#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>

#include <hw_irq.h>
#include <hw_pci.h>
#include <ur8.h>
#include <ur8int.h>
#include <ur8_pci.h>


//#if CONFIG_PCI_DEBUG
#define DEBUG_PCI(args...) printk(KERN_INFO "[PCI-IRQ]: " args)
//#else
//#define DEBUG_PCI(args...) /* args */
//#endif

#if CONFIG_AVM_PCI_DEVICE_COUNT > 0
union _pci_irq {
    struct __pci_irq {
        volatile unsigned int PM_Change : 1;  /* 0 */
        volatile unsigned int Target_Abort_Det : 1;  /* 1  */
        volatile unsigned int Master_Abort_Det : 1;  /* 2  */
        volatile unsigned int Reserved1: 2;  /* 3,4 */
        volatile unsigned int SERR_Detect : 1;  /* 5  */
        volatile unsigned int PERR_Detect : 1;  /* 6 */
        volatile unsigned int Reserved2 : 9;  /* 7,8,9,10,11,12,13,14,15 */
        volatile unsigned int INTA_in : 1;  /* 16 */
        volatile unsigned int INTB_in : 1;  /* 17 */
        volatile unsigned int INTC_in : 1;  /* 18 */
        volatile unsigned int INTD_in : 1;  /* 19 */
        volatile unsigned int PME_in: 1;  /* 20 */
        volatile unsigned int Reserved3 : 3;  /* 21,22,23 */
        volatile unsigned int Soft_Int0 : 1;  /* 24 */
        volatile unsigned int Soft_Int1 : 1;  /* 25 */
        volatile unsigned int Soft_Int2 : 1;  /* 26 */
        volatile unsigned int Soft_Int3 : 1;  /* 27 */
        volatile unsigned int Reserved4 : 3;  /* 28,29,30 */
        volatile unsigned int v2p_intr : 1;  /* 31 */
    } Bits;
    volatile unsigned int Reg;
};


struct _pci_interrupt_controler {
    volatile unsigned int RevisionID;
    unsigned int Reserved1[3] ; /* 04,08,0c */
    union _pci_irq Status_Set;  /* 10 */
    union _pci_irq Status_Clear;  /* 14 */
    unsigned int Reserved2[2] ; /* 18,1c */
    union _pci_irq HostIrq_EnableSet;  /* 20 */
    union _pci_irq HostIrq_EnableClear;  /* 24 */
    unsigned int Reserved3[2] ; /* 18,1c */
    union _pci_irq BackEnd_Application_IrqEnableSet;  /* 30 */
    union _pci_irq BackEnd_Application_IrqEnableClear;  /* 34 */
};


struct _pci_interrupt_controler *PCI = (struct _pci_interrupt_controler *)0xa8614000;

/* not needed
unsigned int pci_irq_ack(unsigned int instance, unsigned int irq) {
    printk(KERN_ERR "[%s] caught irq: %d, %x\n",__func__,irq, PCI->Status_Clear.Reg);
    PCI->Status_Clear.Reg = (1 << (irq + 16));
    return 0;
}
*/

 /* not needed
//unsigned int pci_irq_end(unsigned int instance, unsigned int irq) {
unsigned int pci_irq_end(unsigned int irq) {
    //printk(KERN_ERR "[%s] caught irq: %d, %x\n",__func__,irq, PCI->Status_Clear.Reg);
#if (CONFIG_AVM_PCI_DEVICE_COUNT == 1)
	PCI->Status_Clear.Reg = (1 << (CONFIG_AVM_PCI_DEV1_INT + 16));
#else
    PCI->Status_Clear.Reg = (1 << (irq + 16));
#endif    
    return 0;
}
*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
//irqreturn_t pci_interrupt(int irq, void *dev_id, struct pt_regs *regs) { //RSP old
irqreturn_t pci_interrupt(int irq, void *dev_id) {
    irqreturn_t irq_handled = IRQ_NONE;
    
	//printk(KERN_ERR "INTX:%s: Status_Set: %x, BE: %x\n",__func__,PCI->Status_Set.Reg,PCI->BackEnd_Application_IrqEnableClear.Reg);
    
	/* if(PCI->Status_Set.Bits.INTA_in && PCI->HostIrq_EnableSet.Bits.INTB_in) {*/
    /* BackEnd_Application_IrqEnableClear shows the masked Status_Set bits */

#if (CONFIG_AVM_PCI_DEVICE_COUNT == 1)
    	do_IRQ (CONFIG_AVM_PCI_DEV1_INT + UR8_INT_PCI_BEGIN);
    	irq_handled = IRQ_HANDLED;
#else

#	if defined (CONFIG_AVM_PCI_INT_A)
	if (PCI->BackEnd_Application_IrqEnableClear.Bits.INTA_in) {
    	do_IRQ(0 + UR8_INT_PCI_BEGIN);
    	irq_handled = IRQ_HANDLED;
	}
#	endif	

#	if defined (CONFIG_AVM_PCI_INT_B)
	if (PCI->BackEnd_Application_IrqEnableClear.Bits.INTB_in) {
    	do_IRQ(1 + UR8_INT_PCI_BEGIN);
    	irq_handled = IRQ_HANDLED;
	}
#	endif	

#	if defined (CONFIG_AVM_PCI_INT_C)
	if (PCI->BackEnd_Application_IrqEnableClear.Bits.INTC_in) {
    	do_IRQ(2 + UR8_INT_PCI_BEGIN);
    	irq_handled = IRQ_HANDLED;
	}
#	endif

#endif //(CONFIG_AVM_PCI_DEVICE_COUNT == 1)

	/* optimized: as PCI INTs are Level triggered, we can try to clear all Flags at once,
	   so no pci_irq_end is needed */
    PCI->Status_Clear.Reg = (7 << (16));

    return irq_handled;
}


/* VERSION von TI

        if ( sw_irq_nr >= AV_PCI_INTA_SWIRQ )
        {       // find the bit number in the interrupt registers 
                hw_irq = map_swirq_to_intr_state( sw_irq_nr );
                AV_PCI_INTR_CLEAR = ( 1 << hw_irq );
                AV_PCI_INTR_DISABLE = ( 1 << hw_irq );
                AV_PCI_BE_INTR_DISABLE = ( 1 << hw_irq );
        }
}
*/
//unsigned int pci_irq_disable(unsigned int instance, unsigned int irq) {
unsigned int pci_irq_disable(unsigned int irq) {
        /*--- u32 hw_irq; ---*/
    /*--- printk("[pci_irq_disable] for irq %i\n",irq); ---*/
        
	// find the channel in the pci interrupt register
        // from the irq_nr and disable that 
                // find the bit number in the interrupt registers 
		/*--- hw_irq = irq + UR8_Y_INTA_BIT; // hw_irq = map_swirq_to_intr_state( sw_irq_nr ); ---*/
                /*--- UR8_PCI_INTR_CLEAR = ( 1 << hw_irq ); ---*/
                /*--- UR8_PCI_INTR_DISABLE = ( 1 << hw_irq ); ---*/
                /*--- UR8_PCI_BE_INTR_DISABLE = ( 1 << hw_irq ); ---*/

    //printk(KERN_ERR "[%s] disable irq: %d\n",__func__,irq);

    //PCI->Status_Clear.Reg = (1 << (irq + 16));
    //PCI->HostIrq_EnableClear.Reg = (1 << (irq + 16));
#if (CONFIG_AVM_PCI_DEVICE_COUNT == 1)
    PCI->BackEnd_Application_IrqEnableClear.Reg = (1 << (CONFIG_AVM_PCI_DEV1_INT + 16));
#else
    PCI->BackEnd_Application_IrqEnableClear.Reg = (1 << (irq + 16));
#endif    

	return 0;
}


/* VERSION von TI
void avalanche_pci_enable_irq( unsigned int sw_irq_nr )
{        u32 hw_irq;
        if ( ( sw_irq_nr >= AV_PCI_INTA_SWIRQ ) && ( sw_irq_nr <= (AV_PCI_INTA_SWIRQ + 3 ) ) )
        {       DbgPrint( "Enable PCI IRQ %d\n", sw_irq_nr );
                DbgPrint( "AV_PCI_INTR_ENABLE %d \n",AV_PCI_INTR_ENABLE);
                hw_irq = map_swirq_to_intr_state( sw_irq_nr );
                AV_PCI_INTR_CLEAR = ( 1 << hw_irq );
                AV_PCI_INTR_ENABLE = ( 1 << hw_irq );
                AV_PCI_BE_INTR_ENABLE = ( 1 << hw_irq );
                DbgPrint( "AV_PCI_INTR_ENABLE %x hw_irq %d \n",AV_PCI_INTR_ENABLE,hw_irq);
        } else
        {       if ( sw_irq_nr == AV_PCI_HWIRQ )
                {       DbgPrint( "Enabling Master PCI IRQ\n" );
                        disable_irq( LNXINTNUM( AV_PCI_HWIRQ ) );
                        enable_irq( LNXINTNUM( AV_PCI_HWIRQ ) );
                } else DbgPrint( "Enable PCI IRQ = %d???\n", sw_irq_nr );
        }
}
*/
//unsigned int pci_irq_enable(unsigned int instance, enum _pci_interrupt_types dev_type, unsigned int irq) {
//unsigned int pci_irq_enable(unsigned int instance, unsigned int irq) {
unsigned int pci_irq_enable(unsigned int irq) {
    /*--- printk("[pci_irq_enable %u]\n", irq); ---*/

    //PCI->Status_Clear.Reg = (1 << (irq + 16));
    //PCI->HostIrq_EnableClear.Reg = (1 << (irq + 16));
    //PCI->BackEnd_Application_IrqEnableClear.Reg = (1 << (irq + 16));

    //PCI->HostIrq_EnableSet.Reg = (1 << (irq + 16));
#if (CONFIG_AVM_PCI_DEVICE_COUNT == 1)
    PCI->BackEnd_Application_IrqEnableSet.Reg = (1 << (CONFIG_AVM_PCI_DEV1_INT + 16));
#else
    PCI->BackEnd_Application_IrqEnableSet.Reg = (1 << (irq + 16));
#endif    

    //printk(KERN_ERR "[%s] irq: %d\n",__func__,irq);
    return 0;
}


static int __init ur8_pci_int_init(void)
{ 	
    /*setting the interrupt to pulsed mode. */
    //#define UR8_PCI_INTR_DISABLE		(*(volatile unsigned long *) ( UR8_IRQ_CTRL_BASE + 0x60 ) )
    //UR8_PCI_INTR_DISABLE &= ~(1 << UR8INT_PCI);

    //ur8_intr_type_set(LNXINTNUM(AVALANCHE_PCI), 0); */
    /*--- printk(KERN_ERR "PCI_STATUS_SET: %x\n",PCI->Status_Set.Reg); ---*/
    /*--- printk(KERN_ERR "PCI_STATUS_CLR: %x\n",PCI->Status_Clear.Reg); ---*/
    /*--- printk(KERN_ERR "PCI_HOST_INT_EN: %x\n",PCI->HostIrq_EnableSet.Reg); ---*/
    /*--- printk(KERN_ERR "PCI_HOST_INT_CLR: %x\n",PCI->HostIrq_EnableClear.Reg); ---*/
    /*--- printk(KERN_ERR "PCI_BE_INT_En: %x\n",PCI->BackEnd_Application_IrqEnableSet.Reg); ---*/
    /*--- printk(KERN_ERR "PCI_BE_INT_CLR: %x\n",PCI->BackEnd_Application_IrqEnableClear.Reg); ---*/

    ur8int_set_type(UR8INT_PCI, ur8_interrupt_type_level, ur8_interrupt_type_active_high);

    if (request_irq(UR8INT_PCI, pci_interrupt, 0, "UR8 Unified PCI", NULL) != 0)
    {   printk(KERN_ERR "Failed to setup handler for unified PCI interrupt\n");
        return -1;
    }
    //enable_irq(UR8INT_PCI);
    /*--- printk(KERN_ERR "[pci_int_init] done\n"); ---*/
    return 0;
}
late_initcall(ur8_pci_int_init);
#endif //CONFIG_AVM_PCI_DEVICE_COUNT > 0
