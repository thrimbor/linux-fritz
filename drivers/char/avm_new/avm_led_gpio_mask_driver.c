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

struct _asm_led_gpio_mask_context {
    unsigned int gpio_mask;
    unsigned int pos;
    unsigned int reverse_mask;
    char name[32];
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_mask_driver_init(unsigned int gpio_mask, unsigned int pos, char *name) {
    struct _asm_led_gpio_mask_context *context;
    unsigned int i;
    context = (struct _asm_led_gpio_mask_context *)kmalloc(sizeof(struct _asm_led_gpio_mask_context), GFP_ATOMIC);
    if(context == NULL) {
        DEB_ERR("[avm_led] no memory for gpio_mask driver context (%u) bytes\n", sizeof(struct _asm_led_gpio_mask_context));
        return -ENOMEM;
    }

    memset(context, 0, sizeof(struct _asm_led_gpio_mask_context));
#if defined(CONFIG_MIPS)
    context->reverse_mask = gpio_mask & ~(1 << 7);
#endif /*--- #if defined(CONFIG_MIPS) ---*/
    context->gpio_mask    = gpio_mask;
    context->pos          = pos;

    i = strlen(name);
    i = i >= sizeof(context->name) ? sizeof(context->name) - 1 : i;
    memcpy(context->name, name, i);
    context->name[i] = '\0';

    for(i = 0 ; gpio_mask ; gpio_mask >>= 1, i++) {;
        if(gpio_mask & 0x01) {
            avm_gpio_ctrl(1 << i, GPIO_PIN, GPIO_OUTPUT_PIN);
        }
    }

    avm_led_gpio_mask_driver_action((unsigned int)context, 0);
    return (int)context;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_led_gpio_mask_driver_show(unsigned int handle, unsigned int *pPos) {
    struct _asm_led_gpio_mask_context *context = (struct _asm_led_gpio_mask_context *)handle;
    if(pPos)
        *pPos = context->pos;
    DEB_NOTE("[avm_led]: multi gpio driver: GPIO-Mask 0x%X (pin_mask 0x%X \"%s\")\n",
            context->gpio_mask,
            context->pos,
            context->name);
    return context->name;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_gpio_mask_driver_exit(unsigned int handle) {
    if(handle) 
        kfree((void *)handle);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_mask_driver_action(unsigned int handle, unsigned int on) {
    struct _asm_led_gpio_mask_context *context = (struct _asm_led_gpio_mask_context *)handle;
    unsigned int i;

    if(on)
        i = context->gpio_mask & ~(context->reverse_mask);
    else
        i = context->gpio_mask & context->reverse_mask;

    avm_gpio_set_bitmask(context->gpio_mask, i);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_mask_driver_sync(unsigned int handle, unsigned int state_id) {
    return 0;
}
