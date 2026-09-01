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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <asm/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>

#define AVM_EVENT_PUSH_BUTTON_DEBUG

#if defined(AVM_EVENT_PUSH_BUTTON_DEBUG)
#define DBG_TRC(args...)   printk(KERN_INFO args)
#else /*--- #if defined(AVM_EVENT_PUSH_BUTTON_DEBUG) ---*/
#define DBG_TRC(args...) 
#endif /*--- #else ---*/ /*--- #if !defined(AVM_EVENT_PUSH_BUTTON_DEBUG) ---*/

#define AVM_EVENT_INTERNAL
#include <linux/avm_event.h>
#include <asm/atomic.h>
#include "avm_sammel.h"
#include "avm_event.h"
#include "avm_led_driver.h"  /* rename gpio funktions */

#define AVM_EVENT_PUSH_BUTTON_SAMPLE_TIME       250
/*--- #define AVM_EVENT_PUSH_BUTTON_SAMPLE_TIME       5000 ---*/
#define MSEC(x)                                 ((HZ * (x)) / 1000)

/*------------------------------------------------------------------------------------------*\
 * 2.4 Kernel H-Files
\*------------------------------------------------------------------------------------------*/
#if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE 
#include <asm/smplock.h>
#include <asm/avalanche/avalanche_map.h>
#include <asm/avalanche/sangam/hw_gpio.h>
#endif /*--- #if KERNEL_VERSION(2, 6, 0) > LINUX_VERSION_CODE ---*/ 

