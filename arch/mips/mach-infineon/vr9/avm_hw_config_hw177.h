/*------------------------------------------------------------------------------------------*\
 *
 * Hardware Config für FRITZ!Box Fon 6840 LTE (Sub-Rev 1 & 2+)
 *
\*------------------------------------------------------------------------------------------*/

#undef AVM_HARDWARE_CONFIG
#define AVM_HARDWARE_CONFIG  

struct _avm_hw_config avm_hardware_config_hw177_subrev_1[] = {


    /****************************************************************************************\
     *
     * GPIO Config
     *
    \****************************************************************************************/


    /*------------------------------------------------------------------------------------------*\
     * LEDs / Taster
    \*------------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_led_power",
        .value  = 32,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_power_red",
        .value  = 33,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_internet",
        .value  = 38,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_dect",
        .value  = 39,  /*--- bei Hardware Sub Revision 1 ist dieses GPIO 39 stat 35 ---*/
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_wlan",
        .value  = 36,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_info",
        .value  = 47,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_info_red",
        .value  = 34,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },

    /*--- DECT button connected to EXTIN 1 ---*/
    {
        .name   = "gpio_avm_button_dect",
        .value  = 1,
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR 
        }
    },
    /*--- WLAN button connected to EXTIN 0 ---*/
    {
        .name   = "gpio_avm_button_wlan",
        .value  = 29,
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET 
        }
    },


    /*--------------------------------------------------------------------------------------*\
     * DECT
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_dect_reset",
        .value  = 30,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PIGLET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_dect_uart_rx",
        .value  = 11,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PIGLET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_dect_uart_tx",
        .value  = 12,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PIGLET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    /*--------------------------------------------------------------------------------*\
     * TDM-Check (Clockmessung)
    \*--------------------------------------------------------------------------------*/
    { 
        .name   = "gpio_avm_tdm_fsc",              
        .value  = 0,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_TDMCHECK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_tdm_dcl",
        .value  = 40,
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
        .value  = 0,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET
        }
    },
    {
        .name   = "gpio_avm_pcmlink_do",
        .value  = 25,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_pcmlink_di",
        .value  = 41,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET 
        }
    },
    {
        .name   = "gpio_avm_pcmlink_dcl",
        .value  = 40,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET 
        }
    },
    {
        .name   = "gpio_avm_spi_cs2",
        .value  = 22,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR 
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
        .value  = 10,
        .param  = avm_hw_param_gpio_out_active_high,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SPI_FLASH,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
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
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_nand_cle",
        .value  = 24,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_nand_rd",
        .value  = 49,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_nand_rd_by",
        .value  = 48,
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_nand_cs1",
        .value  = 23,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },


    /*--------------------------------------------------------------------------------------*\
     * USB
    \*--------------------------------------------------------------------------------------*/
#if defined(CONFIG_USB_HOST_IFX) || defined(CONFIG_USB_HOST_IFX_MODULE)
    {
        .name   = "gpio_avm_usb_pwr_en0",
        .value  =  IFX_GPIO_USB_VBUS2,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_USB,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_usb_pwr_en1",
        .value  =  IFX_GPIO_USB_VBUS1,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_USB,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_usb_fault_det0",
        .value  =  9,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_USB,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_usb_fault_det1",
        .value  =  6,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_USB,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
#endif


    /*--------------------------------------------------------------------------------------*\
     * ETHERNET
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_extphy_25mhz",
        .value  =  3,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_25MHZ_CLOCK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_extphy1_reset",
        .value  = 37,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_RESET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR| IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_extphy2_reset",
        .value  = 44,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_RESET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR| IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_extphy_int",
        .value  = 35,  /*--- bei Hardware Sub Revision 1 ist dieses GPIO 35 stat 39 ---*/
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
           .module_id = IFX_GPIO_MODULE_EXTPHY_INT,
           .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET
        }
    },
    {
        .name   = "gpio_avm_mii_mdio",
        .value  = 42,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_MDIO,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_mii_mdc",
        .value  = 43,
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
        .value  = 21,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCIE,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET 
        }
    },


    {   .name   = NULL }
};

