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
#include <linux/interrupt.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/ar7wdt.h>
#if defined(CONFIG_FUSIV_VX185)
#include <vx185.h>
#else /*--- #if defined(CONFIG_FUSIV_VX185) ---*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
#include <asm/mips-boards/prom.h>
#else
#include <asm/prom.h>
#endif
#include <vx180.h>
#include "ikan_wdt.h"
#include "ikan6850.h"
#endif/*--- #else  ---*//*--- #if defined(CONFIG_FUSIV_VX185) ---*/
#include "avm_sammel.h"

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


/*--- #define WATCHDOG_USE_TIMER3 ---*/
#if defined(WATCHDOG_USE_TIMER3)
#define TIMER3_BASE             0xB9070040
#define TIMER3_CPE_CLB_STS      ((volatile unsigned short *)(TIMER3_BASE + 0x00))
#define TIMER3_CPE_CFG          ((volatile unsigned short *)(TIMER3_BASE + 0x04))
#define TIMER3_CPE_COUNTER_LO   ((volatile unsigned short *)(TIMER3_BASE + 0x08))
#define TIMER3_CPE_COUNTER_HI   ((volatile unsigned short *)(TIMER3_BASE + 0x0C))
#define TIMER3_CPE_PERIOD_LO    ((volatile unsigned short *)(TIMER3_BASE + 0x10))
#define TIMER3_CPE_PERIOD_HI    ((volatile unsigned short *)(TIMER3_BASE + 0x14))
#define TIMER3_CPE_WIDTH_LO     ((volatile unsigned short *)(TIMER3_BASE + 0x18))
#define TIMER3_CPE_WIDTH_HI     ((volatile unsigned short *)(TIMER3_BASE + 0x1C))
#endif /*--- #if defined(WATCHDOG_USE_TIMER3) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(WATCHDOG_USE_TIMER3)
irqreturn_t ar7wdt_hw_irq(int irq, void *handle) {
    extern void fusiv_set_irq_prio(unsigned int irq_nr, unsigned int prio);
    static int count = 10;
    *TIMER3_CPE_CLB_STS      = 1 << 2;  /*--- ack ---*/
    if(count) {
        count--;
    } else {
        /*--- *TIMER3_CPE_CLB_STS      = 1 << 13; ---*/
        printk(KERN_ERR "INT_PRTY_2: 0x%08x ", *((volatile unsigned int *)(0xb9050108)));
        fusiv_set_irq_prio(irq, 6);
        printk(KERN_ERR "INT_PRTY_2: 0x%08x ", *((volatile unsigned int *)(0xb9050108)));
    }
    printk(KERN_ERR "################### IRQ Timer 3 %d #####################\n", count);

    return IRQ_HANDLED;
}

#endif /*--- #if defined(WATCHDOG_USE_TIMER3) ---*/


/*-----------------------------------------------------------------------------------------------*\
 * System Clock: ca. 166 MHZ 
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_init(void) {
    DBG_ERR( "[vx180:watchdog] start ...\n");
#if defined(WATCHDOG_USE_TIMER3)
    unsigned long addr;
    unsigned long period;
    unsigned long width;
    int ret;

    unsigned long sec = 10;
    unsigned long hz = 166000000;
    period = hz * sec;
    width = period / 2;

#define TIMER3_CPE_CLB_STS      ((volatile unsigned short *)(TIMER3_BASE + 0x00))
#define TIMER3_CPE_CFG          ((volatile unsigned short *)(TIMER3_BASE + 0x04))
#define TIMER3_CPE_COUNTER_LO   ((volatile unsigned short *)(TIMER3_BASE + 0x08))
#define TIMER3_CPE_COUNTER_HI   ((volatile unsigned short *)(TIMER3_BASE + 0x0C))
#define TIMER3_CPE_PERIOD_LO    ((volatile unsigned short *)(TIMER3_BASE + 0x10))
#define TIMER3_CPE_PERIOD_HI    ((volatile unsigned short *)(TIMER3_BASE + 0x14))
#define TIMER3_CPE_WIDTH_LO     ((volatile unsigned short *)(TIMER3_BASE + 0x18))
#define TIMER3_CPE_WIDTH_HI     ((volatile unsigned short *)(TIMER3_BASE + 0x1C))

    ret = request_irq(GPT3_INT, ar7wdt_hw_irq, IRQF_DISABLED, "Watchdog", NULL);

*TIMER3_CPE_CFG          = 0x1d;
*TIMER3_CPE_PERIOD_HI    = (period >> 16);
*TIMER3_CPE_PERIOD_LO    = (period & 0x00FFFF);
*TIMER3_CPE_WIDTH_HI     = (width >> 16);
*TIMER3_CPE_WIDTH_LO     = (width & 0x00FFFF);
*TIMER3_CPE_CLB_STS      = 0x1000;
#elif !defined(CONFIG_FUSIV_VX185)/*--- #if defined(WATCHDOG_USE_TIMER3) ---*/
    *FUSIV_WDT_CTRL  = FUSIV_WDT_CTRL_DISABLE | FUSIV_WDT_CTRL_RELOAD(0xFFFC); 
    *FUSIV_WDT_CTRL  = FUSIV_WDT_CTRL_SERVICE | FUSIV_WDT_CTRL_RELOAD(0xFFFC); 
    DBG_ERR( "[vx180:watchdog] status: 0x%x ...\n", *FUSIV_WDT_STAT);
    enable_irq(INT_SCU);
