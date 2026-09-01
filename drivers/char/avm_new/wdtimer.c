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
/******************************************************************************
 * FILE PURPOSE:    Watchdog Timer Module Source file
 ********************************************************************************
 * FILE NAME:       wdtimer.c
 *
 * DESCRIPTION:     Platform and OS independent Implementation for watchdog timer module
 *
 * REVISION HISTORY:
 * 27 Nov 02 - PSP TII  
 * 
 * This watchdog timer module has prescalar and counter which divide the input
 *  reference frequency and upon expiration, the system is reset.
 * 
 *                        ref_freq 
 * Reset freq = ---------------------
 *                  (prescalar * counter)
 * 
 * This watchdog timer supports timer values in mSecs. Thus
 * 
 *           prescalar * counter * 1 KHZ
 * mSecs =   --------------------------
 *                  ref_freq
 * 
 *******************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/avalanche/generic/avalanche_types.h>
#include <asm/avalanche/generic/hal_modules/wdtimer.h>
#include "avm_event.h"

#define KHZ                         1000
#define KICK_VALUE                  1
#define KICK_LOCK_STATE             0x3
#define CHANGE_LOCK_STATE           0x3
#define DISABLE_LOCK_STATE          0x3
#define PRESCALE_LOCK_STATE         0x3

#define MAX_PRESCALE_REG_VALUE      0xFFFF  
#define MAX_CHANGE_REG_VALUE        0xFFFF
#define MAX_CLK_DIVISOR_VALUE       0xFFFE0001 /* MAX_PRESCALE_REG_VALUE * MAX_CHANGE_REG_VALUE */

#define MIN_PRESCALE_REG_VALUE      0x0
#define MIN_CHANGE_REG_VALUE        0x1

#define KICK_LOCK_1ST_STAGE         0x5555
#define KICK_LOCK_2ND_STAGE         0xAAAA
#define PRESCALE_LOCK_1ST_STAGE     0x5A5A
#define PRESCALE_LOCK_2ND_STAGE     0xA5A5
#define CHANGE_LOCK_1ST_STAGE       0x6666
#define CHANGE_LOCK_2ND_STAGE       0xBBBB
#define DISABLE_LOCK_1ST_STAGE      0x7777
#define DISABLE_LOCK_2ND_STAGE      0xCCCC
#define DISABLE_LOCK_3RD_STAGE      0xDDDD

/* private functions */
static int write_kick_reg(unsigned int);
static int write_change_reg(unsigned int);
static int write_prescale_reg(unsigned int);
static int write_disable_reg(unsigned int);

static unsigned int refclk_freq;
static volatile WDTIMER_STRUCT_T *p_wdtimer;

/****************************************************************************
 * PUBLIC FUNCTIONS
 ***************************************************************************/
/****************************************************************************
 * FUNCTION: wdtimer_init
 ****************************************************************************
 * Description: The routine initializes the watch dog timer module 
 ***************************************************************************/ 
void wdtimer_init (unsigned int base_addr, unsigned int vbus_clk_freq) {
    p_wdtimer = (WDTIMER_STRUCT_T *)base_addr;
    refclk_freq = vbus_clk_freq;
}


/****************************************************************************
 * FUNCTION: wdtimer_set_period
 ****************************************************************************
 * Description: The routine sets the timeout period of watchdog timer 
 ***************************************************************************/ 
int wdtimer_set_period(unsigned int msec) {
    unsigned int change_value;
    unsigned int count_value;
    unsigned int prescale_value = 1;
    unsigned int refclk_khz = (refclk_freq / KHZ);
    
    /* The min time period is 1 msec and since the reference clock freq is always going 
       to be more than "min" divider value, minimum value is not checked.
       Check the max time period that can be derived from the timer in milli-secs
    */
    /*--- printk("wdtimer_set_period(%u ms)\n", msec); ---*/

    if ( (msec == 0) || (refclk_khz== 0) ||
         (msec > ((MAX_CLK_DIVISOR_VALUE) / refclk_khz)) )
    {
        printk("WDTIMER_ERR_INVAL: msec=%u > MAX_CLK_DIVISOR_VALUE=%u/%u MHz\n", msec,  MAX_CLK_DIVISOR_VALUE, refclk_khz);
        return (WDTIMER_ERR_INVAL);
    }

    /* compute the prescale value and change register values of the timer 
     * corresponding to the variable  milsec; 
     * divide by 1000 since time period is in millisec
     */
    count_value = refclk_khz * msec; 
    
    if (count_value > MAX_CHANGE_REG_VALUE)
    {
        /* Add 1 to change value so that prescale value computed
         * will be within range 
         */
        change_value = count_value / MAX_PRESCALE_REG_VALUE + 1;
        prescale_value = count_value / change_value;
    }    
    else 
    {
        change_value =  count_value;
    }
                                                               
     /* write to prescale register*/
     if (write_prescale_reg(prescale_value - 1) == WDTIMER_RET_ERR) {
         printk("WDTIMER_RET_ERR: write_prescale_reg(%u - 1) failed\n", prescale_value);
         return WDTIMER_RET_ERR;  
     }

    /* write to change register*/
    if (write_change_reg(change_value ) == WDTIMER_RET_ERR) {
         printk("WDTIMER_RET_ERR: write_change_reg(%u) failed\n", change_value);
         return WDTIMER_RET_ERR;  
    }
    
    
    /*--- printk("wdtimer_set_period(...) success\n"); ---*/
    return WDTIMER_RET_OK;
}

