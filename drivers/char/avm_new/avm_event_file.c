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
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/device.h>

#define AVM_EVENT_INTERNAL
#include <linux/avm_event.h>
#include "avm_sammel.h"
#include "avm_event.h"

/*------------------------------------------------------------------------------------------*\
 *  2.6 Kernel H-Files
\*------------------------------------------------------------------------------------------*/
#if KERNEL_VERSION(2, 6, 0) < LINUX_VERSION_CODE 
#include <linux/cdev.h>
#include <asm/mach_avm.h>
#endif 

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 19)
#define AVM_EVENT_UDEV
#endif/*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/

/*------------------------------------------------------------------------------------------*\
 *  2.4 Kernel H-Files
\*------------------------------------------------------------------------------------------*/
#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE 
#include <asm/smplock.h>
#endif 

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- MODULE_DESCRIPTION("AVM Central Event distribution"); ---*/
/*--- MODULE_LICENSE("\n(C) Copyright 2004, AVM\n"); ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_event_open(struct inode *, struct file *);
static int avm_event_close(struct inode *, struct file *);
static int avm_event_fasync(int, struct file *, int);
static ssize_t avm_event_write(struct file *, const char *, size_t , loff_t *);
static ssize_t avm_event_read(struct file *, char *, size_t , loff_t *);
void avm_event_cleanup(void);
static unsigned int avm_event_poll(struct file *filp, poll_table *wait);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_event avm_event;
struct semaphore avm_event_sema;
unsigned long long avm_event_source_mask;
#if KERNEL_VERSION(2, 6, 0) < LINUX_VERSION_CODE 
DEFINE_SPINLOCK(avm_event_lock);
#endif /*--- #if KERNEL_VERSION(2, 6, 0) < LINUX_VERSION_CODE ---*/ 

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct file_operations avm_event_fops = {
    owner:   THIS_MODULE,
    open:    avm_event_open,
    release: avm_event_close,
    read:    avm_event_read,
    write:   avm_event_write,
    /*--- ioctl:   avm_event_ioctl, ---*/
    fasync:  avm_event_fasync,
    poll: avm_event_poll,
};

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int __init avm_event_init(void) {

#if KERNEL_VERSION(2, 6, 0) < LINUX_VERSION_CODE 
    int reason;

    DEB_INFO("[avm_event] register_chrdev_region()\n");
#if defined(AVM_EVENT_UDEV)
    reason = alloc_chrdev_region(&avm_event.device, 0, 1, "avm_event");
#else /*--- #if defined(AVM_EVENT_UDEV) ---*/
    avm_event.device = MKDEV(AVM_EVENT_MAJOR, 0);
    reason = register_chrdev_region(avm_event.device, 1, "avm_event");
#endif
    if(reason) {
        DEB_ERR("[avm_event] register_chrdev_region failed: reason %d!\n", reason);
        return -ERESTARTSYS;
    }

	avm_event.cdev = cdev_alloc();
	if (!avm_event.cdev) {
        unregister_chrdev_region(avm_event.device, 1);
        DEB_ERR("[avm_event] cdev_alloc failed!\n");
        return -ERESTARTSYS;
    }

	avm_event.cdev->owner = avm_event_fops.owner;
	avm_event.cdev->ops = &avm_event_fops;
	kobject_set_name(&(avm_event.cdev->kobj), "avm_event");
    spin_lock_init(&avm_event_lock);
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
#else /*--- #if KERNEL_VERSION(2, 6, 0) < LINUX_VERSION_CODE ---*/ 
    DEB_INFO("[%s]: register_chrdev()\n", "avm_event");
    avm_event.major = register_chrdev(0 /*--- dynamic major ---*/, "avm_event", &avm_event_fops);
    if(avm_event.major < 1) {
        DEB_ERR("%s: register_chrdrv failed: reason %d\n", "avm_event", avm_event.major);
        return -ERESTARTSYS;
    }
#endif /*--- #else ---*/ /*--- #if KERNEL_VERSION(2, 6, 0) < LINUX_VERSION_CODE ---*/ 
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/

    sema_init(&avm_event_sema, 1);

    avm_event_init2(512 /* max_items */, 512 /* max_datas */);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) 
	if(cdev_add(avm_event.cdev, avm_event.device, 1)) {
        kobject_put(&avm_event.cdev->kobj);
        unregister_chrdev_region(avm_event.device, 1);
        DEB_ERR("[avm_event] cdev_add failed!\n");
        return -ERESTARTSYS;
    }
#if defined(AVM_EVENT_UDEV)
    /*--- Geraetedatei anlegen: ---*/
    avm_event.osclass = class_create(THIS_MODULE, "avm_event");
    device_create(avm_event.osclass, NULL, 1, NULL, "%s%d", "avm_event", 0);
