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
 * In dieser Datei werden die moeglichen Blinkmodi definiert bzw. erzeugt.                  *
\*------------------------------------------------------------------------------------------*/

#include <linux/version.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/avm_led.h>
#include "avm_sammel.h"
#include "avm_led.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mach_avm.h>
#else
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _led_time_step {
#define BRIGHTNESS(lux, optional_virtled) ((lux == 0 ? 0 :       \
                                           lux == 1 ? 0x1 :      \
                                           lux == 2 ? 0x3 :      \
                                           lux == 3 ? 0x7 : 0xF) << (optional_virtled * 4))
#define BRIGHTNESS_LIST(luxv0, luxv1, luxv2, luxv3, luxv4) \
                                                BRIGHTNESS(luxv0, 0) | \
                                                BRIGHTNESS(luxv1, 1) | \
                                                BRIGHTNESS(luxv2, 2) | \
                                                BRIGHTNESS(luxv3, 3) | \
                                                BRIGHTNESS(luxv4, 4) 
    unsigned int on;
#define TIME_STEP_FLAG_TIMER                0x00
#define TIME_STEP_FLAG_REFRESH_TIMER        0x01
#define TIME_STEP_FLAG_USE_PARAM1           0x02
#define TIME_STEP_FLAG_USE_PARAM2           0x04
#define TIME_STEP_FLAG_DEC_PARAM2           0x08
#define TIME_STEP_FLAG_RESTART              0x10
#define TIME_STEP_FLAG_RESTART_IF_PARAM2    0x20
#define TIME_STEP_FLAG_RESTART_RELOAD       0x40
#define TIME_STEP_FLAG_END                  0x80
#define TIME_STEP_PARAM1_AS_ON             0x100
    unsigned short flag;
    unsigned char mult_factor;
    unsigned char div_factor;
};


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _led_time_step led_mode_off[] = {
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_END }
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _led_time_step led_mode_on[] = {
    { on: BRIGHTNESS(4,0), flag: TIME_STEP_FLAG_RESTART | TIME_STEP_FLAG_REFRESH_TIMER | TIME_STEP_FLAG_END, mult_factor: 255, div_factor: 1  }
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _led_time_step led_mode_flash_on[] = {
    { on: BRIGHTNESS(4,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 1 },
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_END }
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _led_time_step led_mode_flash_off[] = {
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 1 },
    { on: BRIGHTNESS(4,0), flag: TIME_STEP_FLAG_END }
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _led_time_step led_mode_symmetric_blink[] = {
    { on: BRIGHTNESS(4,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 1 },
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_RESTART | TIME_STEP_FLAG_USE_PARAM2 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 1 }
};

/*------------------------------------------------------------------------------------------*\
 *   count = 2
 *    XXXX____XXXX________________XXXX____XXXX________________XXXX____XXXX____________
 *    |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
\*------------------------------------------------------------------------------------------*/
struct _led_time_step led_mode_blink_code[] = {
    { on: BRIGHTNESS(4,0), flag: TIME_STEP_FLAG_DEC_PARAM2 | TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 1 },
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_RESTART_IF_PARAM2 | TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 1 },
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_RESTART_RELOAD | TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 4, div_factor: 1 }
};

/*------------------------------------------------------------------------------------------*\
 *   count = 2
 *    abcde   abcde    f          qbcde   abcde    f
 *    _X_X_____X_X_________________X_X_____X_X_________________X_X_____X_X____________
 *    |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
\*------------------------------------------------------------------------------------------*/
struct _led_time_step led_mode_double_blink_code[] = {
    /*--- (a) 1/4 param1 aus, decrement param2 ---*/
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_DEC_PARAM2 | TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 4 },
    /*--- (b) 1/4 param1 ein ---*/
    { on: BRIGHTNESS(4,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 4 },
    /*--- (c) 1/4 param1 aus ---*/
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 4 },
    /*--- (d) 1/4 param1 ein ---*/
    { on: BRIGHTNESS(4,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 4 },
    /*--- (e) 1 param1 aus ---*/
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_RESTART_IF_PARAM2 | TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 1 },
    /*--- (f) 1 param1 aus ---*/
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_RESTART_RELOAD | TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 3, div_factor: 1 }
};

