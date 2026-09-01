#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/prom.h>
#include <linux/avm_hw_config.h>


/*------------------------------------------------------------------------------------------*\
 * FRITZ!WLAN Repeater 300E (Virian)
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw173[] = {
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw173);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box ATH DB120
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw96[] = {
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw96);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box 6810 LTE (ab C-Karten)
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw180[] = {
    { .name = "gpio_avm_button_wlan",    .value = 4,  .param = avm_hw_param_gpio_in_active_low },
    { .name = "gpio_avm_button_dect",    .value = 16, .param = avm_hw_param_gpio_in_active_low },
    { .name = "gpio_avm_led_power",      .value = 18, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_dect",       .value = 19, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_wlan",       .value = 20, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_internet",   .value = 22, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_info",       .value = 21, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_dect_reset",     .value = 11 },
    { .name = "gpio_avm_dect_rd",        .value = 9  },
    { .name = "gpio_slic_pcm_fs",        .value = 12 },
    { .name = "gpio_slic_pcm_clk",       .value = 13 },
    { .name = "gpio_slic_pcm_do",        .value = 14 },
    { .name = "gpio_slic_pcm_di",        .value = 15 },
    { .name = "usb_hub_available",       .value = 1  },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw180);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box 6841 LTE
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw184[] = {
    { .name = "gpio_avm_button_wlan",   .value = 4,  .param = avm_hw_param_gpio_in_active_low },
    { .name = "gpio_avm_button_dect",   .value = 16, .param = avm_hw_param_gpio_in_active_low },
    { .name = "gpio-usb_reset",         .value = 19, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_shift_clk",     .value = 20, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_shift_din",     .value = 22, .param = avm_hw_param_gpio_in_active_low  },
    { .name = "shift_register_size",    .value = 8 }, 
    { .name = "gpio_avm_led_info",      .value = 100 },
    { .name = "gpio_avm_led_info_red",  .value = 101 },
    { .name = "gpio_avm_led_wlan",      .value = 102 },
    { .name = "gpio_avm_led_dect",      .value = 103 },
    { .name = "gpio_avm_led_internet",  .value = 104 },
    { .name = "gpio_avm_led_power_red", .value = 105 },
    { .name = "gpio_avm_led_power",     .value = 106 },
    { .name = "gpio_avm_extphy1_reset", .value = 18 },
    { .name = "gpio_avm_usb_reset",     .value = 19 },
    { .name = "gpio_avm_dect_reset",    .value = 21 },
    { .name = "gpio_avm_dect_rd",       .value = 9  },
    { .name = "gpio_slic_pcm_fs",       .value = 12 },
    { .name = "gpio_slic_pcm_clk",      .value = 13 },
    { .name = "gpio_slic_pcm_do",       .value = 14 },
    { .name = "gpio_slic_pcm_di",       .value = 15 },
    { .name = "usb_hub_available",      .value = 1  },
    { .name = "usb_hub_external_port1", .value = 3  },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw184);

/*------------------------------------------------------------------------------------------*\
 * FRITZ!Powerline 546E
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw190[] = {
    { .name = "gpio_avm_button_wlan",       .value = 21, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_button_power",      .value = 14, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_button_plc",        .value = 16, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_power",         .value = 17, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_wlan",          .value = 13, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_plc",           .value = 19, .param = avm_hw_param_gpio_out_active_high },
    { .name = "gpio_avm_relay",             .value = 12 },
    { .name = "gpio_avm_prolific_ac_mode",  .value =  4 },
    { .name = "gpio_avm_prolific_ac_reset", .value = 15 },
    { .name = "gpio_avm_reset_plc",         .value = 20 },
    { .name = "uart_enable_ath_hi",         .value =  2 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw190);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Repeater 310
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw194[] = {
    { .name = "gpio_avm_button_wlan",   .value = 22, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_fsi0",      .value = 13, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_fsi1",      .value = 14, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_fsi2",      .value = 15, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_fsi3",      .value = 16, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_fsi4",      .value = 17, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_wlan",      .value = 19, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_power",     .value = 20, .param = avm_hw_param_gpio_out_active_low },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw194);

/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box 6842 LTE
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw195[] = {
    { .name = "gpio_avm_button_wlan",   .value = 16, .param = avm_hw_param_gpio_in_active_low },
    { .name = "gpio_avm_button_dect",   .value = 4,  .param = avm_hw_param_gpio_in_active_low },

    /*--- Shift Register: ---*/
    { .name = "gpio_avm_shift_clk",     .value = 20, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_shift_din",     .value = 22, .param = avm_hw_param_gpio_in_active_low  },
    { .name = "gpio_avm_led_wlan",      .value = 100 },
    { .name = "gpio_avm_led_wlan_red",  .value = 101 },
    { .name = "gpio_avm_led_info",      .value = 102 },
    { .name = "gpio_avm_led_internet",  .value = 103 },
    { .name = "gpio_avm_led_dect",      .value = 104 },
    { .name = "gpio_avm_led_power_red", .value = 105 },
    { .name = "gpio_avm_led_power",     .value = 106 },
    { .name = "gpio_avm_extphy1_reset", .value = 108 },
    { .name = "gpio-usb_reset",         .value = 109, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_usb_reset",     .value = 109 },
    { .name = "gpio_avm_dect_reset",    .value = 110 },
    { .name = "gpio_avm_lte_ant_a",     .value = 111 },
    { .name = "gpio_avm_lte_ant_b",     .value = 112 },
    { .name = "shift_register_size",    .value = 16 },

    { .name = "gpio_avm_dect_rd",       .value = 9  },
    { .name = "gpio_slic_pcm_fs",       .value = 12 },
    { .name = "gpio_slic_pcm_clk",      .value = 13 },
    { .name = "gpio_slic_pcm_do",       .value = 14 },
    { .name = "gpio_slic_pcm_di",       .value = 15 },
    { .name = "usb_hub_available",      .value = 1  },
    { .name = "usb_hub_external_port1", .value = 3  },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw195);


/*------------------------------------------------------------------------------------------*\
 * Fritz!Repeater 450
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw200[] = {
    { .name = "gpio_avm_button_wlan",    .value = 4, .param = avm_hw_param_gpio_in_active_high },
    { .name = "gpio_avm_led_lan",        .value = 13, .param = avm_hw_param_gpio_out_active_low},
    { .name = "gpio_avm_led_wlan",       .value = 15, .param = avm_hw_param_gpio_out_active_low},

    { .name = "gpio_avm_uart_rx",        .value = 9  },
    { .name = "gpio_avm_uart_tx",        .value = 10 },
    { .name = "gpio_avm_extphy1_reset",  .value = 11, .param = avm_hw_param_gpio_out_active_low},
    { .name = "gpio_avm_mdio_clk",       .value = 12 },
    { .name = "gpio_avm_led_fsi0",       .value = 200 },
    { .name = "gpio_avm_led_power",      .value = 14, .param = avm_hw_param_gpio_out_active_high},
    { .name = "gpio_avm_led_fsi1",       .value = 201 },
    { .name = "gpio_avm_led_fsi2",       .value = 16, .param = avm_hw_param_gpio_out_active_low},
    { .name = "gpio_avm_led_fsi3",       .value = 17, .param = avm_hw_param_gpio_out_active_low},
    { .name = "gpio_avm_led_fsi4",       .value = 18, .param = avm_hw_param_gpio_out_active_low},
    { .name = "gpio_avm_mdio_data",      .value = 19 },

    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw200);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Powerline 540E HW-Sub-Rev 1
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw201_subrev_1[] = {
    { .name = "gpio_avm_button_wlan",       .value = 21, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_button_led_standby",.value = 14, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_button_plc",        .value = 16, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_display",       .value = 17, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_wlan",          .value = 13, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_plc",           .value = 19, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_relay",             .value = 12 },
    { .name = "gpio_avm_prolific_ac_mode",  .value =  4 },
    { .name = "gpio_avm_prolific_ac_reset", .value = 15 },
    { .name = "gpio_avm_reset_plc",         .value = 20 },
    { .name = "uart_enable_ath_hi",         .value =  2 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw201_subrev_1);

/*------------------------------------------------------------------------------------------*\
 * FRITZ!Powerline 540E default (HW-Sub-Rev > 1) => nur noch 2 Taster
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw201[] = {
    { .name = "gpio_avm_button_wlan",       .value = 21, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_button_plc",        .value = 16, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_wlan",          .value = 13, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_led_plc",           .value = 19, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_relay",             .value = 12 },
    { .name = "gpio_avm_reset_plc",         .value = 20 },
    { .name = "uart_enable_ath_hi",         .value =  2 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw201);

/*------------------------------------------------------------------------------------------*\
 * Fritz!Repeater DVBC
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw205[] = {
    { .name = "gpio_avm_button_wlan",    .value = 4, .param = avm_hw_param_gpio_in_active_low },
    { .name = "gpio_avm_led_lan",        .value = 200 },
    { .name = "gpio_avm_led_wlan",       .value = 201 },

    { .name = "gpio_avm_uart_rx",        .value = 9  },
    { .name = "gpio_avm_uart_tx",        .value = 10 },
    { .name = "gpio_avm_extphy1_reset",  .value = 11, .param = avm_hw_param_gpio_out_active_low},
    { .name = "gpio_avm_mdio_clk",       .value = 12 },
    { .name = "gpio_avm_led_fsi0",       .value = 13, .param = avm_hw_param_gpio_out_active_low},
    { .name = "gpio_avm_led_power",      .value = 14, .param = avm_hw_param_gpio_out_active_low},
    { .name = "gpio_avm_led_fsi1",       .value = 15, .param = avm_hw_param_gpio_out_active_low},
    { .name = "gpio_avm_i2c_clk",        .value = 16 },
    { .name = "gpio_avm_i2c_data",       .value = 17 },
    { .name = "gpio_avm_led_fsi4",       .value = 18, .param = avm_hw_param_gpio_out_active_low},
    { .name = "gpio_avm_mdio_data",      .value = 19 },
    { .name = "usb_hub_available",       .value = 1  },

    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw205);


/*------------------------------------------------------------------------------------------*\
 * Fritz!Repeater 11ac
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw206[] = {
    { .name = "gpio_avm_shift_clk",             .value = 14, .param = avm_hw_param_gpio_out_active_low },
    { .name = "gpio_avm_shift_din",             .value = 15, .param = avm_hw_param_gpio_in_active_low  },
    { .name = "gpio_avm_led_fsi0",              .value = 100 },
    { .name = "gpio_avm_led_fsi1",              .value = 101 },
    { .name = "gpio_avm_led_fsi2",              .value = 102 },
    { .name = "gpio_avm_led_fsi3",              .value = 103 },
    { .name = "gpio_avm_led_fsi4",              .value = 104 },
    { .name = "gpio_avm_led_lan",               .value = 105 },
    { .name = "gpio_avm_led_wlan",              .value = 106 },
    { .name = "shift_register_size",            .value = 8 },
    { .name = "gpio_avm_led_power",             .value = 13  },
    { .name = "gpio_avm_button_wlan",           .value = 4   },

    { .name = "gpio_avm_uart_rx",               .value = 9   },
    { .name = "gpio_avm_uart_tx",               .value = 10  },

    { .name = "gpio_avm_extphy1_reset",         .value = 11  },
    { .name = "gpio_avm_mdio_clk",              .value = 12, },
    { .name = "gpio_avm_mdio_data",             .value = 19  },

    { .name = "gpio_avm_peregrine_reset",       .value = 17 },
    { .name = "gpio_avm_pcie_reset",            .value = 18 },

    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw206);


/*------------------------------------------------------------------------------------------*\
 * Anmerkung: spezifische Hardware-Subrevisions müssen vor der Default-Subrevision stehen
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config_table avm_hw_config_tables[] = {
    { .hwrev = 173,                .table = avm_hardware_config_hw173 },          /*--- FRITZ!WLAN Repeater 300E (Virian) ---*/
    { .hwrev =  96,                .table = avm_hardware_config_hw96  },          /*--- FRITZ!Box ATH DB120 ---*/
    { .hwrev = 180,                .table = avm_hardware_config_hw180 },          /*--- FRITZ!Box 6810 LTE (ab C-Karten) ---*/
    { .hwrev = 184,                .table = avm_hardware_config_hw184 },          /*--- FRITZ!Box 6841 LTE ---*/
    { .hwrev = 190,                .table = avm_hardware_config_hw190 },          /*--- FRITZ!Powerline 546E ---*/
    { .hwrev = 194,                .table = avm_hardware_config_hw194 },          /*--- FRITZ!Repeater 310 ---*/
    { .hwrev = 195,                .table = avm_hardware_config_hw195 },          /*--- FRITZ!Box 6842 LTE ---*/
    { .hwrev = 200,                .table = avm_hardware_config_hw200 },          /*--- FRITZ!Repeater 450 ---*/
    { .hwrev = 201, .hwsubrev = 1, .table = avm_hardware_config_hw201_subrev_1 }, /*--- FRITZ!Powerline 540E  HW-Sub-Rev 1 ---*/
    { .hwrev = 201,                .table = avm_hardware_config_hw201 },          /*--- FRITZ!Powerline 540E ---*/
    { .hwrev = 205,                .table = avm_hardware_config_hw205 },          /*--- FRITZ!Repeater DVBC ---*/
    { .hwrev = 206,                .table = avm_hardware_config_hw206 },          /*--- FRITZ!Repeater 11ac ---*/
    { .hwrev = 207,                .table = avm_hardware_config_hw195 },          /*--- FRITZ!Box AVM 6842 LTE ---*/
};
EXPORT_SYMBOL(avm_hw_config_tables);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config *avm_current_hw_config = NULL;
EXPORT_SYMBOL(avm_current_hw_config);

