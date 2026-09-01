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
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <asm/prom.h>
#include <linux/timer.h>
#include "common_routines.h"
#if defined(CONFIG_AR9)
#include "ar9/ar9.h"
#elif defined(CONFIG_VR9)
#include "vr9/vr9.h"
#elif defined(CONFIG_AR10)
#include "ar10/ar10.h"
#else
#error unknown platform 
#endif
#include <linux/ar7wdt.h>
#include "avm_sammel.h"
#include "ifxmips_wdt.h"

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


/*------------------------------------------------------------------------------------------*\
 * Configure the prewarning level for WDT
 *   pwl    0 for 1/2 of the max WDT period
 *          1 for 1/4 of the max WDT period
 *          2 for 1/8 of the max WDT period
 *          3 for 1/16 of the max WDT period
\*------------------------------------------------------------------------------------------*/
static void wdt_prewarning_limit(int pwl)
{
    u32 wdt_cr=0;
    
    wdt_cr = *IFX_WDT_CR;
    *IFX_WDT_CR = IFX_WDT_CR_PW_SET(IFX_WDT_PW1);

    wdt_cr &= 0xff00ffff;//(!IFX_WDT_CR_PW_SET(0xff));
    wdt_cr &= 0xf3ffffff;//(!IFX_WDT_CR_PWL_SET(3));
    wdt_cr |= (IFX_WDT_CR_PW_SET(IFX_WDT_PW2) | 
               IFX_WDT_CR_PWL_SET(pwl));

    /* Set reload value in second password access */
    *IFX_WDT_CR = wdt_cr;
}

/*------------------------------------------------------------------------------------------*\
 * Configure the clock divider for WDT
 *   clkdiv    0 for CLK_WDT = 1 x CLK_TIMER
 *             1 for CLK_WDT = 64 x CLK_TIMER
 *             2 for CLK_WDT = 4096 x CLK_TIMER
 *             3 for CLK_WDT = 262144 x CLK_TIMER
\*------------------------------------------------------------------------------------------*/
static void wdt_set_clkdiv(int clkdiv)
{
    u32 wdt_cr=0;

    wdt_cr = *IFX_WDT_CR;
    *IFX_WDT_CR = IFX_WDT_CR_PW_SET(IFX_WDT_PW1);

    wdt_cr &= 0xff00ffff; //(!IFX_WDT_CR_PW_SET(0xff));
    wdt_cr &= 0xfcffffff; //(!IFX_WDT_CR_CLKDIV_SET(3));
    wdt_cr |= (IFX_WDT_CR_PW_SET(IFX_WDT_PW2) |
               IFX_WDT_CR_CLKDIV_SET(clkdiv));

    /* Set reload value in second password access */
    *IFX_WDT_CR = wdt_cr;
}

/*------------------------------------------------------------------------------------------*\
 * System Clock: ca. ??? MHZ 
\*------------------------------------------------------------------------------------------*/
static int wdt_enable(unsigned int timeout)
{
    unsigned int wdt_cr=0;
    unsigned int wdt_reload=0;
    unsigned int wdt_clkdiv=0, clkdiv, wdt_pwl=0, pwl, ffpi;
    
    /* clock divider & prewarning limit */
    clkdiv = IFX_WDT_CR_CLKDIV_GET(*IFX_WDT_CR);
    switch(clkdiv)
    {
        case 0: wdt_clkdiv = 1; break;
        case 1: wdt_clkdiv = 64; break;
        case 2: wdt_clkdiv = 4096; break;
        case 3: wdt_clkdiv = 262144; break;
    }
    
    pwl = IFX_WDT_CR_PWL_GET(*IFX_WDT_CR);
    switch(pwl)
    {
        case 0: wdt_pwl = 0x8000; break;
        case 1: wdt_pwl = 0x4000; break;
        case 2: wdt_pwl = 0x2000; break;
        case 3: wdt_pwl = 0x1000; break;
    }

    ffpi = ifx_get_fpi_hz();

    /* calculate reload value */
    wdt_reload = (timeout * (ffpi / wdt_clkdiv)) + wdt_pwl;

    if (wdt_reload > 0xFFFF)
    {
        DBG_ERR("[ifx:watchdog] timeout too large %d\n", timeout);

        DBG_ERR("[ifx:watchdog] wdt_pwl=0x%x, wdt_clkdiv=%d, ffpi=%d, wdt_reload = 0x%x\n", wdt_pwl, wdt_clkdiv, ffpi, wdt_reload);

        return -EINVAL;
    }
 
    /* Write first part of password access */
    *IFX_WDT_CR = IFX_WDT_CR_PW_SET(IFX_WDT_PW1);

    wdt_cr = *IFX_WDT_CR;
    wdt_cr &= ( !IFX_WDT_CR_PW_SET(0xff) &
                !IFX_WDT_CR_PWL_SET(0x3) &
                !IFX_WDT_CR_CLKDIV_SET(0x3) &
                !IFX_WDT_CR_RELOAD_SET(0xffff));

    wdt_cr |= ( IFX_WDT_CR_PW_SET(IFX_WDT_PW2) |
                IFX_WDT_CR_PWL_SET(pwl) |
                IFX_WDT_CR_CLKDIV_SET(clkdiv) |
                IFX_WDT_CR_RELOAD_SET(wdt_reload) |
                IFX_WDT_CR_GEN);

    /* Set reload value in second password access */
    *IFX_WDT_CR = wdt_cr;

    return 0 ;
}




