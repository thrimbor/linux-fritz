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

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <asm/errno.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/avm_led.h>
#include "avm_sammel.h"
#include "avm_led.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mach_avm.h>
#else
#endif


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct _avm_led_list avm_led_kernel_if_list;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_kernel_if {
    /*--- muss erstes element in der structur sein ---*/
    struct _avm_led_list_header list;
    int handle;
    void (*remove_call_back)(int);
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_kernel_if *avm_led_get_first_led_kernel_if(void) {
    return (struct _avm_led_kernel_if *)avm_led_kernel_if_list.first;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_kernel_if *avm_led_get_next_led_kernel_if(struct _avm_led_kernel_if *V) {
    return (struct _avm_led_kernel_if *)V->list.next;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_alloc_handle(char *name, int instance, void (*remove_call_back)(int)) {
    struct _avm_led_kernel_if *H;

    H = kmalloc(sizeof(struct _avm_led_kernel_if), GFP_ATOMIC);
    if(H == NULL)
        return -ENOMEM;

    H->handle = (int)avm_led_get_virt_led_handle(name, instance);
    if(H->handle == -ENXIO) {
        kfree(H);
        return 0;
    }
    H->remove_call_back = remove_call_back;

    avm_led_add_to_list(&avm_led_kernel_if_list, (struct _avm_led_list_header *)H);
    return (int)H;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_free_handle(int handle) {
    struct _avm_led_kernel_if *H = (struct _avm_led_kernel_if *)handle;

    avm_led_del_from_list(&avm_led_kernel_if_list, (struct _avm_led_list_header *)H);
    kfree((void *)H);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_notify_unvalid_handle(int handle) {
    struct _avm_led_kernel_if *H, *next;
    H = avm_led_get_first_led_kernel_if();
    while(H) {
        next = avm_led_get_next_led_kernel_if(H);
        if((H->handle == handle) && (H->remove_call_back)) {
            (*H->remove_call_back)((int)H);
            avm_led_free_handle((int)H);
        }
        H = next;
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_action_with_handle(int handle, int state) {
    struct _avm_led_kernel_if *H = (struct _avm_led_kernel_if *)handle;
    if(H == NULL)
        return;
    avm_led_virt_led_action(H->handle, state);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_action_with_name_and_instance(char *name, int instance, int state) {
    int handle;

    handle = avm_led_get_virt_led_handle(name, instance);
    if(handle) {
        avm_led_virt_led_action(handle, state);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
EXPORT_SYMBOL(avm_led_action_with_name_and_instance);
EXPORT_SYMBOL(avm_led_action_with_handle);
EXPORT_SYMBOL(avm_led_alloc_handle);
EXPORT_SYMBOL(avm_led_free_handle);

