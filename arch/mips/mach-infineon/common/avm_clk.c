#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>

#include <ifx_clk.h>
#include <common_routines.h>
#include <asm/mach_avm.h>

/*--------------------------------------------------------------------------------*\
 * compatibility with avm-sdk
\*--------------------------------------------------------------------------------*/
unsigned int lantiq_get_clock(enum _avm_clock_id clock_id){
    switch(clock_id) {
        case avm_clock_id_cpu:
            return ifx_get_cpu_hz();
        case avm_clock_id_fpi:
            return ifx_get_fpi_hz();
        case avm_clock_id_ddr:
            return ifx_get_ddr_hz();
        case avm_clock_id_usb:
             return cgu_get_usb_clock();
        case avm_clock_id_pci:
            return cgu_get_pci_clock();
        case avm_clock_id_ephy:
            return cgu_get_ethernet_clock();
        case avm_clock_id_pp32:
            return cgu_get_pp32_clock();
        default:
            break;
    }
    return 0;
}
EXPORT_SYMBOL(lantiq_get_clock);
