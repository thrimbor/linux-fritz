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
#ifndef _tffs_local_h_
#define _tffs_local_h_

#define TFFS_BLOCK_MTD

#ifdef TFFS_BLOCK_MTD
#include <linux/fs.h>
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/
#include <linux/zlib.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_TFFS_CRYPT)
#include <linux/crypto.h>
extern unsigned char *tffs_fs_key;
#endif /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
/*--- #define TFFS_DEBUG ---*/

extern unsigned int tffs_panic_mode;
#if defined(CONFIG_TFFS_PANIC_LOG) && (defined(CONFIG_VR9) || defined(CONFIG_AR10))
#define SPI_PANIC_LOG
#endif


#if defined(TFFS_DEBUG)
#define DBG(a)       printk a
#else /*--- #if defined(TFFS_DEBUG) ---*/
#define DBG(a)  
#endif /*--- #else ---*/ /*--- #if defined(TFFS_DEBUG) ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#define MODULE_NAME             "tffs"
#define MAX_ENV_ENTRY           256
#if defined(FLASH_ENV_ENTRY_SIZE)
    #undef FLASH_ENV_ENTRY_SIZE
#endif /*--- #if defined(FLASH_ENV_ENTRY_SIZE) ---*/
#define FLASH_ENV_ENTRY_SIZE    256

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
struct _tffs {
    int                        major;
    int                        minor;
    int                        anzahl;
    dev_t                      device;
    struct cdev               *cdev;
    struct cdev               *cdev_ticfg;
    /*--- devfs_handle_t devfs_handle; ---*/ /* FIXME To be deleted */
    /*--- devfs_handle_t devfs_ticfg_handle; ---*/
	/*--- devfs_handle_t devfs_dir_handle; ---*/
};
extern struct _tffs tffs;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _tffs_thread_state {
    tffs_thread_state_off,
    tffs_thread_state_init,
    tffs_thread_state_idle,
    tffs_thread_state_process,
    tffs_thread_state_down
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern struct semaphore tffs_sema;
extern int tffs_thread_event;
extern enum _tffs_thread_state tffs_thread_state;
extern unsigned int tffs_request_count;
extern int tffs_mtd[2];
extern unsigned int tffs_spi_mode;

#define PANIC_LOG_BUF_SIZE 268000
extern char *panic_log_buf;


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _tffs_open {
    unsigned int init_flag;
    unsigned int id;
    unsigned int panic_mode;
    unsigned char *z_Buffer;
    unsigned int z_length;
    z_stream stream;
#if defined(CONFIG_TFFS_CRYPT)
    unsigned int key_len;
#ifndef CONFIG_DAVINCI_DRM_KUNDENVERSION
    unsigned char key[16];
#else /*--- #ifndef CONFIG_DAVINCI_DRM_KUNDENVERSION ---*/
    unsigned char *key;
#endif /*--- #else ---*/ /*--- #ifndef CONFIG_DAVINCI_DRM_KUNDENVERSION ---*/
    struct crypto_tfm tfm;
    unsigned int workspace[140];
#endif /*--- #if defined(CONFIG_TFFS_CRYPT) ---*/
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_urlader_env_get_value(char *var);
int avm_urlader_env_unset_variable(char *var);
int avm_urlader_env_set_variable(char *var, char *val);
int avm_urlader_env_defrag(void);
char *avm_urlader_env_get_value_by_id(unsigned int id);
char *avm_urlader_env_get_variable(int idx);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int tffs_ioctl(struct inode *inode, struct file *filp, unsigned int ioctl_param, unsigned long argv);
ssize_t tffs_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *offp);
ssize_t tffs_read(struct file *filp, char *read_buffer, size_t max_read_length, loff_t *offp);
int tffs_open(struct inode *inode, struct file *filp);
int tffs_flush(struct file *filp, fl_owner_t id);
int tffs_release(struct inode *inode, struct file *filp);
int tffs_lock(void);
void tffs_unlock(void);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_PROC_FS
int tffs_read_proc ( char *page, char **start, off_t off,int count, int *eof, void *data_unused);
int tffs_write_proc(struct file *file, const char *buffer, unsigned long count, void *data);
#endif /*--- #ifdef CONFIG_PROC_FS ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_SYSCTL
int avm_urlader_env_init(void);
#endif /* CONFIG_SYSCTL */

#ifdef TFFS_BLOCK_MTD
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _tffs_bdev {
    uint8_t     use_bdev;
    size_t      size;
    char        path[64];
    struct file *filep[2];
};
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/

#include <linux/mtd/mtd.h>
struct tffs_info {
    uint8_t              use_bdev;
    size_t               size;
    union {
        struct mtd_info     *mtd;
        struct file         *file;
    } tffs;
};

#endif /*--- #ifndef _tffs_local_h_ ---*/
