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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/ctype.h>
#include <asm/mach_avm.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/semaphore.h>
#if defined(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif /*--- #if defined(CONFIG_PROC_FS) ---*/
#include <linux/zlib.h>
#include <linux/tffs.h>
#include "tffs_local.h"


#ifdef CONFIG_SYSCTL

#define DEV_ADAM2	    6
#define DEV_ADAM2_ENV	    1

#define ADAM2_ENV_STR_SIZE  FLASH_ENV_ENTRY_SIZE        /*--- 256 Bytes ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int avm_urlader_getenv_proc(ctl_table   *ctl,
                                   int          write,
                                   void        *buffer,
                                   size_t      *lenp,
                                   loff_t      *ppos) {

    char info[ADAM2_ENV_STR_SIZE];
    char *val, len;
    char *default_val = NULL;
    int free_val;

    /*--- printk(KERN_INFO"[avm_urlader_getenv_proc]: ctl->ctl_name=%u, write=%u *lenp=%u *ppos=%u\n", ---*/
            /*--- (int)ctl->ctl_name, (int)write, (int)*lenp, (int)*ppos); ---*/

    if(write) {
        printk(KERN_ERR"write not supported\n");
        return -EFAULT;
    }
	if (!*lenp || *ppos) {
		*lenp = 0;
		return 0;
	}

	switch(ctl->ctl_name) {
        case FLASH_FS_ANNEX:
            default_val = "B";
            break;
        case FLASH_FS_FIRMWARE_VERSION:
            default_val = "";
            break;
        default:
            printk(KERN_ERR"%s: id %x not supported\n", __func__, ctl->ctl_name);
            return -EFAULT;
    }

    free_val = 1;
    val = avm_urlader_env_get_value_by_id(ctl->ctl_name);

    if(val == NULL) {
        if(default_val) {
            val = default_val;
        } else {
            val = "";
        }
        free_val = 0;
    }

    /* Scan thru the flash, looking for each possible variable index */
    len = strnlen(val, ADAM2_ENV_STR_SIZE);
    /*--- printk(KERN_INFO"[avm_urlader_getenv_proc] val=%s len=%u\n", val, len); ---*/

    memcpy(info, val, len + 1);

    if(free_val)
        kfree(val);

    if(len > *lenp)
        len = *lenp;

    if(len) {
        if(copy_to_user(buffer, info, len)) {
            return -EFAULT;
        }
    }

    *lenp = len;
    *ppos += len;

    /*--- printk(KERN_INFO"[avm_urlader_getenv_proc] *lenp=%u *ppos\n", *lenp, (int)*ppos); ---*/

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int avm_urlader_setenv_sysctl(ctl_table *ctl, int write,
			void *buffer, size_t *lenp, loff_t *ppos)
{
    char *var, *val, *ptr;
    int i;
    size_t len, total;
    
    if (!*lenp || (*ppos && !write)) {
        *lenp = 0;
        return 0;
    }

    if(ctl->ctl_name != DEV_ADAM2_ENV) {
        return -ENODEV;
    }

    if(write) {
        char info[ADAM2_ENV_STR_SIZE];
    
        if (*lenp < ADAM2_ENV_STR_SIZE) {
            copy_from_user(info, buffer, *lenp);
            if (info[*lenp - 1] == '\n')
                info[*lenp - 1] = 0;
            else
                info[*lenp] = 0;
            *ppos = *lenp;
        } else {
            return -EFAULT;
        }

        ptr = strpbrk(info, " \t");
        if(!ptr)	{
            /* We have no way to distinguish between unsetting a
             * variable and setting a non-value variable.
             *
             * Consequently, we will treat any variable
             * without a value as an unset request.
             */
            avm_urlader_env_unset_variable(info);
        } else {
            /* Set the variable-value pair in flash */
            *ptr++ = 0;
            if(avm_urlader_env_set_variable(info, ptr) != 0) {
                printk(KERN_NOTICE "Defragging the environment variable region.\n");
                avm_urlader_env_defrag();
                if( avm_urlader_env_set_variable(info, ptr) != 0 )
                    printk(KERN_WARNING "Failed to write %s to environment variable region.\n", info);
            }
        }
    } else {
        len = total = 0;
        /* Scan thru the flash, looking for each possible variable index */
        for(i = 0; i < MAX_ENV_ENTRY; i++) {
            if( (var = avm_urlader_env_get_variable(i)) != NULL ) {
                char info[ADAM2_ENV_STR_SIZE * 2];
                if( (val = avm_urlader_env_get_value(var)) != NULL) {
                    len = snprintf(info, ( ADAM2_ENV_STR_SIZE * 2), "%s\t%s\n", var, val);
                    kfree(val);
                }

                kfree(var);

                if (len > *lenp - total)
                    len = *lenp - total;

                if(len) {
                    if(copy_to_user(buffer + total, info, len)) {
                        return -EFAULT;
                    }
                }
#if !defined(CONFIG_TFFS_ENV)
#error "no CONFIG_TFFS_ENV defined"
#endif /*--- #if !defined(CONFIG_TFFS_ENV) ---*/
                total += len;
                len = 0;
            }
        }
        *lenp = total;
        *ppos += total;
    }

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern unsigned int avm_reset_status(void);
extern void set_reboot_status_to_Update(void);
static int avm_urlader_reset_status_proc(ctl_table   *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos) {

    unsigned int status;
    unsigned char *reason;

	if (!*lenp || (*ppos && !write)) {
		*lenp = 0;
		return 0;
	}

    if(write) {
        char info[ADAM2_ENV_STR_SIZE];
    
        if (*lenp < ADAM2_ENV_STR_SIZE) {
            copy_from_user(info, buffer, *lenp);
            if (info[*lenp - 1] == '\n')
                info[*lenp - 1] = 0;
            else
                info[*lenp] = 0;
            *ppos = *lenp;
        } else {
            return -EFAULT;
        }
#if ! defined(CONFIG_MIPS_UR8) && ! defined(CONFIG_MIPS_FUSIV)
        if ( ! strcmp(info, "FW-Update")) {
            set_reboot_status_to_Update();
        } else if ( ! strcmp(info, "reboot")) {
            set_reboot_status_to_Update();
        } else
#endif
            return -EFAULT;
    } else {

        status = avm_reset_status();
        switch (status) {
            case 0:
                reason = "poweron";
                break;
            case 1:
                reason = "software";
                break;
            case 2:
                reason = "watchdog";
                break;
            case 3:
                reason = "reboot";
                break;
#if defined(CONFIG_MACH_ATHEROS)
            case 4:
                reason = "nmi_workaround";
                break;
#endif
            default:
                reason = "unbekannt";
                break;
        }

        *lenp = strlen(reason);
        *ppos = *lenp;

        if(copy_to_user(buffer, reason, *lenp)) {
            return -EFAULT;
        }
    }

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
ctl_table avm_urlader_env_table[] = {
	{
        .ctl_name       = DEV_ADAM2_ENV, 
        .procname       = "environment", 
        .data           = NULL, 
        .maxlen         = ADAM2_ENV_STR_SIZE, 
        .mode           = 0644, 
        .child          = NULL, 
        .proc_handler   = &avm_urlader_setenv_sysctl 
    }, {
        .ctl_name       = FLASH_FS_FIRMWARE_VERSION, 
        .procname       = "firmware_version", 
        .data           = NULL, 
        .maxlen         = ADAM2_ENV_STR_SIZE, 
        .mode           = 0444, 
        .child          = NULL, 
        .proc_handler   = &avm_urlader_getenv_proc
    }, {
        .ctl_name       = FLASH_FS_ANNEX, 
        .procname       = "annex",            
        .data           = NULL, 
        .maxlen         = ADAM2_ENV_STR_SIZE, 
        .mode           = 0444, 
        .child          = NULL, 
        .proc_handler   = &avm_urlader_getenv_proc
    }, {
        .procname       = "reboot_status",            
        .data           = NULL, 
        .maxlen         = ADAM2_ENV_STR_SIZE,
        .mode           = 0644, 
        .child          = NULL, 
        .proc_handler   = &avm_urlader_reset_status_proc
    }, {
        .ctl_name       = 0
    }
};

ctl_table avm_urlader_root_table[] = {
	{
        .ctl_name       = DEV_ADAM2_ENV, 
        .procname       = "urlader", 
        .data           = NULL, 
        .maxlen         = 0, 
        .mode           = 0555, 
        .child          = avm_urlader_env_table 
    }, {
        .ctl_name       = 0
    }
};

static struct ctl_table_header *avm_urlader_sysctl_header;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avm_urlader_env_sysctl_register(void)
{
	static int initialized;

	if (initialized == 1)
		return;

	avm_urlader_sysctl_header  = register_sysctl_table(avm_urlader_root_table);
	//avm_urlader_root_table->child->de->owner = THIS_MODULE; // PORT_ERROR_32: who or what is ->de->owner

	initialized = 1;
}

#if defined(MODULE)
static void avm_urlader_env_sysctl_unregister(void)
{
	unregister_sysctl_table(avm_urlader_sysctl_header);
}
#endif /*--- #if defined(MODULE) ---*/

int avm_urlader_env_init(void)
{
	avm_urlader_env_sysctl_register();
	printk(KERN_INFO "Adam2 environment variables API installed.\n");
	return 0;
}

#if defined(MODULE)
void avm_urlader_env_exit(void)
{
	avm_urlader_env_sysctl_unregister();
}
#endif /*--- #if defined(MODULE) ---*/

#endif /* CONFIG_SYSCTL */

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#ifdef CONFIG_PROC_FS
unsigned int tffs_info_value;
int tffs_read_proc ( char *page, char **start, off_t off,int count, int *eof, void *data_unused) {
	int     len   = 0;
    off_t   begin = 0;

    if(tffs_lock()) {
        printk(KERN_ERR"tffs_read_proc: lock() failed\n");
        return -EINVAL;
    }

	len = sprintf(page + len, "TFFS \n");
    if(len + begin > off + count)
        goto tffs_read_proc_done;
    if(len + begin < off) {
        begin += len;
        len = 0;
    }

	len += sprintf(page + len, "mount=mtd%u\n", tffs_mtd[0]);
    if(len + begin > off + count)
        goto tffs_read_proc_done;
    if(len + begin < off) {
        begin += len;
        len = 0;
    }

	len += sprintf(page + len, "mount=mtd%u\n", tffs_mtd[1]);
    if(len + begin > off + count)
        goto tffs_read_proc_done;
    if(len + begin < off) {
        begin += len;
        len = 0;
    }

	len += sprintf(page + len, "request=%u\n", tffs_request_count);
    if(len + begin > off + count)
        goto tffs_read_proc_done;
    if(len + begin < off) {
        begin += len;
        len = 0;
    }

    if(tffs_info_value) {
	    len += sprintf(page + len, "fill=%u\n", tffs_info_value);
        if(len + begin > off + count)
            goto tffs_read_proc_done;
        if(len + begin < off) {
            begin += len;
            len = 0;
        }
    }

    if(tffs_thread_event) {
	    len += sprintf(page + len, "event panding\n");
        if(len + begin > off + count)
            goto tffs_read_proc_done;
        if(len + begin < off) {
            begin += len;
            len = 0;
        }
    }

#if defined(CONFIG_TFFS_EXCLUSIVE)
    len += sprintf(page + len, "mode: read/write: exclusive\n");
#else /*--- #if defined(CONFIG_TFFS_EXCLUSIVE) ---*/
    len += sprintf(page + len, "mode: read/write: shared\n");
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_TFFS_EXCLUSIVE) ---*/

    if(len + begin > off + count)
        goto tffs_read_proc_done;
    if(len + begin < off) {
        begin += len;
        len = 0;
    }

    switch(tffs_thread_state) {
        case tffs_thread_state_off:
	        len += sprintf(page + len, "thread state=off\n");
            break;
        case tffs_thread_state_init:
	        len += sprintf(page + len, "thread state=init\n");
            break;
        case tffs_thread_state_idle:
	        len += sprintf(page + len, "thread state=idle\n");
            break;
        case tffs_thread_state_process:
	        len += sprintf(page + len, "thread state=process\n");
            break;
        case tffs_thread_state_down:
	        len += sprintf(page + len, "thread state=down\n");
            break;
    }
    if(len + begin > off + count)
        goto tffs_read_proc_done;
    if(len + begin < off) {
        begin += len;
        len = 0;
    }
    *eof = 1;

tffs_read_proc_done:
    tffs_unlock();

    if (off >= len + begin)
            return 0;
    *start = page + (begin - off);
    return ((count < begin + len - off) ? count : begin + len - off);
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int tffs_atoi(char *p) {
    int value = 0;
    while(*p && *p >= '0' && *p <= '9') {
        value *= 10;
        value += *p - '0';
        p++;
    }
    return value;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int tffs_write_proc(struct file *file, const char *buffer, unsigned long count, void *data) {
    char page[80];
    char *p = page;
    int type = -1;
    void *Handle;
    int status = 0;

    if(count > sizeof(page)) {
		return -EFAULT;
    }
	if( copy_from_user(page, buffer, count) ) {
		return -EFAULT;
	}
	if( count )
		page[count-1]='\0';
	else
		page[count]='\0';

    if(!strncmp(p, "info", sizeof("info") - 1)) {
        type = _TFFS_INFO;
    } else if(!strncmp(p, "index", sizeof("index") - 1)) {
        type = _TFFS_REINDEX;
    } else if(!strncmp(p, "cleanup", sizeof("cleanup") - 1)) {
        type = _TFFS_CLEANUP;
    } else if(!strncmp(p, "clear_id", sizeof("clear_id") - 1)) {
        type = _TFFS_CLEAR_ID;
    } else if(!strncmp(p, "werkseinstellung", sizeof("werkseinstellung") - 1)) {
        type = _TFFS_WERKSEINSTELLUNG;
    }
    if(type != -1) {
        if(tffs_lock()) {
            printk(KERN_ERR"tffs_write_proc: lock() failed\n");
            return -EINVAL;
        }
        Handle = TFFS_Open();
        switch(type) {
            case _TFFS_INFO:
                status = TFFS_Info(Handle, &tffs_info_value);
                printk(KERN_INFO"/proc/tffs: info request: %s\n", status ? "failed" : "success");
                break;
            case _TFFS_REINDEX:
                status = TFFS_Create_Index();
                printk(KERN_INFO"/proc/tffs: index request: %s\n", status ? "failed" : "success");
                break;
            case _TFFS_CLEANUP:
                status = TFFS_Cleanup(Handle);
                printk(KERN_INFO"/proc/tffs: cleanup request: %s\n", status ? "failed" : "success");
                break;
            case _TFFS_WERKSEINSTELLUNG:
                status = TFFS_Werkseinstellungen(Handle);
                printk(KERN_INFO"/proc/tffs: werkseinstellungen request: %s\n", status ? "failed" : "success");
                break;
            case _TFFS_CLEAR_ID:
                while(*p && *p != ' ' && *p != '\t') p++;
                while(*p && ( *p == ' ' || *p == '\t')) p++;
                type = tffs_atoi(p);
                if(type) {
                    status = TFFS_Clear(Handle, type);
                    printk(KERN_INFO"/proc/tffs: clear id 0x%x request: %s\n", type, status ? "failed" : "success");
                } else {
                    printk(KERN_ERR"/proc/tffs: clear id request: invalid id 0x%x\n", type);
                }
                break;
        }
        TFFS_Close(Handle);
        tffs_unlock();
    }
    return status == 0 ? count : -EFAULT;
}
#endif /*--- #ifdef CONFIG_PROC_FS ---*/

