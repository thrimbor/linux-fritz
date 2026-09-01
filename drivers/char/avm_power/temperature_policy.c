#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/avm_power.h>
#include <linux/avm_debug.h>
#include <linux/ar7wdt.h>
#include <linux/stop_machine.h>
#include <asm/processor.h>
#include <asm/irq.h>

#include "avm_power.h"
#include <linux/env.h>

#include <linux/avm_hw_config.h>
#include <ifx_gpio.h>

extern int ifx_ts_get_temp(int *p_temp);
extern void r4k_wait_irqoff(void);
/*--------------------------------------------------------------------------------*\
 * Condition-Table
\*--------------------------------------------------------------------------------*/
struct _generic_cond_statetab {
    int lower_val;
    unsigned long lower_timeout;
    unsigned int lower_signal;
    int higher_val;
    unsigned long higher_timeout;
    unsigned int higher_signal;
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _generic_cond_state {
    struct _generic_cond_statetab *tabgcs;
    unsigned int entries; 
    unsigned long lowerlast_time;
    unsigned long higherlast_time;
    unsigned int actindex; 
};

/*--------------------------------------------------------------------------------*\
 * ret: Indexchange: 0, -1, 1
\*--------------------------------------------------------------------------------*/
static inline int __generic_change_cond(struct _generic_cond_state *pgcs, struct _generic_cond_statetab *acttab, int val, unsigned int time) {

    if(val <= acttab->lower_val) {
        pgcs->higherlast_time = 0;
        if(pgcs->lowerlast_time == 0) {
            pgcs->lowerlast_time = time;
            /*--- printk("%s#l: val=%d <= %d, time=%lu\n", __func__, val, acttab->lower_val, pgcs->lowerlast_time); ---*/
        }
        if((time - pgcs->lowerlast_time) >= acttab->lower_timeout) {
            pgcs->lowerlast_time = 0;
            return -1;
        }
    } else if(val >= acttab->higher_val) {
        pgcs->lowerlast_time = 0;
        if(pgcs->higherlast_time == 0) {
            pgcs->higherlast_time = time;
            /*--- printk("%s#h: val=%d >= %d, time %lu\n", __func__, val, acttab->higher_val, pgcs->higherlast_time); ---*/
        }
        if((time - pgcs->higherlast_time) >= acttab->higher_timeout) {
            pgcs->higherlast_time = 0;
            return 1;
        }
    }
    return 0;
}
/*--------------------------------------------------------------------------------*\
 * ret: signal oder 0
\*--------------------------------------------------------------------------------*/
static unsigned int generic_change_cond(struct _generic_cond_state *pgcs, signed int val, unsigned long time) {
    unsigned int ret;
    /*--- printk("%s: index=%u, val=%d, time=%lu\n", __func__, pgcs->actindex, val, time); ---*/
    ret = __generic_change_cond(pgcs, &pgcs->tabgcs[pgcs->actindex], val, time);
    switch(ret) {
        case -1: 
            if(pgcs->actindex == 0) {
                ret = 0;
                break;
            }
            ret = pgcs->tabgcs[pgcs->actindex--].lower_signal;
            /*--- printk("%s: change to lower index = %u sig=%x\n", __func__, pgcs->actindex, ret); ---*/
            break;
        case 1:
            if((pgcs->actindex + 1) >= pgcs->entries) {
                ret = 0;
                break;
            }
            ret = pgcs->tabgcs[pgcs->actindex++].higher_signal;
            /*--- printk("%s: change to higher index = %u sig=%x\n", __func__, pgcs->actindex, ret); ---*/
            break;
        default:
            ret = 0;
            break;
    }
    return ret;
}
#define SEND_CRASHREPORT    0x1000
#define STOP_MACHINE        0x2000

/*--- #define TEST_TEMP_POLICY ---*/
/*--------------------------------------------------------------------------------*\
 * Conditions:
 * aktuelle Temperatur < lower_val  und das lower_timeout lang: aktuelles lower_signal schicken (falls index != 0), dann Index erniedrigen
 * aktuelle Temperatur > higher_val und das higher_timeout lang: aktuelles higher_signal schicken (falls index != letzte), dann Index erhoehen 
 *
 * Beachte:
 * lower_val  fuer ERSTEN Eintrag fuehrt NIE zu einem lower_signal 
 * higher_val fuer LETZTEN Eintrag fuehrt NIE zu einem higher_signal
\*--------------------------------------------------------------------------------*/
static struct _generic_cond_statetab temperature_table[] =  {
#if defined(TEST_TEMP_POLICY)
#define CHILLED_TEMPERATURE    67
    {lower_val:  0,  lower_timeout:  0 * HZ, lower_signal: 0,                        higher_val: 67, higher_timeout: 20 * HZ,  higher_signal: ETH_THROTTLE_DEMAND_POLICY},
    {lower_val: 67,  lower_timeout: 20 * HZ, lower_signal: ETH_NORMAL_DEMAND_POLICY, higher_val: 69, higher_timeout: 20 * HZ,  higher_signal: SEND_CRASHREPORT },
    {lower_val:  0,  lower_timeout:  0 * HZ, lower_signal: 0,                        higher_val:  0, higher_timeout: 20 * HZ,  higher_signal: STOP_MACHINE},
    {lower_val:  0,  lower_timeout:  0 * HZ, lower_signal: 0,                        higher_val:  0, higher_timeout:  0 * HZ,  higher_signal: 0}, /*--- sonst wird higher_signal nicht gesendet ---*/
#else/*--- #if 0 ---*/
#define CHILLED_TEMPERATURE    80
    {lower_val:   0, lower_timeout:  0 * 60 * HZ, lower_signal: 0,                        higher_val:102, higher_timeout: 6 * 60 * HZ,  higher_signal: ETH_THROTTLE_DEMAND_POLICY},
    {lower_val:  96, lower_timeout: 90 * 60 * HZ, lower_signal: ETH_NORMAL_DEMAND_POLICY, higher_val:118, higher_timeout: 1 * 60 * HZ,  higher_signal: SEND_CRASHREPORT },
    {lower_val:   0, lower_timeout:  0 * 60 * HZ, lower_signal: 0,                        higher_val:  0, higher_timeout: 1 * 60 * HZ,  higher_signal: STOP_MACHINE},
    {lower_val:   0, lower_timeout:  0 * 60 * HZ, lower_signal: 0,                        higher_val:  0, higher_timeout: 0 * 60 * HZ,  higher_signal: 0}, /*--- sonst wird higher_signal nicht gesendet ---*/
#endif/*--- #else ---*//*--- #if 0 ---*/
};

#define ARRAY_EL(a) (sizeof(a) / sizeof((a)[0]))
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _generic_cond_state temperature_cond_state = {
    tabgcs:  temperature_table,
    entries: ARRAY_EL(temperature_table),
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _led_list {
        char *name;
        unsigned int  gpio;
} led_list[] = {
        { name: "gpio_avm_led_info",    gpio: (unsigned int)-1},
        { name: "gpio_avm_led_pppoe",   gpio: (unsigned int)-1},
        { name: "gpio_avm_led_wlan",    gpio: (unsigned int)-1},
        { name: "gpio_avm_led_lan_all", gpio: (unsigned int)-1},
        { name: "gpio_avm_led_power",   gpio: (unsigned int)-1},
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void init_leds(void) {
    enum _avm_hw_param param;
    int gpio;
    unsigned int i;
    for( i= 0; i < ARRAY_EL(led_list); i++) {
        if(avm_get_hw_config(AVM_HW_CONFIG_VERSION, led_list[i].name, &gpio, &param) == 0) {
            led_list[i].gpio = gpio;
        }
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void toogle_leds(void) {
    static int status;
    unsigned int i;

    for( i= 0; i < ARRAY_EL(led_list); i++) {
        if(led_list[i].gpio < 0x100) {
            if(status) {
                ifx_gpio_output_clear(led_list[i].gpio, IFX_GPIO_MODULE_LED);
            } else {
                ifx_gpio_output_set(led_list[i].gpio, IFX_GPIO_MODULE_LED);
            }
        }
    }
    status = !status;
}
#define USEC_TO_CLK(a) ((a) * 250U)
#define MSEC_TO_CLK(a) USEC_TO_CLK((a) * 1000U)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int __try_stop_machine(void *_sref __attribute__((unused))) {
    return 0;
}
/*--------------------------------------------------------------------------------*\
Whole machine is stopped with interrupts off when this runs.
\*--------------------------------------------------------------------------------*/
static void machine_stopped(void) {
    int temperature, old_temperature = 0;
    unsigned int last_cycle = 0;
    local_irq_disable();
    dvpe();
    restore_printk();
    /*--- __printk(KERN_ERR"%s cpu=%x\n", __func__, smp_processor_id());  ---*/
    init_leds();
    AVM_WATCHDOG_disable(0, NULL, 0);
    for(;;) {
        if((read_c0_count() - last_cycle) > MSEC_TO_CLK(1000U)) {
            last_cycle = read_c0_count();
            jiffies   += HZ;
            ifx_ts_get_temp(&temperature);
            temperature /= 10;
            if(temperature < CHILLED_TEMPERATURE) {
                panic("chilled: t%d\n", temperature);
            }
            if(abs(temperature - old_temperature) > 3) {
                printk(KERN_ERR"[%x]<machine stopped:t%d>\n", smp_processor_id(), temperature);
                old_temperature = temperature;
            }
            toogle_leds();
        }
        if(cpu_wait != r4k_wait_irqoff) {
            continue;
        }
        write_c0_compare(read_c0_count() + MSEC_TO_CLK(1000U));
        cpu_wait();
	}
}
static int temperature_flag = 0x1;
/*--------------------------------------------------------------------------------*\
 * ret ==  ETH_NORMAL_DEMAND_POLICY
 * ret ==  ETH_THROTTLE_DEMAND_POLICY
 * ret ==  0:  nothing todo
\*--------------------------------------------------------------------------------*/
unsigned int temperature_policy(signed int temperature) {
    unsigned int ret;
    if(unlikely(temperature_flag)) {
        char *kernel_args;
        if(temperature_flag & 0x2) {
            return 0;
        }
        kernel_args= prom_getenv("kernel_args");
        if(kernel_args && strstr(kernel_args, "no_temperature_policy")) {
            printk(KERN_ERR"%s: attention: no policy is set\n", __func__);
            temperature_flag = 0x2;
            return 0;
        }
        temperature_flag = 0;
    }
    ret = generic_change_cond(&temperature_cond_state, temperature, jiffies);
    switch(ret) {
        case SEND_CRASHREPORT:
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
            display_pminfostat("stop");
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/
            avm_DebugSignalLog(1, "send crashreport and stop t%d\n", temperature);
            ret = 0;
            break;
        case STOP_MACHINE:
            printk(KERN_ERR"%s: stop machine t%d\n", __func__, temperature);
            stop_machine(__try_stop_machine, NULL, NULL);
            machine_stopped(); /*--- never return ---*/
            break;
        case ETH_THROTTLE_DEMAND_POLICY:
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
            display_pminfostat("throttle eth");
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/
            avm_DebugSignalLog(1, "throttle eth t%d\n", temperature);
            break;
        case ETH_NORMAL_DEMAND_POLICY:
            printk(KERN_ERR"%s: normal eth t%d\n", __func__, temperature);
            break;
        default:
            ret = 0;
            break;
    }
    return ret;
}
