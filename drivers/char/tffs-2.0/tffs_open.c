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
#include <linux/vmalloc.h>
#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_TFFS_CRYPT)
#include <linux/crypto.h>
int aes_reinit(struct crypto_tfm *tfm);
int aes_set_key(struct crypto_tfm *tfm, const unsigned char *in_key, unsigned int key_len);
void aes_decrypt(struct crypto_tfm *tfm, void *out, const void *in);
void aes_encrypt(struct crypto_tfm *tfm, void *out, const void *in);
#endif /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #define TFFS_DEBUG ---*/
#include "tffs_local.h"

unsigned int tffs_panic_mode = 0;

#ifdef SPI_PANIC_LOG
unsigned int tffs_spi_init(void);
#endif

extern struct semaphore tffs_mtd_sema;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if  0
void tffs_dump_block(unsigned char *p, unsigned int len) {
    int i, ii;
    printk("[dump] 0x%8p (%u bytes)\n", p, len);
    for(i = 0 ; i < len ; i += 16, p += 16) {
        printk("\t0x%8p: ", p);
        for(ii = 0 ; ii < 16 && (i + ii) < len ; ii++) 
            printk("0x%02x ", p[ii]);
        for( ; ii < 16 ; ii++ )
            printk("     ");
        printk(" : ");
        for(ii = 0 ; ii < 16 && (i + ii) < len ; ii++) 
            printk("%c ", p[ii] > ' ' ? p[ii] : '.');
        printk("\n");
    }
}
#endif 

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(TFFS_DEBUG)
static void dump_buffer(char *text, unsigned int len, unsigned char *buffer) {
    int i;
#define dump_buffer_block_size      128
    for(i = 0 ; i < len ; i += dump_buffer_block_size) {
        printk("%s(%u bytes): 0x%x: % *B\n", 
            text,
            len, 
            len, 
            len - i > dump_buffer_block_size ? dump_buffer_block_size : len - i,
            buffer + i);
    }
}
#endif /*--- #if defined(TFFS_DEBUG) ---*/