struct _avm_hw_config avm_hardware_config_hw177[] = {


    /****************************************************************************************\
     *
     * GPIO Config
     *
    \****************************************************************************************/


    /*------------------------------------------------------------------------------------------*\
     * LEDs / Taster
    \*------------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_led_power",
        .value  = 32,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_power_red",
        .value  = 33,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_internet",
        .value  = 38,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_dect",
        .value  = 35,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_wlan",
        .value  = 36,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_info",
        .value  = 47,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_info_red",
        .value  = 34,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },

    /*--- DECT button connected to EXTIN 1 ---*/
    {
        .name   = "gpio_avm_button_dect",
        .value  = 1,
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR 
        }
    },
    /*--- WLAN button connected to EXTIN 0 ---*/
    {
        .name   = "gpio_avm_button_wlan",
        .value  = 29,
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET 
        }
    },


    /*--------------------------------------------------------------------------------------*\
     * DECT
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_dect_reset",
        .value  = 30,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PIGLET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_dect_uart_rx",
        .value  = 11,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PIGLET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_dect_uart_tx",
        .value  = 12,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PIGLET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    /*--------------------------------------------------------------------------------*\
     * TDM-Check (Clockmessung)
    \*--------------------------------------------------------------------------------*/
    { 
        .name   = "gpio_avm_tdm_fsc",              
        .value  = 0,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_TDMCHECK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_tdm_dcl",
        .value  = 40,
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
        .value  = 0,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET
        }
    },
    {
        .name   = "gpio_avm_pcmlink_do",
        .value  = 25,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_pcmlink_di",
        .value  = 41,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET 
        }
    },
    {
        .name   = "gpio_avm_pcmlink_dcl",
        .value  = 40,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_SET 
        }
    },
    {
        .name   = "gpio_avm_spi_cs2",
        .value  = 22,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCMLINK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR 
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
        .value  = 10,
        .param  = avm_hw_param_gpio_out_active_high,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SPI_FLASH,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
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
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_nand_cle",
        .value  = 24,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_nand_rd",
        .value  = 49,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_nand_rd_by",
        .value  = 48,
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_nand_cs1",
        .value  = 23,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_NAND,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },


    /*--------------------------------------------------------------------------------------*\
     * USB
    \*--------------------------------------------------------------------------------------*/
#if defined(CONFIG_USB_HOST_IFX) || defined(CONFIG_USB_HOST_IFX_MODULE)
    {
        .name   = "gpio_avm_usb_pwr_en0",
        .value  =  IFX_GPIO_USB_VBUS2,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_USB,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_usb_pwr_en1",
        .value  =  IFX_GPIO_USB_VBUS1,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_USB,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_usb_fault_det0",
        .value  =  9,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_USB,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_usb_fault_det1",
        .value  =  6,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_USB,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
#endif


    /*--------------------------------------------------------------------------------------*\
     * ETHERNET
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_extphy_25mhz",
        .value  =  3,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_25MHZ_CLOCK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_extphy1_reset",
        .value  = 37,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_RESET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR| IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_extphy2_reset",
        .value  = 44,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_RESET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR| IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_extphy_int",
        .value  = 39,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
           .module_id = IFX_GPIO_MODULE_EXTPHY_INT,
           .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET
        }
    },
    {
        .name   = "gpio_avm_mii_mdio",
        .value  = 42,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_MDIO,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_mii_mdc",
        .value  = 43,
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
        .value  = 21,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCIE,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET 
        }
    },




    {   .name   = NULL }
};

EXPORT_SYMBOL(avm_hardware_config_hw177_subrev_1);
EXPORT_SYMBOL(avm_hardware_config_hw177);


