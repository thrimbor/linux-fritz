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
#ifdef CONFIG_SMP
#define __SMP__
#endif /*--- #ifdef CONFIG_SMP ---*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/mtd/mtd.h>
#include <linux/tffs.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/zlib.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/io.h>

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
/*--- #define TFFS_DEBUG ---*/
#include "tffs_local.h"

extern unsigned int tffs_panic_mode;

#ifdef SPI_PANIC_LOG
int tffs_spi_read(unsigned int address, unsigned int mtd_id, unsigned char *pdata, unsigned int len);
int tffs_spi_write(unsigned int address, unsigned int mtd_id, unsigned char *pdata, unsigned int len);

#if defined(CONFIG_VR9) || defined(CONFIG_AR10)
    #define SPI_PANIC_MODE      (tffs_panic_mode && tffs_spi_mode)
#endif /*--- #if defined(CONFIG_VR9) ---*/

#endif /*--- #ifdef(SPI_PANIC_LOG) ---*/
#ifdef TFFS_BLOCK_MTD
extern struct _tffs_bdev tffs_bdev;
static unsigned int TFFS_Init_done = 0;
#endif
 
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static unsigned int TFFS_mtd_number = (unsigned int)-1;
static unsigned int TFFS_mtd_number2 = (unsigned int)-1;

#define MAX_PAR 8

static struct _TFFS_Entry *TFFS_Global_Index[FLASH_FS_ID_LAST];
static unsigned int TFFS_Global_Index_created = 0;
struct tffs_info TFFS_mtd;
unsigned int current_mtd = 0;
unsigned int avail_mtd[2];
int tffs_mtd_offset[2];
unsigned char *TFFS_Cleanup_Buffer = NULL;
struct semaphore tffs_mtd_sema;
unsigned int tffs_written;

