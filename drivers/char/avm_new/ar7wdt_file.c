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
#include <linux/fs.h>
#include <linux/device.h>
#include "avm_sammel.h"
#include "ar7wdt.h"

typedef unsigned int UINT32;
typedef int INT32;
#include <asm/mach_avm.h>
#if defined(CONFIG_MIPS)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
#include <asm/mips-boards/prom.h>
#else
#include <asm/prom.h>
#endif
#endif /*--- #if defined(CONFIG_MIPS) ---*/
#if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
#include <asm/mach-ohio/wdtimer.h>
#elif defined(CONFIG_MIPS_UR8)
#include <asm/mach-ur8/wdtimer.h>
#endif /*--- #if defined(CONFIG_MIPS) ---*/
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/fs.h>


/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
/*--- #define AR7WDT_DEBUG ---*/

#if defined(AVM_WATCHDOG_DEBUG)
#define DBG(...)  printk(KERN_INFO __VA_ARGS__)
#else /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/
#define DBG(...)  
#endif /*--- #else ---*/ /*--- #if defined(AVM_WATCHDOG_DEBUG) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 19)
#define AVM_WDT_UDEV
#endif/*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/

extern int ar7wdt_no_reboot;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _ar7wdt_cmd {
    char *cmd;
    int cmd_len;
    int (*funktion)(int, char *, int);
} ar7wdt_cmd[] = {
    { "start",      sizeof("start") - 1,      AVM_WATCHDOG_register },
    { "register",   sizeof("register") - 1,   AVM_WATCHDOG_register },
    { "release",    sizeof("release") - 1,    AVM_WATCHDOG_release },
    { "timeout",    sizeof("timeout") - 1,    AVM_WATCHDOG_set_timeout },
    { "time",       sizeof("time") - 1,       AVM_WATCHDOG_set_timeout },
    { "trigger",    sizeof("trigger") - 1,    AVM_WATCHDOG_trigger },
    { "disable",    sizeof("disable") - 1,    AVM_WATCHDOG_disable },
    { "init-start", sizeof("init-start") - 1, AVM_WATCHDOG_init_start },
    { "init-done",  sizeof("init-done") - 1,  AVM_WATCHDOG_init_done },
    { NULL,       0,                      NULL }
};

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int ar7wdt_open(struct inode *, struct file *);
static int ar7wdt_release(struct inode *, struct file *);
static ssize_t ar7wdt_read(struct file *, char *, size_t, loff_t *);
static ssize_t ar7wdt_write(struct file *, const char *, size_t, loff_t *);
void ar7wdt_cleanup(void);
static int ar7wdt_fasync(int fd, struct file *filp, int mode);
static unsigned int ar7wdt_poll(struct file *filp, poll_table *wait);


struct file_operations ar7wdt_fops = {
    owner:   THIS_MODULE,
    open:    ar7wdt_open,
    release: ar7wdt_release,
    read:    ar7wdt_read,
    write:   ar7wdt_write,
    fasync:  ar7wdt_fasync,
    poll:    ar7wdt_poll,
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct _avm_wdt {
	struct char_device_struct *device_region;
    dev_t                      device;
    struct cdev               *cdev;
#if defined(AVM_WDT_UDEV)
    struct class              *osclass;
#endif
} avmwdt;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
int ar7wdt_disable_watchdog(char *str) {
    switch(*str) {
        case '0': ar7wdt_no_reboot = 0; return 0;
        case '1': ar7wdt_no_reboot = 1; return 0;
        case '2': ar7wdt_no_reboot = 2; return 0;
    }
    return 1;
}

__setup("ar7wdt_no_reboot=", ar7wdt_disable_watchdog);
#endif /*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28) ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int __init ar7wdt_init(void) {
    int reason;

