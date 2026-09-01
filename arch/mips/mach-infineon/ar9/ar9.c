/******************************************************************************
**
** FILE NAME    : ar9.c
** PROJECT      : IFX UEIP
** MODULES      : BSP Basic
**
** DATE         : 27 May 2009
** AUTHOR       : Xu Liang
** DESCRIPTION  : source file for AR9
** COPYRIGHT    :       Copyright (c) 2009
**                      Infineon Technologies AG
**                      Am Campeon 1-12, 85579 Neubiberg, Germany
**
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
** HISTORY
** $Date        $Author         $Comment
** 27 May 2009   Xu Liang        The first UEIP release
*******************************************************************************/
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kallsyms.h>
#include <linux/delay.h>
#include <linux/avm_hw_config.h>

/*
 *  Chip Specific Head File
 */
#include <ifx_types.h>
#include <ifx_regs.h>
#include <ifx_pmu.h>
#include <common_routines.h>
#include <asm/mipsmtregs.h>

/*
 *  Chip Specific Variable/Function
 */

static char g_pkt_base[2048] __initdata __attribute__((__aligned__(32)));
static unsigned int g_desc_base[2] __initdata __attribute__((__aligned__(32)));
static void __init ifx_dplus_clean(void)
{
#define DMRX_PGCNT          ((volatile unsigned int *)0xbe191854)
#define DMRX_PKTCNT         ((volatile unsigned int *)0xbe191858)

    volatile unsigned int *desc_base = (volatile unsigned int *)KSEG1ADDR((unsigned int)g_desc_base);
    int i, j;

    IFX_REG_W32_MASK(1 << 18, 1 << 17, (volatile unsigned int *)IFX_SW_P0_CTL); //  force port 0 link down
    IFX_REG_W32_MASK(1 << 18, 1 << 17, (volatile unsigned int *)IFX_SW_P1_CTL); //  force port 1 link down

    if ( (IFX_REG_R32(DMRX_PGCNT) & 0x00FF) == 0 )
        return;

    IFX_REG_W32_MASK(1 << 17, 1 << 18, (volatile unsigned int *)IFX_SW_P2_CTL); //  force port 2 link up

    IFX_REG_W32(0, IFX_DMA_PS(0));
    IFX_REG_W32(0x1F68, IFX_DMA_PCTRL(0));
    IFX_REG_W32(0, IFX_DMA_IRNEN);  // disable all DMA interrupt

    for ( i = 0; i < 8; i += 2 ) {
        IFX_REG_W32(i, IFX_DMA_CS(0));
        IFX_REG_W32_MASK(0, 2, IFX_DMA_CCTRL(0));       //  reset channel
        while ( (IFX_REG_R32(IFX_DMA_CCTRL(0)) & 2) );  //  wait until reset finish
        IFX_REG_W32(1, IFX_DMA_CDLEN(0));               //  only 1 descriptor
        IFX_REG_W32(CPHYSADDR((unsigned int)desc_base), IFX_DMA_CDBA(0));       //  use local variable (array) as descriptor base address
        desc_base[0] = 0x80000000 | sizeof(g_pkt_base);
        desc_base[1] = CPHYSADDR((unsigned int)g_pkt_base);

        IFX_REG_W32_MASK(0, 1, IFX_DMA_CCTRL(0));       //  start receiving
        while ( 1 ) {
            for ( j = 0; j < 1000 && (desc_base[0] & 0x80000000) != 0; j++ );   //  assume packet can be finished within 1000 loops
            if ( (desc_base[0] & 0x80000000) != 0 )     //  no more packet
                break;
            desc_base[0] = 0x80000000 | sizeof(g_pkt_base);
        }
        IFX_REG_W32_MASK(1, 0, IFX_DMA_CCTRL(0));       //  stop receiving
    }

    IFX_REG_W32_MASK(3 << 17, 0, (volatile unsigned int *)IFX_SW_P0_CTL);       //  do not force port 0 link down
    IFX_REG_W32_MASK(3 << 17, 0, (volatile unsigned int *)IFX_SW_P1_CTL);       //  do not force port 1 link down
    IFX_REG_W32_MASK(3 << 17, 0, (volatile unsigned int *)IFX_SW_P2_CTL);       //  do not force port 2 link down

    if ( (IFX_REG_R32(DMRX_PGCNT) & 0x00FF) != 0 || (IFX_REG_R32(DMRX_PKTCNT) & 0x00FF) != 0 )
        prom_printf("%s error: IFX_REG_R32(DMRX_PGCNT) = 0x%08x, IFX_REG_R32(DMRX_PKTCNT) = 0x%08x\n", __func__, IFX_REG_R32(DMRX_PGCNT), IFX_REG_R32(DMRX_PKTCNT));
}

#define NVEC_INSTALLED	    0x45601CBA
#define BOOT_NVEC_INSTALL   0xBF2001D8
#define BOOT_NVEC           0xBF2001C4
extern void nmi_handler(void);
void __init setup_nmi(void) {

     volatile ulong *base;
     base = (ulong *)((ulong)BOOT_NVEC_INSTALL);
     *base = (ulong)NVEC_INSTALLED;

     base = (ulong *)((ulong)BOOT_NVEC);
     *base = (ulong)nmi_handler;

}

