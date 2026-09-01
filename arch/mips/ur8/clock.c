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
 * UR8 specific setup.
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
#include <linux/sysdev.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/mips-boards/generic.h>
#include <asm/prom.h>
#include <asm/mach-ur8/ur8.h>
#include <asm/mach-ur8/ur8int.h>
#include <asm/mach_avm.h>
#include <asm/mach-ur8/hw_boot.h>
#include <asm/mach-ur8/hw_emif.h>
#include <asm/time.h>

#ifdef CONFIG_AVM_POWERMETER
#include <linux/avm_power.h>
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/


#define PRINTK      printk
/*--- #define UR8_CLK_DEBUG ---*/


#if defined(UR8_CLK_DEBUG)
#define DBG_TRC     PRINTK
#else/*--- #if defined(UR8_CLK_DEBUG) ---*/
#define DBG_TRC(arg...)
#endif/*--- #else ---*//*--- #if defined(UR8_CLK_DEBUG) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static spinlock_t ur8_clock_spinlock;
#define MHZ     1000000
#define default_ur8_lan_xtal  24*MHZ
const unsigned int ur8_lan_xtal = default_ur8_lan_xtal;

#if defined(CONFIG_UR8_CLOCK_SWITCH)
struct _ur8_clk_notify_system_clock_change {
    struct _ur8_clk_notify_system_clock_change *next;
    enum _avm_clock_id clock_id;
    unsigned int (*notify)(enum _avm_clock_id, unsigned int);
};
static spinlock_t ur8_clock_switch_spinlock;
static struct _ur8_clk_notify_system_clock_change *ur8_system_clock_notify_first;
static struct proc_dir_entry *ur8_change_clock_entry;
static unsigned int ur8_set_pci_clock(unsigned int clk);

#define MAX_CLK_LISTENTRY       10
static struct _clk_listentry  {
    volatile unsigned int locked;
    struct _ur8_clk_notify_system_clock_change entry;
} clk_listentry[MAX_CLK_LISTENTRY];


static void test_bus_speed(void);

