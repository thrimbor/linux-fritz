#include <linux/kernel.h>
#include <linux/avm_kernel_config.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_kernel_config **avm_kernel_config;
struct _kernel_modulmemory_config  *kernel_modulmemory_config;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void init_avm_kernel_config(void) {
    struct _avm_kernel_config *P;

    if(init_avm_kernel_config_ptr()) {
        printk(KERN_ERR "[%s] AVM Kernel Config failed\n", __FUNCTION__);
        return;
    }
    printk(KERN_ERR "[%s] AVM Kernel Config (ptr %p)\n", __FUNCTION__, avm_kernel_config);
    P = *avm_kernel_config;

    while(P->tag <= avm_kernel_config_tags_last) {
        if(P->config == NULL)
            return;
        switch(P->tag) {
            case avm_kernel_config_tags_undef:
                printk(KERN_ERR "[%s] AVM Kernel Config: undef entry\n", __FUNCTION__);
                break;
            case avm_kernel_config_tags_modulememory:
                printk(KERN_ERR "[%s] AVM Kernel Config: module memory entry\n", __FUNCTION__);
                kernel_modulmemory_config = (struct _kernel_modulmemory_config *)(P->config);
                break;
            case avm_kernel_config_tags_last:
                printk(KERN_ERR "[%s] AVM Kernel Config: module memory entry\n", __FUNCTION__);
                return;
            case avm_kernel_config_tags_avmnet:
                printk(KERN_ERR "[%s] AVM Kernel Config: unhandled avmnet entry\n", __FUNCTION__);
                break;
            case avm_kernel_config_tags_hw_config:
                printk(KERN_ERR "[%s] AVM Kernel Config: unhandled hw_config entry\n", __FUNCTION__);
                break;
            case avm_kernel_config_tags_cache_config:
                printk(KERN_ERR "[%s] AVM Kernel Config: unhandled cache_config entry\n", __FUNCTION__);
                break;
        }
        P++;
    }
    printk(KERN_ERR "[%s] AVM Kernel Config: internal error, should not be reached.\n", __FUNCTION__);
}
