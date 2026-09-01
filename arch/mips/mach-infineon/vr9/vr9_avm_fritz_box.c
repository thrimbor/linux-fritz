/******************************************************************************
**
** FILE NAME    : vr9_avm_fritz_box.c
** PROJECT      : IFX UEIP
** MODULES      : BSP Basic
**
** DATE         : 21 Jan 2010
** AUTHOR       :
** DESCRIPTION  : source file for VR9
** COPYRIGHT    :       Copyright (c) 2009
**                      Infineon Technologies AG
**                      Am Campeon 1-12, 85579 Neubiberg, Germany
**
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
** HISTORY
** $Date        $Author         $Comment
** 27 May 2009   Xu Liang        The first UEIP release
*******************************************************************************/



#include <linux/module.h>
#include <linux/types.h>
#include <linux/kallsyms.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#include <ifx_regs.h>
#include <ifx_types.h>
#include <irq.h>
#include <ifx_ledc.h>
#include <ifx_led.h>
#include <ifx_si.h>


struct ifx_si_eiu_config g_si_eiu_config = {
    .irq            = -1,   //  no serial input
    .intsync        = 0,
    .sampling_clk   = 0,
    .shift_clk      = 0,
    .group          = 0,
    .active_high    = 0,
};

struct ifx_ledc_config_param g_board_ledc_hw_config = {
    .operation_mask         = IFX_LEDC_CFG_OP_UPDATE_SOURCE | 
                              IFX_LEDC_CFG_OP_BLINK | 
                              IFX_LEDC_CFG_OP_UPDATE_CLOCK | 
                              IFX_LEDC_CFG_OP_STORE_MODE | 
                              IFX_LEDC_CFG_OP_SHIFT_CLOCK | 
                              IFX_LEDC_CFG_OP_DATA_OFFSET | 
                              IFX_LEDC_CFG_OP_NUMBER_OF_LED | 
                              IFX_LEDC_CFG_OP_DATA | 
                              IFX_LEDC_CFG_OP_MIPS0_ACCESS | 
                              IFX_LEDC_CFG_OP_DATA_CLOCK_EDGE,
    .source_mask            = 0xFF,
    .source                 = 0xFF, //  LEDs controlled by EXT Src
    .blink_mask             = (1 << 24) - 1,
    .blink                  = 0,    //  disable blink for all LEDs
    .update_clock           = LED_CON1_UPDATE_SRC_FPI,
    .fpid                   = 1,
    .store_mode             = 0,    //  single store
    .fpis                   = 2,
    .data_offset            = 0,
    .number_of_enabled_led  = 24,
    .data_mask              = (1 << 24) - 1,
    .data                   = 0,
    .mips0_access_mask      = (1 << 24) - 1,
    .mips0_access           = (1 << 24) - 1,
    .f_data_clock_on_rising = 0,    //  falling edge
};
EXPORT_SYMBOL(g_board_ledc_hw_config);

struct ifx_led_device g_board_led_hw_config[] = {
#if 0
    {
        .default_trigger    = NULL,
        .phys_id            = 0,
        .value_on           = 0,
        .value_off          = 1,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 1,
        .value_on           = 0,
        .value_off          = 1,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 2,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 3,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 4,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 5,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 6,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 7,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
#endif
    {
        .default_trigger    = NULL,
        .phys_id            = 8,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 9,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 10,
        .value_on           = 0,
        .value_off          = 1,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 11,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 12,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 13,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 14,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 15,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 16,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 17,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 18,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 19,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 20,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 21,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 22,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .default_trigger    = NULL,
        .phys_id            = 23,
        .value_on           = 1,
        .value_off          = 0,
        .flags              = IFX_LED_DEVICE_FLAG_PHYS_LEDC,
    },
    {
        .flags              = IFX_LED_DEVICE_FLAG_INVALID,
    }
};
EXPORT_SYMBOL(g_board_led_hw_config);