/*--------------------------------------------------------------------------------*\
 * kmalloc darf hier nicht verwendet werden (zu frueh!)
\*--------------------------------------------------------------------------------*/
static struct _ur8_clk_notify_system_clock_change *alloc_clk_listentry(void) {
    unsigned int i;
    unsigned long flags;
    spin_lock_irqsave(&ur8_clock_spinlock, flags);
    for(i = 0; i < MAX_CLK_LISTENTRY; i++) {
        if(clk_listentry[i].locked == 0) {
            clk_listentry[i].locked = 1;
            spin_unlock_irqrestore(&ur8_clock_spinlock, flags);
            /*--- PRINTK("[clk]alloc_clk_listentry: %d\n", i); ---*/
            return &clk_listentry[i].entry;
        }
    }
    spin_unlock_irqrestore(&ur8_clock_spinlock, flags);
    PRINTK("[clk]alloc_clk_listentry: failed!\n");
    return NULL;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void free_clk_listentry(struct _ur8_clk_notify_system_clock_change *entry) {
    unsigned int i;
    unsigned long flags;
    spin_lock_irqsave(&ur8_clock_spinlock, flags);
    for(i = 0; i < MAX_CLK_LISTENTRY; i++) {
        if(clk_listentry[i].locked && (&clk_listentry[i].entry == entry)) {
            clk_listentry[i].locked = 0;
            /*--- PRINTK("[clk]free_clk_listentry: %d\n", i); ---*/
            spin_unlock_irqrestore(&ur8_clock_spinlock, flags);
            return;
        }
    }
    spin_unlock_irqrestore(&ur8_clock_spinlock, flags);
    PRINTK("[clk]free_clk_listentry: failed!\n");
    return;
}
#endif /*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_UR8_CLOCK_SWITCH)
static int __init ur8_clk_switch_init(void);
static unsigned int ur8_call_clock_notifier(enum _avm_clock_id clock_id);
#endif
static unsigned int ur8_set_mips_clock(unsigned int clk, int startup);
static unsigned int ur8_set_system_clock(unsigned int clk, int startup);
static unsigned int ur8_set_usb_clock(unsigned int clk);
static unsigned int ur8_set_peripheral_clock(unsigned int clk);
static unsigned int ur8_set_dsp_clock(unsigned int clk);
static unsigned int ur8_trigger_notifier(enum _avm_clock_id clock_id);
static unsigned int ur8_set_c55x_clock(unsigned int clk);
static unsigned int ur8_set_vlynq_clock(unsigned int clk);
static unsigned int ur8_set_tdm_clock(unsigned int clk);

static unsigned int ur8_get_mips_clock(void);
static unsigned int ur8_get_usb_clock(void);
static unsigned int ur8_get_ephy_clock(void);
static unsigned int ur8_get_pci_clock(void);
static unsigned int ur8_get_tdm_clock(void);
static unsigned int ur8_get_dsp_c55_system_clock(enum _avm_clock_id clock_id);
static unsigned int ur8_clk_set_emif(struct _ur8_clock_pll *Pll, unsigned int mult, 
                                                                 unsigned int pre_div,
                                                                 unsigned int always_set); 

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
typedef struct _emif_async_chip_timing_ {
    char *name;
    unsigned int timing;
    unsigned int t_cs;      /*--- chip enable setup time ---*/
    unsigned int t_ce;
    unsigned int t_oe;
    unsigned int t_acc;
    unsigned int t_as;      /*--- address setup time ---*/
    unsigned int t_ch;
    unsigned int t_rc;
    unsigned int t_wc;
    unsigned int t_wp;
    unsigned int t_wph;
    unsigned int t_ah;      /*--- address hold time ---*/
    unsigned int asize;
} emif_async_chip_timing;

typedef struct _emif_sync_chip_timing_ {
    char *name;
    unsigned int timing;
    unsigned int t_rfc;
    unsigned int t_rrd;
    unsigned int t_rc;
    unsigned int t_ras;
    unsigned int t_wr;
    unsigned int t_rcd;
    unsigned int t_rp;
} emif_sync_chip_timing;

/*------------------------------------------------------------------------------------------*\
 * um Rundungsfehler beim Rechnen mit Integer zu vermeiden alle Werte * 10
\*------------------------------------------------------------------------------------------*/
emif_async_chip_timing flash_timing[1] = {
    {
        .name = "Flash -9",
        .timing = 90,
        .t_cs = 0 * 10,
        .t_ce = 90 * 10,
        .t_oe = 35 * 10,
        .t_acc = 90 * 10,
        .t_as = 0 * 10,
        .t_ch = 0 * 10,
        .t_rc = 90 * 10,
        .t_wc = 90 * 10,
        .t_wp = 35 * 10,
        .t_wph = 30 * 10,
        .t_ah = 45 * 10,
        .asize = 1
    }
};

emif_sync_chip_timing sdram_timing[3] = {
    {
        .name = "SRAM -6",
        .timing = 60,
        .t_rfc = 60 * 10,
        .t_rrd = 12 * 10,
        .t_rc = 60 * 10,
        .t_ras = 42 * 10,
        .t_wr = 12 * 10,
        .t_rcd = 18 * 10,
        .t_rp = 18 * 10
    },
    {
        .name = "SRAM -7",
        .timing = 70,
        .t_rfc = 63 * 10,
        .t_rrd = 14 * 10,
        .t_rc = 63 * 10,
        .t_ras = 45 * 10,
        .t_wr = 14 * 10,
        .t_rcd = 18 * 10,
        .t_rp = 18 * 10
    },
    {
        .name = "SRAM -75",
        .timing = 75,
        .t_rfc = 67 * 10,
        .t_rrd = 15 * 10,
        .t_rc = 67 * 10,
        .t_ras = 45 * 10,
        .t_wr = 15 * 10,
        .t_rcd = 20 * 10,
        .t_rp = 20 * 10
    }
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _valid_clk {
    unsigned int pre_div;
    unsigned int mult;
    unsigned int freq;
    unsigned int startup_sync   : 1;
    unsigned int startup_mips   : 1;
    unsigned int startup_system : 1;
    unsigned int startup_tdm    : 1;
};


/*--------------------------------------------------------------------------------*\
 * ACHTUNG! PLL's sind nur fuer den Bereich von 200 bis 550 MHz freigegeben!
\*--------------------------------------------------------------------------------*/
struct _valid_clk valid_pll_setting[] = {
    { freq: default_ur8_lan_xtal, pre_div: 0, mult:0},       /* 24 MHz Umschalten in den PLL-BYPASS */
    { freq: (default_ur8_lan_xtal * 25) / 3, pre_div: 2, mult:24},    /* 200 MHz */
    { freq: default_ur8_lan_xtal * 9, pre_div: 0, mult:8},            /* 216 MHz */
    { freq: default_ur8_lan_xtal * 10, pre_div: 0, mult:9},           /* 240 MHz */
    { freq: (default_ur8_lan_xtal * 22) / 2, pre_div: 1, mult:21},    /* 264 MHz */
    { freq: (default_ur8_lan_xtal * 25) / 2, pre_div: 1, mult:24},    /* 300 MHz */
    { startup_system: 1, startup_sync: 1, startup_mips: 1, freq: (default_ur8_lan_xtal * 15) , pre_div: 0, mult:14},     /* 360 MHz */
    { freq: (default_ur8_lan_xtal * 16) , pre_div: 0, mult:15},     /* 384 MHz */
    { freq: (default_ur8_lan_xtal * 17) , pre_div: 0, mult:16},     /* 408 MHz */
    { freq: (default_ur8_lan_xtal * 18) , pre_div: 0, mult:17},     /* 432 MHz */
    { freq: (default_ur8_lan_xtal * 19) , pre_div: 0, mult:18},     /* 456 MHz */
    { freq: (default_ur8_lan_xtal * 20) , pre_div: 0, mult:19},     /* 480 MHz */
    { freq: (default_ur8_lan_xtal * 21) , pre_div: 0, mult:20},     /* 504 MHz */

    { freq: (default_ur8_lan_xtal * 22) , pre_div: 0, mult:21},     /* 528 MHz */
    { freq: (default_ur8_lan_xtal * 23) , pre_div: 0, mult:22},     /* 552 MHz */
    { freq: (default_ur8_lan_xtal * 24) , pre_div: 0, mult:23},     /* 576 MHz */
    { 0, 0, 0, 0, 0, 0, 0 }
};

/*--------------------------------------------------------------------------------*\
 * ACHTUNG! PLL's sind nur fuer den Bereich von 200 bis 550 MHz freigegeben!
\*--------------------------------------------------------------------------------*/
struct _valid_clk valid_tdm_clk[] = {
    { startup_tdm: 1, freq: 2048000, pre_div: 2, mult:31},  /*--- 2048 MBit ---*/
    {                 freq: 1228800, pre_div: 4, mult:31},  /*--- 153,6 MHz - out of range !! ---*/
    {                 freq: 3072000, pre_div: 1, mult:31},  /*--- 384 MHz ---*/
    {                 freq: 192000,  pre_div: 0, mult:0 },  /*--- bypass ---*/
    { 0, 0, 0, 0, 0, 0, 0 }
};


#if defined(UR8_CLK_DEBUG)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static const char *ur8_name_clock_id(enum _avm_clock_id clock_id) {
    return
        clock_id == avm_clock_id_cpu        ? "id_cpu"        :
        clock_id == avm_clock_id_system     ? "id_system"     :
        clock_id == avm_clock_id_usb        ? "id_usb"        :
        clock_id == avm_clock_id_dsp        ? "id_dsp"        :
        clock_id == avm_clock_id_vbus       ? "id_vbus"       :
        clock_id == avm_clock_id_peripheral ? "id_peripheral" :
        clock_id == avm_clock_id_c55x       ? "id_c55x"       :
        clock_id == avm_clock_id_ephy       ? "id_ephy"       :
        clock_id == avm_clock_id_pci        ? "id_pci"        :
        clock_id == avm_clock_id_tdm        ? "id_tdm"        : "unknown";
}
#endif/*--- #if defined(UR8_CLK_DEBUG) ---*/

/*--------------------------------------------------------------------------------*\
 * flag = 0: liefere NormFrequenz sonst 'K', 'M', ' '
\*--------------------------------------------------------------------------------*/
static inline unsigned int ur8_norm_clock(unsigned int clk, int flag) {
    if(flag == 0) {
        return ((clk / MHZ) * MHZ) == clk ? clk / MHZ : ((clk / 1000) * 1000) == clk ? clk / 1000 : clk;
    } 
    return ((clk / MHZ) * MHZ) == clk ? 'M': ((clk / 1000) * 1000) == clk ? 'K' : ' ';
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ur8_clk_get_pll_factor(struct _ur8_clock_pll *pll, unsigned int count) {
    unsigned long flags;
    DBG_TRC("ur8_clk_get_pll_factor: %p pll->Bits.pll_disable=%d nobypass=%d hfenable=%d prediv=%d mul=%d\n", 
                                                                                    pll,
                                                                                    pll->PLL_ctrl.Bits.pll_disable, 
                                                                                    pll->PLL_ctrl.Bits.nobypass, 
                                                                                    pll->PLL_ctrl.Bits.half_enable,
                                                                                    pll->PLL_ctrl.Bits.prediv, 
                                                                                    pll->PLL_ctrl.Bits.mult);
    spin_lock_irqsave(&ur8_clock_spinlock, flags);
    if(pll->PLL_ctrl.Bits.pll_disable) {
        /*--- PLL disabled. The output may high or low but now switching ---*/
        spin_unlock_irqrestore(&ur8_clock_spinlock, flags);
        return 0;
    }
    if(pll->PLL_ctrl.Bits.pwr_dwn) {
        /*--- PLL in powerdown ??  bypassed clock ??? or what ---*/
        spin_unlock_irqrestore(&ur8_clock_spinlock, flags);
        return count;
    }
    if(pll->PLL_ctrl.Bits.nobypass == 0) {
        /*--- PLL bypassed ---*/
        spin_unlock_irqrestore(&ur8_clock_spinlock, flags);
        return count;
    }
    count = (count / (pll->PLL_ctrl.Bits.prediv + 1)) * (pll->PLL_ctrl.Bits.mult + 1);
    if(pll->PLL_ctrl.Bits.half_enable) {
        /*--- auch bei Bypass ?? ---*/
        count /= 2;
    }
    spin_unlock_irqrestore(&ur8_clock_spinlock, flags);
    DBG_TRC("ur8_clk_get_pll_factor: %d\n", count);
    return count;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_calc_clock_cycles(int value, int clock) {

    int count = (value*10)/clock;

    if (!value)
        return 0;
    count *= 10;   
    count += 90;        /*--- +90% ---*/
    count /= 100;       /*--- und abrunden ---*/
    if (count > 0)      /*--- Ueberlauf verhindern! sonst kommt nur noch Schrott heraus ---*/
        count -= 1;
    return count;

}
/*------------------------------------------------------------------------------------------*\
 * clock [Hz] 
 * um bei der Rechnung mit Integer Rundungsfehler zu vermeiden wird alles mit 10 multipliziert
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_calc_flash_timing(int clock, emif_async_chip_timing *chip) {
    
    int ta = 0, ew = 0, ss = 0;
    int t_clk = 10000 / (clock / MHZ);      /*--- clk*10 [ns] ---*/ 
    int w_setup = max(chip->t_wph - chip->t_ch, chip->t_as);
    int w_strobe = chip->t_wp;
    int w_hold = max(chip->t_ah - chip->t_wp, chip->t_ch);
    int r_setup = max(chip->t_acc - chip->t_oe, chip->t_ce - chip->t_oe);
    int r_strobe = chip->t_oe + (t_clk * 2);
    int r_hold = chip->t_rc - r_strobe - r_setup;

    if (w_hold < 0)
        w_hold = 0;
    if (r_hold < 0)
        r_hold = 0;

    return     ((ss<<31) | 
                (ew<<30) | 
                (ur8_calc_clock_cycles(w_setup, t_clk)<<26) | 
                (ur8_calc_clock_cycles(w_strobe, t_clk)<<20) | 
                (ur8_calc_clock_cycles(w_hold, t_clk)<<17) | 
                (ur8_calc_clock_cycles(r_setup, t_clk)<<13) | 
                (ur8_calc_clock_cycles(r_strobe, t_clk)<<7) | 
                (ur8_calc_clock_cycles(r_hold, t_clk)<<4) | 
                (ta<<2) | 
                (chip->asize));

}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_calc_sdram_timing(int clock, emif_sync_chip_timing *chip) {

    int t_clk = 10000 / (clock / MHZ);      /*--- clk*10 [ns] ---*/ 

    return ((ur8_calc_clock_cycles(chip->t_rp, t_clk) << 24) | 
            (ur8_calc_clock_cycles(chip->t_rcd, t_clk) << 20) |
            (ur8_calc_clock_cycles(2 * t_clk, t_clk) << 16) |
            (ur8_calc_clock_cycles(chip->t_ras, t_clk) << 12) |
            (ur8_calc_clock_cycles(chip->t_rc, t_clk) << 8) |
            (ur8_calc_clock_cycles(chip->t_rrd, t_clk) << 4) |
            (ur8_calc_clock_cycles(chip->t_rfc, t_clk) << 0));

}
/*------------------------------------------------------------------------------------------*\
 * siehe EMIF-Module Spezification
\*------------------------------------------------------------------------------------------*/
static inline unsigned int ur8_calc_sdram_refresh(int clock) {

    int t_clk = clock / MHZ;      /*--- [MHz] ---*/ 

    return (78 * t_clk)/10;
}
/*------------------------------------------------------------------------------------------*\
 * 0 - CL2 Mode
 * 1 - CL3 Mode - siehe EMIF-Module Spezification
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_allow_CL2_Mode(unsigned int clock, emif_sync_chip_timing *chip) {

    switch (chip->timing) {
        case 60:
            if (clock > (133*MHZ))
                return 1;
            break;
        case 70:
        case 75:
            if (clock > (100*MHZ))
                return 1;
            break;
        default:
            return 1;
    }
    return 0;       /*--- CL2 erlaubt ---*/
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ur8_clk_set_pll(struct _ur8_clock_pll *Pll, 
                             unsigned int enable, 
                             unsigned int mult, 
                             unsigned int pre_div) {
    unsigned long flags;
    unsigned int always_set = 0;

    DBG_TRC("ur8_clk_set_pll: %p enable=%d mult=%d pre_div=%d\n", Pll, enable, mult, pre_div);
    spin_lock_irqsave(&ur8_clock_spinlock, flags);
    if(enable == 0) {
        Pll->PLL_ctrl.Bits.pll_disable = 1;
        spin_unlock_irqrestore(&ur8_clock_spinlock, flags);
        return;
    }

    always_set = ur8_clk_set_emif(Pll, mult, pre_div, always_set);

    if((mult == 0) && (pre_div == 0)) {
        Pll->PLL_ctrl.Bits.unreset     = 0;
        Pll->PLL_ctrl.Bits.ext_bypass  = 1;
        Pll->PLL_ctrl.Bits.nobypass    = 0;
        Pll->PLL_ctrl.Bits.unreset     = 1;
        spin_unlock_irqrestore(&ur8_clock_spinlock, flags);
        return;
    }
    
    Pll->PLL_ctrl.Bits.pll_disable = 0;
    Pll->PLL_ctrl.Bits.ext_bypass  = 1;
    Pll->PLL_ctrl.Bits.unreset     = 0;
    Pll->PLL_ctrl.Bits.half_enable = 0;
    Pll->PLL_ctrl.Bits.pll_disable = 0;
    Pll->PLL_ctrl.Bits.nobypass    = 0;
    udelay(1);
    Pll->PLL_ctrl.Bits.mult        = mult;
    Pll->PLL_ctrl.Bits.prediv      = pre_div;
    Pll->PLL_ctrl.Bits.nobypass    = 1;
    udelay(1);
    Pll->PLL_ctrl.Bits.unreset     = 1;
    udelay(5);
    Pll->PLL_ctrl.Bits.ext_bypass  = 0;

    ur8_clk_set_emif(Pll, mult, pre_div, always_set);

    spin_unlock_irqrestore(&ur8_clock_spinlock, flags);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
volatile unsigned int change_by_proc_device;

#if defined(CONFIG_UR8_CLOCK_SWITCH)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_change_clock_read(char* buf, char **start, off_t offset, int count, int *eof, void *data) {
    unsigned int mips_clk   = ur8_get_clock(avm_clock_id_cpu);
    unsigned int system_clk = ur8_get_clock(avm_clock_id_system);
    unsigned int dsp_clk    = ur8_get_clock(avm_clock_id_dsp);
    unsigned int c55x_clk   = ur8_get_clock(avm_clock_id_c55x);
    unsigned int tdm_clk    = ur8_get_clock(avm_clock_id_tdm);
    unsigned int vbus_clk   = ur8_get_clock(avm_clock_id_vbus);
    unsigned int system_pll = ur8_get_clock(avm_clock_id_systempll);
    unsigned int pci_clk    = ur8_get_clock(avm_clock_id_pci);
    unsigned int len;
    

    sprintf(buf, "UR8 Clock: CPU: %u %cHz System: %u %cHz SystemPLL: %d DSP: %u %cHz C55x: %u %cHz"
                 " TDM: %u %cHz VBUS: %u %cHz PCI: %u %cHz\n",
                    ur8_norm_clock(mips_clk, 0),   ur8_norm_clock(mips_clk, 1),
                    ur8_norm_clock(system_clk, 0), ur8_norm_clock(system_clk, 1),
                    system_pll,
                    ur8_norm_clock(dsp_clk, 0),    ur8_norm_clock(dsp_clk, 1),
                    ur8_norm_clock(c55x_clk, 0),   ur8_norm_clock(c55x_clk, 1),
                    ur8_norm_clock(tdm_clk, 0),    ur8_norm_clock(tdm_clk, 1),
                    ur8_norm_clock(vbus_clk, 0),   ur8_norm_clock(vbus_clk, 1),
                    ur8_norm_clock(pci_clk, 0),    ur8_norm_clock(pci_clk, 1));
    len = strlen(buf);
    return len;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void ur8_printvalid_clock(enum _avm_clock_id clock_id){
    switch(clock_id) {
        case avm_clock_id_cpu:
            ur8_set_mips_clock(0, 0); /*--- clk == 0: printe gueltige Werte ---*/
            break;
        case avm_clock_id_system:
            ur8_set_system_clock(0, 0); /*--- clk == 0: printe gueltige Werte ---*/
            break;
        case avm_clock_id_dsp:
            ur8_set_dsp_clock(0); /*--- clk == 0: printe gueltige Werte ---*/
            break;
        case avm_clock_id_c55x:
            ur8_set_c55x_clock(0); /*--- clk == 0: printe gueltige Werte ---*/
            break;
        case avm_clock_id_vbus:
            ur8_set_vlynq_clock(0); /*--- clk == 0: printe gueltige Werte ---*/
            break;
        case avm_clock_id_tdm:
            ur8_set_tdm_clock(0); /*--- clk == 0: printe gueltige Werte ---*/
            break;
        default:
            break;
    }
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _clock_eval {
    const char *Name;
    unsigned int NameSize;
    enum _avm_clock_id clock_id;
} clock_eval[] = {
    { "CPU:",       sizeof("CPU:") - 1,         avm_clock_id_cpu},
    { "System:",    sizeof("System:") - 1,      avm_clock_id_system},
    { "SystemPLL:", sizeof("SystemPLL:") - 1,   avm_clock_id_systempll},
    { "DSP:",       sizeof("DSP:") - 1,         avm_clock_id_dsp},
    { "C55x:",      sizeof("C55x:") - 1,        avm_clock_id_c55x},
    { "TDM:",       sizeof("TDM:") - 1,         avm_clock_id_tdm},
    { "VBUS:",      sizeof("VBUS:") - 1,        avm_clock_id_vbus},
    {  NULL,        0,                          avm_clock_id_non}
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_change_clock_write(struct file *file, const char __user *buff, unsigned long count, void *data) {
    struct _clock_eval *ce = clock_eval;
    unsigned int clk = 1;
    char *p;

    if(strstr(buff, "TEST")) {
        test_bus_speed();
    }

    if(strstr(buff, "kHz")) clk = 1000;
    if(strstr(buff, "KHz")) clk = 1000;
    if(strstr(buff, "mHz")) clk = 1000 * 1000;
    if(strstr(buff, "MHz")) clk = 1000 * 1000;

    if(clk) {
        DBG_TRC("[ur8_change_clock_write] faktor is %u\n", clk);
    }
    while(ce->Name) {
        if((p = strstr(buff, ce->Name))) {
            p += ce->NameSize;
            while(*p && (*p == ' ' || *p == '\t')) p++; 
            clk *= simple_strtol(p, NULL, 0);
            change_by_proc_device = 1;
            if(ur8_set_clock(ce->clock_id, clk)) {
                change_by_proc_device = 0;
                PRINTK(KERN_ERR" failed - valid values are: ");
                ur8_printvalid_clock(ce->clock_id);
                PRINTK("\n");
            } else {
                change_by_proc_device = 0;
                PRINTK(KERN_ERR" success\n");
            }
            break;
        }
        ce++;
    } 
    return count;
}
#endif /*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/

/*------------------------------------------------------------------------------------------*\
 * nur so kann die PLL3 eingeschaltet werden, ansonsten bleibt sie im Bypass!
 * PLL1 & PLL2 werden schon im Urlader eingeschaltet
 * set_pll schaltet die PLL nicht ein
\*------------------------------------------------------------------------------------------*/
int __init ur8_clk_enable_pll(struct _ur8_clock_pll *Pll) {

    Pll->PLL_ctrl.Bits.nobypass    = 1;
    mdelay(2);
    Pll->PLL_ctrl.Bits.unreset     = 1;
    mdelay(200);
    Pll->PLL_ctrl.Bits.ext_bypass  = 0;
    return 0;

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int __init ur8_clk_init(void) {
    
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    struct _valid_clk *pval2    = valid_tdm_clk;
    struct _hw_boot *BOOT  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    int mips_async = BOOT->hw_boot_config.Bits.mips_async;
    unsigned long flags;

    spin_lock_init(&ur8_clock_spinlock);

    spin_lock_irqsave(&ur8_clock_spinlock, flags);
    ur8_set_mips_clock(0, 1);
    if(mips_async) {
        ur8_set_system_clock(0, 1);
    }

    DBG_TRC("ur8_clk_init()\n");
#if defined(CONFIG_UR8_CLOCK_SWITCH)
    spin_lock_init(&ur8_clock_switch_spinlock);
    ur8_system_clock_notify_first = NULL;
#endif /*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/
    ur8_clk_enable_pll(&CLOCK->PLL3);
    spin_unlock_irqrestore(&ur8_clock_spinlock, flags);

    while(pval2->freq) {
        if(pval2->startup_tdm){
            ur8_set_clock(avm_clock_id_tdm, pval2->freq);
            break;
        } 
        pval2++;
    }




    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_UR8_CLOCK_SWITCH)
static int __init ur8_clk_switch_init(void) {
    ur8_change_clock_entry = create_proc_entry("clocks", 06666, NULL);
    if(ur8_change_clock_entry) {
        ur8_change_clock_entry->read_proc = ur8_change_clock_read;
        ur8_change_clock_entry->write_proc = ur8_change_clock_write;
        ur8_change_clock_entry->data = NULL;
    }
    return 0;
};

late_initcall(ur8_clk_switch_init);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ur8_set_clock_notify(enum _avm_clock_id clock_id, unsigned int (*notify)(enum _avm_clock_id, unsigned int new_clk), int set_function) {
    struct _ur8_clk_notify_system_clock_change *aktuell = ur8_system_clock_notify_first;
    struct _ur8_clk_notify_system_clock_change *N;
    unsigned long flags;

#if defined(UR8_CLK_DEBUG)
    DBG_TRC("[ur8_set_clock_notify] %s notify %s 0x%p ", ur8_name_clock_id(clock_id), set_function ? "enable" : "disable",
             notify);
    __print_symbol("0x%x\n", (unsigned long) notify);
#endif /*--- #if defined(UR8_CLK_DEBUG) ---*/
    if(set_function) {
        N = alloc_clk_listentry();
        if(N == NULL)
            return -1;

        spin_lock_irqsave(&ur8_clock_switch_spinlock, flags);
        N->next     = NULL;
        N->clock_id = clock_id;
        N->notify   = notify;
        /*----------------------------------------------------------------------------------*\
         * nicht der erste, es gibt schon einen
        \*----------------------------------------------------------------------------------*/
        if(ur8_system_clock_notify_first) {
            while(aktuell->next) {
                aktuell = aktuell->next;
            }
            aktuell->next = N;
        } else {
            ur8_system_clock_notify_first = N;
        }
        spin_unlock_irqrestore(&ur8_clock_switch_spinlock, flags);
        return 0;
    } else {
        /*----------------------------------------------------------------------------------*\
         * der erste und einzige
        \*----------------------------------------------------------------------------------*/
        spin_lock_irqsave(&ur8_clock_switch_spinlock, flags);
        if(ur8_system_clock_notify_first == NULL) {
            spin_unlock_irqrestore(&ur8_clock_switch_spinlock, flags);
            return -1;
        }
        if((ur8_system_clock_notify_first->notify ==  notify) &&
           (ur8_system_clock_notify_first->clock_id == clock_id)) {
            N = ur8_system_clock_notify_first;
            ur8_system_clock_notify_first = ur8_system_clock_notify_first->next;
            spin_unlock_irqrestore(&ur8_clock_switch_spinlock, flags);
            free_clk_listentry(N);
            return 0;
        }

        /*----------------------------------------------------------------------------------*\
         * der erste ist es nicht, gibt es noch weitere
        \*----------------------------------------------------------------------------------*/
        while(aktuell->next) {
            if(aktuell->next->notify ==  notify) {
                N = aktuell->next;
                aktuell->next = aktuell->next->next;
                spin_unlock_irqrestore(&ur8_clock_switch_spinlock, flags);
                free_clk_listentry(N);
                return 0;
            }
            aktuell = aktuell->next;
        }
        spin_unlock_irqrestore(&ur8_clock_switch_spinlock, flags);
    }
    return -1;
    return 0;
}
EXPORT_SYMBOL(ur8_set_clock_notify);
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_call_clock_notifier(enum _avm_clock_id clock_id) {
    struct _ur8_clk_notify_system_clock_change *aktuell = ur8_system_clock_notify_first;
    unsigned int clk = ur8_get_clock(clock_id);
    unsigned int ret = 0;
    while(aktuell) {
        if((aktuell->notify) && (aktuell->clock_id == clock_id)) {
#if defined(UR8_CLK_DEBUG)
            DBG_TRC("[ur8_call_clock_notifier] call 0x%p ", aktuell->notify);
            __print_symbol("0x%x\n", (unsigned long) aktuell->notify);
#endif /*--- #if defined(UR8_CLK_DEBUG) ---*/
            ret |= (aktuell->notify)(clock_id, clk);
        }
        aktuell = aktuell->next;
    }
    return ret;
}
#endif /*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/
/*--------------------------------------------------------------------------------*\
 * triggert alle notifier entsprechend clock_id-Mask
\*--------------------------------------------------------------------------------*/
static unsigned int ur8_trigger_notifier(enum _avm_clock_id clock_idmask){
    int ret = 0;
    if(clock_idmask & avm_clock_id_cpu) {
#ifdef CONFIG_AVM_POWERMETER
        PowerManagmentRessourceInfo(powerdevice_cpuclock, FREQUENZ_TO_PERCENT(ur8_get_clock(avm_clock_id_cpu), 360000000));
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/
#if defined(CONFIG_UR8_CLOCK_SWITCH)
        ret |= ur8_call_clock_notifier(avm_clock_id_cpu);
#endif/*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/
    }
    if(clock_idmask & avm_clock_id_system) {
#ifdef CONFIG_AVM_POWERMETER
        PowerManagmentRessourceInfo(powerdevice_systemclock, FREQUENZ_TO_PERCENT(ur8_get_clock(avm_clock_id_system), 120000000));
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/
#if defined(CONFIG_UR8_CLOCK_SWITCH)
        ret |= ur8_call_clock_notifier(avm_clock_id_system);
#endif/*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/
    }
    if(clock_idmask & avm_clock_id_dsp) {
#ifdef CONFIG_AVM_POWERMETER
        PowerManagmentRessourceInfo(powerdevice_dspclock, FREQUENZ_TO_PERCENT(ur8_get_clock(avm_clock_id_dsp), 360000000));
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/
#if defined(CONFIG_UR8_CLOCK_SWITCH)
        ret |= ur8_call_clock_notifier(avm_clock_id_dsp);
#endif/*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/
    }
    if(clock_idmask & avm_clock_id_usb) {
    }
    if(clock_idmask & avm_clock_id_vbus) {
    }
    if(clock_idmask & avm_clock_id_peripheral) {
#if defined(CONFIG_UR8_CLOCK_SWITCH)
        ret |= ur8_call_clock_notifier(avm_clock_id_peripheral);
#endif /*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/
    }
    if(clock_idmask & avm_clock_id_c55x) {
#if defined(CONFIG_UR8_CLOCK_SWITCH)
        ret |= ur8_call_clock_notifier(avm_clock_id_c55x);
#endif/*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/
    }
    if(clock_idmask & avm_clock_id_ephy) {
    }
    if(clock_idmask & avm_clock_id_pci) {
    }
    if(clock_idmask & avm_clock_id_tdm) {
    }
    return ret;
}
/*--------------------------------------------------------------------------------*\
 * muss cached/ in Registern laufen!!!!
 * EMIF schreiben - in Assembler überprüft
\*--------------------------------------------------------------------------------*/
static void __attribute__((aligned (32))) __ur8_clk_set_emif(unsigned int new_clk, unsigned int sdram_timing_index) {
    static int change = 1;
    struct EMIF_register_memory_map *EMIF = (struct EMIF_register_memory_map *)&(*(volatile unsigned int *)(UR8_EMIF_BASE));
    union EMIF_SDRAMBankCR BANKCR_tmp;
    register unsigned int cl;
    /*--------------------------------------------------------------------------------*\
     * nur mit "static-CR" sieht der Code schoen speicherzugriffsfrei (d.h. nur Zugriff auf cached CR) aus:
    \*--------------------------------------------------------------------------------*/
    static struct _cachedreg {
        unsigned int RefreshCr;
        unsigned int BANKCR_tmp;
        unsigned int SDRAMTiming_Reg;
        unsigned int AsyncBankCR;
    } __attribute__((aligned(32))) CR;
    BANKCR_tmp.i = EMIF->SDRAMBankCR.i;
    cl = ur8_allow_CL2_Mode(new_clk, &sdram_timing[sdram_timing_index]);
    if(BANKCR_tmp.Bits.cl != cl) {
        BANKCR_tmp.Bits.cl = cl;
        CR.BANKCR_tmp      = BANKCR_tmp.i;
        /*--------------------------------------------------------------------------------*\
            CAS Latency
            ...
            A write to this field will cause the EMIF to start the SDRAM initialization sequence!!!

            -> scheint kritisch zu sein, also nur schreiben wenn UNBEDINGT notwendig!
        \*--------------------------------------------------------------------------------*/
        printk("[%s]CL change to  %d critical, maybe hang emif-interface\n", __func__, cl); 
        cl = 1;
    } else {
        cl = 0;
    }
    CR.RefreshCr       = ur8_calc_sdram_refresh(new_clk);
    CR.SDRAMTiming_Reg = ur8_calc_sdram_timing(new_clk, &sdram_timing[sdram_timing_index]);
    CR.AsyncBankCR     = ur8_calc_flash_timing(new_clk, &flash_timing[0]);
    /*--- printk("prefetch_range(%p, %d)          \n",__ur8_clk_set_emif, 1024); ---*/
    /*--- keine Chance die Laenge der Fkt. zu ermitteln? Per Offset zur nachfolgenden Funktion: Compiler optimiert auch Fkt.position ---*/
    prefetch_range(__ur8_clk_set_emif, 2048);
    EMIF->SDRAMRefreshCR   = CR.RefreshCr;
    if((change && (new_clk == 120000000)) || change_by_proc_device) {
        /*--- Workarround fuer avm_idle: Werte bei 120 MHz lassen - nur falls ueber /proc/clocks geaendert ---*/
        change = change_by_proc_device;
        if(cl) {
            EMIF->SDRAMBankCR.i    = CR.BANKCR_tmp;
            udelay(16);
        }
        EMIF->SDRAMTimingReg.i = CR.SDRAMTiming_Reg;
        udelay(16);
        EMIF->AsyncBankCR[0].i = CR.AsyncBankCR;
        udelay(16);
        printk("[ur8_clk_set_emif]clk=%d SDRAM Refresh 0x%x Timing 0x%x Bank 0x%x Async 0x%x\n", new_clk, EMIF->SDRAMRefreshCR, EMIF->SDRAMTimingReg.i,EMIF->SDRAMBankCR.i, EMIF->AsyncBankCR[0].i); 
    }
    return;
}
/*------------------------------------------------------------------------------------------*\
 * ur8_clk_set_emif wird 2x aufgerufen
 * 1. vor dem Setzen einer PLL mit set = 0;
 *      - der R�ckgabewert zeigt ob die EMIF-Werte gesetzt wurden
 * 2. nach dem Setzen der PLL - mit dem R�ckgabewert des 1. Aufrufs
 * so ist sichergestellt, dass bei h�here Systemfrequenz die EMIF-Werte vor der PLL gesetzt
 * werden und bei kleinerer Systemfrequenz danach
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_clk_set_emif(struct _ur8_clock_pll *Pll, unsigned int mult, unsigned int pre_div, unsigned int set){
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    struct _hw_boot *BOOT  = (struct _hw_boot *)&(*(volatile unsigned int *)(UR8_DEVICE_CONFIG_BASE));
    struct _ur8_clock_pll *pll;
    int bootcfg    = BOOT->hw_boot_config.Bits.clk_ratio;    
    int new_clk, clk, sdram_timing_index = 0;     /*--- default sind -6 DRAMs ---*/
    char *HWRevision;

    new_clk = ((default_ur8_lan_xtal / (pre_div + 1)) * (mult + 1)) / (bootcfg + 1);

    if (!new_clk)
        return 1;
    
    DBG_TRC("[ur8_clk_set_emif] Systemclock %d new_clk %d\n", ur8_get_clock(avm_clock_id_system), new_clk);

    /*--- wird die PLL vom Systemtakt verstellt? ---*/
    if(BOOT->hw_boot_config.Bits.mips_async) {
        /*--- MIPS asynchron: PLL2 oder PLL3  ---*/
        pll = CLOCK->CLOCK_CFG.Bits.sys_asyc_clk_sel ? &CLOCK->PLL3 : &CLOCK->PLL2;
        DBG_TRC("[ur8_clk_set_emif] use SYSTEM-Clock PLL%d\n", CLOCK->CLOCK_CFG.Bits.sys_asyc_clk_sel ? 3 : 2);
    } else {
        /*--- MIPS synchron: nur PLL1 moeglich ---*/
        pll = &CLOCK->PLL1;
        DBG_TRC("[ur8_clk_set_emif] use SYSTEM-Clock PLL1\n");
    }

    if (Pll == pll) {       
        /*--- das EMIF-Timing muss angepasst werden ---*/
        clk = ur8_clk_get_pll_factor(pll, default_ur8_lan_xtal) / (bootcfg + 1);

        DBG_TRC("[ur8_clk_set_emif] use clk %d new clk %d %s\n", clk, new_clk, set ? "set":"");

        if ((new_clk > clk) || set) {

            HWRevision = prom_getenv("HWRevision");
            if (HWRevision && strstr(HWRevision, "122"))
                sdram_timing_index = 2;     /*--- hier wurden auch langsamere SDRAMs verbaut ---*/

            __ur8_clk_set_emif(new_clk, sdram_timing_index);
            /*--- PRINTK("[ur8_clk_set_emif] SDRAM Refresh 0x%x Timing 0x%x Bank 0x%x Async 0x%x\n", EMIF->SDRAMRefreshCR, EMIF->SDRAMTimingReg.i,EMIF->SDRAMBankCR.i, EMIF->AsyncBankCR[0].i);  ---*/
        }
    }
    return 1;
}
/*------------------------------------------------------------------------------------------*\
    Returnwert: Mask mit allen Notifieren: 0 -> Fehler!
    clk == 0: printen aller gueltigen Frequenzen
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_set_mips_clock(unsigned int clk, int startup) {
    struct _valid_clk *pval = valid_pll_setting;
    struct _hw_boot *BOOT  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    int mips_async = BOOT->hw_boot_config.Bits.mips_async;

    while(pval->freq) {
        if((startup && mips_async && pval->startup_mips) || (startup && !mips_async && pval->startup_sync)) {
            printk(KERN_ERR "[mips_clock] default %d MHz\n", default_ur8_lan_xtal / (pval->pre_div + 1) * (pval->mult + 1));
            break;
        } else if((clk == 0) && !startup) {
            PRINTK("%u %cHZ ", ur8_norm_clock(pval->freq, 0),  ur8_norm_clock(pval->freq, 1)); 
        } else if((pval->freq == clk)){
            break;
        } 
        pval++;
    }
    if(pval->freq == 0) {
        if(clk) PRINTK(KERN_ERR"ur8_set_mips_clock %d failed\n", clk);
        return 0;
    }

    ur8_clk_set_pll(&CLOCK->PLL1, 1, pval->mult, pval->pre_div);
    if(mips_async) {
        return avm_clock_id_cpu;
    }
    return avm_clock_id_cpu | avm_clock_id_system | avm_clock_id_dsp | avm_clock_id_pci | avm_clock_id_ephy | avm_clock_id_c55x | avm_clock_id_peripheral | avm_clock_id_vbus;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_set_system_pll(unsigned int new_pll) {

    struct _hw_boot         *BOOT = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    int mips_async = BOOT->hw_boot_config.Bits.mips_async;
    unsigned int pll;

    if (mips_async) {
        pll = CLOCK->CLOCK_CFG.Bits.sys_asyc_clk_sel ? 3:2;
        DBG_TRC("[ur8_set_system_pll] pll %d\n", pll);
        if (new_pll == pll) {
            DBG_TRC("[ur8_set_system_pll] PLL %d always set\n", pll);
            return 0;
        }

        DBG_TRC("[ur8_set_system_pll] to pll %d\n", new_pll);
        switch (new_pll) {
            case 2:
                CLOCK->CLOCK_CFG.Bits.sys_asyc_clk_sel = 0;
                break;
            case 3:
                CLOCK->CLOCK_CFG.Bits.sys_asyc_clk_sel = 1;
                break;
        }
        return avm_clock_id_system | avm_clock_id_dsp | avm_clock_id_c55x | avm_clock_id_peripheral;

    } else {
        PRINTK(KERN_ERR "[ur8_set_system_pll] can not change System-PLL\n");
        return 0;
    }

}

/*------------------------------------------------------------------------------------------*\
    Returnwert: Mask mit allen Notifieren: 0 -> Fehler!
    clk == 0: printen aller gueltigen Frequenzen
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_set_system_clock(unsigned int clk, int startup) {
    struct _valid_clk *pval = valid_pll_setting;
    struct _hw_boot         *BOOT = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    int mips_async = BOOT->hw_boot_config.Bits.mips_async;
    int bootcfg    = BOOT->hw_boot_config.Bits.clk_ratio;    
    unsigned int    pll_freq, ret = 0;
    int pll;

    pll_freq = clk * (bootcfg + 1);

    DBG_TRC("[ur8_set_system_clock] clk %d pll_freq %d\n", clk, pll_freq);

    while(pval->freq) {
        if(startup && mips_async && pval->startup_system) {
            printk(KERN_ERR "[system_clock] default %d MHz\n", default_ur8_lan_xtal / (pval->pre_div + 1) * (pval->mult + 1) / (bootcfg + 1));
            break;
        } else if((clk == 0) && !startup) {
            PRINTK("%u %cHZ ", ur8_norm_clock(pval->freq / (bootcfg + 1), 0),  ur8_norm_clock(pval->freq / (bootcfg + 1), 1)); 
        } else if (pval->freq == pll_freq) {
            break;
        }
        pval++;
    }
    if(pval->freq == 0) {
        if(clk)PRINTK(KERN_ERR"ur8_set_system_clock %d failed\n", clk);
        return 0;
    }

    if (mips_async)
        pll = CLOCK->CLOCK_CFG.Bits.sys_asyc_clk_sel ? 3:2;
    else
        pll = 1;

    /*--- TODO Ausgabe entfernen ---*/
    PRINTK(KERN_INFO"ur8_set_system_clock: %ssync mips %d use PLL %d\n", mips_async ? "a" : "", clk, pll);
    switch(pll) {
        case 1: /*--- synchron ---*/
            ret = ur8_set_mips_clock(clk * (bootcfg + 1), 0);
            if (ret == 0) {
                PRINTK(KERN_ERR"ur8_set_system_clock: error to set system-clock: %d\n", clk);
                return 0;
            }
            break;
        case 2:
            ur8_clk_set_pll(&CLOCK->PLL2, 1, pval->mult, pval->pre_div);
            break;
        case 3:
            ur8_clk_set_pll(&CLOCK->PLL3, 1, pval->mult, pval->pre_div);
            break;
    }

    /*--- im async-mode wird die CPU-Clock nicht ge�ndert ---*/
    return ret | avm_clock_id_system | avm_clock_id_dsp | avm_clock_id_pci | avm_clock_id_ephy | avm_clock_id_c55x | avm_clock_id_peripheral | avm_clock_id_vbus;
}

/*------------------------------------------------------------------------------------------*\
    Returnwert: Mask mit allen Notifieren: 0 -> Fehler!
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_set_usb_clock(unsigned int clk) {
    if(clk != default_ur8_lan_xtal) {
        return 0;
    }
    return avm_clock_id_usb;
}

/*------------------------------------------------------------------------------------------*\
    Returnwert: Mask mit allen Notifieren: 0 -> Fehler!
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_set_pci_clock(unsigned int clk) {
	
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    struct _hw_boot *BOOT  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    int mips_async = BOOT->hw_boot_config.Bits.mips_async;
    int divisor = 1;

	switch(clk) {
		case 33000000:
		case 33333333:
            if(mips_async) {  /*--- PLL 1 verwenden ---*/
                int cpu_clock = ur8_get_mips_clock();
                switch(cpu_clock) {
                    case 360000000:  
                        divisor = 11 - 1;  /*--- ergibt 32,7 MHz ---*/
                        break;
                    case 480000000:
                        divisor = 14 - 1;  /*--- ergibt 34,3 MHz ---*/
                        break;
                    default:
                        goto use_pll2;
                }
                CLOCK->CLOCK_CFG.Bits.pci_mux_sel = 1; // 360MHz PLL1
                goto program_divisor;
            }
			divisor = 8;
			break;
		case 30000000:
			divisor = 9;
			break;
		default:
			return 0;
	}
use_pll2:
    CLOCK->CLOCK_CFG.Bits.pci_mux_sel = 0; // 300MHz PLL2

    ur8_clk_set_pll(&CLOCK->PLL2, 1, 24, 1);

    printk("ur8_clk_get_pll_factor: %u\n", ur8_clk_get_pll_factor(&CLOCK->PLL2, default_ur8_lan_xtal));

program_divisor:
    CLOCK->CLOCK_CFG.Bits.freeze_div_by = 1; 
    CLOCK->CLOCK_CFG.Bits.pci_clk_dir = 0; // Output from TNETD7531
    CLOCK->CLOCK_CFG.Bits.pci_div_sel = divisor;
    CLOCK->CLOCK_CFG.Bits.freeze_div_by = 0; 
    
    printk("ur8_get_pci_clock %u\n", ur8_get_pci_clock());

    return ur8_get_pci_clock();
}

/*------------------------------------------------------------------------------------------*\
    Returnwert: Mask mit allen notifieren: 0 -> Fehler!
    clk == 0: printen aller gueltigen Frequenzen
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_set_dsp_clock(unsigned int clk) {
    struct _valid_clk *pval = valid_pll_setting;
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    struct _hw_boot *BOOT  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    int mips_async = BOOT->hw_boot_config.Bits.mips_async;
    
    while(pval->freq) {
        if(clk == 0) {
            PRINTK("%u %cHZ ", ur8_norm_clock(pval->freq, 0),  ur8_norm_clock(pval->freq, 1)); 
        } else if(pval->freq == clk){
            break;
        }
        pval++;
    }

    if(pval->freq == 0) {
        if(clk) PRINTK(KERN_ERR "ur8_set_dsp_clock %d failed\n", clk);
        return 0;
    }

    DBG_TRC(KERN_INFO"[ur8_set_dsp_clock]: %csync mips %d\n", mips_async ? 'a' : 0, clk);
    if (mips_async) {       /*--- Async - Mode ---*/
        DBG_TRC(KERN_INFO"[ur8_set_dsp_clock]: use PLL%d\n", CLOCK->CLOCK_CFG.Bits.sys_asyc_clk_sel ? 3:2);
        if (CLOCK->CLOCK_CFG.Bits.sys_asyc_clk_sel) {
            ur8_clk_set_pll(&CLOCK->PLL3, 1, pval->mult, pval->pre_div);
            return avm_clock_id_system | avm_clock_id_dsp | avm_clock_id_pci | avm_clock_id_ephy | avm_clock_id_c55x | avm_clock_id_peripheral | avm_clock_id_vbus;
        } else {
            ur8_clk_set_pll(&CLOCK->PLL2, 1, pval->mult, pval->pre_div);
            return avm_clock_id_system | avm_clock_id_dsp | avm_clock_id_c55x | avm_clock_id_peripheral | avm_clock_id_tdm;
        }
    } else {                /*--- Sync Mode - set Mips-Clock ---*/
        if(ur8_set_mips_clock(clk, 0) == 0) {  /*--- wird von PLL1 abgeleitet: MIPS-Clock aendern ---*/
            PRINTK(KERN_ERR"ur8_set_dsp_clock: error to set dsp-clock: %d\n", clk);
            return 0;
        }
        return avm_clock_id_cpu | avm_clock_id_system | avm_clock_id_dsp | avm_clock_id_pci | avm_clock_id_ephy | avm_clock_id_c55x | avm_clock_id_peripheral | avm_clock_id_vbus;
    }
}

