/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2002 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * IKAN specific setup.
 */
#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/delay.h>
#include <linux/kernel.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/time.h>
#include <asm/mach_avm.h>
#include <vx180.h>

#ifdef CONFIG_AVM_POWERMETER
#include <linux/avm_power.h>
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/

/*--- #define ENABLE_CLOCK_GATING ---*/


#define PRINTK      printk
/*--- #define IKAN_CLK_DEBUG ---*/


#if defined(IKAN_CLK_DEBUG)
#define DBG_TRC     PRINTK
#else/*--- #if defined(IKAN_CLK_DEBUG) ---*/
#define DBG_TRC(arg...)
#endif/*--- #else ---*//*--- #if defined(IKAN_CLK_DEBUG) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init ikan_clk_switch_init(void);
static unsigned int ikan_get_mips_clock(void);
static unsigned int ikan_get_usb_clock(void);
static unsigned int ikan_get_ephy_clock(void);
static unsigned int ikan_get_pci_clock(void);
static unsigned int ikan_get_tdm_clock(void);
static inline unsigned int ikan_norm_clock(unsigned int clk, int flag);
unsigned int ikan_get_clock(enum _avm_clock_id clock_id);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct proc_dir_entry *ikan_change_clock_entry;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(ENABLE_CLOCK_GATING)
static void ikan_clock_status(char *buf) {
    unsigned int status = *(unsigned int *)VX180_CLK_GATE_CONTROL;
    void print_status(char *name, unsigned int bit) {
        sprintf(buf + strlen(buf), "\t%.31s: %s\n", name, status & (1 << bit) ? "enabled" : "disabled");
    }

    print_status("PCI System clock (PCI_CLK_EN)", 0);
    print_status("VPHY System clock (VPHY_CLK_EN)", 1);
    print_status("AMC System clock (EBI_CLK_EN)", 2);
    print_status("EMAC1 System clock (GIGE1_CLK_EN)", 3);
    print_status("GIGE1 APU clock(GIGE1_APU_CLK_EN) ", 4);
    print_status("GIGE2 System clock (GIGE2_CLK_EN)", 5);
    print_status("GIGE2 APU clock (GIGE2_APU_CLK_EN)", 6);
    print_status("VAP System clock (VAP_SYS_CLK_EN)", 7);
    print_status("VAP APU clock (VAP_APU_CLK_EN)", 8);
    print_status("BMU APU clock (BMU_APU_CLK_EN)", 9);
    print_status("BMU System clock (BMU_SYS_CLK_EN)", 10);
    print_status("WAP APU clock (WAP_APU_CLK_EN)", 11);
    print_status("WAP System clock (WAP_SYS_CLK_EN)", 12);
    print_status("USB Host System clock (USBH_CLK_EN)", 13);
    print_status("SAP APU clock (SAP_APU_CLK_EN)", 14);
    print_status("SAP System clock (SAP_APU_CLK_EN)", 15);

}
#endif /*--- #if defined(ENABLE_CLOCK_GATING) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ikan_change_clock_read(char* buf, char **start, off_t offset, int count, int *eof, void *data) {
    unsigned int len;
    unsigned int cpu_clk    = ikan_get_clock(avm_clock_id_cpu);
    unsigned int system_clk = ikan_get_clock(avm_clock_id_system);
    unsigned int usb_clk    = ikan_get_clock(avm_clock_id_usb);
    unsigned int usb2_clk   = ikan_get_clock(avm_clock_id_usb2);
    unsigned int ddr_clk    = ikan_get_clock(avm_clock_id_ddr);
    unsigned int ephy_clk   = ikan_get_clock(avm_clock_id_ephy);
    unsigned int pci_clk    = ikan_get_clock(avm_clock_id_pci);
    unsigned int pcl66_clk  = ikan_get_clock(avm_clock_id_pci66);
    unsigned int ap_clk     = ikan_get_clock(avm_clock_id_ap);
    unsigned int _21xx_clk   = ikan_get_clock(avm_clock_id_21xx);

    sprintf(buf, "Clocks: "
                 " CPU: %u %cHz"
                 " SYSTEM: %u %cHz"
                 " USB: %u %cHz"
                 " USB2: %u %cHz"
                 " DDR: %u %cHz"
                 " EPHY: %u %cHz"
                 " PCI: %u %cHz"
                 " PCL66: %u %cHz"
                 " AP: %u %cHz"
                 " 21xx: %u %cHz"
                 "\n",
            ikan_norm_clock(cpu_clk, 0), ikan_norm_clock(cpu_clk, 1),
            ikan_norm_clock(system_clk, 0), ikan_norm_clock(system_clk, 1),
            ikan_norm_clock(usb_clk, 0), ikan_norm_clock(usb_clk, 1),
            ikan_norm_clock(usb2_clk, 0), ikan_norm_clock(usb2_clk, 1),
            ikan_norm_clock(ddr_clk, 0), ikan_norm_clock(ddr_clk, 1),
            ikan_norm_clock(ephy_clk, 0), ikan_norm_clock(ephy_clk, 1),
            ikan_norm_clock(pci_clk, 0), ikan_norm_clock(pci_clk, 1),
            ikan_norm_clock(pcl66_clk, 0), ikan_norm_clock(pcl66_clk, 1),
            ikan_norm_clock(ap_clk, 0), ikan_norm_clock(ap_clk, 1),
            ikan_norm_clock(_21xx_clk, 0), ikan_norm_clock(_21xx_clk, 1));

