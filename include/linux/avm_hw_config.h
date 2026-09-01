#ifndef _AVM_HW_CONFIG_H_
#define _AVM_HW_CONFIG_H_

#define AVM_HW_CONFIG_VERSION 1

#include <linux/string.h>

enum _avm_hw_param {
    avm_hw_param_no_param                   = 0,

    avm_hw_param_gpio_out_active_low        = 10,
    avm_hw_param_gpio_out_active_high       = 11,
    avm_hw_param_gpio_in_active_low         = 12,
    avm_hw_param_gpio_in_active_high        = 13,

    avm_hw_param_last_param
};

struct _avm_hw_config {
    char *name;
    int value;
    enum _avm_hw_param param;
    union _manufactor_hw_config {
        struct _manufactor_lantiq_gpio_config {
            int          module_id;  /*!< input, module ID */
            unsigned int config;     /*!< input, config flags */
        } manufactor_lantiq_gpio_config;
    } manufactor_hw_config;
};

struct _avm_hw_config_table {
    unsigned int hwrev;
    unsigned int hwsubrev;
    struct _avm_hw_config *table;
};

extern struct _avm_hw_config_table avm_hw_config_tables[];

extern struct _avm_hw_config *avm_current_hw_config;


struct _avm_hw_config *avm_get_hw_config_table(void);
    
/*------------------------------------------------------------------------------------------*\
 * Config-Wert name nicht gefunden:
 *  return -1, p_value & p_param nicht gültig
 * version != AVM_HW_CONFIG_VERSION:
 *  return -2, p_value & p_param nicht gültig
 * sonst:
 *  return 0
\*------------------------------------------------------------------------------------------*/
static inline int avm_get_hw_config(unsigned int version, const char *name, int *p_value, enum _avm_hw_param *p_param)
{
    unsigned i;

    if(version != AVM_HW_CONFIG_VERSION)
        return -2;

    if(!avm_current_hw_config) {
        avm_current_hw_config = avm_get_hw_config_table();
        if(!avm_current_hw_config)
            return -1;
    }

    for(i = 0; avm_current_hw_config[i].name; i++) {
        if(strcmp(avm_current_hw_config[i].name, name) == 0) {
            if (p_value) 
                *p_value = avm_current_hw_config[i].value;
            if(p_param)
                *p_param = avm_current_hw_config[i].param;
            return 0;
        }
    }

    return -1; /*--- name nicht gefunden ---*/
}

#endif /*--- #ifndef _AVM_HW_CONFIG_H_ ---*/