/*--------------------------------------------------------------------------------*\
    Returnwert: Mask mit allen notifieren: 0 -> Fehler!
    clk == 0: printen aller gueltigen Frequenzen
\*--------------------------------------------------------------------------------*/
static unsigned int ur8_set_tdm_clock(unsigned int clk) {
    struct _valid_clk *pval = valid_tdm_clk;
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));

    while(pval->freq) {
        if(clk == 0) {
            PRINTK("%u %cHZ ", ur8_norm_clock(pval->freq, 0),  ur8_norm_clock(pval->freq, 1)); 
        } else if((pval->freq == clk)){
            break;
        }
        pval++;
    }
    if(pval->freq == 0) {
        if(clk) PRINTK(KERN_ERR"ur8_set_tdm_clock %d failed\n", clk);
        return 0;
    }
    /*--- derived from PLL3 divided by 125 ---*/
    ur8_clk_set_pll(&CLOCK->PLL3, 1, pval->mult, pval->pre_div);
    return avm_clock_id_tdm;
}

/*--------------------------------------------------------------------------------*\
 * aendert nur den Divider im Clk-Config !
    clk == 0: printen aller gueltigen Frequenzen
\*--------------------------------------------------------------------------------*/
static unsigned int ur8_set_c55x_clock(unsigned int clk) {
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    struct _hw_boot *BOOT  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    struct _ur8_clock_pll *pll;
    unsigned int refclk, c55ss_div_sel;

    if(BOOT->hw_boot_config.Bits.mips_async) {
        /*--- MIPS asynchron: PLL2 oder PLL3  ---*/
        pll = CLOCK->CLOCK_CFG.Bits.sys_asyc_clk_sel ? &CLOCK->PLL3 : &CLOCK->PLL2;
    } else {
        /*--- MIPS synchron: nur PLL1 moeglich ---*/
        pll = &CLOCK->PLL1;
    }
    refclk = ur8_clk_get_pll_factor(pll, default_ur8_lan_xtal);

    for(c55ss_div_sel = 1; c55ss_div_sel < 8; c55ss_div_sel++) { 
        if(clk == 0) {
            PRINTK("%u %cHZ ", ur8_norm_clock(refclk / c55ss_div_sel, 0),  ur8_norm_clock(refclk / c55ss_div_sel, 1)); 
        } else if(refclk / c55ss_div_sel == clk) {
            CLOCK->CLOCK_CFG.Bits.c55ss_div_sel= c55ss_div_sel- 1;
            return avm_clock_id_c55x;
        }
    }
    return 0;
}

