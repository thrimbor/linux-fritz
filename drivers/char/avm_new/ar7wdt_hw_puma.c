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
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/ar7wdt.h>
#include <mach/hw_timer.h>
#include <arch-avalanche/generic/pal.h>
#include "avm_sammel.h"

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
// #define AVM_WATCHDOG_DEBUG
#if defined(AVM_WATCHDOG_DEBUG)
#define DBG(...)  printk(KERN_INFO __VA_ARGS__)
#else /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/
#define DBG(...)  
#endif /*--- #else ---*/ /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int ar7wdt_hw_is_wdt_running(void)
{
    volatile PAL_SYS_WDTIMER_STRUCT_T *p_wdtimer = AVALANCHE_WATCHDOG_TIMER_BASE;

    return p_wdtimer->disable != 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_init(void)
{
    int result;

    // get the watchdog module out of reset and initialize watchdog timer
    PAL_sysResetCtrl(AVALANCHE_WDT_RESET, OUT_OF_RESET);
    PAL_sysWdtimerInit(AVALANCHE_WATCHDOG_TIMER_BASE, PAL_sysClkcGetFreq(PAL_SYS_CLKC_WDT));

    // disable watchdog timer
    result = PAL_sysWdtimerCtrl(AVALANCHE_WDT_DISABLE_VALUE);
    if(result){
        printk(KERN_ERR "[%s] Disabling watchdog timer failed, aborting!\n", __func__);
        goto wdt_set_err;
    }

    // set default timeout value in ms
    result = PAL_sysWdtimerSetPeriod(AVALANCHE_WDT_MARGIN_MAX_VAL * 1000);
    if(result){
        printk(KERN_ERR "[%s] setting watchdog timer to %d seconds failed, aborting!\n", __func__, AVALANCHE_WDT_MARGIN_MAX_VAL);
        goto wdt_set_err;
    }

    // re-enable watchdog
    result = PAL_sysWdtimerCtrl(AVALANCHE_WDT_ENABLE_VALUE);
    if(result){
        printk(KERN_ERR "[%s] Re-enabling watchdog timer failed, aborting!\n", __func__);
        goto wdt_set_err;
    }

    // start watchdog by kicking it
    result = PAL_sysWdtimerKick();
    if(result){
        printk(KERN_ERR "[%s] Kicking the watchdog timer failed!\n", __func__);
    }

wdt_set_err:
    return;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_deinit(void)
{
    int result;

    DBG("stopping WDT\n");

    result = PAL_sysWdtimerCtrl(AVALANCHE_WDT_DISABLE_VALUE);
    if(result){
        printk(KERN_ERR "[%s] Disabling watchdog failed!\n", __func__);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7wdt_hw_reboot(void)
{
    DBG("ar7wdt_hw_reboot!!\n");
    panic("ar7wdt_hw_reboot: watchdog expired\n");
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7wdt_hw_trigger(void)
{
    int result;

    DBG("ar7wdt_hw_trigger !!\n");

    result = PAL_sysWdtimerKick();
    if(result){
        printk(KERN_ERR "[%s] Kicking the watchdog failed!\n", __func__);
    }
}

