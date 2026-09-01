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

#include <linux/version.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/avm_led.h>
#include <linux/avm_profile.h>
#include <linux/avm_debug.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <linux/kallsyms.h>
#endif
#include <asm/uaccess.h>
#if defined(CONFIG_MIPS)
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#endif
#if defined(CONFIG_ARM)
#endif
#include "avm_sammel.h"
#include "avm_led.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mach_avm.h>
#else
#endif

/*------------------------------------------------------------------------------------------*\
 * avm_led_profile.c
\*------------------------------------------------------------------------------------------*/
int avm_led_profile_init(unsigned int table_index, struct _avm_virt_led *V __attribute__((unused))) {
    switch(table_index) {
        case 0:  /* off */
            printk("[avm_led] profile_init: disable\n");
            return 1;
        case 1: /* on */
            printk("[avm_led] profile_init: enable\n");
            return 2;
        case 2: /* write CSV File */
            printk("[avm_led] profile_init: write CSV file\n");
            return 3;
        case 3: /* write ASCII File */
            printk("[avm_led] profile_init: write ASCII file\n");
            return 4;
        case 4: /* display ASCII*/
            printk("[avm_led] profile_init: display ASCII\n");
            return 5;
        case 5: /* on (wraparround-mode) */
            printk("[avm_led] profile_init: enable (wraparround-mode)\n");
            return 6;
        case 6: /* on skb trace */
            printk("[avm_led] profile_init: enable skb trace\n");
            return 7;
        case 7: /* on skb trace (wraparround-mode) */
            printk("[avm_led] profile_init: enable skb trace (wraparround-mode)\n");
            return 8;
        default:
            printk("[avm_led] profile_init: other %u\n", table_index);
            return 33;
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_profile_exit(int handle __attribute__((unused))) {
    return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_profile_stop(int handle __attribute__((unused))) {
    return;
}

#define MIPS_PERFOMANCE_HAS_MORE                    (1 << 31)
#define MIPS_PERFOMANCE_IRQ_ENABLE                  (1 << 4)
#define MIPS_PERFOMANCE_USER_MODE_ENABLE            (1 << 3)
#define MIPS_PERFOMANCE_SUPERVISOR_MODE_ENABLE      (1 << 2)
#define MIPS_PERFOMANCE_KERNEL_MODE_ENABLE          (1 << 1)
#define MIPS_PERFOMANCE_EXCEPTION_ENABLE            (1 << 0)
#define MIPS_PERFORMANCE_VPE_SPECIFIC_ENABLE        (0x01 << 20)
#define MIPS_PERFORMANCE_TC_SPECIFIC_ENABLE         (0x02 << 20)
#define MIPS_PERFORMANCE_VPEID(x)                   (((x) & 0x0F) << 16)
#define MIPS_PERFORMANCE_TCID(x)                    (((x) & 0xFF) << 22)

char *print_perfomance_counter_mode(unsigned int mode) {
    static char Buffer[128];
    sprintf(Buffer, "User-Mode %s Supervisor-Mode %s Kernel-Mode %s Exeption-Mode %s",
                            (MIPS_PERFOMANCE_USER_MODE_ENABLE & mode) ? "enabled" : "disable",
                            (MIPS_PERFOMANCE_SUPERVISOR_MODE_ENABLE & mode) ? "enabled" : "disable",
                            (MIPS_PERFOMANCE_KERNEL_MODE_ENABLE & mode) ? "enabled" : "disable",
                            (MIPS_PERFOMANCE_EXCEPTION_ENABLE & mode) ? "enabled" : "disable");
    return Buffer;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_profile_timer(unsigned long handle __attribute__((unused))) {
    return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *avm_profile_data_short_names[] = {
    "FREE", /*--- avm_profile_data_type_free, ---*/
    "TEXT", /*--- avm_profile_data_type_text, ---*/
    "CODE", /*--- avm_profile_data_type_code_address_info, ---*/
    "DATA", /*--- avm_profile_data_type_data_address_info, ---*/
    "SKB",  /*--- avm_profile_data_type_trace_skb, ---*/
    "BIRQ", /*--- avm_profile_data_type_hw_irq_begin, ---*/
    "EIRQ", /*--- avm_profile_data_type_hw_irq_end, ---*/
    "BSWI", /*--- avm_profile_data_type_sw_irq_begin, ---*/
    "ESWI", /*--- avm_profile_data_type_sw_irq_end, ---*/
    "BTIM", /*--- avm_profile_data_type_timer_begin, ---*/
    "ETIM", /*--- avm_profile_data_type_timer_end, ---*/
    "BLET", /*--- avm_profile_data_type_tasklet_begin, ---*/
    "ELET", /*--- avm_profile_data_type_tasklet_end, ---*/
    "BLHT", /*--- avm_profile_data_type_hi_tasklet_begin, ---*/
    "ELHT", /*--- avm_profile_data_type_hi_tasklet_end, ---*/
    "BWRK", /*--- avm_profile_data_type_workitem_begin, ---*/
    "EWRK", /*--- avm_profile_data_type_workitem_end, ---*/
    "BCPT", /*--- avm_profile_data_type_cpphytx_begin, ---*/
    "ECPT", /*--- avm_profile_data_type_cpphytx_end, ---*/
    "BCPR", /*--- avm_profile_data_type_cpphyrx_begin, ---*/
    "ECPR", /*--- avm_profile_data_type_cpphyrx_end, ---*/
    "BFUN", /*--- avm_profile_data_type_func_begin, ---*/
    "EFUN", /*--- avm_profile_data_type_func_end, ---*/
    "BTLT", /*--- avm_profile_data_type_trigger_tasklet_begin, ---*/
    "ETLT", /*--- avm_profile_data_type_trigger_tasklet_end, ---*/
    "ERROR", /*--- avm_profile_data_type_unknown ---*/
    NULL
};

char *avm_profile_data_long_names[] = {
    "free", /*--- avm_profile_data_type_free, ---*/
    "text", /*--- avm_profile_data_type_text, ---*/
    "code", /*--- avm_profile_data_type_code_address_info, ---*/
    "data", /*--- avm_profile_data_type_data_address_info, ---*/
    "skb",  /*--- avm_profile_data_type_trace_skb, ---*/
    "begin hw irq", /*--- avm_profile_data_type_hw_irq_begin, ---*/
    "end hw irq", /*--- avm_profile_data_type_hw_irq_end, ---*/
    "begin sw irq", /*--- avm_profile_data_type_sw_irq_begin, ---*/
    "end sw irq", /*--- avm_profile_data_type_sw_irq_end, ---*/
    "begin timer", /*--- avm_profile_data_type_timer_begin, ---*/
    "end timer", /*--- avm_profile_data_type_timer_end, ---*/
    "begin tasklet", /*--- avm_profile_data_type_tasklet_begin, ---*/
    "end tasklet", /*--- avm_profile_data_type_tasklet_end, ---*/
    "begin hitasklet", /*--- avm_profile_data_type_hi_tasklet_begin, ---*/
    "end hitasklet", /*--- avm_profile_data_type_hi_tasklet_end, ---*/
    "begin workitem", /*--- avm_profile_data_type_workitem_begin, ---*/
    "end workitem", /*--- avm_profile_data_type_workitem_end, ---*/
    "begin cpphy_tx", /*--- avm_profile_data_type_cpphytx_begin, ---*/
    "end cpphy_tx", /*--- avm_profile_data_type_cpphytx_end, ---*/
    "begin cpphy_rx", /*--- avm_profile_data_type_cpphyrx_begin, ---*/
    "end cpphy_rx", /*--- avm_profile_data_type_cpphyrx_end, ---*/
    "begin func", /*--- avm_profile_data_type_func_begin, ---*/
    "end func", /*--- avm_profile_data_type_func_end, ---*/
    "begin tasklet-trigger", /*--- avm_profile_data_type_trigger_tasklet_begin, ---*/
    "end tasklet-trigge", /*--- avm_profile_data_type_trigger_tasklet_end, ---*/
    "unknown", /*--- avm_profile_data_type_unknown ---*/
    NULL
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_MIPS
char *perfomance_counter_options[4][64] = {
#if !defined(CONFIG_CPU_MIPS_74K)
    [0] = {

        [ 0] = "Cycles",
        [ 1] = "Instructions completed",
        [ 2] = "branch instructions completed",
        [ 3] = "JR r31 (return) instructions",
        [ 4] = "JR (not r31) instructions",
        [ 5] = "ITLB accesses",
        [ 6] = "DTLB accesses",
        [ 7] = "JTLB instruction accesses",
        [ 8] = "JTLB data accesses",
        [ 9] = "Instruction Cache accesses",
        [10] = "Data cache load/stores",
        [11] = "Data cache load/store misses",
        [14] = "integer instructions completed",
        [15] = "loads completed",
        [16] = "J/JAL completed",
        [17] = "no-ops completed",
        [18] = "Main pipeline stalls",
        [19] = "SC instructions completed",
        [20] = "Prefetch instructions to cached addresses",
        [21] = "L2 cache writebacks",
        [22] = "L2 cache misses",
        [23] = "Exceptions taken",
        [24] = "cache fixup",
        [26] = "DSP Instructions Completed",
        [28] = "Impl. specific PM event",
        [29] = "Impl. specific ISPRAM event",
        [30] = "Impl. specific CorExtend event",
        [32] = "ITC loads",
        [33] = "Uncached loads",
        [34] = "fork instructions completed",
        [35] = "CP2 register-to-register Instns Completed",
        [37] = "I$ Miss Stall cycles",
        [38] = "L2 I-miss stall cycles",
        [39] = "D$ miss cycles",
        [40] = "Uncached access block cycles",
        [41] = "MDU stall cycles",
        [42] = "CP2 stall cycles",
        [43] = "ISPRAM Stall Cycles",
        [44] = "CACHE Instn stall cycles",
        [45] = "Load to Use stalls",
        [46] = "Read-CP0-value interlock stalls",
        [47] = "Relax bubbles",
        [48] = "IFU FB full refetches",
        [49] = "EJTAG Instruction Triggerpoints",
        [50] = "FSB < 1/4 full",
        [51] = "FSB > 1/2 full",
        [52] = "LDQ < 1/4 full",
        [53] = "LDQ > 1/2 full",
        [54] = "WBB < 1/4 full",
        [55] = "WBB > 1/2 full",
#if defined(CONFIG_CPU_MIPS_34K)
        [25] = "Cycles with no Instns for any TC",
        [31] = "Impl. specific customer yield manager event",
        [63] = NULL
#endif /*--- #if defined(CONFIG_CPU_MIPS_34K) ---*/
    },
    [1] = {
        [ 0] = "Cycles",
        [ 1] = "Instructions completed",
        [ 2] = "Branch mispredictions",
        [ 3] = "JR r31 mispredictions",
        [ 4] = "JR r31 not predicted",
        [ 5] = "ITLB misses",
        [ 6] = "DTLB misses",
        [ 7] = "JTLB instruction misses",
        [ 8] = "JTLB data misses",
        [ 9] = "Instruction cache misses",
        [10] = "Data cache writebacks",
        [11] = "Data cache load/store misses",
        [14] = "FPU instructions completed",
        [15] = "stores completed",
        [16] = "MIPS16 instructions completed",
        [17] = "integer multiply/divide completed",
        [18] = "replay traps (other than uTLB)",
        [19] = "SC instructions failed",
        [20] = "Prefetch instructions completed with cache hit",
        [21] = "L2 cache accesses",
        [22] = "L2 cache single bit errors corrected",
        [24] = "Refetches: refetched and reissued by IFU",
        [25] = "ALU stalls",
        [26] = "ALU-DSP Saturations Done",
        [27] = "MDU-DSP Saturations Done",
        [28] = "Impl. specific Cp2 event",
        [29] = "Impl. specific DSPRAM event",
        [33] = "Uncached Stores",
        [34] = "yield instructions completed",
        [35] = "CP2 To/From Instns completed",
        [37] = "D$ miss stall cycles",
        [39] = "L2 miss cycles",
        [41] = "FPU stall cycles",
        [42] = "CorExtend stall cycles",
        [43] = "DSPRAM stall cycles",
        [45] = "ALU to AGEN stalls",
        [46] = "Branch mispredict stalls",
        [48] = "FB entry allocated",
        [49] = "EJTAG Data Triggerpoints",
        [50] = "FSB 1/4-1/2 full",
        [51] = "FSB full pipeline stalls",
        [52] = "LDQ 1/4-1/2 full",
        [53] = "LDQ full pipeline stalls",
        [54] = "WBB 1/4-1/2 full",
        [55] = "WBB full pipeline stalls",
#if defined(CONFIG_CPU_MIPS_34K)
        [23] = "Cycles spent in Single Threaded Mode",
        [31] = "Custom ITC event",
        [44] = "Cycles spent waiting in ST mode",
#endif /*--- #if defined(CONFIG_CPU_MIPS_34K) ---*/
        [63] = NULL
        
    },
    [2] = {
        [ 0] = "Cycles",
        [ 1] = "Instructions completed",
        [ 2] = "branch instructions completed",
        [ 3] = "JR r31 (return) instructions",
        [ 4] = "JR (not r31) instructions",
        [ 5] = "ITLB accesses",
        [ 6] = "DTLB accesses",
        [ 7] = "JTLB instruction accesses",
        [ 8] = "JTLB data accesses",
        [ 9] = "Instruction Cache accesses",
        [10] = "Data cache load/stores",
        [11] = "Data cache load/store misses",
        [14] = "integer instructions completed",
        [15] = "loads completed",
        [16] = "J/JAL completed",
        [17] = "no-ops completed",
        [18] = "Main pipeline stalls",
        [19] = "SC instructions completed",
        [20] = "Prefetch instructions to cached addresses",
        [21] = "L2 cache writebacks",
        [22] = "L2 cache misses",
        [23] = "Exceptions taken",
        [24] = "cache fixup",
        [26] = "DSP Instructions Completed",
        [28] = "Impl. specific PM event",
        [29] = "Impl. specific ISPRAM event",
        [30] = "Impl. specific CorExtend event",
        [32] = "ITC loads",
        [33] = "Uncached loads",
        [34] = "fork instructions completed",
        [35] = "CP2 register-to-register Instns Completed",
        [37] = "I$ Miss Stall cycles",
        [38] = "L2 I-miss stall cycles",
        [39] = "D$ miss cycles",
        [40] = "Uncached access block cycles",
        [41] = "MDU stall cycles",
        [42] = "CP2 stall cycles",
        [43] = "ISPRAM Stall Cycles",
        [44] = "CACHE Instn stall cycles",
        [45] = "Load to Use stalls",
        [46] = "Read-CP0-value interlock stalls",
        [47] = "Relax bubbles",
        [48] = "IFU FB full refetches",
        [49] = "EJTAG Instruction Triggerpoints",
        [50] = "FSB < 1/4 full",
        [51] = "FSB > 1/2 full",
        [52] = "LDQ < 1/4 full",
        [53] = "LDQ > 1/2 full",
        [54] = "WBB < 1/4 full",
        [55] = "WBB > 1/2 full",
#if defined(CONFIG_CPU_MIPS_34K)
        [25] = "Cycles with no Instns for any TC",
        [31] = "Impl. specific customer yield manager event",
#endif /*--- #if defined(CONFIG_CPU_MIPS_34K) ---*/
        [63] = NULL
    },
    [3] = {
        [ 0] = "Cycles",
        [ 1] = "Instructions completed",
        [ 2] = "Branch mispredictions",
        [ 3] = "JR r31 mispredictions",
        [ 4] = "JR r31 not predicted",
        [ 5] = "ITLB misses",
        [ 6] = "DTLB misses",
        [ 7] = "JTLB instruction misses",
        [ 8] = "JTLB data misses",
        [ 9] = "Instruction cache misses",
        [10] = "Data cache writebacks",
        [11] = "Data cache load/store misses",
        [14] = "FPU instructions completed",
        [15] = "stores completed",
        [16] = "MIPS16 instructions completed",
        [17] = "integer multiply/divide completed",
        [18] = "replay traps (other than uTLB)",
        [19] = "SC instructions failed",
        [20] = "Prefetch instructions completed with cache hit",
        [21] = "L2 cache accesses",
        [22] = "L2 cache single bit errors corrected",
        [24] = "Refetches: refetched and reissued by IFU",
        [25] = "ALU stalls",
        [26] = "ALU-DSP Saturations Done",
        [27] = "MDU-DSP Saturations Done",
        [28] = "Impl. specific Cp2 event",
        [29] = "Impl. specific DSPRAM event",
        [33] = "Uncached Stores",
        [34] = "yield instructions completed",
        [35] = "CP2 To/From Instns completed",
        [37] = "D$ miss stall cycles",
        [39] = "L2 miss cycles",
        [41] = "FPU stall cycles",
        [42] = "CorExtend stall cycles",
        [43] = "DSPRAM stall cycles",
        [45] = "ALU to AGEN stalls",
        [46] = "Branch mispredict stalls",
        [48] = "FB entry allocated",
        [49] = "EJTAG Data Triggerpoints",
        [50] = "FSB 1/4-1/2 full",
        [51] = "FSB full pipeline stalls",
        [52] = "LDQ 1/4-1/2 full",
        [53] = "LDQ full pipeline stalls",
        [54] = "WBB 1/4-1/2 full",
        [55] = "WBB full pipeline stalls",
#if defined(CONFIG_CPU_MIPS_34K)
        [23] = "Cycles spent in Single Threaded Mode",
        [31] = "Custom ITC event",
        [44] = "Cycles spent waiting in ST mode",
#endif /*--- #if defined(CONFIG_CPU_MIPS_34K) ---*/
        [63] = NULL
    }
#else /*--- !defined(CONFIG_CPU_MIPS_74K) ---*/
    [0] = {
        [ 0] = "Cycles",
        [ 1] = "Instructions completed",
        [ 2] = "JR r31 (return) instructions",
        [ 3] = "Cycles where no instn is fetched or after wait",
        [ 4] = "ITLB accesses",
        [ 6] = "Instruction cache accesses",
        [ 7] = "Cycles without Instn fetch due to I-cache miss",
        [ 8] = "Cycles waiting for direct Instn fetch",
        [ 9] = "Replays in IFU due to full Instn buffer",
        [13] = "Cycles with no Instn to ALU due to full buffer",
        [14] = "Cycles with no Instn to ALU due to no free ALU CB",
        [15] = "Cycles without Instn added to ALU due to no free FIFOs",
        [16] = "Cycles with no ALU-pipe issue: no Instn avail.",
        [17] = "Cycles with no ALU-pipe issue: no operands ready",
        [18] = "Cycles with no ALU-pipe issue: ressource busy",
        [19] = "ALU pipe-bubbles issued",
        [20] = "Cycles with no Instn issued",
        [21] = "Out-of-order ALU issue",
        [22] = "Graduated JAR/JALR.HB",
        [23] = "Cacheable loads",
        [24] = "D-Cache writebacks",
        [26] = "D-side JTLB accesses",
        [28] = "L2 cache writebacks",
        [29] = "L2 cache misses",
        [30] = "Pipe stalls due to full FSB",
        [31] = "Pipe stalls due to full LDQ",
        [32] = "Pipe stalls due to full WBB",
        [35] = "Redirects following optimistic instn issue which failed",
        [36] = "JR (not r31) instructions",
        [37] = "Branch-likely instns graduated",
        [38] = "L2 I-miss stall cycles",
        [39] = "Branches graduated",
        [40] = "Integer instns graduated",
        [41] = "Loads graduated",
        [42] = "j/ja1 graduated",
        [43] = "Co-ops graduated",
        [44] = "DSP instructions graduated",
        [45] = "DSP branch instructions graduated",
        [46] = "Uncached loads graduated",
        [49] = "EJTAG Instruction Triggerpoints",
        [50] = "CP1 branches mispredicted",
        [51] = "sc instructions graduated",
        [52] = "prefetch instns graduated top of LSGB",
        [53] = "Cycles where no instns graduated",
        [54] = "Cycles where one instn graduated",
        [55] = "GFifo blocked cycles",
        [56] = "Cycles where 0 instns graduated",
        [58] = "Exceptions taken",
        [59] = "Impl. specific CorExtend event",
        [62] = "Impl. specific ISPRAM event",
        [63] = "L2 single bit errors corrected"
    },
    [1] = {
        [ 0] = "Cycles",
        [ 1] = "Instructions completed",
        [ 2] = "JR r31 mispredictions",
        [ 3] = "JR r31 not predicted",
        [ 4] = "ITLB misses",
        [ 5] = "JTLB instruction access fails",
        [ 6] = "Instruction cache misses",
        [ 7] = "L2 I-miss cycles",
        [ 8] = "PDTrace back stalls",
        [ 9] = "Fetch slots killed in IFU", 
        [13] = "AGEN issue pool full",
        [14] = "run out of AGEN CBs",
        [15] = "IOIQ FIFO full",
        [16] = "No instns avail. for AGEN-pipe issue",
        [17] = "No operands avail. for AGEN-pipe issue",
        [18] = "No AGEN-pipe issue, waiting for data",
        [20] = "Cycles with two instns issued",
        [21] = "Out-of-order AGEN issue",
        [22] = "D-cache line refill (not LD/ST misses)",
        [23] = "All D-cache accesses",
        [24] = "D-Cache misses",
        [25] = "D-side JTBL translt. fails",
        [26] = "Bogus D-ache misses",
        [28] = "L2 cache accesses",
        [29] = "L2 cache misses",
        [30] = "FSB >1/2 full",
        [31] = "LDQ >1/2 full",
        [32] = "WBB >1/2 full",
        [35] = "Copro. load instns.",
        [36] = "jr $31 graduated after mispredict",
        [37] = "CP1/CP2 conditional branch instns. graduated",
        [38] = "Mispredicted branch-like ins. graduated",
        [39] = "Mispredicted branches graduated",
        [40] = "FPU instructions graduated",
        [41] = "Stores graduated",
        [42] = "MIPS16 instn. graduated",
        [43] = "integer multiply/divide graduated",
        [44] = "ALU-DSP graduated, result saturated",
        [45] = "MDU-DSP graduated, result saturated",
        [46] = "Uncached stores graduated",
        [49] = "EJTAG data triggers",
        [51] = "sc instrns. failed",
        [52] = "prefetch instns. cache hits",
        [53] = "load misses graduated",
        [54] = "Two instns. graduated",
        [55] = "Floating point stores graduated",
        [56] = "Cycles where 0 instns. graduated",
        [58] = "Replays initiated from graduation",
        [59] = "Impl. specific system event",
        [61] = "Reserved for CP2 event",
        [62] = "Impl. specific DSPRAM block event"
    },
    [2] = {
        [ 0] = "Cycles",
        [ 1] = "Instructions completed",
        [ 2] = "JR r31 (return) instructions",
        [ 3] = "Cycles where no instn is fetched or after wait",
        [ 4] = "ITLB accesses",
        [ 6] = "Instruction cache accesses",
        [ 7] = "Cycles without Instn fetch due to I-cache miss",
        [ 8] = "Cycles waiting for direct Instn fetch",
        [ 9] = "Replays in IFU due to full Instn buffer",
        [13] = "Cycles with no Instn to ALU due to full buffer",
        [14] = "Cycles with no Instn to ALU due to no free ALU CB",
        [15] = "Cycles without Instn added to ALU due to no free FIFOs",
        [16] = "Cycles with no ALU-pipe issue: no Instn avail.",
        [17] = "Cycles with no ALU-pipe issue: no operands ready",
        [18] = "Cycles with no ALU-pipe issue: ressource busy",
        [19] = "ALU pipe-bubbles issued",
        [20] = "Cycles with no Instn issued",
        [21] = "Out-of-order ALU issue",
        [22] = "Graduated JAR/JALR.HB",
        [23] = "Cacheable loads",
        [24] = "D-Cache writebacks",
        [26] = "D-side JTLB accesses",
        [28] = "L2 cache writebacks",
        [29] = "L2 cache misses",
        [30] = "Pipe stalls due to full FSB",
        [31] = "Pipe stalls due to full LDQ",
        [32] = "Pipe stalls due to full WBB",
        [35] = "Redirects following optimistic instn issue which failed",
        [36] = "JR (not r31) instructions",
        [37] = "Branch-likely instns graduated",
        [38] = "L2 I-miss stall cycles",
        [39] = "Branches graduated",
        [40] = "Integer instns graduated",
        [41] = "Loads graduated",
        [42] = "j/ja1 graduated",
        [43] = "Co-ops graduated",
        [44] = "DSP instructions graduated",
        [45] = "DSP branch instructions graduated",
        [46] = "Uncached loads graduated",
        [49] = "EJTAG Instruction Triggerpoints",
        [50] = "CP1 branches mispredicted",
        [51] = "sc instructions graduated",
        [52] = "prefetch instns graduated top of LSGB",
        [53] = "Cycles where no instns graduated",
        [54] = "Cycles where one instn graduated",
        [55] = "GFifo blocked cycles",
        [56] = "Cycles where 0 instns graduated",
        [58] = "Exceptions taken",
        [59] = "Impl. specific CorExtend event",
        [62] = "Impl. specific ISPRAM event",
        [63] = "L2 single bit errors corrected"
    },
    [3] = {
        [ 0] = "Cycles",
        [ 1] = "Instructions completed",
        [ 2] = "JR r31 mispredictions",
        [ 3] = "JR r31 not predicted",
        [ 4] = "ITLB misses",
        [ 5] = "JTLB instruction access fails",
        [ 6] = "Instruction cache misses",
        [ 7] = "L2 I-miss cycles",
        [ 8] = "PDTrace back stalls",
        [ 9] = "Fetch slots killed in IFU", 
        [13] = "AGEN issue pool full",
        [14] = "run out of AGEN CBs",
        [15] = "IOIQ FIFO full",
        [16] = "No instns avail. for AGEN-pipe issue",
        [17] = "No operands avail. for AGEN-pipe issue",
        [18] = "No AGEN-pipe issue, waiting for data",
        [20] = "Cycles with two instns issued",
        [21] = "Out-of-order AGEN issue",
        [22] = "D-cache line refill (not LD/ST misses)",
        [23] = "All D-cache accesses",
        [24] = "D-Cache misses",
        [25] = "D-side JTBL translt. fails",
        [26] = "Bogus D-ache misses",
        [28] = "L2 cache accesses",
        [29] = "L2 cache misses",
        [30] = "FSB >1/2 full",
        [31] = "LDQ >1/2 full",
        [32] = "WBB >1/2 full",
        [35] = "Copro. load instns.",
        [36] = "jr $31 graduated after mispredict",
        [37] = "CP1/CP2 conditional branch instns. graduated",
        [38] = "Mispredicted branch-like ins. graduated",
        [39] = "Mispredicted branches graduated",
        [40] = "FPU instructions graduated",
        [41] = "Stores graduated",
        [42] = "MIPS16 instn. graduated",
        [43] = "integer multiply/divide graduated",
        [44] = "ALU-DSP graduated, result saturated",
        [45] = "MDU-DSP graduated, result saturated",
        [46] = "Uncached stores graduated",
        [49] = "EJTAG data triggers",
        [51] = "sc instrns. failed",
        [52] = "prefetch instns. cache hits",
        [53] = "load misses graduated",
        [54] = "Two instns. graduated",
        [55] = "Floating point stores graduated",
        [56] = "Cycles where 0 instns. graduated",
        [58] = "Replays initiated from graduation",
        [59] = "Impl. specific system event",
        [61] = "Reserved for CP2 event",
        [62] = "Impl. specific DSPRAM block event"
    }

#endif /*--- !defined(CONFIG_CPU_MIPS_74K) ---*/

};
#endif /*--- #ifdef CONFIG_MIPS ---*/

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
unsigned int format_profile_header(char *p, unsigned long timediff) {
    unsigned int len;
    len = sprintf(p, "# measure time %lu msec\n", (timediff * 1000) / HZ);
#if defined(CONFIG_MIPS) && !defined(CONFIG_MIPS_UR8)
    {
    unsigned int i;
    i = read_c0_perfctrl0();
    /*--- printk(KERN_ERR "read_c0_perfctrl0: 0x%x\n", i); ---*/
    len += sprintf(p + len, "# performance 0 \"%s\" (%s)\n", 
            perfomance_counter_options[0][(i >> 5) & ((1 << 7) - 1)],
            print_perfomance_counter_mode(i));

    i = read_c0_perfctrl1();
    /*--- printk(KERN_ERR "read_c0_perfctrl1: 0x%x\n", i); ---*/
    len += sprintf(p + len, "# performance 1 \"%s\" (%s)\n", 
            perfomance_counter_options[1][(i >> 5) & ((1 << 7) - 1)],
            print_perfomance_counter_mode(i));
    }
#endif /*--- #if defined(CONFIG_MIPS) && !defined(CONFIG_MIPS_UR8) ---*/
    return len;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
unsigned int format_profile_line(char *p, struct _avm_profile_data *data, unsigned int handle, unsigned int supress_kernel_output) {
    char *description = "";
    unsigned char Symbols[256];
    unsigned int len = 0;

    /*--------------------------------------------------------------------------*\
    \*------------------------------------------------------------------------------*/
    /*--- if((data->curr < (struct task_struct *)0x94000000) || (data->curr > (struct task_struct *)0x96000000)) { ---*/
        /*--- if((data->curr < (struct task_struct *)0xC0000000) || (data->curr > (struct task_struct *)0xE0000000)) { ---*/
            /*--- printk("[profiling:%d %s] invallid current pointer 0x%x\n", i, ---*/ 
                    /*--- avm_profile_data_short_names[data->type], ---*/ 
                    /*--- (void *)data->curr); ---*/
            /*--- continue; ---*/
        /*--- } ---*/
    /*--- } ---*/

    /*--------------------------------------------------------------------------*\
    \*------------------------------------------------------------------------------*/
    /*--- if((data->addr < 0x94000000) || (data->addr > 0x96000000)) { ---*/
        /*--- printk("[profiling:%d %s] invallid addr pointer 0x%x\n", i, ---*/ 
                /*--- avm_profile_data_short_names[data->type], ---*/ 
                /*--- data->addr); ---*/
        /*--- continue; ---*/
    /*--- } ---*/

    switch(data->type) {
        case avm_profile_data_type_free:
        case avm_profile_data_type_unknown:
            break;
        default:
            printk(KERN_ERR"[profiling] internal error data type %d unknown\n", data->type);
            break;
        case avm_profile_data_type_code_address_info:
            sprint_symbol(Symbols, data->addr);
            switch(handle) {
                case 3:
                    len = sprintf(p, "%x;0x%08X;0x%08X;0x%08X;CODE;0x%08x;%s;%.*s;%u\n", 
                            data->cpu_id, 
                            data->time, 
                            data->total_access, 
                            data->total_activate,
                            data->addr, Symbols, TASK_COMM_LEN, 
                            data->curr->comm, 
                            data->id);
                    if(supress_kernel_output == 0) printk("c");
                    break;
                case 4:
                    len = sprintf(p, "%x;0x%08X: CODE 0x%08x %s (%.*s interrupted by irq %u)\n", 
                            data->cpu_id, 
                            data->time, 
                            data->addr, 
                            Symbols, 
                            TASK_COMM_LEN, 
                            data->curr->comm, 
                            data->id);
                    if(supress_kernel_output == 0)printk("c");
                    break;
                case 5:
                    printk(KERN_INFO"%x;0x%08X: CODE 0x%08x %s (%.*s interrupted by irq %u)\n", 
                            data->cpu_id, 
                            data->time, 
                            data->addr, 
                            Symbols, 
                            TASK_COMM_LEN, 
                            data->curr->comm, 
                            data->id);
                    break;
            }
            break;
        case avm_profile_data_type_data_address_info:
            sprint_symbol(Symbols, data->addr);
            switch(handle) {
                case 3:
                    len = sprintf(p, "%x;0x%08X;0x%08X;0x%08X;DATA;0x%08x;%s;%.*s;%u\n", 
                            data->cpu_id, 
                            data->total_access, 
                            data->total_activate, 
                            data->time, 
                            data->addr, 
                            Symbols, 
                            TASK_COMM_LEN, 
                            data->curr->comm, 
                            data->id);
                    if(supress_kernel_output == 0)printk("d");
                    break;
                case 4:
                    len = sprintf(p, "%x;0x%08X: DATA 0x%08x %s (%.*s interrupted by irq %u)\n", 
                            data->cpu_id, 
                            data->time, 
                            data->addr, 
                            Symbols, 
                            TASK_COMM_LEN, 
                            data->curr->comm, 
                            data->id);
                    if(supress_kernel_output == 0)printk("d");
                    break;
                case 5:
                    printk(KERN_INFO"%x;0x%08X: DATA 0x%08x %s (%.*s interrupted by irq %u)\n", 
                            data->cpu_id, 
                            data->time, 
                            data->addr, 
                            Symbols, 
                            TASK_COMM_LEN, 
                            data->curr->comm, 
                            data->id);
                    break;
            }
            break;
        case avm_profile_data_type_trace_skb:
            if(supress_kernel_output == 0)printk("s");
            goto print_work_trace;
        case avm_profile_data_type_text:
            /*--- printk("t"); ---*/
            break;
        case avm_profile_data_type_hw_irq_begin:
        case avm_profile_data_type_hw_irq_end:
            description = "interrupted by irq";
            goto print_work_trace;
        case avm_profile_data_type_sw_irq_begin:
        case avm_profile_data_type_sw_irq_end:
            description = "id:";
            goto print_work_trace;
        case avm_profile_data_type_timer_begin:
        case avm_profile_data_type_timer_end:
            description = "id:";
            goto print_work_trace;
        case avm_profile_data_type_tasklet_begin:
        case avm_profile_data_type_tasklet_end:
            description = "id:";
            goto print_work_trace;
        case avm_profile_data_type_hi_tasklet_begin:
        case avm_profile_data_type_hi_tasklet_end:
            description = "id:";
            goto print_work_trace;
        case avm_profile_data_type_trigger_tasklet_begin:
        case avm_profile_data_type_trigger_tasklet_end:
            description = "id:";
            goto print_work_trace;
        case avm_profile_data_type_workitem_begin:
        case avm_profile_data_type_workitem_end:
            description = "id:";
            goto print_work_trace;
        case avm_profile_data_type_cpphyrx_begin:
        case avm_profile_data_type_cpphyrx_end:
        case avm_profile_data_type_cpphytx_begin:
        case avm_profile_data_type_cpphytx_end:
        case avm_profile_data_type_func_begin:
        case avm_profile_data_type_func_end:
            description = "id:";
            goto print_work_trace;

print_work_trace:
            sprint_symbol(Symbols, data->addr);
            switch(handle) {
                case 3:
                    len = sprintf(p, "%x;0x%08X;0x%08X;0x%08X;%s;0x%08x;%s;%.*s;%u;\n", 
                            data->cpu_id, 
                            data->time, 
                            data->total_access, 
                            data->total_activate, 
                            avm_profile_data_short_names[data->type], 
                            data->addr, 
                            Symbols, 
                            TASK_COMM_LEN, 
                            data->curr->comm, 
                            data->id);
                    if(supress_kernel_output == 0)printk("d");
                    break;
                case 4:
                    len = sprintf(p, "%x;0x%08X:%s 0x%08x %s (%.*s %s %u);\n", 
                            data->cpu_id, 
                            data->time, 
                            avm_profile_data_short_names[data->type], 
                            data->addr, 
                            Symbols, 
                            TASK_COMM_LEN, 
                            data->curr->comm, description, 
                            data->id);
                    if(supress_kernel_output == 0)printk("d");
                    break;
                case 5:
                    printk(KERN_INFO"%x;0x%08X: %s 0x%08x %s (%.*s %s %u)\n", 
                            data->cpu_id, 
                            data->time, 
                            avm_profile_data_long_names[data->type], 
                            data->addr, 
                            Symbols, 
                            TASK_COMM_LEN, 
                            data->curr->comm, description, 
                            data->id);
                    break;
            }
    }
    return len;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_profile_action(int handle, unsigned int trace_mask, unsigned int param2 __attribute__((unused))) {
    /*--- extern struct _avm_profile_data **avm_simple_profiling_enable(unsigned int on, unsigned int *count, unsigned long *timediff); ---*/
    unsigned int len, i;
    unsigned long timediff;
    static unsigned char Buffer[512];
    static struct file *fp;
    mm_segment_t  oldfs;  
    size_t bytesWritten;
    char *p = Buffer;

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    void open_output_file(char *name) {
        fp = filp_open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(IS_ERR(fp)) {
            printk(KERN_ERR "[avm_led] Failed: Could not open \"/var/profile.log\"\n");
            return;
        }
        /* Lesezugriff auf File(system) erlaubt? */
        if (fp->f_op->write == NULL) {
            printk(KERN_ERR "[avm_led] Failed: Could not write \"/var/profile.log\"\n");
            return;
        }

        /* Von Anfang an lesen */
        fp->f_pos = 0;
    }

    void close_output_file(void) {
        filp_close(fp, NULL);
    }

    printk(KERN_INFO"[avm_led_profile_action] handle %u\n", handle);

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    switch(handle) {
        default:
            break;
        case 1: /* disable */
            avm_simple_profiling_enable(0, 0, &len, &timediff);
            printk(KERN_INFO"[avm_led] profile_action: disable, %u entries recorded (output: /proc/avm/profile/csv (preferred), 2: CSV, 3: ASCII)\n", len);
            return;
        case 2: /* enable */
        case 6: /* enable wraparound */
        case 7: /* enable skb trace */
        case 8: /* enable skb trace wraparound */
            avm_simple_profiling_enable(handle, trace_mask, &len, NULL);
            printk(KERN_INFO"[avm_led] profile_action: enable\n");
            return;
        case 3: /* write CSV file */
        case 4: /* write ASCII file */
        case 5:
            avm_simple_profiling_enable(0, 0, &len, &timediff);
            printk(KERN_INFO"[avm_led] profile_action: %u entries recorded, write %s file\n", len, handle == 3 ? "CSV" : "ASCII");

            if(handle != 5) {
                open_output_file("/var/profile.csv");
            }
            format_profile_header(Buffer, timediff);
            p = &Buffer[strlen(Buffer)];
            /*--- kein break; ---*/
            /*------------------------------------------------------------------------------*\
            \*------------------------------------------------------------------------------*/
            for(i = 0 ; i < len ; i++) {
                unsigned int len;
                struct _avm_profile_data *data;

                data = avm_simple_profiling_by_idx(i);
                if(data == NULL) {
                    break;
                }
                format_profile_line(p, data, handle, 0);
                if(handle != 5) {
                    len = strlen(Buffer);
                    oldfs = get_fs();
                    set_fs(KERNEL_DS);
                    bytesWritten = fp->f_op->write(fp, Buffer, len, &fp->f_pos);
                    if(bytesWritten != len) {
                        printk(KERN_ERR "memory full (abort writing file)\n");
                        set_fs(oldfs);
                        break;
                    }
                    p = Buffer;
                    set_fs(oldfs);
                }
            }
            if(handle != 5) {
                close_output_file();
                printk(KERN_INFO"\n[avm_led] file closed\n");
            }
            return;
    }
    return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_led_profile_sync(int virt_led_handle __attribute__((unused))) {
    return;
}

#if defined(CONFIG_AVM_DEBUG) || defined(CONFIG_AVM_DEBUG_MODULE)
static void *profiler_DbgHandle;
#define SKIP_SPACE(a)       while(*(a) && ((*(a) == ' ') || (*(a) == '\t'))) (a)++ 
#define SKIP_NON_SPACE(a)   while(*(a) && ((*(a) != ' ') && (*(a) != '\t'))) (a)++ 
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

#if defined(CONFIG_CPU_MIPS_34K)

#define PERFCNT_RELOAD_LAST_VAL 0xFFFFFFFF
#define MAX_COUNT_TCS 4
#define MAX_COUNT_REGS 4
static unsigned int preset_counter_value_storage[MAX_COUNT_TCS][MAX_COUNT_REGS];

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

static unsigned int supportet_tcs(void){
    unsigned int mvpconf0;
    mvpconf0 = read_c0_mvpconf0();
    return ((mvpconf0 & MVPCONF0_PTC) >> MVPCONF0_PTC_SHIFT) + 1;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

static void write_c0_perfctl(unsigned int ctl_reg, unsigned int tc, unsigned int val){
    unsigned long vpflags;

#if 0
    if (tc == smp_processor_id() ) {

       // printk(KERN_ERR "direct access for tc=%d\n", tc);
        switch(ctl_reg){
            case 0:
                write_c0_perfctrl0( val );
                break;
            case 1:
                write_c0_perfctrl1( val );
                break;
        default:
            printk(KERN_ERR "count reg %d is not supported\n", ctl_reg);
            break;
        }

       return;
    }
#endif

	vpflags = dvpe();
    settc(tc);
    switch(ctl_reg){
        case 0:
            mttc0(25, 0, val);
            break;
        case 1:
            mttc0(25, 2, val);
            break;
        default:
            printk(KERN_ERR "performance ctrl reg %d is not supported\n", ctl_reg);
            break;
    }
	evpe(vpflags);
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int current_tc(void){
    unsigned int tcs = supportet_tcs();
    unsigned int tc;
    for (tc = 0; tc < tcs; tc++){
        settc(tc);
	    if (read_tc_c0_tcbind() == (unsigned int)read_c0_tcbind())
	        return tc;
    }
    BUG();
    return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void write_c0_perfcnt(unsigned int count_reg, unsigned int tc, unsigned int val){
    unsigned long vpflags;

    BUG_ON( count_reg >= MAX_COUNT_REGS );
    BUG_ON( tc >= MAX_COUNT_TCS );

    if (val == PERFCNT_RELOAD_LAST_VAL){
        val = preset_counter_value_storage[tc][count_reg];
        // printk(KERN_ERR "restore[%d][%d]=%#x\n", tc,count_reg, val);
    } else {
       preset_counter_value_storage[tc][count_reg] = val;
       // printk(KERN_ERR "store[%d][%d]=%#x\n", tc, count_reg, val);
    }
#if 0
    if (tc == smp_processor_id() ){
       // printk(KERN_ERR "direct access for tc=%d\n", tc);
        switch(count_reg){
            case 0:
                write_c0_perfcntr0( val );
                break;
            case 1:
                write_c0_perfcntr1( val );
                break;
        default:
            printk(KERN_ERR "count reg %d is not supported\n", count_reg);
            break;
        }

       return;
    }
#endif

    // printk(KERN_ERR "indirect access for tc=%d\n", tc);
	vpflags = dvpe();
    settc(tc);
    switch(count_reg){
        case 0:
            mttc0(25, 1, val);
            break;
        case 1:
            mttc0(25, 3, val);
            break;
        default:
            printk(KERN_ERR "count reg %d is not supported\n", count_reg);
            break;
    }
	evpe(vpflags);
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

unsigned int read_c0_perfcnt(unsigned int count_reg, unsigned int tc){
    unsigned long vpflags;
    unsigned int res = 0;
	vpflags = dvpe();

    settc(tc);
    switch(count_reg){
        case 0:
            res = mftc0(25, 1);
            break;
        case 1:
            res = mftc0(25, 3);
            break;
        default:
            printk(KERN_ERR "count reg %d is not supported\n", count_reg);
            break;
    }

    evpe(vpflags);
    return res;
}
EXPORT_SYMBOL(read_c0_perfcnt);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

static unsigned int read_c0_perfctl(unsigned int ctl_reg, unsigned int tc){
    unsigned long vpflags;
    unsigned int res = 0;
	vpflags = dvpe();

    settc(tc);
    switch(ctl_reg){
        case 0:
            res = mftc0(25, 0);
            break;
        case 1:
            res = mftc0(25, 2);
            break;
        default:
            printk(KERN_ERR "ctl reg %d is not supported\n", ctl_reg);
            break;
    }

    evpe(vpflags);
    return res;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

static void write_c0_perfctl_for_counting_tc_mask(unsigned int perf_ctl_reg, unsigned int val, unsigned int counting_tc_mask){
    unsigned int i;
    unsigned int ntc = supportet_tcs();

    for ( i = 0; i < ntc; i++){
        if (( 1 << i) & counting_tc_mask )
            write_c0_perfctl(perf_ctl_reg, i, val);
    }
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void write_c0_perfcnt_for_counting_tc_mask(unsigned int perf_ctl_reg, unsigned int val, unsigned int counting_tc_mask){
    unsigned int i;
    unsigned int ntc = supportet_tcs();

    for ( i = 0; i < ntc; i++){
        if (( 1 << i) & counting_tc_mask )
            write_c0_perfcnt(perf_ctl_reg, i, val);
    }
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

unsigned int ack_irqs_reload_counter(void) {

    unsigned int tc;
    unsigned int reloaded_tcs_mask = 0;
    unsigned int ntc = supportet_tcs();

    for ( tc = 0; tc < ntc; tc++ ) {
       unsigned int pcntr = 0;
       unsigned int pcntr_control = 0;

       do {
           pcntr_control =  read_c0_perfctl( pcntr, tc );
           if ( pcntr_control &  MIPS_PERFOMANCE_IRQ_ENABLE ) {
                // printk(KERN_ERR "irq_enabled in tc=%d, pctr=%d \n",tc, pcntr );
                if (read_c0_perfcnt(pcntr, tc) & (1<< 31)){
                    write_c0_perfcnt(pcntr, tc, PERFCNT_RELOAD_LAST_VAL);
                    // printk(KERN_ERR "bit 31 set in tc=%d, pctr=%d, reload=%#x \n",tc, pcntr, read_c0_perfcnt(pcntr, tc) );
                    reloaded_tcs_mask |= ( 1 << tc );
                }
           }
           pcntr += 1;
       } while (pcntr_control & MIPS_PERFOMANCE_HAS_MORE);
    }

    return reloaded_tcs_mask;
}
EXPORT_SYMBOL(ack_irqs_reload_counter);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

#endif

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void cmdlineparse(char *cmdline, void *dummy __attribute__((unused))) {
    char *p = cmdline; 
    unsigned int val;
    SKIP_SPACE(p);
    if((!*p || !strcmp(p, "help"))) {
        printk(KERN_ERR "use: echo profiler <cmd-nr> [mask=0x<trace-mask>] [count=<sample-count/%d>]\n", profile_DataSetsPerBlock);
        printk(KERN_ERR "\t0: disable, 1: enable,2: write csv,3: write ascii,4: display ascii,5: enable+wraparound,6: enable skb,7: skb+wraparound\n");
        printk(KERN_ERR "use: echo profiler performance [<register-nr>] [<Mode-nr>|<Mode-name>|liste]\n");
        return;
    }
    if((*p >= '0') && (*p <= '9')) {
        unsigned int trace_mask = 
            /*--- (1 << avm_profile_data_type_text) | ---*/
            (1 << avm_profile_data_type_code_address_info) |
            (1 << avm_profile_data_type_data_address_info) |
            /*--- (1 << avm_profile_data_type_trace_skb) | ---*/
            (1 << avm_profile_data_type_hw_irq_begin) |
            (1 << avm_profile_data_type_hw_irq_end) |
            (1 << avm_profile_data_type_sw_irq_begin) |
            (1 << avm_profile_data_type_sw_irq_end) |
            (1 << avm_profile_data_type_timer_begin) |
            (1 << avm_profile_data_type_timer_end) |
            (1 << avm_profile_data_type_tasklet_begin) |
            (1 << avm_profile_data_type_tasklet_end) |
            (1 << avm_profile_data_type_hi_tasklet_begin) |
            (1 << avm_profile_data_type_hi_tasklet_end) |
            (1 << avm_profile_data_type_workitem_begin) |
            (1 << avm_profile_data_type_workitem_end) |
            (1 << avm_profile_data_type_cpphytx_begin) |
            (1 << avm_profile_data_type_cpphytx_end) |
            (1 << avm_profile_data_type_cpphyrx_begin) |
            (1 << avm_profile_data_type_cpphyrx_end) |
            /*--- (1 << avm_profile_data_type_func_begin) | ---*/
            /*--- (1 << avm_profile_data_type_func_end) | ---*/
            0;
        if(strstr(p, "count=")) {
            sscanf(strstr(p, "count="), "count=%d", &profile_BlockNeeded);
        }
        if(strstr(p, "mask=0x")) {
            sscanf(strstr(p, "mask=0x"), "mask=0x%x", &trace_mask);
        }
        sscanf(p, "%x", &val);
        avm_led_profile_action(val+1, trace_mask, 0);
        return;
    }
#ifdef CONFIG_MIPS
    if(!strncmp(p, "perform", sizeof("perform") - 1)) {
        int i;
        unsigned int perf_reg_number = 0;
        int perf_reg_flags = 0;
        int perf_reg_index = 0;
        unsigned int perf_max_registers = 0;
        int config7 = 0;
        int perf_vpeid = -1;
        int perf_tcid = -1;
        int new_perf_ctl = 0;

#if defined(CONFIG_CPU_MIPS_34K)
        unsigned int current_tc_mask = (1 << smp_processor_id());
        int counting_tc_mask = current_tc_mask;
        int set_perf_irq = 0;
        int preset_count = 0;
#endif

        i = read_c0_config1();
        if((i & (1 << 4)) == 0) {
            printk(KERN_ERR "[simple-profiling]: no performance counters implemented\n");
            return;
        }
        perf_max_registers = 1;
        config7 = read_c0_config7();
        i = read_c0_perfctrl0();
        if(i & (1U << 31)) {
            perf_max_registers = 2;
            i = read_c0_perfctrl1();
            if(i & (1U << 31)) {
                perf_max_registers = 3;
                i = read_c0_perfctrl2();
                if(i & (1U << 31)) {
                    perf_max_registers = 4;
                    i = read_c0_perfctrl3();
                    if(i & (1U << 31)) {
                        perf_max_registers = 5;
                    }
                }
            }
        }
        printk(KERN_ERR "[simple-profiling]: %d performance counters implemented, %s\n", perf_max_registers, (config7 & (1 << 19))?"NEW_34K":"OLD_34K" );
        /*
         *  NEW_34K: the core has two performance counters, replicated per-TC
         *  OLD_34K: had four performance counters, not replicated
         */

        SKIP_NON_SPACE(p);
        SKIP_SPACE(p);

#if defined(CONFIG_CPU_MIPS_34K)
        if(strstr(p, "readcount")) {
            unsigned int tc, count_reg;
            unsigned int ntc = supportet_tcs();

            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);

            for (tc = 0; tc < ntc; tc++)
                for (count_reg = 0; count_reg < perf_max_registers; count_reg++)
                    printk(KERN_ERR "performance_count[tc=%d, counter=%d]  = %#x\n", tc, count_reg, read_c0_perfcnt(count_reg, tc));

            return;
        }

        if(strstr(p, "readconfig")) {
            unsigned int tc, count_reg;
            unsigned int ntc = supportet_tcs();

            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);

            for (tc = 0; tc < ntc; tc++)
                for (count_reg = 0; count_reg < perf_max_registers; count_reg++)
                    printk(KERN_ERR "performance_control[tc=%d, counter=%d]  = %#x\n", tc, count_reg, read_c0_perfctl(count_reg, tc));

            return;
        }

        if(strstr(p, "touchconfig")) {
            unsigned int touch_tc;
            unsigned int touch_reg;
            unsigned int ntc = supportet_tcs();

            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);

            if((*p >= '0') && (*p <= '9')) {
                sscanf(p, "%d", &touch_tc);
            }
            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);

            if((*p >= '0') && (*p <= '9')) {
                sscanf(p, "%d", &touch_reg);
            }
            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);

            if ( ( touch_tc <= ntc ) && ( touch_reg <= perf_max_registers )){
                unsigned int val = read_c0_perfctl(touch_reg, touch_tc);
                printk(KERN_ERR "read [tc=%d, counter=%d]  = %#x\n", touch_tc, touch_reg, val);
                write_c0_perfctl(touch_reg, touch_tc, val);
                printk(KERN_ERR "write [tc=%d, counter=%d]  = %#x\n", touch_tc, touch_reg, val);
            }
            return;
        }

        if(strstr(p, "resetconfig")) {
            unsigned int touch_tc;
            unsigned int touch_reg;
            unsigned int ntc = supportet_tcs();

            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);

            if((*p >= '0') && (*p <= '9')) {
                sscanf(p, "%d", &touch_tc);
            }
            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);

            if((*p >= '0') && (*p <= '9')) {
                sscanf(p, "%d", &touch_reg);
            }
            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);

            if ( ( touch_tc <= ntc ) && ( touch_reg <= perf_max_registers )){
                unsigned int val = read_c0_perfctl(touch_reg, touch_tc);
                printk(KERN_ERR "read [tc=%d, counter=%d]  = %#x\n", touch_tc, touch_reg, val);
                val = 0;
                write_c0_perfctl(touch_reg, touch_tc, val);
                printk(KERN_ERR "write [tc=%d, counter=%d]  = %#x\n", touch_tc, touch_reg, val);
            }
            return;
        }
#endif

        if((*p >= '0') && (*p <= '3')) {
            sscanf(p, "%d", &perf_reg_number);
        }

        if (perf_reg_number >= perf_max_registers){
            printk(KERN_INFO "Error: you cannot access perf_reg %d, we have just %d regs available:\n", perf_reg_number, perf_max_registers);
            return;
        }

        SKIP_NON_SPACE(p);
        SKIP_SPACE(p);
        if(!*p) {
show_performance_counter_options:
            printk(KERN_INFO "Optionen des Performance Counter %d:\n", perf_reg_number);
            for(i = 0 ; i < 64 ; i++) {
                if(perfomance_counter_options[perf_reg_number][i] == NULL)
                    continue;
                printk(KERN_INFO "\t[%2d]: %s\n", i, perfomance_counter_options[perf_reg_number][i]);
            }
            return;
        }
        if((*p >= '0') && (*p <= '9')) {
            sscanf(p, "%d", &perf_reg_index);
        } else {
            for(i = 0 ; i < 64 ; i++) {
                if(perfomance_counter_options[perf_reg_number][i] == NULL)
                    continue;
                if(!strncmp(perfomance_counter_options[perf_reg_number][i], p , strlen(p))) {
                    perf_reg_index = i;
                    break;
                }
            }
            if(i == 64) {
                goto show_performance_counter_options;
            }
        }
        SKIP_NON_SPACE(p);
        SKIP_SPACE(p);

        if(strstr(p, "examine_vpe=")) {
            sscanf(strstr(p, "examine_vpe="), "examine_vpe=%x", &perf_vpeid);
            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);
        }

        if(strstr(p, "examine_tc=")) {
            sscanf(strstr(p, "examine_tc="), "examine_tc=%x", &perf_tcid);
            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);
        }

#if defined(CONFIG_CPU_MIPS_34K)
        if(strstr(p, "counting_tc_mask=")) {
            sscanf(strstr(p, "counting_tc_mask="), "counting_tc_mask=%x", &counting_tc_mask);
            printk(KERN_INFO "setup counting_tc_mask=%#x\n",counting_tc_mask);

            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);
        }

        if(strstr(p, "preset_count=")) {
            sscanf(strstr(p, "preset_count="), "preset_count=%x", &preset_count);
            printk(KERN_INFO "setup preset_count=%#x\n", preset_count);
            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);
        }

        if(strstr(p, "irq=")) {
            sscanf(strstr(p, "irq="), "irq=%d", &set_perf_irq);
            SKIP_NON_SPACE(p);
            SKIP_SPACE(p);
        }
#endif
        
        perf_reg_flags = 
                MIPS_PERFOMANCE_USER_MODE_ENABLE |
                MIPS_PERFOMANCE_SUPERVISOR_MODE_ENABLE |
                MIPS_PERFOMANCE_KERNEL_MODE_ENABLE |
                /*--- MIPS_PERFOMANCE_EXCEPTION_ENABLE | ---*/
                0;


#if defined(CONFIG_CPU_MIPS_34K)
        if (set_perf_irq){
            printk(KERN_INFO "Enable Performance Counter irq\n");
            perf_reg_flags |= MIPS_PERFOMANCE_IRQ_ENABLE;
        }

        if( (perf_vpeid != -1)&&(perf_tcid != -1)) {
            printk(KERN_INFO "Error: you cannot use VPEID and TCID together\n");
            return;
        }
        if(perf_vpeid != -1){
                perf_reg_flags |= 
                    MIPS_PERFORMANCE_VPE_SPECIFIC_ENABLE |
                    MIPS_PERFORMANCE_VPEID(perf_vpeid);
        }
        if(perf_tcid != -1){
                perf_reg_flags |=
                    MIPS_PERFORMANCE_TC_SPECIFIC_ENABLE |
                    MIPS_PERFORMANCE_TCID(perf_tcid);
        }
#endif /*--- #if defined(CONFIG_CPU_MIPS_34K)  ---*/

        printk(KERN_INFO "Enable Performance Counter %d for %s (%s)\n",
            perf_reg_number, perfomance_counter_options[perf_reg_number][perf_reg_index],
            print_perfomance_counter_mode(perf_reg_flags));

        new_perf_ctl = (perf_reg_index << 5) | perf_reg_flags;

        switch(perf_reg_number) {
            case 0:
#if defined(CONFIG_CPU_MIPS_34K)
                    printk(KERN_INFO "Setup tcs=%#x current_tc_mask=%#x, reg=%d\n",counting_tc_mask, current_tc_mask, perf_reg_number);
                    if ( preset_count != -1)
                        write_c0_perfcnt_for_counting_tc_mask(0, preset_count, counting_tc_mask);
                    write_c0_perfctl_for_counting_tc_mask(0, new_perf_ctl, counting_tc_mask);
#else

                write_c0_perfctrl0(new_perf_ctl);
#endif
                break;
            case 1:
#if defined(CONFIG_CPU_MIPS_34K)
                    printk(KERN_INFO "Setup tcs=%#x current_tc_mask=%#x, reg=%d\n",counting_tc_mask, current_tc_mask, perf_reg_number);
                    if ( preset_count != -1)
                        write_c0_perfcnt_for_counting_tc_mask(1, preset_count, counting_tc_mask);
                    write_c0_perfctl_for_counting_tc_mask(1, new_perf_ctl, counting_tc_mask);
#else

                write_c0_perfctrl1(new_perf_ctl);
#endif
                break;
            case 2:
                write_c0_perfctrl2(new_perf_ctl);
                break;
            case 3:
                write_c0_perfctrl3(new_perf_ctl);
                break;
            default:
                    break;
        }

/*--- #define read_c0_perfctrl0()	__read_32bit_c0_register($25, 0) ---*/
/*--- #define read_c0_perfcntr0()	__read_32bit_c0_register($25, 1) ---*/
/*--- #define write_c0_perfcntr0(val)	__write_32bit_c0_register($25, 1, val) ---*/
/*--- #define read_c0_perfctrl1()	__read_32bit_c0_register($25, 2) ---*/
/*--- #define read_c0_perfcntr1()	__read_32bit_c0_register($25, 3) ---*/
/*--- #define write_c0_perfcntr1(val)	__write_32bit_c0_register($25, 3, val) ---*/
/*--- #define read_c0_perfctrl2()	__read_32bit_c0_register($25, 4) ---*/
/*--- #define write_c0_perfctrl2(val)	__write_32bit_c0_register($25, 4, val) ---*/
/*--- #define read_c0_perfcntr2()	__read_32bit_c0_register($25, 5) ---*/
/*--- #define write_c0_perfcntr2(val)	__write_32bit_c0_register($25, 5, val) ---*/
/*--- #define read_c0_perfctrl3()	__read_32bit_c0_register($25, 6) ---*/
/*--- #define write_c0_perfctrl3(val)	__write_32bit_c0_register($25, 6, val) ---*/
/*--- #define read_c0_perfcntr3()	__read_32bit_c0_register($25, 7) ---*/
/*--- #define write_c0_perfcntr3(val)	__write_32bit_c0_register($25, 7, val) ---*/

    }
#endif /*--- #ifdef CONFIG_MIPS ---*/
}

#if defined(CONFIG_PROC_FS)
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/file.h>

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _profile_readcsv {
    unsigned int pos;
    unsigned int entries;
    unsigned long timediff;
    struct _avm_profile_data *data;
    unsigned int cnt;
};

/*--------------------------------------------------------------------------------*\
  Returns false if pos at or past end of file. 
\*--------------------------------------------------------------------------------*/
static int update_iter(struct _profile_readcsv *iter, loff_t pos) {
	/* Module symbols can be accessed randomly. */
    iter->data = avm_simple_profiling_by_idx(pos);
	iter->pos  = pos;
    if(iter->data == NULL) {
        return 0;
    }
	return 1;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void *s_next(struct seq_file *m, void *p, loff_t *pos) {
	(*pos)++;

	if (!update_iter(m->private, *pos)) {
		return NULL;
    }
	return p;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void *s_start(struct seq_file *m, loff_t *pos) {
	if (!update_iter(m->private, *pos)) {
		return NULL;
    }
	return m->private;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void s_stop(struct seq_file *m __attribute__((unused)), void *p __attribute__((unused))) {
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int s_show(struct seq_file *m, void *p __attribute__((unused))) {
	struct _profile_readcsv *iter = m->private;
    char seqbuf[512];

    if(iter->pos == 0) {
        format_profile_header(seqbuf, iter->timediff);
        seq_printf(m, "%s",seqbuf);
    }
	if (iter->data == NULL) {
		return 0;
    }
    if(iter->cnt++ > 2000) {
        iter->cnt = 0;
        printk("*");
    }
    format_profile_line(seqbuf, iter->data, 3, 1);
	seq_printf(m, "%s",seqbuf);
	return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static const struct seq_operations readcsv_op = {
	.start = s_start,
	.next = s_next,
	.stop = s_stop,
	.show = s_show
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void reset_iter(struct _profile_readcsv *iter, loff_t new_pos) {
    avm_simple_profiling_enable(0, 0, &iter->entries, &iter->timediff);
	iter->pos = new_pos;
    iter->cnt = 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int readcsv_open(struct inode *inode __attribute__((unused)), struct file *file) {
	/*
	 * We keep iterator in m->private, since normal case is to
	 * s_start from where we left off, so we avoid doing
	 */
	struct _profile_readcsv *iter;
	int ret;

	iter = kmalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter) {
		return -ENOMEM;
    }
	reset_iter(iter, 0);
    
	ret = seq_open(file, &readcsv_op);
	((struct seq_file *)file->private_data)->private = iter;
	return ret;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static const struct file_operations readcsv_operations = {
	.open    = readcsv_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release_private,
};


static struct proc_dir_entry *profileprocdir;
#define PROC_AVMDIR       "avm"
#define PROC_PROFILEDIR   "avm/profile"
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void lproc_init(void) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
    struct file *dir_avm;

	dir_avm = filp_open("/proc/"PROC_AVMDIR, O_DIRECTORY, 0);
	if (IS_ERR(dir_avm)) {
        proc_mkdir(PROC_AVMDIR, NULL);
	} else {
        fput(dir_avm);
    }
#endif/*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34) ---*/
	profileprocdir = proc_mkdir(PROC_PROFILEDIR, NULL);
	if(profileprocdir == NULL) {
        return;
	}
	proc_create("csv", 0444, profileprocdir, &readcsv_operations);
}
#if defined(CONFIG_AVM_DEBUG_MODULE) && (CONFIG_AVM_DEBUG_MODULE == 1)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void lproc_exit(void) {
	if(profileprocdir) {
        remove_proc_entry("csv", profileprocdir);
        remove_proc_entry(PROC_PROFILEDIR, NULL);
        profileprocdir= NULL;
    }
}
#endif/*--- #if defined(CONFIG_AVM_DEBUG_MODULE) && (CONFIG_AVM_DEBUG_MODULE == 1) ---*/
#endif/*--- #if defined(CONFIG_PROC_FS) ---*/
/*--------------------------------------------------------------------------------*\
 * Das Led-Modul ist komplett ausgelagert worden, so ist es nun notwendig
 * den Profiler ebenfalls anders anzusteuern:
 * echo profiler <val> >/dev/debug
\*--------------------------------------------------------------------------------*/
void __init avm_profiler_init(void) {
    profiler_DbgHandle = avm_DebugCallRegister("profiler", cmdlineparse, NULL);

#if defined(CONFIG_MIPS) && !defined(CONFIG_AVM_SIMPLE_PROFILING_YIELD_PCNT)
    cmdlineparse("perform 0 10", "");
    cmdlineparse("perform 1 11", "");
#endif
#if defined(CONFIG_PROC_FS)
    lproc_init();
#endif/*--- #if defined(CONFIG_PROC_FS) ---*/
}

#if defined(CONFIG_AVM_DEBUG_MODULE) && (CONFIG_AVM_DEBUG_MODULE == 1)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void avm_profiler_exit(void) {
    if(profiler_DbgHandle) {
        avm_DebugCallUnRegister(profiler_DbgHandle);
        profiler_DbgHandle = NULL;
    }
#if defined(CONFIG_PROC_FS)
    lproc_exit();
#endif/*--- #if defined(CONFIG_PROC_FS) ---*/
}
#endif/*--- #if defined(CONFIG_AVM_DEBUG_MODULE) && (CONFIG_AVM_DEBUG_MODULE == 1) ---*/
#endif/*--- #if defined(CONFIG_AVM_DEBUG) || defined(CONFIG_AVM_DEBUG_MODULE) ---*/
