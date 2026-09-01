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
#include <linux/mm.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
/*--- #include <asm/semaphore.h> ---*/
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/vmalloc.h>
#include <linux/poll.h>
#include <linux/version.h>
#include <linux/avm_debug.h>
#include <linux/device.h>
#include <linux/socket.h>
#include <linux/jiffies.h>
#include <linux/un.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <net/sock.h>
#ifdef CONFIG_KALLSYMS
#include <linux/kallsyms.h>
#endif/*--- #ifdef CONFIG_KALLSYMS ---*/

#include <linux/cdev.h>
#include <asm/mach_avm.h>

#include "avm_sammel.h"
#include "avm_debug.h"

#include <linux/fs.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 19)
#define AVM_DEBUG_UDEV
#endif/*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/

#define DEB_ERR(args...)     printk(KERN_ERR args)
/*--- #define DEB_INFO(args...)     printk(KERN_INFO args) ---*/
#define DEB_INFO(args...)
/*--- #define DEB_NOTE(args...)     printk(KERN_INFO args) ---*/
#define DEB_NOTE(args...)

#define MAX_DEBUG_MESSAGE_LEN 1024
/*------------------------------------------------------------------------------------------*\
 * Ersetzt printk: Aufruf per cat /dev/debug &
 * frueher im UBIK2-Treiber
\*------------------------------------------------------------------------------------------*/
#define TRUE 1
#define FALSE 0

#define SKIP_SPACES(p) while((p) && *(p) && ((*(p) == ' ') || (*(p) == '\t'))) (p)++;
#define AVM_DBG_MODE     "AVM_PRINTK"
#define PRINTK_DBG_MODE  "STD_PRINTK"
#define AVM_DBG_EOF      "AVMDBG_EOF"
#define AVM_DBG_SIGNAL   "AVMDBG_SIGNAL"

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _debug_client {
    void *refdata;
    char *prefix;
    void (*CallBackDebug)(char *string, void *refdata);
    struct _debug_client *next;
};
#define MAX_DEBUG_STACK   8 /*--- count of max nested printk's ---*/
struct _debugstack {
    volatile unsigned int used;
    unsigned char *buf;
};
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct _avm_debug {
    unsigned int init;
    spinlock_t write_lock;
    /*--- spinlock_t read_lock; ---*/
    spinlock_t client_lock;
    spinlock_t synclock;
    atomic_t   open_flag;
             unsigned char *buffer;
    volatile unsigned int read;
    volatile unsigned int write;
    volatile unsigned int wrap;
             unsigned int size;
             unsigned int lost;
    volatile unsigned int is_open;
             unsigned int major;
             unsigned int written;
             dev_t        device;
             struct cdev *cdev;
#if defined(AVM_DEBUG_UDEV)
    struct class *osclass;
#endif/*--- #if defined(AVM_DEBUG_UDEV) ---*/
	wait_queue_head_t recvwait;
    struct _debug_client *dbg_clientAnker;
    unsigned int eof_sync;  /*--- erzwinge EOF bei read ---*/
    struct task_struct *thread_pid;
    unsigned int signal;
    struct _debugstack debugstack[MAX_DEBUG_STACK];
    struct completion on_exit;
    wait_queue_head_t wait_queue;
    struct socket *s_push;
    char comment[64];
} avm_debug;

static int avm_debug_open(struct inode *inode, struct file *filp);
static int avm_debug_close(struct inode *inode, struct file *filp);
static long avm_debug_ioctl(struct file *, unsigned, unsigned long);
static ssize_t avm_debug_read(struct file *filp, char *read_buffer, size_t max_read_length, loff_t *read_pos);
static unsigned int avm_debug_poll(struct file *file, poll_table * wait);
static ssize_t avm_debug_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *write_pos);
static struct _debug_client *find_dbgclient_by_prefix(char *prefix);
static int avm_kernelprintk(const char *format, ...);
static int avm_kernelvprintk(const char *format, va_list args);

#ifdef CONFIG_PRINTK
extern void set_vprintk(int (*__print)(const char * fmt, va_list args)) __attribute__((weak));
static void avmdebug_sync(void);
extern void (*debug_sync)(void) __attribute__ ((weak));
#endif/*--- #ifdef CONFIG_PRINTK ---*/

static int avmdebug_thread( void *data );

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
avm_debug_write_t avm_debug_write_minor[AVM_DEBUG_MAX_MINOR + 1];
avm_debug_read_t avm_debug_read_minor[AVM_DEBUG_MAX_MINOR + 1];
avm_debug_open_t avm_debug_open_minor[AVM_DEBUG_MAX_MINOR + 1];
avm_debug_close_t avm_debug_close_minor[AVM_DEBUG_MAX_MINOR + 1];
avm_debug_ioctl_t avm_debug_ioctl_minor[AVM_DEBUG_MAX_MINOR + 1];

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static ssize_t avm_debug_write_dummy(struct file *filp __attribute__ ((unused)), const char *buff __attribute__ ((unused)), size_t count __attribute__ ((unused)), loff_t *off __attribute__ ((unused))) {
    return -ENOENT;
}

static ssize_t avm_debug_read_dummy(struct file *filp __attribute__ ((unused)), char *buff __attribute__ ((unused)), size_t count __attribute__ ((unused)), loff_t *off __attribute__ ((unused))) {
    return -ENOENT;
}

