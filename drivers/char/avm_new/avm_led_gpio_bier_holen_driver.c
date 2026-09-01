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
#include <linux/timer.h>
#include "avm_sammel.h"
#include "avm_led.h"
#include "avm_led_driver.h"

#if defined(CONFIG_AVM_LED_BIER_HOLEN)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mach_avm.h>
#else
#include <asm/avalanche/avalanche_map.h>
#include <asm/avalanche/sangam/hw_gpio.h>
#endif

struct _asm_led_gpio_bier_holen_context {
    unsigned int gpio_mask;
    unsigned int gpio_revmask;
    unsigned int dimpos;
    volatile unsigned int on;
    unsigned int virtled;
    struct timer_list timer;
    unsigned char gpioled[8];   /*--- [0] erste Bit aus GPIO-Mask etc. 0xFF: inaktiv ---*/
    char name[32];
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void brightness_mode_timer(unsigned long handle) {
    struct _asm_led_gpio_bier_holen_context *context = (struct _asm_led_gpio_bier_holen_context *)handle;
    unsigned int i, ledgpio= 0;
    unsigned int on = context->on;
    
    if(++context->dimpos > 3) {
        context->dimpos = 0;
    }
    for(i = 0; i < 8; i++, on >>= 4) {
        if(context->gpioled[i] != 0xFF) {
            /*--- nur das sind gültige Leds ---*/
            if(on & (1 << context->dimpos)) {
                /*--- in welchen Dim-Status ---*/
                ledgpio |= 1 << (context->gpioled[i]);
            } else {
                /*--- in welchen Dim-Status ---*/
                ledgpio &= ~(1 << (context->gpioled[i]));
            }
        }
    }
    /*--- DEB_NOTE("[avm_led]brightness_mode_timer %x %x %x\n", on, context->gpio_mask, ledgpio); ---*/
    avm_gpio_set_bitmask(context->gpio_mask, ledgpio ^ context->gpio_revmask);

    del_timer(&(context->timer));
    context->timer.expires = jiffies + 1;
    add_timer(&(context->timer));
}

/*------------------------------------------------------------------------------------------*\
 * virtled: 4 Bit: bestimmt welcher gpio zu welcher mask gehört
 * also Reihenfolge GPIO Bit1 zuerst gesetzt, dazu gehört Wert aus ersten Nibble virtled 0:3 
 * dann erst wieder Bit4 gesetzt dazugehöriger Wert im zweiten Nibble 4:7            
 * z.B. alle 5 GPIO's: Power = 7(virtled=4) Internet = 13(virtled=3) Fest = 12(virtled=2) WLAN = 10(virtled=1) INFO = 9 (virtled=0)
 *                          9(0) -> 10(1) -> 12(2) -> 13(3) -> 7(4)  - 
 *                          9(0) <- 10(1) <- 12(2) <- 13(3) <- 7(4) <-
 *
 * Reihenfolge der GPIO-Mask:   7 9 10 12 13 =   0x3680  (Maske der gesetzten Bits)   
 *                 virtled:     4 0  2  1  3 =  0x31204  (mit kleinsten Nibble beginnen!)                                                                                                              
 *
 *
 *                 Achtung GPIO 7  ist nicht lowaktiv (1 = Led an)
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_bier_holen_driver_init(unsigned int gpio_mask, unsigned int virtled, char *name) {
    struct _asm_led_gpio_bier_holen_context *context;
    unsigned int i, MaxLeds = 0;
    context = (struct _asm_led_gpio_bier_holen_context *)kmalloc(sizeof(struct _asm_led_gpio_bier_holen_context), GFP_ATOMIC);
    if(context == NULL) {
        DEB_ERR("[avm_led] no memory for gpio_mask driver context (%u) bytes\n", sizeof(struct _asm_led_gpio_bier_holen_context));
        return -ENOMEM;
    }
    context->gpio_mask    = gpio_mask;
    context->gpio_revmask = gpio_mask & ~(1 << 7) ;    /*--- Bit 7 ist highaktiv ---*/
    context->virtled      = virtled;
    context->dimpos       = 0;  

    i = strlen(name);
    i = i >= sizeof(context->name) ? sizeof(context->name) - 1 : i;
    memcpy(context->name, name, i);
    context->name[i] = '\0';

    memset(context->gpioled, 0xFF, sizeof(context->gpioled));
    for(i = 0 ; gpio_mask ; gpio_mask >>= 1, i++) {
        if(gpio_mask & 0x01) {
            avm_gpio_ctrl(1 << i, GPIO_PIN, GPIO_OUTPUT_PIN);
            if(MaxLeds < 8) {
                context->gpioled[virtled & 0x7] = i;
                MaxLeds++;
                virtled >>= 4;
            }

        }
    }
    avm_gpio_set_bitmask(context->gpio_mask, 0 ^ context->gpio_revmask ); /*--- alle Leds aus ---*/
    init_timer(&(context->timer));
    context->timer.function = brightness_mode_timer;
    context->timer.data     = (int)context;
    
    del_timer(&(context->timer));
    context->timer.expires = 1 + jiffies;
    add_timer(&(context->timer));
    return (int)context;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_led_gpio_bier_holen_driver_show(unsigned int handle, unsigned int *pPos) {
    struct _asm_led_gpio_bier_holen_context *context = (struct _asm_led_gpio_bier_holen_context *)handle;
    int i;
    if(pPos) {
        *pPos = context->virtled;
    }
            DEB_NOTE("[avm_led]: gpio bier holen driver: GPIO-Bier holen 0x%X (virtled 0x%X \"%s\")\n",
            context->gpio_mask,
            context->virtled,
            context->name);
    for(i = 0; i < 8; i++) 
        if(context->gpioled[i] != 0xFF) {
            DEB_NOTE("[avm_led]: virtled=%i -> gpio: %d\n", i, context->gpioled[i]);
        }
    return context->name;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_gpio_bier_holen_driver_exit(unsigned int handle) {
    struct _asm_led_gpio_bier_holen_context *context = (struct _asm_led_gpio_bier_holen_context *)handle;
    DEB_NOTE("[avm_led]avm_led_gpio_bier_holen_driver_exit %p\n", context);
    if(handle) {
        del_timer(&(context->timer));
        avm_gpio_set_bitmask(context->gpio_mask, 0 ^ context->gpio_revmask ); /*--- alle Leds aus ---*/
        kfree((void *)handle);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_bier_holen_driver_action(unsigned int handle, unsigned int on) {
    struct _asm_led_gpio_bier_holen_context *context = (struct _asm_led_gpio_bier_holen_context *)handle;

    context->on = on;
    /*--- context->on = (1<<0) | (3 << 4) | (5 << 8) | (7 << 12) | (5 << 16); ---*/
    /*--- DEB_NOTE("[avm_led]: avm_led_gpio_bier_holen_driver_action: %p %x\n", handle, on); ---*/
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_bier_holen_driver_sync(unsigned int handle, unsigned int state_id) {
    return 0;
}

#endif/*--- #if defined(CONFIG_AVM_LED_BIER_HOLEN) ---*/
