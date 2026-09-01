/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * HISTORY
 * $Version $Date      $Author     $Comment
 * 2.0      07/05/09    Xu Liang
 * 1.0      19/07/07    Teh Kok How
 */



#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>

#include <asm/time.h>
#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>

#include <ifx_types.h>
#include <ifx_regs.h>
#include <ifx_pmu.h>
#include <ifx_rcu.h>
#include <ifx_gpio.h>

#include <linux/avm_hw_config.h>


extern void prom_printf(const char *, ...);
extern unsigned int ifx_get_cpu_hz(void);
extern void ifx_reboot_setup(void);
#ifdef CONFIG_KGDB
  extern int kgdb_serial_init(void);
#endif



#ifdef CONFIG_MIPS_APSP_KSPD
  unsigned long cpu_khz;
#endif



void __init bus_error_init(void)
{
    /* nothing */
}

static inline long get_counter_resolution(void)
{
#if defined(CONFIG_DANUBE) || defined(CONFIG_AR9) || defined(CONFIG_VR9) || defined(CONFIG_AR10)
    volatile long res;
    __asm__ __volatile__(
        ".set   push\n"
        ".set   noreorder\n"
        ".set   mips32r2\n"
        "rdhwr  %0, $3\n"
        "ehb\n"
        ".set pop\n"
     :  "=&r" (res)
     : /* no input */
     : "memory");
     instruction_hazard();
     return res;
#else
    return 2;
#endif
}

void __init plat_time_init(void)
{
    u32 resolution = get_counter_resolution();

    mips_hpt_frequency = ifx_get_cpu_hz() / resolution;

#ifdef CONFIG_USE_EMULATOR
    //  we do some hacking here to give illusion we have a faster counter
    //  frequency so that the time interrupt happends less frequently
    mips_hpt_frequency *= 25;
#endif

    /*--- prom_printf("mips_hpt_frequency = %d, counter_resolution = %d\n", mips_hpt_frequency, resolution); ---*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    //  timer interrupt is wired to IP7
    //  no matter what value is read by read_c0_intctl()
    cp0_compare_irq = 7;
#endif

#ifdef CONFIG_MIPS_APSP_KSPD
    cpu_khz = ifx_get_cpu_hz() / 1000;
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
/*
 * THe CPU counter for System timer, set to HZ
 * GPTU Timer 6 for high resolution timer, set to hr_time_resolution
 * Also misuse this routine to print out the CPU type and clock.
 */
void __init plat_timer_setup(struct irqaction *irq)
{
    /* cpu counter for timer interrupts */
    setup_irq(MIPS_CPU_TIMER_IRQ, irq);
}
#else
unsigned int __cpuinit get_c0_compare_int(void)
{
  return MIPS_CPU_TIMER_IRQ;
}
#endif

void __init plat_mem_setup(void)
{
    ifx_reboot_setup();
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
    board_time_init = plat_time_init;
#endif

}

static int __init power_managment_setup(void) {
    prom_printf("[%s] reset 'DSL, DFE, AFE, VOICE, DSLTC, ARC'\n", __FUNCTION__);
    ifx_rcu_rst(IFX_RCU_DOMAIN_DSLDFE, 0);
    ifx_rcu_rst(IFX_RCU_DOMAIN_DSLAFE, 0);
    ifx_rcu_rst(IFX_RCU_DOMAIN_VOICE, 0);
    ifx_rcu_rst(IFX_RCU_DOMAIN_DSLTC, 0);
    ifx_rcu_rst(IFX_RCU_DOMAIN_ARC, 0);

    prom_printf("[%s] reset 'LDO'\n", __FUNCTION__);
    ifx_rcu_rst(IFX_RCU_DOMAIN_LDO, 0);

    prom_printf("[%s] power down 'PPE TC, PPE EMA, LEDC, DFEV1, DFEV0'\n", __FUNCTION__);
    ifx_pmu_set(IFX_PMU_MODULE_PPE_TC, 0);
    ifx_pmu_set(IFX_PMU_MODULE_PPE_EMA, 0);
    ifx_pmu_set(IFX_PMU_MODULE_LEDC, 0);
    ifx_pmu_set(IFX_PMU_MODULE_DFEV1, 0);
    ifx_pmu_set(IFX_PMU_MODULE_DFEV0, 0);

    prom_printf("[%s] power down 'MSI1, PDI1 PCIE1 PCIE1_PHY'\n", __FUNCTION__);
    ifx_pmu_set(IFX_PMU_MODULE_PDI1, 0);
    ifx_pmu_set(IFX_PMU_MODULE_MSI1, 0);
    ifx_pmu_set(IFX_PMU_MODULE_PCIE1_CTRL, 0);
    ifx_pmu_set(IFX_PMU_MODULE_PCIE1_PHY, 0);
    
    ifx_pmu_set(IFX_PMU_MODULE_DSL_DFE, 0);
    prom_printf("[%s] disable power domain 'DSL + DFE'\n", __FUNCTION__);
    ifx_pmu_pg_dsl_dfe_disable();

    return 0;
}

core_initcall(power_managment_setup);

/*------------------------------------------------------------------------------------------*\
 * If there is a Peacock on this board, get it out of reset
 * before the PCIE system gets initialised
\*------------------------------------------------------------------------------------------*/
static int __init peacock_setup(void)
{
    enum _avm_hw_param pin_param = avm_hw_param_no_param;
    int reset_pin;
    int result;

    // check for Peacock and get its GPIO parameters
    result = avm_get_hw_config(AVM_HW_CONFIG_VERSION,
                               "gpio_avm_peacock_reset",
                               &reset_pin,
                               &pin_param);

    if(result == 0){
        // Peacock found, do some sanity checks
        if(pin_param != avm_hw_param_gpio_out_active_low
           && pin_param != avm_hw_param_gpio_out_active_high)
        {
            printk(KERN_ERR "[%s]: Received bogus GPIO pin parameter from avm_hw_config: %x\n", __func__, pin_param);
            return -1;
        }

        if(ifx_gpio_register(IFX_GPIO_MODULE_WLAN_PEACOCK_RESET)){
            printk(KERN_ERR "[%s]: Could not register GPIO WLAN_PEACOCK_RESET\n", __func__);
            return -1;
        }

        printk(KERN_INFO "[%s] Getting Peacock out of reset\n", __func__);
//        printk(KERN_INFO "[%s] GPIO 0x%x, active-%s\n", __func__, reset_pin, pin_param == avm_hw_param_gpio_out_active_low ? "low" : "high");
        if(pin_param == avm_hw_param_gpio_out_active_low){
            ifx_gpio_output_set(reset_pin, IFX_GPIO_MODULE_WLAN_PEACOCK_RESET);
        }else{
            ifx_gpio_output_clear(reset_pin, IFX_GPIO_MODULE_WLAN_PEACOCK_RESET);
        }

        mdelay(100);
    }

    return 0;
}
postcore_initcall(peacock_setup);