#if defined(ENABLE_CLOCK_GATING)
    ikan_clock_status(buf);
#endif /*--- #if defined(ENABLE_CLOCK_GATING) ---*/
    len = strlen(buf);
    return len;
}

/*------------------------------------------------------------------------------------------*\


\*------------------------------------------------------------------------------------------*/
#if defined(ENABLE_CLOCK_GATING)
int ikan_change_clock_write(struct file *file, const char __user *buff, unsigned long count, void *data) {
    unsigned int bit;
    if(strstr(buff, "SAP_SYS_CLK_EN")) { bit = 0 ; 
    } else if(strstr(buff, "SAP_APU_CLK_EN")) { bit = 1 ; 
    } else if(strstr(buff, "USBH_CLK_EN")) { bit = 2 ; 
    } else if(strstr(buff, "WAP_SYS_CLK_EN")) { bit = 3 ; 
    } else if(strstr(buff, "WAP_APU_CLK_EN")) { bit = 4 ; 
    } else if(strstr(buff, "BMU_SYS_CLK_EN")) { bit = 5 ; 
    } else if(strstr(buff, "BMU_APU_CLK_EN")) { bit = 6 ; 
    } else if(strstr(buff, "VAP_APU_CLK_EN")) { bit = 7 ; 
    } else if(strstr(buff, "VAP_SYS_CLK_EN")) { bit = 8 ; 
    } else if(strstr(buff, "GIGE2_APU_CLK_EN")) { bit = 9 ; 
    } else if(strstr(buff, "GIGE2_CLK_EN")) { bit = 10 ; 
    } else if(strstr(buff, "GIGE1_APU_CLK_EN")) { bit = 11 ; 
    } else if(strstr(buff, "GIGE1_CLK_EN")) { bit = 12 ; 
    } else if(strstr(buff, "EBI_CLK_EN")) { bit = 13 ; 
    } else if(strstr(buff, "VPHY_CLK_EN")) { bit = 14 ; 
    } else if(strstr(buff, "PCI_CLK_EN")) { bit = 15 ; 
    } else { printk(KERN_ERR "Invallid keyword, availble: \n"
                           "\tSAP_SYS_CLK_EN SAP_APU_CLK_EN USBH_CLK_EN WAP_SYS_CLK_EN WAP_APU_CLK_EN\n"
                           "\tBMU_SYS_CLK_EN BMU_APU_CLK_EN VAP_APU_CLK_EN VAP_SYS_CLK_EN GIGE2_APU_CLK_EN\n"
                           "\tGIGE2_CLK_EN GIGE1_APU_CLK_EN GIGE1_CLK_EN EBI_CLK_EN VPHY_CLK_EN PCI_CLK_EN\n");
        return -EFAULT;
    }
    if(strstr(buff, "disable-all")) {
        printk(KERN_ERR "*VX180_CLK_GATE_CONTROL = 0\n");
        *(unsigned int *)VX180_CLK_GATE_CONTROL  = 0;
    } else if(strstr(buff, "disable")) {
        printk(KERN_ERR "*VX180_CLK_GATE_CONTROL &= ~(1 << %d)   0x%x\n", bit, ~(1 << bit));
        *(unsigned int *)VX180_CLK_GATE_CONTROL &= ~(1 << bit);
    } else if(strstr(buff, "enable")) {
        printk(KERN_ERR "*VX180_CLK_GATE_CONTROL |= (1 << %d)   0x%x\n", bit, (1 << bit));
        *(unsigned int *)VX180_CLK_GATE_CONTROL |= 1 << bit;
    }
    return count;
}
#endif /*--- #if defined(ENABLE_CLOCK_GATING) ---*/