#endif/*--- #if defined(AVM_EVENT_UDEV) ---*/
#else /*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/ 
    avm_event.minor   = 0;
	avm_event.devfs_handle = devfs_register(NULL, "avm_event", DEVFS_FL_DEFAULT, avm_event.major, avm_event.minor,
                                	   S_IFCHR | S_IRUGO | S_IWUSR, &avm_event_fops, NULL);

    DEB_INFO("[%s]: major %d (success)\n", "avm_event", avm_event.major);
#endif /*--- #else ---*/ /*--- #if KERNEL_VERSION(2, 6, 0) < LINUX_VERSION_CODE ---*/ 
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 0)) && defined(CONFIG_AVM_PUSH_BUTTON)
    avm_event_push_button_init();
#endif /*--- #if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 0)) && defined(CONFIG_AVM_PUSH_BUTTON) ---*/

#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 32)
    avm_event_proc_init();
#endif/*--- #if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 32) ---*/
    return 0;
}

/*--- module_init(avm_event_init); ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#if defined(AVM_EVENT_MODULE)
void avm_event_cleanup(void) {
    DEB_INFO("[%s]: unregister_chrdev(%u)\n", "avm_event", avm_event.major);
#if defined(CONFIG_AVM_PUSH_BUTTON)
    avm_event_push_button_deinit();
#endif /*--- #if defined(CONFIG_AVM_PUSH_BUTTON) ---*/
    avm_event_deinit2();
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
#if KERNEL_VERSION(2, 6, 0) < LINUX_VERSION_CODE 
#if defined(AVM_EVENT_UDEV)
    device_destroy(avm_event.osclass, 1);
    class_destroy(avm_event.osclass);
#endif/*--- #if defined(AVM_EVENT_UDEV) ---*/
    cdev_del(avm_event.cdev); /* Delete char device */
    avm_event_deinit2();
    unregister_chrdev_region(avm_event.device, 1);
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
#else /*--- #if KERNEL_VERSION(2, 6, 0) < LINUX_VERSION_CODE ---*/ 
    if(avm_event.devfs_handle)
	    devfs_unregister(avm_event.devfs_handle);
	devfs_unregister_chrdev(avm_event.major, "avm_event");
#endif /*--- #else ---*/ /*--- #if KERNEL_VERSION(2, 6, 0) < LINUX_VERSION_CODE ---*/ 
    return;
}

