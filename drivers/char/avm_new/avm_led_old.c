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
#include <asm/errno.h>

#include <linux/avm_led.h>
#include "avm_sammel.h"
#include "avm_led.h"


#if defined(CONFIG_MIPS_AVALANCHE_LED) || defined(CONFIG_MIPS_AVALANCHE_LED_MODULE)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define MAX_OLD_STATES      16
#define MAX_OLD_LEDS        21

struct _avm_led_old_mapping {
    int handle;
    char *name;
    int instance;
    struct _avm_led_old_state {
        unsigned int mapped;
        enum _avm_led_modi state_nr;
    } old_states[MAX_OLD_STATES];
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_old_mapping avm_led_old_mapping[MAX_OLD_LEDS] = {
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.power:module = adsl, 0 : 1, adsl 
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 0 ---*/
        name:   "adsl", 
        instance: 0,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(2) },
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.dsl:module = pppoe, 0 : 2, ppp   
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 1 ---*/
        name:   "pppoe",
        instance: 0,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_4Hz },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.lan:module = cpmac, 0 : 3, lan 
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 2 ---*/
        name:   "cpmac", 
        instance: 0,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.wlan:module = wlan, 0 : 4, wifi 
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 3 ---*/
        name:   "wlan",
        instance: 0,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_0_5Hz },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.usb:module = usb, 0 : 5, usb 
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 4 ---*/
        name:   "usb", 
        instance: 0,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_flash_off_125ms },
            { mapped: 1, state_nr: avm_led_flash_off_125ms },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.usb:module = usb, 1 : 6, usb
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 5 ---*/
        name:   "usb", 
        instance: 1,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_flash_off_125ms },
            { mapped: 1, state_nr: avm_led_flash_off_125ms },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.info:module = info, 0 : 7, info 
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 6 ---*/
        name:   "info",
        instance: 0,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.power:module = adsl, 1 : 8, power
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 7 ---*/
        name:   "adsl", 
        instance: 1,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_0_5Hz },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     *    ................... 9
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 8 ---*/
        name:   "info",
        instance: 0,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 0, state_nr: 0 },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.isdn:module = isdn, 1 : 10, isdn-b1
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 9 ---*/
        name:   "isdn",
        instance: 1,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_0_5Hz },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.isdn:module = isdn, 2 : 11, isdn-b2
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 10 ---*/
        name:   "isdn",
        instance: 2,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_0_5Hz },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.isdn:module = isdn, 0 : 12, isdn-d
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 11 ---*/
        name:   "isdn",
        instance: 0,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_0_5Hz },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.festnetz:module = ab, 1 : 13, pots 
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 12 ---*/
        name:   "ab",
        instance: 1,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_0_5Hz },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.internet:module = internet, 0 : 14, sip            
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 13 ---*/
        name:   "internet",
        instance: 0,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_0_5Hz },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.lan:module = cpmac, 1 : 15, lan1
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 14 ---*/
        name:   "cpmac", 
        instance: 1,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.info:module = info, 2 : 16, freecall
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 15 ---*/
        name:   "info",
        instance: 2,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_4Hz },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(2) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(3) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(4) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(5) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(6) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(7) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(8) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(9) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(10) },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.error:module = error, 0 : 17, error
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 16 ---*/
        name:   "error",
        instance: 0,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },  
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(1) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(2) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(3) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(4) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(5) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(6) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(7) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(8) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(9) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(10) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(11) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(12) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(13) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(14) }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.info:module = info, 1 : 18, traffic
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 17 ---*/
        name:   "info",
        instance: 1,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_4Hz },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(2) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(3) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(4) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(5) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(6) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(7) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(8) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(9) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(10) },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.info:module = info, 3 : 19, avmusbwlan
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 18 ---*/
        name:   "info",
        instance: 3,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.internet:module = internet, 1 : 20, mwi
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 19 ---*/
        name:   "internet",
        instance: 1,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_4Hz },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(1) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(2) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(3) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_BLINK_CODE_2Hz(1) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_BLINK_CODE_2Hz(2) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_BLINK_CODE_2Hz(3) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_MIXED_BLINK_CODE_2Hz(1) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_MIXED_BLINK_CODE_2Hz(2) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_MIXED_BLINK_CODE_2Hz(3) },
            { mapped: 0, state_nr: 0 }
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * arch/led-cfg/led.festnetz:module = ab, 2 : 21, fest_mwi
    \*--------------------------------------------------------------------------------------*/
    {   /*--- index 20 ---*/
        name:   "ab",
        instance: 2,
        old_states: {
            { mapped: 1, state_nr: avm_led_off }, 
            { mapped: 1, state_nr: avm_led_on },
            { mapped: 1, state_nr: avm_led_symmetric_blink_2Hz },
            { mapped: 1, state_nr: avm_led_symmetric_blink_4Hz },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(1) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(2) },
            { mapped: 1, state_nr: AVM_LED_BLINK_CODE_2Hz(3) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_BLINK_CODE_2Hz(1) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_BLINK_CODE_2Hz(2) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_BLINK_CODE_2Hz(3) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_MIXED_BLINK_CODE_2Hz(1) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_MIXED_BLINK_CODE_2Hz(2) },
            { mapped: 1, state_nr: AVM_LED_DOUBLE_MIXED_BLINK_CODE_2Hz(3) },
            { mapped: 0, state_nr: 0 }
        }
    }
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void *avalanche_led_register (const char *module_name, int instance_num) {
    int handle;
    handle = avm_led_get_virt_led_handle((char *)module_name, instance_num);
    if((handle <= 0) && (handle > -255))
        return NULL;
    return (void *)handle;
}
EXPORT_SYMBOL(avalanche_led_register);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avalanche_led_init(void) {
    int tmp_handle;
    int i;
    static int once = 1;
    if(!once)
        return;

    once = 0;
    for(i = 0 ; i < MAX_OLD_LEDS ; i++) {
        tmp_handle = avm_led_get_virt_led_handle( avm_led_old_mapping[i].name, avm_led_old_mapping[i].instance);
        if((tmp_handle <= 0) && (tmp_handle > -255)) {
            DEB_ERR("[avm_old_led] once: can't setup %s,%u\n", avm_led_old_mapping[i].name, avm_led_old_mapping[i].instance);
            tmp_handle = 0;
        }
        avm_led_old_mapping[i].handle = tmp_handle;
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avalanche_led_get_handle_from_old_id(int old_handle) {
    DEB_NOTE("[avm_old_led]: search new handle, old handle = %u\n", old_handle);
    avalanche_led_init();
    if((old_handle < 1) || (old_handle > MAX_OLD_LEDS)) {
        DEB_ERR("[avm_old_led]: search new handle failed, old handle = 0x%u\n", old_handle);
        return 0;
    }
    return avm_led_old_mapping[old_handle - 1].handle;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avalanche_led_get_old_id_from_handle(int handle) {
    int old_id = MAX_OLD_LEDS;

    while(old_id) {
        if(handle == avm_led_old_mapping[old_id - 1].handle)
            return old_id;
        old_id--;
    }
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void  avalanche_led_action (void *handle, int state_id) {
    int i;
    avalanche_led_init();

    if(handle == NULL) {
        DEB_WARN("[avm_old_led] avalanche_led_action got a NULL handle!\n");
        return;
    }

    for(i = 0 ; i < MAX_OLD_LEDS ; i++) {
        if(avm_led_old_mapping[i].handle == (int)handle) {
            if((state_id < 0) || (state_id >= MAX_OLD_STATES)) {
                DEB_WARN("[avm_old_led]: %s,%u no state map entry for state %u\n", 
                    avm_led_old_mapping[i].name,
                    avm_led_old_mapping[i].instance,
                    state_id);
                return;
            }

            if(avm_led_old_mapping[i].old_states[state_id].mapped != 1) {
                if(avm_led_old_mapping[i].old_states[state_id].mapped == 1) {
                    DEB_WARN("[avm_old_led]: %s,%u state %u is not mapped\n", 
                        avm_led_old_mapping[i].name,
                        avm_led_old_mapping[i].instance,
                        state_id);
                }
                avm_led_old_mapping[i].old_states[state_id].mapped = 2; /* mapped and displayed */
                return;
            }

            DEB_NOTE("[avm_old_led]: %s,%u map %u --> %u\n", 
                avm_led_old_mapping[i].name,
                avm_led_old_mapping[i].instance,
                state_id,
                avm_led_old_mapping[i].old_states[state_id].state_nr);

            avm_led_virt_led_action((int)handle, avm_led_old_mapping[i].old_states[state_id].state_nr);
        }
    }
    DEB_WARN("[avm_old_led]: no led map entry for %s,%u (state %u)\n", 
        ((struct _avm_virt_led *) handle)->name, ((struct _avm_virt_led *) handle)->instance, state_id);
}
EXPORT_SYMBOL(avalanche_led_action);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avalanche_led_unregister(void *handle) {
    return 0;
}
EXPORT_SYMBOL(avalanche_led_unregister);




#endif /*--- #if defined(CONFIG_MIPS_AVALANCHE_LED) || defined(CONFIG_MIPS_AVALANCHE_LED_MODULE) ---*/

