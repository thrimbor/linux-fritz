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
#include <asm/errno.h>
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <asm/mach_avm.h>

#if defined(CONFIG_TFFS_ENV)
#define TFFS_NAME_TABLE
#include "tffs_local.h"
#include <linux/tffs.h>

static void *avm_urlader_handle;
static void *avm_urlader_open(void);

/*--- #define TFFS_CONFIG ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(TFFS_CONFIG)
#define DBG(a)  printk a
#else /*--- #if defined(TFFS_CONFIG) ---*/
#define DBG(a)
#endif /*--- #else ---*/ /*--- #if defined(TFFS_CONFIG) ---*/


/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void avm_urlader_extract_name_table(unsigned char *buffer, int len) {
    unsigned int index = 0;
    unsigned int name_len;
    memset(&TFFS_Name_Table[0], 0, sizeof(TFFS_Name_Table));

    DBG((KERN_INFO "avm_urlader_extract_name_table(buffer=%x, len=%x)\n", (unsigned int)buffer, len));
    while(len > 0 && index < MAX_ENV_ENTRY) {
        TFFS_Name_Table[index].id = *(unsigned int *)buffer;
        buffer   += sizeof(unsigned int);
        len      -= sizeof(unsigned int);
        name_len  = strlen(buffer) + 1;
        name_len  = (name_len + 3) & ~0x03;

        memcpy(TFFS_Name_Table[index].Name, buffer, name_len);
        len      -= name_len;
        buffer   += name_len;
        index++;
    }
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static unsigned int avm_urlader_build_name_table(unsigned char *buffer, int max_len) {
    unsigned int index = 0;
    unsigned int name_len, output_len = 0;

    memset(&TFFS_Name_Table[0], 0, sizeof(TFFS_Name_Table));
    memcpy(&TFFS_Name_Table[0], &T_Init[0], sizeof(T_Init));

    DBG((KERN_INFO "avm_urlader_build_name_table(buffer=%x, max_len=%x)\n", (unsigned int)buffer, max_len));
    while(index < MAX_ENV_ENTRY && TFFS_Name_Table[index].id && output_len + 64 < max_len) {
        if(TFFS_Name_Table[index].Name && TFFS_Name_Table[index].Name[0]) {
            *(unsigned int *)buffer = TFFS_Name_Table[index].id;
            buffer      += sizeof(unsigned int);
            output_len  += sizeof(unsigned int);
            name_len  = strlen(TFFS_Name_Table[index].Name) + 1;
            name_len  = (name_len + 3) & ~0x03;
            memcpy(buffer, TFFS_Name_Table[index].Name, name_len);
            output_len  += name_len;
            buffer      += name_len;
        }
        index++;
    }
    return output_len;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static unsigned int avm_urlader_get_name(char *Name) {
    struct _TFFS_Name_Table *T = &TFFS_Name_Table[0];
    DBG(("avm_urlader_get_name(%s)\n", Name));
    while(T->id) {
        if(Name[0] == T->Name[0] && !strcmp(Name, T->Name))
            return T->id;  /*--- gefunden ---*/
        if(Name[0] < T->Name[0])
            return (unsigned int)-1;  /*--- hat keine Zweck mehr ---*/
        T++;
    }
    DBG(("avm_urlader_get_name(%s): not found\n", Name));
    return (unsigned int)-1;
}

/*-----------------------------------------------------------------------------------------------*\
 * liefert den Namen für den Index der Variable
 *
 * Get the variable referenced by index
 *
 * NOTE: Caller is responsibe for freeing the memory. 
\*-----------------------------------------------------------------------------------------------*/
char *avm_urlader_env_get_variable(int idx) {
    char *buffer;
    DBG(("avm_urlader_env_get_variable(%u)\n", idx));

    if(idx < 0) {
        DBG(("avm_urlader_env_get_variable(%d) failed, invalid handle\n", idx));
        return NULL;
    }

    if(avm_urlader_handle == NULL)
        avm_urlader_handle = avm_urlader_open();
    if(avm_urlader_handle == NULL) {
        DBG(("avm_urlader_env_get_variable(%u) failed, no avm_urlader_handle\n", idx));
        return NULL;
    }

    if(idx < (sizeof(TFFS_Name_Table) / sizeof(struct _TFFS_Name_Table)) - 1) {
        if(TFFS_Name_Table[idx].Name && TFFS_Name_Table[idx].Name[0]) {
            buffer = kmalloc(strlen(TFFS_Name_Table[idx].Name) + 1, GFP_KERNEL);
            if(buffer == NULL) {
                DBG(("avm_urlader_env_get_variable(%u) found, but no memory\n", idx));
                return NULL;
            }
            strcpy(buffer, TFFS_Name_Table[idx].Name);
            return buffer;
        }
    }
    DBG(("avm_urlader_env_get_variable(%u) not found\n", idx));
    return NULL;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static void *avm_urlader_open(void) {
    unsigned char *buffer;
    unsigned int ret_length, status;
    void *handle = NULL;
    
    if(avm_urlader_handle == NULL) {
        DBG((KERN_INFO "avm_urlader_open(): open TFFS\n"));
        handle = TFFS_Open();
        if(handle == NULL) {
            DBG((KERN_INFO "avm_urlader_open(): open TFFS success\n"));
        }
    }
    if(handle == NULL) {
        DBG((KERN_ERR "avm_urlader_open(): open TFFS failed\n"));
        return NULL;
    }

    buffer = kmalloc(FLASH_ENV_ENTRY_SIZE * MAX_ENV_ENTRY, GFP_KERNEL);
    if(buffer == 0) {
        DBG((KERN_ERR "avm_urlader_open(): out of memory\n"));
        TFFS_Close(handle);
        return NULL;
    }

    ret_length = FLASH_ENV_ENTRY_SIZE * MAX_ENV_ENTRY;
    status = TFFS_Read(handle, FLASH_FS_NAME_TABLE, buffer, &ret_length);
    if(status == 0) {
        DBG((KERN_INFO "avm_urlader_open(): read name table success (%u bytes)\n", ret_length));
        avm_urlader_extract_name_table(buffer, ret_length);
        DBG((KERN_INFO "avm_urlader_open(): extract name table success\n"));
        if((T_Init[0].id != TFFS_Name_Table[0].id) || (T_Init[0].Name[1] != TFFS_Name_Table[0].Name[1])) {
            printk("WARNING: TFFS Name Table update ! (current %s new %s)\n", TFFS_Name_Table[0].Name, T_Init[0].Name);
            status = 1; /*--- rebuild name table ---*/
        } else {
            printk("TFFS Name Table %c\n", TFFS_Name_Table[0].Name[1]);
        }
    }
    if(status != 0) {
        DBG((KERN_ERR "avm_urlader_open(): read name table failed\n"));
        ret_length = avm_urlader_build_name_table(buffer, FLASH_ENV_ENTRY_SIZE * MAX_ENV_ENTRY);
        DBG((KERN_INFO "avm_urlader_open(): build name table success (%u bytes)\n", ret_length));
        status = TFFS_Write(handle, FLASH_FS_NAME_TABLE, buffer, ret_length, 0);
        if(status) {
            DBG((KERN_ERR "avm_urlader_open(): write name table failed\n"));
            kfree(buffer);
            TFFS_Close(handle);
            return NULL;
        }
        DBG((KERN_INFO "avm_urlader_open(): write name table success (%u bytes)\n", ret_length));
    }
    kfree(buffer);
    return handle;
}

/*-----------------------------------------------------------------------------------------------*\
 * liefert den Inhalt der Variable
 *
 * Get the value associated with an environment variable
 *
 * NOTE: Caller is responsibe for freeing the memory. 
\*-----------------------------------------------------------------------------------------------*/
char *avm_urlader_env_get_value_by_id(unsigned int id) {
    unsigned int length;
    unsigned int status;
    unsigned char *buffer;

    DBG(("avm_urlader_env_get_value_by_id(%u)\n", id));

    if(avm_urlader_handle == NULL)
        avm_urlader_handle = avm_urlader_open();
    if(avm_urlader_handle == NULL) {
        DBG(("avm_urlader_env_get_value_by_id(%u) failed, no avm_urlader_handle\n", id));
        return NULL;
    }

    length = FLASH_ENV_ENTRY_SIZE;
    buffer = kmalloc(length + 1, GFP_KERNEL);
    if(buffer == NULL)
        return NULL;

    status = TFFS_Read(avm_urlader_handle, id, buffer, &length);
    buffer[length] = '\0';

    if(status) {
        kfree(buffer);
        return NULL;
    }
    DBG(("avm_urlader_env_get_value_by_id(%u) : '%s'\n", id, buffer));
    return buffer;
}

/*-----------------------------------------------------------------------------------------------*\
 * liefert den Inhalt der Variable
 *
 * Get the value associated with an environment variable
 *
 * NOTE: Caller is responsibe for freeing the memory. 
\*-----------------------------------------------------------------------------------------------*/
char *avm_urlader_env_get_value(char *var) {
    unsigned int length;
    unsigned int status;
    unsigned char *buffer;
    unsigned int id;

    DBG(("avm_urlader_env_get_value(%s)\n", var));

    if(avm_urlader_handle == NULL)
        avm_urlader_handle = avm_urlader_open();
    if(avm_urlader_handle == NULL) {
        DBG(("avm_urlader_env_get_value(%s) failed, no avm_urlader_handle\n", var));
        return NULL;
    }

    id = avm_urlader_get_name(var);
    if(id == (unsigned int)-1)
        return NULL;
    DBG(("avm_urlader_env_get_value(%s) id=%u\n", var, id));

    length = FLASH_ENV_ENTRY_SIZE;
    buffer = kmalloc(length + 1, GFP_KERNEL);
    if(buffer == NULL)
        return NULL;

    status = TFFS_Read(avm_urlader_handle, id, buffer, &length);
    buffer[length] = '\0';

    if(status) {
        kfree(buffer);
        return NULL;
    }
    DBG(("avm_urlader_env_get_value(%s) : '%s'\n", var, buffer));
    return buffer;
}

/*-----------------------------------------------------------------------------------------------*\
 * Set the variable to a specific value.
 *
 * NOTE: If the value is NULL, the variable will be unset. Otherwise, the
 * variable-value pair will be written to flash.
\*-----------------------------------------------------------------------------------------------*/
int avm_urlader_env_set_variable(char *var, char *val) {
    unsigned int id;

    if(avm_urlader_handle == NULL)
        avm_urlader_handle = avm_urlader_open();
    if(avm_urlader_handle == NULL)
        return -1;

    id = avm_urlader_get_name(var);

    if(id == (unsigned int)-1)
        return -EFAULT;

    if(val == NULL)
        return TFFS_Clear(avm_urlader_handle, id);

    return TFFS_Write(avm_urlader_handle, id, val, strlen(val) + 1, 0);
}

/*-----------------------------------------------------------------------------------------------*\
 * Unset the variable to a specific value.
\*-----------------------------------------------------------------------------------------------*/
int avm_urlader_env_unset_variable(char *var) {
	return avm_urlader_env_set_variable(var, NULL);
}

/*-----------------------------------------------------------------------------------------------*\
 * Defrag the block associated with the Adam2 environment variables.
\*-----------------------------------------------------------------------------------------------*/
int avm_urlader_env_defrag(void) {
    int err;
    extern void tffs_unlock(void);
    extern int tffs_lock(void);
    if(avm_urlader_handle == NULL)
        avm_urlader_handle = avm_urlader_open();
    if(avm_urlader_handle == NULL)
        return -1;
    if(tffs_lock())
        return -1;
    err = TFFS_Cleanup(avm_urlader_handle);
    tffs_unlock();
    return err;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
EXPORT_SYMBOL(avm_urlader_env_get_value);
EXPORT_SYMBOL(avm_urlader_env_unset_variable);
EXPORT_SYMBOL(avm_urlader_env_set_variable);

#endif /*--- #if defined(CONFIG_TFFS_ENV) ---*/
