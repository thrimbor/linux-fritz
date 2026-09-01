/******************************************************************************
 * FILE NAME    : avm_cpuidle.c                                               *
 * COMPANY      : AVM                                                         *
 * AUTHOR       : Christoph Buettner                                          *
 * DESCRIPTION  : cpufreq interface                                           *
 *                                                                            *
 *****************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <asm/mach_avm.h>
#include <common_routines.h>

/*-Defines----------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AVM_CPUFREQ_DESCRIPTION    "AVM cpufreq driver for AR9+VR9+AR10"
#define AVM_CPUFREQ_VERSION        "0.0.1"


static struct cpufreq_driver avm_cpufreq__cfreq_driver;

#if defined(CONFIG_AR9)
static struct cpufreq_frequency_table avm_cpufreq__frequency_table[] =
{
    { .frequency = 393215 , .index = 0 }, 
    { .frequency = 333333 , .index = 1 }, 
    { .frequency = 111111 , .index = 2 }, 
    { .frequency = CPUFREQ_TABLE_END }
};
#elif defined(CONFIG_VR9)
static struct cpufreq_frequency_table avm_cpufreq__frequency_table[] =
{
    { .frequency = 500000 , .index = 0 }, 
    { .frequency = 393215 , .index = 1 }, 
    { .frequency = 333333 , .index = 2 }, 
    { .frequency = 125000 , .index = 3 }, 
    { .frequency = CPUFREQ_TABLE_END }
};
#else /*--- AR10 ---*/
static struct cpufreq_frequency_table avm_cpufreq__frequency_table[] =
{
    { .frequency = 500000 , .index = 0 }, 
    { .frequency = 250000 , .index = 1 }, 
    { .frequency = 125000 , .index = 2 }, 
    { .frequency = CPUFREQ_TABLE_END }
};
#endif

