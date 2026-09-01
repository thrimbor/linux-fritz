/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2006 AVM GmbH <fritzbox_info@avm.de>
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
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
/*--- #include <linux/devfs_fs_kernel.h> ---*/
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
/*--- #include <linux/avm_event.h> ---*/
#define AVM_EVENT_INTERNAL
#include <linux/avm_event.h>
#include <asm/atomic.h>
#include "avm_sammel.h"
#include "avm_event.h"

/*------------------------------------------------------------------------------------------*\
 * 2.4 Kernel H-Files
\*------------------------------------------------------------------------------------------*/
#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE 
#include <asm/smplock.h>
#endif /*--- #if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE ---*/ 


#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 32)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int avm_event_proc_list(ctl_table *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos);
static char *avm_event_get_function_mask_names(unsigned long long mask);
#endif/*--- #if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 32) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct _avm_event_open_data *first, *last;
static struct _avm_event_source *avm_event_source[MAX_AVM_EVENT_SOURCES];

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avm_event_source_notify(enum _avm_event_id id);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE 
#define LOCAL_LOCK()                    { save_flags(flags); cli(); }
#define LOCAL_UNLOCK()                  restore_flags(flags)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#else /*--- #if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE ---*/ 
extern spinlock_t avm_event_lock;
#define LOCAL_LOCK()                    spin_lock_irqsave(&avm_event_lock, flags)
#define LOCAL_UNLOCK()                  spin_unlock_irqrestore(&avm_event_lock, flags)
#endif /*--- #else ---*/ /*--- #if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE ---*/ 

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int atomic_test_and_set(unsigned int *ptr, unsigned int value) {
    int ret = 0;
    unsigned long flags;
    LOCAL_LOCK();
    if(*ptr == 0) {
        *ptr = value;
        ret = 1;
    }
    LOCAL_UNLOCK();
    return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void atomic_set_bits_long_long(unsigned long long *Dest, unsigned long long or_mask) {
    unsigned long flags;
    LOCAL_LOCK();
    *Dest |= or_mask;
    LOCAL_UNLOCK();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void atomic_clear_bits_long_long(unsigned long long *Dest, unsigned long long or_mask) {
    unsigned long flags;
    LOCAL_LOCK();
    *Dest &= ~or_mask;
    LOCAL_UNLOCK();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void atomic_add_value(unsigned int *addr, signed int value) {
	unsigned long flags;
    LOCAL_LOCK();
    *addr += value;
    LOCAL_UNLOCK();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void atomic_add_to_front_of_queue(struct _avm_event_open_data *open_data) {
	unsigned long flags;

    open_data->next = open_data->prev = NULL;

    LOCAL_LOCK();

    if(last == NULL) { /*--- aller erster Eintrag ---*/
        last = open_data;
    } else {
        first->prev = open_data;
        open_data->next = first;
    }
    first = open_data;

    LOCAL_UNLOCK();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void atomic_rm_from_queue(struct _avm_event_open_data *open_data) {
	unsigned long flags;
    LOCAL_LOCK();

    if(open_data->prev) {
        open_data->prev->next = open_data->next;
    } else { /*--- (first == open_data) wenn es keinen Vorgaenger gibt, muss dieser Eintrag der erste sein ---*/
        first = open_data->next;
    }
    if(open_data->next) {
        open_data->next->prev = open_data->prev;
    } else { /*--- (last == open_data) wenn es keine Nachfolger, muss dieser Eintrag der letzte sein ---*/
        last = open_data->prev;
    }
    LOCAL_UNLOCK();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if !defined(CONFIG_NO_PRINTK)
void dump_all_user_data(void) {
    struct _avm_event_open_data *open_data;
    struct _avm_event_item *I;
    struct _avm_event_data *D;
    unsigned int count = 0, i = 0;
    open_data = first;

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    while(open_data) {
        count++;
        printk("[dump_all_user_data] user %u \"%s\" %s pending events\n",
                count, open_data->Name ? open_data->Name : "unknown",
                open_data->item ? "has" : "has no");
        i = 0;
        I = (struct _avm_event_item *) open_data->item;
        while(I) {
            i++;
            if(I->data) {
                printk("\titem %u: link-count %u, len %u", 
                        i, I->data->link_count, I->data->data_length);
                if(I->data->data) {
                    struct _avm_event_header *event_header; 
                    event_header = (struct _avm_event_header *)(I->data->data);
                    printk(" id: %u\n", event_header->id);
                } else {
                    printk(" ERROR: no data pointer\n");
                }
            } else {
                printk("\titem %u: ERROR: has no data\n", i);
            }
            I = (struct _avm_event_item *) I->next;
        }
        open_data = open_data->next;
    }
    printk("%u user und %u events\n", count, i);
    
#define ILLEGAL_POINTER(p)      ((((unsigned int)(p) < 0x94000000) || ((unsigned int)(p) >= 0x96000000)) && (p != NULL))
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    i = 0;  /* unused count */
    count = 0;  /* temp unsuged count */
    I = (struct _avm_event_item *) avm_event_first_Item;

    printk("[dump_all_user_data] Items:\n");
    while(I) {
        if(ILLEGAL_POINTER(I)) {
            printk("[dump_all_user_data] Illegal Pointer I=0x%p\n", I);
            return;
        }
        if((unsigned int)(I->next) == (unsigned int)-1) {
            count++, i++;
        } else {
            if(count > 0) {
                printk("[dump_all_user_data] %u unused Items\n", count);
                count = 0;
            }
            if(ILLEGAL_POINTER(I->data)) {
                printk("[dump_all_user_data] Illegal Pointer I->data=0x%p\n", I->data);
                return;
            } else if(I->data) {
                printk("\titem: link-count %u, len %u", 
                        I->data->link_count, I->data->data_length);
                if(ILLEGAL_POINTER(I->data->data)) {
                    printk("[dump_all_user_data] Illegal Pointer I->data->data=0x%p\n", I->data->data);
                } else if(I->data->data) {
                    struct _avm_event_header *event_header; 
                    event_header = (struct _avm_event_header *)(I->data->data);
                    printk(" id: %u\n", event_header->id);
                } else {
                    printk(" ERROR: no data pointer\n");
                }
            } else {
                printk("\titem: ERROR: has no data\n");
            }
        }
        I = (struct _avm_event_item *)(I->debug);
    }
    if(i > 0) {
        printk("[dump_all_user_data] %u unused Items\n", i);
    }

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    D = (struct _avm_event_data *) avm_event_first_Data;
    count = 0, i = 0;  /* tmp and all unsigned count */
    printk("[dump_all_user_data] Data:\n");
    while(D) {
        if(ILLEGAL_POINTER(D)) {
            printk("[dump_all_user_data] Illegal Pointer D=0x%p\n", D);
            return;
        } else if(ILLEGAL_POINTER(D->data)) {
            printk("[dump_all_user_data] Illegal Pointer D->data=0x%p\n", D->data);
            return;
        } else if(D->data) {
            struct _avm_event_header *event_header; 
            event_header = (struct _avm_event_header *)(D->data);
            if(count > 0) {
                printk("[dump_all_user_data] %u unused Data(s)\n", count);
                count = 0;
            }
            printk("\tid: %u\n", event_header->id);
        } else {
            count++, i++;
        }
        D = D->debug;
    }
    if(i > 0) {
        printk("[dump_all_user_data] %u unused Datas\n", i);
    }
}
#endif /*--- #if !defined(CONFIG_NO_PRINTK) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_event_register(struct _avm_event_open_data *open_data, struct _avm_event_cmd_param_register *data) {
    int i;
    DEB_INFO("[avm_event_register]: Name=%s Mask=%LX\n", data->Name, data->mask);
    atomic_add_to_front_of_queue(open_data);
    DEB_INFO("[avm_event_register]: open_data=0x%p first=0x%p last=0x%p\n", open_data, first, last);
    strcpy(open_data->Name, data->Name);
    open_data->event_mask_registered = data->mask;
    for(i = 0 ; i < avm_event_last ; i++) {
        if(open_data->event_mask_registered & ((unsigned long long)1 << i)) {
            avm_event_source_notify((enum _avm_event_id)i);
        }
    }
    DEB_INFO("[avm_event_register]: success\n");
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_event_release(struct _avm_event_open_data *open_data, struct _avm_event_cmd_param_release *data __attribute__((unused))) {
    unsigned int i;
    DEB_INFO("[avm_event_release]: first=0x%p last=0x%p\n", first, last);

    atomic_rm_from_queue(open_data);

    for(i = 0 ; i < sizeof(unsigned long long) * 8 ; i++) {
        if(open_data->event_mask_registered & ((unsigned long long)1 << i)) {
            while(open_data->item) {
                avm_event_commit(open_data, i);
            }
        }
    }
    DEB_INFO("[avm_event_release]: (first=%x last=%x) success\n", (unsigned int)first, (unsigned int)last);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_event_trigger(struct _avm_event_open_data *open_data, struct _avm_event_cmd_param_trigger *data) {
    DEB_INFO("[avm_event_trigger]: Id %u\n", data->id);
    if(open_data->event_mask_registered & ((unsigned long long)1 << data->id)) {
        avm_event_source_notify((enum _avm_event_id)data->id);
    } else {
        DEB_WARN("[avm_event_trigger]: Id %u event mask not registered\n", data->id);
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
 * Den ersten Eintrag für den man benachrichtigt wurde holen
\*------------------------------------------------------------------------------------------*/
int avm_event_get(struct _avm_event_open_data *open_data, unsigned char **rx_buffer, unsigned int *rx_buffer_length, unsigned int *event_pos __attribute__((unused))) {
    if(open_data->item) {
        *rx_buffer        = open_data->item->data->data;
        *rx_buffer_length = open_data->item->data->data_length;
        DEB_INFO("[avm_event_get]: success\n");

        return 0;
    }
    DEB_INFO("[avm_event_get]: empty\n");
    return 0;
}

/*------------------------------------------------------------------------------------------*\
 * Ruecksetzen der notified Maske, herunterzaehlen des link_count und ggf. freigaben
 * der event_data
\*------------------------------------------------------------------------------------------*/
void avm_event_commit(struct _avm_event_open_data *open_data, unsigned int event_pos __attribute__((unused))) {
    unsigned long flags;
    struct _avm_event_item *I = (struct _avm_event_item *) open_data->item;
    DEB_INFO("[avm_event_commit]: Name=%s\n", open_data->Name);
    if(I) { /*--- wenn ein event item vorhanden ---*/
        /*----------------------------------------------------------------------------------*\
         * geschuetztes manipulieren der Liste (erstes Element endfernen)
        \*----------------------------------------------------------------------------------*/
#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE 
        save_flags(flags);
        cli();
        open_data->item = I->next; /*--- das naechste Element ---*/
        restore_flags(flags);
#else /*--- #if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE ---*/ 
        spin_lock_irqsave(&avm_event_lock, flags);
        open_data->item = I->next; /*--- das naechste Element ---*/
        spin_unlock_irqrestore(&avm_event_lock, flags);
#endif /*--- #else ---*/ /*--- #if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE ---*/ 
        /*----------------------------------------------------------------------------------*\
        \*----------------------------------------------------------------------------------*/
        /*--- avm_event_free_data(I->data); ---*/
        avm_event_free_item(I);
    }
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    {
        int count = 0;
        unsigned char iii[64];
        memset(iii, 0, sizeof(iii));
        I = (struct _avm_event_item *)open_data->item;
        while(I) {
            if(I->data) {
                struct _avm_event_header *event_header; 
                event_header = (struct _avm_event_header *)(I->data->data);
                iii[event_header->id]++;
            }
            I = (struct _avm_event_item *)I->next;
            count++;
        }
        DEB_INFO("[avm_event_commit]: Name=%s \n", open_data->Name);
        for(count = 0 ; count < 64 ; count++) {
            if(iii[count]) { /*--- BUG FIX, Events ---*/
#if 0
                if(iii[count] > 20) {
                    struct _avm_event_item *II = (struct _avm_event_item *) open_data->item;
                    int i;
                    for(i = 0 ; i < 15 ; i++) {
                        spin_lock_irqsave(&avm_event_lock, flags);
                        open_data->item = II->next; /*--- das naechste Element ---*/
                        spin_unlock_irqrestore(&avm_event_lock, flags);
                        avm_event_free_item(II);
                    }
                    printk(KERN_ERR "WARNING: id: %d: %d events pending, 15 event dropped\n", count, iii[count]);
                    iii[count] -= 15;
                } else {
#endif
                    DEB_INFO("\tid: %d: %d events\n", count, iii[count]);
#if 0
                }
#endif
            }
        }
    }
    open_data->receive_count++;
    return;
}

/*------------------------------------------------------------------------------------------*\
 * wird NUR im Kontext der sich registrierenden Applikation aufgerufen, somit ist die
 * 'avm_event_sema' gesetzt
\*------------------------------------------------------------------------------------------*/
static void avm_event_source_notify(enum _avm_event_id id) {
    int i;
    for( i = 0 ; i < MAX_AVM_EVENT_SOURCES ; i++) {
        if(avm_event_source[i] == NULL)
            continue;
        if(avm_event_source[i]->notify && (avm_event_source[i]->event_mask & ((unsigned long long)1 << id))) {
            (*avm_event_source[i]->notify)(avm_event_source[i]->Context, id);
            break; /*--- eine id kann nur von einem geliefert werden ---*/
        }
    }
}

/*------------------------------------------------------------------------------------------*\
 * wird von beiden Kontexten aufgerufen
\*------------------------------------------------------------------------------------------*/
void *avm_event_source_register(char *name, unsigned long long id_mask, void (*notify)(void *, enum _avm_event_id), void *Context) {
    unsigned int i;
    struct _avm_event_source *S;
    DEB_INFO("[avm_event_source_register]: name=%s mask=%LX\n", name, id_mask);

    if(id_mask & avm_event_source_mask) {
        DEB_ERR("[avm_event_source_register]: overlapping event_mask current=%LX new=%LX\n", id_mask, avm_event_source_mask);
        return NULL;
    }

    if(strlen(name) > MAX_EVENT_SOURCE_NAME_LEN) {
        DEB_ERR("[%s]: Event name '%s' is too long!\n", __FUNCTION__, name);
        return NULL;
    }

    S = vmalloc(sizeof(struct _avm_event_source));
    if(S == NULL) {
        DEB_ERR("[avm_event_source_register]: out of memory \n");
        return NULL;
    }

    strncpy(S->Name, name, MAX_EVENT_SOURCE_NAME_LEN);
    S->Name[MAX_EVENT_SOURCE_NAME_LEN] = 0;
    S->signatur   = AVM_EVENT_SIGNATUR;
    S->notify     = notify;
    S->Context    = Context;
    S->event_mask = id_mask;
    S->send_count = 0;

    for( i = 0 ; i < MAX_AVM_EVENT_SOURCES ; i++) {
        if(atomic_test_and_set((unsigned int *)&avm_event_source[i], (unsigned int)S)) {
            break;
        }
    }
    if(i == MAX_AVM_EVENT_SOURCES) {
        vfree(S);
        DEB_ERR("[avm_event_source_register]: out of resources\n");
        return NULL;
    }
    
    atomic_set_bits_long_long(&avm_event_source_mask, id_mask);
    DEB_INFO("[avm_event_source_register]: handle=0x%X success\n", (unsigned int)S);

#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE 
    MOD_INC_USE_COUNT;
#endif /*--- #if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE ---*/ 
    return (void *)S;
}

/*------------------------------------------------------------------------------------------*\
 * wird von beiden Kontexten aufgerufen
\*------------------------------------------------------------------------------------------*/
void avm_event_source_release(void *handle) {
    struct _avm_event_source *S = (struct _avm_event_source *)handle;
    unsigned int i;

    DEB_INFO("[avm_event_source_release]: handle=0x%X\n", (unsigned int)handle);
    if(handle == NULL) {
        DEB_ERR("[avm_event_source_release]: invalid handle NULL\n");
        return;
    }
    if(S->signatur != AVM_EVENT_SIGNATUR) {
        DEB_ERR("[avm_event_source_release]: missing signatur\n");
        return;
    }

    atomic_clear_bits_long_long(&avm_event_source_mask, S->event_mask);

    for( i = 0 ; i < MAX_AVM_EVENT_SOURCES ; i++) {
        if(S == avm_event_source[i]) {
            avm_event_source[i] = NULL;
            vfree(S);
            break;
        }
    }
    DEB_INFO("[avm_event_source_release]: success\n");
#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE 
    MOD_DEC_USE_COUNT;
#endif /*--- #if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE ---*/ 
    return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_event_source_check_id(void *handle, enum _avm_event_id id) {
    struct _avm_event_open_data *O = first;

    DEB_INFO("[avm_event_source_check_id]: handle=0x%x id=%u\n", (unsigned int)handle, id);
    if(handle == NULL) {
        DEB_ERR("[avm_event_source_trigger]: not registered\n");
        return 0;
    }
    while(O) {  /*--- alle geoeffneten handles durchgehen ---*/
        /*--- jeder der auf diese id ein bit in seiner Maske gesetzt hat, wird mit informationen versorgt und anschliessend geweckt ---*/
        if(O->event_mask_registered & ((unsigned long long)1 << id)) {
            return 1;
        }
        O = O->next;
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
 * wird von beiden Kontexten aufgerufen
\*------------------------------------------------------------------------------------------*/
int avm_event_source_trigger(void *handle, enum _avm_event_id id, unsigned int data_length, void *data) {
    struct _avm_event_data *D;
    struct _avm_event_open_data *O = first;
    struct _avm_event_header *H;
    struct _avm_event_source *S;
    int status;

    DEB_INFO("[avm_event_source_trigger]: handle=0x%x id=%u data=0x%p len=%u (%s)\n", (unsigned int)handle, id, data, data_length, current->comm);

    H = (struct _avm_event_header *)data;
    if(handle == NULL) {
        DEB_ERR("[avm_event_source_trigger]: not registered\n");
        if(data)
            kfree(data);
        return 0;
    }
    S = (struct _avm_event_source *)handle;

    S->send_count++;

    if(!H || H->id != id) {
        DEB_ERR("[avm_event_source_trigger]: avm_event_header inkorrekt !\n");
        if(data)
            kfree(data);
        return 0;
    }

    D = avm_event_alloc_data();
    if(D == NULL) {
        DEB_ERR("[avm_event_source_trigger]: out of memory (data descriptors) context=%s\n", current->comm);
#if !defined(CONFIG_NO_PRINTK)
        dump_all_user_data();
#endif /*--- #if !defined(CONFIG_NO_PRINTK) ---*/
        if(data)
            kfree(data);
        return 0;
    }
    D->link_count  = 1; 
    D->data        = data;
    D->data_length = data_length;

    status = 0;
    while(O) {  /*--- alle geoeffneten handles durchgehen ---*/
        /*--- jeder der auf diese id ein bit in seiner Maske gesetzt hat, wird mit informationen versorgt und anschliessend geweckt ---*/
        if(O->event_mask_registered & ((unsigned long long)1 << id)) {
            if(avm_event_source_trigger_one(O, D)) {
                /* kein speicher mehr */
                break;
            }
            status++;
        }
        O = O->next;
    }
    avm_event_free_data(D);
    DEB_INFO("[avm_event_source_trigger]: success (%u instances notified)\n", D ? D->link_count : 0);
    return status; /*--- anzahl der benachrichtigten empfaenger ---*/
}

/*------------------------------------------------------------------------------------------*\
 * wird von beiden Kontexten aufgerufen
\*------------------------------------------------------------------------------------------*/
int avm_event_source_trigger_one(struct _avm_event_open_data *O, struct _avm_event_data *D) {
    unsigned long flags;
    struct _avm_event_item *I;

    I = avm_event_alloc_item();
    if(I == NULL) {
        DEB_ERR("[avm_event_source_trigger_one]: out of memory (items) context=%s\n", current->comm);
#if !defined(CONFIG_NO_PRINTK)
        dump_all_user_data();
#endif /*--- #if !defined(CONFIG_NO_PRINTK) ---*/
        return 1;
    }

    I->data = D;
    I->next = NULL;
    atomic_add_value(&(D->link_count), 1); /*--- D->link_count++; ---*/

    /*--------------------------------------------------------------------------------------*\
     * geschueztes verketten (einhaengen)
    \*--------------------------------------------------------------------------------------*/
    LOCAL_LOCK();
    if(O->item == NULL) { /*--- erster ---*/
        O->item = I;
    } else { /*--- ende finden und anhängen ---*/
        struct _avm_event_item *i = (struct _avm_event_item *) O->item;
        while(i->next) /*--- bis zum Ende die schlange abarbeiten ---*/
            i = (struct _avm_event_item *) i->next;
        i->next = I;
    }
    LOCAL_UNLOCK();
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/

    DEB_INFO("[avm_event_source_trigger_one]: wake up '%s'\n", O->Name);

    O->queue_count++;

    wake_up(&(O->wait_queue));
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
EXPORT_SYMBOL(avm_event_source_check_id);
EXPORT_SYMBOL(avm_event_source_trigger);
EXPORT_SYMBOL(avm_event_source_release);
EXPORT_SYMBOL(avm_event_source_register);

#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 32)

#define AVM_EVENT_PROC_LIST_SERVERS         1
#define AVM_EVENT_PROC_LIST_CLIENTS         2
#define AVM_EVENT_PROC_LIST_EVENT           3

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
ctl_table avm_event_proc_table[] = {
	{
        .ctl_name       = AVM_EVENT_PROC_LIST_SERVERS, 
        .procname       = "servers", 
        .data           = NULL,
        .maxlen         = 0,
        .mode           = 0444, 
        .child          = NULL, 
        .proc_handler   = &avm_event_proc_list,
    }, {
        .ctl_name       = AVM_EVENT_PROC_LIST_CLIENTS, 
        .procname       = "clients", 
        .data           = NULL,
        .maxlen         = 0,
        .mode           = 0444, 
        .child          = NULL, 
        .proc_handler   = &avm_event_proc_list,
#if 0
    }, {
        .ctl_name       = AVM_EVENT_PROC_LIST_EVENT, 
        .procname       = "events", 
        .data           = NULL, 
        .maxlen         = 0,
        .mode           = 0444, 
        .child          = NULL, 
        .proc_handler   = &avm_event_proc_list,
#endif
    }, {
        .ctl_name       = 0
    }
};

ctl_table avm_event_proc_root_table[] = {
	{
        .ctl_name       = 0, 
        .procname       = "avm_event", 
        .data           = NULL, 
        .maxlen         = 0, 
        .mode           = 0555, 
        .child          = avm_event_proc_table 
    }, {
        .ctl_name       = 0
    }
};

static struct ctl_table_header *avm_event_proc_header;

char *avm_event_function_names[64] = {
    [0] = "avm_event_id_wlan_client_status",

    [7] = "avm_event_id_autoprov",
    [8] = "avm_event_id_usb_status",

    [11] = "avm_event_id_dsl_get_arch_kernel",
    [12] = "avm_event_id_dsl_set_arch",
    [13] = "avm_event_id_dsl_get_arch",
    [14] = "avm_event_id_dsl_set",
    [15] = "avm_event_id_dsl_get",
    [16] = "avm_event_id_dsl_status",
    [17] = "avm_event_id_dsl_connect_status",

    [19] = "avm_event_id_push_button",
    [20] = "avm_event_id_telefon_wlan_command",
    [21] = "avm_event_id_capiotcp_startstop",
    [22] = "avm_event_id_telefon_up",
    [23] = "avm_event_id_reboot_req",

    [24] = "avm_event_id_appl_status",
    [25] = "avm_event_id_led_status",
    [26] = "avm_event_id_led_info",

    [27] = "avm_event_id_telefonprofile",
    [28] = "avm_event_id_temperature",
    [29] = "avm_event_id_cpu_idle",
    [30] = "avm_event_id_powermanagment_status",

    [32] = "avm_event_id_ethernet_status",
    [33] = "avm_event_id_ethernet_connect_status",

    [40] = "avm_event_id_pm_ressourceinfo_status",

    [63] = "avm_event_id_user_source_notify",
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_event_proc_init(void) {
	avm_event_proc_header  = register_sysctl_table(avm_event_proc_root_table);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static char *avm_event_get_function_mask_names(unsigned long long mask) {
    int i;
    static char buffer[1024];
    int len = 0;
    for(i = 0 ; i < sizeof(unsigned long long) * 8 ; i++) {
        if((mask & (1ULL << i)) == 0)
            continue;
        if(avm_event_function_names[i]) {
            len += snprintf(buffer + len, sizeof(buffer) - len, "%s ", avm_event_function_names[i]);
        } else {
            len += snprintf(buffer + len, sizeof(buffer) - len, "undef-%d ", i);
        }
    }
    return buffer;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int avm_event_proc_list(ctl_table *ctl, int write, void *user_buffer, size_t *lenp, loff_t *ppos) {
    size_t len = 0;
    int count;
    struct _avm_event_source *S;
    struct _avm_event_open_data *O;
    char *buffer = kmalloc(*lenp, GFP_KERNEL);

    if (!*lenp || (*ppos && !write) || !buffer) {
        *lenp = 0;
        return 0;
    }

    /*--- printk(KERN_ERR "[%s] *lenp = %d, *ppos = %lld\n", __FUNCTION__, *lenp, *ppos); ---*/

    switch(ctl->ctl_name) {
        case AVM_EVENT_PROC_LIST_SERVERS:
            /*--- printk(KERN_ERR "[%s] AVM_EVENT_PROC_LIST_SERVERS\n", __FUNCTION__); ---*/
            *ppos = 0ULL;
            len = sprintf(buffer, "[avm_event] list Event Servers\n");
            for( count = 0; count < sizeof(avm_event_source) / sizeof(avm_event_source[0]) ; count++) {
                int ii;
                S = avm_event_source[count];
                if(S == NULL)
                    continue;
                ii = snprintf(buffer + len, *lenp - len, "Server: %*.s sent:%5d notify: %60.60pF: Function: %s\n",
                        MAX_EVENT_SOURCE_NAME_LEN, S->Name, 
                        S->send_count,
                        S->notify ?  S->notify : "NULL",
                        avm_event_get_function_mask_names(S->event_mask));
                /*--- printk(KERN_ERR "Buffer[%d .. %d] = '%*.*s'\n", len, len + ii, ii, ii, buffer + len); ---*/

                len += ii;
            }
            len += snprintf(buffer + len, *lenp - len, "--------------------------\n");
            *ppos += len;
            *lenp = len;
            /*--- printk(KERN_ERR "[%s] len = %d *lenp = %d, *ppos = %lld\n", __FUNCTION__, len, *lenp, *ppos); ---*/
            break;
        case AVM_EVENT_PROC_LIST_CLIENTS:
            /*--- printk(KERN_ERR "[%s] AVM_EVENT_PROC_LIST_SERVERS\n", __FUNCTION__); ---*/
            *ppos = 0ULL;
            len = sprintf(buffer, "[avm_event] list Event Clients\n");

            O = first;
            for(O = first ; O ; O = O->next) {
                len += snprintf(buffer + len, *lenp - len, "Client: %*.s: receive:%5d Functions: %s\n",
                        MAX_EVENT_SOURCE_NAME_LEN, O->Name, 
                        O->receive_count,
                        avm_event_get_function_mask_names(O->event_mask_registered));
                if(O->receive_count != O->queue_count) {
                    len += snprintf(buffer + len, *lenp - len, "\t\t%5d event still pending.\n", 
                            O->queue_count - O->receive_count);
                }
            }
            len += snprintf(buffer + len, *lenp - len, "--------------------------\n");
            *ppos += len;
            *lenp = len;
            /*--- printk(KERN_ERR "[%s] len = %d *lenp = %d, *ppos = %lld\n", __FUNCTION__, len, *lenp, *ppos); ---*/
            break;
        case AVM_EVENT_PROC_LIST_EVENT:
            break;
    }
    copy_to_user(user_buffer, buffer, len);
    kfree(buffer);
    return 0;
}
#endif/*--- #if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 32) ---*/
