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
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/ar7wdt.h>

/*--- #include <linux/proc_fs.h> ---*/
#include <linux/sched.h>
#include <linux/unistd.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/fs.h>

#include <linux/mm.h>
#include <linux/vmstat.h>

static void AVM_WATCHDOG_check_all_triggered(void);

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#if defined(CONFIG_ARM)
#define __printk    printk
#endif
/*--- #define AVM_WATCHDOG_DEBUG ---*/

#if defined(AVM_WATCHDOG_DEBUG)
#define DBG(...)  printk(KERN_INFO __VA_ARGS__)
#else /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/
#define DBG(...)  
#endif /*--- #else ---*/ /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/

#include "avm_sammel.h"
#include "ar7wdt.h"

/*--- #define DBG_TRIGGER(args...)     printk(args) ---*/
#define DBG_TRIGGER(args...)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _ar7wdt_data ar7wdt_data;
struct timer_list ar7wdt_timer;
struct timer_list PanicTimer;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void AVM_WATCHDOG_timer_handler(unsigned long);
static struct semaphore ar7wdt_sema;
extern int ar7wdt_no_reboot;

/*--- #define WATCHDOG_LIST_TASK_STATISTIC ---*/
#if defined(WATCHDOG_LIST_TASK_STATISTIC)
static void show_task_statistic(void);
#endif/*--- #if defined(WATCHDOG_LIST_TASK_STATISTIC) ---*/
static unsigned long get_act_pagefaults(void);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void AVM_WATCHDOG_timer_handler_action(unsigned int handle);
volatile unsigned int AVM_WATCHDOG_locked = 0;

