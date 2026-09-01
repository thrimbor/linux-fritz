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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/file.h>
/*--- #include <asm/semaphore.h> ---*/
#include <asm/errno.h>
/*--- #include <linux/wait.h> ---*/
/*--- #include <linux/vmalloc.h> ---*/
/*--- #include <linux/poll.h> ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <linux/cdev.h>
#include <asm/mach_avm.h>
#define LOCAL_MAJOR     AVM_NEW_LED_MAJOR
#else
#include <linux/devfs_fs_kernel.h>
#define LOCAL_MAJOR     0
static devfs_handle_t avm_led_devfs_handle;
#endif

#include "avm_sammel.h"
#include "avm_led.h"
#include "avm_led_driver.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)
#include <linux/avm_event.h>
#endif /*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- MODULE_DESCRIPTION("AVM new led module "); ---*/
/*--- MODULE_LICENSE("\n(C) Copyright 2006 by AVM\n"); ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_led_open(struct inode *, struct file *);
static int avm_led_close(struct inode *, struct file *);
static int avm_led_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led avm_led;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct file_operations avm_led_fops = {
    owner:   THIS_MODULE,
    open:    avm_led_open,
    release: avm_led_close,
    /*--- read:    avm_led_read, ---*/
    write:   avm_led_write,
    ioctl:   avm_led_ioctl,
    /*--- fasync:  avm_led_fasync, ---*/
    /*--- poll: avm_led_poll, ---*/
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_load_config(void) {
    static struct file *filp;
    unsigned char *buffer;
    unsigned int configLength = 0, bytesRead;
    mm_segment_t oldfs;  
    char *p;
    char filename[] = "/etc/led.conf";

    buffer = kmalloc(4096, GFP_KERNEL);
    if(buffer == NULL) {
        DEB_ERR("[avm_led] avm_led_load_config: Could not allocate memory for config file!\n");
        return -1;
    }
  
    filp = filp_open(filename, 00, O_RDONLY);
    if(IS_ERR(filp)) {
        DEB_ERR("[avm_led_load_config] Could not open config file\n");
        return -EACCES;
    }
  
    if(filp->f_dentry != NULL) {
        if(filp->f_dentry->d_inode != NULL) {
            configLength = (unsigned int)filp->f_dentry->d_inode->i_size + 0x200;
        }
    }
  
    if (filp->f_op->read == NULL) {
        return -EACCES; /* File(system) doesn't allow reads */
    }
         
    /* Disable parameter checking */
    oldfs = get_fs();
    set_fs(KERNEL_DS);
  
    /* Now read bytes from postion "StartPos" */
    filp->f_pos = 0;
  
    bytesRead = filp->f_op->read(filp, buffer, configLength, &filp->f_pos);
  
    DEB_INFO("[avm_led_load_config] file length = %d\n", bytesRead);
  
    set_fs(oldfs);
    /* Close the file */
    fput(filp);

    p = buffer;
    while(bytesRead) {
        ssize_t written;

        written = avm_led_write(NULL, (const char *) p, bytesRead, (loff_t *) NULL);
        if(written > 0) {
            p += written;
            bytesRead -= written;
        } else {
            DEB_ERR("[avm_led_load_config] Syntax error in led config file %s at \"%.20s\"\n",
                    filename, p);
            break;
        }
    }
    
    kfree(buffer);

    return 0;
}
/*--- late_initcall(avm_led_load_config); ---*/


/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int __init avm_led_init(void) {
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    int reason;
#   endif /*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/

    DEB_INFO("[avm_led] register_chrdev_region()\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    avm_led.device = MKDEV(LOCAL_MAJOR, 0);
    reason = register_chrdev_region(avm_led.device, 1, "avm_led");
    if(reason) {
        DEB_ERR("[avm_led] register_chrdev_region failed: reason %d!\n", reason);
        return -ERESTARTSYS;
    }

	avm_led.cdev = cdev_alloc();
	if (!avm_led.cdev) {
        unregister_chrdev_region(avm_led.device, 1);
        DEB_ERR("[avm_led] cdev_alloc failed!\n");
        return -ERESTARTSYS;
    }

	avm_led.cdev->owner = avm_led_fops.owner;
	avm_led.cdev->ops = &avm_led_fops;
	kobject_set_name(&(avm_led.cdev->kobj), "avm_led");
#else
    avm_led.device = register_chrdev(0 /*--- dynamic major ---*/, "avm_led", &avm_led_fops);
    if(avm_led.device < 1) {
        DEB_ERR("[%s]: register_chrdrv failed: reason %d\n", "avm_led", avm_led.device);
        MOD_DEC_USE_COUNT;
        return -ERESTARTSYS;
    }
#endif
		
    avm_led_init_tables();
    /*--- avm_led_init_hardware(); ---*/
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)
    avm_led_event_init();
#endif /*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) ---*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	if(cdev_add(avm_led.cdev, avm_led.device, 1)) {
        kobject_put(&avm_led.cdev->kobj);
        unregister_chrdev_region(avm_led.device, 1);
        DEB_ERR("[avm_led] cdev_add failed!\n");
        return -ERESTARTSYS;
    }
