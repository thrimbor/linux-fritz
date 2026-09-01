#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#ifdef CONFIG_AR9
volatile unsigned int *BCU_CON_Register    = (volatile unsigned int *)(0xBE100000 + 0x10);
volatile unsigned int *BCU_ECON_Register   = (volatile unsigned int *)(0xBE100000 + 0x14);
volatile unsigned int *BCU_EADD_Register   = (volatile unsigned int *)(0xBE100000 + 0x20);
volatile unsigned int *BCU_EDAT_Register   = (volatile unsigned int *)(0xBE100000 + 0x24);
volatile unsigned int *BCU_IRNEN_Register  = (volatile unsigned int *)(0xBE100000 + 0xF4);
/*------------------------------------------------------------------------------------------*\
 * LANTIQ:
 *
 * Von der Entwicklung wurde bestätigt, dass die beiden Offsets der Register IRNICR und 
 * IRNCR vertauscht sind, d.h. der Interrupt wird durch Schreiben auf Offset 0xF8 quittiert, 
 * und nicht auf Offset 0xFC, wie in der Registerbeschreibung angegeben. 
 *
 * Wie bestätigen Sie in Ihrer SW diesen Interrupt? 
 * Vielleicht wird er ja nicht richtig quittiert, 
 * sondern wegen der fehlerhaften Dokumentation neu gesetzt. 
\*------------------------------------------------------------------------------------------*/
/*--- volatile unsigned int *BCU_IRNICR_Register = (volatile unsigned int *)(0xBE100000 + 0xF8); ---*/
/*--- volatile unsigned int *BCU_IRNCR_Register  = (volatile unsigned int *)(0xBE100000 + 0xFC); ---*/
volatile unsigned int *BCU_IRNCR_Register  = (volatile unsigned int *)(0xBE100000 + 0xF8);
volatile unsigned int *BCU_IRNICR_Register = (volatile unsigned int *)(0xBE100000 + 0xFC);
#else /*--- #ifdef CONFIG_AR9 ---*/
volatile unsigned int *BCU_CON_Register    = (volatile unsigned int *)(0xBE100000 + 0x10);
volatile unsigned int *BCU_ECON_Register   = (volatile unsigned int *)(0xBE100000 + 0x20);
volatile unsigned int *BCU_EADD_Register   = (volatile unsigned int *)(0xBE100000 + 0x24);
volatile unsigned int *BCU_EDAT_Register   = (volatile unsigned int *)(0xBE100000 + 0x28);
volatile unsigned int *BCU_IRNEN_Register  = (volatile unsigned int *)(0xBE100000 + 0xF4);
/*------------------------------------------------------------------------------------------*\
 * LANTIQ:
 *
 * Von der Entwicklung wurde bestätigt, dass die beiden Offsets der Register IRNICR und 
 * IRNCR vertauscht sind, d.h. der Interrupt wird durch Schreiben auf Offset 0xF8 quittiert, 
 * und nicht auf Offset 0xFC, wie in der Registerbeschreibung angegeben. 
 *
 * Wie bestätigen Sie in Ihrer SW diesen Interrupt? 
 * Vielleicht wird er ja nicht richtig quittiert, 
 * sondern wegen der fehlerhaften Dokumentation neu gesetzt. 
\*------------------------------------------------------------------------------------------*/
/*--- volatile unsigned int *BCU_IRNICR_Register = (volatile unsigned int *)(0xBE100000 + 0xF8); ---*/
/*--- volatile unsigned int *BCU_IRNCR_Register  = (volatile unsigned int *)(0xBE100000 + 0xFC); ---*/
volatile unsigned int *BCU_IRNCR_Register  = (volatile unsigned int *)(0xBE100000 + 0xF8);
volatile unsigned int *BCU_IRNICR_Register = (volatile unsigned int *)(0xBE100000 + 0xFC);
#endif /*--- #else ---*/ /*--- #ifdef CONFIG_AR9 ---*/
#define BCU_IRQ_ACK()                         *BCU_IRNCR_Register  = 1

volatile unsigned int *sBCU_CON_Register    = (volatile unsigned int *)(0xBC200400 + 0x10);
volatile unsigned int *sBCU_ECON_Register   = (volatile unsigned int *)(0xBC200400 + 0x20);
volatile unsigned int *sBCU_EADD_Register   = (volatile unsigned int *)(0xBC200400 + 0x24);
volatile unsigned int *sBCU_EDAT_Register   = (volatile unsigned int *)(0xBC200400 + 0x28);
volatile unsigned int *sBCU_IRNEN_Register  = (volatile unsigned int *)(0xBC200400 + 0xF4);
volatile unsigned int *sBCU_IRNICR_Register = (volatile unsigned int *)(0xBC200400 + 0xF8);
volatile unsigned int *sBCU_IRNCR_Register  = (volatile unsigned int *)(0xBC200400 + 0xFC);