static DEFINE_SPINLOCK(ar7_wdt_lock);
static DEFINE_SPINLOCK(ar7_wdt_timer_lock);
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int _AVM_WATCHDOG_atoi(char *p) {
    int value = 0;

    while(p && *p && ((*p < '0') || (*p > '9')))
        p++;

    while(p && *p) {
        if(*p >= '0' && *p <= '9') {
            value *= 10;
            value += *p - '0';
        } else {
            break;
        }
        p++;
    }
    return value;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AVM_WATCHDOG_DEL_TIMER    0x01
#define AVM_WATCHDOG_SET_TIMER    0x02
static void _AVM_WATCHDOG_ctrl_timer(int flags, int i) {
    unsigned long lockflags;
    struct timer_list *timer;
    int default_time;

    DBG(KERN_INFO "_AVM_WATCHDOG_ctrl_timer(flags=0x%x, handle=%d)\n", flags, i + 1);

    if(i == -1) {
        timer = &ar7wdt_timer;
#if defined(CONFIG_MIPS_FUSIV) || defined(CONFIG_LANTIQ) || defined(CONFIG_MACH_ATHEROS)
        default_time = 5;
#elif defined(CONFIG_ARCH_PUMA5) || defined(CONFIG_MACH_PUMA6)
        default_time = 10;
#else /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/
        default_time = 20;
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_MIPS_FUSIV) ---*/
    } else {
        timer        = &(ar7wdt_data.appl[i].Timer);
        default_time = ar7wdt_data.appl[i].default_time;
    }
    spin_lock_irqsave(&ar7_wdt_timer_lock, lockflags);
    if(flags & AVM_WATCHDOG_DEL_TIMER) {
        del_timer(timer);
    }
    if(flags & AVM_WATCHDOG_SET_TIMER) {
        mod_timer(timer, jiffies + HZ * default_time);
    }
    spin_unlock_irqrestore(&ar7_wdt_timer_lock, lockflags);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void AVM_WATCHDOG_init(void) {
    DBG(KERN_INFO "AVM_WATCHDOG_init:\n");
    memset(&ar7wdt_data, 0x00, sizeof(ar7wdt_data));
    sema_init(&ar7wdt_sema, 1);
	init_timer(&ar7wdt_timer);
    ar7wdt_timer.function = AVM_WATCHDOG_timer_handler;
    ar7wdt_timer.data     = -1;
    _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER, -1);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void AVM_WATCHDOG_deinit(void) {
    DBG(KERN_INFO "AVM_WATCHDOG_deinit:\n");
    _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, -1);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_find_handle(char *name, int len __attribute__((unused))) {
    int i;
    DBG(KERN_ERR "AVM_WATCHDOG_find_handle('%s', len=%u):\n", name, len);
    for( i = 0 ; i < MAX_WDT_APPLS ; i++) {
        if(ar7wdt_data.mask & (1 << i)) {
            if(!strcmp(ar7wdt_data.appl[i].Name, name)) {
                DBG(" handle=%u\n", i + 1);
                return i + 1;
            }
        }
    }
    DBG(" handle= not found\n");
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_strip_name(char **name, int len) {
    DBG(KERN_ERR "AVM_WATCHDOG_strip_name('%s', len=%u): ", *name, len);
    while((*name)[len - 1] == '\n' || (*name)[len - 1] == '\r')
        (*name)[len - 1] = '\0', len = strlen(*name);
    len = min(len, MAX_WDT_NAME_LEN - 1);
    (*name)[len] = '\0';
    len = strlen(*name);
    DBG("-->('%s', len=%u)", *name, len);
    while(len && (**name == ' ' || **name == '\t' || **name == '\n' || **name == '\r'))
        (*name)++, len--;
    DBG("-->('%s', len=%u):\n", *name, len);
    return len;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void AVM_WATCHDOG_timer_init_ctrl_handler(unsigned long _handle __attribute__((unused))) {
    if(ar7wdt_no_reboot == 0) {
        panic("ar7wdt_hw_reboot: init sequence hangs !\n");
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_init_start(int handle __attribute__((unused)), char *name, int len __attribute__((unused))) {
    /*--- ignore handle ---*/
    unsigned int Sekunden = _AVM_WATCHDOG_atoi(name);
    if(Sekunden == 0) Sekunden = 120; /*--- 2 Minuten ---*/

    {
        char hw_wdt_is_running = 1;

#ifdef CONFIG_MIPS_UR8
        hw_wdt_is_running = ar7wdt_hw_is_wdt_running();
#elif defined(CONFIG_ARCH_PUMA5) || defined(CONFIG_MACH_PUMA6)
        hw_wdt_is_running = ar7wdt_hw_is_wdt_running();
#elif defined(CONFIG_MIPS_FUSIV)
        #warning WATCHDOG: Ein wiederholtes initialisieren des Watchdogs auf dieser HW nicht implementiert!
#elif defined(CONFIG_LANTIQ)
        #warning WATCHDOG: Ein wiederholtes initialisieren des Watchdogs auf dieser HW nicht implementiert!
#elif defined(CONFIG_MACH_ATHEROS)
        #warning WATCHDOG: Ein wiederholtes initialisieren des Watchdogs auf dieser HW nicht implementiert!
#elif defined(CONFIG_ARCH_DAVINCI)
        #warning WATCHDOG: Ein wiederholtes initialisieren des Watchdogs auf dieser HW nicht implementiert!
#else
        #warning WATCHDOG: Ein wiederholtes initialisieren des Watchdogs auf dieser HW nicht implementiert!
#endif

        if(!hw_wdt_is_running) {
            ar7wdt_hw_init();
            ar7wdt_hw_trigger();
        }
    }

    init_waitqueue_head(&(ar7wdt_data.appl[MAX_WDT_APPLS].wait_queue));
    ar7wdt_data.appl[MAX_WDT_APPLS].fasync         = NULL;
    ar7wdt_data.appl[MAX_WDT_APPLS].default_time   = Sekunden;
    ar7wdt_data.appl[MAX_WDT_APPLS].Timer.function = AVM_WATCHDOG_timer_init_ctrl_handler;
    ar7wdt_data.appl[MAX_WDT_APPLS].Timer.data     = MAX_WDT_APPLS + 1; /*--- handle ---*/

    init_timer(&(ar7wdt_data.appl[MAX_WDT_APPLS].Timer));

    strcpy(ar7wdt_data.appl[MAX_WDT_APPLS].Name, "init-ctrl");

#ifdef CONFIG_PRINTK
    printk("AVM_WATCHDOG: System Init UEberwachung %u Sekunden\n", Sekunden);
#endif /*--- #ifdef CONFIG_PRINTK ---*/
    _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER, MAX_WDT_APPLS);
    return MAX_WDT_APPLS + 1;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_init_done(int handle, char *name __attribute__((unused)), int len __attribute__((unused))) {
#ifdef CONFIG_PRINTK
    printk("AVM_WATCHDOG: System Init UEberwachung abgeschlossen (%lu ms noch verfuegbar)\n", 10 * (ar7wdt_data.appl[MAX_WDT_APPLS].Timer.expires - jiffies));
#endif /*--- #ifdef CONFIG_PRINTK ---*/
    _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, MAX_WDT_APPLS);
    return handle;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void AVM_WATCHDOG_ungraceful_release(int handle)
{
    DBG(KERN_INFO "AVM_WATCHDOG_ungraceful_release(%u)\n", handle);
    if(handle < 1 || handle > MAX_WDT_APPLS) {
        DBG(KERN_ERR "AVM_WATCHDOG_ungraceful_release(hdl=%u): invalid handle\n", handle);
        return;
    }
    handle--;
    if (ar7wdt_data.mask & (1 << handle)) {
#ifdef CONFIG_PRINTK
        printk(KERN_EMERG "AVM_WATCHDOG_ungraceful_release: handle %u (%s) still registered!\n", handle + 1, ar7wdt_data.appl[handle].Name);
#endif /*--- #ifdef CONFIG_PRINTK ---*/
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_register(int handle __attribute__((unused)), char *name, int len) {
    unsigned long flags;
    int i;
    DBG(KERN_INFO "%s(%s): start\n", __func__, name);

    if(ar7wdt_no_reboot >= 2) {
        return -EINVAL; /*--- inval argument ---*/
    }
    len = AVM_WATCHDOG_strip_name(&name, len);

    for( i = 1 ; i < MAX_WDT_APPLS ; i++) {
        spin_lock_irqsave(&ar7_wdt_lock, flags);
        if(ar7wdt_data.mask & (1 << i)) {
            if(strcmp(ar7wdt_data.appl[i].Name, name)) {
                spin_unlock_irqrestore(&ar7_wdt_lock, flags);
                continue;
            }
            spin_unlock_irqrestore(&ar7_wdt_lock, flags);
            DBG(KERN_INFO "%s(%s): already registered (accept) handle=%u\n", __func__, name, i + 1);
            return i + 1;
        }
        spin_unlock_irqrestore(&ar7_wdt_lock, flags);
    }
    DBG(KERN_INFO "%s(%s): not yet registered\n", __func__, name);

    for( i = 1 ; i < MAX_WDT_APPLS ; i++) {
        spin_lock_irqsave(&ar7_wdt_lock, flags);
        if(ar7wdt_data.mask & (1 << i)) {
            spin_unlock_irqrestore(&ar7_wdt_lock, flags);
            continue;
        }
        ar7wdt_data.mask      |=  (1 << i);
        ar7wdt_data.triggered &= ~(1 << i);
        ar7wdt_data.requested |=  (1 << i);
        spin_unlock_irqrestore(&ar7_wdt_lock, flags);

	    init_timer(&(ar7wdt_data.appl[i].Timer));

        ar7wdt_data.appl[i].req_jiffies = jiffies;
        ar7wdt_data.appl[i].avg_trigger = 0;
        ar7wdt_data.appl[i].pagefaults  = get_act_pagefaults();

        init_waitqueue_head(&(ar7wdt_data.appl[i].wait_queue));
        ar7wdt_data.appl[i].fasync = NULL;
        ar7wdt_data.appl[i].default_time = WDT_DEFAULT_TIME;
        ar7wdt_data.appl[i].Timer.function = AVM_WATCHDOG_timer_handler;
        ar7wdt_data.appl[i].Timer.data     = i + 1; /*--- handle ---*/
        strncpy(ar7wdt_data.appl[i].Name, name, len);
        ar7wdt_data.appl[i].Name[len] = '\0';

        _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER, i);

        DBG(KERN_INFO "%s(%s): handle=%u mask=0x%08x\n", __func__, name, i + 1, ar7wdt_data.mask);

        return i + 1;
    }
    DBG(KERN_ERR "%s(%s): not registered, too many appls\n", __func__, name);
    return -EUSERS; /*--- to many users ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct fasync_struct **AVM_WATCHDOG_get_fasync_ptr(int handle) {
    if(handle < 1 || handle > MAX_WDT_APPLS) {
        DBG(KERN_ERR "AVM_WATCHDOG_get_fasync_ptr(hdl=%u): invalid handle\n", handle);
        return NULL; /*--- io error ---*/
    }
    handle--;
    if((ar7wdt_data.mask & (1 << handle)) == 0) {
        DBG(KERN_ERR "AVM_WATCHDOG_get_fasync_ptr(hdl=%u): invalid handle (not set in mask)\n", handle + 1);
        return NULL; /*--- io error ---*/
    }
    return &(ar7wdt_data.appl[handle].fasync);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
wait_queue_head_t *AVM_WATCHDOG_get_wait_queue(int handle) {
    if(handle < 1 || handle > MAX_WDT_APPLS) {
        DBG(KERN_ERR "AVM_WATCHDOG_get_wait_queue(hdl=%u): invalid handle\n", handle);
        return NULL; /*--- io error ---*/
    }
    handle--;
    if((ar7wdt_data.mask & (1 << handle)) == 0) {
        DBG(KERN_ERR "AVM_WATCHDOG_get_wait_queue(hdl=%u): invalid handle (not set in mask)\n", handle + 1);
        return NULL; /*--- io error ---*/
    }
    return &(ar7wdt_data.appl[handle].wait_queue);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_poll(int handle) {
    if(handle < 1 || handle > MAX_WDT_APPLS) {
        DBG(KERN_ERR "AVM_WATCHDOG_poll(hdl=%u): invalid handle\n", handle);
        return -EINVAL; /*--- inval argument ---*/
    }
    handle--;
    if((ar7wdt_data.mask & (1 << handle)) == 0) {
        DBG(KERN_ERR "AVM_WATCHDOG_poll(hdl=%u): invalid handle (not set in mask)\n", handle + 1);
        return -EINVAL; /*--- inval argument ---*/
    }
    return (ar7wdt_data.requested & (1 << handle)) ? 1 : 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_release(int handle, char *name, int len) {
    unsigned long flags;
    DBG(KERN_INFO "%s(hdl=%u): name='%s', len=%u\n", __func__, handle, name, len);
    len = AVM_WATCHDOG_strip_name(&name, len);

    if(handle < 1 || handle > MAX_WDT_APPLS) {
        DBG(KERN_ERR "%s(hdl=%u): invalid handle\n", __func__, handle);
        return -EINVAL; /*--- inval argument ---*/
    }
    handle--;
    if(len) {
        if(strcmp(name, ar7wdt_data.appl[handle].Name)) {
            DBG(KERN_INFO "AVM_WATCHDOG_release(hdl=%u): name='%s', len=%u (name and handle to not correspond)\n", handle, name, len);
            handle = AVM_WATCHDOG_find_handle(name, len);
            if(handle == 0) {
                DBG(KERN_ERR "%s(hdl=%u): invalid name\n", __func__, handle + 1);
                return -EINVAL; /*--- inval argument ---*/
            }
            handle--;
        }
    }
    spin_lock_irqsave(&ar7_wdt_lock, flags);
    if((ar7wdt_data.mask & (1 << handle)) == 0) {
        spin_unlock_irqrestore(&ar7_wdt_lock, flags);
        DBG(KERN_ERR "%s(hdl=%u): invalid handle (not set in mask)\n", __func__, handle + 1);
        return -EINVAL; /*--- inval argument ---*/
    }
    ar7wdt_data.mask      &= ~(1 << handle);
    ar7wdt_data.requested &= ~(1 << handle);
    ar7wdt_data.states    &= ~(1 << handle);
    ar7wdt_data.triggered &= ~(1 << handle);
    spin_unlock_irqrestore(&ar7_wdt_lock, flags);

    /*--- timer stoppen ---*/
    _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, handle);

    DBG(KERN_INFO "%s(hdl=%u): success\n", __func__, handle + 1);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_set_timeout(int handle, char *time, int len __attribute__((unused))) {
    unsigned long flags;
    if(handle < 1 || handle > MAX_WDT_APPLS) {
        DBG(KERN_ERR "%s(hdl=%u): invalid handle\n", __func__, handle);
        return -EINVAL; /*--- inval argument ---*/
    }
    handle--;
    spin_lock_irqsave(&ar7_wdt_lock, flags);
    if((ar7wdt_data.mask & (1 << handle)) == 0) {
        spin_unlock_irqrestore(&ar7_wdt_lock, flags);
        DBG(KERN_ERR "%s(hdl=%u): invalid handle\n", __func__, handle + 1);
        return -EINVAL; /*--- inval argument ---*/
    }
    ar7wdt_data.appl[handle].default_time = _AVM_WATCHDOG_atoi(time) / 2; 
    spin_unlock_irqrestore(&ar7_wdt_lock, flags);

    _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER | AVM_WATCHDOG_DEL_TIMER, handle);
    DBG(KERN_INFO "%s(hdl=%u): success (new timeout=%u)\n", __func__, handle + 1, ar7wdt_data.appl[handle].default_time);
    return handle + 1;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned long get_act_pagefaults(void) {
    unsigned long page_faults = 0;
#ifdef CONFIG_VM_EVENT_COUNTERS
   struct vm_event_state new;
   all_vm_events((unsigned long *)&new);
   page_faults = new.event[PGFAULT];
#endif/*--- #ifdef CONFIG_VM_EVENT_COUNTERS ---*/
   return page_faults;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_trigger(int handle, char *time __attribute__((unused)), int len __attribute__((unused))) {
    unsigned long flags;

    if(ar7wdt_no_reboot >= 2) {
        return -EINVAL; /*--- inval argument ---*/
    }
    if(handle < 1 || handle > MAX_WDT_APPLS) {
        DBG(KERN_ERR "%s(hdl=%u): invalid handle\n", __func__, handle);
        return -EINVAL; /*--- inval argument ---*/
    }
    handle--;

    spin_lock_irqsave(&ar7_wdt_lock, flags);
    if((ar7wdt_data.mask & (1 << handle)) == 0) {
        spin_unlock_irqrestore(&ar7_wdt_lock, flags);
        DBG(KERN_ERR "%s(hdl=%u): invalid handle (not set in mask)\n", __func__, handle + 1);
        return -EINVAL; /*--- inval argument ---*/
    }
    ar7wdt_data.triggered |=  (1 << handle);
    ar7wdt_data.states    &= ~(1 << handle);
    spin_unlock_irqrestore(&ar7_wdt_lock, flags);

    if(likely(ar7wdt_data.appl[handle].avg_trigger)) {
        ar7wdt_data.appl[handle].avg_trigger += ((signed long)((jiffies - ar7wdt_data.appl[handle].req_jiffies) - ar7wdt_data.appl[handle].avg_trigger)) >> 3;
    } else {
        ar7wdt_data.appl[handle].avg_trigger = (jiffies - ar7wdt_data.appl[handle].req_jiffies);
    }
    ar7wdt_data.appl[handle].req_jiffies = jiffies;
    ar7wdt_data.appl[handle].pagefaults  = get_act_pagefaults();
    /*--- timer neu aufsetzen ---*/
    _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER | AVM_WATCHDOG_SET_TIMER, handle);
    AVM_WATCHDOG_check_all_triggered();

#if defined(WATCHDOG_LIST_TASK_STATISTIC)
    show_task_statistic();
#endif/*--- #if defined(WATCHDOG_LIST_TASK_STATISTIC) ---*/
    DBG_TRIGGER(KERN_ERR "%s(hdl=%u): %s avg_trigger = %lu success\n", __func__, handle + 1, ar7wdt_data.appl[handle].Name, ar7wdt_data.appl[handle].avg_trigger);
    return handle + 1;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_disable(int handle __attribute__((unused)), char *time __attribute__((unused)), int len __attribute__((unused))) {
    unsigned long flags;
    int i;
    printk(KERN_INFO "%s()\n", __func__);
    printk(KERN_INFO "registered appls:\n");
    for (i = 0; i < MAX_WDT_APPLS; i++) {
        spin_lock_irqsave(&ar7_wdt_lock, flags);
        if((ar7wdt_data.mask & (1 << i))) {
            ar7wdt_data.mask      &= ~(1 << i);
            ar7wdt_data.requested &= ~(1 << i);
            ar7wdt_data.states    &= ~(1 << i);
            ar7wdt_data.triggered &= ~(1 << i);
            spin_unlock_irqrestore(&ar7_wdt_lock, flags);
            _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, i);
            printk(KERN_INFO "  hdl=%u, name=%s, disabled.\n", i + 1, ar7wdt_data.appl[i].Name);
        } else {
            spin_unlock_irqrestore(&ar7_wdt_lock, flags);
        }
    }
    _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, -1);
    ar7wdt_hw_deinit();
    ar7wdt_no_reboot = 3;
    return 1; /*--- sonst Endlosschleife, weil private_data == 0 ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_read(int handle, char *Buffer, int max_len) {
    unsigned long flags;
    if(handle < 1 || handle > MAX_WDT_APPLS) {
        DBG(KERN_ERR "%s(hdl=%u): invalid handle\n", __func__, handle);
        return -EINVAL; /*--- inval argument ---*/
    }
    handle--;
    spin_lock_irqsave(&ar7_wdt_lock, flags);
    if((ar7wdt_data.mask & (1 << handle)) == 0) {
        spin_unlock_irqrestore(&ar7_wdt_lock, flags);
        DBG(KERN_ERR "%s(hdl=%u): invalid handle (not set in mask)\n", __func__, handle + 1);
        return -EINVAL; /*--- inval argument ---*/
    }
    if((ar7wdt_data.requested & (1 << handle)) == 0) {
        spin_unlock_irqrestore(&ar7_wdt_lock, flags);
        Buffer[0] = '\0';
        DBG(KERN_INFO "%s(hdl=%u): no request\n", __func__, handle + 1);
        return handle + 1;
    }
    ar7wdt_data.requested &= ~(1 << handle);
    spin_unlock_irqrestore(&ar7_wdt_lock, flags);
    max_len = min(max_len - 1, (int)sizeof("alive ?"));
    strncpy(Buffer, "alive ?", max_len);
    Buffer[max_len] = '\0';
    DBG(KERN_INFO "%s(hdl=%u): request='%s'\n", __func__, handle + 1, Buffer);
    return handle + 1;
}

/*--------------------------------------------------------------------------------*\
 * mode = 0: in jiffies
 * sonst in usec (aber liefere msec)
\*--------------------------------------------------------------------------------*/
static char *human_timediff(char *buf, long timediff, int mode) {
    if(mode == 0) {
        if(timediff >= 0) { 
            long msec = (timediff * 1000) / HZ;
            sprintf(buf, "%3lu.%03lu s", msec / 1000, msec % 1000);
        } else {
            strcpy(buf, "never");
        }
    } else if(mode == 1) {
        sprintf(buf, "%10lu.%03lu ms", timediff / 1000, (timediff % 1000));
    } else {
        sprintf(buf, "%10lu.%03lu us", timediff / 1000, (timediff % 1000));
    }
    return buf;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
#define TASK_STATE_TO_CHAR_STR "RSDTtZX"
#endif/*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned char task_state(unsigned int state) {
    static const char stat_nam[] = TASK_STATE_TO_CHAR_STR;
	return state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?';
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int cmp_wdt_name(char *name) {
    int i;
    for (i = 0; i < MAX_WDT_APPLS; i++) {
        if((ar7wdt_data.mask & (1 << i))) {
            if(strcmp(name, ar7wdt_data.appl[i].Name) == 0) {
                return i + 1;
            }
        }
    }
    return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void show_page_statistic(int now __attribute__((unused))) {
#ifdef CONFIG_VM_EVENT_COUNTERS
    static int lastjiffies;
    int tdiff =  (jiffies - lastjiffies);
    if(now || (tdiff > 10 * HZ)) {
        int i;
        static struct vm_event_state old;
        struct vm_event_state new;
        lastjiffies = jiffies;

        all_vm_events((unsigned long *)&new);
        for(i = 0; i < NR_VM_EVENT_ITEMS; i++){ 
            old.event[i] = new.event[i] - old.event[i];
        }
        tdiff /= HZ;
        if(tdiff == 0) tdiff = 1;
        printk(KERN_ERR"PGIN %lu(%lu) PGOUT %lu(%lu) PGFAULT %lu(%lu) SWPIN %lu(%lu) SWPOUT %lu(%lu) PGREFILL %lu(%lu)\n", 
                new.event[PGPGIN],  old.event[PGPGIN] / tdiff,
                new.event[PGPGOUT], old.event[PGPGOUT] / tdiff,
                new.event[PGFAULT], old.event[PGFAULT] / tdiff,
                new.event[PSWPIN],  old.event[PSWPIN] / tdiff,
                new.event[PSWPOUT], old.event[PSWPOUT] / tdiff,
                new.event[PGREFILL_NORMAL], old.event[PGREFILL_NORMAL] / tdiff
            );
            memcpy(&old, &new, sizeof(old));
        }
#endif/*--- #ifdef CONFIG_VM_EVENT_COUNTERS ---*/
}
#if defined(WATCHDOG_LIST_TASK_STATISTIC)
static struct _hist_task_times {
    unsigned long stime_act;
    unsigned long utime_act;
    unsigned long stime_last;
    unsigned long utime_last;
    unsigned int pid;
    char         name[13];
    unsigned int mark;
} hist_task_times[120];

#define ARRAY_EL(a) (sizeof(a) / sizeof(a[0]))
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _hist_task_times *get_histtask_by_pid(unsigned int pid, char *name) {
    static unsigned int last;
    unsigned int i;
    for(i = last; i < ARRAY_EL(hist_task_times); i++) {
        if(hist_task_times[i].pid == pid) {
            last = i;
            hist_task_times[i].mark = 2;
            return &hist_task_times[i];
        }
    }
    for(i = 0; i < last; i++) {
        if(hist_task_times[i].pid == pid) {
            last = i;
            hist_task_times[i].mark = 2;
            return &hist_task_times[i];
        }
    }
    /*--- Create ---*/
    for(i = 0; i < ARRAY_EL(hist_task_times); i++) {
        if(hist_task_times[i].mark == 0) {
            last = i;
            hist_task_times[i].utime_last = 0;
            hist_task_times[i].stime_last = 0;
            hist_task_times[i].mark = 2;
            hist_task_times[i].pid  = pid;
            strncpy(hist_task_times[i].name, name, sizeof(hist_task_times[i].name));
            return &hist_task_times[i];
        } 

    }
    printk("<error pid %d>", pid);
    return NULL;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void show_task_statistic(void) {
	struct task_struct *g, *p; 
    /*--- Summiere alle utime/stime ermittle maxrun ---*/
    static unsigned long lastjiffies;
    unsigned long flags;
    unsigned long utime_sum = 0, stime_sum = 0;
    unsigned int i;

    unsigned long tdiff =  (jiffies - lastjiffies);
    if(tdiff < 20 * HZ) {
        return;
    }
    lastjiffies = jiffies;

	read_lock_irqsave(&tasklist_lock, flags);

	do_each_thread(g, p) {
        struct _hist_task_times *pht = get_histtask_by_pid(p->pid, p->comm);
        if(pht) {
            pht->stime_act = task_stime(p);
            pht->utime_act = task_utime(p);

            utime_sum += pht->utime_act - pht->utime_last;
            stime_sum += pht->stime_act - pht->stime_last;
        }
	} while_each_thread(g, p);

	read_unlock_irqrestore(&tasklist_lock, flags);

    printk(KERN_EMERG "[%lu]%s: sum utime=%lu stime=%lu %lu (%lu %% from system - measure time %lu s)\n", 
            jiffies, __func__, 
            utime_sum, stime_sum, (utime_sum + stime_sum), ((utime_sum + stime_sum) * 100 ) / tdiff, tdiff / HZ);

    utime_sum |= 1;
    stime_sum |= 1;

    for(i = 0; i < ARRAY_EL(hist_task_times); i++) {
        struct _hist_task_times *pht = &hist_task_times[i];
        if(pht->mark == 2) {
            if(((pht->stime_act - pht->stime_last) * 100 > (unsigned long)stime_sum) || 
               ((pht->utime_act - pht->utime_last) * 100 > (unsigned long)utime_sum)) {
                printk(KERN_EMERG "name: %-13s pid %4d s/u %3lu %%%% %3lu %%%%)\n", 
                    pht->name, pht->pid,
                    ((pht->stime_act - pht->stime_last) * 1000) / (unsigned long)stime_sum,
                    ((pht->utime_act - pht->utime_last) * 1000) / (unsigned long)utime_sum);
            }
            pht->stime_last = pht->stime_act;
            pht->utime_last = pht->utime_act;
        }
        if(pht->mark) pht->mark--;
    }
}
#endif/*--- #if defined(WATCHDOG_LIST_TASK_STATISTIC) ---*/

volatile struct task_struct *hungingtask = NULL;
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct task_struct *watchdog_task_list(int notrigger_handle) {
    char txtbuf[2][64];
	struct task_struct *g, *p, *parent, *hungtask = NULL;
	unsigned long flags, page_faults;
    int handle, delayed = 0;
    unsigned int handle_mask = 0;
    int (*saved_printk)(const char *format, ...);
	read_lock_irqsave(&tasklist_lock, flags);

    saved_printk = printk;
    set_printk(__printk); /*--- folgende Ausgaben sofort ausgeben: ---*/
    /*--- printk("utime_sum: %lld unorm %d\n", utime_sum, unorm); ---*/
    printk(KERN_EMERG "[%lu]AVM_WATCHDOG_reboot(hdl=%u, name=%s): reboot (current: %s pgfault %lu)\n", 
                                    jiffies,
                                    notrigger_handle + 1, ar7wdt_data.appl[notrigger_handle].Name, 
                                    current->comm,
                                    current->signal->cmaj_flt
            );
    page_faults =  get_act_pagefaults();
    printk(KERN_EMERG "pagefaults absolut %lu since last %s-trigger %lu\n",  page_faults, ar7wdt_data.appl[notrigger_handle].Name, page_faults - ar7wdt_data.appl[notrigger_handle].pagefaults);
	do_each_thread(g, p) {
        if((handle = cmp_wdt_name(p->comm)) == 0) {
#if 0
            printk(KERN_EMERG "name=%-13s pid %4d state: %c pgfault %lu\n", p->comm, p->pid, 
                                                                                     task_state(p->state),
                                                              current->signal->cmaj_flt
                                                         );
#endif
			continue;
        }
        handle--;
        handle_mask |= (1 << handle); 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
        printk(KERN_EMERG "  hdl=%2u, name=%-13s pid %4d last-delta: trigger %s(avg %s) state: %c pgfault %lu\n", handle + 1, 
                            ar7wdt_data.appl[handle].Name, p->pid,
                            human_timediff(txtbuf[0], jiffies - ar7wdt_data.appl[handle].req_jiffies, 0),
                            human_timediff(txtbuf[1], ar7wdt_data.appl[handle].avg_trigger, 0),
                            task_state(p->state),
                            p->signal->cmaj_flt
                            );
#else/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32) ---*/
        printk(KERN_EMERG "  hdl=%2u, name=%-13s pid %4d last-delta: trigger %s(avg: %s) state: %c\n", handle + 1, 
                            ar7wdt_data.appl[handle].Name, p->pid,
                            human_timediff(txtbuf[0], jiffies - ar7wdt_data.appl[handle].req_jiffies, 0),
                            human_timediff(txtbuf[1], ar7wdt_data.appl[handle].avg_trigger, 0),
                            task_state(p->state));
#endif/*--- #else ---*//*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32) ---*/
        parent     = p;
        /*--- parent dieser Applikation ermitteln: ---*/
        while(parent->real_parent && (parent != parent->real_parent) && (strcmp(parent->real_parent->comm, parent->comm) == 0)) {
            /*--- printk(KERN_EMERG "  parent: %s pid %4d\n", parent->real_parent->comm, parent->real_parent->pid); ---*/
            parent = parent->real_parent;
        }
        if((notrigger_handle == handle) && (delayed == 0)) {
            delayed = 1;
            if((p->state == TASK_UNINTERRUPTIBLE) &&
               (parent->state & TASK_INTERRUPTIBLE)) {
                /*--------------------------------------------------------------------------------*\
                falls parent nicht TASK_INTERRUPTIBLE, jedoch aktueller Thread TASK_UNINTERRUPTIBLE:
                dann diesen statt parent abschiessen 
                \*--------------------------------------------------------------------------------*/
                parent = p;
            }
            printk(KERN_EMERG " force SIGBUS for %s (pid= %d)\n", parent->comm, parent->pid);
            hungtask = parent;
        }
	} while_each_thread(g, p);

    for(handle = 0; handle < MAX_WDT_APPLS; handle++) {
        if((ar7wdt_data.mask & (1 << handle))) {
            if((handle_mask & (1 << handle)) == 0) {
                printk(KERN_EMERG "  hdl=%2u, name=%-13s trigger-delta: %s (avg: %s) maybe crashed\n", handle + 1, ar7wdt_data.appl[handle].Name,
                                     human_timediff(txtbuf[0], jiffies - ar7wdt_data.appl[handle].req_jiffies, 0),
                                     human_timediff(txtbuf[1], ar7wdt_data.appl[handle].avg_trigger, 0));
            }
        }
    }
    if(hungtask) {
        printk(KERN_EMERG "ar7wdt_hw_reboot: kernel context for %s (pid= %d):\n", hungtask->comm, hungtask->pid);
        sched_show_task(hungtask);
    }
    printk(KERN_EMERG "ar7wdt_hw_reboot: kernel context for all blocked tasks:\n");
	show_state_filter(TASK_UNINTERRUPTIBLE);
    set_printk(saved_printk); /*--- ... und zurueck, um restore_printk() zu  ermoeglichen ---*/
	read_unlock_irqrestore(&tasklist_lock, flags);
    ar7wdt_hw_trigger();
    return hungtask;
}
/*--------------------------------------------------------------------------------*\
 * 2 stufig:
 * context != 0: hartes wakeup des haengenden Tasks (auch wenn TASK_UNINTERRUPTIBLE)
 * context  == 0: panic
\*--------------------------------------------------------------------------------*/
static void panic_function(unsigned long context) {
    ar7wdt_hw_trigger();
    restore_printk();
    if(context) {
        struct task_struct *hungtask = (struct task_struct *)context; 
        siginfo_t info;

        info.si_signo = SIGBUS;
        info.si_errno = ETIME;
        info.si_code  = BUS_OBJERR;
        info.si_addr  = (void *)0xEBADEBAD;

        force_sig_info(SIGBUS, &info, hungtask);
        if(hungtask->state == TASK_UNINTERRUPTIBLE) {
            printk(KERN_EMERG "ar7wdt_hw_reboot: violent wake up task %s (pid= %d):\n", hungtask->comm, hungtask->pid);
            hungingtask = hungtask; /*--- in sched.h bekanntmachen ---*/
            wake_up_process(hungtask);
        }
        /*--- und nun noch panic verzoegert triggern ---*/
        PanicTimer.data     = 0;
        PanicTimer.function = panic_function;
        PanicTimer.expires  = jiffies + HZ * 5;
        add_timer(&PanicTimer);
    } else {
        panic("ar7wdt_hw_reboot: delayed watchdog expired\n");
    }
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int AVM_WATCHDOG_reboot(int handle) {
    if(handle < 1 || handle > MAX_WDT_APPLS) {
        DBG(KERN_ERR "AVM_WATCHDOG_reboot(hdl=%u): invalid handle\n", handle);
        return -EINVAL; /*--- inval argument ---*/
    }
    handle--;
    if((ar7wdt_data.mask & (1 << handle)) == 0) {
        DBG(KERN_ERR "AVM_WATCHDOG_reboot(hdl=%u): invalid handle (not set in mask)\n", handle + 1);
        return -EINVAL; /*--- inval argument ---*/
    }
    if(ar7wdt_data.states & (1 << handle)) {
        struct task_struct *hungtask = watchdog_task_list(handle);
#ifdef CONFIG_SCHEDSTATS
        if (ar7wdt_no_reboot == 1) {
#if KERNEL_VERSION(2, 6, 19) >= LINUX_VERSION_CODE
            show_sched_stats();
#endif
            show_state();
        }
#endif
#if defined(CONFIG_PROC_FS)
#if defined(CONFIG_MIPS_OHIO) && defined(CONFIG_SCHEDSTATS)
        show_process_eip(ar7wdt_data.appl[handle].Name);
#endif
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
        if(ar7wdt_no_reboot == 0) {
#if defined(CONFIG_ARCH_DAVINCI)    /*--- erstmal den Handle ausblinken und nicht rebooten ---*/
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/avm_led.h>
            {

                extern unsigned int davinci_revision[5];
                int tmp, led_handle, appl_handle = 0, led_on, led_off;
                unsigned long next_jiffies;
                if(davinci_revision[1] == 1) {
                    led_handle = avm_led_alloc_handle("error", 1, NULL);
                    led_on = 0;
                    led_off = 1;
                } else {
                    led_handle = avm_led_alloc_handle("error", 0, NULL);
                    led_on = 1;
                    led_off = 0;
                }

                if (!strcmp(ar7wdt_data.appl[handle].Name, "galio"))
                    appl_handle = 1;
                if (!strcmp(ar7wdt_data.appl[handle].Name, "mediasrv"))
                    appl_handle = 2;
                if (!appl_handle)
                    appl_handle = 3;

                if (led_handle) {
                    avm_led_action_with_handle(led_handle, led_off);
                    while (1) {
                        tmp = appl_handle;
                        next_jiffies = jiffies + msecs_to_jiffies(1000);
                        while (time_after(next_jiffies, jiffies))
                            ;

                        while(tmp--) {
                            avm_led_action_with_handle(led_handle, led_on);
                            next_jiffies = jiffies + msecs_to_jiffies(1000);
                            while (time_after(next_jiffies, jiffies))
                                ;
                            avm_led_action_with_handle(led_handle, led_off);
                            next_jiffies = jiffies + msecs_to_jiffies(1000);
                            while (time_after(next_jiffies, jiffies))
                                ;
                        }
                    }
                } 
            }
#endif       
            init_timer(&PanicTimer);
            PanicTimer.data     = (unsigned long)hungtask;
            PanicTimer.function = panic_function;
            PanicTimer.expires  = hungtask ? (jiffies + HZ * 1) : (jiffies + HZ * 5);
            add_timer(&PanicTimer);
            return 0;
        }
    }
    printk(KERN_ERR "AVM_WATCHDOG_reboot(hdl=%u): timer not triggered\n", handle + 1);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void AVM_WATCHDOG_check_all_triggered(void) {
    int i;
    unsigned int mask    = 0;
    unsigned int trigger = 0;
    unsigned long flags;

    spin_lock_irqsave(&ar7_wdt_lock, flags);
    for( i = 0 ; i < MAX_WDT_APPLS ; i++) {
        if(ar7wdt_data.mask & (1 << i)) {
            mask |= 1 << i;
            if(ar7wdt_data.triggered & (1 << i)) {
                trigger |= 1 << i;
            }
        }
    }
    if(mask == trigger) {
        ar7wdt_data.triggered = 0;
        spin_unlock_irqrestore(&ar7_wdt_lock, flags);
        ar7wdt_hw_trigger();
    } else {
        spin_unlock_irqrestore(&ar7_wdt_lock, flags);
        DBG(KERN_INFO "%s(): not triggered mask 0x%x\n", __func__, mask ^ trigger);
    }
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void AVM_WATCHDOG_timer_handler_action(unsigned int handle) {
    unsigned long flags;
    spin_lock_irqsave(&ar7_wdt_lock, flags);
    ar7wdt_data.states    |= 1 << handle;
    ar7wdt_data.requested |= 1 << handle;
    spin_unlock_irqrestore(&ar7_wdt_lock, flags);

    if(ar7wdt_data.appl[handle].fasync) {
        kill_fasync(&(ar7wdt_data.appl[handle].fasync), SIGIO, POLL_IN);
    } else {
        wake_up_interruptible(&(ar7wdt_data.appl[handle].wait_queue));
    }
    _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER, handle);
    DBG_TRIGGER(KERN_INFO "%s(hdl=%u): timer triggered once\n", __func__, handle + 1);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void AVM_WATCHDOG_timer_handler(unsigned long _handle) {
    unsigned long flags;
    int handle = (int)_handle;

    DBG(KERN_INFO "%s(hdl=%d)\n", __func__, handle);

    if(handle == -1) {
        _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_SET_TIMER, handle);
        ar7wdt_hw_trigger();
        return;
    }
    if(handle < 1 || handle > MAX_WDT_APPLS) {
        DBG(KERN_ERR "%s(hdl=%u): invalid handle\n", __func__, handle);
        return;
    }
    handle--;

    spin_lock_irqsave(&ar7_wdt_lock, flags);
    if((ar7wdt_data.mask & (1 << handle)) == 0) {
        spin_unlock_irqrestore(&ar7_wdt_lock, flags);
        DBG(KERN_ERR "%s(hdl=%u): invalid handle\n", __func__, handle + 1);
        return;
    }
    if(ar7wdt_data.states & (1 << handle)) {
        spin_unlock_irqrestore(&ar7_wdt_lock, flags);
        DBG(KERN_INFO "%s(hdl=%u): timer triggered twice (reboot)\n", __func__, handle + 1);
        AVM_WATCHDOG_reboot(handle + 1);
        return;
    }
    spin_unlock_irqrestore(&ar7_wdt_lock, flags);
    _AVM_WATCHDOG_ctrl_timer(AVM_WATCHDOG_DEL_TIMER, handle);
    AVM_WATCHDOG_timer_handler_action(handle);
    return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void AVM_WATCHDOG_emergency_retrigger(void) {

    if ( ! ar7wdt_no_reboot) {
        ar7wdt_hw_trigger();
    }
}
EXPORT_SYMBOL(AVM_WATCHDOG_emergency_retrigger);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
EXPORT_SYMBOL(AVM_WATCHDOG_register);
EXPORT_SYMBOL(AVM_WATCHDOG_release);
EXPORT_SYMBOL(AVM_WATCHDOG_set_timeout);
EXPORT_SYMBOL(AVM_WATCHDOG_trigger);
EXPORT_SYMBOL(AVM_WATCHDOG_read);
EXPORT_SYMBOL(AVM_WATCHDOG_reboot);
EXPORT_SYMBOL(AVM_WATCHDOG_poll);

EXPORT_SYMBOL(ar7wdt_no_reboot);
#endif /*--- #if defined(CONFIG_AVM_WATCHDOG) ---*/

