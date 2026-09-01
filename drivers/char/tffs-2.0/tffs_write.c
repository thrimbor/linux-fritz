/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2004, 2005 AVM GmbH <fritzbox_info@avm.de>
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
#include <linux/vmalloc.h>
#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #define TFFS_DEBUG ---*/
#include "tffs_local.h"

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
ssize_t tffs_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *offp) {
    unsigned int status;
    struct _tffs_open *open_data = (struct _tffs_open *)filp->private_data;
    char *local_write_buffer;

    DBG((KERN_DEBUG "%s: tffs_write(%u bytes)\n", MODULE_NAME, write_length));

    if(write_length == 0) {
        return (unsigned int)-EFAULT;
    }
    if(open_data->id == 0) {
        DBG((KERN_ERR "tffs_write: no read supported on minor 0\n"));
        return -EOPNOTSUPP;
    }
    if(open_data->panic_mode) {
        local_write_buffer = (char *)write_buffer;
    } else {
        local_write_buffer = vmalloc(write_length);
        if (local_write_buffer == NULL) {
            return (unsigned int)-ENOMEM;
        }
        if(copy_from_user(local_write_buffer, write_buffer, write_length)) {
            vfree(local_write_buffer);
            return (unsigned int)-EFAULT;
        }
    }

    open_data->stream.next_in   = local_write_buffer;
    open_data->stream.avail_in  = write_length;

    DBG((KERN_INFO "before: deflate(total_in=%u next_in=0x%p avail_in=%u total_out=%u next_out=0x%p avail_out=%u)\n", 
            (int)open_data->stream.total_in, open_data->stream.next_in, (int)open_data->stream.avail_in,
            (int)open_data->stream.total_out, open_data->stream.next_out, (int)open_data->stream.avail_out));

    status = zlib_deflate(&open_data->stream, Z_NO_FLUSH);

    DBG((KERN_INFO "after: deflate(total_in=%u next_in=0x%p avail_in=%u total_out=%u next_out=0x%p avail_out=%u): status = %d\n", 
            (int)open_data->stream.total_in, open_data->stream.next_in, (int)open_data->stream.avail_in,
            (int)open_data->stream.total_out, open_data->stream.next_out, (int)open_data->stream.avail_out,
            status));

    if(!tffs_panic_mode && (open_data->panic_mode == 0)) {
        vfree(local_write_buffer);
    }

    switch(status) {
        case Z_OK:
            *offp += write_length;
            DBG((KERN_INFO "%s: tffs_write: status %d\n", MODULE_NAME, status));
            return (int)write_length;
        default:
            if(open_data->stream.msg)
                printk("[tffs_write] msg = %s\n", open_data->stream.msg);
            break;
    }
    DBG((KERN_ERR "%s: tffs_write failed: deflate status %d (return EFAULT)\n", MODULE_NAME, status));
    return (unsigned int)-EFAULT;
}