static long avm_debug_ioctl_dummy(struct file *filp __attribute__ ((unused)), unsigned cmd __attribute__ ((unused)), unsigned long arg __attribute__ ((unused))) {
    return -ENOENT;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct file_operations avm_debug_fops = {
    owner:   THIS_MODULE,
    open:    avm_debug_open,
    release: avm_debug_close,
    write:   avm_debug_write,
    read:    avm_debug_read,
    unlocked_ioctl:   avm_debug_ioctl,
    poll:    avm_debug_poll,
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void avmdebug_lock(spinlock_t *lock, unsigned long *flags) {
    spin_lock_irqsave(lock, *flags);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void avmdebug_unlock(spinlock_t *lock, unsigned long flags) {
    spin_unlock_irqrestore(lock, flags);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_debug_register_minor(int minor, 
                             avm_debug_open_t open,
                             avm_debug_close_t close,
                             avm_debug_write_t write,
                             avm_debug_read_t read,
                             avm_debug_ioctl_t ioctl
                             ) {

    if((minor < 1) || (minor > AVM_DEBUG_MAX_MINOR)) {
        return -ENXIO;
    }

    if((avm_debug_write_minor[minor] != avm_debug_write_dummy) ||
       (avm_debug_read_minor[minor] != avm_debug_read_dummy) ||
       (avm_debug_ioctl_minor[minor] != avm_debug_ioctl_dummy) ||
       (avm_debug_open_minor[minor] != NULL) ||
       (avm_debug_close_minor[minor] != NULL)) {
        return -EEXIST;
    }
    avm_debug_write_minor[minor] = write ? write : avm_debug_write_dummy;
    avm_debug_read_minor[minor]  = read ? read : avm_debug_read_dummy;
    avm_debug_ioctl_minor[minor] = ioctl ? ioctl : avm_debug_ioctl_dummy;
    avm_debug_open_minor[minor]  = open;
    avm_debug_close_minor[minor] = close;
    return 0;
}
EXPORT_SYMBOL(avm_debug_register_minor);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_debug_release_minor(int minor) {
    if((minor < 1) || (minor > AVM_DEBUG_MAX_MINOR)) {
        return -ENXIO;
    }
    avm_debug_write_minor[minor] = avm_debug_write_dummy;
    avm_debug_read_minor[minor]  = avm_debug_read_dummy;
    avm_debug_ioctl_minor[minor] = avm_debug_ioctl_dummy;
    avm_debug_open_minor[minor]  = NULL;
    avm_debug_close_minor[minor] = NULL;
    return 0;
}

EXPORT_SYMBOL(avm_debug_release_minor);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int __init avm_debug_init(void) {
    int reason;
    int minor, i;
    unsigned char *p;

    for(minor = 1 ; minor <= AVM_DEBUG_MAX_MINOR ; minor++) {
        avm_debug_write_minor[minor] = avm_debug_write_dummy;
        avm_debug_read_minor[minor]  = avm_debug_read_dummy;
        avm_debug_ioctl_minor[minor] = avm_debug_ioctl_dummy;
        avm_debug_open_minor[minor]  = NULL;
        avm_debug_close_minor[minor] = NULL;
    }

    memset((void *)&avm_debug, 0, sizeof(avm_debug));
    DEB_INFO("[avm_debug] register_chrdev_region()\n");
#if defined(AVM_DEBUG_UDEV)
    reason = alloc_chrdev_region(&avm_debug.device, 0, AVM_DEBUG_MINOR_COUNT, "debug");
#else /*--- #if defined(AVM_DEBUG_UDEV) ---*/
    avm_debug.device = MKDEV(DEBUG_TRACE_MAJOR, 0);
    reason = register_chrdev_region(avm_debug.device, AVM_DEBUG_MINOR_COUNT, "debug");
#endif
    if(reason) {
        DEB_ERR("[avm_debug] register_chrdev_region failed: reason %d!\n", reason);
        return -ERESTARTSYS;
    }
	avm_debug.cdev = cdev_alloc();
	if (!avm_debug.cdev) {
        unregister_chrdev_region(avm_debug.device, AVM_DEBUG_MINOR_COUNT);
        DEB_ERR("[avm_debug] cdev_alloc failed!\n");
        return -ERESTARTSYS;
    }
    spin_lock_init(&avm_debug.client_lock);
    /*--- spin_lock_init(&avm_debug.read_lock); ---*/
    spin_lock_init(&avm_debug.write_lock);
    spin_lock_init(&avm_debug.synclock);
    init_waitqueue_head(&avm_debug.recvwait);

	avm_debug.cdev->owner = avm_debug_fops.owner;
	avm_debug.cdev->ops = &avm_debug_fops;
	kobject_set_name(&(avm_debug.cdev->kobj), "debug");
		
    avm_debug.size = 64 * 1024;
    /*--- avm_debug.size = 1024; ---*/
    avm_debug.buffer = kmalloc(avm_debug.size + (MAX_DEBUG_STACK * MAX_DEBUG_MESSAGE_LEN), GFP_KERNEL);
    if(avm_debug.buffer == NULL) {
        DEB_ERR("[avm_debug] Could not allocate debug buffer space!\n");
        return -ENOMEM;
    }
    DEB_INFO("[avm_debug] major %d (success)\n", MAJOR(avm_debug.device));
    p = avm_debug.buffer + avm_debug.size;
    for(i = 0; i< MAX_DEBUG_STACK; i++) {
        avm_debug.debugstack[i].used = 0;
        avm_debug.debugstack[i].buf = p;
        p += MAX_DEBUG_MESSAGE_LEN;
    }
	if(cdev_add(avm_debug.cdev, avm_debug.device, AVM_DEBUG_MINOR_COUNT)) {
        kobject_put(&avm_debug.cdev->kobj);
        unregister_chrdev_region(avm_debug.device, AVM_DEBUG_MINOR_COUNT);
        DEB_ERR("[avm_debug] cdev_add failed!\n");
        return -ERESTARTSYS;
    }
#if defined(AVM_DEBUG_UDEV)
    /*--- Geraetedatei anlegen: ---*/
    avm_debug.osclass = class_create(THIS_MODULE, "debug");
    device_create(avm_debug.osclass, NULL, 1, NULL, "%s%d", "debug", 0);
#endif/*--- #if defined(AVM_DEBUG_UDEV) ---*/
#ifdef CONFIG_PRINTK
    if(!IS_ERR(&debug_sync) && &debug_sync) {/*--- Die Adresse der Variable muss auf Fehler und NULL geprueft werden ---*/
        debug_sync = avmdebug_sync;
    }
#endif/*--- #ifdef CONFIG_PRINTK ---*/
    init_waitqueue_head(&avm_debug.wait_queue);
    init_completion(&avm_debug.on_exit);
	kernel_thread(avmdebug_thread, (void *) &avm_debug, CLONE_SIGHAND);
    atomic_set(&avm_debug.open_flag, 0);
    avm_debug.init = 1;
    return 0;
}
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
void avm_debug_cleanup(void) {
    if(avm_debug.init == 0) {
        return;
    }
    avm_debug.init = 0;
    if(avm_debug.thread_pid) {
		send_sig(SIGTERM, avm_debug.thread_pid, 1);
        avm_debug.thread_pid = NULL;
        wait_for_completion( &avm_debug.on_exit );
    }
    DEB_INFO("[avm_debug] unregister_chrdev(%u)\n", MAJOR(avm_debug.device));
#if defined(AVM_DEBUG_UDEV)
    device_destroy(avm_debug.osclass, 1);
    class_destroy(avm_debug.osclass);
#endif/*--- #if defined(AVM_DEBUG_UDEV) ---*/
    cdev_del(avm_debug.cdev);
    unregister_chrdev_region(avm_debug.device, AVM_DEBUG_MINOR_COUNT);
    if(avm_debug.buffer != NULL)
        vfree(avm_debug.buffer);
}
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_debug_open(struct inode *inode, struct file *filp) {
    int minor = MINOR(inode->i_rdev);
    filp->private_data = (void *)minor;

    if(avm_debug_open_minor[minor]) {
        return (*avm_debug_open_minor[minor])(inode, filp);
    }
    if(filp->f_mode & FMODE_READ){
        if(atomic_add_return(1, &avm_debug.open_flag) > 1) {
            return -EBUSY;
        }
    }
    DEB_INFO("[avm_debug]: avm_debug_open:\n");
    return 0;
}
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_debug_close(struct inode *inode, struct file *filp) {
    unsigned int minor = (unsigned int)filp->private_data;
    DEB_INFO("[avm_debug]: avm_debug_close:\n");

    if(avm_debug_close_minor[minor]) {
        return (*avm_debug_close_minor[minor])(inode, filp);
    }
    atomic_set(&avm_debug.open_flag, 0);
    return 0;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static long avm_debug_ioctl(struct file *filp, unsigned cmd, unsigned long args) {
    unsigned int minor = (unsigned int)filp->private_data;

    if(avm_debug_ioctl_minor[minor])
        return (*avm_debug_ioctl_minor[minor])(filp, cmd, args);

    return -ENXIO;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned int inc_idx(unsigned int idx, unsigned int max_idx) {
    if(++idx >= max_idx) {
        return 0;
    }
    return idx;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static ssize_t avm_debug_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *write_pos) {
    char Buffer[512], *p, *pa = NULL;
    unsigned int org_write_length __attribute__((unused));
    unsigned int minor = (unsigned int)filp->private_data;

    if(avm_debug_write_minor[minor])
        return (*avm_debug_write_minor[minor])(filp, write_buffer, write_length, write_pos);
     
    if(write_pos != NULL) {
        DEB_INFO("[avm_debug]: write_length = %u *write_pos = 0x%LX\n", write_length, *write_pos);
    }
    org_write_length = write_length;

    if(write_length >= sizeof(Buffer)) {
        write_length = sizeof(Buffer) - 1;
        DEB_NOTE("[avm_debug] long line reduce to %u bytes\n", write_length);
    }
    if(filp == NULL) {
        memcpy(Buffer, write_buffer, write_length);
    } else {
        if(copy_from_user(Buffer, write_buffer, write_length)) {
            DEB_ERR("[avm_debug]: write: copy_from_user failed\n");
            return -EFAULT;
        }
    }
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    Buffer[write_length] = '\0';
    DEB_NOTE("[avm_debug] org_len=%u len %u = '%s'\n", org_write_length, write_length, Buffer);
    p = strchr(Buffer, 0x0A);
    if(p) {
        *p = '\0';
        pa = p + 1;
        write_length = strlen(Buffer) + 1;
        DEB_NOTE("[avm_debug] multi line reduce to %u bytes. remain %s\n", write_length, pa);
    }
    p = Buffer;

    /*--------------------------------------------------------------------------------------*\
     * cmd extrahieren
    \*--------------------------------------------------------------------------------------*/
    SKIP_SPACES(p);
    if(!strncmp(AVM_DBG_MODE, p, sizeof(AVM_DBG_MODE) - 1)) {
        p += sizeof(AVM_DBG_MODE) - 1;

        SKIP_SPACES(p);
#ifdef CONFIG_PRINTK
        printk(KERN_ERR"\n[avm_debug]redirect kernel-messages (/dev/debug)\n");
        set_printk(avm_kernelprintk);
        if(!IS_ERR(&set_vprintk) && &set_vprintk) {
            set_vprintk(avm_kernelvprintk);
        }
#endif /*--- #ifdef CONFIG_PRINTK ---*/
    } else if(!strncmp(PRINTK_DBG_MODE, p, sizeof(PRINTK_DBG_MODE) - 1)) {
#ifdef CONFIG_PRINTK
        __printk("\n[avm_debug]standard kernel-messages\n");
        restore_printk();   /*--- alle weiteren Ausgaben nur noch über standard-printk ---*/
#endif /*--- #ifdef CONFIG_PRINTK ---*/
    } else if(!strncmp(AVM_DBG_EOF, p, sizeof(AVM_DBG_EOF) - 1)) {
        int val;
        p += sizeof(AVM_DBG_EOF) - 1;
        SKIP_SPACES(p);
        val = (*p == '1') ? 1 : 0;
        avm_debug.eof_sync = val;
        if(avm_debug.eof_sync) {
            /*--- unsigned long flags; ---*/
            /*--- Debugbuffer nochmal reaktivieren ---*/
            /*--- read_lock nutzen weil der 'read' Poiner manipuliert werden soll ---*/
            /*--- avmdebug_lock(&avm_debug.read_lock, &flags); ---*/
            if(avm_debug.written >= avm_debug.size) {
                avm_debug.read = inc_idx(avm_debug.write, avm_debug.size);
            } else {
                avm_debug.read = 0;
            }
            /*--- avmdebug_unlock(&avm_debug.read_lock, flags); ---*/
            printk(KERN_ERR"---> reanimated debugbuffer: read=%d write=%d, written=%d <---\n", avm_debug.read, avm_debug.write, avm_debug.written);
        }
        /*--- printk(KERN_ERR"\n[avm_debug]eofsync %d\n", avm_debug.eof_sync); ---*/
    } else if(!strncmp(AVM_DBG_SIGNAL, p, sizeof(AVM_DBG_SIGNAL) - 1)) {
        int val = -1;
        p += sizeof(AVM_DBG_SIGNAL) - 1;
        SKIP_SPACES(p);
        if(*p) sscanf(p, "%d", &val);
        if(val != -1) {
            if(pa) avm_DebugPrintf("avm_DebugSignal: %s\n", pa);
            avm_DebugSignal(val | 0x80000000);
        }
    } else {
        struct _debug_client *pdbg = find_dbgclient_by_prefix(p);
        if(pdbg) {
            pdbg->CallBackDebug(p + strlen(pdbg->prefix), pdbg->refdata);
#ifdef CONFIG_PRINTK
        } else {
            printk(KERN_ERR"\n[avm_debug]unknown mode: use: %s, %s or %s <on>\n", PRINTK_DBG_MODE, AVM_DBG_MODE, AVM_DBG_EOF);
#endif /*--- #ifdef CONFIG_PRINTK ---*/
        }
    }
    return write_length;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _debug_client *find_dbgclient_by_prefix(char *prefix){
    struct _debug_client *pdbg;
    unsigned long flags;

    avmdebug_lock(&avm_debug.client_lock, &flags);
    pdbg = avm_debug.dbg_clientAnker;
    while(pdbg) {
        if(strncmp(prefix, pdbg->prefix, strlen(pdbg->prefix)) == 0) {
            avmdebug_unlock(&avm_debug.client_lock, flags);
            return pdbg;
        }
        pdbg = pdbg->next;
    }
    avmdebug_unlock(&avm_debug.client_lock, flags);
    return NULL;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _debug_client *add_dbgclient(char *prefix, void (*CallBackDebug)(char *string, void *refdata), void *refdata){
    struct _debug_client *pdbg;
    unsigned long flags;
    pdbg = kmalloc(sizeof(struct _debug_client) + strlen(prefix) + 1, GFP_KERNEL);
    if(pdbg == NULL) {
        return NULL;
    }
    pdbg->CallBackDebug = CallBackDebug;
    pdbg->refdata       = refdata;
    pdbg->prefix        = (char *)pdbg + sizeof(struct _debug_client);
    strcpy(pdbg->prefix, prefix);
    pdbg->next = NULL;
    avmdebug_lock(&avm_debug.client_lock, &flags);
    pdbg->next                = avm_debug.dbg_clientAnker;
    avm_debug.dbg_clientAnker = pdbg;
    avmdebug_unlock(&avm_debug.client_lock, flags);
    return pdbg;
}
/*-------------------------------------------------------------------------------------*\
 * Debug-Funktion am Treiber anmelden
 * prefix: der Inputdaten werden nach diesem Prefix durchsucht, und bei Match 
 * wird die CallbackFkt aufgerufen
 * um also den string 'blabla=haha' zum Treiber angemeldet mit prefix 'unicate_' zu transportieren
 * ist einfach ein "echo unicate_blabla=haha >/dev/debug" auf der Konsole auszufuehren
 * ret: handle (fuer UnRegister)
\*-------------------------------------------------------------------------------------*/
void *avm_DebugCallRegister(char *prefix, void (*CallBackDebug)(char *string, void *refdata), void *refdata){
    struct _debug_client *client;
    DEB_INFO("[avm_debug] DebugCallRegister(\"%s\", 0x%p, %p)\n",prefix, CallBackDebug, refdata);

    if(prefix == NULL || CallBackDebug == NULL) {
        DEB_ERR("[avm_debug] DebugCallRegister(\"%s\", 0x%p, %p): invalid param\n",prefix, CallBackDebug, refdata);
        return NULL;
    }
    SKIP_SPACES(prefix);
    client = find_dbgclient_by_prefix(prefix);
    if(client) {
        DEB_ERR("[avm_debug]DebugCallRegister: prefix '%s' already exist\n", prefix);
        return NULL;
    }
    return add_dbgclient(prefix, CallBackDebug, refdata);
}
EXPORT_SYMBOL(avm_DebugCallRegister);

/*--------------------------------------------------------------------------------*\
 * Debug-Funktion am Treiber abmelden
\*--------------------------------------------------------------------------------*/
void avm_DebugCallUnRegister(void *handle){
    struct _debug_client *pdbg, *prev = NULL;
    unsigned long flags;
    DEB_INFO("[avm_debug]avm_DebugCallUnRegister: %p done\n", handle);
    avmdebug_lock(&avm_debug.client_lock, &flags);
    pdbg = avm_debug.dbg_clientAnker;
    while(pdbg) {
        if(pdbg == handle) {
            if(prev == NULL) {
                avm_debug.dbg_clientAnker = pdbg->next;
            } else {
                prev->next = pdbg->next;
            }
            avmdebug_unlock(&avm_debug.client_lock, flags);
            kfree(pdbg);
            DEB_INFO("[avm_debug]avm_DebugCallUnRegister: %p done\n", pdbg);
            return;
        }
        prev = pdbg;
        pdbg = pdbg->next;
    }
    avmdebug_unlock(&avm_debug.client_lock, flags);
    DEB_ERR("[avm_debug]avm_DebugCallUnRegister: error: no handle for %p found\n", pdbg);
}
EXPORT_SYMBOL(avm_DebugCallUnRegister);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int avm_debug_poll(struct file *file, poll_table * wait) {
    unsigned int mask = POLLOUT;

    poll_wait(file, &avm_debug.recvwait, wait);
	if (avm_debug.read != avm_debug.write) {
        mask |= POLLIN | POLLRDNORM;
    }
    return mask;
}
#define AVM_DBGWRAP_STR "[AVMDBG_OVR]"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static ssize_t avm_debug_read(struct file *filp, char *read_buffer, size_t max_read_length, loff_t *read_pos) {
    unsigned int copy_length = 0, wrap_length = 0;
    unsigned int local_read, local_write;
    /*--- unsigned long flags, locked = 0; ---*/

    if(filp) {
        unsigned int minor = (unsigned int)filp->private_data;
        if(avm_debug_read_minor[minor])
            return (*avm_debug_read_minor[minor])(filp, read_buffer, max_read_length, read_pos);
    }
    for( ;; ) {
        if(avm_debug.read == avm_debug.write) {
            if(filp == NULL) {
                return copy_length;
            }
            if(avm_debug.eof_sync) {
                /*--- erzwinge Beenden von cat etc. ---*/
                /*--- __printk("avm_debug_read: end"); ---*/
                return -EPIPE;
            }
            if (filp->f_flags & O_NONBLOCK) {
                return -EAGAIN;
            }
            interruptible_sleep_on(&avm_debug.recvwait);
            if (signal_pending(current)) {
                return -ERESTARTNOHAND;
            }
            continue;
        }
        /*--- avmdebug_lock(&avm_debug.read_lock, &flags); ---*/
        local_read = avm_debug.read;
        local_write = avm_debug.write;

        if(local_read < local_write) {
            copy_length = local_write - local_read;
        } else {
            copy_length = avm_debug.size - local_read;
        }
        /*--- avmdebug_unlock(&avm_debug.read_lock, flags); ---*/

        if(avm_debug.wrap) {
            avm_debug.wrap = 0;
            if(max_read_length >= sizeof(AVM_DBGWRAP_STR) - 1) {
                wrap_length      = sizeof(AVM_DBGWRAP_STR) - 1;
                if(filp == NULL) {
                    if(read_buffer)memcpy(read_buffer, AVM_DBGWRAP_STR, wrap_length);
                } else {
                    if(copy_to_user(read_buffer, AVM_DBGWRAP_STR, wrap_length)) {
                        return -EFAULT;
                    }
                }
                max_read_length -= wrap_length;
                if(read_buffer)read_buffer     += wrap_length;
            }
        }
        if(copy_length > max_read_length) {
            copy_length = max_read_length;
        }
        if(filp == NULL) {
            if (read_buffer) {
                memcpy(read_buffer, avm_debug.buffer + local_read, copy_length);
            }
        } else {
            if(copy_to_user(read_buffer, avm_debug.buffer + local_read, copy_length)) {
                /*--- DEB_ERR("[avm_debug]: copy_to_user failed (read_pos %llu / copy length %u)\n", read_pos ? *read_pos : 0, copy_length); ---*/
                return -EFAULT;
            }
        }
        /*--- avmdebug_lock(&avm_debug.read_lock, &flags); ---*/
        if(avm_debug.read + copy_length >= avm_debug.size)
            avm_debug.read = 0;
        else
            avm_debug.read += copy_length;
        /*--- avmdebug_unlock(&avm_debug.read_lock, flags); ---*/
        break;
    }
    if(read_pos)*read_pos += copy_length + wrap_length;
    return copy_length + wrap_length;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int avm_debugfill(void) {
    unsigned long size;
    
    if(avm_debug.read > avm_debug.write) {
        size = avm_debug.size - (avm_debug.read - avm_debug.write);
    } else {
        size = avm_debug.write - avm_debug.read;
    }
    return size;
}
/*------------------------------------------------------------------------------------------*\
 * ret: letzte Zeichen war \r bzw. \n
\*------------------------------------------------------------------------------------------*/
int DebugPrintf_Puts(char *DebugData, unsigned int length) {
    unsigned int local_read, local_write, wrap = 0;
    unsigned long flags;
    int ret = 0;

    if((DebugData == NULL) || (length == 0)) {
        return 0;
    }
    if(*(DebugData+length-1) == '\r' ||
       *(DebugData+length-1) == '\n' ) {
        ret = 1;
    }
    avmdebug_lock(&avm_debug.write_lock, &flags);
    local_write = avm_debug.write;
    local_read  = avm_debug.read;
    avm_debug.written += length;
    while(length--) {
        avm_debug.buffer[local_write] = *DebugData++;
        local_write = inc_idx(local_write, avm_debug.size);
        if(local_write == local_read) {
            wrap++;
        }
    }
    if(wrap) {
        avm_debug.read = inc_idx(local_write, avm_debug.size);
        avm_debug.wrap = wrap;
    }
    avm_debug.write = local_write;
    avmdebug_unlock(&avm_debug.write_lock, flags);
    wake_up(&avm_debug.recvwait);
    return ret;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static char *itoa(unsigned int zahl, char *Buffer, unsigned int base) {
    char tmp[sizeof(unsigned int) * 8 + 2];
    char ch;
    char *Ptr = Buffer;
    unsigned int Len = 0;
    static const char HexTab[] = "0123456789ABCDEF";

    if(zahl == 0) {
        Buffer[0] = '0';
        Buffer[1] = '\0';
        return Buffer;
    }

    Buffer[0] = '\0';

    switch(base) {
        case 16:
            while(zahl) {
                tmp[Len] = HexTab[zahl & 0x0F];
                zahl >>= 4;
                Len++;
            }
            break;

        case 8:
            while(zahl) {
                tmp[Len] = HexTab[zahl & 0x07];
                zahl >>= 3;
                Len++;
            }
            break;

        case 2:
            while(zahl) {
                tmp[Len] = HexTab[zahl & 0x01];
                zahl >>= 1;
                Len++;
            }
            break;

        case 10:
            while(zahl) {
                ch = (char)(zahl % 10);
                zahl /= 10;
                if(ch <= 9)
                    tmp[Len] = (char)(ch + '0');
                else
                    tmp[Len] = (char)(ch + 'A' - 10);
                Len++;
            }
            break;

        default:
            while(zahl) {
                ch = (char)(zahl % base);
                zahl /= base;
                if(ch <= 9)
                    tmp[Len] = (char)(ch + '0');
                else
                    tmp[Len] = (char)(ch + 'A' - 10);
                Len++;
            }
    }

    while(Len) {
        *Ptr++ = tmp[--Len];
    }
    *Ptr = '\0';
    return Buffer;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int atoi (const char *nptr) {   
    int num = 0, neg = 0;
    while(*nptr && (*nptr == ' ' || *nptr == '\t'))
    if(*nptr == '\0')
        nptr++;

    switch(*nptr) {
        case '\0':
            return 0;
        case '-':
            neg = 1;
            nptr++;
            break;
        case '+':
            neg = 0;
            nptr++;
            break;
    }

    while(*nptr && *nptr >= '0' && *nptr <= '9') {
        num = (10 * num) + (*nptr - '0');
        nptr++;
    }

    return neg ? -num : num;
}

#define avm_LimitOut(ActLimit) if((int)pud->Pos >= (int)(ActLimit)) {*(DebugData + pud->Pos) = '\0'; DebugPrintf_Puts(DebugData, pud->Pos); pud->Sum += pud->Pos;pud->Pos = 0;}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
struct _avmdebug_datahandler {
    unsigned int Pos; 
    unsigned int Sum; 
    int field_length, field_prec;
    enum { no_extension = '\0', name_of_text_symbol = 'S', name_of_function_pointer ='F', adress_range_in_struct_resource='R', dump_memory = 'B'} p_ext;
    unsigned char NextIsLong;
    unsigned char FillZero;
    unsigned char Leftjust;
    char SetSign;
};

/*-------------------------------------------------------------------------------------*\
  %[0-+ #:*][prec/len][l]d
\*-------------------------------------------------------------------------------------*/
static const char *avmdebug_parse_percent(const char *format, struct _avmdebug_datahandler *pud, va_list *marker) {
    format++;
    pud->field_length = 0;
    pud->field_prec   = 0; 
    pud->Leftjust     = FALSE; 
    pud->SetSign      = 0;
    pud->FillZero     = FALSE;
    while ((*format == '-') || (*format == '+') || 
           (*format == ' ') || (*format == '0') || 
           (*format == '#') || (*format == ':')) {
        switch(*format) {
            case '-': 
                pud->Leftjust = TRUE;
                break;
            case ' ': 
                if (pud->SetSign == 0) pud->SetSign = ' ';
                break;
            case '0': 
                if (pud->Leftjust == FALSE) pud->FillZero = TRUE;
                break;
            case '+':
            case '#': 
            case ':': 
                pud->SetSign = *format;
        }
        format++;
    }
    if (*format == '*') {
        format++;
        pud->field_length = (va_arg(*marker, int));
    } else if(*format >= '0' && *format <= '9') { /*--- mindestanzahl der zahllaenge ---*/
        pud->field_length = atoi(format);
        while(*format >= '0' && *format <= '9') {
            format++;
        }
        if(pud->field_length < 0) {
            pud->field_length = 0;
        }
    }
    if(*format == '.') { /*--- ignorieren ---*/
        format++;
        if (*format == '*') {
            format++;
            pud->field_prec = (va_arg(*marker, int));
        } else {
            pud->field_prec = atoi(format);
            while(*format >= '0' && *format <= '9') {
                format++;
            }
        }
        if(pud->field_prec < 0) {
            pud->field_prec = 0;
        }
    }
    switch(*format) {
        case 'F':
        case 'N':
        case 'h':
            pud->NextIsLong = FALSE;
            format++;
            break;
        case 'l':
            pud->NextIsLong = FALSE;
            format++;
            if((*format) != 'l') {
                break;
            }
            /*--- kein break ---*/
        case 'L':
            pud->NextIsLong = TRUE;
            format++;
            break;
        case 'z':
            if(sizeof(size_t) == sizeof(unsigned long long)) {
                pud->NextIsLong = TRUE;
            } else {
                pud->NextIsLong = FALSE;
            }
            format++;
            break;
        case 'Z':
            if(sizeof(size_t) == sizeof(unsigned long long)) {
                pud->NextIsLong = TRUE;
            } else {
                pud->NextIsLong = FALSE;
            }
            format++;
            break;
    }
    if ((pud->field_prec > pud->field_length) && (*format != 's')) {
         pud->field_length = pud->field_prec; 
    }
    return format;
}
    
/*-------------------------------------------------------------------------------------*\
 * auch für bin, octal
\*-------------------------------------------------------------------------------------*/
static void avmdebug_set_uint(char *DebugData, struct _avmdebug_datahandler *pud, va_list *marker, unsigned int mode) {
    int Len;
    unsigned int Value;
    char Data[66];

    if(pud->NextIsLong == TRUE) {
        unsigned long long lValue;
        lValue = va_arg(*marker, long long);
        sprintf(Data, "%llu", lValue);
        Value = lValue ? 1 : 0;
    } else {
        Value = va_arg(*marker, int); 
        itoa(Value, Data, mode);
    }
    if((pud->SetSign != 0)) {
        if(mode == 10)
            *(DebugData + pud->Pos) = pud->SetSign;
        else if((mode == 8) && (Value != 0))
            *(DebugData + pud->Pos) = '0';
        pud->Pos++;
    }
    Len = strlen(Data);
    if(pud->Leftjust == TRUE) { /*--- linksbuendig ---*/
        memcpy((unsigned char *)(DebugData + pud->Pos), (unsigned char *)Data, Len);
        pud->Pos += Len;
    }
    while(pud->field_length > Len) {
        avm_LimitOut(MAX_DEBUG_MESSAGE_LEN - 2);
        if(pud->FillZero && (pud->Leftjust == FALSE))
            *(DebugData + pud->Pos) = '0';
        else
            *(DebugData + pud->Pos) = ' ';
        pud->Pos++;
        pud->field_length--;
    }
    if(pud->Leftjust == FALSE) { /*--- rechtsbuendig ---*/
        memcpy((unsigned char *)(DebugData + pud->Pos), (unsigned char *)Data, Len);
        pud->Pos += Len;
    }
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void avmdebug_set_int(char *DebugData, struct _avmdebug_datahandler *pud, va_list *marker) {
    int Len;
    char Data[66];
    int Variable;

    if(pud->NextIsLong == TRUE) {
        signed long long lValue;
        lValue = va_arg(*marker, long long);
        sprintf(Data, "%lld", lValue);
    } else {
        Variable = va_arg(*marker, int);
        if((signed int)Variable < 0) {
            *(DebugData + pud->Pos) = '-';
            Variable = (unsigned int)(0 - (signed int)Variable);
            pud->Pos++;
        } else {
            if(pud->SetSign != 0) {
                *(DebugData + pud->Pos) = pud->SetSign;
                pud->Pos++;
            }
        }
        itoa((unsigned int)Variable, Data, 10);
   }
   Len = strlen(Data);
   if(pud->Leftjust == TRUE) { /*--- linksbuendig ---*/
       memcpy((unsigned char *)(DebugData + pud->Pos), (unsigned char *)Data, Len);
       pud->Pos += Len;
   }
   while(pud->field_length > Len) {
       avm_LimitOut(MAX_DEBUG_MESSAGE_LEN - 2);
       if(pud->FillZero && (pud->Leftjust == FALSE))
           *(DebugData + pud->Pos) = '0';
       else
           *(DebugData + pud->Pos) = ' ';
       pud->Pos++;
       pud->field_length--;
   }
   if(pud->Leftjust == FALSE) { /*--- rechtsbuendig ---*/
       memcpy((unsigned char *)(DebugData + pud->Pos), (unsigned char *)Data, Len);
       pud->Pos += Len;
   }
}
extern char *module_alloc_find_module_name(char *buff, char *end, unsigned long addr) __attribute__ ((weak));
/*-------------------------------------------------------------------------------------*\
 * auch für hex, pointer (mode = 1)
\*-------------------------------------------------------------------------------------*/
static void avmdebug_set_hex(char *DebugData, struct _avmdebug_datahandler *pud, va_list *marker, unsigned int mode) {
    char Data[16 + 1]; /*--- maximale Stellen einer 64 Bit hexzahl + 2 ---*/
    int Len;
    unsigned int Val = 0;

    if(pud->NextIsLong == TRUE) {
        signed long long lValue;
        lValue = va_arg(*marker, long long);
        sprintf(Data, "%llx", lValue);
    } else {
        Val = va_arg(*marker, int); 
        if((mode == 1) && (Val == 0)) {
            strcpy(Data, "(null)");
        } else {
            itoa(Val, Data, 16);
        }
    }
    if(mode == 1) {
        char *modname;
        const char *name;
        unsigned long offset, size;
        char tmp[256];
        switch(pud->p_ext) {
            case name_of_function_pointer: 
                /*--- ignore: only ia64 und ppc: ---*/
                /*--- Val = dereference_function_descriptor(Val); ---*/
                /*--- kein break; ---*/
            case name_of_text_symbol:
#ifdef CONFIG_KALLSYMS
                name = kallsyms_lookup(Val, &size, &offset, &modname, tmp);
                if(!name) {
                    break;
                }
                if(modname) {
                    Len = snprintf(tmp, sizeof(tmp), "%s+%#lx/%#lx [%s]", name, offset, size, modname);
                }else {
                    Len = snprintf(tmp, sizeof(tmp), "%s+%#lx/%#lx", name, offset, size);
                }
#else
                if(!IS_ERR(module_alloc_find_module_name)) {
                    name = module_alloc_find_module_name(tmp, tmp + sizeof(tmp), Val);
                } else {
                    break;
                }
                Len = name - tmp;
#endif
                avm_LimitOut(Len + 1);
                strcpy((unsigned char *)(DebugData + pud->Pos), (unsigned char *)tmp);
                pud->Pos += Len;
                return;
            case adress_range_in_struct_resource:
                /*--- ignore ---*/
                break;
            case dump_memory:
                /* Already done */
                return;
            default:
                break;
        }
    }
    Len = strlen(Data);
    if((pud->Leftjust == TRUE) || (pud->field_length <= Len)) { /*--- linksbuendig ---*/
        if (pud->SetSign == '#') {
            *(DebugData + pud->Pos++) = '0';
            *(DebugData + pud->Pos++) = 'x';
        } 
        memcpy((unsigned char *)(DebugData + pud->Pos), (unsigned char *)Data, Len);
        pud->Pos += Len;
        while(pud->field_length > Len) {
            avm_LimitOut(MAX_DEBUG_MESSAGE_LEN - 2);
            *(DebugData + pud->Pos++) = ' ';
            pud->field_length--;
        }
    } else {
        if(pud->FillZero) {
            if (pud->SetSign == '#') {
                /*--- field_length -= 2; ---*/
                *(DebugData + pud->Pos++) = '0';
                *(DebugData + pud->Pos++) = 'x';
            }
            while(pud->field_length > Len) {
                avm_LimitOut(MAX_DEBUG_MESSAGE_LEN - 2);
                *(DebugData + pud->Pos++) = '0';
                pud->field_length--;
            }
        } else {
            while(pud->field_length > Len) {
                avm_LimitOut(MAX_DEBUG_MESSAGE_LEN - 2);
                *(DebugData + pud->Pos++) = ' ';
                pud->field_length--;
            }
            if (pud->SetSign == '#') {
                *(DebugData + pud->Pos++) = '0';
                *(DebugData + pud->Pos++) = 'x';
            }
        } 
        avm_LimitOut(MAX_DEBUG_MESSAGE_LEN - Len - 1);
        memcpy((unsigned char *)(DebugData + pud->Pos), (unsigned char *)Data, Len);
        pud->Pos += Len;
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int check_memory_pointer(void *addr){
    int ret = 0;
#if defined(CONFIG_MIPS_UR8)
    if((((unsigned int)addr < 0x94000000) || ((unsigned int)addr >= 0x98000000)) &&
       (((unsigned int)addr < 0xA4000000) || ((unsigned int)addr >= 0xA4008000))) {
        if(vmalloc_to_page(addr) == NULL) {
            ret = 1;
        }
    }
#elif defined(CONFIG_MIPS)
    if((((unsigned int)addr < 0x80000000))) {
        if(vmalloc_to_page(addr) == NULL) {
            ret = 1;
        }
    }
#elif defined(CONFIG_ARM) /*--- #if defined(CONFIG_MIPS) ---*/
    if((unsigned int)addr < TASK_SIZE) {
        ret = 1;
    } else if(((unsigned int)addr >= VMALLOC_START) && ((unsigned int)addr < VMALLOC_END)) {
        if(vmalloc_to_page(addr) == NULL) {
            ret = 1;
        }
    }
#endif/*--- #elif defined(CONFIG_ARM)  ---*//*--- #if defined(CONFIG_MIPS) ---*/
    return ret;
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void avmdebug_set_hexfield(char *DebugData, struct _avmdebug_datahandler *pud, va_list *marker) {
    unsigned char *B;
    char Hex[] = "0123456789ABCDEF";
    char tmp[32];

    B = (va_arg(*marker, unsigned char *));
    if (pud->field_length == 0) {
        memcpy((unsigned char *)(DebugData + pud->Pos), "(0)", sizeof("(0)") - 1);
        pud->Pos += sizeof("(0)") - 1;
        return;
    }
    if(B == NULL) {
        memcpy((unsigned char *)(DebugData + pud->Pos), "(null)", sizeof("(null)") - 1);
        pud->Pos += sizeof("(null)") - 1;
        return;
    }
    if(check_memory_pointer(B)){
        sprintf(tmp, "(inval=0x%x)", (unsigned int)B);
        B = tmp;
    }
    if(pud->Leftjust == TRUE) { /*--- reverse ---*/
        B += pud->field_length - 1;
    }
    while(pud->field_length--) {
        avm_LimitOut(MAX_DEBUG_MESSAGE_LEN - 10);
        if(pud->SetSign == '0') {
            *(DebugData + pud->Pos++) = '0';
            *(DebugData + pud->Pos++) = 'x';
        }
        *(DebugData + pud->Pos++) = Hex[*B >> 4];
        *(DebugData + pud->Pos++) = Hex[*B & 0x0F];
        if(pud->Leftjust == TRUE) { /*--- reverse ---*/
            B--;
        } else {
            B++;
        }
        if(pud->field_length) {
            if(pud->SetSign == ':')
                *(DebugData + pud->Pos++) = ':';
            else
                *(DebugData + pud->Pos++) = ' ';
        }
    }
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void avmdebug_set_string(char *DebugData, struct _avmdebug_datahandler *pud, va_list *marker) {
    char tmp[32];
    unsigned int Len = 0;
    char *pstring = (va_arg(*marker, char *)), *ptmp;

    if(pstring == NULL) {
        pstring = "(null)";
    }
    if(check_memory_pointer(pstring)){
        sprintf(tmp, "(inval=0x%x)", (unsigned int)pstring);
        pstring = tmp;
    }
    if (pud->field_prec == 0) {
        pud->field_prec = MAX_DEBUG_MESSAGE_LEN - pud->Pos - 2;
    }
    ptmp = pstring;
    while(*ptmp++ && (Len < (unsigned)pud->field_prec)) {
        Len++;
    }
    if(pud->Leftjust == TRUE) { /*--- linksbuendig ---*/
        pud->field_length -= Len;
        while(Len) {
            unsigned int LimitLen = MAX_DEBUG_MESSAGE_LEN - 1 - pud->Pos;
            if(LimitLen > Len) {
                LimitLen = Len;
            }
            memcpy((unsigned char *)(DebugData + pud->Pos), (unsigned char *)pstring, LimitLen);
            pud->Pos += LimitLen;
            Len      -= LimitLen;
            avm_LimitOut(MAX_DEBUG_MESSAGE_LEN - Len - 1);
        }
    }
    while((unsigned)pud->field_length > Len) {
        avm_LimitOut(MAX_DEBUG_MESSAGE_LEN - 2);
        if(pud->FillZero && (pud->Leftjust == FALSE))
            *(DebugData + pud->Pos) = '0';
        else
            *(DebugData + pud->Pos) = ' ';
        pud->Pos++;
        pud->field_length--;
    }
    if(pud->Leftjust == FALSE) { /*--- rechtsbuendig ---*/
        while(Len) {
            unsigned int LimitLen = MAX_DEBUG_MESSAGE_LEN - 1 - pud->Pos;
            if(LimitLen > Len) {
                LimitLen = Len;
            }
            memcpy((unsigned char *)(DebugData + pud->Pos), (unsigned char *)pstring, LimitLen);
            pud->Pos += LimitLen;
            Len      -= LimitLen;
            avm_LimitOut(MAX_DEBUG_MESSAGE_LEN - Len - 1);
        }
    }
}

#if (MAX_DEBUG_MESSAGE_LEN < 127)
    #error MAX_DEBUG_MESSAGE_LEN zu klein ( <127 )!!!! 
#endif
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
unsigned char *alloc_debugstack(unsigned int **used) {
    unsigned long flags;
    unsigned int i;
    for(i = 0; i < MAX_DEBUG_STACK; i++) { 
        avmdebug_lock(&avm_debug.write_lock, &flags);
        if(avm_debug.debugstack[i].used == 0) {
            avm_debug.debugstack[i].used = 1;
            avmdebug_unlock(&avm_debug.write_lock, flags);
            *used = (unsigned int *)&avm_debug.debugstack[i].used;
            return avm_debug.debugstack[i].buf;
        } else {
            avmdebug_unlock(&avm_debug.write_lock, flags);
        }
    }
    return NULL;
}

#if defined(CONFIG_SMP)
#define CPU_ARGUMENT_STRING()  "[%x] "
#define CPU_ID()    ,smp_processor_id()
#else
#define CPU_ARGUMENT_STRING() " "
#define CPU_ID()    
#endif/*--- #if defined(CONFIG_SMP) ---*/

#if defined(CONFIG_PRINTK_TIME)
#define TIME_ARGUMENT_STRING()  "[%5lu.%06lu]" 
#define TIME_ARGUMENT()         (unsigned long)(clk), clk_rem / 1000
#else
#define TIME_ARGUMENT_STRING()  "[%08lu]"
#define TIME_ARGUMENT()         (unsigned long)(jiffies)
#endif
/*-------------------------------------------------------------------------------------*\
 * Mode: in: 0x1 print timestamp
 *       in: 0x2 verodert: Aufruf von AVM_DebugPrintf() -> Support von %*B aber nicht aller Kernel-Format
 *       out: 0x1 last was \r bzw. \n
\*-------------------------------------------------------------------------------------*/
int avm_DebugvPrintf(unsigned *Mode, const char *format, va_list marker) {
    struct _avmdebug_datahandler ud, *pud = &ud;
    char *DebugData;
    unsigned int *used;
    int cr = 0, len;

    DebugData = alloc_debugstack(&used);
    if(DebugData == NULL) {
        avm_debug.lost++;
        if(Mode) *Mode = cr;
        return 0;
    }
    if(avm_debug.lost) {
#if defined(CONFIG_PRINTK_TIME)
        /* Follow the token with the time */
		unsigned long clk_rem;
        unsigned long long clk = cpu_clock(smp_processor_id());
		clk_rem = do_div(clk, 1000000000);
#endif /*--- #if defined(CONFIG_PRINTK_TIME) ---*/
        len = sprintf(DebugData, "<"TIME_ARGUMENT_STRING()"debug-message lost %d >", TIME_ARGUMENT(), avm_debug.lost);
        if(len > 0) {
            pud->Sum = len;
            cr = DebugPrintf_Puts(DebugData, pud->Sum);
        }
        avm_debug.lost = 0;
    }
    if(Mode && (*Mode & 0x1)) {
#if defined(CONFIG_PRINTK_TIME)
        /* Follow the token with the time */
		unsigned long clk_rem;
        unsigned long long clk = cpu_clock(smp_processor_id());
		clk_rem = do_div(clk, 1000000000);
#endif /*--- #if defined(CONFIG_PRINTK_TIME) ---*/
        len = sprintf(DebugData, TIME_ARGUMENT_STRING() CPU_ARGUMENT_STRING(), TIME_ARGUMENT() CPU_ID());
        if(len > 0) {
            pud->Sum = len;
            cr = DebugPrintf_Puts(DebugData, pud->Sum);
        }
    } else {
        pud->Sum        = 0;
    }
#ifdef CONFIG_KALLSYMS
   if(Mode && (*Mode & 0x2)) {
#else
       /*--- wir benoetigen Modul-Textaufloesung: ---*/
#endif/*--- #ifdef CONFIG_KALLSYMS ---*/
        pud->Pos        = 0;
        pud->NextIsLong = FALSE;
        pud->p_ext = no_extension;
        while(*format) {
            avm_LimitOut(MAX_DEBUG_MESSAGE_LEN - 66);
            switch(*format) {
                case '\b':
                    if(pud->Pos) pud->Pos--;
                    break;
                case '\t':  /*--- tab size 4 ---*/
                    while(pud->Pos & 0x03)
                        *(DebugData + pud->Pos++) = ' ';
                    break;
                case '%':
                    /*---------------------------------------------------------------------*\
                        %[0]4[l]d
                    \*---------------------------------------------------------------------*/
                    format = avmdebug_parse_percent(format, pud, &marker);
                    switch(*format) {
                        case '\0':
                            /*--- printk("--- erzeugte Fehler: %x\n", *(format+1)); ---*/
                            continue;
                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        case '%':
                            *(DebugData + pud->Pos) = '%';
                            pud->Pos++;
                            break;
                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        case 'n':
                            *((unsigned int *)(va_arg(marker, void *))) = pud->Pos;
                            format++;
                            break;
                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        case 'c':
                            *(DebugData + pud->Pos) = (unsigned char)(va_arg(marker, int));
                            pud->Pos++;
                            break;
                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        case 'u':
                            avmdebug_set_uint(DebugData, pud, &marker, 10);
                            break;
                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        case 'i':
                        case 'd':
                            avmdebug_set_int(DebugData, pud, &marker);
                            break;
                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        case 'b':
                            avmdebug_set_uint(DebugData, pud, &marker, 2);
                            break;
                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        case 'o':
                            avmdebug_set_uint(DebugData, pud, &marker, 8);
                            break;
                        /*-----------------------------------------------------------------*\
                            unsigned char Bytes[Count] Count default = 1
                        \*-----------------------------------------------------------------*/
                        case 'B': 
                            avmdebug_set_hexfield(DebugData, pud, &marker);
                            break;
                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        case 's':
                            avmdebug_set_string(DebugData, pud, &marker);
                            break;
                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        case 'p':
                        case 'P':
                            switch(*(format+1)) {
                                case 'S':
                                case 'F':
                                case 'f':
                                case 'R':
                                    pud->p_ext = *(format+1);
                                    format++;
                                    break;
                                case 'B':
                                    avmdebug_set_hexfield(DebugData, pud, &marker);
                                    pud->p_ext = *(format+1);
                                    format++;
                                    break;
                                default:
                                    pud->p_ext = no_extension;
                            }
                            avmdebug_set_hex(DebugData, pud, &marker, 1);
                            break;
                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        case 'x':
                        case 'X':
                            avmdebug_set_hex(DebugData, pud, &marker, 0);
                            break;
                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        case 't': 
                            {
                                unsigned int Time = va_arg(marker, int);
                                *(DebugData + pud->Pos++) = (char)0xAB;
                                *(DebugData + pud->Pos++) = (char)(Time >> 0);
                                *(DebugData + pud->Pos++) = (char)(Time >> 8);
                                *(DebugData + pud->Pos++) = (char)(Time >> 16);
                                *(DebugData + pud->Pos++) = (char)(Time >> 24);
                                *(DebugData + pud->Pos++) = (char)0xBA;
                            }
                            break;

                        /*-----------------------------------------------------------------*\
                        \*-----------------------------------------------------------------*/
                        default:
                            *(DebugData + pud->Pos) = *format;
                            pud->Pos++;
                    }
                    pud->NextIsLong = FALSE;
                    break;
                default:
                    *(DebugData + pud->Pos) = *format;
                    pud->Pos++;
            }
            format++;        
        }
#ifdef CONFIG_KALLSYMS
    } else {
        pud->Pos = vsnprintf(DebugData, MAX_DEBUG_MESSAGE_LEN, format, marker);
    }
#endif/*--- #ifdef CONFIG_KALLSYMS ---*/
    if(pud->Pos == 0) {
        *used = 0;
        if(Mode) *Mode = cr;
        return pud->Sum;
    }
    *(DebugData + pud->Pos) = '\0';
    cr = DebugPrintf_Puts(DebugData, pud->Pos);
    if(Mode) *Mode = cr;
    *used = 0;
    return pud->Pos + pud->Sum;
}
EXPORT_SYMBOL(avm_DebugvPrintf);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void avm_DebugPrintf(const char *format, ...) {
    int print_time = 0x1| 0x2;
    va_list marker;
    va_start(marker,format);
    if(avm_debug.init) {
        avm_DebugvPrintf(&print_time, format, marker);
    } else {
        vprintk(format, marker);
    }
    va_end(marker);
}

#ifdef CONFIG_PRINTK
static char syncbuf[257];
/*--------------------------------------------------------------------------------*\
 * avm_DebugPrintf -> printk
 * aber nur das letzte 1 Kbyte
\*--------------------------------------------------------------------------------*/
#define OUTPUT_LASTBUFFER       (4096 * 2)
static void avmdebug_sync(void) {
    int start = 0;
    unsigned long flags;
    unsigned long actlen, waste = 0, mw;
    if(avm_debug.init == 0) {
        return;
    }
    avmdebug_lock(&avm_debug.synclock, &flags);
    actlen = avm_debugfill();
    if(actlen > OUTPUT_LASTBUFFER) {
        waste = actlen - OUTPUT_LASTBUFFER;
    }
    mw = waste;
    /*--- __printk("avmdebug_sync: %ld, %ld %ld\n", mw, avm_debug.read, avm_debug.write); ---*/
    while((actlen = avm_debug_read(NULL, NULL, waste, NULL))) {
        waste -= actlen;
    }
    for(;;) {
        int len;
        len = avm_debug_read(NULL, syncbuf, sizeof(syncbuf) -1, NULL);
        if(len <= 0) {
            break;
        }
        if(start == 0) {
            start = 1;
            __printk("\n---- start avmdebug(suppress %ld bytes) ----\n", mw);
        }
        syncbuf[len] = 0;
        __printk("%s", syncbuf);
    }
    if(start) {
        __printk("\n---- eof avmdebug ----\n");
    }
    avmdebug_unlock(&avm_debug.synclock, flags);
}
#endif /*--- #ifdef CONFIG_PRINTK ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avm_kernelvprintk(const char *format, va_list args){
    int r = 0;
#ifdef CONFIG_PRINTK
    r = avm_DebugvPrintf(NULL, format, args);
#endif  /*--- #ifdef CONFIG_PRINTK ---*/
    return r;
}
/*-------------------------------------------------------------------------------------*\
 * Ersatz für printk (nur 2.6-er Kernel)
 * inklusive Loglevelauswertung
\*-------------------------------------------------------------------------------------*/
static int avm_kernelprintk(const char *format, ...) {
#ifdef CONFIG_PRINTK
    va_list marker;
    static int loglevel;
    static int last_was_nl;
    int print_time = 0;

    if(oops_in_progress) {
        int ret;
#ifdef CONFIG_PRINTK
        restore_printk();   /*--- alle weiteren Ausgaben nur noch über standard-printk ---*/
        avmdebug_sync();
#endif  /*--- #ifdef CONFIG_PRINTK ---*/
        /*--- falls Kernel-OOPs, dann auf Linux-Methode biegen ! ---*/
        va_start(marker,format);
        ret = vprintk(format, marker);
        va_end(marker);
        return ret;
    }
	if (format[0] == '<') {
		unsigned char c = format[1];
		if (c && format[2] == '>') {
			switch (c) {
                case '0' ... '7': /* loglevel */
                    loglevel = c - '0';
                    /* Fallthrough - make sure we're on a new line */
                case 'd': /* KERN_DEFAULT */
                    if (!last_was_nl) {
                        last_was_nl = DebugPrintf_Puts("\n", 1);
                    }
#if defined(CONFIG_PRINTK_TIME)
                    print_time = 0x1;
#endif /*--- #if defined(CONFIG_PRINTK_TIME) ---*/
                /* Fallthrough - skip the loglevel */
                case 'c': /* KERN_CONT */
                    format += 3;
                    break;
			}
        }
    }
    if(loglevel < console_loglevel) {
        int ret;
        va_start(marker,format);
        ret = avm_DebugvPrintf(&print_time, format, marker);
        last_was_nl = print_time;
        if(last_was_nl) {
            /*--- letzte Zeichen war \r bzw \n -> Loglevel auf Default ---*/
            loglevel = default_message_loglevel;
        }
        va_end(marker);
        return ret;
    }
#endif /*--- #ifdef CONFIG_PRINTK ---*/
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
EXPORT_SYMBOL(avm_DebugPrintf);

/*--------------------------------------------------------------------------------*\
 * signal 0 .. 31
 * signal: 0 -> pushmail 2
 * signal: 1 -> crashreport
\*--------------------------------------------------------------------------------*/
void avm_DebugSignal(unsigned int signal){
    if(avm_debug.init) {
        unsigned long flags;
        avm_DebugPrintf("%s: %x %s %d\n", __func__, signal & 0x1F, signal & 0x80000000 ? "user pid:" : "kernel info:", (signal & ~0x80000000) >> 8);
        signal &= 0x1F;
        avmdebug_lock(&avm_debug.client_lock, &flags);
        avm_debug.signal |= 0x1 << signal;
        avmdebug_unlock(&avm_debug.client_lock, flags);
        wake_up_interruptible(&avm_debug.wait_queue);
    }
}
EXPORT_SYMBOL(avm_DebugSignal);

/*--------------------------------------------------------------------------------*\
 * signal 0 .. 31
\*--------------------------------------------------------------------------------*/
void avm_DebugSignalLog(unsigned int signal, char *fmt, ...){
    if(avm_debug.init) {
        va_list marker;
        va_start(marker,fmt);
        avm_DebugPrintf("avm_DebugSignal:");
        avm_DebugvPrintf(NULL, fmt, marker);
        avm_DebugPrintf("\n");
        va_end(marker);
        avm_DebugSignal(signal);
    }
}
EXPORT_SYMBOL(avm_DebugSignalLog);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static int send_buf(struct socket *sock, struct sockaddr_un *paddr, unsigned char *buffer, unsigned length, unsigned wait) {
	struct msghdr		msg;
	struct iovec		iov;
	mm_segment_t oldfs;

	if (sock->sk == NULL) {
		return 0;
	}
    iov.iov_base = (void *)buffer;
    iov.iov_len = length;

	msg.msg_name        = paddr;
	msg.msg_namelen     = sizeof(*paddr);
    msg.msg_iov         = &iov;
    msg.msg_iovlen      = 1;
    msg.msg_control     = NULL;
    msg.msg_controllen  = 0;
	msg.msg_flags       = wait ? 0 : MSG_DONTWAIT;

    oldfs = get_fs(); set_fs(KERNEL_DS);
    length = sock_sendmsg(sock, &msg, length);
    set_fs(oldfs);
    return length;
}
#define ARRAY_EL(a) (sizeof(a) / sizeof((a)[0]))
#define PUSHMSG(a) { str: "\x80"a, size: sizeof(a) + 1 }
static struct _pushmsg {
    char *str;
    int size;
}  pushmsg[] =  {
    PUSHMSG("pushmail 2"),
    PUSHMSG("crashreport")
}; 
/*--------------------------------------------------------------------------------*\
 * aus dem Thread-Kontext pushmsg an den ctrlmgr
\*--------------------------------------------------------------------------------*/
static void push_mail(struct _avm_debug *pdbg, unsigned int mode) {
    struct sockaddr_un addr;
    int err;

    memset(&addr, 0x0, sizeof(addr));
    if(pdbg->s_push == NULL) {
        if((err = sock_create(AF_UNIX, SOCK_DGRAM, 0, &pdbg->s_push)) < 0) {
            avm_DebugPrintf(KERN_ERR"[avmdebug]%s: error during creation of socket %d\n", __func__, err);
            return;
        }
        addr.sun_family = AF_UNIX;
        sprintf(addr.sun_path, "/var/tmp/me_avmdebug.ctl");
        if((err = pdbg->s_push->ops->bind(pdbg->s_push, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
            avm_DebugPrintf(KERN_ERR"[avmdebug]%s:bind failed %d\n", __func__, err);
            sock_release(pdbg->s_push);
            pdbg->s_push = NULL;
            return;
        }
    }
    if(mode >= ARRAY_EL(pushmsg)) {
        mode = 0;
    }
    avm_DebugPrintf("[avmdebug] push: %s\n", pushmsg[mode].str + 1);
    addr.sun_family = AF_UNIX;
    sprintf(addr.sun_path, "/var/tmp/me_ctlmgr.ctl");

   if((err = send_buf(pdbg->s_push, &addr, pushmsg[mode].str, pushmsg[mode].size, 1)) != pushmsg[mode].size) {
      avm_DebugPrintf("[avmdebug]%s: failed with ret=%d\n", __func__, err);
   }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void push_mailexit(struct _avm_debug *pdbg) {
#if 0
   if(pdbg->s_push->file && s_push->file->f_dentry && s_push->file->f_dentry->d_op && s_push->file->f_dentry->d_op->d_release) {
       avm_DebugPrintf("unlink %s\n", addr.sun_path);
       s_push->file->f_dentry->d_op->d_release(s_push->file->f_dentry);
       /*--- s_push->file->f_dentry->d_op->d_delete(s_push->file->f_dentry); ---*/
   }
   err = sys_unlink(addr.sun_path);
   avm_DebugPrintf("sys_unlink %s %d\n", addr.sun_path, err);
#endif
    if(pdbg->s_push) {
        sock_release(pdbg->s_push);
        pdbg->s_push = NULL;
    }
}
#define TIME_DIFF(act, old) ((unsigned long)(act) - (unsigned long)(old)) 
#define PUSHMAIL_RETRIGGER  (300 * HZ)      /*--- alle 5 Minuten ---*/
#define AVMDEBUGTHREAD_WAKE_UP  (60 * 60 * HZ)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avmdebug_thread( void *data ) {
    struct _avm_debug *pdbg = (struct _avm_debug *)data;
    int ret;
    unsigned long flags, sig = 0;
    unsigned long timeout    = AVMDEBUGTHREAD_WAKE_UP;
    unsigned long pushmail_lastjiffies = jiffies - PUSHMAIL_RETRIGGER;
    unsigned long timeoutsignal = 0;
    pdbg->thread_pid = current;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    set_user_nice(current, 19);
    daemonize("avmdebug");
    allow_signal(SIGTERM ); 
#else/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/
    daemonize();
    sprintf(current->comm, "avmdebug");
#endif/*--- #else ---*//*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/

    for(;;) {
        ret = wait_event_interruptible_timeout(pdbg->wait_queue, pdbg->signal, timeout);
        if(ret == -ERESTARTSYS) {
            /* interrupted by signal -> exit */ 
            break;
        }
        avmdebug_lock(&avm_debug.client_lock, &flags);
        sig = pdbg->signal | timeoutsignal;
        pdbg->signal = 0;
        avmdebug_unlock(&avm_debug.client_lock, flags);
        /*--- avm_DebugPrintf("[avmdebug]sig %lx tsig %lx timeout %ld\n", sig, timeoutsignal, timeout); ---*/
        while(sig) {
            switch(sig){
                case 0x1 << 0:
                    if(TIME_DIFF(jiffies, pushmail_lastjiffies) > PUSHMAIL_RETRIGGER) {
                        push_mail(pdbg, 0);
                        timeout = AVMDEBUGTHREAD_WAKE_UP;
                        timeoutsignal &= ~0x1 << 0;
                        pushmail_lastjiffies = jiffies;
                    } else {
                        timeoutsignal |= 0x1 << 0;
                        timeout = min(timeout, PUSHMAIL_RETRIGGER - TIME_DIFF(jiffies, pushmail_lastjiffies));
                        /*--- avm_DebugPrintf("[avmdebug]trigger too early: wait %d sec", timeout / HZ); ---*/
                    }
                    sig &= ~(0x1 << 0);
                    break;
                case 0x1 << 1:
                    push_mail(pdbg, 1); /*--- Crashreport ---*/
                    sig &= ~(0x1 << 1);
                    break;
                default:
                    sig = 0;
                    break; 
            }
        }
    }
    push_mailexit(pdbg);
    pdbg->thread_pid = NULL;
    complete_and_exit(&pdbg->on_exit, 0 );
}
