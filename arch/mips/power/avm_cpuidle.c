/******************************************************************************
 * FILE NAME    : avm_cpuidle.c                                               *
 * COMPANY      : AVM                                                         *
 * AUTHOR       : Heiko Blobner                                               *
 * DESCRIPTION  : cpuidle interface                                           *
 *                                                                            *
 *****************************************************************************/
#include <linux/avm_printk.h>
#include <linux/cpuidle.h>
#include <ifx_pmcu.h>

/*-Defines----------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AVM_CPUIDLE_DESCRIPTION    "AVM cpuidle driver for AR9+VR9"
#define AVM_CPUIDLE_VERSION        "0.0.1"

/*-Prototypes-------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_ifx_cpuidle_enter_D0(struct cpuidle_device *dev, struct cpuidle_state *state);
int avm_ifx_cpuidle_enter_D1(struct cpuidle_device *dev, struct cpuidle_state *state);
int avm_ifx_cpuidle_enter_D2(struct cpuidle_device *dev, struct cpuidle_state *state);
int avm_ifx_cpuidle_enter_D3(struct cpuidle_device *dev, struct cpuidle_state *state);

/*-Fields-----------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct cpuidle_driver avm_ifx_cpuidle_drv = {
    .name = "AVM cpuidle", // max 16 chars (including '\0')
};

struct cpuidle_device avm_ifx_cpuidle_dev = {
    .states = {
        //--- D0
        [0] = {
            .name = "D0",
            .desc = "D0 fully on",

            .driver_data = NULL,
            .flags = CPUIDLE_FLAG_POLL | CPUIDLE_FLAG_TIME_VALID,
            .exit_latency = 0,
            .power_usage = 5000,
            .target_residency = 0,

            .enter = avm_ifx_cpuidle_enter_D0,
        },
        //--- D1
        [1] = {
            .name = "D1",
            .desc = "D1 device dependent",

            .driver_data = NULL,
            .flags = CPUIDLE_FLAG_SHALLOW | CPUIDLE_FLAG_TIME_VALID,
            .exit_latency = 10,
            .power_usage = 3000,
            .target_residency = 10,

            .enter = avm_ifx_cpuidle_enter_D1,
        },
        //--- D2
        [2] = {
            .name = "D2",
            .desc = "D2 device dependent",

            .driver_data = NULL,
            .flags = CPUIDLE_FLAG_DEEP | CPUIDLE_FLAG_TIME_VALID,
            .exit_latency = 10,
            .power_usage = 1000,
            .target_residency = 10,

            .enter = avm_ifx_cpuidle_enter_D2,
        },
        //--- D3
        [3] = {
            .name = "D3",
            .desc = "D3 Off",

            .driver_data = NULL,
            .flags = CPUIDLE_FLAG_DEEP | CPUIDLE_FLAG_TIME_VALID,
            .exit_latency = 50,
            .power_usage = 500,
            .target_residency = 500,

            .enter = avm_ifx_cpuidle_enter_D3,
        },
    },
    .state_count = 4,
};

/*-Callbacks--------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_ifx_cpuidle_enter_D0(struct cpuidle_device *dev, struct cpuidle_state *state) {
    ifx_pmcu_state_req(IFX_PMCU_MODULE_PCI,       0, IFX_PMCU_STATE_D0);
#ifdef CONFIG_AR9
    ifx_pmcu_state_req(IFX_PMCU_MODULE_SPI,       0, IFX_PMCU_STATE_D0);
#else /*--- #ifdef CONFIG_AR9 ---*/
    ifx_pmcu_state_req(IFX_PMCU_MODULE_PCIE,      0, IFX_PMCU_STATE_D0);
    ifx_pmcu_state_req(IFX_PMCU_MODULE_USIF_SPI,  0, IFX_PMCU_STATE_D0);
    ifx_pmcu_state_req(IFX_PMCU_MODULE_USIF_UART, 0, IFX_PMCU_STATE_D0);
#endif /*--- #else ---*/ /*--- #ifdef CONFIG_AR9 ---*/

	return 11; // TODO: time elapsed in µs
}