/*--------------------------------------------------------------------------------*\
 * aendert nur den Divider im Clk-Config !
   clk == 0: printen aller gueltigen Frequenzen
\*--------------------------------------------------------------------------------*/
static unsigned int ur8_set_vlynq_clock(unsigned int clk) {
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    struct _hw_boot *BOOT  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    unsigned int refclk, vlynq_div_sel;

    if(BOOT->hw_boot_config.Bits.mips_async) {
        refclk  = ur8_clk_get_pll_factor(&CLOCK->PLL2, default_ur8_lan_xtal); /*--- MIPS asynchron: PLL2  ---*/
    } else {
        refclk  = ur8_clk_get_pll_factor(&CLOCK->PLL1, default_ur8_lan_xtal); /*--- MIPS synchron: PLL1 ---*/
    }
    for(vlynq_div_sel = 1; vlynq_div_sel < 8; vlynq_div_sel++) { 
        if(clk == 0) {
            PRINTK("%u %cHZ ", ur8_norm_clock(refclk / vlynq_div_sel, 0),  ur8_norm_clock(refclk / vlynq_div_sel, 1)); 
        } else { 
            if(refclk / vlynq_div_sel == clk) {
                CLOCK->CLOCK_CFG.Bits.vlynq_div_sel = vlynq_div_sel - 1;
                return avm_clock_id_vbus;
            }
        }
    }
    return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int ur8_set_peripheral_clock(unsigned int clk) {

    struct _hw_boot *BOOT  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    
    if (!BOOT->hw_boot_config.Bits.mips_async)      /*--- syncron mode - can not change system_clock ---*/
        return 0;

    return ur8_set_system_clock(clk * 2, 0);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_get_mips_clock(void) {
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
#if defined(UR8_CLK_DEBUG)
    struct _hw_boot *BOOT  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
#endif/*--- #if defined(UR8_CLK_DEBUG) ---*/
    unsigned clk;
    /*--- MIPS-Clock comes only from PLL1 ---*/
    clk = ur8_clk_get_pll_factor(&CLOCK->PLL1, default_ur8_lan_xtal);
    DBG_TRC("ur8_get_mips_clock: %d %s\n", clk, BOOT->hw_boot_config.Bits.mips_async ? "async" : "sync");
    return clk;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_get_dsp_c55_system_clock(enum _avm_clock_id clock_id) {
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    struct _hw_boot *BOOT  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    struct _ur8_clock_pll *pll;
    unsigned int clk;
    if(BOOT->hw_boot_config.Bits.mips_async) {
        /*--- MIPS asynchron: PLL2 oder PLL3  ---*/
        pll = CLOCK->CLOCK_CFG.Bits.sys_asyc_clk_sel ? &CLOCK->PLL3 : &CLOCK->PLL2;
    } else {
        /*--- MIPS synchron: nur PLL1 moeglich ---*/
        pll = &CLOCK->PLL1;
    }
    clk = ur8_clk_get_pll_factor(pll, default_ur8_lan_xtal);
    switch(clock_id) {
        case avm_clock_id_peripheral: 
            clk /= BOOT->hw_boot_config.Bits.clk_ratio + 1;    
            clk /= 2;
            break;
        case avm_clock_id_system: 
            clk /=  BOOT->hw_boot_config.Bits.clk_ratio + 1;    
            break;
        case avm_clock_id_dsp: 
            /*--- no change ---*/
            break;
        case avm_clock_id_c55x:
            clk /=  CLOCK->CLOCK_CFG.Bits.c55ss_div_sel + 1;
            break;
        default: 
            PRINTK(KERN_ERR"ur8_get_dsp_c55_system_clock: unknown id\n");
            break;
    }
    return clk;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_get_system_pll(void) {

    struct _hw_boot         *BOOT = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));

    if (BOOT->hw_boot_config.Bits.mips_async) {
        return CLOCK->CLOCK_CFG.Bits.sys_asyc_clk_sel ? 3:2;
    } else {
        return 1;       
    }

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_get_usb_clock(void) {
    /*--- only xtal MHz possible ---*/
    return default_ur8_lan_xtal;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_get_ephy_clock(void) {
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    unsigned int clk;
    /*--- derived from PLL2 ---*/
    clk  = ur8_clk_get_pll_factor(&CLOCK->PLL2, default_ur8_lan_xtal);
    clk /= CLOCK->CLOCK_CFG.Bits.ephy_div_sel + 1; 
    DBG_TRC("ur8_get_ephy_clock: %d\n", clk);
    return clk;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_get_pci_clock(void) {
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    struct _ur8_clock_pll *pll;
    unsigned int clk;
    
    if(CLOCK->CLOCK_CFG.Bits.pci_clk_dir) {
        /*--- is input to tnetd7531 ---*/
        PRINTK(KERN_ERR"ur8_get_pci_clock: is input\n");
        return 0;
    }
    pll  = CLOCK->CLOCK_CFG.Bits.pci_mux_sel ? &CLOCK->PLL1 : &CLOCK->PLL2;
    clk  = ur8_clk_get_pll_factor(pll, default_ur8_lan_xtal);
    clk /= CLOCK->CLOCK_CFG.Bits.pci_div_sel + 1;
    DBG_TRC("ur8_get_pci_clock: %d\n", clk);
    return clk;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int ur8_get_tdm_clock(void) {
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    /*--- derived from PLL3 divided by 125 ---*/
    unsigned int clk = ur8_clk_get_pll_factor(&CLOCK->PLL3, default_ur8_lan_xtal) / 125;
    DBG_TRC("ur8_get_tdm_clock: %d\n", clk);
    return clk;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int ur8_get_vlynq_clock(void) {
    struct _hw_clock *const CLOCK = (struct _hw_clock *)&(*(volatile unsigned int *)(UR8_CLOCK_BASE));
    struct _hw_boot *BOOT  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
    struct _ur8_clock_pll *pll;
    unsigned int clk;

    if(BOOT->hw_boot_config.Bits.mips_async) {
        pll = &CLOCK->PLL2;     /*--- MIPS asynchron: PLL2  ---*/
    } else {
        pll = &CLOCK->PLL1;     /*--- MIPS synchron: PLL1 ---*/
    }
    clk  = ur8_clk_get_pll_factor(pll, default_ur8_lan_xtal);
    clk /= CLOCK->CLOCK_CFG.Bits.vlynq_div_sel + 1;
    DBG_TRC("ur8_get_vlynq_clock: divsel=%d clock=%d\n", CLOCK->CLOCK_CFG.Bits.vlynq_div_sel, clk);
    return clk;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ur8_get_clock(enum _avm_clock_id clock_id) {
#if defined(UR8_CLK_DEBUG)
    struct _hw_boot *BOOT  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
#endif/*--- #if defined(UR8_CLK_DEBUG) ---*/
    unsigned int clk = 0;
    switch(clock_id) {
        case avm_clock_id_cpu:
            clk =  ur8_get_mips_clock();
            break;
        case avm_clock_id_c55x:
        case avm_clock_id_system:
        case avm_clock_id_dsp:
        case avm_clock_id_peripheral:
            clk = ur8_get_dsp_c55_system_clock(clock_id);
            break;
        case avm_clock_id_systempll:
            clk = ur8_get_system_pll();
            break;
        case avm_clock_id_vbus:
            clk = ur8_get_vlynq_clock();
            break;
        case avm_clock_id_usb:
            clk = ur8_get_usb_clock();
            break;
        case avm_clock_id_ephy:
            clk = ur8_get_ephy_clock();
            break;
        case avm_clock_id_pci:
            clk = ur8_get_pci_clock();
            break;
        case avm_clock_id_tdm:
            clk =  ur8_get_tdm_clock();
            break;
        default: 
            PRINTK(KERN_ERR"ur8_get_clock: unknown id=%d\n", clock_id);
            break;
    }
    DBG_TRC("ur8_get_clock: %s %u %cHz %s\n", ur8_name_clock_id(clock_id), 
                                                      ur8_norm_clock(clk, 0),
                                                      ur8_norm_clock(clk, 1),
                                                      BOOT->hw_boot_config.Bits.mips_async ? "async" : "sync");
    return clk;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern struct clock_event_device per_cpu__mips_clockevent_device;
ssize_t sysfs_override_clocksource(struct sys_device *dev,
					  struct sysdev_attribute *attr,
					  const char *buf, size_t count);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void update_sysclock(unsigned int cpu_clk) {
    char *name = "MIPS\0";
	struct clock_event_device *cd;
	unsigned int cpu = smp_processor_id();
	cd = &per_cpu(mips_clockevent_device, cpu);

    mips_hpt_frequency = cpu_clk / 2;
    clockevent_set_clock(cd, mips_hpt_frequency);

    switch ( cpu_clk ) {
        case 240000000:
            name = "MIPS-240\0";
            break;
        case 300000000:
            name = "MIPS-300\0";
            break;
        case 360000000:
            name = "MIPS-360\0";
            break;
        default:
            ;
    }
    sysfs_override_clocksource(NULL, NULL, name, strlen(name) + 1);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ur8_set_clock(enum _avm_clock_id clock_id, unsigned int clk) {
    int mask = 0, ret = 1;
    if(clk == 0)  {
        return ret;
    }
    switch(clock_id) {
        case avm_clock_id_cpu:
            mask = ur8_set_mips_clock(clk, 0);
            update_sysclock(clk);
            break;
        case avm_clock_id_dsp:
            mask = ur8_set_dsp_clock(clk);
            break;
        case avm_clock_id_system:
            mask = ur8_set_system_clock(clk, 0);
            break;
        case avm_clock_id_systempll:
            mask = ur8_set_system_pll(clk);
            break;
        case avm_clock_id_c55x:
            mask = ur8_set_c55x_clock(clk);
            break;
        case avm_clock_id_usb:
            mask = ur8_set_usb_clock(clk);
            break;
        case avm_clock_id_peripheral:
            mask = ur8_set_peripheral_clock(clk);
            break;
        case avm_clock_id_tdm:
            mask = ur8_set_tdm_clock(clk);
            break;
        case avm_clock_id_vbus:
            mask = ur8_set_vlynq_clock(clk);
            break;
        case avm_clock_id_ephy:
            PRINTK(KERN_ERR"ur8_set_clock: id=%d clk-change todo\n", clock_id);
            break;
        case avm_clock_id_pci:
            mask = ur8_set_pci_clock(clk);
            break;
        default: 
            PRINTK(KERN_ERR"ur8_get_clock: unknown id\n");
            break;
    }
    if(mask) {
        ret = ur8_trigger_notifier(mask);
    }
    DBG_TRC("ur8_set_clock: %s %u %cHz -> %s\n", ur8_name_clock_id(clock_id),
                                                         ur8_norm_clock(clk, 0),
                                                         ur8_norm_clock(clk, 1),
                                                         ret ?  "failed" : "ok");
    return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned char test_array[0x10000];

static void my_memget(unsigned char *ptr, unsigned int count) {
    volatile unsigned int *pi = (volatile unsigned int *)ptr;
    count >>= 2;
    while(count--) *pi++;
}
static void my_memset(unsigned char *ptr, unsigned int count) {
    volatile unsigned int *pi = (volatile unsigned int *)ptr;
    count >>= 2;
    while(count--) *pi++ = 0;
}

void test_bus_speed(void) {
    signed int start, end;
    unsigned char *noncached = (unsigned char *)(((unsigned int)test_array & 0x1FFFFFFF) | 0xA0000000);

    /* code cache fuellen */
    start = read_c0_count();
    memset(test_array, 0xff, sizeof(test_array));
    start = read_c0_count();
    my_memset(test_array, sizeof(test_array));
    end   = read_c0_count();
    printk(KERN_ERR "[speed-test]: write %u Bytes via cache: %u tics\n", sizeof(test_array), end - start);

    /* code cache fuellen */
    start = read_c0_count();
    memset(noncached, 0xff, sizeof(test_array));
    start = read_c0_count();
    my_memset(noncached, sizeof(test_array));
    end   = read_c0_count();
    printk(KERN_ERR "[speed-test]: write %u Bytes without cache : %u tics\n", sizeof(test_array), end - start);

    /* code cache fuellen */
    start = read_c0_count();
    memset(test_array, 0xff, sizeof(test_array));
    start = read_c0_count();
    my_memget(test_array, sizeof(test_array));
    end   = read_c0_count();
    printk(KERN_ERR "[speed-test]: read %u Bytes via cache: %u tics\n", sizeof(test_array), end - start);

    /* code cache fuellen */
    start = read_c0_count();
    memset(test_array, 0xff, sizeof(test_array));
    start = read_c0_count();
    my_memget(noncached, sizeof(test_array));
    end   = read_c0_count();
    printk(KERN_ERR "[speed-test]: read %u Bytes without cache : %u tics\n", sizeof(test_array), end - start);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_UR8_CLOCK_SWITCH)
unsigned int ur8_get_clock_notify(enum _avm_clock_id clock_id, 
                                   unsigned int (*handler)(enum _avm_clock_id, unsigned int new_clk)) {
    ur8_set_clock_notify(clock_id, handler, 0); /*--- deinstall ---*/
    ur8_set_clock_notify(clock_id, handler, 1); /*--- install ---*/
    return ur8_get_clock(clock_id);
}
EXPORT_SYMBOL(ur8_get_clock_notify);
#endif /*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/

EXPORT_SYMBOL(ur8_get_clock);
EXPORT_SYMBOL(ur8_set_clock);

