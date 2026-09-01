#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#ifdef CONFIG_MIPS
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/bitops.h>
#endif /*--- #ifdef CONFIG_MIPS ---*/

#ifdef CONFIG_LANTIQ
#include <ifx_gptu.h>
#endif
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_AVM_SIMPLE_PROFILING_YIELD
#include <asm/yield_context.h>
#ifndef CONFIG_AVM_SIMPLE_PROFILING_YIELD_PCNT
int yield_thread_freq = 20 * 1000; /* --- freq in Hz -> this is 20 kHz  --- */
int yield_freq_randomizer_percent = 25;
module_param(yield_thread_freq, int, S_IRUSR | S_IWUSR);
module_param(yield_freq_randomizer_percent , int, S_IRUSR | S_IWUSR);
#endif
#endif
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
#include <linux/avm_profile.h>
#include <linux/skbuff.h>

unsigned int profile_BlockNeeded = (CONFIG_AVM_PROFILING_TRACE_MODE * 10);

unsigned int profile_current_mask = 0;
unsigned int profile_current_start_handle = 0;

#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
struct _simple_profiling simple_profiling;
#else /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
# define PROFILE_BUFFER_LEN (CONFIG_AVM_PROFILING_TRACE_MODE * 1024)

struct _simple_profiling {
    /*--- struct _avm_profile_data data[PROFILE_BUFFER_LEN]; ---*/
    atomic_t pos;
    spinlock_t lock;
    unsigned int len;
    unsigned int enabled;
    unsigned int mode;
    unsigned int mask;
    unsigned long start_time;
    unsigned long end_time;
    unsigned int wraparround;
} simple_profiling;
#endif /*--- #else ---*/ /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
EXPORT_SYMBOL(simple_profiling);
/*--------------------------------------------------------------------------------*\
 * time und pos-Reservierung muessen atomar passieren!
\*--------------------------------------------------------------------------------*/
static unsigned int lavm_simple_profiling_incpos(unsigned int *time) {
    unsigned int pos = 0;
    unsigned long flags;
    
    if(simple_profiling.enabled == 1) {
        spin_lock_irqsave(&simple_profiling.lock, flags);
        *time = avm_profile_counter();
         pos = atomic_inc_return(&(simple_profiling.pos)) - 1;
        spin_unlock_irqrestore(&simple_profiling.lock, flags);
        if(pos >= simple_profiling.len - 1) {
            simple_profiling.enabled = 0;
            simple_profiling.end_time = jiffies | 1;
            printk("[simple_profiling] buffer full: %u entries recorded\n", simple_profiling.len);
            return (unsigned int)-1;
        }
    } else if(simple_profiling.enabled == 2) {
        spin_lock_irqsave(&simple_profiling.lock, flags);
        *time = avm_profile_counter();
        pos = atomic_inc_return(&(simple_profiling.pos)) - 1;
        if(pos >= simple_profiling.len) {        
            printk("[simple_profiling] wraparround: %u entries recorded\n", simple_profiling.len);
            if(simple_profiling.end_time == 0) simple_profiling.end_time = jiffies | 1;
            simple_profiling.wraparround = 1;
            simple_profiling.pos.counter = pos = 0;
        }
        spin_unlock_irqrestore(&simple_profiling.lock, flags);
    }
    return pos;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int __avm_simple_profiling(struct pt_regs *regs, unsigned int irq_num) {
    unsigned int pos, epc, time;
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    struct _avm_profile_data *data;
#endif /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    /*--- if(simple_profiling.enabled == 0) return 0; ---*/
    if(simple_profiling.mode != 1) return 0;
    if(!(simple_profiling.mask & (1 << avm_profile_data_type_code_address_info))) return 0;

#if defined(CONFIG_ARM)
    epc = regs->ARM_pc;
#endif /*--- #if defined(CONFIG_ARM) ---*/
#if defined(CONFIG_MIPS)
    epc = regs->cp0_epc;
#endif /*--- #if defined(CONFIG_MIPS) ---*/
    pos  = lavm_simple_profiling_incpos(&time);

    if(pos == (unsigned int)-1) return 0;

#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    data = &((struct _avm_profile_data *)(simple_profiling.data[pos / profile_DataSetsPerBlock]))[pos % profile_DataSetsPerBlock];
    data->type   = avm_profile_data_type_code_address_info;
    data->cpu_id = smp_processor_id();
    data->curr = current;
    data->id   = irq_num;
    data->addr = epc;
    data->time = time;
    data->total_access   = avm_profile_sdramacess();
    data->total_activate = avm_profile_sdramactivate();
#else /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    simple_profiling.data[pos].type = avm_profile_data_type_code_address_info;
    simple_profiling.data[pos].cpu_id= smp_processor_id();
    simple_profiling.data[pos].curr = current;
    simple_profiling.data[pos].id   = irq_num;
    simple_profiling.data[pos].addr = epc;
    simple_profiling.data[pos].time = time;
    simple_profiling.data[pos].total_access   = avm_profile_sdramacess();
    simple_profiling.data[pos].total_activate = avm_profile_sdramactivate();
#endif /*--- #else ---*/ /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    return time;
}
EXPORT_SYMBOL(__avm_simple_profiling);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

#ifdef CONFIG_AVM_SIMPLE_PROFILING_YIELD
unsigned int avm_simple_profiling_yield(unsigned int epc, unsigned int cpu_id, unsigned int irq_num) {
    unsigned int pos, time;
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    struct _avm_profile_data *data;
#endif /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    if(simple_profiling.enabled == 0) return 0;
    if(simple_profiling.mode != 1) return 0;
    if(!(simple_profiling.mask & (1 << avm_profile_data_type_code_address_yield))) return 0;

    pos  = lavm_simple_profiling_incpos(&time);

    if(pos == (unsigned int)-1) return 0;

#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    data = &((struct _avm_profile_data *)(simple_profiling.data[pos / profile_DataSetsPerBlock]))[pos % profile_DataSetsPerBlock];
    data->type   = avm_profile_data_type_code_address_info;
    data->cpu_id = cpu_id;
    data->curr = current;
    data->id   = irq_num;
    data->addr = epc;
    data->time = time;
#ifdef CONFIG_AVM_SIMPLE_PROFILING_YIELD_PCNT
    data->total_access   = read_c0_perfcnt(0,2);
    data->total_activate = read_c0_perfcnt(1,2);
#else
    data->total_access   = avm_profile_sdramacess();
    data->total_activate = avm_profile_sdramactivate();
#endif /*--- #ifdef CONFIG_AVM_SIMPLE_PROFILING_YIELD_PCNT ---*/
#else /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    simple_profiling.data[pos].type = avm_profile_data_type_code_address_info;
    simple_profiling.data[pos].cpu_id= cpu_id;
    simple_profiling.data[pos].curr = current;
    simple_profiling.data[pos].id   = irq_num;
    simple_profiling.data[pos].addr = epc;
    simple_profiling.data[pos].time = time;
    simple_profiling.data[pos].total_access   = avm_profile_sdramacess();
    simple_profiling.data[pos].total_activate = avm_profile_sdramactivate();
#endif /*--- #else ---*/ /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    return time;
}
#endif
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __avm_simple_profiling_skb(unsigned int addr, unsigned int where, struct sk_buff *skb) {
    unsigned int pos, time;
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    struct _avm_profile_data *data;
#endif /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    /*--- if(simple_profiling.enabled == 0) return; ---*/
//     if(simple_profiling.mode != 2) return;
    if(!(simple_profiling.mask & (1 << avm_profile_data_type_trace_skb))) return;

    pos  = lavm_simple_profiling_incpos(&time);
    if(pos == (unsigned int)-1) return;

#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    data = (struct _avm_profile_data *)(simple_profiling.data[pos / profile_DataSetsPerBlock]) + (pos % profile_DataSetsPerBlock);
    data->curr = current;
#endif /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/

#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    data->type = avm_profile_data_type_trace_skb;
    data->cpu_id = smp_processor_id();
    data->id   = skb_cloned(skb);
    data->addr = (unsigned long)__builtin_return_address(0);
    data->time = time;
    data->total_access   = skb->uniq_id & 0xFFFFFF;
    data->total_activate = where;
#else /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    simple_profiling.data[pos].type = avm_profile_data_type_trace_skb;
    simple_profiling.data[pos].cpu_id= smp_processor_id();
    simple_profiling.data[pos].id   = skb_cloned(skb);
    simple_profiling.data[pos].addr = addr;
    simple_profiling.data[pos].time = time;
    simple_profiling.data[pos].total_access   = skb->uniq_id & 0xFFFFFF;
    simple_profiling.data[pos].total_activate = where;
#endif /*--- #else ---*/ /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
}
EXPORT_SYMBOL(__avm_simple_profiling_skb);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_simple_profiling_irq_onoff(unsigned int switch_on, unsigned int addr, unsigned int id) {
#if defined(CONFIG_AVM_SIMPLE_PROFILING_IRQ_ON_OFF)
    if(!(simple_profiling.mask & ((1 << avm_profile_data_type_irq_on) | (1 << avm_profile_data_type_irq_off))))
            return;

    avm_simple_profiling_log(switch_on ? avm_profile_data_type_irq_on : avm_profile_data_type_irq_off, addr, id);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING_IRQ_ON_OFF) ---*/
}
EXPORT_SYMBOL(avm_simple_profiling_irq_onoff);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int avm_simple_profiling_current_pos;

