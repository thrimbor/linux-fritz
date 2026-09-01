/******************************************************************************
**
** FILE NAME    : ifxmips_ar9_clk.c
** PROJECT      : UEIP
** MODULES     	: CGU
**
** DATE         : 19 JUL 2005
** AUTHOR       : Xu Liang
** DESCRIPTION  : Clock Generation Unit (CGU) Driver
** COPYRIGHT    : 	Copyright (c) 2006
**			Infineon Technologies AG
**			Am Campeon 1-12, 85579 Neubiberg, Germany
**
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
** HISTORY
** $Date        $Author         $Comment
** 19 JUL 2005  Xu Liang        Initiate Version
** 21 AUG 2006  Xu Liang        Work around to fix calculation error for 36M
**                              crystal.
** 23 OCT 2006  Xu Liang        Add GPL header.
** 28 Sept 2007 Teh Kok How	Use kernel interface for 64-bit arithmetics
** 28 May 2009  Huang Xiaogang  The first UEIP release
*******************************************************************************/

/*!
  \file ifxmips_ar9_clk.c
  \ingroup IFX_CGU
  \brief This file contains Clock Generation Unit driver
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <asm/irq.h>
#include <asm/time.h>
#include <ifx_regs.h>
#include <ifx_clk.h>
#include <asm/r4kcache.h>
#include <common_routines.h>
#include <linux/percpu.h>
#include <linux/sysdev.h>
#include <asm/mipsregs.h>
#include "../common/ifxmips_dma_reg.h"
#include <asm/avm_busmaster_handler.h>

#define ENABLE_SET_CLOCK_PROFILING      0

#define BARRIER __asm__ __volatile__(".set noreorder\n\t" \
                                     "nop; nop; nop; nop; nop; nop; nop; nop; nop;\n\t" \
                                     "nop; nop; nop; nop; nop; nop; nop; nop; nop;\n\t" \
                                     ".set reorder\n\t")
/*
 * MPS SRAM Base Address
 */
#define MBX_BASEADDRESS                 0xBF200000
#define MPS_MEM_SEG_DATASIZE            512

/*
 *  Frequency of Clock Direct Feed from The Analog Line Driver Chip
 */
#define BASIC_INPUT_CLOCK_FREQUENCY     36000000

/*
 *  CGU PLL0 Configure Register
 */
#define CGU_PLL0_BYPASS                 (*IFX_CGU_PLL0_CFG & (1 << 30))
#define CGU_PLL1_FMOD_S                 GET_BITS(*IFX_CGU_PLL0_CFG, 23, 22)
#define CGU_PLL0_PS_2_EN                (*IFX_CGU_PLL0_CFG & (1 << 21))
#define CGU_PLL0_PS_1_EN                (*IFX_CGU_PLL0_CFG & (1 << 20))
#define CGU_PLL0_DIV_EN                 (*IFX_CGU_PLL0_CFG & (1 << 14))
#define CGU_PLL0_CFG_PLLN               GET_BITS(*IFX_CGU_PLL0_CFG, 13, 6)
#define CGU_PLL0_CFG_PLLM               GET_BITS(*IFX_CGU_PLL0_CFG, 5, 2)
#define CGU_PLL0_CFG_PLLL               (*IFX_CGU_PLL0_CFG & (1 << 1))
#define CGU_PLL0_CFG_PLLEN              (*IFX_CGU_PLL0_CFG & (1 << 0))

/*
 *  CGU PLL1 Configure Register
 */
#define CGU_PLL1_PHDIV_EN               (*IFX_CGU_PLL1_CFG & (1 << 31))
#define CGU_PLL1_BYPASS                 (*IFX_CGU_PLL1_CFG & (1 << 30))
#define CGU_PLL1_CFG_CTEN               (*IFX_CGU_PLL1_CFG & (1 << 29))
#define CGU_PLL1_CFG_DSMSEL             (*IFX_CGU_PLL1_CFG & (1 << 28))
#define CGU_PLL1_CFG_FRAC_EN            (*IFX_CGU_PLL1_CFG & (1 << 27))
#define CGU_PLL1_CFG_PLLK               GET_BITS(*IFX_CGU_PLL1_CFG, 26, 17)
#define CGU_PLL1_CFG_PLLD               GET_BITS(*IFX_CGU_PLL1_CFG, 16, 13)
#define CGU_PLL1_CFG_PLLN               GET_BITS(*IFX_CGU_PLL1_CFG, 12, 6)
#define CGU_PLL1_CFG_PLLM               GET_BITS(*IFX_CGU_PLL1_CFG, 5, 2)
#define CGU_PLL1_CFG_PLLL               (*IFX_CGU_PLL1_CFG & (1 << 1))
#define CGU_PLL1_CFG_PLLEN              (*IFX_CGU_PLL1_CFG & (1 << 0))

/*
 *  CGU Clock Sys Mux Register
 */
#define CGU_SYS_QOS_SEL                 GET_BITS(*IFX_CGU_SYS, 10, 9)
#define CGU_SYS_PPESEL                  (*IFX_CGU_SYS & (1 << 7))
#define CGU_SYS_FPI_SEL                 (*IFX_CGU_SYS & (1 << 6))
#define CGU_SYS_SYS_SEL                 GET_BITS(*IFX_CGU_SYS, 4, 3)
#define CGU_SYS_CPUSEL                  (*IFX_CGU_SYS & (1 << 2))
#define CGU_SYS_DDR_SEL                 (*IFX_CGU_SYS & (1 << 0))

/*
 *  CGU Update Register
 */
#define CGU_UPDATE_UPDATE               (*IFX_CGU_UPDATE & (1 << 0))

/*
 *  CGU Interface Clock Register
 */
#define CGU_IF_CLK_PCI_CLK              GET_BITS(*IFX_CGU_IF_CLK, 24, 20)
#define CGU_IF_CLK_PDA                  (*IFX_CGU_IF_CLK & (1 << 19))
#define CGU_IF_CLK_PCI_B                (*IFX_CGU_IF_CLK & (1 << 18))
#define CGU_IF_CLK_PCIBM                (*IFX_CGU_IF_CLK & (1 << 17))
#define CGU_IF_CLK_PCIS                 (*IFX_CGU_IF_CLK & (1 << 16))
#define CGU_IF_CLK_CLKOD0               GET_BITS(*IFX_CGU_IF_CLK, 15, 14)
#define CGU_IF_CLK_CLKOD1               GET_BITS(*IFX_CGU_IF_CLK, 13, 12)
#define CGU_IF_CLK_CLKOD2               GET_BITS(*IFX_CGU_IF_CLK, 11, 10)
#define CGU_IF_CLK_CLKOD3               GET_BITS(*IFX_CGU_IF_CLK, 9, 8)
#define CGU_IF_CLK_USBSEL               GET_BITS(*IFX_CGU_IF_CLK, 1, 0)
#define CGU_IF_CLK_VLYNQSEL             GET_BITS(*IFX_CGU_IF_CLK, 1, 0)

/*
 *  CGU SDRAM Memory Delay Register
 */
#define CGU_SMD_CLK_IN_S                (*IFX_CGU_SMD & (1 << 22))
#define CGU_SMD_DDR_PRG                 (*IFX_CGU_SMD & (1 << 21))
#define CGU_SMD_DDR_CQ                  (*IFX_CGU_SMD & (1 << 20))
#define CGU_SMD_DDR_EQ                  (*IFX_CGU_SMD & (1 << 19))
#define CGU_SMD_SDR_CLKS                (*IFX_CGU_SMD & (1 << 18))
#define CGU_SMD_MIDS                    GET_BITS(*IFX_CGU_SMD, 17, 12)
#define CGU_SMD_MODS                    GET_BITS(*IFX_CGU_SMD, 11, 6)
#define CGU_SMD_MDSEL                   GET_BITS(*IFX_CGU_SMD, 5, 0)

/*
 *  CGU CT Status Register 1
 */
#define CGU_CT1SR_PDOUT                 GET_BITS(*IFX_CGU_CT1SR, 13, 0)

/*
 *  CGU CT Kval Register
 */
#define CGU_CT_KVAL_PLL1K               GET_BITS(*IFX_CGU_CT_KVAL, 9, 0)

/*
 *  CGU PCM Control Register
 */
