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

#include <linux/autoconf.h>
#if !defined(CONFIG_MIPS_UR8)
#include <linux/kernel.h>
#include <linux/version.h>
#include "wyatt_earp.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mach_avm.h>
#else
#include <asm/avalanche/avalanche_map.h>
#include <asm/avalanche/sangam/hw_gpio.h>
#define avm_gpio_set_bitmask(gpio_mask, value)      avalanche_gpio_out_value(value, gpio_mask, 0)
#endif

/*-------------------------------------------------------------------------------------*\
 * 7170: 5 GPIO's: Power = 7 Internet = 13 Fest = 12 WLAN = 10 INFO = 9 
 * W701V: 12 GPIO's, Power=7 Rest(?):  Ansteuerung ueber Schieberegister 74595
\*-------------------------------------------------------------------------------------*/
int SuspendLEDs(int state){
    unsigned int LedMask, LedReverseMask;
    LedMask =   0 |
                (1 << 7)  |
                /*--- (1 << 13) | ---*/
                /*--- (1 << 12) | ---*/
                /*--- (1 << 10) | ---*/
                /*--- (1 << 9)  | ---*/
                0;
    LedReverseMask = ~(1 << 7) ;    /*--- Bit 7 ist highaktiv ---*/
    DBG_WE_TRC(("Suspend LED's\n"));
    avm_gpio_set_bitmask(LedMask, (LedMask & LedReverseMask)); /*--- alle Leds aus ---*/
#if defined(CONFIG_MIPS_OHIO)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    /*--- W701V ---*/
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

    /*--- avm_led_shift_register_contence &= ~mask; ---*/
    /*--- avm_led_shift_register_contence |= mask & values; ---*/

    R = (union _shift_register_bits *)&(G->OutputData);
    
    R->shift_clock.bit = 0;
    R->store_clock.bit = 0;
    R->master_reset.bit = 0; /*--- reset ---*/
    R->master_reset.bit = 1; /*--- non reset ---*/

    for(i = 8 ; i ; i-- ) {
        R->shift_clock.bit = 0;
        R->shift_data.bit  = 1;
        R->shift_clock.bit = 1;
    }
    R->store_clock.bit = 1;
    R->store_clock.bit = 0;
    R->shift_clock.bit = 0;
#endif/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/
    }
#endif/*--- #if defined(CONFIG_MIPS_OHIO) ---*/
    state = state;
    return 0;
}
#endif/*--- #if !defined(CONFIG_MIPS_UR8) ---*/
