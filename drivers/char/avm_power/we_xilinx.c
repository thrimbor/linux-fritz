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

#if LINUX_VERSION_CODE == KERNEL_VERSION(2,4,17)
# include <asm/avalanche/avalanche_map.h>
# include <asm/avalanche/generic/avalanche_misc.h>
# include <asm/avalanche/sangam/sangam.h>
# include <asm/avalanche/sangam/hw_boot.h>
# include <asm/avalanche/sangam/hw_emif.h>
# include <asm/avalanche/sangam/hw_gpio.h>
#  define avm_gpio_ctrl(pin, mode, dir)	avalanche_gpio_ctrl(pin, mode, dir)
#  define avm_gpio_out_bit(pin, value)	avalanche_gpio_out_bit(pin, value)
#else
# if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,13)
#if 0
#if defined(CONFIG_MIPS_OHIO)
#  define AVALANCHE_EMIF_CONTROL_BASE 		    OHIO_EMIF_BASE
#  define AVALANCHE_DEVICE_CONFIG_LATCH_BASE 	OHIO_DEVICE_CONFIG_BASE
#elif defined(CONFIG_MIPS_AR7)/*--- #if defined(CONFIG_MIPS_OHIO) ---*/
#  define AVALANCHE_EMIF_CONTROL_BASE 		    AR7_EMIF_BASE
#  define AVALANCHE_DEVICE_CONFIG_LATCH_BASE 	AR7_DEVICE_CONFIG_BASE
#endif/*--- #else ---*//*--- #if defined(CONFIG_MIPS_OHIO) ---*/
#endif
#  include <asm/mach_avm.h>
/*--- #  define avalanche_gpio_ctrl(pin, mode, dir)	ohio_gpio_ctrl(pin, mode, dir) ---*/
/*--- #  define avalanche_gpio_out_bit(pin, value)	ohio_gpio_out_bit(pin, value) ---*/
/*--- #  include <asm/mach-ohio/hw_boot.h> ---*/
/*--- #  include <asm/mach-ohio/hw_emif.h> ---*/
/*--- #  include <asm/mach-ohio/hw_gpio.h> ---*/
# else
#  error "we_xilinx: Unknown kernel version!"
# endif
#endif

#include "wyatt_earp.h"

#define	DEFAULT_XILINX_RESET_BIT		     2
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int SuspendXILINX(int state){
    int xilinx_reset_bit = DEFAULT_XILINX_RESET_BIT;
    int Start = 1, bit;

    if(xilinx_reset_bit < 0) {
        bit = -xilinx_reset_bit;
        Start = !Start;
    } else {
        bit = xilinx_reset_bit;
    }

    avm_gpio_ctrl(bit, GPIO_PIN, GPIO_OUTPUT_PIN);
    avm_gpio_out_bit(bit, Start);
    state = state;
    DBG_TRC(("Suspend XILINX\n"));
    return 0;
}
#endif/*--- #if !defined(CONFIG_MIPS_UR8) ---*/