#define CGU_PCMCR_INT_SEL               GET_BITS(*IFX_CGU_PCMCR, 29, 28)
#define CGU_PCMCR_DCL_SEL               GET_BITS(*IFX_CGU_PCMCR, 27, 25)
#define CGU_PCMCR_MUXDCL                (*IFX_CGU_MUXDCL & (1 << 22))
#define CGU_PCMCR_MUXFSC                (*IFX_CGU_MUXDCL & (1 << 18))
#define CGU_PCMCR_PCM_SL                (*IFX_CGU_MUXDCL & (1 << 13))
#define CGU_PCMCR_DNTR                  GET_BITS(*IFX_CGU_PCMCR, 12, 11)
#define CGU_PCMCR_NTRS                  (*IFX_CGU_MUXDCL & (1 << 10))
#define CGU_PCMCR_AC97_EN               (*IFX_CGU_MUXDCL & (1 << 9))
#define CGU_PCMCR_CTTMUX                (*IFX_CGU_MUXDCL & (1 << 8))
#define CGU_PCMCR_CT_MUX_SEL            GET_BITS(*IFX_CGU_PCMCR, 7, 6)
#define CGU_PCMCR_FSC_DUTY              (*IFX_CGU_MUXDCL & (1 << 5))
#define CGU_PCMCR_CTM_SEL               (*IFX_CGU_MUXDCL & (1 << 4))

/*
 *  PCI Clock Control Register
 */
#define CGU_PCI_CR_PADSEL               (*IFX_CGU_PCI_CR & (1 << 31))
#define CGU_PCI_CR_RESSEL               (*IFX_CGU_PCI_CR & (1 << 30))
#define CGU_PCI_CR_PCID_H               GET_BITS(*IFX_CGU_PCI_CR, 23, 21)
#define CGU_PCI_CR_PCID_L               GET_BITS(*IFX_CGU_PCI_CR, 20, 18)

/*
 *  Pre-declaration of File Operations
 */
static ssize_t cgu_read(struct file *, char *, size_t, loff_t *);
static ssize_t cgu_write(struct file *, const char *, size_t, loff_t *);
static int cgu_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
static int cgu_open(struct inode *, struct file *);
static int cgu_release(struct inode *, struct file *);

/*
 *  Calculate PLL Frequency
 */
static inline u32 get_input_clock(int pll);
static inline u32 cal_dsm(int, u32, u32);
static inline u32 mash_dsm(int, u32, u32, u32);
static inline u32 ssff_dsm_1(int, u32, u32, u32);
static inline u32 ssff_dsm_2(int, u32, u32, u32, u32);
static inline u32 dsm(int, u32 M, u32, u32, u32, u32, u32);
static inline u32 cgu_get_pll0_fosc(void);
static inline u32 cgu_get_pll0_fps(int);
//static inline u32 cgu_get_pll0_fdiv(void);
static inline u32 cgu_get_pll1_fosc(void);
static inline u32 cgu_get_pll1_fps(void);
static inline u32 cgu_get_pll1_fdiv(void);
static inline u32 cgu_get_sys_freq(void);

/*
 * MPS SRAM backup mem pointer
 */
static  char    *argv;

/*
 *  Proc Filesystem
 */
static inline void proc_file_create(void);
static inline void proc_file_delete(void);
static int proc_read_version(char *buf, char **start, off_t offset, int count, int *eof, void *data);
static int proc_read_cgu(char *, char **, off_t, int, int *, void *);
static int proc_write_cgu(struct file *, const char *, unsigned long, void *);
#ifdef CONFIG_IFX_CLOCK_CHANGE
  static inline int stricmp(const char *, const char *);
  static int get_number(char **, int *, int);
  static inline void ignore_space(char **, int *);
#endif
EXPORT_SYMBOL(cgu_set_clockout);

/*
 *  Init Help Functions
 */
static inline int ifx_cgu_version(char *);

#ifdef CONFIG_IFX_CLOCK_CHANGE
  u32 cgu_set_clock(u32, u32, u32);
#endif

static struct file_operations cgu_fops = {
    owner:      THIS_MODULE,
    llseek:     no_llseek,
    read:       cgu_read,
    write:      cgu_write,
    ioctl:      cgu_ioctl,
    open:       cgu_open,
    release:    cgu_release
};

