/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2007 AVM GmbH <fritzbox_info@avm.de>
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
#include "tffs_local.h"

unsigned int tffs_spi_mode = 0;

static struct file tffs_panic_file;
static struct inode tffs_panic_inode;
static unsigned int tffs_panic_open_flag;

void tffs_panic_log_open(void) {
    int ret;
    if(tffs_panic_open_flag == 1)
        return;
    memset(&tffs_panic_file, 0, sizeof(tffs_panic_file));
    memset(&tffs_panic_inode, 0, sizeof(tffs_panic_inode));

    tffs_panic_file.f_flags = O_WRONLY;
    tffs_panic_inode.i_rdev = CONFIG_TFFS_PANIC_LOG_ID;   /*--- major muss 0 sein fuer Kernel Mode ---*/

    ret = tffs_open(&tffs_panic_inode, &tffs_panic_file);
    if(ret)
        return;
    tffs_panic_open_flag = 1;
}

void tffs_panic_log_write(char *buffer, unsigned int len) {
    static loff_t off;
    if(tffs_panic_open_flag == 0)
        return;

    tffs_write(&tffs_panic_file, buffer, len, &off);
}


void tffs_panic_log_close(void) {
    if(tffs_panic_open_flag == 0)
        return;
    tffs_release(&tffs_panic_inode, &tffs_panic_file);
}

void tffs_panic_log_register_spi(void) {
    tffs_spi_mode = 1;
}
