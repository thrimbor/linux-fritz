/*------------------------------------------------------------------------------------------*\
 *
 *   Copyright (C) 2004 AVM GmbH <fritzbox_info@avm.de>
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/tffs.h>
#include <linux/zlib.h>
#include <linux/smp_lock.h>
#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
#include <asm/mach_avm.h>


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #define TFFS_DEBUG ---*/
#include "tffs_local.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int tffs_mtd[2] = { CONFIG_TFFS_MTD_DEVICE_0, CONFIG_TFFS_MTD_DEVICE_1 };
MODULE_PARM_DESC (tffs_mtd, "MTD device used for tiny flash file system with double buffering");
MODULE_DESCRIPTION("TFFS 'tiny flash file system with double buffering' driver version 2.2");
MODULE_SUPPORTED_DEVICE("MTD[0..n]");
MODULE_LICENSE("GPL");
#define TFFS_VERSION "2.0"
MODULE_VERSION(TFFS_VERSION);

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *tffs_proc;
#endif /*--- #ifdef CONFIG_PROC_FS ---*/

#ifdef TFFS_BLOCK_MTD
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _tffs_bdev tffs_bdev = {
    use_bdev:    0,
    size:        0
};
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void tffs_cleanup(void);

struct file_operations tffs_fops = {
    owner:   THIS_MODULE,
    open:    tffs_open,
    flush:   tffs_flush,
    release: tffs_release,
    read:    tffs_read,
    write:   tffs_write,
    ioctl:   tffs_ioctl,
};


struct _tffs tffs;
struct semaphore tffs_sema;
static struct semaphore tffs_event_sema;
static struct semaphore tffs_event_lock_sema;
static int tffs_thread_pid;
int tffs_thread_event;
static DECLARE_COMPLETION(tffs_evt_dead);

