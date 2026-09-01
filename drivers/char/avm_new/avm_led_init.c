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
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/errno.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mach_avm.h>
#else
#endif

#include <linux/avm_led.h>
#include "avm_sammel.h"
#include "avm_led.h"

#if AVM_LED_MAX_INSTANCE != 10
#error AVM_LED_MAX_INSTANCE is nicht 10
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct _avm_led_blink_mode_driver mode_driver[] = {
    {  
        init:   avm_led_mode_init,
        exit:   avm_led_mode_exit,
        stop:   avm_led_mode_stop,
        action: avm_led_mode_action,
        stop:   avm_led_mode_stop,
        sync:   avm_led_mode_sync,
        show:   NULL
    }
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
    ,
    {  
        init:   avm_led_profile_init,
        exit:   avm_led_profile_exit,
        stop:   avm_led_profile_stop,
        action: avm_led_profile_action,
        stop:   avm_led_profile_stop,
        sync:   avm_led_profile_sync,
        show:   NULL
    }
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_low_level_driver gpio_driver[] = {
    /*--------------------------------------------------------------------------------------*\
     * low level driver index 0
    \*--------------------------------------------------------------------------------------*/
    {  /* single PIN, gpio direct mapped */
#ifdef CONFIG_AVM_LED_OUTPUT_GPIO
        name:       "gpio_bit_driver",
        init:       avm_led_gpio_bit_driver_init,
        show:       avm_led_gpio_bit_driver_show,
        exit:       avm_led_gpio_bit_driver_exit,
        action:     avm_led_gpio_bit_driver_action,
        sync:       NULL
#else
        init:       NULL
#endif /*--- #ifdef CONFIG_AVM_LED_OUTPUT_GPIO ---*/
    },
    /*--------------------------------------------------------------------------------------*\
     * low level driver index 1
    \*--------------------------------------------------------------------------------------*/
    {  /* all PINs, gpio direct mapped */
#ifdef CONFIG_AVM_LED_OUTPUT_GPIO
        name:       "gpio_mask_driver",
        init:       avm_led_gpio_mask_driver_init,
        show:       avm_led_gpio_mask_driver_show,
        exit:       avm_led_gpio_mask_driver_exit,
        action:     avm_led_gpio_mask_driver_action,
        sync:       NULL
#else
        init:       NULL
#endif /*--- #ifdef CONFIG_AVM_LED_OUTPUT_GPIO ---*/
    },
    /*--------------------------------------------------------------------------------------*\
     * low level driver index 2
    \*--------------------------------------------------------------------------------------*/
    {  /* single PIN, gpio shift register mapped */
#ifdef CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER
        name:       "shift_register_bit_driver",
        init:       avm_led_shift_register_bit_driver_init,
        show:       avm_led_shift_register_bit_driver_show,
        exit:       avm_led_shift_register_bit_driver_exit,
        action:     avm_led_shift_register_bit_driver_action,
        sync:       NULL
#else
        init:       NULL
#endif /*--- #ifdef CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER ---*/
    },
    /*--------------------------------------------------------------------------------------*\
     * low level driver index 3
    \*--------------------------------------------------------------------------------------*/
    {  /* all PINs, gpio shift register mapped */
#ifdef CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER
        name:       "shift_register_mask_driver",
        init:       avm_led_shift_register_mask_driver_init,
        show:       avm_led_shift_register_mask_driver_show,
        exit:       avm_led_shift_register_mask_driver_exit,
        action:     avm_led_shift_register_mask_driver_action,
        sync:       NULL
#else
        init:       NULL
#endif /*--- #ifdef CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER ---*/
    },
    /*--------------------------------------------------------------------------------------*\
     * low level driver index 4 (dual color led)
    \*--------------------------------------------------------------------------------------*/
    {
#ifdef CONFIG_AVM_LED_OUTPUT_DUAL_GPIO
        name:       "gpio_dual_bit_driver",
        init:       avm_led_gpio_dual_bit_driver_init,
        show:       avm_led_gpio_dual_bit_driver_show,
        exit:       avm_led_gpio_dual_bit_driver_exit,
        action:     avm_led_gpio_dual_bit_driver_action,
        sync:       avm_led_gpio_dual_bit_driver_sync,
        get_flags:  avm_led_gpio_dual_bit_driver_get_flags,
#else
        init:       NULL
#endif /*--- #ifdef CONFIG_AVM_LED_OUTPUT_DUAL_GPIO ---*/
    },
    /*--------------------------------------------------------------------------------------*\
     * low level driver index 5 (allway is not existent)
    \*--------------------------------------------------------------------------------------*/
    {
        init:       NULL,
        show:       NULL,
        exit:       NULL,
        action:     NULL,
        sync:       NULL
    },
    /*--------------------------------------------------------------------------------------*\
     * low level driver index 6
    \*--------------------------------------------------------------------------------------*/
    {  /* all PINs, bier holen (bier holen, walking led) */
#if defined(CONFIG_AVM_LED_BIER_HOLEN) && defined(CONFIG_AVM_LED_OUTPUT_GPIO)
        name:       "gpio_bier_holen_driver",
        init:       avm_led_gpio_bier_holen_driver_init,
        show:       avm_led_gpio_bier_holen_driver_show,
        exit:       avm_led_gpio_bier_holen_driver_exit,
        action:     avm_led_gpio_bier_holen_driver_action,
        sync:       NULL
#else /*--- #if defined(CONFIG_AVM_LED_BIER_HOLEN) && defined(CONFIG_AVM_LED_OUTPUT_GPIO) ---*/
        name:       NULL
#endif /*--- #if defined(CONFIG_AVM_LED_BIER_HOLEN) && defined(CONFIG_AVM_LED_OUTPUT_GPIO) ---*/
    },
    /*--------------------------------------------------------------------------------------*\
     * low level driver index 7 (allway is not existent)
    \*--------------------------------------------------------------------------------------*/
    {
        init:       NULL,
        show:       NULL,
        exit:       NULL,
        action:     NULL,
        sync:       NULL
    },
    /*--------------------------------------------------------------------------------------*\
     * low level driver index 8
    \*--------------------------------------------------------------------------------------*/
    {  /* all PINs, bier holen (bier holen, walking led) */
#if defined(CONFIG_AVM_LED_BIER_HOLEN) && defined(CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER)
        name:       "shift_register_bier_holen_driver",
        init:       avm_led_shift_register_bier_holen_driver_init,
        show:       avm_led_shift_register_bier_holen_driver_show,
        exit:       avm_led_shift_register_bier_holen_driver_exit,
        action:     avm_led_shift_register_bier_holen_driver_action,
        sync:       NULL
#else /*--- #if defined(CONFIG_AVM_LED_BIER_HOLEN) && defined(CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER) ---*/
        name:       NULL
#endif /*--- #if defined(CONFIG_AVM_LED_BIER_HOLEN) && defined(CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER) ---*/
    },
    /*--------------------------------------------------------------------------------------*\
     * low level driver index 9 (allway is not existent)
    \*--------------------------------------------------------------------------------------*/
    {
        init:       NULL,
        show:       NULL,
        exit:       NULL,
        action:     NULL,
        sync:       NULL
    }
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_AVM_LED_BIER_HOLEN) || defined(CONFIG_AVM_SIMPLE_PROFILING) 
struct _avm_led_state avm_led_states[3][AVM_LED_STATE_TABLE_SIZE + 1] 
#else /*--- #if defined(CONFIG_AVM_LED_BIER_HOLEN) ---*/
struct _avm_led_state avm_led_states[1][AVM_LED_STATE_TABLE_SIZE + 1] 
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_AVM_LED_BIER_HOLEN) ---*/
                                                                        = {
  {
    {   /*--- index 0 ---*/
        mode_index: 0,  /* OFF State */
        param1:     0,
        param2:     0
    },
    {   /*--- index 1 ---*/
        mode_index: 1,  /* ON State */
        param1:     0,
        param2:     0
    },
    {   /*--- index 2 ---*/
        mode_index: AVM_LED_GOTO_PRIVIOUS_STATE,
        param1:     0,
        param2:     0
    },  
    { /*--- index 3 ---*/ 
        mode_index: 8,  /* bier holen */
        param1:     6000,
        param2:     0
    }, 
    {  /*--- index 4 ---*/
        mode_index: 8,  /* bier holen */
        param1:     4000,
        param2:     0
    }, 
    {  /*--- index 5 ---*/
        mode_index: 8,  /* bier holen */
        param1:     3000,
        param2:     0
    }, 
    {  /*--- index 6 ---*/
        mode_index: 8,  /* bier holen */
        param1:     2000,
        param2:     0
    }, 
    {  /*--- index 7 ---*/
        mode_index: 8,  /* bier holen */
        param1:     1500,
        param2:     0
    }, 
    {   /*--- index 8 ---*/
        mode_index: 2,  /* Flash on */
        param1:     1000,
        param2:     0
    },
    {   /*--- index 9 ---*/
        mode_index: 2,  /* Flash on */
        param1:     500,
        param2:     0
    },
    {   /*--- index 10 ---*/
        mode_index: 2,  /* Flash on */
        param1:     250,
        param2:     0
    },
    {   /*--- index 11 ---*/
        mode_index: 2,  /* Flash on */
        param1:     125,
        param2:     0
    },
    {   /*--- index 12 ---*/
        mode_index: 3,  /* Flash off */
        param1:     1000,
        param2:     0
    },
    {   /*--- index 13 ---*/
        mode_index: 3,  /* Flash off */
        param1:     500,
        param2:     0
    },
    {   /*--- index 14 ---*/
        mode_index: 3,  /* Flash off */
        param1:     250,
        param2:     0
    },
    {   /*--- index 15 ---*/
        mode_index: 3,  /* Flash off */
        param1:     125,
        param2:     0
    },
    {   /*--- index 0x10 ---*/
        mode_index: 4,  /* Symmetric Blink */
        param1:     1000,
        param2:     1000
    },
    {   /*--- index 0x11 ---*/
        mode_index: 4,  /* Symmetric Blink */
        param1:     500,
        param2:     500
    },
    {   /*--- index 0x12 ---*/
        mode_index: 4,  /* Symmetric Blink */
        param1:     250,
        param2:     250
    },
    {   /*--- index 0x13 ---*/
        mode_index: 4,  /* Symmetric Blink */
        param1:     125,
        param2:     125
    },
    {   /*--- index 0x14 ---*/
        mode_index: 9,  /* walking led */
        param1:     4000,
        param2:     0
    },
    {   /*--- index 0x15 ---*/
        mode_index: 9,  /* walking led */
        param1:     3000,
        param2:     0
    },
    {   /*--- index 0x16 ---*/
        mode_index: 9,  /* walking led */
        param1:     2000,
        param2:     0
    },
    {   /*--- index 0x17 ---*/
        mode_index: 9,  /* walking led */
        param1:     1000,
        param2:     0
    }
  }
#if defined(CONFIG_AVM_LED_BIER_HOLEN) || defined(CONFIG_AVM_SIMPLE_PROFILING) 
  ,{
  },
  {
      {
        mode_index: 0
      },
      {
        mode_index: 1
      },
      {
        mode_index: 2
      },
      {
        mode_index: 3
      },
      {
        mode_index: 4
      },
      {
        mode_index: 5
      },
      {
        mode_index: 6
      },
      {
        mode_index: 7
      }
  }
#endif /*--- #if defined(CONFIG_AVM_LED_BIER_HOLEN) || defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
};


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _new_led_id {
    char *Name;
    unsigned int instance;
    enum _avm_event_led_id id;
} new_led_id[] = {
    { "pppoe",    0, logical_led_ppp },
    { "error",    0, logical_led_error },
    { "ab",       1, logical_led_pots },
    { "info",     0, logical_led_info },
    { "info",     1, logical_led_traffic },
    { "budget",   0, logical_led_traffic },
    { "info",     2, logical_led_freecall },
    { "info",     3, logical_led_avmusbwlan },
    { "internet", 0, logical_led_sip },
    { "internet", 1, logical_led_mwi },
    { "ab",       2, logical_led_fest_mwi },
    { "isdn",     0, logical_led_isdn_d },
    { "isdn",     1, logical_led_isdn_b1 },
    { "isdn",     2, logical_led_isdn_b2 },
    { "cpmac",    0, logical_led_lan },
    { "cpmac",    1, logical_led_lan1 },
    { "adsl",     0, logical_led_adsl },
    { "adsl",     1, logical_led_power },
    { "usb",      0, logical_led_usb },
    { "wlan",     0, logical_led_wifi },
    { NULL,       0, logical_led_last }
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _avm_event_led_id avalanche_led_get_new_id_from_handle(int handle) {
    struct _avm_virt_led *V = (struct _avm_virt_led *)handle;
    struct _new_led_id *I = &new_led_id[0];

    /*--- printk("[avalanche_led_get_new_id_from_handle]: look for \"%s\" instance %u\n", ---*/
            /*--- V->name, V->instance); ---*/

    while(I->Name) {
        if((!strcmp(I->Name, V->name)) && (I->instance == V->instance)) {
    /*--- printk("found id=%u\n", I->id); ---*/
            return I->id;
        }
        I++;
    }
    /*--- printk("not found\n"); ---*/
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct _avm_led_list virt_led_list;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER
void avm_led_init_hardware(void) {
    avm_gpio_ctrl(CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_MASTER_RESET, GPIO_PIN, GPIO_OUTPUT_PIN); 
    avm_gpio_ctrl(CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_SHIFT_CLOCK, GPIO_PIN, GPIO_OUTPUT_PIN); 
    avm_gpio_ctrl(CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_STORAGE_CLOCK, GPIO_PIN, GPIO_OUTPUT_PIN); 
    avm_gpio_ctrl(CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER_DATA, GPIO_PIN, GPIO_OUTPUT_PIN); 
}
#endif /*--- #ifdef CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_init_tables(void) {
    unsigned int i, count;

    for(i = 0 ; i < AVM_LED_STATE_TABLE_SIZE ; i++)
        avm_led_states[0][i].index = i;

#if defined(CONFIG_AVM_LED_BIER_HOLEN)
    {
        int mask, wert;

        for(i = 0 ; i < AVM_LED_STATE_TABLE_SIZE ; i++)
            avm_led_states[1][i].index = i;

        for(i = 0 ; i < 100 ; i++) {
            int bits, mask1;
            wert = i ? (i-1) / 4 : 0;
            mask1=((1 << ((wert / 4) * 4)) - 1);
            bits = wert % 4 == 2 ? 0x5 : wert % 4 == 3 ? 0x7 : wert % 4;
            mask=mask1 | (bits  << ((wert / 4)  * 4));
 
            avm_led_states[1][i].mode_index = 10; /* VU - Meter */
            avm_led_states[1][i].param1 = mask;
        }
        avm_led_states[1][100].mode_index = 10; /* VU - Meter */
        avm_led_states[1][100].param1 = mask;
    }
#endif /*--- #if defined(CONFIG_AVM_LED_BIER_HOLEN) ---*/

    /*--------------------------------------------------------------------------------------*\
     * 0x40 - 0x80
    \*--------------------------------------------------------------------------------------*/
    for(i = 0x40, count = 1 ; count <= 16 ; i += 4, count++) {
        avm_led_states[0][i + 0].mode_index = 5;
        avm_led_states[0][i + 1].mode_index = 5;
        avm_led_states[0][i + 2].mode_index = 5;
        avm_led_states[0][i + 3].mode_index = 5;
        avm_led_states[0][i + 0].param1     = 1000;
        avm_led_states[0][i + 1].param1     = 500;
        avm_led_states[0][i + 2].param1     = 250;
        avm_led_states[0][i + 3].param1     = 125;
        avm_led_states[0][i + 0].param2     = count;
        avm_led_states[0][i + 1].param2     = count;
        avm_led_states[0][i + 2].param2     = count;
        avm_led_states[0][i + 3].param2     = count;
    }
    /*--------------------------------------------------------------------------------------*\
     * 0x80 - 0xC0
    \*--------------------------------------------------------------------------------------*/
    for(count = 1 ; count <= 16 ; i += 4, count++) {
        avm_led_states[0][i + 0].mode_index = 6;
        avm_led_states[0][i + 1].mode_index = 6;
        avm_led_states[0][i + 2].mode_index = 6;
        avm_led_states[0][i + 3].mode_index = 6;
        avm_led_states[0][i + 0].param1     = 1000;
        avm_led_states[0][i + 1].param1     = 500;
        avm_led_states[0][i + 2].param1     = 250;
        avm_led_states[0][i + 3].param1     = 125;
        avm_led_states[0][i + 0].param2     = count;
        avm_led_states[0][i + 1].param2     = count;
        avm_led_states[0][i + 2].param2     = count;
        avm_led_states[0][i + 3].param2     = count;
    }
    /*--------------------------------------------------------------------------------------*\
     * 0xC0 - 0x100
    \*--------------------------------------------------------------------------------------*/
    for(count = 1 ; count <= 16 ; i += 4, count++) {
        avm_led_states[0][i + 0].mode_index = 7;
        avm_led_states[0][i + 1].mode_index = 7;
        avm_led_states[0][i + 2].mode_index = 7;
        avm_led_states[0][i + 3].mode_index = 7;
        avm_led_states[0][i + 0].param1     = 1000;
        avm_led_states[0][i + 1].param1     = 500;
        avm_led_states[0][i + 2].param1     = 250;
        avm_led_states[0][i + 3].param1     = 125;
        avm_led_states[0][i + 0].param2     = count;
        avm_led_states[0][i + 1].param2     = count;
        avm_led_states[0][i + 2].param2     = count;
        avm_led_states[0][i + 3].param2     = count;
    }

    avm_led_states[0][AVM_LED_GOTO_PRIVIOUS_STATE].mode_index = AVM_LED_GOTO_PRIVIOUS_STATE;

    memset(&virt_led_list, 0, sizeof(virt_led_list));
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_add_to_list(struct _avm_led_list *L, struct _avm_led_list_header *V) {
    DEB_NOTE("[avm_led_add_virt_led] add 0x%p\n", V);
    if(L->first == NULL) {
        DEB_NOTE("[avm_led_add_virt_led] add to first\n");
        V->prev = V->next = NULL;
        L->first = L->last = V;
        return 0;
    }
    V->prev = L->last;
    V->next = NULL;
    L->last->next = V;
    L->last = V;
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_del_from_list(struct _avm_led_list *L, struct _avm_led_list_header *V) {
    if(V->next) {
        V->next->prev = V->prev;
    }  else { /* bin letzter */
        L->last = V->prev; /* kann u.U. NULL sein */
    }
        
    if(V->prev) { /* bin erster */
        V->prev->next = V->next;
    } else {
        L->first = V->next; /* kann u.U. NULL sein */
    }
    V->next = V->prev = NULL;
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_register_led(char *virt_led_name, unsigned int virt_led_instance, unsigned int driver_type, unsigned int gpio, unsigned int pin_pos, char *pin_name) {
    struct _avm_virt_led *virt_led;
    unsigned int i;

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    if(virt_led_instance >= AVM_LED_MAX_INSTANCE) {
        DEB_ERR("[avm_led_register_led] instance not in range (< %u)\n", AVM_LED_MAX_INSTANCE);
        return -ENXIO;
    }
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    if((driver_type >= AVM_LED_MAX_GPIO_DRIVER) && (driver_type != AVM_LED_GPIO_DRIVER_INCREDIBLE)) {
        DEB_ERR("[avm_led_register_led] instance not in range (< %u)\n", AVM_LED_MAX_GPIO_DRIVER);
        return -ENXIO;
    }

    if(avm_led_monitor_level & AVM_LED_MONITOR_REGISTER)
        printk("[avm_led] DEF %s,%u = %u,0x%x,%u,%s\n", 
                virt_led_name, virt_led_instance, driver_type, gpio, pin_pos, pin_name);

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    virt_led = (struct _avm_virt_led *)kmalloc(sizeof(struct _avm_virt_led), GFP_ATOMIC);

    if(virt_led == NULL) {
        DEB_ERR("[avm_led_register_led] no memory for virt_led (%u bytes)\n", sizeof(struct _avm_virt_led));
        return -ENOMEM;
    }

    memset(virt_led, 0, sizeof(struct _avm_virt_led));

    virt_led->instance_handle = avm_led_alloc_instance(virt_led_name);
    if(virt_led->instance_handle == NULL) {
        kfree(virt_led);
        DEB_ERR("[avm_led_register_led] no memory for instance_handle (%u bytes)\n", sizeof(struct _avm_led_instance_handle ));
        return -ENOMEM;
    }

    /*--- instance und namen setzen ---*/
    i = strlen(virt_led_name);
    i = i >= sizeof(virt_led->name) ? sizeof(virt_led->name) - 1 : i;
    memcpy(virt_led->name, virt_led_name, i);
    virt_led->name[i] = '\0';
    virt_led->instance = virt_led_instance;
    virt_led->instance_handle->V[virt_led->instance] = virt_led;

    if(driver_type == AVM_LED_GPIO_DRIVER_INCREDIBLE) {
        DEB_NOTE("[avm_led] incredible virtual led (without states, modes, and driver)\n");
        avm_led_add_to_list(&virt_led_list, (struct _avm_led_list_header *)virt_led);
        return 0;
    }

    virt_led->led_states = (struct _avm_led_state *)kmalloc(sizeof(struct _avm_led_state) * (AVM_LED_STATE_TABLE_SIZE + 1), GFP_ATOMIC);
    
    if(virt_led->led_states == NULL) {
        kfree(virt_led);
        DEB_ERR("[avm_led_register_led] no memory for led_states (%u bytes)\n", sizeof(struct _avm_led_state) * (AVM_LED_STATE_TABLE_SIZE + 1));
        return -ENOMEM;
    }

#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
    if(!strncmp(virt_led_name, "profile", 7)) {
        memcpy(virt_led->led_states, avm_led_states[2], sizeof(struct _avm_led_state) * (AVM_LED_STATE_TABLE_SIZE + 1));
    } else
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/ 
    /*--- muster table kopieren ---*/
    if(driver_type >= 8)
        memcpy(virt_led->led_states, avm_led_states[1], sizeof(struct _avm_led_state) * (AVM_LED_STATE_TABLE_SIZE + 1));
    else
        memcpy(virt_led->led_states, avm_led_states[0], sizeof(struct _avm_led_state) * (AVM_LED_STATE_TABLE_SIZE + 1));

    /*--- hardware treiber setzen ---*/
    virt_led->driver = &gpio_driver[driver_type];
    DEB_NOTE("lowlevel-driver name: %s\n", virt_led->driver->name);

    /*--- spezialbehandlung fuer dual color leds ---*/
    if(virt_led->driver->get_flags) {
        if((*virt_led->driver->get_flags)() & AVM_LED_DRIVER_FLAGS_DISABLE_INSTANCES) {
            if(virt_led->instance_handle)
                avm_led_free_instance(virt_led->instance_handle);
            virt_led->instance_handle = NULL;
        }
    }

    /*--- hardware treiber initialisieren ---*/
    if(virt_led->driver && virt_led->driver->init) {
        virt_led->driver_handle = virt_led->driver->init(gpio, pin_pos, pin_name);
    }

    /*--- mode treiber setzen ---*/
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
    if(!strncmp(virt_led_name, "profile", 7)) {
        virt_led->mode = &mode_driver[1], printk("[avm_led] profile LED, use mode_driver[1]\n");
    } else
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
        virt_led->mode = &mode_driver[0];

    /*--- mode treiber fuer alle modi initialisieren ---*/
    for(i = 0 ; i < AVM_LED_MAX_MODE_DRIVER ; i++) {
        if(virt_led->mode->init) {
            virt_led->mode_handle[i] = (*virt_led->mode->init)(i, virt_led);
        } else  {
            virt_led->mode_handle[i] = 0;
        }
    }
    
    for(i = 0 ; i < (AVM_LED_STATE_TABLE_SIZE  + 1) ; i++) {
        virt_led->led_states[i].virt_led = virt_led;
    }
    avm_led_add_to_list(&virt_led_list, (struct _avm_led_list_header *)virt_led);
    DEB_NOTE("[avm_led_register_led] registered, success\n");
    return (int)virt_led;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_AVM_LED_MODULE
void avm_led_release_all_leds(void) {
    struct _avm_virt_led *virt_led = avm_led_get_first_virt_led();
    DEB_NOTE("[avm_led]: release all leds\n");
    while(virt_led) {
        avm_led_release_led((int)virt_led);
        virt_led = avm_led_get_next_virt_led(virt_led);
    }
}
#endif /*--- #ifdef CONFIG_AVM_LED_MODULE ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_virt_led *avm_led_get_first_virt_led(void) {
    return (struct _avm_virt_led *)virt_led_list.first;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_virt_led *avm_led_get_next_virt_led(struct _avm_virt_led *V) {
    return (struct _avm_virt_led *)V->list.next;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_map_state *avm_led_get_first_mapped_state(struct _avm_virt_led *V) {
    return (struct _avm_led_map_state *)V->map_states.first;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_map_state *avm_led_get_next_mapped_state(struct _avm_led_map_state *M) {
    return (struct _avm_led_map_state *)M->list.next;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_release_led(int handle) {
    unsigned int i;
    struct _avm_virt_led *virt_led = (struct _avm_virt_led *)handle;

    if(handle == 0)
        return;

    if(avm_led_monitor_level & AVM_LED_MONITOR_RELEASE)
        printk("[avm_led] UNDEF %s,%u\n", virt_led->name, virt_led->instance);

    avm_led_notify_unvalid_handle(handle);

    avm_led_del_all_remaped_states(handle); /*--- alle remaped states freigeben ---*/

    if(virt_led->mode->stop && virt_led->current_state &&  virt_led->mode_handle[virt_led->current_state->mode_index])
        (*virt_led->mode->stop)(virt_led->mode_handle[virt_led->current_state->mode_index]);

    if(virt_led->instance_handle) {
        virt_led->instance_handle->V[virt_led->instance] = NULL;
        avm_led_free_instance(virt_led->instance_handle);
    }

    avm_led_del_from_list(&virt_led_list, (struct _avm_led_list_header *)virt_led);

    if(virt_led->driver->exit)
        virt_led->driver->exit(virt_led->driver_handle);

    for(i = 0 ; i < AVM_LED_MAX_MODE_DRIVER ; i++) {
        if(virt_led->mode->exit && virt_led->mode_handle[i])
            (*virt_led->mode->exit)(virt_led->mode_handle[i]);
    }

    if(virt_led->led_states)
        kfree(virt_led->led_states);
    virt_led->event_length = 0;
    if(virt_led->event_ptr)
        kfree(virt_led->event_ptr);
    kfree(virt_led);
    DEB_NOTE("[avm_led_release_led] release, success\n");
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_map_led(int org_handle, int mapped_to_handle, int both) {
    struct _avm_virt_led *virt_led = (struct _avm_virt_led *)org_handle;

    if((virt_led->mapped_to_handle) && mapped_to_handle) {
        DEB_ERR("[avm_led_map_led] already map, unmap first\n");
        return -EACCES;
    }

    if(avm_led_monitor_level & AVM_LED_MONITOR_MAP) {
        struct _avm_virt_led *mapto_led = (struct _avm_virt_led *)mapped_to_handle;
        if(mapped_to_handle) {
            printk("[avm_led] %s %s,%u = %s,%u\n", 
                    both ? "DOUBLE" : "MAP",
                    virt_led->name, virt_led->instance,
                    mapto_led->name, mapto_led->instance);
        } else {
            printk("[avm_led] UNMAP %s,%u\n", 
                    virt_led->name, virt_led->instance);
        }
    }

    if((virt_led->mapped_to_handle) && (mapped_to_handle == 0)) {
        struct _avm_virt_led *V = (struct _avm_virt_led *)virt_led->mapped_to_handle;
        if(V->mode && V->mode->stop && V->current_state && V->mode_handle[V->current_state->mode_index]) {
            (*V->mode->stop)(V->mode_handle[V->current_state->mode_index]);
        }
        virt_led->mapped_to_handle = 0;
        virt_led->mapped_both      = 0;
        return 0;
    }
    virt_led->mapped_to_handle = mapped_to_handle;
    virt_led->mapped_both      = both;
    if(mapped_to_handle) {
        unsigned int i;
        if(virt_led->current_state) {
            i = virt_led->current_state->index;
            avm_led_virt_led_action(mapped_to_handle, virt_led->current_state->index);
        } else {
            avm_led_virt_led_action(mapped_to_handle, virt_led->current_index);
            i = virt_led->current_index;
        }
        if(avm_led_monitor_level & AVM_LED_MONITOR_MAP) {
            printk("[avm_led] SET %s,%u = %u\n",
                avm_led_virt_led_name(mapped_to_handle), 
                avm_led_virt_led_instance(mapped_to_handle),
                i
            );
        }
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_get_virt_led_handle(char *name, unsigned int instance) {
    struct _avm_virt_led *virt_led = avm_led_get_first_virt_led();
    DEB_NOTE("[avm_led_get_virt_led_handle]: lookup \"%s\" %u \n", name, instance);
    while(virt_led) {
        if((!strcmp(virt_led->name, name)) && (virt_led->instance == instance)) {
            return (int)virt_led;
        }
        virt_led = avm_led_get_next_virt_led(virt_led);
    }
    DEB_NOTE("[avm_led_get_next_virt_led]nothing found\n");
    return -ENXIO;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_show_virt_led(int handle) {
    /*--- unsigned int i; ---*/
    struct _avm_virt_led *virt_led = (struct _avm_virt_led *)handle;
    if(handle == -1) {
        virt_led = avm_led_get_first_virt_led();
        while(virt_led) {
            avm_led_show_virt_led((int)virt_led);
            virt_led = avm_led_get_next_virt_led(virt_led);
        }
        return;
    }

    printk("[avm_led] \"%s\",%u ", virt_led->name, virt_led->instance);
    if(virt_led->mapped_to_handle) {
        printk("\tmapped to virtual led \"%s\",%u\n", 
            avm_led_virt_led_name(virt_led->mapped_to_handle), avm_led_virt_led_instance(virt_led->mapped_to_handle));
    }
    if(virt_led->current_state)
        printk("\tcurrent state: 0x%x (mode=%u param1=%u param2=%u)\n",
            virt_led->current_state->index,
            virt_led->current_state->mode_index,
            virt_led->current_state->param1,
            virt_led->current_state->param2);
    else if(virt_led->current_index)
        printk("\tcurrent state: 0x%x\n", virt_led->current_index);
    else
        printk("\tnot jet used\n");

    if(virt_led->led_states == NULL) {
        printk("\tincredible virtual led (without states, modes, and driver)\n");
    }
    if(virt_led->driver) {
        /*--- printk("\tvirtual led low level driver: 0x%p\n", virt_led->driver); ---*/
        /*--- printk("\tvirtual led low level driver handle: 0x%x\n", virt_led->driver_handle); ---*/

        if(virt_led->driver->show)
            (*virt_led->driver->show)(virt_led->driver_handle, NULL);

        /*--- printk("\tvirtual led mode driver: 0x%p\n", virt_led->mode); ---*/
        /*--- for(i = 0 ; i < AVM_LED_MAX_MODE_DRIVER ; i++) { ---*/
            /*--- printk("\tvirtual led mode driver handle[%u] : 0x%x\n", i, virt_led->mode_handle[i]); ---*/
        /*--- } ---*/
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_led_virt_led_name(int handle) {
    struct _avm_virt_led *V = (struct _avm_virt_led *)handle;
    return V->name;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_virt_led_instance(int handle) {
    struct _avm_virt_led *V = (struct _avm_virt_led *)handle;
    return V->instance;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avm_led_push_stack(struct _avm_led_state *stack[], unsigned int *privious_state_stack_pointer, struct _avm_led_state *element) {

    /*--- printk("[avm_led] push/start: privious_state_stack_pointer=%x\n", (*privious_state_stack_pointer)); ---*/
    stack[*privious_state_stack_pointer] = element;
    (*privious_state_stack_pointer)++;

    if(*privious_state_stack_pointer >= AVM_LED_PRIVIOUS_STACK_SIZE) {
        (*privious_state_stack_pointer) = AVM_LED_PRIVIOUS_STACK_SIZE - 1;
        memmove(&stack[0], &stack[1], (AVM_LED_PRIVIOUS_STACK_SIZE - 1) * sizeof(struct _avm_led_state *));
    }
    /*--- printk("[avm_led] push/end: privious_state_stack_pointer=%x\n", (*privious_state_stack_pointer)); ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct _avm_led_state *avm_led_pop_stack(struct _avm_led_state *stack[], unsigned int *privious_state_stack_pointer) {
    struct _avm_led_state *S;
    /*--- printk("[avm_led] pop/start: privious_state_stack_pointer=%x\n", (*privious_state_stack_pointer)); ---*/
    if(*privious_state_stack_pointer == 0)
        S = NULL;
    else
        S = stack[--(*privious_state_stack_pointer)];

    /*--- printk("[avm_led] pop/start: privious_state_stack_pointer=%x\n", (*privious_state_stack_pointer)); ---*/
    return S;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_map_states(int handle, unsigned int from_state, unsigned int to_state) {
    struct _avm_virt_led *V = (struct _avm_virt_led *)handle;
    struct _avm_led_map_state *M = (struct _avm_led_map_state *)kmalloc(sizeof(struct _avm_led_map_state), GFP_ATOMIC);
    if(M == NULL) {
        DEB_ERR("[avm_led] no memory\n");
        return -ENOMEM;
    }
    memset(M, 0, sizeof(*M));
    M->from_state = from_state;
    M->to_state   = to_state;
    if(avm_led_monitor_level & AVM_LED_MONITOR_MAP_STATE)
        printk("[avm_led] ADD MAPPED STATE(%s,%u): %u->%u\n", V->name, V->instance, M->from_state, M->to_state);
    avm_led_add_to_list(&(V->map_states), (struct _avm_led_list_header *)M);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_map_state *avm_led_find_remaped_state(int handle, unsigned int state) {
    struct _avm_led_map_state *M;
    struct _avm_virt_led *V = (struct _avm_virt_led *)handle;
    for(M = avm_led_get_first_mapped_state(V) ; M && M->from_state != state ; ) {
        M = avm_led_get_next_mapped_state(M);
    }
    return M;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_del_all_remaped_states(int handle) {
    struct _avm_led_map_state *M;
    struct _avm_virt_led *V = (struct _avm_virt_led *)handle;
    for( ;; ) {
        M = avm_led_get_first_mapped_state(V);
        if(M == NULL)
            break;
        avm_led_del_from_list(&(V->map_states), (struct _avm_led_list_header *)M);
        if(avm_led_monitor_level & AVM_LED_MONITOR_MAP_STATE)
            printk("[avm_led] DEL MAPPED STATE(%s,%u): %u->%u\n", V->name, V->instance, M->from_state, M->to_state);
        kfree(M);
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_virt_led_ctrl(int handle, unsigned int master, unsigned int enable) {
    struct _avm_virt_led *V = (struct _avm_virt_led *)handle;
    unsigned int i;
    if(V && V->mapped_to_handle) {
        avm_led_virt_led_ctrl(V->mapped_to_handle, master, enable);
        if(V->mapped_both == 0)
            return;
    }

    if(V && V->instance_handle) {
        if(avm_led_monitor_level & AVM_LED_MONITOR_CTRL) 
            printk("[avm_led] %s%s %s,%u\n", master ? "MASTER_" : "", enable ? "ENABLE" : "DISABLE", V->name, V->instance);

        if(master && !enable) {
            DEB_NOTE("[avm_led] master = yes, enable = no\n");
            for(i = 0 ; i < AVM_LED_MAX_INSTANCE ; i++) {  /*--- alle auf enable ---*/
                DEB_NOTE("V->instance_handle->enable[%u] = 0xFFFFFFFF;\n", i);
                V->instance_handle->enable[i] = 0xFFFFFFFF;
            }
        } else if(master && enable) {
            DEB_NOTE("[avm_led] master = yes, enable = yes\n");
            for(i = 0 ; i < AVM_LED_MAX_INSTANCE ; i++) { /*--- alle auf disable ---*/
                DEB_NOTE("V->instance_handle->enable[%u] = 0;\n", i);
                V->instance_handle->enable[i] = 0;
            }
            V->instance_handle->enable[V->instance] = 0xFFFFFFFF;  /*--- nur 'ich' auf enable ---*/
        } else {
            DEB_NOTE("[avm_led] master = no, enable = %u\n", enable ? "yes" : "no");
            V->instance_handle->enable[V->instance] = enable ? 0xFFFFFFFF : 0;
            DEB_NOTE("V->instance_handle->enable[%u] = 0x%x;\n", V->instance, enable ? 0xFFFFFFFF : 0);
        }
    }
    if(V && V->mode && V->mode->sync) {
        V->mode->sync((unsigned int)V);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_virt_led_action(int handle, unsigned int state) {
    struct _avm_virt_led *V = (struct _avm_virt_led *)handle;
    struct _avm_led_state *S;
    struct _avm_led_map_state *M;

    M = avm_led_find_remaped_state(handle, state);
    if(M) {
        if(avm_led_monitor_level & (AVM_LED_MONITOR_ACTION | AVM_LED_MONITOR_MAP_STATE))
            printk("[avm_led] use REMAP %s,%u state: %u -> %u \n", V->name, V->instance, M->from_state, M->to_state);
        state = M->to_state;
    }

    DEB_NOTE("[avm_led_virt_led_action] handle=0x%x (%s,%u) state=%u\n", handle, V->name, V->instance, state);

    if(state > avm_led_last_state) {
        DEB_ERR("[avm_led] illegal state %u\n", state);
        return;
    }

    V->current_index = state;

    if(V->mapped_to_handle) {
        avm_led_virt_led_action(V->mapped_to_handle, state);
        if(V->mapped_both == 0)
            return;
    }

    if(avm_led_monitor_level & AVM_LED_MONITOR_ACTION) {
        if(state == avm_led_previous)
            printk("[avm_led] SET %s,%u = %s\n", V->name, V->instance, "avm_led_previous");
        else
            printk("[avm_led] SET %s,%u = %u\n", V->name, V->instance, state);
    }

    if(V->led_states == NULL) {
        DEB_NOTE("[avm_led] ignore any action on incredible virtual led\n");
        return;
    }

    S = &(V->led_states[state]);
    if(S == NULL) {
        DEB_NOTE("[avm_led] ignore any action on incredible virtual led\n");
        return;
    }

    if(S->mode_index == AVM_LED_GOTO_PRIVIOUS_STATE) {
        S = avm_led_pop_stack(V->privious_state_stack, &(V->privious_state_stack_pointer));
        if(S == 0) {
            printk("[avm_led] no previous state: SET %s,%u = 0\n", V->name, V->instance);
            S = &(V->led_states[0]);
        }
    } else if(V->current_state) {
        avm_led_push_stack(V->privious_state_stack, &(V->privious_state_stack_pointer), V->current_state);
    }

    if(V->mode->stop && V->current_state && V->mode_handle[V->current_state->mode_index])
        (*V->mode->stop)(V->mode_handle[V->current_state->mode_index]);

    V->current_state = S;  /* fuer das naechste stop immer merken */

    if(V->mode->action &&  V->mode_handle[S->mode_index])
        (*V->mode->action)(V->mode_handle[S->mode_index], S->param1, S->param2);

    if(V->driver->sync)
        (*V->driver->sync)(V->driver_handle, state);

#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)
    if(V->current_state)
        avm_led_info_event(V);
#endif /*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) ---*/

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_instance_handle *avm_led_alloc_instance(char *name) {
    struct _avm_virt_led *V;
    struct _avm_led_instance_handle *I;
    int i;

    for(i = 0, V = NULL ; i < AVM_LED_MAX_INSTANCE ; i++) {
        V = (struct _avm_virt_led *)avm_led_get_virt_led_handle(name, i);
        if(((int) V > 0) || ((int) V < -255)) {
            break;
        } else {
            V = NULL;
        }
    }
    if(V) {
        if(V->instance_handle == NULL) {
            DEB_ERR("[avm_led]:INTERNAL ERROR !!  %s,%u\n", __FILE__, __LINE__);
        } else {
            atomic_inc(&(V->instance_handle->link_count));
            return V->instance_handle;
        }
    }

    I = (struct _avm_led_instance_handle *)kmalloc(sizeof(struct _avm_led_instance_handle), GFP_ATOMIC);
    if(I == NULL)
        return NULL;

    memset(I, 0, sizeof(struct _avm_led_instance_handle));
    for(i = 0 ; i < AVM_LED_MAX_INSTANCE ; i++) {
        I->enable[i] = 0xFFFFFFFF;
    }
    atomic_set(&(I->link_count), 1);
    return I;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_free_instance(struct _avm_led_instance_handle *I) {
    atomic_dec(&(I->link_count));
    if(atomic_read(&(I->link_count)) == 0) {
        kfree((void *)I);
        return 0;
    }
    return 0;
}


