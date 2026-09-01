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
#ifndef _AR7WDT_H_
#define _AR7WDT_H_

#include <linux/version.h>
#include "avm_sammel.h"
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int __init ar7wdt_init(void);
void ar7wdt_cleanup(void);

#if !defined(DEBUG_SAMMEL)
#define DEBUG_SAMMEL
#if defined(AVM_WATCHDOG_DEBUG)
#define DEB_ERR(args...)     printk(KERN_ERR args)
#define DEB_WARN(args...)    printk(KERN_WARNING args)
#define DEB_NOTE(args...)    printk(KERN_NOTICE args)
#define DEB_INFO(args...)    printk(KERN_INFO args)
#define DEB_TRACE(args...)   printk(KERN_INFO args)
#else /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/
#define DEB_ERR(args...)     printk(KERN_ERR args)
#define DEB_WARN(args...)
#define DEB_NOTE(args...)
#define DEB_INFO(args...)
#define DEB_TRACE(args...)
#endif /*--- #else ---*/ /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/
#endif /*--- #if !defined(DEBUG_SAMMEL) ---*/

/*------------------------------------------------------------------------------------------*\
 * wdtimer.c  (nur 2.4 kernel)
\*------------------------------------------------------------------------------------------*/
#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE 
/*--- #if !defined(UINT32) ---*/
#define UINT32      unsigned int
#define INT32       signed int
#include <asm-mips/avalanche/generic/hal_modules/wdtimer.h>
#undef UINT32
#undef INT32

void wdtimer_init (unsigned int base_addr, unsigned int vbus_clk_freq);
int wdtimer_set_period(unsigned int msec);
int wdtimer_ctrl(WDTIMER_CTRL_T wd_ctrl);
int wdtimer_kick(void);
int write_disable_reg(unsigned int disable_value);

#define ar7wdt_hw_deinit()        wdtimer_ctrl(WDTIMER_CTRL_DISABLE)
#define ar7wdt_hw_trigger()       wdtimer_kick()
#endif


#endif /*--- #ifndef _AR7WDT_H_ ---*/