struct _avm_hw_config *avm_get_hw_config_table(void)
{
    static struct _avm_hw_config *current_config_table = NULL;
    unsigned int hwrev, hwsubrev, i;
    char *s;

    if(current_config_table) {
        return current_config_table;
    }

    s = prom_getenv("HWRevision");
    if (s) {
        hwrev = simple_strtoul(s, NULL, 10);
    } else {
        hwrev = 0;
    } 
    s = prom_getenv("HWSubRevision");
    if (s) {
        hwsubrev = simple_strtoul(s, NULL, 10);
    } else {
        hwsubrev = 1;
    }    
    if(hwrev == 0) {
        printk("[%s] error: no HWRevision detected in environment variables\n", __FUNCTION__);
        BUG_ON(1);
        return NULL;
    }
    for(i = 0; i < sizeof(avm_hw_config_tables)/sizeof(struct _avm_hw_config_table); i++) {
        if(avm_hw_config_tables[i].hwrev == hwrev) {
            if((avm_hw_config_tables[i].hwsubrev == 0) || (avm_hw_config_tables[i].hwsubrev == hwsubrev)) {
                current_config_table = avm_hw_config_tables[i].table;
                return current_config_table;
            }
        }
    }
    printk("[%s] error: No hardware configuration defined for HWRevision %d\n", __FUNCTION__, hwrev);
    BUG_ON(1);
    return NULL;
}
EXPORT_SYMBOL(avm_get_hw_config_table);

