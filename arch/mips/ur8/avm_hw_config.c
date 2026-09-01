#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/prom.h>
#include <linux/avm_hw_config.h>


/*------------------------------------------------------------------------------------------*\
 * Speedport W 920V           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw135[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw135);


/*------------------------------------------------------------------------------------------*\
 * Speedport W 503V           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw136[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw136);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box WLAN 3270           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw137[] = {
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw137);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!WLAN Repeater N/G           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw138[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 11 }, 
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 }, 
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 37 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = NULL }                
};
EXPORT_SYMBOL(avm_hardware_config_hw138);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box Fon WLAN 7270 v2 16MB           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw139[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw139);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box Fon WLAN 7270 Annex A           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw141[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw141);


/*------------------------------------------------------------------------------------------*\
 * Speedport W 101 Bridge           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw143[] = {
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw143);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box Fon WLAN 7240           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw144[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw144);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box Fon WLAN 7270 v3           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw145[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw145);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box Fon WLAN 7570 vDSL           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw146[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw146);


/*------------------------------------------------------------------------------------------*\
 * Alice IAD 7570 vDSL           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw153[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw153);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box Fon WLAN 7212           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw154[] = {
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw154);


/*------------------------------------------------------------------------------------------*\
 * Speedport W 504V           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw160[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw160);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box Fon WLAN 504avm           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw164[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw164);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box Fon WLAN 7270 v4           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw167[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw167);


/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box WLAN 3270 v3           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw168[] = {
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw168);


/*------------------------------------------------------------------------------------------*\
 * Speedport W 503V AVM           
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config avm_hardware_config_hw170[] = {
    { .name = "gpio_avm_piglet_noemif_data",   .value = 13 },
    { .name = "gpio_avm_piglet_noemif_done",   .value = 41 },
    { .name = "gpio_avm_piglet_noemif_clk",    .value = 12 }, 
    { .name = "gpio_avm_piglet_noemif_prog",   .value = 39 }, 
    { .name = "gpio_avm_piglet_noemif_fpgaok", .value = 29 }, 
    { .name = "gpio_avm_dect_reset",           .value = 20 },
    { .name = NULL }
};
EXPORT_SYMBOL(avm_hardware_config_hw170);



/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config_table avm_hw_config_tables[] = {
    { .hwrev = 135, .table = avm_hardware_config_hw135 }, /*--- Speedport W 920V ---*/
    { .hwrev = 136, .table = avm_hardware_config_hw136 }, /*--- Speedport W 503V ---*/
    { .hwrev = 137, .table = avm_hardware_config_hw137 }, /*--- FRITZ!Box WLAN 3270 ---*/
    { .hwrev = 138, .table = avm_hardware_config_hw138 }, /*--- FRITZ!WLAN Repeater N/G ---*/
    { .hwrev = 139, .table = avm_hardware_config_hw139 }, /*--- FRITZ!Box Fon WLAN 7270 v2 16MB ---*/
    { .hwrev = 141, .table = avm_hardware_config_hw141 }, /*--- FRITZ!Box Fon WLAN 7270 Annex A ---*/
    { .hwrev = 143, .table = avm_hardware_config_hw143 }, /*--- Speedport W 101 Bridge ---*/
    { .hwrev = 144, .table = avm_hardware_config_hw144 }, /*--- FRITZ!Box Fon WLAN 7240 ---*/
    { .hwrev = 145, .table = avm_hardware_config_hw145 }, /*--- FRITZ!Box Fon WLAN 7270 v3 ---*/
    { .hwrev = 146, .table = avm_hardware_config_hw146 }, /*--- FRITZ!Box Fon WLAN 7570 vDSL ---*/
    { .hwrev = 153, .table = avm_hardware_config_hw153 }, /*--- Alice IAD 7570 vDSL ---*/
    { .hwrev = 154, .table = avm_hardware_config_hw154 }, /*--- FRITZ!Box Fon WLAN 7212 ---*/
    { .hwrev = 160, .table = avm_hardware_config_hw160 }, /*--- Speedport W 504V ---*/
    { .hwrev = 164, .table = avm_hardware_config_hw164 }, /*--- FRITZ!Box Fon WLAN 504avm ---*/
    { .hwrev = 167, .table = avm_hardware_config_hw167 }, /*--- FRITZ!Box Fon WLAN 7270 v4 ---*/
    { .hwrev = 168, .table = avm_hardware_config_hw168 }, /*--- FRITZ!Box WLAN 3270 v3 ---*/
    { .hwrev = 170, .table = avm_hardware_config_hw170 }, /*--- Speedport W 503V AVM ---*/
};
EXPORT_SYMBOL(avm_hw_config_tables);



/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config *avm_current_hw_config = NULL;
EXPORT_SYMBOL(avm_current_hw_config);

struct _avm_hw_config *avm_get_hw_config_table(void)
{
    unsigned int hwrev, i;
    char *s;

    s = prom_getenv("HWRevision");
    if (s) {
        hwrev = simple_strtoul(s, NULL, 10);
    } else {
        hwrev = 0;
    }    
    if(hwrev == 0) {
        printk("[%s] error: no HWRevision detected in environment variables\n", __FUNCTION__);
        BUG_ON(1);
        return NULL;
    }
    for(i = 0; i < sizeof(avm_hw_config_tables)/sizeof(struct _avm_hw_config *); i++) {
        if(avm_hw_config_tables[i].hwrev == hwrev) 
            return avm_hw_config_tables[i].table;
    }
    printk("[%s] error: No hardware configuration defined for HWRevision %d\n", __FUNCTION__, hwrev);
    BUG_ON(1);
    return NULL;
}
EXPORT_SYMBOL(avm_get_hw_config_table);

