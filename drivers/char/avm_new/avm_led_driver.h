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
#ifndef _AVM_LED_DRIVER_H_
#define _AVM_LED_DRIVER_H_

#include <linux/version.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mach_avm.h>
#else
#include <asm/avalanche/avalanche_map.h>
#include <asm/avalanche/sangam/hw_gpio.h>
#define avm_gpio_ctrl(gpio_bit, function, dir)      avalanche_gpio_ctrl(gpio_bit, function, dir)
#define avm_gpio_out_bit(gpio_bit, on)              avalanche_gpio_out_bit(gpio_bit, on)
#define avm_gpio_in_bit(gpio_bit)                   avalanche_gpio_in_bit(gpio_bit)
#define avm_gpio_set_bitmask(gpio_mask, value)      avalanche_gpio_out_value(value, gpio_mask, 0)
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER
void avm_led_shift_register_load(unsigned int mask, unsigned int value);
void avm_led_init_hardware(void);
extern unsigned int avm_led_shift_register_contence;
#endif /*--- #ifdef CONFIG_AVM_LED_OUTPUT_SHIFT_REGISTER ---*/


#endif /*--- #ifndef _AVM_LED_DRIVER_H_ ---*/