    printk("AVM_WATCHDOG: Watchdog Driver for AR7 Hardware (Version %s, build: %s %s)\n", "1.0", __DATE__, __TIME__);

#if defined(CONFIG_AVM_SAMMEL) && defined(CONFIG_MIPS) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
    if(ar7wdt_no_reboot == 0) {
        char *argptr;
        argptr = prom_getcmdline();
        /*--- __printk("kernel cmdline = %s\n", argptr); ---*/
        if(argptr) {
            argptr = strstr(argptr, "ar7wdt_no_reboot=");
            /*--- __printk("kernel: \"%s\"\n", argptr); ---*/
        }
        if(argptr) {
            /*--- __printk("kernel: switch: \"%c\"\n", argptr[sizeof("ar7wdt_no_reboot=") - 1]); ---*/
            switch(argptr[sizeof("ar7wdt_no_reboot=") - 1]) {
                default:
                case '0': break;
                case '1': ar7wdt_no_reboot = 1;
                          break;
                case '2': ar7wdt_no_reboot = 2;
                          break;
            }
        }
        /*--- __printk("kernel: ar7wdt_no_reboot = %u\n", ar7wdt_no_reboot ); ---*/
    }
#endif /*--- #if defined(CONFIG_AVM_SAMMEL) ---*/
    if(ar7wdt_no_reboot == 2) {
        printk("watchdog disabled\n");
        return 0;
    }
    if(ar7wdt_no_reboot) {
        printk("panic reboot disabled\n");
    }

    DBG(KERN_INFO "[avmwdt] register_chrdev_region()\n");
#if defined(AVM_WDT_UDEV)
    reason = alloc_chrdev_region(&avmwdt.device, 0, 1, "watchdog");
#else /*--- #if defined(AVM_WDT_UDEV) ---*/
    avmwdt.device = MKDEV(WATCHDOG_MAJOR,0);
    reason = register_chrdev_region(avmwdt.device, 1, "watchdog");
#endif
    if(reason) {
        DBG(KERN_ERR "[avmwdt] register_chrdev_region()\n");
        printk("[avmwdt] register_chrdev_region failed: reason %d!\n", reason);
        return -ERESTARTSYS;
    }

    avmwdt.cdev = cdev_alloc();
    if (!avmwdt.cdev) {
        unregister_chrdev_region(avmwdt.device, 1);
        printk("[avmwdt] cdev_alloc failed!\n");
        return -ERESTARTSYS;
    }

    avmwdt.cdev->owner = ar7wdt_fops.owner;
    avmwdt.cdev->ops = &ar7wdt_fops;
    kobject_set_name(&(avmwdt.cdev->kobj), "watchdog");

    if (cdev_add(avmwdt.cdev, avmwdt.device, 1)) { 
        kobject_put(&avmwdt.cdev->kobj);
        unregister_chrdev_region(avmwdt.device, 1);
        printk("[avmwdt] cdev_add failed!\n");
        return -ERESTARTSYS;
    }
#if defined(AVM_WDT_UDEV)
    /*--- Geraetedatei anlegen: ---*/
    avmwdt.osclass = class_create(THIS_MODULE, "watchdog");
    device_create(avmwdt.osclass, NULL, 1, NULL, "%s%d", "watchdog", 0);
#endif/*--- #if defined(AVM_WDT_UDEV) ---*/

    AVM_WATCHDOG_init();
    ar7wdt_hw_init();
    ar7wdt_hw_trigger();
    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#if defined(CONFIG_AVM_WATCHDOG_MODULE)
void ar7wdt_cleanup(void) {
    if(ar7wdt_no_reboot == 2) {
        printk("watchdog was disabled\n");
        return;
    }
    AVM_WATCHDOG_deinit();
    if(avmwdt.cdev) {
#if defined(AVM_WDT_UDEV)
        device_destroy(avmwdt.osclass, 1);
        class_destroy(avmwdt.osclass);
#endif/*--- #if defined(AVM_WDT_UDEV) ---*/
        cdev_del(avmwdt.cdev); /* Delete char device */
        unregister_chrdev_region(avmwdt.device, 1);
    }
    ar7wdt_hw_deinit();
    return;
}
#endif /*--- #if defined(CONFIG_AVM_WATCHDOG_MODULE) ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int ar7wdt_fasync(int fd, struct file *filp, int mode) {
    struct fasync_struct **fasync;

