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

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mach_avm.h>
#else
#include <asm/avalanche/avalanche_map.h>
#include <asm/avalanche/sangam/hw_gpio.h>
#endif

enum _dual_led {
    keine_led           = 0,
    rote_led            = 1,
    gruene_led          = 2,
    beide_two_pin_led   = 3,
    beide_three_pin_led = 4,
    rote_led_first      = 5,
    rote_led_6          = 6,
    rote_led_7          = 7,
    rote_led_last       = 8,
    rotgruen_led        = 9
};

struct _asm_led_gpio_dual_bit_context {
    struct _asm_led_gpio_dual_bit_context *prev, *next;
    struct _asm_led_gpio_dual_bit_context *first;
    struct _asm_led_gpio_dual_bit_context *instance[AVM_LED_MAX_INSTANCE];
    struct timer_list timer;
    unsigned int timer_active;
    unsigned int gpio_bit1;
    unsigned int gpio_bit2;
    unsigned int pos;
    unsigned int reverse1;
    unsigned int reverse2;
    enum _dual_led led;
    unsigned int last_on;
    char name[32];
};

static struct _asm_led_gpio_dual_bit_context *first = NULL, *last = NULL;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avm_led_gpio_dual_bit_driver_timer(unsigned long handle);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int avm_led_add_dual_bit_gpio_to_list(struct _asm_led_gpio_dual_bit_context *V) {
    DEB_NOTE("[avm_led_add_dual_bit_gpio_to_list] add 0x%p\n", V);
    if(first == NULL) {
        DEB_NOTE("[avm_led_add_dual_bit_gpio_to_list] add to first\n");
        V->prev = V->next = NULL;
        first = last = V;
        return 0;
    }
    V->prev = last;
    V->next = NULL;
    last->next = V;
    last = V;
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int avm_led_del_dual_bit_gpio_from_list(struct _asm_led_gpio_dual_bit_context *V) {
    if(V->next) {
        V->next->prev = V->prev;
    }  else { /* bin letzter */
        last = V->prev; /* kann u.U. NULL sein */
    }
        
    if(V->prev) { /* bin erster */
        V->prev->next = V->next;
    } else {
        first = V->next; /* kann u.U. NULL sein */
    }
    V->next = V->prev = NULL;
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_dual_bit_driver_init(unsigned int gpio_bit, unsigned int pos, char *name) {
    struct _asm_led_gpio_dual_bit_context *context;
    struct _asm_led_gpio_dual_bit_context *V;

    context = (struct _asm_led_gpio_dual_bit_context *)kmalloc(sizeof(struct _asm_led_gpio_dual_bit_context), GFP_ATOMIC);
    memset(context, 0, sizeof(*context));



    context->gpio_bit1 = gpio_bit;
    if(context->gpio_bit1 == 7) {
        context->reverse1 = 0;
    } else {
        context->reverse1 = 1;
    }
    V = first;
    while(V) {
        if(V->gpio_bit1 == gpio_bit)
            break;
        V = V->next;
    }
    if(V == NULL) {
        avm_gpio_ctrl(context->gpio_bit1, GPIO_PIN, GPIO_OUTPUT_PIN);

        context->gpio_bit2 = gpio_bit + 1;
        if(context->gpio_bit2 == 7) {
            context->reverse2 = 0;
        } else {
            context->reverse2 = 1;
        }
        avm_gpio_ctrl(context->gpio_bit2, GPIO_PIN, GPIO_OUTPUT_PIN);
        context->instance[0] = context;
        context->led = 0;
        context->timer.function = avm_led_gpio_dual_bit_driver_timer;
        context->timer.function = avm_led_gpio_dual_bit_driver_timer;
        init_timer(&(context->timer));
    } else {
        int i;
        context->first = V;
        for(i = 0 ; i < AVM_LED_MAX_INSTANCE; i++) {
            if(V->instance[i] == NULL) {
                V->instance[i] = context;
                context->led = i;
                break;
            }
        }
    }

    context->pos  = pos;
    memcpy(context->name, name, sizeof(context->name));
    context->name[sizeof(context->name) - 1] = '\0';

    avm_led_add_dual_bit_gpio_to_list(context);

    if((context->led != beide_two_pin_led) && (context->led != beide_three_pin_led))
        avm_led_gpio_dual_bit_driver_action((unsigned int)context, 0);

    return (int)context;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_led_gpio_dual_bit_driver_show(unsigned int handle, unsigned int *pPos) {
    struct _asm_led_gpio_dual_bit_context *context = (struct _asm_led_gpio_dual_bit_context *)handle;
    if(pPos)
        *pPos = context->pos;
    DEB_NOTE("[avm_led]: dual gpio driver: GPIO %u %s and GPIO %u %s (pin %u \"%s\")\n",
            context->gpio_bit1,
            context->reverse1 ? "inverted" : "normal",
            context->gpio_bit2,
            context->reverse2 ? "inverted" : "normal",
            context->pos,
            context->name);
    return context->name;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_gpio_dual_bit_driver_exit(unsigned int handle) {
    struct _asm_led_gpio_dual_bit_context *context = (struct _asm_led_gpio_dual_bit_context *)handle;
    int i;
    if(context)  {
        if(context->timer_active)
            del_timer(&(context->timer));
        avm_led_del_dual_bit_gpio_from_list(context);
        if(context->first) {
            for(i = 0 ; i < AVM_LED_MAX_INSTANCE; i++) {
                if(context->first->instance[i] == context) {
                    context->first->instance[i] = NULL;
                    break;
                }
            }
        }
        kfree((void *)context);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avm_led_gpio_dual_bit_driver_timer(unsigned long handle) {
    struct _asm_led_gpio_dual_bit_context *context = (struct _asm_led_gpio_dual_bit_context *)handle;
    struct _asm_led_gpio_dual_bit_context *first   = (context && context->first) ? context->first : context;
    static int test_count = 0;

    avm_led_gpio_dual_bit_driver_action((unsigned int)context, (1 << 31) | 1);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_dual_bit_driver_action(unsigned int handle, unsigned int on) {
    struct _asm_led_gpio_dual_bit_context *context = (struct _asm_led_gpio_dual_bit_context *)handle;
    struct _asm_led_gpio_dual_bit_context *first   = (context && context->first) ? context->first : context;
    unsigned int on1, on2;
    unsigned int called_from_timer = (on & (1 << 31)) ? 1 : 0;

    if(context == 0)
        return 1;
    if(first == 0)
        return 1;

    on &= ~(1 << 31);

    if(first->timer_active) {
        del_timer(&(first->timer));
        first->timer_active = 0;
    }

    switch(context->led) {
        case keine_led:
            return 0;

        case beide_three_pin_led:
            if(on) on1 = 1, on2 = 1;
            else   on1 = 0, on2 = 0;
            break;

        case beide_two_pin_led:
            if(on || called_from_timer) {
                first->timer_active   = 1;
                first->timer.data     = (int)context;
                first->timer.expires  = 1 + jiffies;
                add_timer(&(first->timer));
                switch(first->last_on++) {
                    default:
                    case 0:
                        goto handle_gruene_led;
                    case 1:
                        goto handle_rote_led;
                    case 2:
                        goto handle_gruene_led;
                    case 3:
                        goto handle_gruene_led;
                    case 4:
                        first->last_on = 0;
                        goto handle_rote_led;
                }
            } 
            /*--- kein break ---*/

        case rote_led:
handle_rote_led:
            if(on) on1 = 1, on2 = 0;
            else   on1 = 0, on2 = 0;
            break;

        case rote_led_first ... rote_led_last:
        case gruene_led:
handle_gruene_led:
            if(on) on2 = 1, on1 = 0;
            else   on2 = 0, on1 = 0;
            break;
            
        case rotgruen_led:
            if(on) on2 = 1, on1 = 0;
            else   on2 = 0, on1 = 1;
            break;
    }

    on1 = first->reverse1 ? !on1 :  on1;
    avm_gpio_out_bit(first->gpio_bit1, on1);

    on2 = first->reverse2 ? !on2 :  on2;
    avm_gpio_out_bit(first->gpio_bit2, on2);

    if(first->timer_active == 0) {
        DEB_NOTE("[avm_led]: dual gpio driver: GPIO %u %s and GPIO %u %s\n",
                first->gpio_bit1, on1 ? "up" : "down",
                first->gpio_bit2, on2 ? "up" : "down");
    }

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_dual_bit_driver_get_flags(void) {
    return AVM_LED_DRIVER_FLAGS_PRIO_2_INSTANCE;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_dual_bit_driver_sync(unsigned int handle, unsigned int state_id) {
    return 0;
}
