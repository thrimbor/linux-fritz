/*------------------------------------------------------------------------------------------*\
 *
 * Hardware Config für FRITZ!Box Fon 3390
 *
\*------------------------------------------------------------------------------------------*/

#undef AVM_HARDWARE_CONFIG
#define AVM_HARDWARE_CONFIG  avm_hardware_config_hw193


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
        .name   = "gpio_avm_led_power",
        .value  = 45,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_power_red",
        .value  = 46,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_lan_all",
        .value  = 47,
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
        .name   = "gpio_avm_led_pppoe",
        .value  = 35,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_info",
        .value  = 33,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },

    /*--- DECT button connected to EXTIN 1 ---*/
    {
        .name   = "gpio_avm_button_wps",
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
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },
    {
        .name   = "gpio_avm_extphy1_reset",
        .value  = 32,
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
    {
        .name   = "gpio_avm_pcie_chip_reset",
        .value  = 22,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_PCIE,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET 
        }
    },
    {
        .name   = "gpio_avm_offload_wasp_wakeup",
        .value  =  7,
        .param  = avm_hw_param_gpio_out_active_high,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_WLAN_OFFLOAD_WASP_RESET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR| IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET
        }
    },
    {
        .name   = "gpio_avm_offload_wasp_reset",
        .value  = 34,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_WLAN_OFFLOAD_WASP_RESET,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR| IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDSEL_SET
        }
    },
    {
        .name   = "gpio_avm_offload_mac",
        .value  =  5,
        .param  = avm_hw_param_no_param,
    },


    /*--------------------------------------------------------------------------------------*\
     * SYSTEM
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_pll_clk",
        .value  =  2,
        .param  = avm_hw_param_no_param,
    },
    {
        .name   = "gpio_avm_boot_sel3",
        .value  =  4,
        .param  = avm_hw_param_gpio_in_active_high,
    },


    /*--------------------------------------------------------------------------------------*\
     * UART fix 3390 UART-Verbindung zum WASP
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_uart_tx",
        .value  =  11,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_USIF_UART,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_CLEAR 
        }
    },
    {
        .name   = "gpio_avm_uart_rx",
        .value  =  12,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_USIF_UART,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET | IFX_GPIO_IOCTL_PIN_CONFIG_PUDEN_CLEAR 
        }
    },


    {   .name   = NULL }
};
EXPORT_SYMBOL(AVM_HARDWARE_CONFIG);