/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_init(void) {
    DBG_ERR( "[ifx:watchdog] start ...\n");
    /*--- Setting first Password part: ---*/
    DBG_INFO( "[ifx:watchdog] ctrl: 0x%x ...\n", *IFX_WDT_CR);
    wdt_set_clkdiv(3);       /*--- 750 Hz Watchdog Clock (bei 196MHz Systemclock) ---*/
    wdt_prewarning_limit(3); /*--- 0x1000 Watchdog-Clock-Ticks => 5.5 Sekunden (bei 750 Hz Clock) ---*/
    wdt_enable(WDT_TIMEOUT_SEC);
    /*--- *IFX_WDT_CR  = IFX_WDT_CR_PW_SET(0xBE); ---*/   
    /*--- Setting second Password part & configuring: ---*/
    /*--- *IFX_WDT_CR  = IFX_WDT_CR_PW_SET(0xDC) | *IFX_WDT_CR  | IFX_WDT_CR_GEN; ---*/ 
             /*--- |  IFX_WDT_CR_PWL_SET(value) | IFX_WDT_CR_CLKDIV_SET(value) | IFX_WDT_CR_RELOAD_SET(value); ---*/
    DBG_INFO( "[ifx:watchdog] ctrl: 0x%x ...\n", *IFX_WDT_CR);
    DBG_ERR( "[ifx:watchdog] status: 0x%x ...\n", *IFX_WDT_SR);
    return;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_deinit(void) {
    DBG_ERR( "[ifx:watchdog] stop ...\n");

    /* Write first part of password access */
    *IFX_WDT_CR = IFX_WDT_CR_PW_SET(IFX_WDT_PW1);

    /* Disable the watchdog in second password access (GEN=0) */
    *IFX_WDT_CR = IFX_WDT_CR_PW_SET(IFX_WDT_PW2);
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
    unsigned int status;
    status = *IFX_WDT_SR;
    DBG_ERR( "[ifx:watchdog] before trigger (status=0x%x) timer 0x%x %s%s%s\n", 
            status,
            status & 0xFFFF,
            status & IFX_WDT_SR_PRW   ? "warned "   : "",
            !(status & IFX_WDT_SR_EN) ? "disabled " : "",
            status & IFX_WDT_SR_EXP   ? "overflow"  : "" );

    wdt_enable(WDT_TIMEOUT_SEC);

    status = *IFX_WDT_SR;
    DBG_ERR( "[ifx:watchdog] before trigger (status=0x%x) timer 0x%x %s%s%s\n", 
            status,
            status & 0xFFFF,
            status & IFX_WDT_SR_PRW   ? "warned "   : "",
            !(status & IFX_WDT_SR_EN) ? "disabled " : "",
            status & IFX_WDT_SR_EXP   ? "overflow"  : "" );

}

#endif /*--- #if defined(CONFIG_AVM_WATCHDOG) ---*/


