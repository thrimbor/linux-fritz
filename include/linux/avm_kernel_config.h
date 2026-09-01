#ifndef _INCLUDE_LINUX_AVM_KERNEL_CONFIG_H_
#define _INCLUDE_LINUX_AVM_KERNEL_CONFIG_H_

#include <linux/err.h>

enum _avm_kernel_config_tags {
    avm_kernel_config_tags_undef,
    avm_kernel_config_tags_modulememory,
    avm_kernel_config_tags_avmnet,
    avm_kernel_config_tags_hw_config,
    avm_kernel_config_tags_cache_config,
    avm_kernel_config_tags_last
};

struct _kernel_modulmemory_config { 
    char *name; 
    unsigned int size; 
};

struct _avm_kernel_config {
    enum _avm_kernel_config_tags tag;
    void *config;
};

#ifndef COMPILE_EXTERNAL_CODE
extern struct _avm_kernel_config **avm_kernel_config;
extern struct _kernel_modulmemory_config  *kernel_modulmemory_config;

static inline int init_avm_kernel_config_ptr(void) {
    extern unsigned int __avm_kernel_config_start __attribute__ ((weak));
    if(IS_ERR(&__avm_kernel_config_start) || (&__avm_kernel_config_start == NULL))
        return -1;
    avm_kernel_config = (struct _avm_kernel_config **)&__avm_kernel_config_start;
    return 0;
}

void init_avm_kernel_config(void);
#endif /*--- #ifndef COMPILE_EXTERNAL_CODE ---*/

#endif /*--- #ifndef _INCLUDE_LINUX_AVM_KERNEL_CONFIG_H_ ---*/
