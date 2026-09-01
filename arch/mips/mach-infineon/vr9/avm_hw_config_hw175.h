/*------------------------------------------------------------------------------------------*\
 *
 * Hardware Config für FRITZ!Box Fon 3370 (Sub-Rev 1 & 2+)
 *
\*------------------------------------------------------------------------------------------*/

#undef AVM_HARDWARE_CONFIG
#define AVM_HARDWARE_CONFIG  


struct _avm_hw_config avm_hardware_config_hw175_subrev_1[] = {


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
        .name   = "gpio_avm_led_lan_all",
        .value  = 38,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    /*--- XXX LED3/WLAN für 3370 HWSubRevision <= 1 (ab HWSubRevision >= 2 GPIO 39 für IFX_GPIO_MODULE_EXTPHY_MDIO genutzt) ---*/
    {
        .name   = "gpio_avm_led_wlan",
        .value  = 39,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_pppoe",
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

    /*--- POWER button connected to EXTIN 1 ---*/
    {
        .name   = "gpio_avm_button_power",
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


    /*--------------------------------------------------------------------------------------*\
     * SYSTEM
    \*--------------------------------------------------------------------------------------*/
    /*--- power off wird mit LED Modul aktiviert da ohne Taster sinnlos ---*/
    {
        .name   = "gpio_avm_system_power_off",
        .value  = 45,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SYSTEM,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },




    {   .name   = NULL }
};

struct _avm_hw_config avm_hardware_config_hw175[] = {


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
        .name   = "gpio_avm_led_lan_all",
        .value  = 38,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    /*--- LED3/WLAN für 3370 HWSubRevision >= 2 ---*/
    {
        .name   = "gpio_avm_led_wlan",
        .value  = 35,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_pppoe",
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

    /*--- POWER button connected to EXTIN 1 ---*/
    {
        .name   = "gpio_avm_button_power",
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


    /*--------------------------------------------------------------------------------------*\
     * SYSTEM
    \*--------------------------------------------------------------------------------------*/
    /*--- power off wird mit LED Modul aktiviert da ohne Taster sinnlos ---*/
    {
        .name   = "gpio_avm_system_power_off",
        .value  = 45,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_SYSTEM,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },




    {   .name   = NULL }
};


EXPORT_SYMBOL(avm_hardware_config_hw175_subrev_1);
EXPORT_SYMBOL(avm_hardware_config_hw175);