#else
    avm_led_devfs_handle = devfs_register(NULL, "new_led",
                                        DEVFS_FL_DEFAULT, avm_led.device, 0,
                                	    S_IFCHR | S_IRUGO | S_IWUSR, &avm_led_fops, NULL);
    if(avm_led_devfs_handle == NULL) {
        DEB_ERR("%s: avm_led_file: devfs_register(%s, %u ...) failed \n", "avm_led", "new_led", 0);
        MOD_DEC_USE_COUNT;
        return -ENOMEM;
    }
#endif

    DEB_INFO("[avm_led]: major %d (success)\n", MAJOR(avm_led.device));

    return 0;
}
/*--- module_init(avm_led_init); ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#ifdef CONFIG_AVM_LED_MODULE
void __exit avm_led_cleanup(void) {
    DEB_INFO("[avm_led]: unregister_chrdev(%u)\n", MAJOR(avm_led.device));
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)
    avm_led_event_exit();
#endif /*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) ---*/
    avm_led_release_all_leds();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    cdev_del(avm_led.cdev); /* Delete char device */
    unregister_chrdev_region(avm_led.device, 1);
#else
    devfs_unregister(avm_led_devfs_handle);
    devfs_unregister_chrdev(avm_led.device, "avm_led");
#endif
    return;
}
/*--- module_exit(avm_led_cleanup); ---*/
#endif /*--- #ifdef CONFIG_AVM_LED_MODULE ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_led_open(struct inode *inode, struct file *filp) {
    struct _avm_led_open_data *open_data;

    DEB_INFO("[%s]: avm_led_open:\n", "avm_led");

    /*-------------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------------*/
    open_data = (struct _avm_led_open_data *)kmalloc(sizeof(struct _avm_led_open_data), GFP_KERNEL);
    if(!open_data) {
        DEB_ERR("%s: avm_led_open: open malloc failed\n", "avm_led");
        return -EFAULT;
    }
    memset(open_data, 0, sizeof(*open_data));
    filp->private_data = (void *)open_data;

    DEB_INFO("[%s]: avm_led_open: open success flags=0x%x\n", "avm_led", filp->f_flags);
    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_led_close(struct inode *inode, struct file *filp) {

    DEB_INFO("[%s]: avm_led_close:\n", "avm_led");

    /*--- achtung auf ind wartende "gefreien" und warten bis alle fertig ---*/

    if(filp->private_data) {
        kfree(filp->private_data);
        filp->private_data = NULL;
    }

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int avm_led_ioctl( struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg ) {
    int ret = 0;
    void *event;
    int handle;
    int from_handle;
    int to_handle;
    union __avm_led {
        struct _avm_led_register_struct led_register;
        struct _avm_led_release_struct led_release;
        struct _avm_led_action_struct led_action;
        struct _avm_led_map_struct led_map;
        struct _avm_led_unmap_struct led_unmap;
    } L;
    DEB_TRACE("[led_ioctl]: cmd = %u\n", cmd);


    switch (cmd) {
        case LED_REGISTER:
            if(copy_from_user((char *)&L.led_register, (char *)arg, sizeof(struct _avm_led_register_struct))) {
                DEB_ERR("[%s]: avm_led_ioctl: LED_REGISTER copy_from user failed\n", "avm_led");
                ret = -EFAULT;
                break;
            }
            handle = avm_led_get_virt_led_handle(L.led_release.name, L.led_release.instance);
            if((handle > -255) && (handle <= 0)) {
                ret = -EEXIST;
                break;
            }

            ret = avm_led_register_led(
                            L.led_register.name,
                            L.led_register.instance,
                            L.led_register.driver_type,
                            L.led_register.gpio,
                            L.led_register.led_pos,
                            L.led_register.led_name);
            break;

        case LED_RELEASE:
            if(copy_from_user((char *)&L.led_release, (char *)arg, sizeof(struct _avm_led_release_struct))) {
                DEB_ERR("[%s]: avm_led_ioctl: LED_RELEASE copy_from user failed\n", "avm_led");
                ret = -EFAULT;
                break;
            }
            handle = avm_led_get_virt_led_handle(L.led_release.name, L.led_release.instance);
            if((handle < 0) && (handle > -255)) {
                DEB_WARN("[%s]: avm_led_ioctl: LED_RELEASE: no virtual led \"%s\" instance %u\n", "avm_led", L.led_release.name, L.led_release.instance);
                ret = handle;
            } else {
                avm_led_release_led(handle);
            }
            break;

        case LED_MAP:
            if(copy_from_user((char *)&L.led_map, (char *)arg, sizeof(struct _avm_led_map_struct))) {
                ret = -EFAULT;
                break;
            }
            from_handle = avm_led_get_virt_led_handle(L.led_map.from_name, L.led_map.from_instance);
            if((from_handle < 0) && (from_handle > -255)) {
                DEB_WARN("[%s]: avm_led_ioctl: LED_MAP(from): no virtual led \"%s\" instance %u\n", "avm_led", L.led_map.from_name, L.led_map.from_instance);
                ret = from_handle;
                break;
            } 
            to_handle = avm_led_get_virt_led_handle(L.led_map.to_name, L.led_map.to_instance);
            if((to_handle < 0) && (to_handle > -255)) {
                DEB_WARN("[%s]: avm_led_ioctl: LED_MAP(to): no virtual led \"%s\" instance %u\n", "avm_led", L.led_map.to_name, L.led_map.to_instance);
                ret = to_handle;
                break;
            } 
            ret = avm_led_map_led(from_handle, to_handle, L.led_map.both);
            break;

        case LED_UNMAP:
            if(copy_from_user((char *)&L.led_unmap, (char *)arg, sizeof(struct _avm_led_unmap_struct))) {
                ret = -EFAULT;
                break;
            }
            handle = avm_led_get_virt_led_handle(L.led_unmap.name, L.led_unmap.instance);
            if((handle < 0) && (handle > -255)) {
                DEB_WARN("[%s]: avm_led_ioctl: LED_MAP(from): no virtual led \"%s\" instance %u\n", "avm_led", L.led_unmap.name, L.led_unmap.instance);
                ret = handle;
                break;
            } 
            ret = avm_led_map_led(handle, 0, 0);
            break;

        case LED_ACTION:
            if(copy_from_user((char *)&L.led_action, (char *)arg, sizeof(struct _avm_led_action_struct))) {
                ret = -EFAULT;
                break;
            }
            handle = avm_led_get_virt_led_handle(L.led_action.name, L.led_action.instance);
            if((handle < 0) && (handle > -255)) {
                DEB_WARN("[%s]: avm_led_ioctl: LED_ACTION: no virtual led \"%s\" instance %u\n", "avm_led", L.led_action.name, L.led_action.instance);
                ret = handle;
                break;
            } 

            avm_led_virt_led_action(handle, L.led_action.state);
            break;

        case LED_ACTION_WITH_EVENT:
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)
            if(copy_from_user((char *)&L.led_action, (char *)arg, sizeof(struct _avm_led_action_struct))) {
                DEB_ERR("[%s]: avm_led_ioctl: LED_ACTION_WITH_EVENT copy_from user failed\n", "avm_led");
                ret = -EFAULT;
                break;
            }
            event = kmalloc(L.led_action.event_size, GFP_ATOMIC);
            if(copy_from_user(event, (char *)L.led_action.event, L.led_action.event_size)) {
                DEB_ERR("[%s]: avm_led_ioctl: LED_ACTION_WITH_EVENT/event copy_from user failed\n", "avm_led");
                ret = -EFAULT;
                break;
            }
            L.led_action.event = event;
            handle = avm_led_get_virt_led_handle(L.led_action.name, L.led_action.instance);
            if((handle < 0) && (handle > -255)) {
                DEB_WARN("[%s]: avm_led_ioctl: : no virtual led \"%s\" instance %u\n", "avm_led", L.led_action.name, L.led_action.instance);
                kfree(event);
                ret = handle;
                break;
            } 
            avm_led_virt_led_action(handle, L.led_action.state);
#else /*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) ---*/
            ret = -EACCES;
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) ---*/
            break;
    }
    return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_led_monitor_user(void) {
    static char Buffer[80];
    if(!(avm_led_monitor_level & AVM_LED_MONITOR_USER))
        return "";
    sprintf(Buffer, "(context: %s)", current->comm);
    return Buffer;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
