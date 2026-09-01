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

#include <linux/version.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/avm_led.h>
#include "avm_sammel.h"
#include "avm_led.h"
#include "avm_led_driver.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mach_avm.h>
#else
#include <asm/avalanche/avalanche_map.h>
#include <asm/avalanche/sangam/hw_gpio.h>
#endif

struct _asm_led_shift_register_mask_context {
    unsigned int shift_register_mask;
    unsigned int pos;
    unsigned int reverse_mask;
    char name[32];
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_shift_register_mask_driver_init(unsigned int shift_register_mask, unsigned int pos, char *name) {
    struct _asm_led_shift_register_mask_context *context;
    extern unsigned int avm_led_shift_register_length;
    /*--- unsigned int i; ---*/
    context = (struct _asm_led_shift_register_mask_context *)kmalloc(sizeof(struct _asm_led_shift_register_mask_context), GFP_ATOMIC);
    if(context == NULL) {
        return 0;
    }
    memset(context, 0, sizeof(struct _asm_led_shift_register_mask_context));
    context->shift_register_mask = shift_register_mask;
    context->pos  = pos;
    context->reverse_mask = shift_register_mask;
    memcpy(context->name, name, sizeof(context->name));
    context->name[sizeof(context->name) - 1] = '\0';

    if(shift_register_mask & 0xFF00)
        avm_led_shift_register_length = 16;

    avm_led_shift_register_mask_driver_action((unsigned int)context, 0);
    return (int)context;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_led_shift_register_mask_driver_show(unsigned int handle, unsigned int *pPos) {
    struct _asm_led_shift_register_mask_context *context = (struct _asm_led_shift_register_mask_context *)handle;
    if(pPos)
        *pPos = context->pos;
    DEB_NOTE("[avm_led]: multi shift_register driver: GPIO-Mask 0x%X (pin_mask 0x%X \"%s\")\n",
            context->shift_register_mask,
            context->pos,
            context->name);
    return context->name;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_shift_register_mask_driver_exit(unsigned int handle) {
    if(handle) 
        kfree((void *)handle);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_shift_register_mask_driver_action(unsigned int handle, unsigned int on) {
    struct _asm_led_shift_register_mask_context *context = (struct _asm_led_shift_register_mask_context *)handle;
    unsigned int i;

    if(on)
        i = context->shift_register_mask & ~(context->reverse_mask);
    else
        i = context->shift_register_mask & context->reverse_mask;

    avm_led_shift_register_load(context->shift_register_mask, i);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_shift_register_mask_driver_sync(unsigned int handle, unsigned int state_id) {
    return 0;
}