/*--- module_exit(avm_event_cleanup); ---*/
#endif /*--- #if defined(AVM_EVENT_MODULE) ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_event_fasync(int fd, struct file *filp, int mode) {
    struct _avm_event_open_data *open_data = (struct _avm_event_open_data *)filp->private_data;
    DEB_INFO("[%s] avm_event_fasync:\n", "avm_event");
    return fasync_helper(fd, filp, mode, &(open_data->fasync));
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static unsigned int avm_event_poll(struct file *filp, poll_table *wait) {
    struct _avm_event_open_data *open_data = (struct _avm_event_open_data *)filp->private_data;

	poll_wait (filp, &(open_data->wait_queue), wait);

    if(open_data->item) {
        DEB_INFO("[%s] avm_event_poll: POLLIN (%s)\n", "avm_event", current->comm);
	    return POLLIN | POLLRDNORM;
    }
    DEB_INFO("[%s] avm_event_poll: (%s)\n", "avm_event", current->comm);

    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_event_open(struct inode *inode __attribute__((unused)), struct file *filp) {
    struct _avm_event_open_data *open_data;

    DEB_INFO("[%s]: avm_event_open:\n", "avm_event");
    /*-------------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------------*/
    if(filp->f_flags & O_APPEND) {
        DEB_ERR("[%s]: avm_event_open: open O_APPEND not supported\n", "avm_event");
        return -EFAULT;
    }

    /*-------------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------------*/
    if(down_interruptible(&avm_event_sema)) {
        DEB_ERR("[%s] down_interruptible() failed\n", "avm_event");
        return -ERESTARTSYS;
    }

    /*-------------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------------*/
    open_data = (struct _avm_event_open_data *)kmalloc(sizeof(struct _avm_event_open_data), GFP_KERNEL);
    if(!open_data) {
        DEB_ERR("%s: avm_event_open: open malloc failed\n", "avm_event");
        up(&avm_event_sema);
        return -EFAULT;
    }
    memset(open_data, 0, sizeof(*open_data));

    init_waitqueue_head (&(open_data->wait_queue));
    open_data->pf_owner = &(filp->f_owner);
    filp->private_data = (void *)open_data;

#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE 
    MOD_INC_USE_COUNT;
#endif
    up(&avm_event_sema);
    DEB_INFO("[%s]: avm_event_open: open success flags=0x%x\n", "avm_event", filp->f_flags);
    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_event_close(struct inode *inode __attribute__((unused)), struct file *filp) {

    DEB_INFO("[%s]: avm_event_close:\n", "avm_event");

    if(down_interruptible(&avm_event_sema)) {
        DEB_ERR("%s down_interruptible() failed\n", "avm_event");
        return -ERESTARTSYS;
    }
    /*--- achtung auf ind wartende "gefreien" und warten bis alle fertig ---*/

    if(filp->private_data) {
        struct _avm_event_open_data *open_data = (struct _avm_event_open_data *)filp->private_data;
        struct _avm_event_cmd_param_release avm_event_cmd_param_release;
        if(open_data->registered) {
            DEB_INFO("[close_0]\n");
            if(open_data->event_source_handle)
                avm_event_source_release(open_data->event_source_handle);
            open_data->event_source_handle = NULL;

            DEB_INFO("[close_1]\n");
            strcpy(avm_event_cmd_param_release.Name, open_data->Name);
            DEB_INFO("[close_2]\n");
            (void)avm_event_release(open_data, &avm_event_cmd_param_release);
            DEB_INFO("[close_3]\n");
        }
        avm_event_fasync(-1, filp, 0);  /*--- remove this file from asynchonously notified filp ---*/
        DEB_INFO("[close_4]\n");
        kfree(filp->private_data);
        DEB_INFO("[close_5]\n");
        filp->private_data = NULL;
        DEB_INFO("[close_6]\n");
    }

#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE 
    MOD_DEC_USE_COUNT;
#endif
    up(&avm_event_sema);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_event_source_user_mode_notify(void *Context, enum _avm_event_id id) {
    struct _avm_event_open_data *O = (struct _avm_event_open_data *)Context;
    struct _avm_event_data *D;

    DEB_INFO("[avm_event_source_user_mode_notify]: avm_event_source_user_mode_notify:\n");

    if(O->event_mask_registered & ((unsigned long long)1 << avm_event_id_user_source_notify)) {
        struct _avm_event_user_mode_source_notify *N;
        N = (struct _avm_event_user_mode_source_notify *)kmalloc(sizeof(struct _avm_event_user_mode_source_notify), GFP_KERNEL);
        if(N == NULL) {
            DEB_ERR("[avm_event_source_user_mode_notify]: out of memory\n");
            return;
        }

        D = (struct _avm_event_data *)avm_event_alloc_data();
        if(D == NULL) {
            kfree(N);
            DEB_ERR("[avm_event_source_user_mode_notify]: out of memory\n");
            return;
        }
        N->header.id   = avm_event_id_user_source_notify;
        N->id          = id;
        D->link_count  = 0; 
        D->data        = N;
        D->data_length = sizeof(struct _avm_event_user_mode_source_notify);
        avm_event_source_trigger_one(O, D);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static ssize_t avm_event_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *write_pos __attribute__((unused))) {
    unsigned int status;
    unsigned char *data;
    unsigned int data_length;
    struct _avm_event_cmd Buffer;
    struct _avm_event_open_data *open_data = (struct _avm_event_open_data *)filp->private_data;

    DEB_INFO("[%s]: write_length = %u *write_pos = 0x%LX\n", "avm_event_write", write_length, *write_pos);

    if(write_length < sizeof(Buffer)) {
        DEB_ERR("%s: avm_event_write: write_lengh < %u\n", "avm_event_write", sizeof(Buffer));
        return -EINVAL;
    }
    if(copy_from_user(&Buffer, write_buffer, sizeof(Buffer))) {
        DEB_ERR("%s: avm_event_write: copy_from_user failed\n", "avm_event_write");
        return -EFAULT;
    }
    if((Buffer.cmd != avm_event_cmd_register) && (open_data->registered == 0)) {
        DEB_ERR("%s: avm_event_write: not registered\n", "avm_event_write");
        return -EFAULT;
    }

    if(down_interruptible(&avm_event_sema)) {
        DEB_ERR("%s down_interruptible() failed\n", "avm_event_write");
        return -ERESTARTSYS;
    }
    switch(Buffer.cmd) {
        case avm_event_cmd_register:
            DEB_INFO("[%s]: avm_event_cmd_register\n", "avm_event_write");
            status = avm_event_register(open_data, &Buffer.param.avm_event_cmd_param_register);
            if(status == 0)
                open_data->registered = 1;
            break;

        case avm_event_cmd_release:
            DEB_INFO("[%s]: avm_event_cmd_release\n", "avm_event_write");
            status = avm_event_release(open_data, &Buffer.param.avm_event_cmd_param_release);
            open_data->registered = 0;
            break;

        case avm_event_cmd_trigger:
            DEB_INFO("[%s]: avm_event_cmd_release\n", "avm_event_write");
            status = avm_event_trigger(open_data, &Buffer.param.avm_event_cmd_param_trigger);
            break;

        case avm_event_cmd_source_register:
            DEB_INFO("[%s]: avm_event_cmd_source_register\n", "avm_event_write");
            if(open_data->event_source_handle) {
                status = -EACCES;
                break;
            }
            open_data->event_source_handle = avm_event_source_register(Buffer.param.avm_event_cmd_param_source_register.Name, 
                                                            Buffer.param.avm_event_cmd_param_source_register.mask, 
                                                            avm_event_source_user_mode_notify, open_data);
            if(open_data->event_source_handle) {
                status = 0;
            } else {
                status = -EACCES;
            }
            break;

        case avm_event_cmd_source_release:
            DEB_INFO("[%s]: avm_event_cmd_source_release\n", "avm_event_write");
            status = 0;
            if(open_data->event_source_handle == NULL) {
                break;
            }
            avm_event_source_release(open_data->event_source_handle);
            open_data->event_source_handle = NULL;
            break;

        case avm_event_cmd_source_trigger:
            DEB_INFO("[%s]: avm_event_cmd_source_trigger\n", "avm_event_write");
            if(open_data->event_source_handle == NULL) {
                status = -EACCES;
                break;
            }
            data_length = Buffer.param.avm_event_cmd_param_source_trigger.data_length;
            data = kmalloc(data_length, GFP_KERNEL);
            if(data == NULL) {
                status = -ENOMEM;
                break;
            }
            if(copy_from_user(data, write_buffer + sizeof(struct _avm_event_cmd), data_length)) {
                DEB_ERR("%s: avm_event_write: copy_from_user failed\n", "avm_event_write");
                return -EFAULT;
            }
            avm_event_source_trigger(open_data->event_source_handle, 
                    Buffer.param.avm_event_cmd_param_source_trigger.id, data_length, data);
            status = 0;
            break;

        default:
        case avm_event_cmd_undef:
            DEB_ERR("[%s]: avm_event_cmd_undef\n", "avm_event_write");
            status = -EINVAL;
    }
    up(&avm_event_sema);
    if(status)
        return status;
    return write_length;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static ssize_t avm_event_read(struct file *filp, char *read_buffer, size_t max_read_length, loff_t *read_pos) {
    unsigned int event_pos;
    unsigned int copy_length = 0;
    unsigned int rx_buffer_length = 0;
    unsigned char *rx_buffer;
    unsigned int commit = 0;
    struct _avm_event_open_data *open_data = (struct _avm_event_open_data *)filp->private_data;

    DEB_INFO("[%s]: avm_event_read:\n", "avm_event_read");

avm_event_read_retry:
    if(down_interruptible(&avm_event_sema)) {
        DEB_ERR("%s down_interruptible() failed\n", "avm_event_read");
        return -ERESTARTSYS;
    }
    avm_event_get(open_data, &rx_buffer, &rx_buffer_length, &event_pos);
    /*--------------------------------------------------------------------------------------*\
     * sind überhaupt Daten vorhanden
    \*--------------------------------------------------------------------------------------*/
    if(rx_buffer_length) {
        DEB_INFO("[%s] '%s' rx_buffer_length = %u *read_pos = %Lu (%s)\n", "avm_event_read", open_data->Name, rx_buffer_length, *read_pos, current->comm);
        copy_length = rx_buffer_length - *read_pos;
        if(copy_length <= max_read_length) {
            commit = 1;
        } else {
            copy_length = max_read_length;
        }
    /*--------------------------------------------------------------------------------------*\
     * sind wir bloekierend, nein
    \*--------------------------------------------------------------------------------------*/
    } else if(filp->f_flags & O_NONBLOCK) {
        up(&avm_event_sema);
        DEB_INFO("[%s] non block, empty\n", "avm_event_read");
        return -EAGAIN;
    /*--------------------------------------------------------------------------------------*\
     * sind wir bloekierend, ja
    \*--------------------------------------------------------------------------------------*/
    } else {
        up(&avm_event_sema);
        DEB_INFO("[%s] sleep on\n", "avm_event_read");
        if(wait_event_interruptible(open_data->wait_queue, open_data->item)) {
            DEB_INFO("[%s] handle released\n", "avm_event_read");
            return -ERESTARTSYS;
        }
        DEB_INFO("[%s] wake up\n", "avm_event_read");
        goto avm_event_read_retry;
    }

    if(copy_to_user(read_buffer, rx_buffer + *read_pos, copy_length)) {
        up(&avm_event_sema);
        DEB_ERR("%s: tffs_read: copy_to_user failed\n", "avm_event_read");
        return -EFAULT;
    }
    *read_pos += (loff_t)copy_length;
    if(commit) {
        *read_pos = (loff_t)0;
        avm_event_commit(open_data, event_pos);
    }
    up(&avm_event_sema);
    return copy_length;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE
EXPORT_NO_SYMBOLS;
#endif /*--- #if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
