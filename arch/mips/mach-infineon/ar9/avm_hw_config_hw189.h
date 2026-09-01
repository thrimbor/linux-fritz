/*------------------------------------------------------------------------------------------*\
 *
 * Hardware Config für FRITZ!Box Fon 7312
 *
\*------------------------------------------------------------------------------------------*/

#undef AVM_HARDWARE_CONFIG
#define AVM_HARDWARE_CONFIG  avm_hardware_config_hw189


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
        .value  = 44,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_internet",
        .value  = 47,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_dect",
        .value  = 38,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_wlan",
        .value  = 37,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },
    {
        .name   = "gpio_avm_led_info",
        .value  = 35,
        .param  = avm_hw_param_gpio_out_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OUTPUT_SET
        }
    },

    /*--- DECT button connected to EXTIN 1 ---*/
    {
        .name   = "gpio_avm_button_dect",
        .value  = 2,
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET
        }
    },
    /*--- WLAN button connected to EXTIN 0 ---*/
    {
        .name   = "gpio_avm_button_wlan",
        .value  = 1,
        .param  = avm_hw_param_gpio_in_active_low,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_LED,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN  | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET   | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR
        }
    },


    /*--------------------------------------------------------------------------------------*\
     * DECT
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_dect_reset",
        .value  = 54,
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
     * PCM-BUS (TDM)
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


    /*--------------------------------------------------------------------------------------*\
     * ETHERNET
    \*--------------------------------------------------------------------------------------*/
    {
        .name   = "gpio_avm_extphy_25mhz",
        .value  =  3,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_25MHZ_CLOCK,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "feature_avm_enhance_phy_clock",
        .value  =  1,
        .param  = avm_hw_param_no_param
    },
    {
        .name   = "gpio_avm_extphy1_reset",
        .value  = 34,
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
           .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_CLEAR | IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_SET
        }
    },
    {
        .name   = "gpio_avm_mii_mdio",
        .value  = 42,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_MDIO,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_IN|IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT|IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET|IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR|IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
        }
    },
    {
        .name   = "gpio_avm_mii_mdc",
        .value  = 43,
        .param  = avm_hw_param_no_param,
        .manufactor_hw_config.manufactor_lantiq_gpio_config = {
            .module_id = IFX_GPIO_MODULE_EXTPHY_MDIO,
            .config    = IFX_GPIO_IOCTL_PIN_CONFIG_DIR_OUT|IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL0_SET|IFX_GPIO_IOCTL_PIN_CONFIG_ALTSEL1_CLEAR|IFX_GPIO_IOCTL_PIN_CONFIG_OD_SET
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
EXPORT_SYMBOL(AVM_HARDWARE_CONFIG);