void __avm_simple_profiling_log(enum _avm_profile_data_type type, unsigned int addr, unsigned int id) {
    unsigned int pos, time;
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    struct _avm_profile_data *data;
#endif /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    /*--- if(simple_profiling.enabled == 0) return; ---*/
    if(simple_profiling.mode != 1) return;
    if(!(simple_profiling.mask & (1 << type))) return;

    pos  = lavm_simple_profiling_incpos(&time);
    if(pos == (unsigned int)-1) return;

avm_simple_profiling_current_pos = pos;
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    data = (struct _avm_profile_data *)(simple_profiling.data[pos / profile_DataSetsPerBlock]) + (pos % profile_DataSetsPerBlock);
    data->curr = current;
#endif /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/

    switch(type) {
        default:
        case avm_profile_data_type_free:
        case avm_profile_data_type_unknown:
            break;

        case avm_profile_data_type_text:
        case avm_profile_data_type_code_address_info:
        case avm_profile_data_type_data_address_info:
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
            data->type = type;
            data->cpu_id = smp_processor_id();
            data->addr = addr;
            data->time = time;
            data->total_access   = avm_profile_sdramacess();
            data->total_activate = avm_profile_sdramactivate();
#else /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
            simple_profiling.data[pos].type = type;
            simple_profiling.data[pos].cpu_id = smp_processor_id();
            simple_profiling.data[pos].addr = addr;
            simple_profiling.data[pos].time = time;
            simple_profiling.data[pos].total_access   = avm_profile_sdramacess();
            simple_profiling.data[pos].total_activate = avm_profile_sdramactivate();
#endif /*--- #else ---*/ /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
            break;

        case avm_profile_data_type_hw_irq_begin:
        case avm_profile_data_type_hw_irq_end:
        case avm_profile_data_type_sw_irq_begin:
        case avm_profile_data_type_sw_irq_end:
        case avm_profile_data_type_timer_begin:
        case avm_profile_data_type_timer_end:
        case avm_profile_data_type_tasklet_begin:
        case avm_profile_data_type_tasklet_end:
        case avm_profile_data_type_hi_tasklet_begin:
        case avm_profile_data_type_hi_tasklet_end:
        case avm_profile_data_type_workitem_begin:
        case avm_profile_data_type_workitem_end:
        case avm_profile_data_type_cpphytx_begin:
        case avm_profile_data_type_cpphytx_end:
        case avm_profile_data_type_cpphyrx_begin:
        case avm_profile_data_type_cpphyrx_end:
        case avm_profile_data_type_func_begin:
        case avm_profile_data_type_func_end:
        case avm_profile_data_type_trigger_tasklet_begin:
        case avm_profile_data_type_trigger_tasklet_end:
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
            data->type = type;
            data->cpu_id = smp_processor_id();
            data->id   = id;
            data->addr = addr;
            data->time = time;
            data->total_access   = avm_profile_sdramacess();
            data->total_activate = avm_profile_sdramactivate();
#else /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
            simple_profiling.data[pos].type   = type;
            simple_profiling.data[pos].cpu_id = smp_processor_id();
            simple_profiling.data[pos].id   = id;
            simple_profiling.data[pos].addr = addr;
            simple_profiling.data[pos].time = time;
            simple_profiling.data[pos].total_access   = avm_profile_sdramacess();
            simple_profiling.data[pos].total_activate = avm_profile_sdramactivate();
#endif /*--- #else ---*/ /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
            break;
    }
}
EXPORT_SYMBOL(__avm_simple_profiling_log);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __avm_simple_profiling_text(const  char *text) {
    unsigned int pos, time;
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    struct _avm_profile_data *data;
#endif /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    /*--- if(simple_profiling.enabled == 0) return; ---*/
    if(simple_profiling.mode != 1) return;
    if(!(simple_profiling.mask & (1 << avm_profile_data_type_text))) return;
    pos  = lavm_simple_profiling_incpos(&time);
    if(pos == (unsigned int)-1) return;
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    data = (struct _avm_profile_data *)(simple_profiling.data[pos / profile_DataSetsPerBlock]) + (pos % profile_DataSetsPerBlock);
    data->type = avm_profile_data_type_text;
    data->cpu_id = smp_processor_id();
    data->time = time;
    data->addr = (unsigned int)text;
    data->total_access   = avm_profile_sdramacess();
    data->total_activate = avm_profile_sdramactivate();
#else /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    simple_profiling.data[pos].type = avm_profile_data_type_text;
    simple_profiling.data[pos].cpu_id = smp_processor_id();
    simple_profiling.data[pos].time = time;
    simple_profiling.data[pos].addr = (unsigned int)text;
    simple_profiling.data[pos].total_access   = avm_profile_sdramacess();
    simple_profiling.data[pos].total_activate = avm_profile_sdramactivate();
#endif /*--- #else ---*/ /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    return;
}
EXPORT_SYMBOL(__avm_simple_profiling_text);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