static int __init ikan_clk_switch_init(void) {
    ikan_change_clock_entry = create_proc_entry("clocks", 06666, NULL);
    if(ikan_change_clock_entry) {
        ikan_change_clock_entry->read_proc = ikan_change_clock_read;
#if defined(ENABLE_CLOCK_GATING)
        ikan_change_clock_entry->write_proc = ikan_change_clock_write;
#else /*--- #if defined(ENABLE_CLOCK_GATING) ---*/
        ikan_change_clock_entry->write_proc = NULL;
#endif /*--- #else ---*/ /*--- #if defined(ENABLE_CLOCK_GATING) ---*/
        ikan_change_clock_entry->data = NULL;
    }
    return 0;
};

late_initcall(ikan_clk_switch_init);


#if defined(IKAN_CLK_DEBUG)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static const char *ikan_name_clock_id(enum _avm_clock_id clock_id) {
    return
    clock_id == avm_clock_id_non    ? "id_non"    :
    clock_id == avm_clock_id_cpu    ? "id_cpu"    :
    clock_id == avm_clock_id_system ? "id_system" :
    clock_id == avm_clock_id_usb    ? "id_usb"    :
    clock_id == avm_clock_id_usb2   ? "id_usb2"   :
    clock_id == avm_clock_id_ddr    ? "id_ddr"    :
    clock_id == avm_clock_id_ephy   ? "id_ephy"   :
    clock_id == avm_clock_id_pci    ? "id_pci"    :
    clock_id == avm_clock_id_pci66  ? "id_pci66"  :
    clock_id == avm_clock_id_ap     ? "id_ap"     :
    clock_id == avm_clock_id_21xx   ? "id_21xx"   : "id_unknown";
}
#endif/*--- #if defined(IKAN_CLK_DEBUG) ---*/