static int avm_cpufreq__target(struct cpufreq_policy *policy, unsigned int target_freq, unsigned int relation) {
    unsigned int tab_index = 23;
    unsigned int old_tab_index = 23;
    unsigned int new_speed_khz = 0;
    unsigned int new_speed_mhz = 0;
    unsigned int ddr_speed_mhz = 0;
    struct cpufreq_freqs freqs;
    unsigned int curr_speed_khz = ifx_get_cpu_hz() / 1000;

    cpufreq_frequency_table_target(policy, avm_cpufreq__frequency_table, target_freq, relation, &tab_index);
    /*--- printk(KERN_ERR "[%s] cpufreq_frequency_table_target returned tab_index=%d \n", __FUNCTION__, tab_index); ---*/
    cpufreq_frequency_table_target(policy, avm_cpufreq__frequency_table, curr_speed_khz, relation, &old_tab_index);
    /*--- printk(KERN_ERR "[%s] %d, %d, %d\n", __FUNCTION__, curr_speed_khz, curr_speed_khz*1000, old_tab_index); ---*/

    new_speed_khz = avm_cpufreq__frequency_table[tab_index].frequency;
    /*--- printk(KERN_ERR "[%s:%d] setting frequency to %d \n", __FUNCTION__, __LINE__, new_speed_khz); ---*/

    freqs.old = curr_speed_khz;
    freqs.new = new_speed_khz;
    freqs.cpu = 0;

    cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

#if defined(CONFIG_AR9)
    new_speed_mhz = new_speed_khz / 1000;

    switch(new_speed_mhz) {
        case 393:
            ddr_speed_mhz = 196;
            break;
        case 333:
            ddr_speed_mhz = 166;
            break;
        case 111:
            ddr_speed_mhz = 111;
            break;
    }
    cgu_set_clock((new_speed_mhz), ddr_speed_mhz, ddr_speed_mhz);
#elif defined(CONFIG_VR9)
    // Der vr9 muss stufenweise hochgetaktet werden, sonst stuerzt er ab.
    // Deshalb der Aufwand hier...
    if(tab_index >= old_tab_index) {
        new_speed_mhz = new_speed_khz / 1000;

        switch(new_speed_mhz) {
            case 500:
                ddr_speed_mhz = 250;
                break;
            case 393:
                ddr_speed_mhz = 196;
                break;
            case 333:
                ddr_speed_mhz = 167;
                break;
            case 125:
                ddr_speed_mhz = 125;
                break;
        }

        cgu_set_clock((new_speed_mhz), ddr_speed_mhz, ddr_speed_mhz);
    } else {
        do {
            old_tab_index--;
            new_speed_khz = avm_cpufreq__frequency_table[old_tab_index].frequency;
            /*--- printk(KERN_ERR "[%s:%d] setting frequency to %d \n", __FUNCTION__, __LINE__, new_speed_khz); ---*/

            freqs.new = new_speed_khz;
            new_speed_mhz = new_speed_khz / 1000;

            switch(new_speed_mhz) {
                case 500:
                    ddr_speed_mhz = 250;
                    break;
                case 393:
                    ddr_speed_mhz = 196;
                    break;
                case 333:
                    ddr_speed_mhz = 167;
                    break;
                case 125:
                    ddr_speed_mhz = 125;
                    break;
            }

            cgu_set_clock((new_speed_mhz), ddr_speed_mhz, ddr_speed_mhz);
        }while(old_tab_index>tab_index);
    }
#else /*--- AR10 ---*/
    // Der ar10 stufenweise hoch- und runtertakten...
    if(tab_index >= old_tab_index) {
        new_speed_mhz = new_speed_khz / 1000;

        switch(new_speed_mhz) {
            case 500:
                ddr_speed_mhz = 250;
                break;
            case 250:
                ddr_speed_mhz = 125;
                break;
            case 125:
                ddr_speed_mhz = 125;
                break;
        }

        cgu_set_clock((new_speed_mhz), ddr_speed_mhz, ddr_speed_mhz);
    } else {
        do {
            old_tab_index--;
            new_speed_khz = avm_cpufreq__frequency_table[old_tab_index].frequency;
            /*--- printk(KERN_ERR "[%s:%d] setting frequency to %d \n", __FUNCTION__, __LINE__, new_speed_khz); ---*/

            freqs.new = new_speed_khz;
            new_speed_mhz = new_speed_khz / 1000;

            switch(new_speed_mhz) {
                case 500:
                    ddr_speed_mhz = 250;
                    break;
                case 250:
                    ddr_speed_mhz = 125;
                    break;
                case 125:
                    ddr_speed_mhz = 125;
                    break;
            }

            cgu_set_clock((new_speed_mhz), ddr_speed_mhz, ddr_speed_mhz);
        }while(old_tab_index>tab_index);
    }

#endif
    cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

    return 0;
}

static int __init avm_cpufreq__cpu_init(struct cpufreq_policy *policy) {
    if (policy->cpu != 0)
        return -EINVAL;
    /*--- policy->cpuinfo.transition_latency = CPUFREQ_ETERNAL; ---*/
    policy->cpuinfo.transition_latency = 500000; /* latency in ns = 500µs*/
    cpufreq_frequency_table_cpuinfo(policy, avm_cpufreq__frequency_table);
    return 0;
}

int avm_cpufreq__verify_speed(struct cpufreq_policy *policy) {
    return cpufreq_frequency_table_verify(policy, avm_cpufreq__frequency_table);
}

unsigned int avm_cpufreq__getspeed(unsigned int cpu) {
    if (cpu != 0){
        return -EINVAL;
    } else {
        return ifx_get_cpu_hz() / 1000;
    }
}

static struct cpufreq_driver avm_cpufreq__cfreq_driver = {
    .flags  = CPUFREQ_STICKY,
    .verify = avm_cpufreq__verify_speed,
    .target = avm_cpufreq__target,
    .get    = avm_cpufreq__getspeed,
    .init   = avm_cpufreq__cpu_init,
    .name   = "avm_cpufreq",
};


static int __init avm_cpufreq__register_cpufreq(void) {
    printk(KERN_ERR "Registering %s v%s\n", AVM_CPUFREQ_DESCRIPTION, AVM_CPUFREQ_VERSION);
    return cpufreq_register_driver(&avm_cpufreq__cfreq_driver);
}

late_initcall(avm_cpufreq__register_cpufreq);