//---------
int avm_ifx_cpuidle_enter_D1(struct cpuidle_device *dev, struct cpuidle_state *state) {
    /*--- ifx_pmcu_state_req(IFX_PMCU_MODULE_PCI,       0, IFX_PMCU_STATE_D1); ---*/
#ifdef CONFIG_AR9
    /*--- ifx_pmcu_state_req(IFX_PMCU_MODULE_SPI,       0, IFX_PMCU_STATE_D1); ---*/
#else /*--- #ifdef CONFIG_AR9 ---*/
    ifx_pmcu_state_req(IFX_PMCU_MODULE_PCIE,      0, IFX_PMCU_STATE_D1);
    ifx_pmcu_state_req(IFX_PMCU_MODULE_USIF_SPI,  0, IFX_PMCU_STATE_D1);
    ifx_pmcu_state_req(IFX_PMCU_MODULE_USIF_UART, 0, IFX_PMCU_STATE_D1);
#endif /*--- #else ---*/ /*--- #ifdef CONFIG_AR9 ---*/

	return 12; // TODO: time elapsed in µs
}

//---------
int avm_ifx_cpuidle_enter_D2(struct cpuidle_device *dev, struct cpuidle_state *state) {
    /*--- ifx_pmcu_state_req(IFX_PMCU_MODULE_PCI,       0, IFX_PMCU_STATE_D2); ---*/
#ifdef CONFIG_AR9
    /*--- ifx_pmcu_state_req(IFX_PMCU_MODULE_SPI,       0, IFX_PMCU_STATE_D2); ---*/
#else /*--- #ifdef CONFIG_AR9 ---*/
    ifx_pmcu_state_req(IFX_PMCU_MODULE_PCIE,      0, IFX_PMCU_STATE_D2);
    ifx_pmcu_state_req(IFX_PMCU_MODULE_USIF_SPI,  0, IFX_PMCU_STATE_D2);
    ifx_pmcu_state_req(IFX_PMCU_MODULE_USIF_UART, 0, IFX_PMCU_STATE_D2);
#endif /*--- #else ---*/ /*--- #ifdef CONFIG_AR9 ---*/

	return 13; // TODO: time elapsed in µs
}

//---------
int avm_ifx_cpuidle_enter_D3(struct cpuidle_device *dev, struct cpuidle_state *state) {
    ifx_pmcu_state_req(IFX_PMCU_MODULE_PCI,       0, IFX_PMCU_STATE_D3);
#ifdef CONFIG_AR9
    ifx_pmcu_state_req(IFX_PMCU_MODULE_SPI,       0, IFX_PMCU_STATE_D3);
#else /*--- #ifdef CONFIG_AR9 ---*/
    ifx_pmcu_state_req(IFX_PMCU_MODULE_PCIE,      0, IFX_PMCU_STATE_D3);
    ifx_pmcu_state_req(IFX_PMCU_MODULE_USIF_SPI,  0, IFX_PMCU_STATE_D3);
    ifx_pmcu_state_req(IFX_PMCU_MODULE_USIF_UART, 0, IFX_PMCU_STATE_D3);
#endif /*--- #else ---*/ /*--- #ifdef CONFIG_AR9 ---*/

	return 14; // TODO: time elapsed in µs
}

/*-Module-----------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avm_ifx_cpuidle_init(void) {
    //---
    avm_ifx_cpuidle_dev.safe_state = &avm_ifx_cpuidle_dev.states[1];

    if(cpuidle_register_driver(&avm_ifx_cpuidle_drv)) {
        printk(KERN_ERR "Error: Not possible to register AVM cpuidle driver.");
        goto err_drv;
    }

    if(cpuidle_register_device(&avm_ifx_cpuidle_dev)) {
        printk(KERN_ERR "Error: Not possible to register AVM cpuidle device.");
        goto err_dev;
    }

    AVM_PRINTK_MSG("%s v%s started", AVM_CPUIDLE_DESCRIPTION, AVM_CPUIDLE_VERSION);

    return;

err_dev:
    cpuidle_unregister_device(&avm_ifx_cpuidle_dev);
err_drv:
    cpuidle_unregister_driver(&avm_ifx_cpuidle_drv);

    return;
}

//---------
static void avm_ifx_cpuidle_exit(void) {
    cpuidle_unregister_device(&avm_ifx_cpuidle_dev);

    AVM_PRINTK_MSG("%s v%s terminated", AVM_CPUIDLE_DESCRIPTION, AVM_CPUIDLE_VERSION);
}

module_init(avm_ifx_cpuidle_init);
module_exit(avm_ifx_cpuidle_exit);

