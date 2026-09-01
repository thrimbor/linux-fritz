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

struct _asm_led_shift_register_bit_context {
    unsigned int shift_register_bit;
    unsigned int pos;
    unsigned int reverse;
    char name[32];
};

unsigned int avm_led_shift_register_contence;
unsigned int avm_led_shift_register_length = 8;

#ifndef CONFIG_ARCH_DAVINCI
static unsigned int resource_allocated = 0;
struct resource my_shift_register_resource[] = {
    {
        .name  = "shift register bit clock",
        .start = CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_SHIFT_CLOCK,
        .end   = CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_SHIFT_CLOCK,
        .flags = IORESOURCE_IO
    },
    {
        .name  = "shift register store clock",
        .start = CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_STORAGE_CLOCK,
        .end   = CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_STORAGE_CLOCK,
        .flags = IORESOURCE_IO
    },
    {
        .name  = "shift register master reset",
        .start = CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_MASTER_RESET,
        .end   = CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_MASTER_RESET,
        .flags = IORESOURCE_IO
    },
    {
        .name  = "shift register bit data",
        .start = CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_DATA,
        .end   = CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_DATA,
        .flags = IORESOURCE_IO
    }
};
#endif /*--- #ifndef CONFIG_ARCH_DAVINCI ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_shift_register_bit_driver_init(unsigned int shift_register_bit, unsigned int pos, char *name) {
    struct _asm_led_shift_register_bit_context *context;

    context = (struct _asm_led_shift_register_bit_context *)kmalloc(sizeof(struct _asm_led_shift_register_bit_context), GFP_ATOMIC);
    if(context == NULL) {
        return 0;
    }
    memset(context, 0, sizeof(struct _asm_led_shift_register_bit_context));
    context->shift_register_bit = shift_register_bit;
    context->pos  = pos;
#ifndef CONFIG_ARCH_DAVINCI
    context->reverse = 1;
#endif /*--- #ifndef CONFIG_ARCH_DAVINCI ---*/
    memcpy(context->name, name, sizeof(context->name));
    context->name[sizeof(context->name) - 1] = '\0';

    if(pos > 7)
        avm_led_shift_register_length = 16;

#ifndef CONFIG_ARCH_DAVINCI
    if(resource_allocated == 0) {
        int i;
        for(i = 0 ; i < sizeof(my_shift_register_resource) / sizeof(my_shift_register_resource[0]) ; i++) {
            if(request_resource(&gpio_resource, &my_shift_register_resource[i])) {
                printk(KERN_ERR "ERROR: shift register: could not allocate gpio %d for %s\n",
                    my_shift_register_resource[i].name, my_shift_register_resource[i].start);
            }
        }
        resource_allocated = 1;
    }
#endif /*--- #ifndef CONFIG_ARCH_DAVINCI ---*/

#ifdef CONFIG_ARCH_DAVINCI
    avm_led_shift_register_length = 8;
#else /*--- #ifdef CONFIG_ARCH_DAVINCI ---*/
    avm_gpio_ctrl(CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_MASTER_RESET, GPIO_PIN, GPIO_OUTPUT_PIN);
    avm_gpio_ctrl(CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_SHIFT_CLOCK, GPIO_PIN, GPIO_OUTPUT_PIN);
    avm_gpio_ctrl(CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_STORAGE_CLOCK, GPIO_PIN, GPIO_OUTPUT_PIN);
    avm_gpio_ctrl(CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_DATA, GPIO_PIN, GPIO_OUTPUT_PIN);
#endif /*--- #else ---*/ /*--- #ifdef CONFIG_ARCH_DAVINCI ---*/

    avm_led_shift_register_bit_driver_action((unsigned int)context, 0);
    return (int)context;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_led_shift_register_bit_driver_show(unsigned int handle, unsigned int *pPos) {
    struct _asm_led_shift_register_bit_context *context = (struct _asm_led_shift_register_bit_context *)handle;
    if(pPos)
        *pPos = context->pos;
    DEB_NOTE("[avm_led]: single shift_register driver: GPIO %u %s (pin %u \"%s\")\n",
            context->shift_register_bit,
            context->reverse ? "inverted" : "normal",
            context->pos,
            context->name);
    return context->name;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_shift_register_bit_driver_exit(unsigned int handle) {
    if(handle) 
        kfree((void *)handle);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_shift_register_bit_driver_action(unsigned int handle, unsigned int on) {
    struct _asm_led_shift_register_bit_context *context = (struct _asm_led_shift_register_bit_context *)handle;
    on = context->reverse ? !on : on;

    avm_led_shift_register_load(1 << context->shift_register_bit, (on ? 1 : 0) << context->shift_register_bit);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_shift_register_bit_driver_sync(unsigned int handle, unsigned int state_id) {
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_ARCH_DAVINCI
void avm_led_shift_register_load(unsigned int mask, unsigned int values) {
    gpio_shift_register_load(mask, values);
}
#else /*--- #ifdef CONFIG_ARCH_DAVINCI ---*/
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_shift_register_load(unsigned int mask, unsigned int values) {
    struct _hw_gpio *G = (struct _hw_gpio *)OHIO_GPIO_BASE;
    unsigned int i;
    union _shift_register_bits {
        struct _master_reset {
#if CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_MASTER_RESET != 0
            volatile unsigned int reserved : (CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_MASTER_RESET);
#endif /*--- #if CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_MASTER_RESET != 0 ---*/
            volatile unsigned int bit : 1;
        } master_reset;
        struct _shift_clock {
#if CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_SHIFT_CLOCK != 0
            volatile unsigned int reserved : (CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_SHIFT_CLOCK);
#endif /*--- #if CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_SHIFT_CLOCK != 0 ---*/
            volatile unsigned int bit : 1;
        } shift_clock ;
        struct _store_clock {
#if CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_STORAGE_CLOCK != 0
            volatile unsigned int reserved : (CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_STORAGE_CLOCK);
#endif /*--- #if CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_STORE_CLOCK != 0 ---*/
            volatile unsigned int bit : 1;
        } store_clock ;
        struct _shift_data {
#if CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_DATA != 0
            volatile unsigned int reserved : (CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_DATA);
#endif /*--- #if CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_DATA != 0 ---*/
            volatile unsigned int bit : 1;
        } shift_data ;
        /*--- volatile unsigned int Register; ---*/
    } *R = (union _shift_register_bits *)&(G->OutputData);

    avm_led_shift_register_contence &= ~mask;
    avm_led_shift_register_contence |= mask & values;

    R = (union _shift_register_bits *)&(G->OutputData);
    
    R->shift_clock.bit = 0;
    R->store_clock.bit = 0;
    R->master_reset.bit = 0; /*--- reset ---*/
    R->master_reset.bit = 1; /*--- non reset ---*/

    for(mask = avm_led_shift_register_contence, i = avm_led_shift_register_length ; i ; i-- ) {
        R->shift_clock.bit = 0;
        R->shift_data.bit  = (mask & (1 << (i - 1))) ? 1 : 0;
        R->shift_clock.bit = 1;
    }
    R->store_clock.bit = 1;
    R->store_clock.bit = 0;
    R->shift_clock.bit = 0;
}
#endif

