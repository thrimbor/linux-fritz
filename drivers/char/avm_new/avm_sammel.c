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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/fs.h>
/*--- #include <asm/semaphore.h> ---*/

/*------------------------------------------------------------------------------------------*\
 *  2.6 Kernel H-Files
\*------------------------------------------------------------------------------------------*/
#if KERNEL_VERSION(2, 6, 0) < LINUX_VERSION_CODE 
#include <linux/cdev.h>
#include <asm/mach_avm.h>
#endif 

/*------------------------------------------------------------------------------------------*\
 *  2.4 Kernel H-Files
\*------------------------------------------------------------------------------------------*/
#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE 
#include <linux/devfs_fs_kernel.h>
#include <asm-mips/avalanche/sangam/sangam.h>
#include <asm-mips/avalanche/sangam/sangam_clk_cntl.h>
#include <asm/avalanche/generic/led_config.h>
#include <asm/avalanche/generic/avalanche_misc.h>
#include <asm/smplock.h>
#endif 

#define AVM_EVENT_INTERNAL
#include <linux/ar7wdt.h>
#include <linux/avm_event.h>

#if defined(CONFIG_MIPS)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
#include <asm/mips-boards/prom.h>
#else/*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28) ---*/
#include <asm/prom.h>
#endif/*--- #else ---*//*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28) ---*/
#else
#include <linux/env.h>
#include <asm/setup.h>
#endif /*--- #if defined(CONFIG_MIPS) ---*/
#include "avm_sammel.h"
#include "avm_led.h"
#include "avm_event.h"
#include "avm_debug.h"
#include "ar7wdt.h"

#define MODULE_NAME     "avm"
MODULE_DESCRIPTION("AR7 Watchdog Timer + LED Driver + AVM Central Event distribution");
MODULE_LICENSE("GPL");


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ar7wdt_no_reboot = 0;
module_param(ar7wdt_no_reboot, int,  0744);

int avm_event_enable_push_button = 0;
module_param(avm_event_enable_push_button, int, 0744);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int __init avm_sammel_init(void) {
    int ret;
    char *hwrev;
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    hwrev = prom_getenv("HWRevision");
    /*--- printk("HWRevision=\"%s\"\n", hwrev); ---*/
    if(hwrev) {
        char buff[20];
        char *p;

        strcpy(buff, hwrev);
        strcat(buff, " ");
        /*----------------------------------------------------------------------------------*\
         * aus "94.0.0.1 "  -->   "94 " erzeugen
        \*----------------------------------------------------------------------------------*/
        p = strchr(buff, '.');
        if(p) {
            *p++ = ' ';
            *p++ = '\0';
        }

        if(strstr(AVM_NEW_HWREV_LIST, buff)) {
            avm_event_enable_push_button = 1;
        }
    }

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    printk("[avm] configured: "
#ifdef CONFIG_AVM_WATCHDOG
            "watchdog "
#endif
#ifdef CONFIG_AVM_WATCHDOG_MODULE
            "watchdog (module) "
#endif
#ifdef CONFIG_AVM_EVENT
            "event "
#endif
#ifdef CONFIG_AVM_DEBUG
            "debug "
#endif
#ifdef CONFIG_AVM_EVENT_MODULE
            "event (module) "
#endif
#ifdef CONFIG_AVM_LED
            "led "
#endif
#ifdef CONFIG_AVM_LED_MODULE
            "led (module) "
#endif
#ifdef CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER
            "enable shift register "
#endif 
#ifdef CONFIG_AVM_LED_OUTPUT_GPIO
            "enable direct gpio "
#endif
            "\n");
#ifdef CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER
    printk("\tgpio usage: "
            "reset=%u "
            "clock=%u "
            "store=%u "
            "data=%u ",
            CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_MASTER_RESET,
            CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_SHIFT_CLOCK,
            CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_STORAGE_CLOCK,
            CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_DATA);
#endif /*--- #ifdef CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER ---*/
    printk("\n");
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/

#if defined(CONFIG_AVM_WATCHDOG) || defined(CONFIG_AVM_WATCHDOG_MODULE)
    ret = ar7wdt_init();
    if(ret) {
        printk("[avm]: ar7wdt_init: failed\n");
        return ret;
    }
#endif /*--- #if (CONFIG_AVM_WATCHDOG == 1) || (CONFIG_AVM_WATCHDOG_MODULE == 1) ---*/
#ifdef CONFIG_AVM_LED
#ifdef CONFIG_AVM_LED_MODULE
    ret = avm_led_init() || avm_led_load_config();
#else /*--- #ifdef CONFIG_AVM_LED_MODULE ---*/
    ret = avm_led_init();
#endif /*--- #else ---*/ /*--- #ifdef CONFIG_AVM_LED_MODULE ---*/
    if(ret) {
        printk("[avm]: led_init: failed\n");
        return ret;
    }
#endif /*--- #ifdef CONFIG_AVM_LED ---*/
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)
    ret = avm_event_init();
    if(ret) {
        printk("[avm]: avm_event_init: failed\n");
        return ret;
    }
#endif /*--- #if (CONFIG_AVM_EVENT == 1) || (CONFIG_AVM_EVENT_MODULE == 1) ---*/
#if defined(CONFIG_AVM_DEBUG) || defined(CONFIG_AVM_DEBUG_MODULE)
    ret = avm_debug_init();
    if(ret) {
        printk("[avm]: avm_event_init: failed\n");
        return ret;
    }
#if defined(CONFIG_AVM_SIMPLE_PROFILING)
    avm_profiler_init();
    printk("[avm]: AVM Simple Profiling enabled.\n");
#endif/*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
#endif /*--- #if (CONFIG_AVM_DEBUG == 1) || (CONFIG_AVM_DEBUG_MODULE == 1) ---*/
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
module_init(avm_sammel_init);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(AVM_SAMMEL_MODULE)
int avm_sammel_deinit(void) {
#if (CONFIG_AVM_WATCHDOG_MODULE == 1)
    ar7wdt_cleanup();
#endif /*--- #if (CONFIG_AVM_WATCHDOG_MODULE == 1) ---*/
#ifdef CONFIG_AVM_LED
    avm_led_cleanup();
#endif /*--- #ifdef CONFIG_AVM_LED ---*/
#if (CONFIG_AVM_EVENT_MODULE == 1)
    avm_event_cleanup();
#endif /*--- #if (CONFIG_AVM_EVENT_MODULE == 1) ---*/
#if (CONFIG_AVM_EVENT_MODULE == 1)
    avm_event_cleanup();
#endif /*--- #if (CONFIG_AVM_EVENT_MODULE == 1) ---*/
#if (CONFIG_AVM_DEBUG_MODULE == 1)
#if defined(CONFIG_AVM_SIMPLE_PROFILING)
    avm_profiler_exit();
#endif/*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
    avm_debug_cleanup();
#endif /*--- #if (CONFIG_AVM_DEBUG_MODULE == 1) ---*/
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
module_exit(avm_sammel_deinit);
#endif /*--- #if defined(AVM_SAMMEL_MODULE) ---*/