#ifdef CONFIG_AVM_SIMPLE_PROFILING_YIELD

static int avm_simple_profiling_yield_handler( int signal, void *ref ){

    unsigned int irq = 0;
	unsigned long vpflags;
	unsigned int ntc = 2; //currently we are just interested in our SMP VPE contexts
	unsigned int tcs_to_profile_mask = ( 1 << 1 ) | ( 1 << 0 );  //default - will be replaced in YIELD_PCNT-Mode
	int tc;

#ifdef CONFIG_AVM_SIMPLE_PROFILING_YIELD_PCNT
    tcs_to_profile_mask = ack_irqs_reload_counter();
#else
    ifx_gptu_timer_yield_ack( simple_profiling.yield_timer_no, yield_freq_randomizer_percent );
#endif

	vpflags = dvpe();

	for (tc = 0; tc < ntc; tc++) {
	    if ( tcs_to_profile_mask & (1 << tc) ){
            unsigned int tcrestart_val;
            unsigned int cpu_id = tc;

            settc(tc);
            tcrestart_val = read_tc_c0_tcrestart();
            avm_simple_profiling_yield( tcrestart_val, cpu_id, irq );
	    }
	}

	evpe(vpflags);

    return YIELD_HANDLED ;
}
#endif

/*------------------------------------------------------------------------------------------*\
 * on = 2: wraparround-mode
\*------------------------------------------------------------------------------------------*/
void avm_simple_profiling_enable(unsigned int on, unsigned int mask, unsigned int *count, unsigned long *timediff) {
    if(on) {
        if(mask) {
            profile_current_mask = mask;
            profile_current_start_handle = on;
        }
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
        if(simple_profiling.data == NULL) {
            int i;
            simple_profiling.data = (void **)kmalloc(sizeof(void *) * profile_BlockNeeded, GFP_ATOMIC);
            memset(simple_profiling.data, 0, sizeof(void *) * profile_BlockNeeded);
            simple_profiling.len = profile_DataSetsPerBlock * profile_BlockNeeded;
            for(i = 0 ; i < profile_BlockNeeded ; i++) {
                struct page *ptr = alloc_pages(__GFP_NORETRY, 0);
                if(ptr == NULL) {
                    simple_profiling.len = profile_DataSetsPerBlock * i;
                    printk("[avm_simple_profiling_enable] resize %d pages instead %d pages\n", i, profile_BlockNeeded);
                    break;
                }
                simple_profiling.data[i] = (void *)lowmem_page_address(ptr);
                /*--- printk("\t%d: %x ==> %x\n", i, ptr, simple_profiling.data[i]); ---*/
            }
            printk("[avm_simple_profiling_enable] need %d pages for %d bytes Buffer %d samples per block, trace-mask 0x%x\n",
                    i, simple_profiling.len, profile_DataSetsPerBlock, mask);
        }
#endif /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/

        atomic_set(&(simple_profiling.pos), 0);
        simple_profiling.start_time = jiffies;
        simple_profiling.end_time   = 0;
        simple_profiling.wraparround = 0;
        simple_profiling.mask = mask;
        switch (on) {
            default:
                printk(KERN_ERR "Unknown profiling mode. Assuming normal profiling without waparound.\n");
            case 2:
                simple_profiling.enabled = 1;
                simple_profiling.mode = 1;
                break;
            case 6:
                simple_profiling.enabled = 2;
                simple_profiling.mode = 1;
                break;
            case 7:
                simple_profiling.enabled = 1;
                simple_profiling.mode = 2;
                break;
            case 8:
                simple_profiling.enabled = 2;
                simple_profiling.mode = 2;
                break;
        }
        if(count) *count = 0;
        if(timediff)*timediff = 0;
        avm_profile_counter_init();
#ifdef CONFIG_AVM_SIMPLE_PROFILING_YIELD
        simple_profiling.yield_handler = NO_YIELD_HANDLER;
        if ( mask == ( 1 << avm_profile_data_type_code_address_yield ) ){
                simple_profiling.yield_ref = &simple_profiling;
#ifdef CONFIG_AVM_SIMPLE_PROFILING_YIELD_PCNT
                simple_profiling.yield_signal = YIELD_SIGNAL_PERFORMANE_COUNTER;
#else
                simple_profiling.yield_signal = YIELD_SIGNAL_TIMER1;
#endif
                printk(KERN_ERR "[%s] start yield profiling callback=%p ref_addr=%#x \n", __FUNCTION__, avm_simple_profiling_yield_handler, simple_profiling.yield_ref );
                simple_profiling.yield_handler = request_yield_handler( simple_profiling.yield_signal, avm_simple_profiling_yield_handler, simple_profiling.yield_ref );

#ifndef CONFIG_AVM_SIMPLE_PROFILING_YIELD_PCNT
                if ( simple_profiling.yield_handler  >= 0 ) {

                    printk(KERN_ERR "[%s] request_yield_handler successful  req_res =%d\n", __FUNCTION__,  simple_profiling.yield_handler);

                    /* --- freq in 1/1000 Hz  --- */
                    simple_profiling.yield_timer_no = ifx_gptu_timer_set( 0, yield_thread_freq * 1000, 1, 0, TIMER_FLAG_YIELD_MODE , 0, 0 );

                    switch( simple_profiling.yield_timer_no ) {
                        case TIMER1A:
                            YIELDEN_GPCT_1A(1);
                            break;
                        case TIMER1B:
                            YIELDEN_GPCT_1B(1);
                            break;
                        case TIMER2A:
                            YIELDEN_GPCT_2A(1);
                            break;
                        case TIMER2B:
                            YIELDEN_GPCT_2B(1);
                            break;
                        case TIMER3A:
                            YIELDEN_GPCT_3A(1);
                            break;
                        case TIMER3B:
                            YIELDEN_GPCT_3B(1);
                            break;
                        default:
                            printk(KERN_ERR "[%s] could not reserve valid HW Timer: %d\n", __FUNCTION__, simple_profiling.yield_timer_no);
                            ifx_gptu_timer_free( simple_profiling.yield_timer_no );
                            simple_profiling.yield_handler = NO_YIELD_HANDLER;
                            return;
                    }
                    ifx_gptu_timer_start( simple_profiling.yield_timer_no, 0 );
                    printk(KERN_ERR "[%s] request_yield_handler successful timer_no= %d\n", __FUNCTION__, simple_profiling.yield_timer_no);
                }
#endif
        }
#endif
        return;
    }
    simple_profiling.enabled = 0;
    avm_profile_counter_exit();
#ifdef CONFIG_AVM_SIMPLE_PROFILING_YIELD
    if ( simple_profiling.yield_handler >= 0 ){

#ifndef CONFIG_AVM_SIMPLE_PROFILING_YIELD_PCNT
        ifx_gptu_timer_stop( simple_profiling.yield_timer_no );
        ifx_gptu_timer_free( simple_profiling.yield_timer_no );
        free_yield_handler( simple_profiling.yield_signal, simple_profiling.yield_ref );

        switch( simple_profiling.yield_timer_no ) {
            case TIMER1A:
                YIELDEN_GPCT_1A(0);
                break;
            case TIMER1B:
                YIELDEN_GPCT_1B(0);
                break;
            case TIMER2A:
                YIELDEN_GPCT_2A(0);
                break;
            case TIMER2B:
                YIELDEN_GPCT_2B(0);
                break;
            case TIMER3A:
                YIELDEN_GPCT_3A(0);
                break;
            case TIMER3B:
                YIELDEN_GPCT_3B(0);
                break;
        }
#endif
        simple_profiling.yield_handler = NO_YIELD_HANDLER;
    }
#endif
    if(simple_profiling.end_time  == 0) simple_profiling.end_time = jiffies;
    if(count) {
        if(simple_profiling.wraparround == 0) {
            *count = simple_profiling.pos.counter;
        } else {
            *count = simple_profiling.len;
        }    
    }
    printk("[simple_profiling] off: %u entries\n", simple_profiling.wraparround ? simple_profiling.len : simple_profiling.pos.counter);
    if(timediff) {
        if(time_after(simple_profiling.start_time, simple_profiling.end_time)) {
            *timediff = simple_profiling.end_time + 0xFFFFFFFF - simple_profiling.start_time;
        } else { 
            *timediff = simple_profiling.end_time - simple_profiling.start_time;
        }
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_simple_profiling_restart(void) {
    if(profile_current_start_handle || profile_current_mask) {
        /*--- printk("[%s] Push-Button triggering profile %d mask=0x%x\nUse:\n" ---*/
               /*--- "  'echo profiler 0 >/dev/debug' to stop profiling\n" ---*/
               /*--- "  'echo profiler 2 >/dev/debug' to write results to /var/profile.csv\n", ---*/ 
               /*--- __FUNCTION__, profile_current_start_handle, profile_current_mask); ---*/
        avm_simple_profiling_enable(profile_current_start_handle, profile_current_mask, NULL, NULL);
    }
    else {
        printk("[%s] Push-Button for profiling ignored (not initialized)\n", __func__);
    }
    return;
}
EXPORT_SYMBOL(avm_simple_profiling_restart);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _avm_profile_data *avm_simple_profiling_by_idx(unsigned int idx) {
    unsigned int pos;
    struct _avm_profile_data *data;
    if(simple_profiling.wraparround == 0) {
        if(idx < simple_profiling.pos.counter) {
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
            data = ((struct _avm_profile_data *)(simple_profiling.data[idx / profile_DataSetsPerBlock]) + (idx % profile_DataSetsPerBlock));
#else/*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
            data = &simple_profiling.data[idx];
#endif/*--- #else ---*//*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
            return data;
        }
        return NULL;
    }
    pos = simple_profiling.pos.counter + idx;
    if(pos >= simple_profiling.len) {
        pos -= simple_profiling.len;
        if(pos >= simple_profiling.pos.counter) {
            return NULL;
        }
    }
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    data = ((struct _avm_profile_data *)(simple_profiling.data[pos / profile_DataSetsPerBlock]) + (pos % profile_DataSetsPerBlock));
#else/*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    data = &simple_profiling.data[pos];
#endif/*--- #else ---*//*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    return data;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_simple_profiling_init(void) {
    spin_lock_init(&simple_profiling.lock);
#if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2)
    simple_profiling.len = 0;
#else /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    simple_profiling.len = PROFILE_BUFFER_LEN;
#endif /*--- #else ---*/ /*--- #if defined(AVM_PROFILING_VERSION) && (AVM_PROFILING_VERSION >= 2) ---*/
    simple_profiling.enabled = 0;
    atomic_set(&(simple_profiling.pos), 0);
    printk(KERN_ERR "[simple_profiling] %u entries %u min\n", simple_profiling.len, CONFIG_AVM_PROFILING_TRACE_MODE);
    return 0;
}

module_init(avm_simple_profiling_init);
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/

