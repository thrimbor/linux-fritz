#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/prom.h>
#include <linux/avm_hw_config.h>

#include <ifx_board.h>
#include <ifx_gpio.h>

#include "avm_hw_config_hw192.h"
#include "avm_hw_config_hw198.h"


extern void prom_printf(const char * fmt, ...);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config_table avm_hw_config_tables[] = {
    { .hwrev= 192, .table = avm_hardware_config_hw192 },   /*--- FRITZ!Box Italo ---*/
    { .hwrev= 198, .table = avm_hardware_config_hw198 },   /*--- FRITZ!Box Italo R ---*/
};
EXPORT_SYMBOL(avm_hw_config_tables);



/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_hw_config *avm_current_hw_config = NULL;
EXPORT_SYMBOL(avm_current_hw_config);


#define GPIO_BOARD_CONFIG_SIZE 100  /*--- Last is reserved for marking end of config ---*/
struct ifx_gpio_ioctl_pin_config g_board_gpio_pin_map[GPIO_BOARD_CONFIG_SIZE];
EXPORT_SYMBOL(g_board_gpio_pin_map);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
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
        hwsubrev = 0;
    }    
    if(hwrev == 0) {
        prom_printf("[%s] error: no HWRevision detected in environment variables\n", __FUNCTION__);
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
    prom_printf("[%s] error: No hardware configuration defined for HWRevision %d\n", __FUNCTION__, hwrev);
    BUG_ON(1);
    return NULL;
}
EXPORT_SYMBOL(avm_get_hw_config_table);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void init_gpio_config(void) 
{
    int index;
    struct _avm_hw_config *config;
    
    config = avm_get_hw_config_table();
    if(!config) {
        return; // error: no hardware config found!
    }

    index = 0;
    while(config->name) {
        if(config->manufactor_hw_config.manufactor_lantiq_gpio_config.module_id > 0) {
            if(index >= GPIO_BOARD_CONFIG_SIZE - 1) {
                prom_printf("[%s] error: Number of GPIOs in hardware config exceeds limit of %d\n", __FUNCTION__, GPIO_BOARD_CONFIG_SIZE);
                return;
            }
            g_board_gpio_pin_map[index].pin       = config->value;
            g_board_gpio_pin_map[index].module_id = config->manufactor_hw_config.manufactor_lantiq_gpio_config.module_id;
            g_board_gpio_pin_map[index].config    = config->manufactor_hw_config.manufactor_lantiq_gpio_config.config;
            index++;
            /*--- prom_printf("[%s] %d: GPIO config for %s: pin=%d module_id=0x%x config=0x%x\n", __FUNCTION__, index, config->name, config->value, ---*/
                    /*--- g_board_gpio_pin_map[index].module_id,  g_board_gpio_pin_map[index].config); ---*/
        }
        config++;
    }

    /*--- writing the end-of-gpio-config marker ---*/
    g_board_gpio_pin_map[index].module_id = IFX_GPIO_PIN_AVAILABLE;

}

