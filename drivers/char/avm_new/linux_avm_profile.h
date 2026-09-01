/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2006 AVM GmbH <fritzbox_info@avm.de>
 *
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
\*------------------------------------------------------------------------------------------*/
#ifndef _avm_profile_h_
#define _avm_profile_h_

#define AVM_PROFILING_VERSION   3
#ifndef CONFIG_AVM_PROFILING_TRACE_MODE 
#define CONFIG_AVM_PROFILING_TRACE_MODE         50
#endif /*--- #ifndef CONFIG_AVM_PROFILING_TRACE_MODE ---*/ 

#include <linux/skbuff.h>
#include <linux/version.h>

#if defined(CONFIG_AVM_SIMPLE_PROFILING)

#if defined(CONFIG_ARCH_PUMA5) || defined(CONFIG_MACH_PUMA6)
#include <asm/performance.h>
static inline unsigned int avm_profile_counter(void) {
    return read_p15_cycle_counter() << 5; /*--- / 64 * 32 = takt / 2 ->  wie get_cycle() auf dem MIPS ---*/
}

static inline unsigned int avm_profile_sdramacess(void) {
    return read_p15_performance_counter_0();
}

static inline unsigned int avm_profile_sdramactivate(void) {
    return read_p15_performance_counter_1();
}

static inline void avm_profile_counter_init(void) {
    union __performance_monitor_control C;

    write_secure_debug_enable_register(0, 1);
    C.Register = read_p15_performance_monitor_control();
    C.Bits.CycleCounterDivider = 1; /*--- / 64 * 32 = takt / 2 ->  wie get_cycle() auf dem MIPS ---*/
    C.Bits.EvtCount0 = 0xB;  /*--- Data  cache miss ---*/
    C.Bits.EvtCount1 = 0x0;  /*--- Instruction cache miss ---*/
    C.Bits.EnableCounters      = 1;
    write_p15_performance_monitor_control(C.Register);
}

static inline void avm_profile_counter_exit(void) {
#if 0
    union __performance_monitor_control C;
    C.Register = read_p15_performance_monitor_control();
    C.Bits.EvtCount0 = 0xB;  /*--- Data  cache miss ---*/
    C.Bits.EvtCount1 = 0x0;  /*--- Instuction cache miss ---*/
    write_p15_performance_monitor_control(C.Register);
#endif
}

#include <linux/skbuff.h>
#endif
#if defined(CONFIG_MIPS)
#define avm_profile_counter()   read_c0_count()

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
#if defined(CONFIG_MIPS_FUSIV) || defined(CONFIG_LANTIQ) || defined(CONFIG_MACH_ATHEROS)
#include <asm/mipsregs.h>
static inline void avm_profile_counter_init(void) { }
static inline void avm_profile_counter_exit(void) { }
static inline unsigned int avm_profile_sdramacess(void) {
    return read_c0_perfcntr0();
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned int avm_profile_sdramactivate(void) {
    return read_c0_perfcntr1();
}
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_MIPS_UR8)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28) 
#include <asm/mips-boards/ur8.h>
#include <asm/mach-ur8/hw_emif.h>
#else
#include <ur8.h>
#include <asm/mach-ur8/hw_emif.h>
#endif 
static inline void avm_profile_counter_init(void) { }
static inline void avm_profile_counter_exit(void) { }