/*------------------------------------------------------------------------------------------*\
 *   count = 2
 *    a   b   cdefg   a   b   cdefg   h 
 *    XXXX_____X_X____XXXX_____X_X____________________XXXX_____X_X____XXXX_____X_X_
 *    |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
\*------------------------------------------------------------------------------------------*/
struct _led_time_step led_mode_mixed_blink_code[] = {
    /*--- (a) 1 param1 ein ---*/
    { on: BRIGHTNESS(4,0), flag: TIME_STEP_FLAG_DEC_PARAM2 | TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 1 },
    /*--- (b) 1 param1 aus ---*/
    { on: BRIGHTNESS(0,0), flag:                             TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 1 },
    /*--- (c) 1/4 param1 aus ---*/
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 4 },
    /*--- (d) 1/4 param1 ein ---*/
    { on: BRIGHTNESS(4,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 4 },
    /*--- (e) 1/4 param1 aus ---*/
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 4 },
    /*--- (f) 1/4 param1 ein ---*/
    { on: BRIGHTNESS(4,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 4 },
    /*--- (g) 1 param1 aus ---*/
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_RESTART_IF_PARAM2 | TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 1 },
    /*--- (g) 5 param1 aus ---*/
    { on: BRIGHTNESS(0,0), flag: TIME_STEP_FLAG_RESTART_RELOAD | TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 4, div_factor: 1 }
};

