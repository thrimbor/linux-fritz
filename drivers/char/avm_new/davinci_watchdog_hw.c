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
#include <linux/init.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <linux/ar7wdt.h>
#include "avm_sammel.h"

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
/*--- #define AVM_WATCHDOG_DEBUG ---*/

#if defined(AVM_WATCHDOG_DEBUG)
#define DBG(...)  printk(KERN_INFO __VA_ARGS__)
#else /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/
#define DBG(...)  
#endif /*--- #else ---*/ /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/

#include <asm/arch/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/hw_timer.h>

volatile struct timer *TIMER2 = (struct timer *)io_p2v(DAVINCI_TIMER2_BASE);

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_init(void) {
    unsigned int reg;

    TIMER2->tcr.Register = 0;
    TIMER2->tgcr.Register = 0;

    TIMER2->tgcr.Register = 8;  /* timer mode 64 Bit Watchdog */
    TIMER2->tgcr.Register = 0xb;

    /* assume clock ist  27 MHz */
    TIMER2->prd12         = 27000000 * 40;
    TIMER2->prd34         = 0;

    TIMER2->wdtcr.Register = 0x4000;
    TIMER2->wdtcr.Register = 0xDA7E4000;        /*--- watchdog to activate state ---*/

    return;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_deinit(void) {
    TIMER2->prd12         = (unsigned int)-1;
    TIMER2->prd34         = (unsigned int)-1;
    DBG("stopping WDT (wait for long)\n");
    return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void davinci_watchdog_reset(void) {
    printk("reboot by watchdog hardware ...\n");
    /* illegal if watchdog enable */
    TIMER2->wdtcr.Register = 0x00004000; 
    TIMER2->wdtcr.Register = 0xA5C64000;        /*--- watchdog to pre-activate state ---*/
    TIMER2->wdtcr.Register = 0x00004000;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7wdt_hw_reboot(void) {
    panic("[ar7wdt_hw_reboot] soft reboot\n");
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7wdt_hw_trigger(void) {
    TIMER2->wdtcr.Register = 0xA5C64000;        /*--- watchdog to pre-activate state ---*/
    TIMER2->wdtcr.Register = 0xDA7E4000;        /*--- watchdog to activate state ---*/
}

