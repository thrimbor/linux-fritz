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
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/tffs.h>
#include <linux/zlib.h>
#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
#include "tffs_local.h"

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int tffs_ioctl(struct inode *inode, struct file *filp, unsigned int ioctl_param, unsigned long argv) {
    struct _tffs_cmd tffs_cmd;
    unsigned char *buffer;
    unsigned int result_size = sizeof(tffs_cmd);
    unsigned int number = _IOC_NR(ioctl_param);
    struct _tffs_open *open_data = (struct _tffs_open *)filp->private_data;

    DBG((KERN_INFO "%s: tffs_ioctl: type %u number %u size %u\n", MODULE_NAME, _IOC_TYPE(ioctl_param), number, _IOC_SIZE(ioctl_param)));

    if(open_data->id != 0) {
        /*--- printk(KERN_WARNING "%s: tffs_ioctl: ioctl not supported by minor != 0\n", MODULE_NAME); ---*/
        return -EOPNOTSUPP;
    }

    if(argv == 0) {
        printk(KERN_ERR "%s: tffs_ioctl: no data pointer for cmd number %u\n", MODULE_NAME, number);
        return -EFAULT;
    }
    if(copy_from_user(&tffs_cmd, (const void *)argv, sizeof(tffs_cmd))) {
        printk(KERN_ERR "%s: tffs_ioctl: copy_from_user failed\n", MODULE_NAME);
        return -EFAULT;
    }

    switch(number) {
        case _TFFS_READ_ID:
            if((filp->f_flags & O_ACCMODE) == O_WRONLY) {
                printk(KERN_ERR "%s: tffs_ioctl: read failed: flags=O_WRONLY\n", MODULE_NAME);
                return -EFAULT;
            }
            buffer = kmalloc(tffs_cmd.size, GFP_KERNEL);
            if(!buffer) {
                printk(KERN_ERR "%s: tffs_ioctl: alloc(%u) failed\n", MODULE_NAME, tffs_cmd.size);
                return -EFAULT;
            }
            DBG((KERN_DEBUG "%s: tffs_ioctl: read \n", MODULE_NAME));

            if(down_interruptible(&tffs_sema)) {
                kfree(buffer);
                printk(KERN_ERR "%s: tffs_ioctl(READ): down_interruptible() failed\n", MODULE_NAME);
                return -ERESTARTSYS;
            }
            tffs_cmd.status = TFFS_Read(filp->private_data, tffs_cmd.id, buffer, &tffs_cmd.size);
            up(&tffs_sema);

            if(tffs_cmd.status == 0) {
                if(copy_to_user((void *)tffs_cmd.buffer, buffer, (unsigned long)tffs_cmd.size)) {
                    kfree(buffer);
                    printk(KERN_ERR "%s: tffs_ioctl: copy_to_user failed\n", MODULE_NAME);
                    return -EFAULT;
                }
                result_size += tffs_cmd.size;
            }
            kfree(buffer);
            break;

        case _TFFS_WRITE_ID:
            if((filp->f_flags & O_ACCMODE) == O_RDONLY) {
                printk(KERN_ERR "%s: tffs_ioctl: write failed: flags=O_RDONLY\n", MODULE_NAME);
                return -EFAULT;
            }
            buffer = kmalloc(tffs_cmd.size, GFP_KERNEL);
            if(!buffer) {
                printk(KERN_ERR "%s: tffs_ioctl: alloc(%u) failed\n", MODULE_NAME, tffs_cmd.size);
                return -EFAULT;
            }
            if(copy_from_user(buffer, (void *)tffs_cmd.buffer, (unsigned long)tffs_cmd.size)) {
                kfree(buffer);
                printk(KERN_ERR "%s: tffs_ioctl: copy_from_user failed\n", MODULE_NAME);
                return -EFAULT;
            }
            DBG((KERN_DEBUG "%s: tffs_ioctl: write\n", MODULE_NAME));

            if(down_interruptible(&tffs_sema)) {
                kfree(buffer);
                printk(KERN_ERR "%s: tffs_ioctl(WRITE): down_interruptible() failed\n", MODULE_NAME);
                return -ERESTARTSYS;
            }
            tffs_cmd.status = TFFS_Write(filp->private_data, tffs_cmd.id, buffer, tffs_cmd.size, 0);
            up(&tffs_sema);

            kfree(buffer);
            break;

#if 0          /*--- rausgeflogen durch h.schillert 01.03.2007 ---*/ 
        case _TFFS_FORMAT:
            DBG((KERN_DEBUG "%s: tffs_ioctl: format\n", MODULE_NAME));
            if(down_interruptible(&tffs_sema)) {
                printk(KERN_ERR "%s: tffs_ioctl(FORMAT): down_interruptible() failed\n", MODULE_NAME);
                return -ERESTARTSYS;
            }
            tffs_cmd.status = TFFS_Format(filp->private_data);
            up(&tffs_sema);
            break;
#endif

        case _TFFS_WERKSEINSTELLUNG:
            DBG((KERN_DEBUG "%s: tffs_ioctl: format\n", MODULE_NAME));
            if(down_interruptible(&tffs_sema)) {
                printk(KERN_ERR "%s: tffs_ioctl(WERKSEINSTELLUNG): down_interruptible() failed\n", MODULE_NAME);
                return -ERESTARTSYS;
            }
            tffs_cmd.status = TFFS_Werkseinstellungen(filp->private_data);
            up(&tffs_sema);
            break;

        case _TFFS_CLEAR_ID:
            DBG((KERN_DEBUG "%s: tffs_ioctl: clear id\n", MODULE_NAME));
            if(down_interruptible(&tffs_sema)) {
                printk(KERN_ERR "%s: tffs_ioctl(CLEAR_ID): down_interruptible() failed\n", MODULE_NAME);
                return -ERESTARTSYS;
            }
            tffs_cmd.status = TFFS_Clear(filp->private_data, tffs_cmd.id);
            up(&tffs_sema);
            break;

        case _TFFS_CLEANUP:
            DBG((KERN_DEBUG "%s: tffs_ioctl: cleanup\n", MODULE_NAME));
            if(down_interruptible(&tffs_sema)) {
                printk(KERN_ERR "%s: tffs_ioctl(CLEANUP): down_interruptible() failed\n", MODULE_NAME);
                return -ERESTARTSYS;
            }
            tffs_cmd.status = TFFS_Cleanup(filp->private_data);
            up(&tffs_sema);
            break;


        case _TFFS_REINDEX:
            DBG((KERN_DEBUG "%s: tffs_ioctl: reindex\n", MODULE_NAME));
            if(down_interruptible(&tffs_sema)) {
                printk(KERN_ERR "%s: tffs_ioctl(REINDEX): down_interruptible() failed\n", MODULE_NAME);
                return -ERESTARTSYS;
            }
            tffs_cmd.status = TFFS_Create_Index();
            up(&tffs_sema);
            break;

        case _TFFS_INFO:
            DBG((KERN_DEBUG "%s: tffs_ioctl: info\n", MODULE_NAME));
            if(down_interruptible(&tffs_sema)) {
                printk(KERN_ERR "%s: tffs_ioctl(INFO): down_interruptible() failed\n", MODULE_NAME);
                return -ERESTARTSYS;
            }
            tffs_cmd.status = TFFS_Info(filp->private_data, &tffs_cmd.id);
            up(&tffs_sema);
            break;

        default:
            printk(KERN_ERR "%s: tffs_ioctl: unknwon\n", MODULE_NAME);
	        return -EINVAL;
    }

    if(copy_to_user((void *)argv, &tffs_cmd, sizeof(tffs_cmd))) {
        printk(KERN_ERR "%s: tffs_ioctl: copy_to_user failed\n", MODULE_NAME);
        return -EFAULT;
    }
    return 0;
}