enum _tffs_thread_state tffs_thread_state;
unsigned int tffs_request_count = 0;
char *panic_log_buf = NULL;

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int tffs_lock(void) {
    if(down_interruptible(&tffs_sema)) {
        return -1;
    }
    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void tffs_unlock(void) {
    up(&tffs_sema);
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int tffs_thread(void *reply_data) {
    unsigned int event;
    void *handle;
    tffs_thread_state = tffs_thread_state_init;
	lock_kernel();
	daemonize("tffsd_mtd_0");
	unlock_kernel();
	/*--- strcpy(current->comm, "tffsd_mtd_"); ---*/
    /*--- current->comm[9] = '0' + tffs_mtd[0]; ---*/

    handle = TFFS_Open();
    tffs_thread_state = tffs_thread_state_idle;
    for( ;; ) {
        DBG((KERN_INFO "tffsd: wait for events\n"));
        if(down_interruptible(&tffs_event_sema)) {
            break;
        }
        tffs_thread_state = tffs_thread_state_process;

        /*--- lesen des Events ---*/
        down(&tffs_event_lock_sema);
        event = tffs_thread_event;
        tffs_thread_event = 0;
        up(&tffs_event_lock_sema);

        tffs_request_count++;
        /*--- auswerten des Events ---*/
        DBG((KERN_INFO "tffsd: process event 0x%x\n", event));

        if(event & TFFS_EVENT_CLEANUP) {
            DBG((KERN_DEBUG "%s: tffsd: cleanup\n", MODULE_NAME));
            if(down_interruptible(&tffs_sema)) {
                break;
            }
            TFFS_Cleanup(handle);
            up(&tffs_sema);
        }
    }

    TFFS_Close(handle);
    DBG((KERN_INFO "tffsd: event thread dead\n"));
    tffs_thread_state = tffs_thread_state_down;
    complete_and_exit(&tffs_evt_dead, 0);
    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void tffs_send_event(unsigned int event) {
    DBG((KERN_INFO "tffs_send_event(%x)\n", event));
    down(&tffs_event_lock_sema);
    tffs_thread_event |= event;
    up(&tffs_event_lock_sema);
    up(&tffs_event_sema);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void tffs_init_cleanup(void) {
#if defined(CONFIG_PROC_FS)
    if(tffs_proc)
         remove_proc_entry( "tffs", 0);
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
    if(tffs.cdev_ticfg) {
        kobject_put(&tffs.cdev_ticfg->kobj);
        cdev_del(tffs.cdev_ticfg);
    }
    if(tffs.cdev) {
        kobject_put(&tffs.cdev->kobj);
        cdev_del(tffs.cdev);
    }
    unregister_chrdev_region(tffs.device, 1);
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int
#if defined(CONFIG_TFFS_MODULE)
    __init 
#endif
           tffs_init(void) {
    int reason;

    DBG((KERN_INFO "[tffs] alloc_chrdev_region() param=mtd%u\n", tffs_mtd[0]));
    DBG((KERN_INFO "[tffs] CONFIG_TFFS_MTD_DEVICE_0=%u CONFIG_TFFS_MTD_DEVICE_1=%u\n", CONFIG_TFFS_MTD_DEVICE_0, CONFIG_TFFS_MTD_DEVICE_1));
    tffs.device = MKDEV(0, 0);
    /*--- reason = register_chrdev_region(tffs.device, 1, "tffs"); ---*/
    reason = alloc_chrdev_region(&tffs.device, 0, 1, "tffs");
    if(reason < 0) {
        /*--- DBG((KERN_ERR "[tffs] alloc_chrdev_region failed: reason %d!\n", reason)); ---*/
        DBG((KERN_ERR "[tffs] register_chrdev_region failed: reason %d!\n", reason));
        return -ERESTARTSYS;
    }

    tffs.cdev_ticfg = 0;
	tffs.cdev = cdev_alloc();
	if (!tffs.cdev) {
        DBG((KERN_ERR "[tffs] cdev_alloc failed!\n"));
        tffs_cleanup();
        return -ERESTARTSYS;
    }

	tffs.cdev->owner = tffs_fops.owner;
	tffs.cdev->ops = &tffs_fops;
	kobject_set_name(&(tffs.cdev->kobj), "tffs0");

	if(cdev_add(tffs.cdev, tffs.device, 256)) {
        DBG((KERN_ERR "[tffs] cdev_add failed!\n"));
        tffs_cleanup();
        tffs_init_cleanup();
        return -ERESTARTSYS;
    }

#ifdef TFFS_BLOCK_MTD
    if(!tffs_bdev.use_bdev) {
#endif
        if(TFFS_Init(tffs_mtd[0], tffs_mtd[1])) {
            tffs_cleanup();
            tffs_init_cleanup();
            DBG((KERN_ERR "[tffs] TFFS_Init failed!\n"));
            return -ERESTARTSYS;
        }
#ifdef TFFS_BLOCK_MTD
    }
#endif

    sema_init(&tffs_sema, 1);
    sema_init(&tffs_event_sema, 0);
    sema_init(&tffs_event_lock_sema, 1);

#if 0
	tffs.cdev_ticfg = cdev_alloc();
	if (!tffs.cdev_ticfg) {
        DBG((KERN_ERR "[tffs] cdev_alloc failed!\n"));
        tffs_cleanup();
        tffs_init_cleanup();
        return -ERESTARTSYS;
    }

	tffs.cdev_ticfg->owner = tffs_fops.owner;
	tffs.cdev_ticfg->ops = &tffs_fops;
	kobject_set_name(&(tffs.cdev_ticfg->kobj), "ticfg");

	if(cdev_add(tffs.cdev_ticfg, tffs.device, 256)) {
        DBG((KERN_ERR "[tffs] cdev_add failed!\n"));
        tffs_cleanup();
        tffs_init_cleanup();
        return -ERESTARTSYS;
    }
#endif

	/*--- tffs.devfs_shutdown_handle = devfs_register(NULL, "shutdown_cmd", DEVFS_FL_DEFAULT, tffs.major, FLASH_FS_ID_SHUTDOWN, ---*/
                                	   /*--- S_IFCHR | S_IRUGO | S_IWUSR, &tffs_fops, NULL); ---*/

    DBG((KERN_INFO "[tffs] Character device init successfull \n"));

#   ifdef CONFIG_PROC_FS
	if ((tffs_proc = create_proc_entry( "tffs", 0, 0 ))) {
	  tffs_proc->read_proc = tffs_read_proc;
	  tffs_proc->write_proc = tffs_write_proc;
    }
#   endif

    tffs_thread_state = tffs_thread_state_off;
	tffs_thread_pid = kernel_thread(tffs_thread, &tffs_thread_event, CLONE_SIGHAND);

    printk("TFFS: tiny flash file system driver. GPL (c) AVM Berlin (Version %s)\n", TFFS_VERSION);
    if(!tffs_bdev.use_bdev) {
        printk("      mount on mtd%u and mtd%u (double buffering)\n", tffs_mtd[0], tffs_mtd[1]);
    }

#   ifdef CONFIG_SYSCTL
    avm_urlader_env_init();
#   endif /* CONFIG_SYSCTL */


    panic_log_buf = kmalloc(PANIC_LOG_BUF_SIZE, GFP_ATOMIC);
    if(panic_log_buf == NULL) {
        printk("[%s] WARNING - no panic log buffer reserved\n", __FUNCTION__); 
    }

    return 0;
}

#if defined(CONFIG_TFFS_MODULE)
module_init(tffs_init);
#endif
#if defined(CONFIG_TFFS)
late_initcall(tffs_init);
#endif /*--- #if defined(CONFIG_TFFS) ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void tffs_cleanup(void) {
#if defined(MODLE)
    DBG((KERN_INFO "%s: unregister_chrdev(%u)\n", MODULE_NAME, tffs.major));
    if(down_interruptible(&tffs_sema)) {
        DBG((KERN_ERR "%s: tffs_cleanup: down_interruptible() failed\n", MODULE_NAME));
        return;
    }
#ifdef CONFIG_SYSCTL
    avm_urlader_env_exit();
#endif /* CONFIG_SYSCTL */

    if(tffs_thread_pid) {
        int stat;
        DBG((KERN_INFO "%s: Terminating thread ", MODULE_NAME));
		stat = kill_proc(tffs_thread_pid, SIGTERM, 1);
		if(!stat) {
			DBG(("waiting..."));
			wait_for_completion(&tffs_evt_dead);
		}
		DBG(("done.\n"));
    }

    TFFS_Deinit();
    tffs_init_cleanup();
    up(&tffs_sema);
    kfree(panic_log_buf);
    panic_log_buf = NULL;
#endif /*--- #if defined(MODLE) ---*/
    return;
}

module_exit(tffs_cleanup);

#ifdef TFFS_BLOCK_MTD
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init tffs_setup_partition(char *p) {
    // Erwarteter String: "/dev/sda1,0x10000"
    //
    printk(KERN_ERR "[%s] str=\"%s\"\n", __FUNCTION__, p);
    if(p) {
        char *pp = strchr(p, ',');
        if(pp) {
            pp++;
            tffs_bdev.size = (unsigned int)simple_strtoul(pp, NULL, 16);
            memcpy(tffs_bdev.path, p, pp-p);
            tffs_bdev.path[pp - p - 1] = '\0';    /* \0 auf die Stelle des Kommas */
            printk(KERN_INFO "[%s] tffs path %s\n", __FUNCTION__, tffs_bdev.path);
        }
    }

    //---------
    // Pruefungen
    if(!tffs_bdev.size) {
        DBG((KERN_ERR "[%s] failed to open blockdevice! Invalid size of: %u | Setup string: %s\n", __FUNCTION__, (unsigned int)tffs_bdev.size, p));
        return -1;
    }

    tffs_bdev.use_bdev = 1;
    return 0;
}
__setup("tffs=", tffs_setup_partition);
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/