#if defined(CONFIG_AVM_LED_BIER_HOLEN)
/*------------------------------------------------------------------------------------------*\
 * bier holen effects
\*------------------------------------------------------------------------------------------*/
struct _led_time_step led_mode_bier_holen_code[] = {
    { on: BRIGHTNESS_LIST(1,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(2,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(3,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(4,1,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,2,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,3,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(4,4,1,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,2,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,3,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(4,4,4,1,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,2,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,3,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,4,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(4,4,4,4,1), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,4,2), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,4,3), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(3,4,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(2,4,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(1,4,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,4,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(0,3,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,2,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,1,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(0,0,3,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,2,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,1,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(0,0,0,3,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,2,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,1,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,0,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(0,0,0,0,3), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,0,2), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,0,1), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(0,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,0,1), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,0,2), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,0,3), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(0,0,0,0,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,1,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,2,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,3,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(0,0,0,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,1,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,2,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,3,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(0,0,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,1,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,2,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,3,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(0,4,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(1,4,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(2,4,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(3,4,4,4,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
  
    { on: BRIGHTNESS_LIST(4,4,4,4,3), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,4,2), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,4,1), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,4,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(4,4,4,3,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,2,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,1,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,4,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(4,4,3,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,2,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,1,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,4,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 

    { on: BRIGHTNESS_LIST(4,3,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,2,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,1,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(4,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    
    { on: BRIGHTNESS_LIST(3,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(2,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(1,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 80 }, 
    { on: BRIGHTNESS_LIST(0,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_RESTART_RELOAD, mult_factor: 1, div_factor: 80}
};

/*------------------------------------------------------------------------------------------*\
 * durchlaufende LED's mit "Schleimleuchtspur"
\*------------------------------------------------------------------------------------------*/
struct _led_time_step led_mode_walking_led[] = {
    { on: BRIGHTNESS_LIST(0,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 16 }, 

    { on: BRIGHTNESS_LIST(1,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(2,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(3,0,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(4,1,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(4,2,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 

    { on: BRIGHTNESS_LIST(3,3,0,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(2,4,1,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(1,4,2,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(0,3,3,0,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(0,2,4,1,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(0,1,4,2,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 

    { on: BRIGHTNESS_LIST(0,0,3,3,0), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(0,0,2,4,1), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(0,0,1,4,2), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(0,0,0,3,3), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(0,0,0,2,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(0,0,0,1,4), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 

    { on: BRIGHTNESS_LIST(0,0,0,0,3), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(0,0,0,0,2), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_TIMER, mult_factor: 1, div_factor: 21 }, 
    { on: BRIGHTNESS_LIST(0,0,0,0,1), flag: TIME_STEP_FLAG_USE_PARAM1 | TIME_STEP_FLAG_RESTART_RELOAD, mult_factor: 1, div_factor: 21 }, 
};

/*------------------------------------------------------------------------------------------*\
 * VU-Meter: 
\*------------------------------------------------------------------------------------------*/
struct _led_time_step led_mode_vu_meter[] = {
    { on: BRIGHTNESS_LIST(0,0,0,0,0), flag: TIME_STEP_PARAM1_AS_ON | TIME_STEP_FLAG_END, mult_factor: 1, div_factor: 1}, 
};
#endif/*--- #if defined(CONFIG_AVM_LED_BIER_HOLEN) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _led_mode_table {
    struct _led_time_step *led_mode;
} led_mode_table[AVM_LED_MAX_MODE_DRIVER] = {
    /*--- mode 0 ---*/ { led_mode_off },
    /*--- mode 1 ---*/ { led_mode_on },
    /*--- mode 2 ---*/ { led_mode_flash_on },
    /*--- mode 3 ---*/ { led_mode_flash_off },
    /*--- mode 4 ---*/ { led_mode_symmetric_blink },
    /*--- mode 5 ---*/ { led_mode_blink_code },
    /*--- mode 6 ---*/ { led_mode_double_blink_code },
    /*--- mode 7 ---*/ { led_mode_mixed_blink_code },
#if defined(CONFIG_AVM_LED_BIER_HOLEN)
    /*--- mode 8 ---*/ { led_mode_bier_holen_code },
    /*--- mode 9 ---*/ { led_mode_walking_led },
    /*--- mode 10 ---*/ { led_mode_vu_meter },
#endif/*--- #if defined(CONFIG_AVM_LED_BIER_HOLEN) ---*/
    { NULL }
};

struct _avm_led_mode {
    struct _led_time_step *step;
    unsigned int current_param1;
    unsigned int current_param2;
    unsigned int reload_param1;
    unsigned int reload_param2;
    unsigned int current_pos;
    struct timer_list timer;
    struct _avm_virt_led *virt_led;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_mode_init(unsigned int table_index, struct _avm_virt_led *virt_led) {
    struct _avm_led_mode *mode;
    mode = (struct _avm_led_mode *)kmalloc(sizeof(struct _avm_led_mode), GFP_ATOMIC);
    if(mode == NULL) {
        return 0;
    }
    if(table_index >= (sizeof(led_mode_table)/ sizeof(struct _led_mode_table) - 1)) {
        /*--- zu grosser table-Index: auf led_mode_off ---*/
        table_index = 0;
    }
    DEB_NOTE("[avm_led_mode_init] mode=%p table_index=%u %p\n", mode, table_index, led_mode_table[table_index].led_mode);
    mode->step            = led_mode_table[table_index].led_mode;
    mode->current_param1  = 0;
    mode->current_param2  = 0;
    mode->reload_param1   = 0;
    mode->reload_param2   = 0;
    mode->current_pos     = 0;
    mode->virt_led        = virt_led;

    init_timer(&(mode->timer));
    mode->timer.function = avm_led_mode_timer;
    mode->timer.data     = (int)mode;

    return (int)mode;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_mode_exit(int handle) {
    DEB_NOTE("[avm_led_mode_exit] mode=%x\n", handle);
    avm_led_mode_stop(handle);
    kfree((void *)handle);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avm_led_mode_output_to_driver(int handle, int on) {
    struct _avm_led_mode *mode = (struct _avm_led_mode *)handle;

    if(mode->virt_led && mode->virt_led->driver && mode->virt_led->driver->action) {
        unsigned int i, ignore = 0;
        struct _avm_led_instance_handle *I = mode->virt_led->instance_handle;
        if(I) {
            if(mode->virt_led->driver->get_flags) {
                unsigned int prio_instance = (*mode->virt_led->driver->get_flags)() & 0xF;
                if(mode->virt_led->instance_handle->V[prio_instance]) {
                    if(mode->virt_led->instance_handle->V[prio_instance]->current_state) {
                        if(mode->virt_led->instance_handle->V[prio_instance]->current_state->mode_index) {
                            /*--- printk("[prio] prio is %u current is %u ", prio_instance, mode->virt_led->instance); ---*/
                            if(prio_instance != mode->virt_led->instance) {
                                /*--- printk("ignore\n"); ---*/
                                ignore = 1;
                            } else {
                                /*--- printk("leave\n"); ---*/
                            }
                        }
                    }
                }
            } else {
                I->mask[mode->virt_led->instance] = on;
                for(i = 0, on = 0 ; i < AVM_LED_MAX_INSTANCE ; i++) {
                    on |= I->mask[i] & I->enable[i];
                }
            }
        }

        if(ignore == 0)
            (*mode->virt_led->driver->action)(mode->virt_led->driver_handle, on);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_mode_sync(int virt_led_handle) {
    struct _avm_virt_led *V = (struct _avm_virt_led *)virt_led_handle;

    if(V && V->driver && V->driver->action) {
        unsigned int i, on = 0;
        struct _avm_led_instance_handle *I = V->instance_handle;
        if(I) {
            for(i = 0 ; i < AVM_LED_MAX_INSTANCE ; i++) {
                on |= I->mask[i] & I->enable[i];
            }
        }
        (*V->driver->action)(V->driver_handle, on);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_mode_stop(int handle) {
    struct _avm_led_mode *mode = (struct _avm_led_mode *)handle;
    del_timer(&(mode->timer));
    avm_led_mode_output_to_driver(handle, 0);
    mode->current_pos = 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_mode_timer(unsigned long handle) {
    avm_led_mode_action(handle, 0, 0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_mode_action(int handle, unsigned int param1, unsigned int param2) {
    struct _avm_led_mode *mode = (struct _avm_led_mode *)handle;
    struct _led_time_step *step = &(mode->step[mode->current_pos]);
    unsigned int timer_value = 0;
    unsigned int timer_call = 1;

    if(param1) {
        mode->current_param1 = mode->reload_param1 = param1;
        timer_call = 0;
    }
    if(param2) {
        mode->current_param2 = mode->reload_param2 = param2;
    }

    if(timer_call == 0) {
        DEB_NOTE("[avm_led_mode_action] current_pos=%u param1=%u param2=%u\n", mode->current_pos, param1, param2);
    }

    /*--- DEB_NOTE("[avm_led_mode_action] mode=%p step=%p curpos: %u f=%x p1 %u p2 %u\n", mode, step, mode->current_pos, step->flag, mode->current_param1, mode->current_param2); ---*/
    if(step->flag & TIME_STEP_PARAM1_AS_ON) {
        /*--- printk("[avm_led_mode_output_to_driver] param1=0x%x\n", param1); ---*/
        avm_led_mode_output_to_driver(handle, param1);
    } else {
        avm_led_mode_output_to_driver(handle, step->on);
    }

    if(step->flag & TIME_STEP_FLAG_END) {
        if(step->flag & TIME_STEP_FLAG_REFRESH_TIMER) {
            timer_value = 1;
        } else {
            mode->current_pos = 0;
            DEB_NOTE("[avm_led_mode_action] end, no timer\n");
            return;
        }
    }

    if(step->flag & TIME_STEP_FLAG_USE_PARAM1)
        timer_value = mode->current_param1;
    else if(step->flag & TIME_STEP_FLAG_USE_PARAM2)
        timer_value = mode->current_param2;

    if(step->flag & TIME_STEP_FLAG_DEC_PARAM2)
        mode->current_param2--;

    timer_value = (timer_value * step->mult_factor) / step->div_factor;

    del_timer(&(mode->timer));
    mode->timer.expires = ((HZ * timer_value) / 1000) + jiffies;
    add_timer(&(mode->timer));

    if((step->flag & TIME_STEP_FLAG_RESTART_IF_PARAM2) && (mode->current_param2)) {
        DEB_NOTE("[avm_led_mode_action] restart (param2 != 0) with timer\n");
        mode->current_pos = 0;
        return;
    }

    if(step->flag & TIME_STEP_FLAG_RESTART_RELOAD) {
        mode->current_pos = 0;
        mode->current_param1 = mode->reload_param1;
        mode->current_param2 = mode->reload_param2;
        DEB_NOTE("[avm_led_mode_action] restart and reload with timer\n");
        return;
    }

    if(step->flag & TIME_STEP_FLAG_RESTART) {
        /*--- DEB_NOTE("[avm_led_mode_action] restart with timer\n"); ---*/
        mode->current_pos = 0;
        return;
    }
    mode->current_pos++;
    return;
}