    DBG(KERN_INFO "ar7wdt_fasync: fd=%u\n", fd);
    fasync = AVM_WATCHDOG_get_fasync_ptr((int)filp->private_data);
    if(fasync)
        return fasync_helper(fd, filp, mode, fasync);
    return -EPERM;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static unsigned int ar7wdt_poll(struct file *filp, poll_table *wait) {
	wait_queue_head_t *wait_queue;
    int poll;

	wait_queue = AVM_WATCHDOG_get_wait_queue((int)filp->private_data);
    if (wait_queue == NULL) return 0;

	poll_wait (filp, wait_queue, wait);

	poll = AVM_WATCHDOG_poll((int)filp->private_data);
    if(poll > 0) {
        /*--- DBG(KERN_INFO "ar7wdt_poll: data avail\n"); ---*/
	    return POLLIN | POLLRDNORM;
    }

    /*--- DBG(KERN_INFO "ar7wdt_poll: no data\n"); ---*/
    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int ar7wdt_open(struct inode *inode __attribute__((unused)), struct file *filp) {
    DBG(KERN_INFO "ar7wdt_open: always success\n");
    filp->private_data = 0;
    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int ar7wdt_release(struct inode *inode __attribute__((unused)), struct file *filp) {
    DBG(KERN_INFO "ar7wdt_release: always success\n");
    if(filp->private_data)
    {
        AVM_WATCHDOG_ungraceful_release((int)filp->private_data);
        /*--- AVM_WATCHDOG_reboot((int)filp->private_data); ---*/
        return 0;
    }
    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static ssize_t ar7wdt_read(struct file *filp, char *read_buffer, size_t max_read_length, loff_t *offp) {
    char Buffer[64];
    int len;

    if ((int)filp->private_data == 0) return -EINVAL;

    /*-------------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------------*/
ar7wdt_read_again:
    DBG(KERN_INFO "ar7wdt_read:\n");

    (void)AVM_WATCHDOG_read((int)filp->private_data, Buffer, sizeof(Buffer));
    len = strlen(Buffer);
    if(len == 0) {
	    wait_queue_head_t *wait_queue;
        if(filp->f_flags & O_NONBLOCK) {
            DBG(" empty\n");
            return -EAGAIN;
        }

        DBG(" sleep on 'wait_queue'\n");
	    wait_queue = AVM_WATCHDOG_get_wait_queue((int)filp->private_data);
        if(wait_queue) {
            interruptible_sleep_on(wait_queue);
        }

        DBG(" wake up from 'wait_queue'\n");
        goto ar7wdt_read_again;
    }
    /*-------------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------------*/

    len = min((int)max_read_length, len);

    if(len) {
        if(copy_to_user(read_buffer, Buffer, len)) {
            DBG(KERN_INFO "ar7wdt_read: copy_to_user failed len=%u\n", len);
            return (unsigned int)-EFAULT;
        }
    }
    DBG(KERN_INFO "ar7wdt_read: '%s' len=%u\n", Buffer, len);
    *offp += len;
    return len;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static ssize_t ar7wdt_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *offp) {
    char Buffer[128];
    unsigned int copy_count;
    struct _ar7wdt_cmd *C = &ar7wdt_cmd[0];

    copy_count = min(write_length, sizeof(Buffer) - 1);
    if(copy_from_user(Buffer, write_buffer, copy_count)) {
        return (unsigned int)-EFAULT;
    }
    Buffer[copy_count] = '\0';

    DBG(KERN_INFO "ar7wdt_write: '%s'\n", Buffer);
    while(C->cmd) {
        if(write_length >= (size_t)C->cmd_len && !strncmp(C->cmd, Buffer, C->cmd_len) && C->funktion) {
            DBG(KERN_INFO "ar7wdt_write: call funktion for '%s'\n", C->cmd);
            filp->private_data = (void *)C->funktion((int)filp->private_data, Buffer + C->cmd_len, write_length - C->cmd_len);
            if((int)filp->private_data < 0) {
                /*--- io error ---*/
                DBG(KERN_INFO "ar7wdt_write: error %d\n", (int)filp->private_data);
                return (int)filp->private_data;
            }
            DBG(KERN_INFO "ar7wdt_write: success\n");
            break;
        } else {
            C++;
        }
    }
    if(C->cmd == NULL) {
        /*--- io error ---*/
        DBG(KERN_INFO "ar7wdt_write: no support funktion\n");
        return -EBADRQC; /*--- invallid request code ---*/
    }
    *offp += write_length;
    return write_length;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#endif /*--- #if defined(CONFIG_AVM_WATCHDOG) ---*/

