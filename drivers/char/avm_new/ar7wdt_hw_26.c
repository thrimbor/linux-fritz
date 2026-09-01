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
#include <linux/version.h>
#include <linux/init.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#if defined(CONFIG_MIPS_OHIO)
#include <asm/mips-boards/ohio.h>
#elif defined(CONFIG_MIPS_UR8)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
#include <asm/mach-ur8/ur8.h>
#else
#include <asm/mips-boards/ur8.h>
#endif
#endif
#include <linux/timer.h>
#include <linux/ar7wdt.h>
#if defined(CONFIG_ARCH_PUMA5) || defined(CONFIG_MACH_PUMA6)
#include <mach/hw_timer.h>
#endif/*--- #if defined(CONFIG_ARCH_PUMA5) ---*/
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
#if defined(CONFIG_MIPS_OHIO)
#define WDT_KICK_LOCK       (*(volatile unsigned *)(OHIO_WDT_BASE + 0x00))
#define WDT_KICK            (*(volatile unsigned *)(OHIO_WDT_BASE + 0x04))
#define WDT_CHANGE_LOCK     (*(volatile unsigned *)(OHIO_WDT_BASE + 0x08))
#define WDT_CHANGE          (*(volatile unsigned *)(OHIO_WDT_BASE + 0x0C)) 
#define WDT_DISABLE_LOCK    (*(volatile unsigned *)(OHIO_WDT_BASE + 0x10))
#define WDT_DISABLE         (*(volatile unsigned *)(OHIO_WDT_BASE + 0x14))
#define WDT_PRESCALE_LOCK   (*(volatile unsigned *)(OHIO_WDT_BASE + 0x18))
#define WDT_PRESCALE        (*(volatile unsigned *)(OHIO_WDT_BASE + 0x1C))
#elif defined(CONFIG_MIPS_UR8)
#define WDT_KICK_LOCK       (*(volatile unsigned *)(UR8_WDT_BASE + 0x00))
#define WDT_KICK            (*(volatile unsigned *)(UR8_WDT_BASE + 0x04))
#define WDT_CHANGE_LOCK     (*(volatile unsigned *)(UR8_WDT_BASE + 0x08))
#define WDT_CHANGE          (*(volatile unsigned *)(UR8_WDT_BASE + 0x0C)) 
#define WDT_DISABLE_LOCK    (*(volatile unsigned *)(UR8_WDT_BASE + 0x10))
#define WDT_DISABLE         (*(volatile unsigned *)(UR8_WDT_BASE + 0x14))
#define WDT_PRESCALE_LOCK   (*(volatile unsigned *)(UR8_WDT_BASE + 0x18))
#define WDT_PRESCALE        (*(volatile unsigned *)(UR8_WDT_BASE + 0x1C))
#elif defined(CONFIG_ARCH_PUMA5) || defined(CONFIG_MACH_PUMA6)
#define WDT_KICK_LOCK       (*(volatile unsigned *)(PUMA_WDT_BASE + 0x00))
#define WDT_KICK            (*(volatile unsigned *)(PUMA_WDT_BASE + 0x04))
#define WDT_CHANGE_LOCK     (*(volatile unsigned *)(PUMA_WDT_BASE + 0x08))
#define WDT_CHANGE          (*(volatile unsigned *)(PUMA_WDT_BASE + 0x0C)) 
#define WDT_DISABLE_LOCK    (*(volatile unsigned *)(PUMA_WDT_BASE + 0x10))
#define WDT_DISABLE         (*(volatile unsigned *)(PUMA_WDT_BASE + 0x14))
#define WDT_PRESCALE_LOCK   (*(volatile unsigned *)(PUMA_WDT_BASE + 0x18))
#define WDT_PRESCALE        (*(volatile unsigned *)(PUMA_WDT_BASE + 0x1C))
#endif


/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int ar7wdt_hw_is_wdt_running(void) {
    return WDT_DISABLE == 0x1;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_init(void) {
#ifdef CONFIG_MIPS_FUSIV
#else /*--- #ifdef CONFIG_MIPS_FUSIV ---*/
    unsigned int reg;
    reg = WDT_DISABLE;
    DBG("Watchdog disable reg = %08x\n", reg);
    reg = WDT_CHANGE;
    DBG("Watchdog change reg = %08x\n", reg);
    reg = WDT_PRESCALE;
    DBG("Watchdog prescale reg = %08x\n", reg);
    DBG("setting WDT\n");

    WDT_CHANGE_LOCK = 0x6666;
    WDT_CHANGE_LOCK = 0xBBBB;
    WDT_CHANGE = 0xABCD;
    WDT_PRESCALE_LOCK = 0x5A5A;
    WDT_PRESCALE_LOCK = 0xA5A5;
    WDT_PRESCALE = 0xAFFE;
    reg = WDT_CHANGE;
    DBG("Watchdog change reg = %08x\n", reg);
    reg = WDT_PRESCALE;
    DBG("Watchdog prescale reg = %08x\n", reg);

    DBG("starting WDT\n");
    WDT_DISABLE_LOCK = 0x7777;
    WDT_DISABLE_LOCK = 0xCCCC;
    WDT_DISABLE_LOCK = 0xDDDD;
    WDT_DISABLE = 0x1;

    reg = WDT_DISABLE;
    DBG("Watchdog disable reg = %08x\n", reg);
#endif/*--- #else  ---*//*--- #ifdef CONFIG_MIPS_FUSIV ---*/

    return;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_deinit(void) {
    DBG("stopping WDT\n");
#ifdef CONFIG_MIPS_FUSIV
#else /*--- #ifdef CONFIG_MIPS_FUSIV ---*/
    WDT_DISABLE_LOCK = 0x7777;
    WDT_DISABLE_LOCK = 0xCCCC;
    WDT_DISABLE_LOCK = 0xDDDD;
    WDT_DISABLE = 0x0;
#endif/*--- #else  ---*//*--- #ifdef CONFIG_MIPS_FUSIV ---*/
    return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7wdt_hw_reboot(void) {
    DBG("ar7wdt_hw_reboot!!\n");
    panic("ar7wdt_hw_reboot: watchdog expired\n");
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7wdt_hw_trigger(void) {
    DBG("ar7wdt_hw_trigger !!\n");
#ifdef CONFIG_MIPS_FUSIV
#else /*--- #ifdef CONFIG_MIPS_FUSIV ---*/
    WDT_KICK_LOCK = 0x5555;
    WDT_KICK_LOCK = 0xAAAA;
    WDT_KICK      = 0xAAAA;
#endif/*--- #else  ---*//*--- #ifdef CONFIG_MIPS_FUSIV ---*/
}

#endif /*--- #if defined(CONFIG_AVM_WATCHDOG) ---*/