/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int tffs_open(struct inode *inode, struct file *filp) {
    int minor = MINOR(inode->i_rdev);
    int major = MAJOR(inode->i_rdev);
#ifdef CONFIG_TFFS_PANIC_LOG
    if(!tffs_panic_mode) {
        tffs_panic_mode = (major == 0) && (minor == CONFIG_TFFS_PANIC_LOG_ID);
    }
#endif/*--- #ifdef CONFIG_TFFS_PANIC_LOG ---*/

    DBG((KERN_INFO "%s: tffs_open: MAJOR %u MINOR %u\n", MODULE_NAME, MAJOR(inode->i_rdev), MINOR(inode->i_rdev)));
    /*--- printk(KERN_INFO "%s: tffs_open: MAJOR %u MINOR %u\n", MODULE_NAME, MAJOR(inode->i_rdev), MINOR(inode->i_rdev)); ---*/
                
    if(tffs_panic_mode && down_trylock(&tffs_sema)) {
        printk(KERN_ERR "WARNING: failed to %sopen tffs in panic mode (minor %d)\n", major ? "re-" : "", minor);
        return -EBUSY;
    }
    if(tffs_panic_mode && down_trylock(&tffs_mtd_sema)) {
        up(&tffs_sema);
        printk(KERN_ERR "ERROR: failed to open tffs in panic mode (tffs-mtd busy)\n");
        return -EBUSY;
    }

    if(!tffs_panic_mode && down_interruptible(&tffs_sema)) {
        DBG((KERN_ERR "%s tffs_open: down_interruptible() failed\n", MODULE_NAME));
        return -ERESTARTSYS;
    }
    if(!filp->private_data) {
        filp->private_data = TFFS_Open();
    }
    if(!filp->private_data) {
        DBG((KERN_ERR "%s: tffs_open: TFFS_Open failed\n", MODULE_NAME));
        if(!tffs_panic_mode) up(&tffs_sema);
        return -ENOMEM;
    }

    if(filp->f_flags & O_APPEND) {
        DBG((KERN_ERR "%s: tffs_open: TFFS_Open O_APPEND not supported\n", MODULE_NAME));
        if(!tffs_panic_mode) up(&tffs_sema);
        return -EFAULT;
    }

    if(filp->f_flags & O_RDWR) {
        DBG(("%s: tffs_open: open O_RDONLY and O_WRONLY simultary not supported\n", MODULE_NAME));
        if(!tffs_panic_mode) up(&tffs_sema);
        return -EFAULT;
    }

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    if(minor) {
        int status;
        struct _tffs_open *open_data = (struct _tffs_open *)filp->private_data;

        memset((void *)open_data, 0, sizeof(*open_data));
        open_data->id         = minor;
        open_data->panic_mode = major == 0 ? 1 : 0;
        if(open_data->panic_mode) {
            printk("WARNING: use tffs in panic mode (minor %d)\n", minor);
#if defined(SPI_PANIC_LOG) && (defined(CONFIG_VR9) || defined(CONFIG_AR10))
            if(tffs_spi_mode) {
                if(tffs_spi_init() < 0)
                    printk("[%s] ERROR: reinit of SPI Flash failed. No panic log written!\n", __FUNCTION__);
            }
#endif
        }

#if defined(CONFIG_TFFS_CRYPT)
        memset(&open_data->tfm, 0, sizeof(struct crypto_tfm));
        memset(open_data->workspace, 0, sizeof(open_data->workspace));
        open_data->tfm.__crt_ctx[0] = open_data->workspace;

#ifndef CONFIG_DAVINCI_DRM_KUNDENVERSION
        memcpy(open_data->key, "\x59\x98\x48\xd4\xd6\x3b\xbc\xfd\x93\xda\xf3\x42\x62\xe4\x3c\x57", 16);
#else /*--- #ifndef CONFIG_DAVINCI_DRM_KUNDENVERSION ---*/
        open_data->key     = tffs_fs_key;
#endif /*--- #else ---*/ /*--- #ifndef CONFIG_DAVINCI_DRM_KUNDENVERSION ---*/
        open_data->key_len = 128;
        aes_reinit(&(open_data->tfm));
        aes_set_key(&(open_data->tfm), open_data->key, open_data->key_len >> 3);
        open_data->z_length = (32 * 1024) + 16 + 4;
#ifdef CONFIG_TFFS_PANIC_LOG
        if(open_data->panic_mode) {
            static unsigned char static_z_Buffer[(32 * 1024) + 16 + 4];
            open_data->z_Buffer = static_z_Buffer;
        } else
#endif /*--- #ifdef CONFIG_TFFS_PANIC_LOG ---*/
            open_data->z_Buffer = vmalloc(open_data->z_length);

        if(open_data->z_Buffer) open_data->z_Buffer += 4;
#else /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
        open_data->z_length = (32 * 1024) + 16;
#ifdef CONFIG_TFFS_PANIC_LOG
        if(open_data->panic_mode) {
            static unsigned char static_z_Buffer[(32 * 1024) + 16];
            open_data->z_Buffer = static_z_Buffer;
            restore_printk();
        } else 
#endif /*--- #ifdef CONFIG_TFFS_PANIC_LOG ---*/
            open_data->z_Buffer = vmalloc(open_data->z_length);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
        if(open_data->z_Buffer == NULL) {
            DBG((KERN_ERR "%s: tffs_open: no memory for z_buffer\n", MODULE_NAME));
            TFFS_Close(filp->private_data);
            filp->private_data = NULL;
            if(!tffs_panic_mode) up(&tffs_sema);
            return -ENOMEM;
        }

        open_data->stream.data_type = Z_ASCII;
        open_data->stream.workspace = NULL;

        /*----------------------------------------------------------------------------------*\
        \*----------------------------------------------------------------------------------*/
        if(filp->f_flags & O_WRONLY) {
            int workspacesize = zlib_deflate_workspacesize();
            if(open_data->panic_mode) {
                if(panic_log_buf == NULL) {
                    printk("[%s] ERROR - no panic log buffer reserved\n", __FUNCTION__); 
                }
                open_data->stream.workspace = panic_log_buf;
            } else {
                open_data->stream.workspace = vmalloc(workspacesize);
            }
            if(open_data->stream.workspace == NULL) {
                DBG((KERN_ERR "%s: tffs_open: no memory for (write) workspace\n", MODULE_NAME));
                if(open_data->panic_mode == 0) {
                    if(!tffs_panic_mode && open_data->z_Buffer) {
#if defined(CONFIG_TFFS_CRYPT)
                        vfree(open_data->z_Buffer - 4);
#else /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
                        vfree(open_data->z_Buffer);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
                        open_data->z_Buffer = NULL;
                     }
                }
                TFFS_Close(filp->private_data);
                filp->private_data = NULL;
                if(!tffs_panic_mode)up(&tffs_sema);
                return -ENOMEM;
            } else {
                DBG((KERN_INFO "%s: tffs_open: %u bytes (write) workspace (0x%p)\n", MODULE_NAME, workspacesize, open_data->stream.workspace));
            }
            DBG((KERN_INFO "%s: open for write only\n", MODULE_NAME));

            open_data->stream.next_in   = NULL;
            open_data->stream.avail_in  = 0;
            open_data->stream.total_in  = 0;

            open_data->stream.next_out  = open_data->z_Buffer;
            open_data->stream.avail_out = open_data->z_length;
            open_data->stream.total_out = 0;

            status = zlib_deflateInit(&open_data->stream, Z_DEFAULT_COMPRESSION);
            if(status != Z_OK) {
                DBG((KERN_ERR "%s: tffs_open: zlib_deflateInit failed, status = %d\n", MODULE_NAME, status));
            } else {
                open_data->init_flag        = 1;
            }
            open_data->init_flag        = 1;
        /*----------------------------------------------------------------------------------*\
        \*----------------------------------------------------------------------------------*/
        } else { /*--- (filp->f_flags & O_RDONLY) ---*/
            int workspacesize = zlib_inflate_workspacesize();
            DBG((KERN_INFO "%s: open for read only\n", MODULE_NAME));

#if defined(CONFIG_TFFS_CRYPT)
            /*------------------------------------------------------------------------------*\
             * Format der Daten:    <echte laenge unverschluesselt><crypted data>
            \*------------------------------------------------------------------------------*/
            status = TFFS_Read(open_data, open_data->id, open_data->z_Buffer - 4, &open_data->z_length);
            if(status == 0) {
                unsigned int p;
                struct _aes_block {
                    unsigned int data[4];
                };
                struct _aes_block *in  = (struct _aes_block *)open_data->z_Buffer;
                struct _aes_block *out = (struct _aes_block *)open_data->z_Buffer;
                struct _aes_block prev_c;
                struct _aes_block strich;

                /*--- tffs_dump_block(open_data->z_Buffer - 4, open_data->z_length); ---*/
                /*--- printk("[tffs-decrypt] %u bytes (real read block size)\n", open_data->z_length); ---*/

                memset(&prev_c, 0, sizeof(prev_c));
                for(p = 0 ; p < ((open_data->z_length - 4) >> 4) ; p++) {
                    unsigned int tmp;
                    aes_decrypt(&(open_data->tfm), &strich, in);

                    tmp = prev_c.data[0] ^ strich.data[0];
                    prev_c.data[0] = in->data[0];  /* out und in koennen der gleichen pointer sein, deshalb zwischenspeichern */
                    out->data[0] = tmp;

                    tmp = prev_c.data[1] ^ strich.data[1];
                    prev_c.data[1] = in->data[1];
                    out->data[1] = tmp;

                    tmp = prev_c.data[2] ^ strich.data[2];
                    prev_c.data[2] = in->data[2];
                    out->data[2] = tmp;

                    tmp = prev_c.data[3] ^ strich.data[3];
                    prev_c.data[3] = in->data[3];
                    out->data[3] = tmp;

                    in++, out++;
                }

                /*--- tffs_dump_block(open_data->z_Buffer - 4, open_data->z_length); ---*/

                open_data->z_length = ((unsigned int *)(open_data->z_Buffer))[-1];
                /*--- printk("[tffs-decrypt] %u bytes (real size)\n", open_data->z_length); ---*/
            }
#else /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
            status = TFFS_Read(open_data, open_data->id, open_data->z_Buffer, &open_data->z_length);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/

            if(status) {
                if(open_data->panic_mode == 0) {
                    if(!tffs_panic_mode && open_data->z_Buffer) {
#if defined(CONFIG_TFFS_CRYPT)
                        vfree(open_data->z_Buffer - 4);
#else /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
                        vfree(open_data->z_Buffer);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
                        open_data->z_Buffer = NULL;
                    }
                }
                TFFS_Close(filp->private_data);
                filp->private_data = NULL;
                if(!tffs_panic_mode)up(&tffs_sema);
                DBG((KERN_INFO "%s: read failed, no file\n", MODULE_NAME));
                return status;
            }
#if defined(TFFS_DEBUG)
            dump_buffer("TFFS", open_data->z_length, open_data->z_Buffer);
#endif /*--- #if defined(TFFS_DEBUG) ---*/

            open_data->stream.next_in   = open_data->z_Buffer;
            open_data->stream.avail_in  = open_data->z_length;
            open_data->stream.total_in  = 0;

            open_data->stream.next_out  = NULL;
            open_data->stream.avail_out = 0;
            open_data->stream.total_out = 0;

            open_data->stream.workspace = vmalloc(workspacesize);
            if(open_data->stream.workspace == NULL) {
                DBG((KERN_ERR "%s: tffs_open: no memory for (read) workspace\n", MODULE_NAME));
                if(open_data->panic_mode == 0) {
                    if(!tffs_panic_mode && open_data->z_Buffer) {
#if defined(CONFIG_TFFS_CRYPT)
                        vfree(open_data->z_Buffer - 4);
#else /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
                        vfree(open_data->z_Buffer);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
                        open_data->z_Buffer = NULL;
                    }
                }
                TFFS_Close(filp->private_data);
                filp->private_data = NULL;
                if(!tffs_panic_mode) up(&tffs_sema);
                return -ENOMEM;
            } else {
                DBG((KERN_INFO "%s: tffs_open: %u bytes (read) workspace (0x%p)\n", MODULE_NAME, workspacesize, open_data->stream.workspace));
            }

            status = zlib_inflateInit(&open_data->stream);
            if(status != Z_OK) {
                DBG((KERN_ERR "%s: tffs_open: zlib_inflateInit failed, status = %d\n", MODULE_NAME, status));
            } else {
                open_data->init_flag        = 1;
            }
        /*----------------------------------------------------------------------------------*\
        \*----------------------------------------------------------------------------------*/
        }
    }

    DBG((KERN_INFO "%s: tffs_open: TFFS_Open success flags=0x%x\n", MODULE_NAME, filp->f_flags));
    if(!tffs_panic_mode) up(&tffs_sema);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int tffs_flush(struct file *filp, fl_owner_t id) {

    int status = 0;
    struct _tffs_open *open_data = (struct _tffs_open *)filp->private_data;

    if(!open_data) return -EFAULT;

    if(open_data->id) {
        if(filp->f_flags & O_WRONLY) {

            DBG((KERN_INFO "{%s} Z_SYNC_FLUSH: zlib_deflate(total_in=%u next_in=0x%p avail_in=%u total_out=%u next_out=0x%p avail_out=%u): status = %d\n", 
                    __func__,
                    (int)open_data->stream.total_in, open_data->stream.next_in, (int)open_data->stream.avail_in,
                    (int)open_data->stream.total_out, open_data->stream.next_out, (int)open_data->stream.avail_out,
                    status));

            status = zlib_deflate(&open_data->stream, Z_SYNC_FLUSH);

            /*--- if ((status != Z_OK) || ((status == Z_OK) && ( ! open_data->stream.avail_out))) { ---*/
            if (status != Z_OK) {
                status = -ENOSPC;
            }
        }
    }

    return status;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int tffs_release(struct inode *inode, struct file *filp) {
    int status = 0;
    struct _tffs_open *open_data = (struct _tffs_open *)filp->private_data;

    if(!open_data) return -EFAULT;

    if(open_data->id) {
        if(filp->f_flags & O_WRONLY) {
            open_data->stream.next_in   = NULL;
            open_data->stream.avail_in  = 0;
            status = zlib_deflate(&open_data->stream, Z_FINISH);

            DBG((KERN_INFO "{%s} Z_FINISH: zlib_deflate(total_in=%u next_in=0x%p avail_in=%u total_out=%u next_out=0x%p avail_out=%u): status = %d\n", 
                    __func__,
                    (int)open_data->stream.total_in, open_data->stream.next_in, (int)open_data->stream.avail_in,
                    (int)open_data->stream.total_out, open_data->stream.next_out, (int)open_data->stream.avail_out,
                    status));

            if(status == Z_STREAM_END) {
                zlib_deflateEnd(&open_data->stream);

                if(!tffs_panic_mode && down_interruptible(&tffs_sema)) {
                    printk(KERN_ERR "%s tffs_release: down_interruptible() failed\n", MODULE_NAME);
                } else {
                    DBG((KERN_INFO "%s: tffs_release: write %u bytes for id 0x%x\n", MODULE_NAME, (int)open_data->stream.total_out, open_data->id));

#if defined(TFFS_DEBUG)
                    dump_buffer("TFFS", open_data->stream.total_out, open_data->z_Buffer);
#endif /*--- #if defined(TFFS_DEBUG) ---*/

#if defined(CONFIG_TFFS_CRYPT)
                    {
                        unsigned int i;
                        struct _aes_block {
                            unsigned int data[4];
                        };
                        struct _aes_block *in  = (struct _aes_block *)open_data->z_Buffer;
                        struct _aes_block *out = (struct _aes_block *)open_data->z_Buffer;
                        struct _aes_block prev_c;
                        struct _aes_block strich;

                        /*--- printk("[tffs-encrypt] %lu bytes (real size)\n", open_data->stream.total_out); ---*/

                        ((unsigned int *)open_data->z_Buffer)[-1] = open_data->stream.total_out;

                        while(open_data->stream.total_out % sizeof(struct _aes_block))
                            open_data->stream.total_out++;

                        /*--- printk("[tffs-encrypt] %lu bytes (after align)\n", open_data->stream.total_out); ---*/
                        /*--- tffs_dump_block(open_data->z_Buffer, open_data->stream.total_out); ---*/

                        memset(&prev_c, 0, sizeof(prev_c));
                        for(i = 0 ; i < (open_data->stream.total_out >> 4) ; i++) {
                            strich.data[0] = in->data[0] ^ prev_c.data[0];
                            strich.data[1] = in->data[1] ^ prev_c.data[1];
                            strich.data[2] = in->data[2] ^ prev_c.data[2];
                            strich.data[3] = in->data[3] ^ prev_c.data[3];

                            aes_encrypt(&(open_data->tfm), out, &strich);

                            prev_c = *out;
                            out++, in++;
                        }

                        open_data->stream.total_out += 4;
                        /*--- printk("[tffs-encrypt] %lu bytes (real write size)\n", open_data->stream.total_out); ---*/
                        /*--- tffs_dump_block(open_data->z_Buffer - 4, open_data->stream.total_out); ---*/
                    }
                    TFFS_Write(open_data, open_data->id, open_data->z_Buffer - 4, open_data->stream.total_out, open_data->panic_mode);
#else /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
                    TFFS_Write(open_data, open_data->id, open_data->z_Buffer, open_data->stream.total_out, open_data->panic_mode);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
                    if(!tffs_panic_mode) up(&tffs_sema);
                }
                status = 0; /*--- kein Fehler ---*/
            } else {
                status = -ENOSPC;
            }
        } else {
            zlib_inflateEnd(&open_data->stream);
        }
        if(!tffs_panic_mode) {
            if(open_data->stream.workspace) {
                vfree(open_data->stream.workspace);
                open_data->stream.workspace = NULL;
            }
            if(open_data->z_Buffer) {
#if defined(CONFIG_TFFS_CRYPT)
                vfree(open_data->z_Buffer - 4);
#else /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
                vfree(open_data->z_Buffer);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
                open_data->z_Buffer = NULL;
            }
        }
    }

    TFFS_Close(filp->private_data);
    filp->private_data = NULL;
    return status;
}