/*
 * AVM/TKL: Update PLL0_CFG for cleaner 25MHz Phy-Clock.
 */
#define BARRIER __asm__ __volatile__(".set noreorder\n\t" \
                                     "nop; nop; nop; nop; nop; nop; nop; nop; nop;\n\t" \
                                     "nop; nop; nop; nop; nop; nop; nop; nop; nop;\n\t" \
                                     ".set reorder\n\t")
#define MBX_BASEADDRESS                 0xBF200000
#define MPS_MEM_SEG_DATASIZE            512
#define PLL0_MASK                       0x40307ffd

void __init ifx_enhance_phy_clock(void)
{
    void *start, *end;
    u8 mbox[MPS_MEM_SEG_DATASIZE];
    u32 reg_val;
    int result, value;
    enum _avm_hw_param pin_param;

    value = 0;
    result = avm_get_hw_config(AVM_HW_CONFIG_VERSION,
                               "feature_avm_enhance_phy_clock",
                               &value,
                               &pin_param);

    if(result < 0 || !value){
        return;
    }

    prom_printf("[%s] PLL0_CFG: 0x%08x\n", __func__, IFX_REG_R32(IFX_CGU_PLL0_CFG));

    /*
     * only update if PLL0 still holds reset default values
     */
    if((IFX_REG_R32(IFX_CGU_PLL0_CFG) & PLL0_MASK) != (0x00b05f21 & PLL0_MASK)){
        prom_printf("[%s] PLL0 already modified.\n", __func__);
    } else {
        prom_printf("[%s] Setting new PLL0_CFG for external PHY...\n", __func__);
        local_irq_disable();

        /*
         * set new value. Won't take until a CGU update is triggered
         */
        reg_val = IFX_REG_R32(IFX_CGU_PLL0_CFG) & ~(PLL0_MASK);
        reg_val |= (0x00b0660b & PLL0_MASK);
        IFX_REG_W32(reg_val, IFX_CGU_PLL0_CFG);

        /*
         * Trigger CGU update. Save contents of MBOX SRAM, copy this code to it and execute
         * there, then restore MBOX SRAM. Code blatantly stolen from ifxmips_clk.c
         *
         */
        /* Cache the reset code of this function */
        __asm__ __volatile__ (
                "       .set    push                            \n"
                "       .set    mips3                           \n"
                "       la      %0,startpoint                   \n"
                "       la      %1,endpoint                     \n"
                "       .set    pop                             \n"
                : "=r" (start), "=r" (end)
                :
        );
        memcpy(&mbox[0], (u8*)MBX_BASEADDRESS, MPS_MEM_SEG_DATASIZE);
        memcpy((u8*)MBX_BASEADDRESS, (u8*)start, (end - start));

        __asm__("jr.hb     %0"::"r"(MBX_BASEADDRESS));
        __asm__ __volatile__ (
                "startpoint:                                    \n"
        );

        BARRIER;
        *IFX_CGU_UPDATE = 0x01;
        BARRIER;
        __sync();

        while((IFX_REG_R32(IFX_CGU_PLL0_CFG) & 0x2) == 0)
            ;

        __asm__("jr.hb     %0"::"r"( end + 16 ));

        __asm__ __volatile__ (
                "endpoint:                                      \n"
        );

        BARRIER;
        memcpy((u8*)MBX_BASEADDRESS, &mbox[0], MPS_MEM_SEG_DATASIZE);
#if 1
        /* Put MVPE's into 'configuration state' */
        set_c0_mvpcontrol(MVPCONTROL_VPC);

        settc(1);

        if ((read_tc_c0_tcstatus() & TCSTATUS_A) || !(read_tc_c0_tchalt() & TCHALT_H)) {
            write_tc_c0_tchalt(read_tc_c0_tchalt() & ~TCHALT_H);
            instruction_hazard();
        }

    /* take system out of configuration state */
        clear_c0_mvpcontrol(MVPCONTROL_VPC);

#endif

        prom_printf("[%s] finished.\n", __func__);
    }
}

void __init ifx_chip_setup(void)
{
    ifx_dplus_clean();
    ifx_pmu_disable_all_modules();
    setup_nmi();
}

#define IFX_RCU_RESET_STATUS                  ((volatile u32*)(0xBF203014))
unsigned int avm_reset_status(void) {

    u32 reg;

    reg = IFX_REG_R32(IFX_RCU_RESET_STATUS);

    switch ((reg >> 24) & (0xA8)) {
        case 0x08:          /*--- POR Reset ---*/
            return 0;
        case 0x20:          /*--- Software Reset ---*/
            return 1;
        case 0x80:          /*--- watchdog Reset ---*/
            return 2;
        default:
            return (reg >> 24) & 0xF;
    }
}

EXPORT_SYMBOL(avm_reset_status);

const void (*ifx_bsp_basic_mps_decrypt)(unsigned int addr, int n) = (const void (*)(unsigned int, int))0xbfc017c4;
EXPORT_SYMBOL(ifx_bsp_basic_mps_decrypt);