static struct proc_dir_entry* g_gpio_dir = NULL;
uint32_t base_ram_refresh_timing = 0;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static ssize_t cgu_read(struct file *file, char *buf, size_t count, loff_t *ppos) {
    return -EPERM;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static ssize_t cgu_write(struct file *file, const char *buf, size_t count, loff_t *ppos) {
    return -EPERM;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cgu_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) {
    int ret = 0;
    struct cgu_clock_rates rates;

    if ( _IOC_TYPE(cmd) != IFX_CGU_IOC_MAGIC
        || _IOC_NR(cmd) >= CGU_IOC_MAXNR )
        return -ENOTTY;

    if ( _IOC_DIR(cmd) & _IOC_READ )
        ret = !access_ok(VERIFY_WRITE, arg, _IOC_SIZE(cmd));
    else if ( _IOC_DIR(cmd) & _IOC_WRITE )
        ret = !access_ok(VERIFY_READ, arg, _IOC_SIZE(cmd));
    if ( ret )
        return -EFAULT;

    switch ( cmd ) {
        case IFX_CGU_GET_CLOCK_RATES:
            /*  Calculate Clock Rates   */
            rates.mips0     = cgu_get_mips_clock();
            rates.mips1     = cgu_get_mips_clock();
            rates.cpu       = cgu_get_cpu_clock();
            rates.io_region = cgu_get_io_region_clock();
            rates.fpi_bus1  = cgu_get_fpi_bus_clock();
            rates.fpi_bus2  = cgu_get_fpi_bus_clock();
            rates.pp32      = cgu_get_pp32_clock();
            rates.pci       = cgu_get_pci_clock();
            rates.mii0      = cgu_get_ethernet_clock();
            rates.mii1      = cgu_get_ethernet_clock();
            rates.usb       = cgu_get_usb_clock();
            rates.clockout0 = cgu_get_clockout(0);
            rates.clockout1 = cgu_get_clockout(1);
            rates.clockout2 = cgu_get_clockout(2);
            rates.clockout3 = cgu_get_clockout(3);
            /*  Copy to User Space      */
            copy_to_user((char*)arg, (char*)&rates, sizeof(rates));

            ret = 0;
            break;
        case IFX_CGU_IOC_VERSION:
            {
                struct ifx_cgu_ioctl_version version = {
                    .major = IFX_CGU_VER_MAJOR,
                    .mid   = IFX_CGU_VER_MID,
                    .minor = IFX_CGU_VER_MINOR
                };
                ret = copy_to_user((void *)arg, (void *)&version, sizeof(version));
            }
            break;
        default:
            ret = -ENOTTY;
    }

    return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cgu_open(struct inode *inode, struct file *file) {
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cgu_release(struct inode *inode, struct file *file) {
    return 0;
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get input clock frequency according to GPIO config
 *  Input:
 *    none
 *  Output:
 *    u32 --- frequency of input clock
\*------------------------------------------------------------------------------------------*/
static inline u32 get_input_clock(int pll) {
    return BASIC_INPUT_CLOCK_FREQUENCY;
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    common routine to calculate PLL frequency
 *  Input:
 *    num --- u32, numerator
 *    den --- u32, denominator
 *  Output:
 *    u32 --- frequency the PLL output
\*------------------------------------------------------------------------------------------*/
static inline u32 cal_dsm(int pll, u32 num, u32 den) {
    u64 res;

//    den <<= 1;  //  [(n+1)*freq] / [(m+1)*2], den = (m+1)*2
    res = get_input_clock(pll);
    res *= num;
    do_div(res, den);

    return res;
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    calculate PLL frequency following MASH-DSM
 *  Input:
 *    M   --- u32, denominator coefficient
 *    N   --- u32, numerator integer coefficient
 *    K   --- u32, numerator fraction coefficient
 *  Output:
 *    u32 --- frequency the PLL output
\*------------------------------------------------------------------------------------------*/
static inline u32 mash_dsm(int pll, u32 M, u32 N, u32 K) {
    u32 num = ((N + 1) << 10) + K;
    u32 den = (M + 1) << 10;

    return cal_dsm(pll, num, den);
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    calculate PLL frequency following SSFF-DSM (0.25 < fraction < 0.75)
 *  Input:
 *    M   --- u32, denominator coefficient
 *    N   --- u32, numerator integer coefficient
 *    K   --- u32, numerator fraction coefficient
 *  Output:
 *    u32 --- frequency the PLL output
\*------------------------------------------------------------------------------------------*/
static inline u32 ssff_dsm_1(int pll, u32 M, u32 N, u32 K) {
    u32 num = ((N + 1) << 11) + K + 512;
    u32 den = (M + 1) << 11;

    return cal_dsm(pll, num, den);
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    calculate PLL frequency following SSFF-DSM
 *    (fraction < 0.125 || fraction > 0.875)
 *  Input:
 *    M   --- u32, denominator coefficient
 *    N   --- u32, numerator integer coefficient
 *    K   --- u32, numerator fraction coefficient
 *  Output:
 *    u32 --- frequency the PLL output
\*------------------------------------------------------------------------------------------*/
static inline u32 ssff_dsm_2(int pll, u32 M, u32 N, u32 K, u32 modulo) {
    u32 offset[4] = {512, 1536, 2560, 3584};
    u32 num = ((N + 1) << 12) + K + offset[modulo];
    u32 den = (M + 1) << 12;

    return cal_dsm(pll, num, den);
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    calculate PLL frequency
 *  Input:
 *    M            --- u32, denominator coefficient
 *    N            --- u32, numerator integer coefficient
 *    K            --- u32, numerator fraction coefficient
 *    dsmsel       --- int, 0: MASH-DSM, 1: SSFF-DSM
 *    phase_div_en --- int, 0: 0.25 < fraction < 0.75
 *                          1: fraction < 0.125 || fraction > 0.875
 *  Output:
 *    u32          --- frequency the PLL output
\*------------------------------------------------------------------------------------------*/
static inline u32 dsm(int pll, u32 M, u32 N, u32 K, u32 dsmsel, u32 phase_div_en, u32 modulo) {
    if ( !dsmsel )
        return mash_dsm(pll, M, N, K);
    else
    {
        if ( !phase_div_en )
            return ssff_dsm_1(pll, M, N, K);
        else
            return ssff_dsm_2(pll, M, N, K, modulo);
    }
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get oscillate frequency of PLL0
 *  Input:
 *    none
 *  Output:
 *    u32 --- frequency of PLL0 Fosc
\*------------------------------------------------------------------------------------------*/
static inline u32 cgu_get_pll0_fosc(void) {
    if ( !CGU_PLL0_CFG_PLLEN )
        return 0;
    else if ( CGU_PLL0_BYPASS )
        return get_input_clock(0);
    else
        return dsm(0, CGU_PLL0_CFG_PLLM, CGU_PLL0_CFG_PLLN, 0, 0, 0, 0);
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get output frequency of PLL0 phase shifter
 *  Input:
 *    phase --- int, 1: 1.25 divider, 2: 1.5 divider
 *  Output:
 *    u32   --- frequency of PLL0 Fps
\*------------------------------------------------------------------------------------------*/
static inline u32 cgu_get_pll0_fps(int phase) {
    register u32 fps = cgu_get_pll0_fosc();

    switch ( phase ) {
        case 1:
            //  2
            if ( CGU_PLL0_PS_1_EN )
                fps = (fps + 1) >> 1;
            break;
        case 2:
            //  1.5
            if ( CGU_PLL0_PS_2_EN )
                fps = ((fps << 2) + 3) / 6;
            break;
        case 3:
            //  5 / 3
            fps = (fps * 6 + 5) / 10;
            break;
    }
    return fps;
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get output frequency of PLL0 output divider
 *  Input:
 *    none
 *  Output:
 *    u32 --- frequency of PLL0 Fdiv
\*------------------------------------------------------------------------------------------*/
//static u32 cgu_get_pll0_fdiv(void)
//{
//    //  PCI clock
//    return 33000000;
//}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get oscillate frequency of PLL1
 *  Input:
 *    none
 *  Output:
 *    u32 --- frequency of PLL1 Fosc
\*------------------------------------------------------------------------------------------*/
static inline u32 cgu_get_pll1_fosc(void) {
    if ( !CGU_PLL1_CFG_PLLEN ) {
        return 0;
    } else if ( CGU_PLL1_BYPASS ) {
        return get_input_clock(1);
    } else {
        if ( !CGU_PLL1_CFG_FRAC_EN ) {
            return dsm(1, CGU_PLL1_CFG_PLLM, CGU_PLL1_CFG_PLLN, 0, 0, 0, 0);
        } else {
            return dsm(1, CGU_PLL1_CFG_PLLM, CGU_PLL1_CFG_PLLN, CGU_PLL1_CFG_PLLK, CGU_PLL1_CFG_DSMSEL, CGU_PLL1_PHDIV_EN, CGU_PLL1_FMOD_S);
        }
    }
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get output frequency of PLL1 phase shifter
 *  Input:
 *    none
 *  Output:
 *    u32 --- frequency of PLL1 Fps
\*------------------------------------------------------------------------------------------*/
static inline u32 cgu_get_pll1_fps(void) {
    return cgu_get_pll1_fosc();
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get output frequency of PLL1 output divider
 *  Input:
 *    none
 *  Output:
 *    u32 --- frequency of PLL1 Fdiv
\*------------------------------------------------------------------------------------------*/
static inline u32 cgu_get_pll1_fdiv(void) {
    u32 div = CGU_PLL1_CFG_PLLD + 1;

    return (cgu_get_pll1_fosc() + (div >> 1)) / div;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline u32 cgu_get_sys_freq(void) {
    u32 sys_clk;

    switch ( CGU_SYS_SYS_SEL ) {
        case 0:
            sys_clk = cgu_get_pll0_fps(2); break;
        case 2:
            sys_clk = cgu_get_pll1_fosc(); break;
        default:
            sys_clk = cgu_get_pll0_fosc();
    }

    return sys_clk;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void proc_file_create(void) {
    struct proc_dir_entry *res;

    g_gpio_dir = proc_mkdir("driver/ifx_cgu", NULL);

    create_proc_read_entry("version",
                            0,
                            g_gpio_dir,
                            proc_read_version,
                            NULL);

    res = create_proc_entry("clk_setting",
                            0,
                            g_gpio_dir);

    if ( res ) {
        res->read_proc  = proc_read_cgu;
        res->write_proc = proc_write_cgu;
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void proc_file_delete(void) {
    remove_proc_entry("clk_setting", g_gpio_dir);
    remove_proc_entry("driver/ifx_cgu", NULL);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int proc_read_version(char *buf, char **start, off_t offset, int count, int *eof, void *data) {
    int len = 0;

    len += ifx_cgu_version(buf + len);

    if ( offset >= len ) {
        *start = buf;
        *eof = 1;
        return 0;
    }
    *start = buf + offset;
    if ( (len -= offset) > count )
        return count;
    *eof = 1;
    return len;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int proc_read_cgu(char *page, char **start, off_t off, int count, int *eof, void *data) {
    int len = 0;

    len += sprintf(page + off + len, "pll0 N = %d, M = %d\n", CGU_PLL0_CFG_PLLN, CGU_PLL0_CFG_PLLM);
    len += sprintf(page + off + len, "pll1 N = %d, M = %d, K = %d, modulo = %d\n", CGU_PLL1_CFG_PLLN, 
					CGU_PLL1_CFG_PLLM, CGU_PLL1_CFG_PLLK, CGU_PLL1_FMOD_S);
    len += sprintf(page + off + len, "pll0_fosc    = %d\n", cgu_get_pll0_fosc());
    len += sprintf(page + off + len, "pll0_fps(1)  = %d\n", cgu_get_pll0_fps(1));
    len += sprintf(page + off + len, "pll0_fps(2)  = %d\n", cgu_get_pll0_fps(2));
    len += sprintf(page + off + len, "pll1_fosc    = %d\n", cgu_get_pll1_fosc());
    len += sprintf(page + off + len, "pll1_fps     = %d\n", cgu_get_pll1_fps());
    len += sprintf(page + off + len, "pll1_fdiv    = %d\n", cgu_get_pll1_fdiv());
    len += sprintf(page + off + len, "mips0 clock  = %d\n", cgu_get_mips_clock());
    len += sprintf(page + off + len, "mips1 clock  = %d\n", cgu_get_mips_clock());
    len += sprintf(page + off + len, "cpu clock    = %d\n", cgu_get_cpu_clock());
    len += sprintf(page + off + len, "IO region    = %d\n", cgu_get_io_region_clock());
    len += sprintf(page + off + len, "FPI bus 1    = %d\n", cgu_get_fpi_bus_clock());
    len += sprintf(page + off + len, "FPI bus 2    = %d\n", cgu_get_fpi_bus_clock());
    len += sprintf(page + off + len, "PP32 clock   = %d\n", cgu_get_pp32_clock());
    len += sprintf(page + off + len, "PCI clock    = %d\n", cgu_get_pci_clock());
    len += sprintf(page + off + len, "Ethernet MII0= %d\n", cgu_get_ethernet_clock());
    len += sprintf(page + off + len, "Ethernet MII1= %d\n", cgu_get_ethernet_clock());
    len += sprintf(page + off + len, "USB clock    = %d\n", cgu_get_usb_clock());
    len += sprintf(page + off + len, "Clockout0    = %d\n", cgu_get_clockout(0));
    len += sprintf(page + off + len, "Clockout1    = %d\n", cgu_get_clockout(1));
    len += sprintf(page + off + len, "Clockout2    = %d\n", cgu_get_clockout(2));
    len += sprintf(page + off + len, "Clockout3    = %d\n", cgu_get_clockout(3));

    *eof = 1;

    return len;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cgu_set_pci_clock(int pci_clk) {
    int sys_freq, divisor;
    *IFX_CGU_PCI_CR |= (1 << 31);
    sys_freq = cgu_get_sys_freq();
    divisor = (sys_freq / pci_clk) - 1;

    /*--- printk(KERN_ERR "[%s] divisor=%d\n", __FUNCTION__, divisor); ---*/

    *IFX_CGU_IF_CLK = (*IFX_CGU_IF_CLK & ~(((1 << 5) - 1) << 20)) | (divisor << 20);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int proc_write_cgu(struct file *file, const char *buf, unsigned long count, void *data) {
    char str[2048];
    char *p;
    int len, rlen;

    len = count < sizeof(str) ? count : sizeof(str) - 1;
    rlen = len - copy_from_user(str, buf, len);
    while ( rlen && str[rlen - 1] <= ' ' )
        rlen--;
    str[rlen] = 0;
    for ( p = str; *p && *p <= ' '; p++, rlen-- );
    if ( !*p ) {
        return 0;
    }

#ifdef CONFIG_IFX_CLOCK_CHANGE
    if ( strncmp(p, "pci", 3) == 0 ) {
        int pci_clk;
        p += 3;
        ignore_space(&p, &rlen);
        pci_clk = get_number(&p, &rlen, 0);
        switch(pci_clk) {
            case 33:
                pci_clk = 33333333;
                break;
            case 50:
                pci_clk = 50000000;
                break;
            case 62:
                pci_clk = 62500000;
                break;
            default:
                printk("echo pci [33|50|62] > /proc/driver/ifx_cgu/clk_setting\n");
                return count;
        }
        cgu_set_pci_clock(pci_clk);
        return count;
    }
    if ( stricmp(p, "help") == 0 ) {
        printk("echo <cpu clk> <ddr clk> <fpi clk> > /proc/driver/ifx_cgu/clk_setting\n");
    } else {
        int cpu_clk = 0, ddr_clk = 0, fpi_clk = 0;

        cpu_clk = get_number(&p, &rlen, 0);
        ignore_space(&p, &rlen);
        if ( rlen > 0 )
        {
            ddr_clk = get_number(&p, &rlen, 0);
            ignore_space(&p, &rlen);
            if ( rlen > 0 )
                fpi_clk = get_number(&p, &rlen, 0);
        }

        if ( !fpi_clk )
            fpi_clk = ddr_clk;

        switch ( cpu_clk )
        {
        case 333:
        case 166:
        case 111:
        case 393:
        case 196:
        case 131:
            if ( cgu_set_clock(cpu_clk, ddr_clk, fpi_clk) )
                printk("fail in function \"cgu_set_clock\"\n");
            break;
        default:
            printk("do not support cpu_clk = %d\n", cpu_clk);
        }
    }
#endif

    return count;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_IFX_CLOCK_CHANGE
static inline int stricmp(const char *p1, const char *p2) {
    int c1, c2;

    while ( *p1 && *p2 ) {
        c1 = *p1 >= 'A' && *p1 <= 'Z' ? *p1 + 'a' - 'A' : *p1;
        c2 = *p2 >= 'A' && *p2 <= 'Z' ? *p2 + 'a' - 'A' : *p2;
        if ( (c1 -= c2) ) {
            return c1;
        }
        p1++;
        p2++;
    }

    return *p1 - *p2;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int get_number(char **p, int *len, int is_hex) {
    int ret = 0;
    int n = 0;

    if ( (*p)[0] == '0' && (*p)[1] == 'x' ) {
        is_hex = 1;
        (*p) += 2;
        (*len) -= 2;
    }

    if ( is_hex ) {
        while ( *len && ((**p >= '0' && **p <= '9') || (**p >= 'a' && **p <= 'f') || (**p >= 'A' && **p <= 'F')) ) {
            if ( **p >= '0' && **p <= '9' )
                n = **p - '0';
            else if ( **p >= 'a' && **p <= 'f' )
               n = **p - 'a' + 10;
            else if ( **p >= 'A' && **p <= 'F' )
                n = **p - 'A' + 10;
            ret = (ret << 4) | n;
            (*p)++;
            (*len)--;
        }
    } else {
        while ( *len && **p >= '0' && **p <= '9' ) {
            n = **p - '0';
            ret = ret * 10 + n;
            (*p)++;
            (*len)--;
        }
    }

    return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void ignore_space(char **p, int *len) {
    while ( *len && (**p <= ' ' || **p == ':' || **p == '.' || **p == ',') ) {
        (*p)++;
        (*len)--;
    }
}
#endif

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get frequency of MIPS (0: core, 1: DSP)
 *  Input:
 *    cpu --- int, 0: core, 1: DSP
 *  Output:
 *    u32 --- frequency of MIPS coprocessor (0: core, 1: DSP)
\*------------------------------------------------------------------------------------------*/
u32 cgu_get_mips_clock(void) {
    return CGU_SYS_CPUSEL ? cgu_get_io_region_clock() : cgu_get_sys_freq();
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get frequency of MIPS core
 *  Input:
 *    none
 *  Output:
 *    u32 --- frequency of MIPS core
\*------------------------------------------------------------------------------------------*/
u32 cgu_get_cpu_clock(void) {
    return cgu_get_mips_clock();
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get frequency of sub-system and memory controller
 *  Input:
 *    none
 *  Output:
 *    u32 --- frequency of sub-system and memory controller
\*------------------------------------------------------------------------------------------*/
u32 cgu_get_io_region_clock(void) {
    return (cgu_get_sys_freq() + 1) / (CGU_SYS_DDR_SEL ? 3 : 2);
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get frequency of FPI bus
 *  Input:
 *    fpi --- int, 1: FPI bus 1 (FBS1/Fast FPI Bus), 2: FPI bus 2 (FBS2)
 *  Output:
 *    u32 --- frequency of FPI bus
\*------------------------------------------------------------------------------------------*/
u32 cgu_get_fpi_bus_clock(void) {
    return CGU_SYS_FPI_SEL ? cgu_get_io_region_clock() / 2 : cgu_get_io_region_clock();
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get frequency of PP32 processor
 *  Input:
 *    none
 *  Output:
 *    u32 --- frequency of PP32 processor
\*------------------------------------------------------------------------------------------*/
u32 cgu_get_pp32_clock(void) {
    return cgu_get_pll0_fps(CGU_SYS_PPESEL ? 1 : 3);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
u32 cgu_get_qsb_clock(void) {
    return cgu_get_fpi_bus_clock() >> CGU_SYS_QOS_SEL;
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get frequency of PCI bus
 *  Input:
 *    none
 *  Output:
 *    u32 --- frequency of PCI bus
\*------------------------------------------------------------------------------------------*/
u32 cgu_get_pci_clock(void) {
    if ( CGU_PCI_CR_PADSEL ) {
        /*--- printk(KERN_ERR "[%s] CGU_IF_CLK_PCI_CLK %d sys_clk() %d\n", __FUNCTION__, CGU_IF_CLK_PCI_CLK, cgu_get_base_freq()); ---*/
        return cgu_get_sys_freq() / (CGU_IF_CLK_PCI_CLK + 1);
    } else {
        return 33000000;
    }
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get frequency of ethernet module (MII)
 *  Input:
 *    mii --- int, 0: mii0, 1: mii1
 *  Output:
 *    u32 --- frequency of ethernet module
\*------------------------------------------------------------------------------------------*/
u32 cgu_get_ethernet_clock(void) {
    return 50000000;
}

/*------------------------------------------------------------------------------------------*\
 *  Description:
 *    get frequency of USB
 *  Input:
 *    none
 *  Output:
 *    u32 --- frequency of USB
\*------------------------------------------------------------------------------------------*/
u32 cgu_get_usb_clock(void) {
    switch ( CGU_IF_CLK_USBSEL ) {
        case 0:
        case 3:
            return (get_input_clock(0) + 1) / 3;
        case 1:
            return get_input_clock(0);
        case 2:
            return (cgu_get_pll0_fps(3) + 12) / 25;
        default:
            break;
    }
    return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int clockouttable[4][4] =  {
    { 32768          , 1536 *    1000,  8192 *    1000,   12 * 1000000 }, /*--- CLKOUT0 ---*/
    { 40    * 1000000,   12 * 1000000,    24 * 1000000,   36 * 1000000 }, /*--- CLKOUT1 ---*/
    { 24    * 1000000,    0          ,     0          ,    0           }, /*--- CLKOUT2 ---*/
    { 12    * 1000000,   50 * 1000000, 32768          , 8192 *    1000 }, /*--- CLKOUT3 ---*/
};
static DEFINE_SPINLOCK(clklock);
/*--------------------------------------------------------------------------------*\
 *  Description:
 *    set frequency of CLK_OUT pin
 *  Input:
 *    clkout --- int, clock out pin number
 *    clk        frequency
 *  Output:
 *    u32    --- frequency of CLK_OUT pin
\*--------------------------------------------------------------------------------*/
u32 cgu_set_clockout(int clkout, u32 clk) {
    unsigned int i;
    if ( clk == 0 || clkout > 3 || clkout < 0 ) {
        return 0;
    }
    for(i = 0; i < sizeof(clockouttable) / sizeof(clockouttable[0]); i++) {
        if(clk == clockouttable[clkout][i]) {
            unsigned long flags;
            spin_lock_irqsave(&clklock, flags);
            *IFX_CGU_IF_CLK = SET_BITS(*IFX_CGU_IF_CLK, 15 - clkout * 2, 14 - clkout * 2, i);
            spin_unlock_irqrestore(&clklock, flags);
            return clk;
        }
    }
    return 0;
}
/*--------------------------------------------------------------------------------*\
 *  Description:
 *    get frequency of CLK_OUT pin
 *  Input:
 *    clkout --- int, clock out pin number
 *  Output:
 *    u32    --- frequency of CLK_OUT pin
\*--------------------------------------------------------------------------------*/
u32 cgu_get_clockout(int clkout) {
    unsigned int idx;

    if ( clkout > 3 || clkout < 0 ) {
        return 0;
    }
    idx =  GET_BITS(*IFX_CGU_IF_CLK, 15 - clkout * 2, 14 - clkout * 2);
    return clockouttable[clkout][idx];
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_IFX_CLOCK_CHANGE
#if defined(ENABLE_SET_CLOCK_PROFILING) && ENABLE_SET_CLOCK_PROFILING
static unsigned int g_extra_counter_start, g_extra_counter_end;
#endif
extern void ifx_update_asc_clock_settings(void);

extern unsigned long cycles_per_jiffy;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
extern struct clocksource clocksource_mips;
#else
static struct clocksource clocksource_mips;
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void __attribute__((aligned (32))) update_cgu(u32 cgu_sys, u32 mem_reconfig, u32 dll_start_point, u32 new_ram_refresh_value) {
    void *start, *end;
    *IFX_CGU_SYS = cgu_sys;
    __sync();

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

    //      printk("start = %p, end = %p", start, end);
    memcpy((u8*)argv, (u8*)MBX_BASEADDRESS, MPS_MEM_SEG_DATASIZE);
    memcpy((u8*)MBX_BASEADDRESS, (u8*)start, (end - start));

    __asm__("jr     %0"::"r"(MBX_BASEADDRESS));

    //        for (iptr = (void *)((unsigned int)start & ~(L1_CACHE_BYTES - 1));
    //             iptr < end; iptr += L1_CACHE_BYTES)
    //                cache_op(Fill, iptr);

    __asm__ __volatile__ (
            "startpoint:                                    \n"
    );

    BARRIER;
    if ( mem_reconfig ) {
        //        *(volatile u32 *)0xbf801110 = 0x010d;   //Put DDR into self Refresh mode
        *IFX_DDR_MC_DC17 |= 0x0100;   //Put DDR into self Refresh mode
        __sync();
        //        *(volatile u32 *)0xbf801000 = dll_start_point;
        *IFX_DDR_MC_DC00 = dll_start_point;
        __sync();
    }
    BARRIER;

    *IFX_CGU_UPDATE = 0x01;
    BARRIER;
    __sync();

    if ( mem_reconfig )
    {
        //        *(volatile u32 *)0xbf801030 = 0x0000;   //  Stop DDR
        *IFX_DDR_MC_DC03 = 0x0000;   //  Stop DDR
        __sync();
        *(volatile u32 *)0xbf8011c0 = new_ram_refresh_value & 0xfff;   //  new ram refresh cycle
        //        *(volatile u32 *)0xbf801030 = 0x0100;   //  Restart DDR
        *IFX_DDR_MC_DC03 = 0x0100;   //  Restart DDR
        __sync();

 #if defined(ENABLE_SET_CLOCK_PROFILING) && ENABLE_SET_CLOCK_PROFILING
        g_extra_counter_start = read_c0_count();
 #endif
        //        while ( (*(volatile u32 *)0xbf800070 & 0x08) == 0 );    //check for DLL relock
        while ( (*IFX_SDRAM_MC_STAT & 0x08) == 0 );    //check for DLL relock
 #if defined(ENABLE_SET_CLOCK_PROFILING) && ENABLE_SET_CLOCK_PROFILING
        g_extra_counter_end = read_c0_count();
 #endif
        //        *(volatile u32 *)0xbf801110 = 0x0d; //Put DDR out of self Refresh mode
        *IFX_DDR_MC_DC17 &= ~0x0100; //Put DDR out of self Refresh mode
    }

        __asm__("jr     %0"::"r"( end + 16 ));

    __asm__ __volatile__ (
            "endpoint:                                      \n"
    );

    BARRIER;
    memcpy((u8*)MBX_BASEADDRESS, (u8*)argv, MPS_MEM_SEG_DATASIZE);

#if 1 
    /* Put MVPE's into 'configuration state' */
    set_c0_mvpcontrol(MVPCONTROL_VPC);

    settc(1);

    if ((read_tc_c0_tcstatus() & TCSTATUS_A) || !(read_tc_c0_tchalt() & TCHALT_H)) {
//      printk("TCSTATUS_A = 0x%p\n", (read_tc_c0_tcstatus() & TCSTATUS_A));
//      BARRIER;
        write_tc_c0_tchalt(read_tc_c0_tchalt() & ~TCHALT_H);
        instruction_hazard();
    }

    /* take system out of configuration state */
    clear_c0_mvpcontrol(MVPCONTROL_VPC);

#endif

}
static unsigned int early_init_phase = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
static u64 clocksource_max_deferment(struct clocksource *cs)
{
    u64 max_nsecs, max_cycles;

    /*
     * Calculate the maximum number of cycles that we can pass to the
     * cyc2ns function without overflowing a 64-bit signed result. The
     * maximum number of cycles is equal to ULLONG_MAX/cs->mult which
     * is equivalent to the below.
     * max_cycles < (2^63)/cs->mult
     * max_cycles < 2^(log2((2^63)/cs->mult))
     * max_cycles < 2^(log2(2^63) - log2(cs->mult))
     * max_cycles < 2^(63 - log2(cs->mult))
     * max_cycles < 1 << (63 - log2(cs->mult))
     * Please note that we add 1 to the result of the log2 to account for
     * any rounding errors, ensure the above inequality is satisfied and
     * no overflow will occur.
     */
    max_cycles = 1ULL << (63 - (ilog2(cs->mult) + 1));

    /*
     * The actual maximum number of cycles we can defer the clocksource is
     * determined by the minimum of max_cycles and cs->mask.
     */
    max_cycles = min_t(u64, max_cycles, (u64) cs->mask);
    max_nsecs = clocksource_cyc2ns(max_cycles, cs->mult, cs->shift);

    /*
     * To ensure that the clocksource does not wrap whilst we are idle,
     * limit the time the clocksource can be deferred by 12.5%. Please
     * note a margin of 12.5% is used because this can be computed with
     * a shift, versus say 10% which would require division.
     */
    return max_nsecs - (max_nsecs >> 5);
}
#endif
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void update_sysclock(u32 cgu_sys)
{
    int i;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
    u64 temp;
#endif

    switch ( cgu_sys & 0x18 )
    {
    case 0x00: i = 333333333; break;
    case 0x08: i = 500000000; break;
    case 0x10: i = 393333333; break;
    default: i = 0;
    }
    if ( i )
    {
		/*If CPU frequency is the same as DDR*/
        if ( (cgu_sys & (1 << 2)) ) /*CPU_SEL, 1:CPU=DDR,0:CPU=SYS*/
        {
			/*calculate the DDR frequency so we know the new CPU frequency*/
			if ( (cgu_sys & 0x01) ) /*DDR_SEL, 1:DDR/SYS=1/3,0:DDR/SYS=1/2*/
                i = (i + 1) / 3;
            else
                i = (i + 1) / 2;
        }
        mips_hpt_frequency = (i + 1) / 2; /*half of the CPU frequency*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
        cycles_per_jiffy = (i + HZ) / (HZ * 2);
#endif

        /* Calclate a somewhat reasonable rating value */
	    clocksource_mips.rating = 200 + mips_hpt_frequency / 10000000;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
	    /* Find a shift value */
	    for ( i = 32; i > 0; i-- )
	    {
		    temp = (u64)NSEC_PER_SEC << i;
		    do_div(temp, mips_hpt_frequency);
		    if ( (temp >> 32) == 0 )
			    break;
	    }
    	clocksource_mips.shift = i;
    	clocksource_mips.mult = (u32)temp;

    	clocksource_mips.error = 0;
    	clocksource_mips.xtime_nsec = 0;
    	clocksource_calculate_interval(&clocksource_mips, tick_nsec);
#else
//        clocksource_set_clock(&clocksource_mips, mips_hpt_frequency);
//        clocksource_mips.max_idle_ns = clocksource_max_deferment(&clocksource_mips);
#endif
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void show_clock_config(void) {
#if 0
    printk(KERN_ERR "[%s] PLL0_CFG:\n"
            "    %s\n"
            "    %s\n"
            "    M Divider %d\n"
            "    N Divider %d\n"
            "    Divider %s\n"
            "    PS 1 EN %s (1/2) fractional divider\n"
            "    PS 2 EN %s (2/3) fractional divider\n"
            "    PLL1 fmodulo s1 %s\n"
            "    PLL1 fmodulo s0 %s\n"
            "    PLL0 %s\n",

            __FUNCTION__,
            *IFX_CGU_PLL0_CFG & (1 << 1) ? "enabled" : "disabled",
            *IFX_CGU_PLL0_CFG & (1 << 1) ? "locked" : "unlocked",
            (*IFX_CGU_PLL0_CFG >> 2) & ((1 << 4) - 1),
            (*IFX_CGU_PLL0_CFG >> 6) & ((1 << 8) - 1),
            *IFX_CGU_PLL0_CFG & (1 << 14) ? "enabled" : "disabled",
            *IFX_CGU_PLL0_CFG & (1 << 20) ? "enabled" : "disabled",
            *IFX_CGU_PLL0_CFG & (1 << 21) ? "enabled" : "disabled",
            *IFX_CGU_PLL0_CFG & (1 << 22) ? "enabled" : "disabled",
            *IFX_CGU_PLL0_CFG & (1 << 23) ? "enabled" : "disabled",
            *IFX_CGU_PLL0_CFG & (1 << 30) ? "bypassed" : "enabled");

    printk(KERN_ERR "[%s] PLL1_CFG:\n"
            "    %s\n"
            "    %s\n"
            "    M Divider %d\n"
            "    N Divider %d\n"
            "    Output Divider %d\n"
            "    K Divider %d\n"
            "    fractional divider %s\n"
            "    DSM Selection %s\n"
            "    Clock Track Enable %s\n"
            "    PLL1 %s\n"
            "    Phase Divider %s\n",
            __FUNCTION__,
            *IFX_CGU_PLL1_CFG & (1 << 1) ? "enabled" : "disabled",
            *IFX_CGU_PLL1_CFG & (1 << 1) ? "locked" : "unlocked",
            (*IFX_CGU_PLL1_CFG >> 2) & ((1 << 4) - 1),
            (*IFX_CGU_PLL1_CFG >> 6) & ((1 << 7) - 1),
            (*IFX_CGU_PLL1_CFG >> 13) & ((1 << 4) - 1),
            (*IFX_CGU_PLL1_CFG >> 17) & ((1 << 10) - 1),
            *IFX_CGU_PLL1_CFG & (1 << 27) ? "enabled" : "disabled",
            *IFX_CGU_PLL1_CFG & (1 << 28) ? "SSFF" : "MASH",
            *IFX_CGU_PLL1_CFG & (1 << 29) ? "enabled" : "disabled",
            *IFX_CGU_PLL1_CFG & (1 << 30) ? "bypassed" : "enabled",
            *IFX_CGU_PLL1_CFG & (1 << 31) ? "enabled" : "disabled");

    printk(KERN_ERR "[%s] IF_CLK:\n"
            "    USB_SEL: %s"
            "    CLK D3: %s"
            "    CLK D2: %s"
            "    CLK D1: %s"
            "    CLK D0: %s"
            "    PCI CLK source %s\n"
            "    PCI Master Busy %s\n"
            "    PCI Slave Busy %s\n"
            "    PCI DMA %s\n"
            "    PCI_CLK divider %d\n",

            __FUNCTION__,

            (*IFX_CGU_IF_CLK & (3 << 0)) == (0 << 0) ? "12 MHz (36MHz / 3)" :
                (*IFX_CGU_IF_CLK & (3 << 0)) == (1 << 0) ? "reserved" :
                (*IFX_CGU_IF_CLK & (3 << 0)) == (2 << 0) ? "12 MHz (PLL0 / 25)" : "12 Mhz (36 Mhz XTAL / 3)",

            (*IFX_CGU_IF_CLK & (3 << 8)) == (0 << 8) ? "12 MHz" :
                (*IFX_CGU_IF_CLK & (3 << 8)) == (1 << 8) ? "50 MHz" :
                (*IFX_CGU_IF_CLK & (3 << 8)) == (2 << 8) ? "8.192 MHz" : "32.768 KHz",

            (*IFX_CGU_IF_CLK & (3 << 10)) == (0 << 10) ? "25 MHz" :
                (*IFX_CGU_IF_CLK & (3 << 10)) == (1 << 10) ? "01 reserved" :
                (*IFX_CGU_IF_CLK & (3 << 10)) == (2 << 10) ? "10 reserved" : "11 reserved",

            (*IFX_CGU_IF_CLK & (3 << 12)) == (0 << 12) ? "40 MHz" :
                (*IFX_CGU_IF_CLK & (3 << 12)) == (1 << 12) ? "12 MHz" :
                (*IFX_CGU_IF_CLK & (3 << 12)) == (2 << 12) ? "24 MHz" : "36 MHz",

            (*IFX_CGU_IF_CLK & (3 << 14)) == (0 << 14) ? "32.768 KHz" :
                (*IFX_CGU_IF_CLK & (3 << 14)) == (1 << 14) ? "1.536 MHz" :
                (*IFX_CGU_IF_CLK & (3 << 14)) == (2 << 14) ? "8.192 MHz" : "12 MHz",

            *IFX_CGU_IF_CLK & (1 << 16) ? "external" : "internal",
            *IFX_CGU_IF_CLK & (1 << 17) ? "busy" : "idle",
            *IFX_CGU_IF_CLK & (1 << 18) ? "busy" : "idle",
            *IFX_CGU_IF_CLK & (1 << 19) ? "enabled" : "disabled",
            (*IFX_CGU_IF_CLK >> 20) & ((1 << 5) - 1));
#endif
}

static void dump_tc(int t)
{
        unsigned long val;

        settc(t);
        printk(KERN_DEBUG "VPE loader: TC index %d targtc %ld "
               "TCStatus 0x%lx halt 0x%lx\n",
               t, read_c0_vpecontrol() & VPECONTROL_TARGTC,
               read_tc_c0_tcstatus(), read_tc_c0_tchalt());

        printk(KERN_DEBUG " tcrestart 0x%lx\n", read_tc_c0_tcrestart());
        printk(KERN_DEBUG " tcbind 0x%lx\n", read_tc_c0_tcbind());

        val = read_c0_vpeconf0();
        printk(KERN_DEBUG " VPEConf0 0x%lx MVP %ld\n", val,
               (val & VPECONF0_MVP) >> VPECONF0_MVP_SHIFT);

        printk(KERN_DEBUG " c0 status 0x%lx\n", read_vpe_c0_status());
        printk(KERN_DEBUG " c0 cause 0x%lx\n", read_vpe_c0_cause());

        printk(KERN_DEBUG " c0 badvaddr 0x%lx\n", read_vpe_c0_badvaddr());
        printk(KERN_DEBUG " c0 epc 0x%lx\n", read_vpe_c0_epc());
}

void cgu_dump_vpe(int t)
{
        settc(t);

        printk(KERN_DEBUG "VPEControl 0x%lx\n", read_vpe_c0_vpecontrol());
        printk(KERN_DEBUG "VPEConf0 0x%lx\n", read_vpe_c0_vpeconf0());

        dump_tc(t);
}

u32 __attribute__((aligned (32))) cgu_set_clock(u32 cpu_clk, u32 ddr_clk, u32 fpi_clk)
{
    unsigned long sys_flag;
    unsigned long dmt_flag;
    unsigned int vpe_flag;
    // 1. Wert = CPU-Takt
    // 2. Wert = DDR-Takt
    // 3. Wert = Bitmaske fuer das Register
    u32 cgu_sys_templ[] = {
        333, 166, 0x00,
        333, 111, 0x01,
        166, 166, 0x04,
        111, 111, 0x05,
        393, 196, 0x10,
        196, 196, 0x14,
        131, 131, 0x15,
    };
    u32 cgu_sys = 0;
    u32 mem_reconfig;
    u32 dll_start_point;
 #if defined(ENABLE_SET_CLOCK_PROFILING) && ENABLE_SET_CLOCK_PROFILING
    u32 count_start, count_end;
 #endif
    int i;
    uint32_t new_ram_refresh_timing = 0;

    //-------
    // Disable DMA-Channel
    //   -Aufruf zur vorbereitung des stoppens der DMA-Services
    if(avm_prepare_busmaster_stop()) {
        printk(KERN_ERR "[%s:%d] Error while send 'prepare_stop' cmd to all registerd busmaster\n", __FUNCTION__, __LINE__);
        return -EFAULT;
    }
    //-------
    show_clock_config();

    if ( fpi_clk != ddr_clk && fpi_clk != ddr_clk / 2 ) {
        avm_run_busmaster();
        return -EINVAL;
    }

    for ( i = 0; i < sizeof(cgu_sys_templ); i += 3 ) {
        if ( cpu_clk == cgu_sys_templ[i + 0] && ddr_clk == cgu_sys_templ[i + 1]
            /* && (cgu_sys_templ[i + 2] & 0x18) == (*IFX_CGU_SYS & 0x18) */ ) {
            cgu_sys = cgu_sys_templ[i + 2];
            new_ram_refresh_timing = (base_ram_refresh_timing * cgu_sys_templ[i + 1]) / 1000;
            /*--- printk(KERN_ERR "new_ram_refresh_timing:%u\n", new_ram_refresh_timing); ---*/
            break;
        }
    }
    if ( i >= sizeof(cgu_sys_templ) ) {
        avm_run_busmaster();
        return -EINVAL;
    }

    dll_start_point = 0x1D1D;

    mem_reconfig = ((cgu_sys ^ *IFX_CGU_SYS) & 0x0019) ? 1 : 0;
    if ( mem_reconfig && ((cgu_sys ^ *IFX_CGU_SYS) & 0x0018) == 0 /* sys clk src is not changed */ ) {
        switch ( i ) {
            case 0 * 3:
            case 2 * 3:
                dll_start_point = 0x0152;
                break;
            case 1 * 3:
            case 3 * 3:
                dll_start_point = 0x0162;
                break;
        }
    }

    /*--- printk(KERN_ERR "[%s] cgu_sys 0x%x dll_start_point 0x%x mem_reconfig 0x%x\n", __FUNCTION__, cgu_sys, dll_start_point, mem_reconfig); ---*/

    if ( dll_start_point != 0x1D1D ) {
        printk("use optimized dll_start_point (%04x)\n", dll_start_point);
    }

#define IFX_CGU_SYS_DDR_SEL         (1 << 0)
#define IFX_CGU_SYS_DDR_SEL_250     (1 << 0)
#define IFX_CGU_SYS_CPU_CLOCK       (1 << 2)
#define IFX_CGU_SYS_SYS_SEL(x)      ((x) << 3)
#define IFX_CGU_SYS_SYS_SEL_333     (0 << 3)
#define IFX_CGU_SYS_SYS_SEL_393     (2 << 3)
#define IFX_CGU_SYS_PPE_SEL         (1 << 7)
#define IFX_CGU_SYS_QSB_SEL(x)      ((x) << 9)

    cgu_sys |= *IFX_CGU_SYS & 0x0680;

    /*--- printk(KERN_ERR "[%s] IFX_CGU_SYS 0x%x   (new) cpu_sys 0x%x\n", __FUNCTION__, *IFX_CGU_SYS, cgu_sys); ---*/
    if ( fpi_clk != ddr_clk ) {
        cgu_sys |= 1 << 6;
    }

#if 0
    printk(KERN_ERR "[%s] IFX_CGU_SYS: \n"
            "\tDLL_SEL %s\n"
            "\tCPU_SEL %s\n"
            "\tSYS_SEL %s\n"
            "\tmagic 6 %s\n"
            "\tPPE_SEL %s\n"
            "\tQSB_SEL %s\n",
            __FUNCTION__,
  /* DLL_SEL */  *IFX_CGU_SYS & (1 << 0) ? "1/3 of SYS_CLK" : "1/2 of SYS_CLK",
  /* CPU_SEL */  *IFX_CGU_SYS & (1 << 2) ? "same as DDR frquency" : "same as System frequency",
  /* SYS_SEL */  ((*IFX_CGU_SYS & (3 << 3)) == (0 << 3)) ? "333 MHz" : 
                    ((*IFX_CGU_SYS & (3 << 3)) == (1 << 3)) ? "x1 reserved" : 
                    ((*IFX_CGU_SYS & (3 << 3)) == (2 << 3)) ? "PLL1_CLK same as PLL1 base frequency 393 MHz" : "11 reserved",
  /* magic 6 */  *IFX_CGU_SYS & (1 << 6) ? "true" : "false",
  /* PPE_SEL */  *IFX_CGU_SYS & (1 << 7) ? "250 MHz" : "reserved",
  /* DQB_SEL */  ((*IFX_CGU_SYS & (3 << 9)) == (0 << 9)) ? "1/1 of FPI frequency" : 
                    ((*IFX_CGU_SYS & (3 << 9)) == (1 << 9)) ? "1/2 of FPI frequency" : 
                    ((*IFX_CGU_SYS & (3 << 9)) == (2 << 9)) ? "1/4 of FPI frequency" :  "1/4 of FPI frequency");
#endif


    //-------
    // Disable DMA-Channel
    //   -letztendliches Stoppen der DMA-Services, bzw. Abfrage ob dies geschehen ist
    if(avm_stop_busmaster()) {
        printk(KERN_ERR "[%s:%d] Error while send 'stop' cmd to all registerd busmaster\n", __FUNCTION__, __LINE__);
        return -EFAULT;
    }
    //-------

    local_irq_save(sys_flag);

    dmt_flag = dmt();
    vpe_flag = dvpe();

#if 1 
    /* Put MVPE's into 'configuration state' */
    set_c0_mvpcontrol(MVPCONTROL_VPC);

    settc(1);

    if ((read_tc_c0_tcstatus() & TCSTATUS_A) || !(read_tc_c0_tchalt() & TCHALT_H)) {
        write_tc_c0_tchalt(TCHALT_H);
        instruction_hazard();
    }

    /* take system out of configuration state */
    clear_c0_mvpcontrol(MVPCONTROL_VPC);
#endif

#if defined(ENABLE_SET_CLOCK_PROFILING) && ENABLE_SET_CLOCK_PROFILING
    g_extra_counter_start = g_extra_counter_end = 0;
    count_start = read_c0_count();
#endif

    update_cgu(cgu_sys, mem_reconfig, dll_start_point, new_ram_refresh_timing);
#if defined(ENABLE_SET_CLOCK_PROFILING) && ENABLE_SET_CLOCK_PROFILING
    count_end = read_c0_count();
#endif

    evpe(vpe_flag);
    emt(dmt_flag);

    local_irq_restore(sys_flag);

    update_sysclock(cpu_clk);
    ifx_update_asc_clock_settings(); // hbl <-- erzeugt eine Slave BCU Interrupt, warum der Vr9 nicht?

    //-------
    // Enable DMA-Channel
    //   -alle vorhin gestoppten DMA-channel wieder anschalten
    if(avm_run_busmaster()) {
        printk(KERN_ERR "[%s:%d] Error while send 'run' cmd to all registerd busmaster\n", __FUNCTION__, __LINE__);
        return -EFAULT;
    }
    //-------

    /*--- printk(KERN_ERR "[%s] c0_status=%u\n", __FUNCTION__, read_c0_status()); ---*/
    /*--- printk(KERN_ERR "[%s] line=%d\n", __FUNCTION__, __LINE__); ---*/
 #if defined(ENABLE_SET_CLOCK_PROFILING) && ENABLE_SET_CLOCK_PROFILING
    /*--- printk("Succeeded in switching to CPU %d, DDR %d, FPI %d, clock cycle %u. extra cycle %u\n", ---*/ 
	/*--- cpu_clk, ddr_clk, fpi_clk, count_end - count_start, g_extra_counter_end - g_extra_counter_start); ---*/
 #else
    /*--- printk("Succeeded in switching to CPU %d, DDR %d, FPI %d\n", cpu_clk, ddr_clk, fpi_clk); ---*/
 #endif

    return 0;
}
#endif

static inline int ifx_cgu_version(char *buf)
{
    return ifx_drv_ver(buf, "CGU", IFX_CGU_VER_MAJOR, IFX_CGU_VER_MID, IFX_CGU_VER_MINOR);
}

static int __init cgu_init(void)
{
    int ret;
    char ver_str[128] = {0};

    ret = register_chrdev(IFX_CGU_MAJOR, "ifx_cgu", &cgu_fops);
    if ( ret != 0 ) {
        printk(KERN_ERR "Can not register CGU device - %d\n", ret);
        return ret;
    }

#ifdef CONFIG_PROC_FS
    proc_file_create();
#endif

    /* malloc MPS backup mem */
    argv = kmalloc(MPS_MEM_SEG_DATASIZE, GFP_KERNEL);

    ifx_cgu_version(ver_str);
    printk(KERN_INFO "%s", ver_str);
 
    //---
    // Refresh  timings auslesen und Werte fuer alle moeglichen Taktstufen berechnen
    {
        uint32_t timing = (*(volatile u32 *)0xbf103010) & 0x1f;
        uint32_t ram_refresh_timing = (*(volatile u32 *)0xbf8011c0) & 0xfff;

        switch(timing) {
            case 0x05:  // 111, 111,
                base_ram_refresh_timing = ram_refresh_timing * 1000 / 111;
                break;
            case 0x15:  // 131, 131,
                base_ram_refresh_timing = ram_refresh_timing * 1000 / 131;
                break;
            case 0x04:  // 166, 166,
                base_ram_refresh_timing = ram_refresh_timing * 1000 / 166;
                break;
            case 0x14:  // 196, 196,
                base_ram_refresh_timing = ram_refresh_timing * 1000 / 196;
                break;
            case 0x01:  // 333, 111,
                base_ram_refresh_timing = ram_refresh_timing * 1000 / 111;
                break;
            case 0x00:  // 333, 166,
                base_ram_refresh_timing = ram_refresh_timing * 1000 / 166;
                break;
            default:
                printk(KERN_ERR "Unknown frequency. Assume 393/196.¸\n");
            case 0x10:  // 393, 196,
                base_ram_refresh_timing = ram_refresh_timing * 1000 / 196;
                break;
        }

        printk(KERN_ERR "clk-timing: timing:%u, ram_refresh_timing:%u, base_ram_refresh_timing:%u\n", timing, ram_refresh_timing, base_ram_refresh_timing);
    }
    //---

    return IFX_SUCCESS;
}

static void __exit cgu_exit(void)
{
#ifdef CONFIG_PROC_FS
    proc_file_delete();
#endif

    /* free MPS backup mem */
    kfree(argv);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
    if ( unregister_chrdev(IFX_CGU_MAJOR, "ifx_cgu") ) {
        printk(KERN_ERR "Can not unregister CGU device.");
        return;
    }
#else
    unregister_chrdev(IFX_CGU_MAJOR, "ifx_cgu");
#endif
}

static inline unsigned int ifx_get_sys_hz(void)
{
    switch ( *IFX_CGU_SYS & (0x03 << 3) )
    {
        case (0 << 3): return CLOCK_333M;
        case (1 << 3): return CLOCK_500M;
        case (2 << 3): if ( (*IFX_CGU_PLL1_CFG & ~(1 << 29)) == 0x9800f25f ) return CLOCK_333M;
        default:       return CLOCK_393M;
    }
}

/*!
  \fn       unsigned int ifx_get_cpu_hz(void)
  \brief    Get CPU speed
  \return   CPU clock hz
  \ingroup  IFX_CGU_API
 */
unsigned int ifx_get_cpu_hz(void)
{
    unsigned int sys_freq;

    sys_freq = ifx_get_sys_hz();
    if ( (*IFX_CGU_SYS & (1 << 2)) )
    {
        if ( (*IFX_CGU_SYS & 0x01) )
            sys_freq /= 3;
        else
            sys_freq >>= 1;
    }

    return sys_freq;
}
EXPORT_SYMBOL(ifx_get_cpu_hz);

/*!
  \fn       unsigned int ifx_get_ddr_hz(void)
  \brief    Get CPU speed
  \return   CPU clock hz
  \ingroup  IFX_CGU_API
 */
unsigned int ifx_get_ddr_hz(void)
{
    unsigned int ddr_freq = ifx_get_cpu_hz();

    if(!(*IFX_CGU_SYS & (1 << 2)) && !(*IFX_CGU_SYS & (1 << 0))) {
        ddr_freq /= 2;
    }

    return ddr_freq;
}
EXPORT_SYMBOL(ifx_get_ddr_hz);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init pci_clk_setup(char *p) {
    unsigned int clk;
    int rlen;
    if(!p)
        return 0;

    rlen = strlen(p);
    clk = get_number(&p, &rlen, 0);

    switch(clk) {
        case 33:
            clk = 33333333;
            break;
        case 50:
            clk = 50000000;
            break;
        case 62:
            clk = 62500000;
            break;
        default:
            printk("echo pci [33|50|62] > /proc/driver/ifx_cgu/clk_setting\n");
            return 0;
    }
    printk(KERN_INFO "[%s] set PCI clk %d kHz\n", __FUNCTION__, clk / 1000);
    cgu_set_pci_clock(clk);
    return 0;
}
__setup("pci_clk=", pci_clk_setup);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init sys_clk_setup(char *p) {
    int cpu_clk = 0, ddr_clk = 0, fpi_clk = 0;
    int rlen;

    if(!p)
        return 0;

    rlen = strlen(p);

    cpu_clk = get_number(&p, &rlen, 0);
    ignore_space(&p, &rlen);
    if ( rlen > 0 )
    {
        ddr_clk = get_number(&p, &rlen, 0);
        ignore_space(&p, &rlen);
        if ( rlen > 0 )
            fpi_clk = get_number(&p, &rlen, 0);
    }

    if ( !fpi_clk )
        fpi_clk = ddr_clk;

    switch ( cpu_clk ) {
        case 333:
        case 166:
        case 111:
        case 393:
        case 196:
        case 131:
            early_init_phase = 1;
            printk(KERN_INFO "[%s] set clks CPU %d MHz, DDR %d MHz, FPI %d MHz\n",
                    __FUNCTION__, cpu_clk, ddr_clk, fpi_clk);
            if ( cgu_set_clock(cpu_clk, ddr_clk, fpi_clk) ) {
                printk("fail in function \"cgu_set_clock\"\n");
            }
            early_init_phase = 0;
            break;
        default:
            printk("do not support cpu_clk = %d\n", cpu_clk);
    }
    return 0;
}
__setup("sys_clk=", sys_clk_setup);

/*!
  \fn       unsigned int ifx_get_fpi_hz(void)
  \brief    Get FPI bus speed
  \return   FPI bus clock hz
  \ingroup  IFX_CGU_API
 */
unsigned int ifx_get_fpi_hz(void)
{
    unsigned int sys_freq;

    sys_freq = ifx_get_sys_hz();
    if ( (*IFX_CGU_SYS & 0x01) )
        sys_freq /= 3;
    else
        sys_freq >>= 1;
    if ( (*IFX_CGU_SYS & (1 << 6)) )
        sys_freq >>= 1;

    return sys_freq;
}
EXPORT_SYMBOL(ifx_get_fpi_hz);

EXPORT_SYMBOL(cgu_get_mips_clock);
EXPORT_SYMBOL(cgu_get_cpu_clock);
EXPORT_SYMBOL(cgu_get_io_region_clock);
EXPORT_SYMBOL(cgu_get_fpi_bus_clock);
EXPORT_SYMBOL(cgu_get_pp32_clock);
EXPORT_SYMBOL(cgu_get_pci_clock);
EXPORT_SYMBOL(cgu_get_ethernet_clock);
EXPORT_SYMBOL(cgu_get_usb_clock);
EXPORT_SYMBOL(cgu_get_clockout);
#ifdef CONFIG_IFX_CLOCK_CHANGE
  EXPORT_SYMBOL(cgu_set_clock);
#endif

module_init(cgu_init);
module_exit(cgu_exit);