/****************************************************************************
 * FUNCTION: wdtimer_ctrl
 ****************************************************************************
 * Description: The routine enables/disables the watchdog timer
 ***************************************************************************/ 
int wdtimer_ctrl(WDTIMER_CTRL_T wd_ctrl) {
    return write_disable_reg(wd_ctrl);
}

/****************************************************************************
 * FUNCTION: wdtimer_kick
 ****************************************************************************
 * Description: The routine kicks(restarts) the watchdog timer
 ***************************************************************************/ 
int wdtimer_kick(void) {
    return write_kick_reg(KICK_VALUE);
} 


/* private functions */

/****************************************************************************
 * FUNCTION: write_kick_reg
 ****************************************************************************
 * Description: The routine writes the specified value into kick register after unlocking it.
 ***************************************************************************/ 
static int write_kick_reg(unsigned int kick_value) {
    p_wdtimer->kick_lock = KICK_LOCK_1ST_STAGE;      /* Unlocking 1st stage. */

    if( (p_wdtimer->kick_lock & KICK_LOCK_STATE) == 0x1) 
    {
        p_wdtimer->kick_lock = KICK_LOCK_2ND_STAGE; /* Unlocking 2nd stage. */

        if( (p_wdtimer->kick_lock & KICK_LOCK_STATE) == 0x3)
        {
               p_wdtimer->kick  = kick_value;
               return WDTIMER_RET_OK;
        }
        else
        {
            return WDTIMER_RET_ERR; /*2nd stage unlocking failed. */
        }
    }
    else
    {
        return WDTIMER_RET_ERR; /*1st  stage unlocking failed. */
    }
}   


/****************************************************************************
 * FUNCTION: write_prescale_reg
 ****************************************************************************
 * Description: The routine writes the specified value into kick register after unlocking it.
 ***************************************************************************/ 
static int write_prescale_reg(unsigned int prescale_value) {
    p_wdtimer->prescale_lock = PRESCALE_LOCK_1ST_STAGE;      /* Unlocking 1st stage. */

    if( (p_wdtimer->prescale_lock & PRESCALE_LOCK_STATE) == 0x1) 
    {
        p_wdtimer->prescale_lock = PRESCALE_LOCK_2ND_STAGE; /* Unlocking 2nd stage. */

        if( (p_wdtimer->prescale_lock & PRESCALE_LOCK_STATE) == 0x3)
        {
               p_wdtimer->prescale  = prescale_value;
               return WDTIMER_RET_OK;
        }
        else
        {
            return WDTIMER_RET_ERR; /*2nd stage unlocking failed. */
        }
    }
    else
    {
        return WDTIMER_RET_ERR; /*1st  stage unlocking failed. */
    }
}   

/****************************************************************************
 * FUNCTION: write_change_reg
 ****************************************************************************
 * Description: The routine writes the specified value into kick register after unlocking it.
 ***************************************************************************/ 
static int write_change_reg(unsigned int initial_timer_value) {
    p_wdtimer->change_lock = CHANGE_LOCK_1ST_STAGE;      /* Unlocking 1st stage. */

    if( (p_wdtimer->change_lock & CHANGE_LOCK_STATE) == 0x1) 
    {
        p_wdtimer->change_lock = CHANGE_LOCK_2ND_STAGE; /* Unlocking 2nd stage. */

        if( (p_wdtimer->change_lock & CHANGE_LOCK_STATE) == 0x3)
        {
               p_wdtimer->change    = initial_timer_value;
               return WDTIMER_RET_OK;
        }   
        else
        {
            return WDTIMER_RET_ERR; /*2nd stage unlocking failed. */
        }
    }
    else
    {
        return WDTIMER_RET_ERR; /*1st stage unlocking failed. */
    }
}   


/****************************************************************************
 * FUNCTION: write_disable_reg
 ****************************************************************************
 * Description: The routine writes the specified value into kick register after unlocking it.
 ***************************************************************************/ 
int write_disable_reg(unsigned int disable_value) {

    p_wdtimer->disable_lock = DISABLE_LOCK_1ST_STAGE;        /* Unlocking 1st stage. */

    if( (p_wdtimer->disable_lock & DISABLE_LOCK_STATE) == 0x1) 
    {
        p_wdtimer->disable_lock = DISABLE_LOCK_2ND_STAGE;   /* Unlocking 2nd stage. */

        if( (p_wdtimer->disable_lock & DISABLE_LOCK_STATE) == 0x2)
        {
             p_wdtimer->disable_lock = DISABLE_LOCK_3RD_STAGE;  /* Unlocking 3rd stage. */

             if( (p_wdtimer->disable_lock & DISABLE_LOCK_STATE) == 0x3)
             {
                  p_wdtimer->disable    = disable_value;
                  return WDTIMER_RET_OK;
             }    
             else
             {
                 return WDTIMER_RET_ERR; /*3rd stage unlocking failed. */
             }
        }
        else
        {
            return WDTIMER_RET_ERR; /* 2nd stage unlocking failed */
        }
    }
    else
    {
        return WDTIMER_RET_ERR; /*1st stage unlocking failed. */
    }

}   