/*------------------------------------------------------------------------------------------*\
 * 2.6 Kernel H-Files
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mach_avm.h>
#endif

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
static struct _avm_event_push_button_state {
    void *handle;
    struct timer_list timer;
} Button;

extern int avm_event_enable_push_button;

static struct _avm_event_push_config {
    unsigned long long jiffies_64;
    unsigned int gpio;
    volatile unsigned int *register_address; /*--- != NULL -> es handelt sich um kein GPIO sondern Registeraddresse (GPIO gibt dann das Bit an ---*/
    unsigned int value;
    unsigned int locked;
    unsigned int enable;
    enum { taster, pre_taster, switcher } button_type;  /*--- Taster, Pre_Taster: vor Notify darf Event gesammelt werden, oder Schalter ---*/
    unsigned int key[3];
    int lastkey;                   /*--- nur fuer Pre_Taster: zu letzt gemerkte Tastendrücke ---*/
    char *names[3];
    char *name;
    struct resource *gpio_resource;
} avm_event_push_config[] = {
    {
        /*--- normaler Taster (keine Event-Meldung im Notify), da GPIO 8 mit Xilinix-Done geshared ---*/
        gpio: CONFIG_AVM_PUSH_BUTTON_GPIO,
        key: { avm_event_push_button_key_1, avm_event_push_button_key_2, avm_event_push_button_key_3 },
        enable: 1,
        names: { "button_key_1", "button_key_2", "button_key_3" },
        name: "Wlan-Taster",

    },
#ifdef CONFIG_AVM_PUSH_BUTTON2
    {
        /*--- Pre-Taster (Event-Meldung im Notify) ---*/
        gpio: CONFIG_AVM_PUSH_BUTTON2_GPIO,
        key: { avm_event_push_button_key_4, avm_event_push_button_key_5, avm_event_push_button_key_6 },
        names: { "button_key_4", "button_key_5", "button_key_6" },
        button_type: pre_taster,
#if defined (CONFIG_AVM_PIGLET) || defined (CONFIG_AVM_PIGLET_MODULE)\
       	|| defined (CONFIG_AVM_PIGLET_NOEMIF) || defined (CONFIG_AVM_PIGLET_NOEMIF_MODULE)
        enable: 0,
#else
        enable: 1,
#endif
        name: "Werkseinstellungs-Taster"
    },
#endif /*--- #ifdef CONFIG_AVM_PUSH_BUTTON2 ---*/
#ifdef CONFIG_AVM_PUSH_BUTTON3
    /*--- wird erst nach Laden des FPGA's vollstaendig konfiguriert ---*/
    {
        gpio: CONFIG_AVM_PUSH_BUTTON3_GPIO,
        key: { avm_event_push_button_key_7, avm_event_push_button_key_8, avm_event_push_button_key_9 },
        enable: 0,
        names: { "button_key_7", "button_key_8", "button_key_9" },
        name: "ATA-Schalter"

    },
#endif /*--- #ifdef CONFIG_AVM_PUSH_BUTTON3 ---*/
#ifdef CONFIG_AVM_PUSH_BUTTON4
    /*--- wird erst nach Laden des FPGA's vollstaendig konfiguriert ---*/
    {
        gpio: CONFIG_AVM_PUSH_BUTTON4_GPIO,
        key: { avm_event_push_button_key_10, avm_event_push_button_key_11, avm_event_push_button_key_12 },
        enable: 0,
        names: { "button_key_10", "button_key_11", "button_key_12" },
        name: "Taster_4"

    },
#endif /*--- #ifdef CONFIG_AVM_PUSH_BUTTON4 ---*/
#ifdef CONFIG_AVM_PUSH_BUTTON5
    /*--- wird erst nach Laden des FPGA's vollstaendig konfiguriert ---*/
    {
        gpio: CONFIG_AVM_PUSH_BUTTON5_GPIO,
        key: { avm_event_push_button_key_21, avm_event_push_button_key_22, avm_event_push_button_key_23 },
        enable: 0,
        names: { "button_key_21", "button_key_22", "button_key_23" },
        name: "Taster_5"

    },
#endif /*--- #ifdef CONFIG_AVM_PUSH_BUTTON5 ---*/
    {   
        name: NULL
    }
};


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_event_push_button_timer_handler(unsigned long Data __attribute__((unused))) {
    unsigned int state, TriggerEvent;
    struct _avm_event_push_config *P = &avm_event_push_config[0];

    del_timer(&(Button.timer));

    while(P->name) {
        if(P->enable == 0) {
            P++;
            continue;
        }
        TriggerEvent = 0;
        /*--- DBG_TRC("[A]"); ---*/
        if(P->jiffies_64 == (unsigned long long)0)
            P->jiffies_64 = jiffies_64;
        if(P->register_address == NULL) {
            state = avm_gpio_in_bit(P->gpio) ? avm_event_push_button_gpio_low : avm_event_push_button_gpio_high;
            /*--- DBG_TRC("X%s%uX", P->name, state); ---*/
        } else {
            state = ((*P->register_address >> P->gpio) & 1) ? avm_event_push_button_gpio_high : avm_event_push_button_gpio_low;
        }
        if(P->button_type == switcher) {
            /*--- es handelt sich um einen Schalter also bei Unterschied sofort reagieren ---*/
            if(state != P->value) {
                TriggerEvent = 1;
            }
        } else {
            if(state == avm_event_push_button_gpio_low)
                P->locked = 0;
            if(P->locked) {  /*--- warten bis Taste losgelassen wurde ---*/
                P++;
                continue;
            }
            /*--- es handelt sich um einen Taster ---*/
            /*--- new button pressed ---*/
            if(state == avm_event_push_button_gpio_high && P->value == avm_event_push_button_gpio_low) {
                /*--- DBG_TRC("[B]"); ---*/
                P->jiffies_64 = jiffies_64;
                P->value = avm_event_push_button_gpio_high;
                DBG_TRC("[avm_new]push_button '%s' presses: GPIO=%u time=%u\n", P->name, P->gpio, (unsigned int)P->jiffies_64);
            }
            /*--- losgelassen ---*/
            if(state == avm_event_push_button_gpio_low && P->value == avm_event_push_button_gpio_high) {
                TriggerEvent = 1;
            } else if(state == avm_event_push_button_gpio_high) {
                unsigned int pressed = (unsigned int)(jiffies_64 - P->jiffies_64);
                if(pressed > MSEC(CONFIG_AVM_PUSH_BUTTON_TIME_2 + 100)) {
                    P->locked = 1;
                    state = avm_event_push_button_gpio_low;
                    TriggerEvent = 1;
                }
            }
        }
        /*--- new button released ---*/
        if(TriggerEvent) {
            struct _avm_event_push_button *event;
            event = (struct _avm_event_push_button *)kmalloc(sizeof(struct _avm_event_push_button), GFP_ATOMIC);
            if(event) {
                event->id      = avm_event_id_push_button;
                if(P->button_type == switcher) {
                    event->key = (state == avm_event_push_button_gpio_low) ? P->key[0] : P->key[1];
                    event->pressed = 0;
                } else {
                    event->pressed = (unsigned int)(jiffies_64 - P->jiffies_64);
                    if(event->pressed < MSEC(CONFIG_AVM_PUSH_BUTTON_TIME_1)) {
                        event->key = P->key[0];
                    } else if(event->pressed < MSEC(CONFIG_AVM_PUSH_BUTTON_TIME_2)) {
                        event->key = P->key[1];
                    } else {
                        event->key = P->key[2];
                    }
                    event->pressed *= 10;
                }
                P->value   = state;
                P->lastkey = event->key; 
                /*--- vor avm_event_source_trigger aufrufen, da event dort freigegeben wird ---*/
                DBG_TRC("[avm_new]push_button '%s', released: GPIO=%u presses=%u key=%s\n", 
                        P->name,
                        P->gpio, 
                        (unsigned int)event->pressed,
                        P->names[event->key - P->key[0]]
                );
                avm_event_source_trigger(Button.handle, avm_event_id_push_button, sizeof(struct _avm_event_push_button), event);
            }
        }
        P++;
    }
    Button.timer.function = avm_event_push_button_timer_handler;
    Button.timer.data     = -1;
    Button.timer.expires  = jiffies + MSEC(AVM_EVENT_PUSH_BUTTON_SAMPLE_TIME);
    add_timer(&(Button.timer));
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_event_push_button_notify(void *Context __attribute__((unused)), enum _avm_event_id id __attribute__((unused))) {
    struct _avm_event_push_button *event;
    struct _avm_event_push_config *P = &avm_event_push_config[0];
    unsigned int state;
    
    while(P->name) {
        if(P->enable == 0) {
            P++;
            continue;
        }
        if(P->button_type == switcher || (P->button_type == pre_taster && P->lastkey != -1)) {
            /*--- nur Event fuer Schalter nach oben, oder wenn Pre-Taster-Events registriert wurde ---*/
            event = (struct _avm_event_push_button *)kmalloc(sizeof(struct _avm_event_push_button), GFP_ATOMIC);
            if(event == NULL)
                return;
            event->id = avm_event_id_push_button;
            if(P->button_type == switcher) {
                if(P->register_address == NULL) {
                    state = avm_gpio_in_bit(P->gpio) ? avm_event_push_button_gpio_low : avm_event_push_button_gpio_high;
                } else {
                    state = ((*P->register_address >> P->gpio) & 1) ? avm_event_push_button_gpio_high : avm_event_push_button_gpio_low;
                }
                event->key     = (state == avm_event_push_button_gpio_low) ? P->key[0] : P->key[1];
            } else {
                event->key     = P->lastkey;
            }
            event->pressed = 0;
            DBG_TRC("[avm_new]push_button_notify '%s', key=%s\n", P->name, P->names[event->key - P->key[0]]);
            avm_event_source_trigger(Button.handle, avm_event_id_push_button, sizeof(struct _avm_event_push_button), event);
        }
        P++;
    }
}

/*------------------------------------------------------------------------------------------*\
 * Ermoeglicht es die Nutzung eines Buttons extern an/abzuschalten und entsprechend zu 
 * konfigurieren
 * Dies ist z.B. notwendig, wenn es sich bei dem Button um kein GPIO, sondern einem FPGA-Register 
 * handelt, der erst nach laden des FPGA's zur Verfügung steht.
 * name: Suchname der Schalters
 * enable:             0 aus, 1 an: Übernahme der Werte nur bei enable = 1, 
 *                     2 keine Übernahme aber enable
 * gpio:               GPIO/Register-Bit 
 * register_address:   != NULL Register-Adresse anstelle GPIO
 * button_type:        0 = Taster, 1 = Schalter
 * Returnwert: 0: ok und gesetzt 1 Schalter nicht gefunden
\*------------------------------------------------------------------------------------------*/
int avm_event_push_button_ctrl(char *name, unsigned int enable, unsigned int gpio, volatile unsigned int *register_addr, unsigned int button_type) {
    struct _avm_event_push_config *P = &avm_event_push_config[0];
    if(avm_event_enable_push_button == 0) {
        printk(KERN_WARNING "[avm_led] old button interface disabled\n");
        return 0;
    }
    while(P->name) {
        if(strcmp(name, P->name) == 0) {
            if(enable) {
                if(enable == 1) {
                    /*--- Uebernahme der Werte ---*/
                    P->gpio             = gpio;
                    P->register_address = register_addr;
                    P->button_type      = button_type ? switcher : taster; 
                }
                if(P->register_address == NULL) {
                    avm_gpio_ctrl(P->gpio, GPIO_PIN, GPIO_INPUT_PIN);
#ifdef CONFIG_MIPS_UR8
                    if (P->gpio == 23) {
                        avm_gpio_ctrl(P->gpio, GPIO_PIN, GPIO_OUTPUT_PIN);
                        avm_gpio_out_bit(P->gpio, 1);
                    }
#endif
                    P->value = avm_gpio_in_bit(P->gpio) ? avm_event_push_button_gpio_low : avm_event_push_button_gpio_high;
                    DBG_TRC("[avm_new]push_button_ctrl: gpio=%u value=%u enabled\n", P->gpio, P->value);
                }
            }
            P->enable        = enable;
            DBG_TRC("[avm_new]push_button_ctrl:'%s': enable=%u gpio=%u regaddr=%p button_type=%u\n", name, enable, P->gpio, P->register_address, P->button_type);
            return 0;
        }
        P++;
    }
    DBG_TRC("[avm_new]push_button_ctrl error: can't find button %s: to enable to %u\n", name, enable);
    return 1;
}
EXPORT_SYMBOL(avm_event_push_button_ctrl);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_event_push_button_init(void) {
    struct _avm_event_push_config *P = &avm_event_push_config[0];
    if(avm_event_enable_push_button == 0)
        return 0;

    Button.handle         = avm_event_source_register("push_button", (unsigned long long)1 << avm_event_id_push_button,avm_event_push_button_notify, (void *)0);

	init_timer(&(Button.timer));
    Button.timer.function = avm_event_push_button_timer_handler;
    Button.timer.data     = -1;
    Button.timer.expires  = jiffies + MSEC(AVM_EVENT_PUSH_BUTTON_SAMPLE_TIME);

    while(P->name) {
        int size = sizeof(struct resource) + strlen(P->name) + 1;
        P->gpio_resource = kzalloc(size, GFP_ATOMIC);
        if( P->gpio_resource == NULL) {
            return 0;
        }
        {   
            char *ptmp = (char *)P->gpio_resource + sizeof(struct resource);
            strcpy(ptmp, P->name);
            P->gpio_resource->name  = ptmp;
        }
        P->gpio_resource->start = P->gpio;
        P->gpio_resource->end   = P->gpio;
        if(request_resource(&gpio_resource, P->gpio_resource)) {
            printk(KERN_ERR "GPIO %u: already in use, push button init failed\n", P->gpio);
            kfree(P->gpio_resource);
            return 0;
        }

        if(P->register_address == NULL && P->enable) {
            avm_gpio_ctrl(P->gpio, GPIO_PIN, GPIO_INPUT_PIN);
#ifdef CONFIG_MIPS_UR8
            if (P->gpio == 23) {
                avm_gpio_ctrl(P->gpio, GPIO_PIN, GPIO_OUTPUT_PIN);
                avm_gpio_out_bit(P->gpio, 1);
            }
#endif
            P->value   = avm_gpio_in_bit(P->gpio) ? avm_event_push_button_gpio_low : avm_event_push_button_gpio_high;
            DBG_TRC("[avm_new] push_button_gpio=%u value=%u enabled\n", P->gpio, P->value);
        }
        P->lastkey = -1; 
        P++;
    }
    add_timer(&(Button.timer));
    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
late_initcall(avm_event_push_button_init);
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_event_push_button_deinit(void) {
    struct _avm_event_push_config *P = &avm_event_push_config[0];

    if(avm_event_enable_push_button == 0)
        return;
    avm_event_enable_push_button = 0;
    del_timer(&Button.timer);
    avm_event_source_release(Button.handle);

    while(P->name) {
        if(P->gpio_resource) {
	        release_resource(P->gpio_resource);
            kfree(P->gpio_resource);
            P->gpio_resource = NULL;
        }
        P++;
    }
}

EXPORT_SYMBOL(avm_event_push_button_deinit);