volatile unsigned int *sBIU_FPISCR_Register = (volatile unsigned int *)(0xBF880000 + 0x2D8);
volatile unsigned int *sBIU_FPISSR_Register = (volatile unsigned int *)(0xBF880000 + 0x2E0);
volatile unsigned int *sBIU_FBCFG_Register  = (volatile unsigned int *)(0xBF880000 + 0x2e8);
volatile unsigned int *sBIU_AHBCR_Register  = (volatile unsigned int *)(0xBF880000 + 0x2f0);
volatile unsigned int *sBIU_AHBSR_Register  = (volatile unsigned int *)(0xBF880000 + 0x2f8);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void show_BCU_info(void) {
    unsigned int ErrorDataInfo = *BCU_ECON_Register;
    unsigned int ErrorAddrInfo = *BCU_EADD_Register;
    unsigned int ErrorDataCapture = *BCU_EDAT_Register;

    printk(KERN_ERR "BCU[BCU_EADD]: Error Address Register: 0x%08x\n", ErrorAddrInfo);
    printk(KERN_ERR "BCU[BCU_EDAT]: Error Data Capture Register: 0x%08x\n", ErrorDataCapture);
    printk(KERN_ERR "BCU[BCU_CON] : Control Register: 0x%08x\n", *BCU_CON_Register);

    printk(KERN_ERR "BCU[BCU_ECON]: Error Register (0x%08x):"
            "\n\tFPI Bus Operation Code 0x%x "
            "\n\tFPI Bus Tag Number 0x%x "
            "\n\tFPI Bus Read Signal: %s "
            "\n\tFPI Bus Write Signal: %s "
            "\n\tFPI Bus Supervisor Mode: %s "
            "\n\tFPI Bus Acknowladge Signal: 0x%x "
            "\n\tFPI Bus Abort: %s "
            "\n\tFPI Bus Ready: %s "
            "\n\tFPI Bus Timeout: %s "
            "\n\tFPI Bus Error Counter: %d\n",
            ErrorDataInfo,
            (ErrorDataInfo >> 28) & ((1 << 4) - 1),
            (ErrorDataInfo >> 24) & ((1 << 4) - 1),
            (ErrorDataInfo & (1 << 23)) ? "passive" : "active",
            (ErrorDataInfo & (1 << 22)) ? "passive" : "active",
            (ErrorDataInfo & (1 << 21)) ? "active" : "passive",
            (ErrorDataInfo >> 19) & ((1 << 2) - 1),
            (ErrorDataInfo & (1 << 18)) ? "passive" : "active",
            (ErrorDataInfo & (1 << 17)) ? "active" : "passive",
            (ErrorDataInfo & (1 << 16)) ? "occurred" : "not pressed",
            (ErrorDataInfo >> 0) & ((1 << 16) - 1)
    );
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void show_sBCU_info(void) {
    unsigned int ErrorDataInfo = *sBCU_ECON_Register;
    unsigned int ErrorAddrInfo = *sBCU_EADD_Register;
    unsigned int ErrorDataCapture = *sBCU_EDAT_Register;

    printk(KERN_ERR "Slave BCU[sBCU_EADD]: Error Address Register: 0x%08x\n", ErrorAddrInfo);
    printk(KERN_ERR "Slave BCU[sBCU_EDAT]: Error Data Capture Register: 0x%08x\n", ErrorDataCapture);
    printk(KERN_ERR "Slave BCU[sBCU_CON] : Control Register: 0x%08x\n", *BCU_CON_Register);

    printk(KERN_ERR "Slave BCU[sBCU_ECON]: Error Register (0x%08x):"
            "\n\tFPI Bus Operation Code 0x%x "
            "\n\tFPI Bus Tag Number 0x%x "
            "\n\tFPI Bus Read Signal: %s "
            "\n\tFPI Bus Write Signal: %s "
            "\n\tFPI Bus Supervisor Mode: %s "
            "\n\tFPI Bus Acknowladge Signal: 0x%x "
            "\n\tFPI Bus Abort: %s "
            "\n\tFPI Bus Ready: %s "
            "\n\tFPI Bus Timeout: %s "
            "\n\tFPI Bus Error Counter: %d\n",
            ErrorDataInfo,
            (ErrorDataInfo >> 28) & ((1 << 4) - 1),
            (ErrorDataInfo >> 24) & ((1 << 4) - 1),
            (ErrorDataInfo & (1 << 23)) ? "passive" : "active",
            (ErrorDataInfo & (1 << 22)) ? "passive" : "active",
            (ErrorDataInfo & (1 << 21)) ? "active" : "passive",
            (ErrorDataInfo >> 19) & ((1 << 2) - 1),
            (ErrorDataInfo & (1 << 18)) ? "passive" : "active",
            (ErrorDataInfo & (1 << 17)) ? "active" : "passive",
            (ErrorDataInfo & (1 << 16)) ? "occurred" : "not pressed",
            (ErrorDataInfo >> 0) & ((1 << 16) - 1)
    );
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void show_sBIU_info(void) {
    unsigned int Status = *sBIU_AHBSR_Register ;

    printk(KERN_ERR "[%s] BIU AHB Status Register: 0x%x\n", __FUNCTION__, Status);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
irqreturn_t bcu_control_interrupt(int irq, void *context __attribute__ ((unused))) {

    switch(irq) {
        case INT_NUM_IM0_IRL25: /*--- "FPI Master Bus BCU" ---*/
            printk(KERN_ERR "[%s] 'FPI Master Bus BCU' Error Interrupt\n", __FUNCTION__);
            show_BCU_info();
            BCU_IRQ_ACK();
            break;
        case INT_NUM_IM1_IRL25: /*--- "FPI Slave BCU0" ---*/
            printk(KERN_ERR "[%s] 'FPI Slave BCU0' Error Interrupt\n", __FUNCTION__);
            show_sBCU_info();
            *sBCU_IRNCR_Register  = 0;
            break;
        case INT_NUM_IM1_IRL27: /*--- "SBIU Error" ---*/
            printk(KERN_ERR "[%s] 'SBIU Error' Error Interrupt\n", __FUNCTION__);
            show_sBIU_info();
            break;
        default:
            printk(KERN_ERR "[%s] 'Unknown %d' Error Interrupt\n", __FUNCTION__, irq);
            break;
    }


    return IRQ_HANDLED;
}



/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int bcu_control_init(void) {
    int ret;
    ret = request_irq(INT_NUM_IM0_IRL25, bcu_control_interrupt, IRQF_DISABLED, "FPI Master Bus BCU", NULL);
    if(ret < 0) {
        printk(KERN_ERR "[%s] request irq %s (%d) failed\n", __FUNCTION__, "INT_NUM_IM0_IRL25", INT_NUM_IM0_IRL25);
    }
    ret = request_irq(INT_NUM_IM1_IRL25, bcu_control_interrupt, IRQF_DISABLED, "FPI Slave BCU0", NULL);
    if(ret < 0) {
        printk(KERN_ERR "[%s] request irq %s (%d) failed\n", __FUNCTION__, "INT_NUM_IM1_IRL25", INT_NUM_IM1_IRL25);
    }
    ret = request_irq(INT_NUM_IM1_IRL27, bcu_control_interrupt, IRQF_DISABLED, "SBIU Error", NULL);
    if(ret < 0) {
        printk(KERN_ERR "[%s] request irq %s (%d) failed\n", __FUNCTION__, "INT_NUM_IM1_IRL27", INT_NUM_IM1_IRL27);
    }


    *BCU_CON_Register    = 
                (0xffff << 0) | /* BCU But Time Out, default 0xFFFF */
                (1 << 16) |  /* BCU Debug Trace Enable, default ON */
                /*--- (1 << 17) | ---*/  /* BCU Arbiter Priority Mode, default 0 (fixed) */
                /*--- (1 << 18) | ---*/  /* BCU Power Savin Enable (Automatic Clock Control), default OFF, 0 */
                (1 << 19) |  /* Starvation protection, default ON, 1 */
                (0x40 << 24) | /* Starvation Counter */
                0;

    *sBCU_CON_Register    = 
                (0xffff << 0) | /* Slave BCU But Time Out, default 0xFFFF */
                (1 << 16) |  /* Slave BCU Debug Trace Enable, default ON */
                /*--- (1 << 18) | ---*/  /* Slave BCU Power Savin Enable (Automatic Clock Control), default OFF, 0 */
                (1 << 19) |  /* Starvation protection, default ON, 1 */
                (0x40 << 24) | /* Starvation Counter */
                0;

    /*--- *sBCU_IRNCR_Register  = 0; ---*/
    BCU_IRQ_ACK();

    *sBCU_IRNEN_Register  = 1;
    *BCU_IRNEN_Register  = 1;

    printk(KERN_ERR "[%s] interrupts for BCU installed\n", __FUNCTION__);

    return 0;
}


device_initcall(bcu_control_init);
