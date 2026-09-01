/*------------------------------------------------------------------------------------------*\
 *
 * Hardware Config für FRITZ!Box 7272 (Italo)
 *
\*------------------------------------------------------------------------------------------*/


#undef AVM_HARDWARE_CONFIG
#define AVM_HARDWARE_CONFIG  avm_hardware_config_hw192


struct _avm_hw_config AVM_HARDWARE_CONFIG[] = {


    /****************************************************************************************\
     *
     * GPIO Config
     *
    \****************************************************************************************/


    /*------------------------------------------------------------------------------------------*\
     * LEDs / Taster
    \*------------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_led_power",  /*--- LED5 ---*/
        .value  = 0x107 - 4,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT
        }
    },
    {
        .name   = "gpio_avm_led_internet",  /*--- LED4 ---*/
        .value  = 0x107 - 3,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT
        }
    },
    {
        .name   = "gpio_avm_led_festnetz",  /*--- LED3 ---*/
        .value  = 0x107 - 2,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT
        }
    },
    {
        .name   = "gpio_avm_led_wlan",  /*--- LED2 ---*/
        .value  = 0x107 - 1,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT
        }
    },
    {
        .name   = "gpio_avm_led_info",  /*--- LED1 ---*/
        .value  = 0x107 - 0,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT
        }
    },
    {
        .name   = "gpio_avm_led_info_red",  /*--- LED1R ---*/
        .value  = 0x107 - 5,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT
        }
    },

    /*--- DECT button connected to EXTIN 0 ---*/
    {
        .name   = "gpio_avm_button_dect",
        .value  = 0,
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR 
        }
    },
    /*--- WLAN button connected to EXTIN 1 ---*/
    {
        .name   = "gpio_avm_button_wlan",
        .value  = 1,
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET 
        }
    },

    /*--------------------------------------------------------------------------------------*\
     * SHIFT REGISTER
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_shift_led_st",
        .value  = 4,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SHIFT_REG,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR 
        }
    },
    {
        .name   = "gpio_avm_shift_led_d",
        .value  = 5,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SHIFT_REG,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR 
        }
    },
    {
        .name   = "gpio_avm_shift_led_sh",
        .value  = 6,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SHIFT_REG,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR 
        }
    },


    /*--------------------------------------------------------------------------------------*\
     * DECT
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_dect_reset",
        .value  = IFX_GPIO_PIN_ID(2, 2),
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PIGLET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_dect_uart_rx",
        .value  = IFX_GPIO_PIN_ID(0, 11),
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PIGLET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_dect_uart_tx",
        .value  = IFX_GPIO_PIN_ID(0, 10),
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PIGLET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * FPGA
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_piglet_noemif_prog",
        .value  = IFX_GPIO_PIN_ID(0, 9),
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_FPGA,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_piglet_noemif_clk",
        .value  = IFX_GPIO_PIN_ID(0, 8),
        .param  = avm_hw_param_gpio_out_active_high,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_FPGA,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_piglet_noemif_data",
        .value  = IFX_GPIO_PIN_ID(2, 10),
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_FPGA,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_piglet_noemif_done",
        .value  = IFX_GPIO_PIN_ID(2, 11),
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_FPGA,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET
        }
    },
    {
        .name   = "gpio_avm_piglet_noemif_fpgaok",
        .value  = IFX_GPIO_PIN_ID(2, 3),
        .param  = avm_hw_param_gpio_in_active_high,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_FPGA,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    /*--------------------------------------------------------------------------------*\
     * TDM-Check (Clockmessung)
    \*--------------------------------------------------------------------------------*/
    { 
        .name   = "gpio_avm_tdm_fsc",              
        .value  = IFX_GPIO_PIN_ID(3, 10),
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_TDMCHECK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_tdm_dcl",
        .value  = IFX_GPIO_PIN_ID(1, 11),
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_TDMCHECK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR  
        }
    },
    /*--------------------------------------------------------------------------------------*\
     * PCM-BUS
    \*--------------------------------------------------------------------------------------*/
    { 
        .name   = "gpio_avm_pcmlink_fsc",              
        .value  = IFX_GPIO_PIN_ID(3, 10),
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET
        }
    },
    {
        .name   = "gpio_avm_pcmlink_do",
        .value  = IFX_GPIO_PIN_ID(1, 9),
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_pcmlink_di",
        .value  = IFX_GPIO_PIN_ID(1, 10),
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET 
        }
    },
    {
        .name   = "gpio_avm_pcmlink_dcl",
        .value  = IFX_GPIO_PIN_ID(1, 11),
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET 
        }
    },

    /*--------------------------------------------------------------------------------------*\
     * SPI
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_spi_clk",
        .value  = 18,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SSC,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_spi_do",
        .value  = 17,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SSC,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_spi_di",
        .value  = 16,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SSC,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_spi_flash_cs",
        .value  = 15,
        .param  = avm_hw_param_gpio_out_active_high,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SPI_FLASH,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_spi_usif_cs0",
        .value  = 14,
        .param  = avm_hw_param_gpio_out_active_high,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SPI_FLASH,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_spi_usif_clk",
        .value  = 19,
        .param  = avm_hw_param_gpio_out_active_high,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SPI_FLASH,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },

    /*--------------------------------------------------------------------------------------*\
     * NAND
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_nand_ale",
        .value  = 13,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_nand_cle",
        .value  = 24,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_nand_rd_by",
        .value  = 48,
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_CLEAR

        }
    },
    {
        .name   = "gpio_avm_nand_rd",
        .value  = 49,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET

        }
    },
    {
        .name   = "gpio_avm_nand_wr",
        .value  = 59,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET

        }
    },
    {
        .name   = "gpio_avm_nand_wp",
        .value  = 60,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET

        }
    },
    {
        .name   = "gpio_avm_nand_cs1",
        .value  = 23,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_nand_d0",
        .value  = 51,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_nand_d1",
        .value  = 50,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_nand_d2",
        .value  = 52,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_nand_d3",
        .value  = 57,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_nand_d4",
        .value  = 56,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_nand_d5",
        .value  = 55,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_nand_d6",
        .value  = 54,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_nand_d7",
        .value  = 53,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },

    /*--------------------------------------------------------------------------------------*\
     * USB
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_usb_pwr_en0",
        .value  =  0x107 - 6,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_USB,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT
        }
    },

    /*--------------------------------------------------------------------------------------*\
     * PLL-Clock
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_pll_clk",
        .value  = 3,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_MDIO,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },


    /*--------------------------------------------------------------------------------------*\
     * PCIE / WLAN / Ext. WASP
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_pcie_reset",
        .value  = 36,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCIE,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET 
        }
    },
    {
        .name   = "gpio_avm_pcie_chip_reset",
        .value  = 61,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCIE,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET 
        }
    },
    {   /* PCIe Interface 1 deaktivieren */
        .name   = "pcie_disable_interface_1",
        .value  = 1
    },



    {   .name   = NULL }
};
EXPORT_SYMBOL(AVM_HARDWARE_CONFIG);


