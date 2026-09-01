/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2006 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
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
#if defined(CONFIG_AVM_WATCHDOG)
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
#include <asm/mips-boards/prom.h>
#else
#include <asm/prom.h>
#endif
#include <linux/timer.h>
#include <linux/ar7wdt.h>
#include "avm_sammel.h"

#ifdef CONFIG_MACH_AR7240
#include <ar7240.h>
#else
#include <atheros.h>
#endif

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
/*--- #define AVM_WATCHDOG_DEBUG ---*/

#if defined(AVM_WATCHDOG_DEBUG)
#define DBG_ERR(...)   printk(KERN_ERR __VA_ARGS__)
#define DBG_INFO(...)  printk(KERN_INFO __VA_ARGS__)
#else /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/
#define DBG_ERR(...)  
#define DBG_INFO(...)  
#endif /*--- #else ---*/ /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/

#ifdef CONFIG_MACH_AR7240
extern uint32_t ar7240_ahb_freq;
uint32_t ath_ahb_freq = ar7240_ahb_freq;
#else
extern uint32_t ath_ahb_freq;
#endif

#define TIME_OUT_SECS   10
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_init(void) {
    DBG_ERR( "[ath:watchdog] start ...\n");
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
    if(nmi_workaround_func.cb_ath_workaround_watchdog) {
        ath_workaround_watchdog(TIME_OUT_SECS);
    } else 
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
    {
        ath_reg_wr(ATH_WATCHDOG_TMR, (TIME_OUT_SECS * ath_ahb_freq));
#if defined(CONFIG_MACH_AR724x)
        ath_reg_wr(ATH_WATCHDOG_TMR_CONTROL, ATH_WD_ACT_RESET); 
#else
        ath_reg_wr(ATH_WATCHDOG_TMR_CONTROL, ATH_WD_ACT_NMI);
#endif
    }
    DBG_ERR( "[ath:watchdog] action: %u (hardware reset) - ticks: %u (= %u seconds * %u hz)\n", 
            ath_reg_rd(ATH_WATCHDOG_TMR_CONTROL), 
            TIME_OUT_SECS * ath_ahb_freq, 
            TIME_OUT_SECS, ath_ahb_freq);
    return;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_deinit(void) {
    DBG_ERR( "[ath:watchdog] stop ...\n");
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
    if(nmi_workaround_func.cb_ath_workaround_watchdog) {
        ath_workaround_watchdog(0);
    } else 
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
    {
        ath_reg_wr(ATH_WATCHDOG_TMR_CONTROL, ATH_WD_ACT_NONE);
    }
    return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7wdt_hw_reboot(void) {
    DBG_ERR("ar7wdt_hw_reboot!!\n");
    panic("ar7wdt_hw_reboot: watchdog expired\n");
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7wdt_hw_trigger(void) {
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
    if(nmi_workaround_func.cb_ath_workaround_watchdog) {
        ath_workaround_watchdog(TIME_OUT_SECS);
    } else  
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
    {
#ifdef AVM_WATCHDOG_DEBUG
        unsigned int ticks_left = ath_reg_rd(ATH_WATCHDOG_TMR);
        DBG_ERR( "[ath:watchdog] %x %x triggered %u.%u secs before reset\n", 
                ath_reg_rd(ATH_WATCHDOG_TMR_CONTROL), 
                ath_reg_rd(ATH_WATCHDOG_TMR), 
                ticks_left / ath_ahb_freq, (((ticks_left%ath_ahb_freq)*10U) / ath_ahb_freq) % 10);
#endif
        ath_reg_wr(ATH_WATCHDOG_TMR, (TIME_OUT_SECS * ath_ahb_freq));
    }
}

#endif /*--- #if defined(CONFIG_AVM_WATCHDOG) ---*/


