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
ssize_t tffs_read(struct file *filp, char *read_buffer, size_t max_read_length, loff_t *offp) {
    unsigned int status;
    unsigned int copy_count;
    struct _tffs_open *open_data = (struct _tffs_open *)filp->private_data;
    char *local_read_buffer = vmalloc(max_read_length);

    if(local_read_buffer == NULL) {
        /*--- kein speicher mehr ---*/ 
        DBG((KERN_ERR "tffs_read: vmalloc(%u) failed\n", max_read_length));
        return -ENOMEM;
    }

    if(open_data->id == 0) {
        DBG((KERN_ERR "tffs_read: no read supported on minor 0\n"));
        vfree(local_read_buffer);
        return -EOPNOTSUPP;
    }

    if(open_data->stream.avail_in == 0) {
        DBG((KERN_INFO "tff_read: input streem empty, return 0 bytes\n"));
        vfree(local_read_buffer);
        return 0;
    }

    open_data->stream.total_in  = 0;
    open_data->stream.next_out  = local_read_buffer;
    open_data->stream.avail_out = max_read_length;

    DBG((KERN_INFO "before: inflate(total_in=%u next_in=0x%p avail_in=%u total_out=%u next_out=0x%p avail_out=%u)\n", 
            (int)open_data->stream.total_in, open_data->stream.next_in, (int)open_data->stream.avail_in,
            (int)open_data->stream.total_out, open_data->stream.next_out, (int)open_data->stream.avail_out));

    status = zlib_inflate(&open_data->stream, Z_SYNC_FLUSH);

    DBG((KERN_INFO "after: inflate(total_in=%u next_in=0x%p avail_in=%u total_out=%u next_out=0x%p avail_out=%u): status = %d\n", 
            (int)open_data->stream.total_in, open_data->stream.next_in, (int)open_data->stream.avail_in,
            (int)open_data->stream.total_out, open_data->stream.next_out, (int)open_data->stream.avail_out,
            status));

    switch(status) {
        case Z_STREAM_END:
            zlib_inflateEnd(&open_data->stream);
            /*--- kein break ---*/
        case Z_OK:
            copy_count = max_read_length - open_data->stream.avail_out;
            if(!copy_to_user(read_buffer, local_read_buffer, copy_count)) {
                *offp += open_data->stream.total_out;
                DBG((KERN_INFO "%s: tffs_read: status %d read %u bytes\n", MODULE_NAME, status, copy_count));
                vfree(local_read_buffer);
                return (int)copy_count;
            }
            break;
        default:
            if(open_data->stream.msg) {
                printk("[tffs_read] id 0x%x msg = %s\n", open_data->id, open_data->stream.msg);
#ifdef CONFIG_VR9
                dump_stack();
#endif
            }
            break;
    }
    if(local_read_buffer)
        vfree(local_read_buffer);
    return (unsigned int)-EFAULT;
}