#elif defined(CONFIG_FUSIV_VX185)/*--- #if defined(WATCHDOG_USE_TIMER3) ---*/
    *VX185_TOP_COUNT_BASE   = 1U << 31; /*--- 250 MHz: ca. 8,5 Sekunden ---*/
    *VX185_CONTROL_WDT_BASE = VX185_CONTROL_WDT_TIMER_X_ENABLE | VX185_CONTROL_WDT_RESET_MASK | VX185_WDT_LOCKVALUE;
#else
#error unknown platform
#endif /*--- #elif !defined(CONFIG_FUSIV_VX185) ---*//*--- #if defined(WATCHDOG_USE_TIMER3) ---*/
    return;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void ar7wdt_hw_deinit(void) {
    DBG_ERR( "[vx180:watchdog] stop ...\n");
#if defined(WATCHDOG_USE_TIMER3)
#elif !defined(CONFIG_FUSIV_VX185)/*--- #if defined(WATCHDOG_USE_TIMER3) ---*/
    *FUSIV_WDT_CTRL = FUSIV_WDT_CTRL_DISABLE | FUSIV_WDT_CTRL_SERVICE;
    printk( "[vx180:watchdog] disabled status %x\n", *FUSIV_WDT_STAT);
#elif defined(CONFIG_FUSIV_VX185)/*--- #if defined(WATCHDOG_USE_TIMER3) ---*/
    *VX185_CONTROL_WDT_BASE = VX185_WDT_LOCKVALUE;
#else
#error unknown platform
#endif /*--- #elif !defined(CONFIG_FUSIV_VX185) ---*//*--- #if defined(WATCHDOG_USE_TIMER3) ---*/
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
#if defined(WATCHDOG_USE_TIMER3)
#elif !defined(CONFIG_FUSIV_VX185)/*--- #if defined(WATCHDOG_USE_TIMER3) ---*/
    unsigned int status;
    status = *(volatile unsigned int *)FUSIV_WDT_STAT;
    *FUSIV_WDT_CTRL  = FUSIV_WDT_CTRL_SERVICE; 
    DBG_INFO( "[vx180:watchdog] before trigger (status=0x%x) timer 0x%x %s%s%s (errors %d)\n", 
            status,
            status >> 16,
            status & (1 << 0) ? "warned " : "",
            status & (1 << 1) ? "disabled " : "",
            status & (1 << 2) ? "overflow" : "",
            (status >> 3) & 0x3);

    status = *(volatile unsigned int *)FUSIV_WDT_STAT;
    DBG_INFO( "[vx180:watchdog] after trigger (status=0x%x) timer 0x%x %s%s%s (errors %d)\n", 
            status,
            status >> 16,
            status & (1 << 0) ? "warned " : "",
            status & (1 << 1) ? "disabled " : "",
            status & (1 << 2) ? "overflow" : "",
            (status >> 3) & 0x3);
#elif defined(CONFIG_FUSIV_VX185)/*--- #if defined(WATCHDOG_USE_TIMER3) ---*/
    /*--- printk(KERN_ERR"%s: %p %x %x %x\n", __func__, VX185_CONTROL_WDT_BASE, *VX185_CONTROL_WDT_BASE, *VX185_TOP_COUNT_BASE, *VX185_PET_WDT_BASE); ---*/
    *VX185_PET_WDT_BASE = VX185_WDT_LOCKVALUE;
#else
#error unknown platform
#endif /*--- #elif !defined(CONFIG_FUSIV_VX185) ---*//*--- #if defined(WATCHDOG_USE_TIMER3) ---*/

}

#endif /*--- #if defined(CONFIG_AVM_WATCHDOG) ---*/


