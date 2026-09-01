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
#include <linux/ioport.h>
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

struct _asm_led_gpio_bit_context *first_context = NULL;

struct _asm_led_gpio_bit_context {
    struct _asm_led_gpio_bit_context *next;
    unsigned int link_count;
    unsigned int gpio_bit;
    unsigned int pos;
    unsigned int reverse;
    char name[32];
/*--- #if defined(CONFIG_ARCH_DAVINCI) ---*/
    struct resource *led_resource;
/*--- #endif ---*/ /*--- #if defined(CONFIG_ARCH_DAVINCI) ---*/
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_bit_driver_init(unsigned int gpio_bit, unsigned int pos, char *name) {
    struct _asm_led_gpio_bit_context *context;
    struct _asm_led_gpio_bit_context *C = first_context;

    /*--- printk("[avm_led_gpio_bit_driver_init] start: gpio_bit %u pos %u name %s\n", gpio_bit, pos, name); ---*/
    while(C) {
        /*--- printk("[avm_led_gpio_bit_driver_init] check %u == %u\n", C->gpio_bit, gpio_bit); ---*/
        if(C->gpio_bit == gpio_bit) {
            C->link_count++;
            avm_led_gpio_bit_driver_action((unsigned int)C, 0);
            /*--- printk("[avm_led_gpio_bit_driver_init] old found: gpio_bit %u pos %u name %s\n", gpio_bit, pos, name); ---*/
            return (int)C;
        }
        C = C->next;
    }

    context = (struct _asm_led_gpio_bit_context *)kzalloc(sizeof(struct _asm_led_gpio_bit_context), GFP_ATOMIC);
    if(context == NULL) {
        printk(KERN_ERR"[avm_led_gpio_bit_driver_init] alloc error %d\n", __LINE__);
        return 0;
    }
    avm_gpio_ctrl(gpio_bit, GPIO_PIN, GPIO_OUTPUT_PIN);
#if defined(CONFIG_MIPS)
    if(gpio_bit == 7) {
        context->reverse = 0;
    } else {
        context->reverse = 1;
    }
#endif /*--- #if defined(CONFIG_MIPS) ---*/
    context->link_count = 1;
/*--- #if defined(CONFIG_ARCH_DAVINCI) ---*/
    context->led_resource = kzalloc(sizeof(struct resource) + strlen(name) + strlen("LED:  ") + 1, GFP_ATOMIC);
    if(context->led_resource == NULL) {
        kfree(context);
        printk(KERN_ERR"[avm_led_gpio_bit_driver_init] alloc error %d\n", __LINE__);
        return 0;
    }
    printk("[avm_led_gpio_bit_driver_init] gpio %u pos %u name %s\n", gpio_bit, pos, name);
    { 
        char *ptmp = (char *)((unsigned int)context->led_resource + sizeof(struct resource));
        strcpy(ptmp, "LED: ");
        strcat(ptmp, name);
        context->led_resource->name = ptmp;
    }
    context->led_resource->start = gpio_bit;
    context->led_resource->end   = gpio_bit;
    if(request_resource(&gpio_resource, context->led_resource)) {
        printk(KERN_ERR "GPIO %u: already in use, led driver init failed\n", gpio_bit);
        kfree(context->led_resource);
        kfree(context);
    }
/*--- #endif ---*/ /*--- #if defined(CONFIG_ARCH_DAVINCI) ---*/

    context->gpio_bit = gpio_bit;
    context->pos  = pos;
    memcpy(context->name, name, sizeof(context->name));
    context->name[sizeof(context->name) - 1] = '\0';

    avm_led_gpio_bit_driver_action((unsigned int)context, 0);
    if(first_context == NULL)
        first_context = context;
    else {
        context->next = first_context;
        first_context = context;
    }
    return (int)context;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_led_gpio_bit_driver_show(unsigned int handle, unsigned int *pPos) {
    struct _asm_led_gpio_bit_context *context = (struct _asm_led_gpio_bit_context *)handle;
    if(pPos)
        *pPos = context->pos;
    DEB_NOTE("[avm_led]: single gpio driver: GPIO %u %s (pin %u \"%s\")\n",
            context->gpio_bit,
            context->reverse ? "inverted" : "normal",
            context->pos,
            context->name);
    return context->name;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_gpio_bit_driver_exit(unsigned int handle) {
    if(handle) 
        kfree((void *)handle);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_bit_driver_action(unsigned int handle, unsigned int on) {
    struct _asm_led_gpio_bit_context *context = (struct _asm_led_gpio_bit_context *)handle;
    on = on ? 1 : 0;
    on = context->reverse ? !on : on;
    return avm_gpio_out_bit(context->gpio_bit, on);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_bit_driver_sync(unsigned int handle, unsigned int state_id) {
    return 0;
}
