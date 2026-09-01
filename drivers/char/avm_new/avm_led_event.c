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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/fs.h>
/*--- #include <asm/semaphore.h> ---*/
#include <asm/errno.h>
/*--- #include <linux/wait.h> ---*/
/*--- #include <linux/vmalloc.h> ---*/
/*--- #include <linux/poll.h> ---*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <linux/cdev.h>
#include <asm/mach_avm.h>
#else
#endif

#include "avm_sammel.h"
#include "avm_led.h"

#include <linux/avm_event.h>
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avm_led_event_notify(void *Context, enum _avm_event_id id) {
    struct _avm_virt_led *V;
    if(   (id == avm_event_id_led_info)
       || (id == avm_event_id_led_status)) {
        V = avm_led_get_first_virt_led();
        while(V) {
            if(V->driver) {
                switch(id) {
                    case avm_event_id_led_info:
                        avm_led_info_event(V);
                        break;
                    case avm_event_id_led_status:
                        avm_led_status_event(V);
                        break;
                    default:
                        break;
                }
            }
            V = avm_led_get_next_virt_led(V);
        }
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_event_init(void) {
    DEB_NOTE("[avm_led] register avm event source mask: 0x%LX\n", 
            ((unsigned long long)1 << avm_event_id_led_status) | ((unsigned long long)1 << avm_event_id_led_info));
    avm_led.event_handle = avm_event_source_register("avm_led", 
            ((unsigned long long)1 << avm_event_id_led_status) |
            ((unsigned long long)1 << avm_event_id_led_info),
            avm_led_event_notify, NULL);
    if(avm_led.event_handle)
        return 0;
    DEB_WARN("[avm_led]: register avm event source failed\n");
    return -1;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_event_exit(void) {
    if(avm_led.event_handle)
        avm_event_source_release(avm_led.event_handle);
    avm_led.event_handle = NULL;
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_info_event(struct _avm_virt_led *V) {
    struct _avm_event_led_info *E;
    char *p = NULL;
    if(avm_led.event_handle == NULL) {
        return -EFAULT;
    }

    E = (struct _avm_event_led_info *)kmalloc(sizeof(struct _avm_event_led_info), GFP_ATOMIC);
    if(E == NULL) {
        return -ENOMEM;
    }

    E->event_header.id = avm_event_id_led_info;
    E->mode      = V->current_state->mode_index;
    E->param1    = V->current_state->param1;
    E->param2    = V->current_state->param2;
    if(V->driver && V->driver->show) {
        p = (*V->driver->show)(V->driver_handle, &(E->pos));
    } else {
        E->pos = 0;
    }
    E->gpio_driver_type = 0;
    E->gpio             = 0;  /*--- pin or mask, depens on driver type ---*/
    if(p) {
        strcpy(E->Name, p);
    } else {
        E->Name[0] = 0;
    }
    return avm_event_source_trigger(avm_led.event_handle, avm_event_id_led_info, sizeof(struct _avm_event_led_info), (void *)E);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_status_event(struct _avm_virt_led *V) {
    
    struct _avm_event_led_status *led_event;

    if((V->event_length == 0) || (V->event_ptr == NULL)) {
        return 0;
    }

    led_event = kmalloc(V->event_length, GFP_ATOMIC);
    if(led_event == NULL) {
        DEB_ERR("[avm_led]: no mem (alloc %u bytes)\n", V->event_length);
        return -ENOMEM;
    }

    memcpy(led_event, V->event_ptr, V->event_length);

    return avm_event_source_trigger(avm_led.event_handle, 
                                    avm_event_id_led_status, 
                                    V->event_length,
                                    led_event);
}


#endif /*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) ---*/