static inline unsigned int avm_profile_sdramacess(void) {
    struct EMIF_register_memory_map *UR8_EMIF_register_memory_map = (struct EMIF_register_memory_map *)UR8_EMIF_BASE;
    return UR8_EMIF_register_memory_map->TotalAccesses; 
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned int avm_profile_sdramactivate(void) {
    struct EMIF_register_memory_map *UR8_EMIF_register_memory_map = (struct EMIF_register_memory_map *)UR8_EMIF_BASE;
    return UR8_EMIF_register_memory_map->TotalActivate; 
}
#endif 
#endif /*--- #if defined(CONFIG_MIPS) ---*/

enum _avm_profile_data_type {
    avm_profile_data_type_free = 0,
    avm_profile_data_type_text = 1,
    avm_profile_data_type_code_address_info = 2,
    avm_profile_data_type_data_address_info = 3,
    avm_profile_data_type_trace_skb = 4,
    avm_profile_data_type_hw_irq_begin = 5,
    avm_profile_data_type_hw_irq_end = 6,
    avm_profile_data_type_sw_irq_begin = 7,
    avm_profile_data_type_sw_irq_end = 8,
    avm_profile_data_type_timer_begin = 9,
    avm_profile_data_type_timer_end = 10,
    avm_profile_data_type_tasklet_begin = 11,
    avm_profile_data_type_tasklet_end = 12,
    avm_profile_data_type_hi_tasklet_begin = 13,
    avm_profile_data_type_hi_tasklet_end = 14,
    avm_profile_data_type_workitem_begin = 15,
    avm_profile_data_type_workitem_end = 16,
    avm_profile_data_type_cpphytx_begin = 17,
    avm_profile_data_type_cpphytx_end = 18,
    avm_profile_data_type_cpphyrx_begin = 19,
    avm_profile_data_type_cpphyrx_end = 20,
    avm_profile_data_type_func_begin = 21,
    avm_profile_data_type_func_end = 22,
    avm_profile_data_type_trigger_tasklet_begin = 23,
    avm_profile_data_type_trigger_tasklet_end = 24,
    avm_profile_data_type_code_address_yield = 25,
    avm_profile_data_type_unknown
};

struct _avm_profile_data {
    /* offset 0x00 */ struct task_struct *curr;
    /* offset 0x04 */ enum _avm_profile_data_type type : 8;
    /* offset 0x05 */ unsigned int cpu_id : 8;
    /* offset 0x06 */ unsigned int id : 16;
    /* offset 0x08 */ unsigned int addr;  /* obersten 3 bit geben die quelle an */
    /* offset 0x0C */ unsigned int time;
    /* offset 0x10 */ unsigned int total_access;
    /* offset 0x14 */ unsigned int total_activate;
    /* length 0x18 */
};

#define profile_DataSetsPerBlock        ((1 << PAGE_SHIFT) / sizeof(struct _avm_profile_data))
/*--- #define profile_DataSetsPerBlock        170 ---*/  /*--- 4096 / 0x18 ==> 170... */
extern unsigned int profile_BlockNeeded;
/*--- #define PROFILE_BUFFER_LEN              (profile_BlockNeeded * (1 << PAGE_SHIFT)) ---*/

#if defined(AVM_PROFILING_VERSION)
struct _simple_profiling {
    void **data;
    atomic_t pos;
    unsigned int len;
    unsigned int enabled;
    unsigned int mode;
    unsigned int mask;
    unsigned long start_time;
    unsigned long end_time;
    unsigned int wraparround;
#ifdef CONFIG_AVM_SIMPLE_PROFILING_YIELD
#define NO_YIELD_HANDLER -1
    int yield_handler;
    void *yield_ref;
    int yield_signal;
    int yield_timer_no;
#endif
    spinlock_t lock;
};
#endif/*--- #if defined(AVM_PROFILING_VERSION) ---*/

extern struct _simple_profiling simple_profiling;
#define avm_simple_profiling_is_enabled()       unlikely(simple_profiling.enabled)

extern void __avm_simple_profiling_text(const  char *text);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void avm_simple_profiling_text(const  char *text){
    if(avm_simple_profiling_is_enabled()) {
        __avm_simple_profiling_text(text);
    }
}
extern void __avm_simple_profiling_skb(unsigned int addr, unsigned int where, struct sk_buff *skb);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void avm_simple_profiling_skb(unsigned int addr, unsigned int where, struct sk_buff *skb){
    if(avm_simple_profiling_is_enabled()) {
        __avm_simple_profiling_skb(addr, where, skb);
    }
}
extern void __avm_simple_profiling_log(enum _avm_profile_data_type type, unsigned int addr, unsigned int id);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void avm_simple_profiling_log(enum _avm_profile_data_type type, unsigned int addr, unsigned int id){
    if(avm_simple_profiling_is_enabled()) {
        __avm_simple_profiling_log(type, addr, id);
    }
}
extern unsigned int __avm_simple_profiling(struct pt_regs *regs, unsigned int irq_num);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned int avm_simple_profiling(struct pt_regs *regs, unsigned int irq_num){
    if(avm_simple_profiling_is_enabled()) {
        return __avm_simple_profiling(regs, irq_num);
    }
    return 0;
}
void avm_simple_profiling_enable(unsigned int on, unsigned int mask, unsigned int *count, unsigned long *timediff);
struct _avm_profile_data *avm_simple_profiling_by_idx(unsigned int idx);
void avm_simple_profiling_restart(void);


#ifdef CONFIG_AVM_SIMPLE_PROFILING_YIELD_PCNT
extern unsigned int ack_irqs_reload_counter(void);
extern unsigned int read_c0_perfcnt(unsigned int count_reg, unsigned int tc);
#endif
#define avm_simple_profiling_disable()          simple_profiling.enabled = 0, avm_profile_counter_exit()

#else /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
#define avm_simple_profiling_text(text)
#define avm_simple_profiling_log(addr, id)
#define avm_simple_profiling(regs, irq)
#define avm_simple_profiling_enable(on, count, timediff)
#define avm_simple_profiling_is_enabled()           0
#define avm_simple_profiling_disable()
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/

#endif /*--- #ifndef _avm_profile_h_ ---*/