/*--------------------------------------------------------------------------------*\
 * flag = 0: liefere NormFrequenz sonst 'K', 'M', ' '
\*--------------------------------------------------------------------------------*/
static inline unsigned int ikan_norm_clock(unsigned int clk, int flag) {
    const unsigned int MHZ = 1000000;
    if(flag == 0) {
        return ((clk / MHZ) * MHZ) == clk ? clk / MHZ : ((clk / 1000) * 1000) == clk ? clk / 1000 : clk;
    } 
    return ((clk / MHZ) * MHZ) == clk ? 'M': ((clk / 1000) * 1000) == clk ? 'K' : ' ';
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int __init ikan_clk_init(void) {
    return 0;
}
arch_initcall(ikan_clk_init);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ikan_compute_clock(unsigned int xtal, unsigned int pre_div, void *pll, unsigned int shift, unsigned int bits, unsigned int post_div) {
    unsigned int clk = xtal / pre_div;
    DBG_TRC("[ikan_clk] clk(%d) = xtal(%d) / pre_div(%d)\n", clk, xtal, pre_div);
    if((shift == 0) && (bits == 0)) {
        clk *= (unsigned int)pll;
        DBG_TRC("\tclk(%d) *= mult(%d)\n", clk, (unsigned int)pll);
    } else {
        int mult = (*(volatile unsigned int *)pll >> shift) & ((1 << bits) - 1);
        clk *= mult;
        DBG_TRC("\tclk(%d) *= mult(%d) (Register 0x%08x)\n", clk, mult, *(unsigned int *)pll);
    }
    clk /= post_div;
    DBG_TRC("\tclk(%d) /= post_div(%d)\n", clk, post_div);
    return clk;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ikan_get_clock(enum _avm_clock_id clock_id) {
    unsigned int clk = 0;
    switch(clock_id) {
        /*----------------------------------------------------------------------------------*\
        \*----------------------------------------------------------------------------------*/
        case avm_clock_id_21xx:
            clk = ikan_compute_clock(
                    default_ikan_vdsl_xtal, 
                    2, /* prae div */
                    (void *)VX180_DSP_PLL_CONTROL,  /* PLL */
                    8 /* shift */, 
                    6 /* bits */, 
                    1 /* post div */
                   );
            break;

        /*----------------------------------------------------------------------------------*\
        \*----------------------------------------------------------------------------------*/
        case avm_clock_id_cpu:
            clk = ikan_compute_clock(
                        default_ikan_lan_xtal, 
                        1, 
                        (void *)VX180_PROC_PLL_CONTROL, 
                        8 /* shift */, 
                        6 /* bits */, 
                        2
                  );
            break;

        /*----------------------------------------------------------------------------------*\
        \*----------------------------------------------------------------------------------*/
        case avm_clock_id_ephy:
            clk = ikan_compute_clock(
                        default_ikan_lan_xtal, 
                        1, 
                        (void *)40,  /* fester Wert */
                        0 /* shift */, 
                        0 /* bits */, 
                        8
                  );
            break;

        case avm_clock_id_pci:
            clk = ikan_compute_clock(
                        default_ikan_lan_xtal, 
                        1, 
                        (void *)40,  /* fester Wert */
                        0 /* shift */, 
                        0 /* bits */, 
                        30
                  );
            break;

        case avm_clock_id_pci66:
            clk = ikan_compute_clock(
                        default_ikan_lan_xtal, 
                        1, 
                        (void *)40,  /* fester Wert */
                        0 /* shift */, 
                        0 /* bits */, 
                        15
                  );
            break;

        /*----------------------------------------------------------------------------------*\
        \*----------------------------------------------------------------------------------*/
        case avm_clock_id_system:
            clk = ikan_compute_clock(
                        default_ikan_lan_xtal, 
                        1, 
                        (void *)VX180_SYSTEM_PLL_CONTROL, 
                        8 /* shift */, 
                        6 /* bits */, 
                        6
                  );
            break;

        case avm_clock_id_ap:
            clk = ikan_get_clock(avm_clock_id_system) * 2;
            break;

        case avm_clock_id_ddr:
            clk = ikan_compute_clock(
                        default_ikan_lan_xtal, 
                        1, 
                        (void *)VX180_SYSTEM_PLL_CONTROL, 
                        8 /* shift */, 
                        6 /* bits */, 
                        3
                  );
            break;


        /*----------------------------------------------------------------------------------*\
        \*----------------------------------------------------------------------------------*/
            clk = 100000000;
            break;

        /*----------------------------------------------------------------------------------*\
        \*----------------------------------------------------------------------------------*/
        case avm_clock_id_usb:
            clk = ikan_compute_clock(
                        default_ikan_lan_xtal, 
                        5, 
                        (void *)48  /* fester Wert 48 */,
                        0 /* shift */, 
                        0 /* bits */, 
                        20
                  );
            break;

        case avm_clock_id_usb2:
            clk = ikan_compute_clock(
                        default_ikan_lan_xtal, 
                        5, 
                        (void *)48  /* fester Wert 48 */,
                        0 /* shift */, 
                        0 /* bits */, 
                        5
                  );
            break;
        /*----------------------------------------------------------------------------------*\
        \*----------------------------------------------------------------------------------*/

        default: 
            PRINTK(KERN_ERR"ikan_get_clock: unknown id=%d\n", clock_id);
            break;
    }
    DBG_TRC("ikan_get_clock: %s %u %cHz\n", ikan_name_clock_id(clock_id), ikan_norm_clock(clk, 0), ikan_norm_clock(clk, 1));
    return clk;
}

EXPORT_SYMBOL(ikan_get_clock);