static DEFINE_MUTEX(create_index_lock);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
__inline int MTD_READ(struct tffs_info *local_TFFS, loff_t from, size_t len, size_t *retlen, u_char *buf) {
#ifdef TFFS_BLOCK_MTD
	mm_segment_t fs;

    if(local_TFFS->use_bdev) {
        if(!local_TFFS->tffs.file) {
            printk(KERN_ERR "[MTD_READ] filep is NULL\n");
            return -EFAULT;
        }

	    fs = get_fs();
	    set_fs(KERNEL_DS);
        if (!local_TFFS->tffs.file->f_op->llseek || !local_TFFS->tffs.file->f_op->read)
            printk(KERN_ERR "[MTD_READ] FAILED =============== llseek %x read %x \n", 
                    (unsigned int)local_TFFS->tffs.file->f_op->llseek, 
                    (unsigned int)local_TFFS->tffs.file->f_op->read);
        local_TFFS->tffs.file->f_op->llseek(local_TFFS->tffs.file, from, SEEK_SET);
        *retlen = local_TFFS->tffs.file->f_op->read(local_TFFS->tffs.file, buf, len, &local_TFFS->tffs.file->f_pos);
        DBG((KERN_INFO "[%s] read 0x%x len 0x%x % *B\n", __FUNCTION__, *retlen, len, *retlen, buf));
	    set_fs(fs);
        return (*retlen != len);
    }
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/

    if((local_TFFS == NULL) || local_TFFS->tffs.mtd->read == NULL) {
        printk("[MTD_READ] mtd_info/mtd_info->read is NULL\n");
        return -EFAULT;
    }
#ifdef SPI_PANIC_LOG
    if(SPI_PANIC_MODE) {
        *retlen = tffs_spi_read((unsigned int)from, ((current_mtd == avail_mtd[0]) ? 0 : 1), buf, len);
        return 0;
    }
#endif
    return (*local_TFFS->tffs.mtd->read)(local_TFFS->tffs.mtd, from, len, retlen, buf);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
__inline int MTD_WRITE(struct tffs_info *local_TFFS, loff_t from, size_t len, size_t *retlen, u_char *buf) {
#ifdef SPI_PANIC_LOG
    if(SPI_PANIC_MODE) {
        *retlen = tffs_spi_write((unsigned int)from, ((current_mtd == avail_mtd[0]) ? 0 : 1), buf, len);
        return 0;
    }
#endif /*--- #ifdef SPI_PANIC_LOG ---*/

#ifdef TFFS_BLOCK_MTD
    {
        mm_segment_t fs;

        if(local_TFFS->use_bdev) {
            if(!local_TFFS->tffs.file) {
                printk(KERN_ERR "[MTD_WRITE] filep is NULL\n");
                return -EFAULT;
            }

            fs = get_fs();
            set_fs(KERNEL_DS);
            if (!local_TFFS->tffs.file->f_op->llseek || !local_TFFS->tffs.file->f_op->write)
                printk(KERN_ERR "[MTD_WRITE] FAILED =============== llseek %x write %x \n", 
                        (unsigned int)local_TFFS->tffs.file->f_op->llseek, 
                        (unsigned int)local_TFFS->tffs.file->f_op->write);
            local_TFFS->tffs.file->f_op->llseek(local_TFFS->tffs.file, from, SEEK_SET);
            *retlen = local_TFFS->tffs.file->f_op->write(local_TFFS->tffs.file, buf, len, &local_TFFS->tffs.file->f_pos);
            if (*retlen != len )
                printk(KERN_ERR "[%s] Tried to write %d, but only %d were written \n", __FUNCTION__, *retlen, len);
            DBG((KERN_INFO "[%s] write 0x%x len 0x%x % *B\n", __FUNCTION__, *retlen, len, *retlen, buf));
            set_fs(fs);
            return (*retlen != len);
        }
    }
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/

    if((local_TFFS == NULL) || (local_TFFS->tffs.mtd->write == NULL)) {
        printk(KERN_ERR "[MTD_WRITE] mtd_info/mtd_info->write is NULL\n");
        return -EFAULT;
    }
	return (*local_TFFS->tffs.mtd->write)(local_TFFS->tffs.mtd, from, len, retlen, buf);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline int get_mtd_device_wrapped(int num, struct tffs_info *mtd) {
    if(tffs_bdev.use_bdev) {
        if (tffs_mtd[0] == num) {
            mtd->tffs.file = tffs_bdev.filep[0];
        }
        if (tffs_mtd[1] == num) {
            mtd->tffs.file = tffs_bdev.filep[1];
        }
    } else {
        mtd->tffs.mtd = get_mtd_device(NULL, num);
        if (IS_ERR(mtd->tffs.mtd))
            return -1;
        mtd->size = mtd->tffs.mtd->size;
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void put_mtd_device_wrapped(struct tffs_info *mtd) {
    if(!mtd->use_bdev) {
        put_mtd_device(mtd->tffs.mtd);
    }
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
unsigned int TFFS_Init(unsigned int mtd_number, unsigned int mtd_number2) {

    struct tffs_info *p_local_TFFS = &TFFS_mtd;

#ifdef TFFS_BLOCK_MTD
    if (TFFS_Init_done) {
        return 0;
    }

    if(tffs_bdev.use_bdev) {
        if (tffs_bdev.size == 0) {
            panic("TFFS: no valid tffs size");
        }
        p_local_TFFS->use_bdev = 1;
        p_local_TFFS->size = tffs_bdev.size;

        tffs_bdev.path[strlen(tffs_bdev.path) - 1] = '1';
        printk(KERN_ERR "[%s] use_bdev file='%s'\n", __FUNCTION__, tffs_bdev.path);
        tffs_bdev.filep[0] = filp_open(tffs_bdev.path, O_RDWR | O_SYNC, 0);
        if(IS_ERR(tffs_bdev.filep[0])) {
            printk(KERN_ERR "[%s] '%s' file open failed (%d) -> trying later\n", __FUNCTION__, tffs_bdev.path, (int)tffs_bdev.filep[0]);
            tffs_bdev.filep[0] = NULL;
            return 1;
        } 
        printk(KERN_ERR "[%s] file open ok\n", __FUNCTION__);
        tffs_bdev.path[strlen(tffs_bdev.path) - 1] = '2';
        printk(KERN_ERR "[%s] use_bdev file='%s'\n", __FUNCTION__, tffs_bdev.path);
        tffs_bdev.filep[1] = filp_open(tffs_bdev.path, O_RDWR | O_SYNC, 0);
        if(IS_ERR(tffs_bdev.filep[1])) {
            printk(KERN_ERR "[%s] '%s' file open failed (%d) -> trying later\n", __FUNCTION__, tffs_bdev.path, (int)tffs_bdev.filep[1]);
            tffs_bdev.filep[1] = NULL;
            return 1;
        }
        printk(KERN_ERR "[%s] file open ok\n", __FUNCTION__);
    } 
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/

    DBG((KERN_INFO "TFFS_Init(mtd%u, mtd%u)\n", mtd_number, mtd_number2));
    TFFS_mtd_number = mtd_number;
    TFFS_mtd_number2 = mtd_number2;

    sema_init(&tffs_mtd_sema, 1);

    if (get_mtd_device_wrapped(TFFS_mtd_number, p_local_TFFS)) {
        DBG((KERN_ERR "TFFS_Init: can't get mtd%u\n", TFFS_mtd_number));
        return (unsigned int)-ENXIO;
    }

    /*------------------------------------------------------------------------------------------*\
     * prüfen welcher buffer der "richtige" ist
    \*------------------------------------------------------------------------------------------*/
    {
        int ret, retlen;
        unsigned int segment[2] = { 0, 0 };
        union _tffs_segment_entry u;

        // TODO: potentieller Lesefehler wird nicht bearbeitet
        ret = MTD_READ(p_local_TFFS, 0, sizeof(union _tffs_segment_entry), &retlen, u.Buffer);
        if(ret) {
            DBG((KERN_ERR "MTD read failed on [%s:%d] could not read a complete _tffs_segment_entry\n", __func__, __LINE__ - 2));
        }

        if(u.Entry.ID == FLASH_FS_ID_SEGMENT) {
            segment[0] = TFFS_GET_SEGMENT_VALUE(&u);
            DBG((KERN_INFO "double_buffer(0): segment value %u\n", segment[0]));
        } else {
            DBG((KERN_INFO "double_buffer(0): no SEGMENT VALUE (0x%x)\n", u));
        }

        put_mtd_device_wrapped(p_local_TFFS);
        if (get_mtd_device_wrapped(TFFS_mtd_number2, p_local_TFFS)) {
            DBG((KERN_ERR "TFFS_Init: can't get mtd%u\n", TFFS_mtd_number2));
            return (unsigned int)-ENXIO;
        }
        current_mtd = TFFS_mtd_number2;

        // TODO: potentieller Lesefehler wird nicht bearbeitet
        ret = MTD_READ(p_local_TFFS, 0, sizeof(union _tffs_segment_entry), &retlen, u.Buffer);
        if(ret) {
            DBG((KERN_ERR "MTD read failed on [%s:%d] could not read a complete _tffs_segment_entry\n", __func__, __LINE__ - 2));
        }

        if(u.Entry.ID == FLASH_FS_ID_SEGMENT) {
            segment[1] = TFFS_GET_SEGMENT_VALUE(&u);
            DBG((KERN_INFO "double_buffer(1): segment value %u\n", segment[1]));
        } else {
            DBG((KERN_INFO "double_buffer(1): no SEGMENT VALUE (0x%x)\n", u));
        }

        if(segment[0] > segment[1]) {
            put_mtd_device_wrapped(p_local_TFFS);
            get_mtd_device_wrapped(TFFS_mtd_number, p_local_TFFS);
            current_mtd = TFFS_mtd_number;
        }
        if(segment[0] == 0 && segment[1] == 0) {
            panic("TFFS: no valid filesystem");
        }
        avail_mtd[0] = TFFS_mtd_number;
        avail_mtd[1] = TFFS_mtd_number2;
        DBG((KERN_INFO "double_buffer: use segment %u (avail: %u + %u)\n", current_mtd, avail_mtd[0], avail_mtd[1]));
    }

#ifdef TFFS_BLOCK_MTD
    if(tffs_bdev.use_bdev) {
        DBG((KERN_INFO "Not using a mtd partition -> using a direct partition.. size = 0x%x\n", tffs_bdev.size));
        TFFS_Init_done = 1;
    } else
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/
        DBG((KERN_INFO "mtd%u size=0x%x\n", TFFS_mtd_number, TFFS_mtd.size));

    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void TFFS_Deinit(void) {
    DBG((KERN_INFO "TFFS_Deinit()\n"));
	if(!TFFS_mtd.use_bdev)
	    put_mtd_device_wrapped(&TFFS_mtd);
    TFFS_mtd_number   = (unsigned int)-1;
	TFFS_mtd.tffs.mtd = NULL;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
DECLARE_WAIT_QUEUE_HEAD(tffs_block_wait);
void *TFFS_Open(void) {
    struct _tffs_open *TFFS;

#ifdef TFFS_BLOCK_MTD
    if(tffs_bdev.use_bdev && !tffs_bdev.filep[0]) {
        printk(KERN_ERR "[%s] tffs file not ready -> waiting ...\n", __FUNCTION__);

        wait_event(tffs_block_wait, !TFFS_Init(tffs_mtd[0], tffs_mtd[1]));

        printk(KERN_ERR "[%s] wakeup\n", __FUNCTION__);
        wake_up(&tffs_block_wait);
    }
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/

    TFFS = kmalloc(sizeof(struct _tffs_open), GFP_KERNEL);
    if(TFFS == NULL) {
        DBG((KERN_ERR "TFFS_Open: malloc(%u) failed\n", sizeof(struct _tffs_open)));
        return 0;
    }
    memset(TFFS, 0, sizeof(struct _tffs_open));

#ifdef TFFS_BLOCK_MTD
    if(tffs_bdev.use_bdev) {
        DBG((KERN_INFO "TFFS_Open: mtd open success (direct partition) size = 0x%x\n", tffs_bdev.size));
    } else 
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/
        DBG((KERN_INFO "TFFS_Open: mtd open success (size 0x%x flags 0x%x)\n", TFFS_mtd.size, TFFS_mtd.tffs.mtd->flags));
	
	/*------------------------------------------------------------*\
	 * Create_Index ist nicht reentrant!
	\*------------------------------------------------------------*/
	mutex_lock( &create_index_lock );
    if (TFFS_Global_Index_created == 0) {
        TFFS_Create_Index();
        TFFS_Global_Index_created = 1;
    }
	mutex_unlock( &create_index_lock );
	/*------------------------------------------------------------*\
	\*------------------------------------------------------------*/

    DBG((KERN_INFO "TFFS_Open: handle: 0x%x\n", (int)TFFS));

    return TFFS;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void TFFS_Close(void *handle) {
    struct _tffs_open *TFFS = (struct _tffs_open *)handle;
    DBG((KERN_INFO "TFFS_Close(0x%x):", (int)handle));

#ifdef TFFS_BLOCK_MTD
    if(tffs_bdev.use_bdev) {
        if(!tffs_bdev.filep[0]) {
            printk("[%s] mtd_info->read is NULL\n", __FUNCTION__);
            kfree(TFFS);
            return;
        }
        tffs_bdev.filep[0]->f_op->fsync(tffs_bdev.filep[0], tffs_bdev.filep[0]->f_dentry, 0xff);
        tffs_bdev.filep[1]->f_op->fsync(tffs_bdev.filep[1], tffs_bdev.filep[1]->f_dentry, 0xff);
    }
    else 
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/
        if( TFFS_mtd.tffs.mtd && TFFS_mtd.tffs.mtd->sync ) 
            TFFS_mtd.tffs.mtd->sync(TFFS_mtd.tffs.mtd);
    

    kfree(TFFS);
    DBG((KERN_INFO " success\n"));
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void TFFS_Format_Callback(struct erase_info *instr) {
	DBG(("TFFS: mtd_erase_callback\n"));
    switch(instr->state) {
        case MTD_ERASE_PENDING:
            break;
        case MTD_ERASING:
            break;
        case MTD_ERASE_SUSPEND:
            break;
        case MTD_ERASE_FAILED:
        case MTD_ERASE_DONE:
	        wake_up((wait_queue_head_t *)instr->priv);
            break;
    }
    return;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#ifdef TFFS_BLOCK_MTD
static char erase_buff[1024];
#endif
unsigned int TFFS_Format(void *handle, struct tffs_info *mtd) {
	int ret=0;

#ifdef TFFS_BLOCK_MTD
    if(mtd->use_bdev) {
        int i;
        size_t retlen;

        memset(erase_buff, 0xff, 1024);
        if(!mtd->tffs.file) {
            printk("[MTD_READ] mtd_info->read is NULL\n");
            return -EFAULT;
        }
        for(i = 0; i < mtd->size / 1024; i++)
            MTD_WRITE(mtd, i*1024, 1024, &retlen, erase_buff);
        if(tffs_bdev.size % 1024) 
            MTD_WRITE(mtd, i*1024, mtd->size % 1024, &retlen, erase_buff);
    }
    else 
#endif /*--- #ifdef TFFS_BLOCK_MTD ---*/
    {
        struct erase_info *erase;

        DECLARE_WAITQUEUE(wait,current);
        wait_queue_head_t wait_q;

        init_waitqueue_head(&wait_q);

        erase = (struct erase_info *)kmalloc(sizeof(struct erase_info), GFP_KERNEL);
        if(erase == NULL) {
            DBG((KERN_ERR "TFFS_Format: malloc(%u) failed\n", sizeof(struct erase_info)));
            return (unsigned int)-ENOMEM;
        }

        DBG((KERN_INFO "TFFS_Format: malloc(%u) success\n", sizeof(struct erase_info)));
        memset(erase, 0, sizeof(struct erase_info));

        erase->mtd      = mtd->tffs.mtd;
        erase->addr     = 0;
        erase->len      = mtd->size;
        erase->callback = TFFS_Format_Callback;
        erase->priv     = (u_long)&wait_q;
        erase->next     = NULL;

        DBG((KERN_INFO "TFFS_Format(handle=0x%x): erase: addr %x len %x\n", (unsigned int)handle, erase->addr, erase->len));

        ret = mtd->tffs.mtd->erase(mtd->tffs.mtd, erase);

        DBG((KERN_INFO "TFFS_Format(handle=0x%x): erase: ret 0x%x\n", (unsigned int)handle, ret));
        if( !ret ) {
            set_current_state(TASK_UNINTERRUPTIBLE);
            add_wait_queue( &wait_q, &wait);
            if( erase->state != MTD_ERASE_DONE && erase->state != MTD_ERASE_FAILED) {
                schedule();
            }

            remove_wait_queue(&wait_q, &wait);
            set_current_state(TASK_RUNNING);

            ret = (erase->state == MTD_ERASE_FAILED) ? -EIO : 0;
            if(ret) {
                DBG((KERN_ERR "Failed (callback) to erase mtd, region [0x%x, 0x%x]\n", erase->addr,erase->len));
            }
        } else {
            DBG((KERN_ERR "Failed to erase mtd, region [0x%x, 0x%x]\n", erase->addr,erase->len));
            ret = -EIO;
        }
        kfree(erase);
    }
    return (unsigned int)ret;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
unsigned int TFFS_Clear(void *handle, enum _tffs_id id) {
    return TFFS_Write(handle, id, NULL, 0, 0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int TFFS_Werkseinstellungen(void *handle) {
    enum _tffs_id id;
    unsigned int count = 0;
    DBG((KERN_INFO "TFFS_Werkseinstellungen(0x%x)\n", (int)handle));
    for(id = FLASH_FS_ID_TICFG ; id <= FLASH_FS_ID_FIRMWARE_CONFIG_LAST ; id++) {
        if(TFFS_Global_Index[id] != (struct _TFFS_Entry *)FLASH_FS_ID_FREE) {
            unsigned int error = TFFS_Write(handle, id, NULL, 0, 0);
            if(error) {
                DBG((KERN_INFO "TFFS_Werkseinstellungen(0x%x): clear id 0x%x failed (%u cleared)\n", (int)handle, (int)id, count));
                return error;
            }
            count++;
        }
    }
    DBG((KERN_INFO "TFFS_Werkseinstellungen(0x%x): success (%u cleared)\n", (int)handle, count));
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _tffs_memcmp {
    tffs_memcmp_equal,
    tffs_memcmp_writeable,
    tffs_memcmp_clear_required
};

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static enum _tffs_memcmp TFFS_Memcmp(unsigned char *FlashMemory, unsigned char *NewMemory, unsigned int Length) {
    unsigned int ret;
    unsigned short *_Flash     = (unsigned short *)FlashMemory;
    unsigned short *_neuerWert = (unsigned short *)NewMemory;
    
    ret = memcmp(FlashMemory, NewMemory, Length);
    if(ret == 0)
        return tffs_memcmp_equal;

#if 1
    Length = (Length + 1) >> 1;
    while(Length--) {
        if((((*_Flash++) | ((~(*_neuerWert++)) & 0xFFFF)) != 0xFFFF)) {
            return tffs_memcmp_clear_required;
        }
    }
    return tffs_memcmp_writeable;
#else
    return tffs_memcmp_clear_required;
#endif
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
unsigned int TFFS_Write(void *handle, enum _tffs_id Id, unsigned char *write_buffer, unsigned int write_length, unsigned int level) {
    int ret, retlen;
    struct _TFFS_Entry *E, Entry;
    struct _TFFS_Entry *E_ToClear = NULL, Entry_ToClear;
    unsigned int Len = 0;

    if(Id >= FLASH_FS_ID_LAST) {
        printk(KERN_ERR"%s: invalid tffs_id: 0x%x\n", __func__, Id);
        return (unsigned int)-ENOENT;
    }
    if(!tffs_panic_mode) down(&tffs_mtd_sema);

    E = (struct _TFFS_Entry *)0;
    while((unsigned int)E + sizeof(struct _TFFS_Entry) + write_length < TFFS_mtd.size) {
        ret = MTD_READ(&TFFS_mtd, (unsigned long)E, sizeof(struct _TFFS_Entry), &retlen, (unsigned char *)&Entry);
        if(ret) {
            if(!tffs_panic_mode) up(&tffs_mtd_sema);
            return ret;
        }

        /*---------------------------------------------------------------------------------------*\
         * ID gefunden
        \*---------------------------------------------------------------------------------------*/
        if(Entry.ID == (unsigned short)Id && Id < FLASH_FS_DROPABLE_DATA) { /*---  Alter Eintrag gefunden ----*/
            DBG((KERN_INFO "alter Eintrag mit ID=%x gefunden ", Id));

            /*-----------------------------------------------------------------------------------*\
             * prüfen ob eintrag identisch oder überschreibbar
            \*-----------------------------------------------------------------------------------*/
            if(Entry.Length == write_length) {
                unsigned char *buff = kmalloc(Entry.Length, GFP_KERNEL);
                if(!buff) {
                    DBG((KERN_ERR "TFFS_Write: malloc(%u) failed\n", Entry.Length));
                    if(!tffs_panic_mode) up(&tffs_mtd_sema);
                    return (unsigned int)-ENOMEM;
                }
                DBG((KERN_INFO "TFFS_Write: malloc(%u) success\n", Entry.Length));
                ret = MTD_READ(&TFFS_mtd, (unsigned long)E + sizeof(struct _TFFS_Entry), Entry.Length, &retlen, buff);
                if(ret) {
                    kfree(buff);
                    DBG((KERN_ERR "TFFS_Write: MTD_READ failed\n"));
                    if(!tffs_panic_mode) up(&tffs_mtd_sema);
                    return ret;
                }

                switch(TFFS_Memcmp(buff, write_buffer, Entry.Length)) {
                    case tffs_memcmp_equal:
                        DBG((KERN_INFO "alter Eintrag mit neuem identisch\n"));
                        kfree(buff);
                        if(!tffs_panic_mode) up(&tffs_mtd_sema);
                        return 0;

#if 1
                    case tffs_memcmp_writeable:
                        DBG((KERN_INFO "alter Eintrag durch neuen überschreibbar\n"));
                        ret = MTD_WRITE(&TFFS_mtd, (unsigned long)E + sizeof(struct _TFFS_Entry), Entry.Length, &retlen, write_buffer);
                        kfree(buff);
                        if(!tffs_panic_mode) up(&tffs_mtd_sema);
                        return ret;
#endif

                    default:
                    case tffs_memcmp_clear_required:
                        break;
                }
                kfree(buff);
            }

            /*-----------------------------------------------------------------------------------*\
             * gefundenen Eintrag auf FLASH_FS_ID_SKIP setzen
            \*-----------------------------------------------------------------------------------*/
            Entry.ID     = FLASH_FS_ID_SKIP;
            if(write_buffer == NULL) {
                ret = MTD_WRITE(&TFFS_mtd, (unsigned long)E, sizeof(struct _TFFS_Entry), &retlen, (unsigned char *)&Entry);
                if(ret || (retlen != sizeof(struct _TFFS_Entry))) {
                    DBG((KERN_ERR "TFFS: write TFFS_Entry (id=SKIP) failed\n"));
                    if(!tffs_panic_mode) up(&tffs_mtd_sema);
                    return ret || (unsigned int)-EIO;
                }

                DBG((KERN_INFO "geloescht\n"));

                TFFS_Global_Index[Id] = (struct _TFFS_Entry *)FLASH_FS_ID_FREE;
            } else {
                E_ToClear = E;
                Entry_ToClear = Entry;
            }
            
            /*-----------------------------------------------------------------------------------*\
             * sollte es keine Daten für neuen satz geben dann sind wir fertig
            \*-----------------------------------------------------------------------------------*/
            if(write_buffer == NULL) {
                DBG((KERN_INFO "(1) nur loeschen kein neuer Eintrag\n"));
                if(!tffs_panic_mode) up(&tffs_mtd_sema);
                return 0;
            }
        }
        /*---------------------------------------------------------------------------------------*\
         * freie ID gefunden
        \*---------------------------------------------------------------------------------------*/
        if(Entry.ID == FLASH_FS_ID_FREE) { /*---  Freier Eintrag gefunden ----*/
            DBG((KERN_INFO "freier Eintrag gefunden\n"));
            if(write_buffer == NULL) {
                DBG((KERN_INFO "(2) nur loeschen kein neuer Eintrag\n"));
                if(!tffs_panic_mode) up(&tffs_mtd_sema);
                return 0;
            }
            Entry.ID     = (unsigned short)Id;
            Entry.Length = (unsigned short)write_length;
            TFFS_Global_Index[Entry.ID] = E;

            tffs_written++;
            ret = MTD_WRITE(&TFFS_mtd, (unsigned long)E, sizeof(struct _TFFS_Entry), &retlen, (unsigned char *)&Entry);
            if(ret || (retlen != sizeof(struct _TFFS_Entry))) {
                DBG((KERN_ERR "TFFS: write TFFS_Entry (id=0x%x, ptr 0x%x %u bytes) failed, reason %d\n", Id, (unsigned int)E, sizeof(struct _TFFS_Entry), ret));
                if(!tffs_panic_mode) up(&tffs_mtd_sema);
                return ret ? ret : (unsigned int)-EIO;
            }
            DBG((KERN_INFO "header mit id %x geschrieben\n", Id));

            ret = MTD_WRITE(&TFFS_mtd, (unsigned long)E + sizeof(struct _TFFS_Entry), Entry.Length, &retlen, write_buffer);
            if(ret || (retlen != Entry.Length)) {
                DBG((KERN_ERR "TFFS: write data (id=0x%x, ptr 0x%x %u bytes) failed, reason %d\n", Id, (unsigned int)E + sizeof(struct _TFFS_Entry), Entry.Length, ret));
                if(!tffs_panic_mode) up(&tffs_mtd_sema);
                return ret ? ret : (unsigned int)-EIO;
            }

            DBG((KERN_INFO "%d bytes daten geschrieben\n", write_length));

            if(E_ToClear != NULL) {
                ret = MTD_WRITE(&TFFS_mtd, (unsigned long)E_ToClear, sizeof(struct _TFFS_Entry), &retlen, (unsigned char *)&Entry_ToClear);
                if(ret || (retlen != sizeof(struct _TFFS_Entry))) {
                    DBG((KERN_ERR "TFFS: write TFFS_Entry (id=SKIP) failed\n"));
                    if(!tffs_panic_mode) up(&tffs_mtd_sema);
                    return ret ? ret : (unsigned int)-EIO;
                }

                DBG((KERN_INFO "geloescht (postum) ID=0x%x Len=%u\n", Entry_ToClear.ID, Entry_ToClear.Length));
            }

            /*-------------------------------------------------------------------------*\
                Prüfen ob Filesystem mehr als 75% Voll ist 
            \*-------------------------------------------------------------------------*/
            if(level == 0) {  /*--- recursives cleanup vermeiden, level ist im kernel Mode 1 ---*/
                unsigned int Fuell = (unsigned int)E + Entry.Length; /*--- aktueller Fuellstand ---*/
                Fuell *= 100;
#ifdef TFFS_BLOCK_MTD
                Fuell /= (unsigned int)((tffs_bdev.use_bdev) ? tffs_bdev.size : TFFS_mtd.size);
#else
                Fuell /= (unsigned int)TFFS_mtd.size;
#endif

                DBG((KERN_INFO "fuellstand %u%%\n", Fuell));
                if(Fuell > 75) {
                    printk(KERN_ERR "TFFS: Fuellstand > 75 ... trigger Cleanup\n");
                    tffs_send_event(TFFS_EVENT_CLEANUP);
                }
            }
            if(!tffs_panic_mode) up(&tffs_mtd_sema);
            return 0;
        }
        
        Len = (Entry.Length + 3) & ~0x03;
        E = (struct _TFFS_Entry *)((unsigned int)E + sizeof(struct _TFFS_Entry) + Len);
    }
    if(level == 0) {
        if(!tffs_panic_mode) up(&tffs_mtd_sema);
        if(TFFS_Cleanup(handle) == 0) 
            return TFFS_Write(handle, Id, write_buffer, write_length, 1);
        return (unsigned int)-EIO;
    }
    if(!tffs_panic_mode) up(&tffs_mtd_sema);
    return (unsigned int)-EIO;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
unsigned int TFFS_Read(void *handle, enum _tffs_id Id, unsigned char *read_buffer, unsigned int *read_length) {
    unsigned int retlen, ret;
    struct _TFFS_Entry *E, Entry;
    unsigned int Len;

    if(Id >= FLASH_FS_ID_LAST) {
        printk(KERN_ERR"%s: invalid tffs_id: 0x%x\n", __func__, Id);
        return (unsigned int)-ENOENT;
    }
    down(&tffs_mtd_sema);

    DBG((KERN_INFO "TFFS_Read(handle=%x): id = 0x%x, max_length = %u\n", (unsigned int)handle, (unsigned int)Id, *read_length));

    E = TFFS_Global_Index[Id];
    if(E != (struct _TFFS_Entry *)FLASH_FS_ID_FREE) {
        DBG((KERN_INFO "Eintrag gefunden\n"));
		ret = MTD_READ(&TFFS_mtd, (unsigned long)E, sizeof(struct _TFFS_Entry), &retlen, (unsigned char *)&Entry);
        if(ret) {
            up(&tffs_mtd_sema);
            return ret;
        }
        Len = min((unsigned int)Entry.Length, *read_length);
		ret = MTD_READ(&TFFS_mtd, (unsigned long)E + sizeof(struct _TFFS_Entry), Len, &retlen, read_buffer);
        if(ret) {
            up(&tffs_mtd_sema);
            return ret;
        }
        *read_length = retlen;
        DBG((KERN_INFO "daten kopiert\n"));
        up(&tffs_mtd_sema);
        return 0;
    }
    DBG((KERN_INFO "Eintrag %i nicht gefunden\n", Id));
    up(&tffs_mtd_sema);
    return (unsigned int)-ENOENT;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
unsigned int TFFS_Create_Index(void) {
    unsigned int retlen, ret;
    struct _TFFS_Entry *E, Entry;
    unsigned int Len;
    unsigned int i;

    for(i = 0 ; i < FLASH_FS_ID_LAST ; i++) {
        TFFS_Global_Index[i] = (struct _TFFS_Entry *)FLASH_FS_ID_FREE;
    }
    E = NULL;
    DBG((KERN_INFO "TFFS_Create_Index(): "));

    while((unsigned int)E + sizeof(struct _TFFS_Entry) < TFFS_mtd.size) {
        ret = MTD_READ(&TFFS_mtd, (unsigned long)E, sizeof(struct _TFFS_Entry), &retlen, (unsigned char *)&Entry);
        if(ret) {
            return ret;
        }

        if(Entry.ID >= (unsigned short)FLASH_FS_ID_LAST) { /*---  Eintrag gefunden ----*/
            DBG((KERN_INFO " [end found]\n"));
            return 0;
        }
        DBG((KERN_INFO " [<0x%x> %u bytes]\n", Entry.ID, Entry.Length));

        /*--- doppelter Eintrag gefunden, diesen Eintrag löschen ---*/
        if(TFFS_Global_Index[Entry.ID] != (struct _TFFS_Entry *)FLASH_FS_ID_FREE) { 
            if(Entry.ID != FLASH_FS_ID_SKIP) {
                printk(" [<0x%x> %u bytes, cleared]\n", Entry.ID, Entry.Length);
				printk( "E = %#x\n", (unsigned int)E);
                Entry.ID = FLASH_FS_ID_SKIP;
                ret = MTD_WRITE(&TFFS_mtd, (unsigned long)E, sizeof(struct _TFFS_Entry), &retlen, (unsigned char *)&Entry);
                if(ret) return ret;
            }
        } else {
            TFFS_Global_Index[Entry.ID] = E;
        }
        Len = (Entry.Length + 3) & ~0x03;
        E = (struct _TFFS_Entry *)((unsigned int)E + sizeof(struct _TFFS_Entry) + Len);
    }
    DBG((KERN_INFO " [filesystem full]\n"));
    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
unsigned int Build_Cleanup_Buffer(unsigned char *Cleanup_Buffer, unsigned int *Cleanup_Buffer_Len, unsigned int *SkipCount) {

    struct _TFFS_Entry *E, *pEntry;
    unsigned char *P;
    unsigned int Len, retlen, ret;

    E = (struct _TFFS_Entry *)0;
    P = Cleanup_Buffer;
    while(((unsigned int)E) + sizeof(struct _TFFS_Entry) < TFFS_mtd.size) {

        pEntry = (struct _TFFS_Entry *)P;
        ret = MTD_READ(&TFFS_mtd, (unsigned long)E, sizeof(struct _TFFS_Entry), &retlen, (unsigned char *)pEntry);
        if(ret) {
            return ret;
        }

        if(pEntry->ID >= (unsigned short)FLASH_FS_ID_LAST) { /*---  Eintrag gefunden ----*/
            DBG((KERN_INFO " [end found]\n"));
            break;
        }

        Len = (pEntry->Length + 3) & ~0x03;

        if(pEntry->ID != FLASH_FS_ID_SKIP) {

            P += sizeof(struct _TFFS_Entry *);
            ret = MTD_READ(&TFFS_mtd, (unsigned long)E + sizeof(struct _TFFS_Entry), pEntry->Length, &retlen, (unsigned char *)P);
            if(ret) {
                return ret;
            }
            DBG((KERN_INFO " [<0x%x> %u bytes]\n", pEntry->ID, pEntry->Length));
            P += Len;
        } else {
            (*SkipCount)++;
            DBG((KERN_INFO " [SKIP %u bytes]\n", pEntry->Length));
        }

        E = (struct _TFFS_Entry *)((unsigned int)E + sizeof(struct _TFFS_Entry) + Len);
    }
    
    *Cleanup_Buffer_Len = P - Cleanup_Buffer;
    DBG((KERN_INFO "[Build_Cleanup_Buffer] buffer_len %d\n", *Cleanup_Buffer_Len));
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int TFFS_Cleanup(void *handle) {
    unsigned int retlen, ret;
    unsigned int SkipCount = 0, Cleanup_Buffer_Len = 0;
    unsigned int current_number = 0;
    struct tffs_info *mtd = 0;
    struct tffs_info local_TFFS = TFFS_mtd;
    unsigned int other;

    DBG((KERN_INFO "TFFS_Cleanup(handle=%x): ", (unsigned int)handle));

    if(TFFS_Cleanup_Buffer == NULL) {
        TFFS_Cleanup_Buffer = vmalloc(TFFS_mtd.size);
        if(TFFS_Cleanup_Buffer == NULL) {
            DBG((KERN_ERR "TFFS_Cleanup: malloc(%u) failed\n", TFFS_mtd.size));
            return (unsigned int)-ENOMEM;
        }
        DBG((KERN_INFO "TFFS_Cleanup: malloc(%u) success\n", TFFS_mtd.size));
    }

    /*------------------------------------------------------------------------------------------*\
     * gültige Daten zusammesuchen, tffs-index bestimmen und tffs löschen, wenn es notwendig ist
    \*------------------------------------------------------------------------------------------*/
    ret = Build_Cleanup_Buffer(TFFS_Cleanup_Buffer, &Cleanup_Buffer_Len, &SkipCount);
    if (!ret && SkipCount) {
        DBG((KERN_INFO " [%u data records skiped]\n", SkipCount));

        tffs_written = 0;
        other = (current_mtd == avail_mtd[0]) ? avail_mtd[1] : avail_mtd[0];
        DBG((KERN_INFO " [double buffer: close old mtd, open mtd%u]\n", other));
        if(get_mtd_device_wrapped(other, &local_TFFS)) {
            panic("TFFS_Cleanup: can't get mtd%u\n", other);
            return (unsigned int)-ENXIO;
        }
        mtd = &local_TFFS;

        DBG((KERN_INFO " [erase filesystem]\n"));
        ret = TFFS_Format(handle, mtd);
        if(ret) {
            DBG((KERN_ERR "TFFS_Cleanup: format failed\n"));
            return ret;
        }
    } else {
        DBG((KERN_INFO "TFFS_Cleanup: no IDs skiped, leave it as it is\n"));
        return 0;
    }

    /*------------------------------------------------------------------------------------------*\
     * nochmal gültige Daten zusammensuchen, falls Daten geschrieben wurden
    \*------------------------------------------------------------------------------------------*/
    down(&tffs_mtd_sema);
    if (tffs_written) {
        printk("[TFFS_Cleanup] tffs_written build Cleanup_Buffer\n");
        SkipCount = 0;
        ret = Build_Cleanup_Buffer(TFFS_Cleanup_Buffer, &Cleanup_Buffer_Len, &SkipCount);
        if (ret) {
            up(&tffs_mtd_sema);
            printk(KERN_INFO "TFFS_Cleanup: Build_Cleanup_Buffer failed 0x%x\n", ret);
            return ret;
        }
    }

    /*------------------------------------------------------------------------------------------*\
    \*------------------------------------------------------------------------------------------*/
    {
        union _tffs_segment_entry *pu;

        pu = (union _tffs_segment_entry *)TFFS_Cleanup_Buffer;
        if(pu->Entry.ID != FLASH_FS_ID_SEGMENT) {
            panic("TFFS_Cleanup: flash segment %u file invalid\n", current_mtd);
        }
        current_number = TFFS_GET_SEGMENT_VALUE(pu);
        DBG((KERN_INFO " [double buffer: read current number %u]\n", current_number));
        TFFS_SET_SEGMENT_VALUE(pu, 0);
    }

    /*------------------------------------------------------------------------------------------*\
     * tffs neu schreiben
    \*------------------------------------------------------------------------------------------*/
    DBG((KERN_INFO " [write filesystem]\n"));
    ret = MTD_WRITE(mtd, 0, Cleanup_Buffer_Len, &retlen, (unsigned char *)TFFS_Cleanup_Buffer);
    if(ret) {
        up(&tffs_mtd_sema);
        return ret;
    }

    /*------------------------------------------------------------------------------------------*\
     * id setzen
    \*------------------------------------------------------------------------------------------*/
    {
        union _tffs_segment_entry u;
        u.Entry.ID = FLASH_FS_ID_SEGMENT;
        u.Entry.Length = sizeof(unsigned int);
        current_number++;
        DBG((KERN_INFO " [double buffer: write current number %u]\n", current_number));
        TFFS_SET_SEGMENT_VALUE(&u, current_number);
        ret = MTD_WRITE(mtd, 0, sizeof(union _tffs_segment_entry), &retlen, (unsigned char *)&u);
        if(ret) {
            up(&tffs_mtd_sema);
            return ret;
        }
    }
    /*------------------------------------------------------------------------------------------*\
     * alles fertig, tffs umsetzen
    \*------------------------------------------------------------------------------------------*/
    DBG((KERN_INFO "TFFS_Cleanup: set mtd to %d\n", other));
    current_mtd = other;
    put_mtd_device_wrapped(&TFFS_mtd);
    if (TFFS_mtd.use_bdev) {
        TFFS_mtd.tffs.file = mtd->tffs.file;
    } else {
        TFFS_mtd.tffs.mtd = mtd->tffs.mtd;
    }

    DBG((KERN_INFO " [recreate index]\n"));
    TFFS_Create_Index();
    up(&tffs_mtd_sema);       /*--- schreiben wieder zulassen ---*/

    DBG((KERN_INFO "TFFS_Cleanup: success\n"));
    return 0;
}


/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
unsigned int TFFS_Info(void *handle, unsigned int *Fill) {
    unsigned int retlen, ret;
    struct _TFFS_Entry *E, Entry;
    unsigned int Count, Max = 0;

    DBG((KERN_INFO "TFFS_Info(handle=%x): ", (unsigned int)handle));

    for(Count = 0, E = 0 ; Count < FLASH_FS_ID_LAST ; Count++) {
        E = TFFS_Global_Index[Count];
        if(E != (struct _TFFS_Entry *)FLASH_FS_ID_FREE) {
            DBG((KERN_INFO "E = 0x%x", (unsigned int)E));
            if(Max < (unsigned int)E) {
                Max = (unsigned int)E;
            }
        }
    }
    if(!Max) {
        *Fill = 0;
        return 0;
    }

    DBG((KERN_INFO "Header des hoechsten Eintrags lesen (0x%x)\n", Max));
    ret = MTD_READ(&TFFS_mtd, (unsigned long)Max, sizeof(struct _TFFS_Entry), &retlen, (unsigned char *)&Entry);
    if(ret) return ret;

    DBG((KERN_INFO " [<0x%x> %u bytes]\n", Entry.ID, Entry.Length));
    Max += Entry.Length;

    *Fill = (Max * 100) / (unsigned int)(TFFS_mtd.size);
    DBG((KERN_INFO "TFFS_Info: Fill=%u%% success\n", *Fill));
    return 0;
}


EXPORT_SYMBOL(TFFS_Open);
EXPORT_SYMBOL(TFFS_Close);
EXPORT_SYMBOL(TFFS_Read);
EXPORT_SYMBOL(TFFS_Write);
