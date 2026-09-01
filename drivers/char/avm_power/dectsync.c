#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/file.h>
#include <linux/avm_power.h>
#include <linux/avm_event.h>
#include <linux/netdevice.h>
#if defined(CONFIG_MIPS_UR8)
#include <asm/mach-ur8/hw_gpio.h>
#include <asm/mach-ur8/hw_irq.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
#include <asm/mach-ur8/ur8_clk.h>
#else
#include <asm/mach_avm.h>
#endif

#endif

#define CPUFREQ_VARIABLE

#define ARRAY_EL(a) (sizeof(a) / sizeof((a)[0]))

#if defined(DECTSYNC_PATCH) 
#include "dectsync.h"
static spinlock_t dectsync_lock = SPIN_LOCK_UNLOCKED;
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
__inline static unsigned long LOCK(void) {
    unsigned long flags;
    spin_lock_irqsave(&dectsync_lock, flags);
    return flags;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
__inline static void UNLOCK(unsigned long flags) {
	spin_unlock_irqrestore(&dectsync_lock, flags);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
/*--- #define DEBUG_DECTSYNC ---*/
#if defined(DEBUG_DECTSYNC)
#define DBG_DS_TRC(args...)     printk(KERN_ERR args)
/*--- #define DBG_DS_TRC(args...)  ---*/
#else
#define DBG_DS_TRC(args...)
#endif
#define DBG_DS_ERR(args...)     printk(KERN_ERR args)

#define TIME_DIFF(act, old) ((act) - (old)) 

#define DECT_SYNC_CHECK()   (hw_gpio->InputData.Register & (1 << DECT_SYNCGPIO))
#define KBIT_TO_BYTES(kbit, sec) ((kbit) * (sec) * 1000 / 8)

#define UR8_MHZ  180L
#if !defined(CPUFREQ_VARIABLE) 
/*--- #define CLK_TO_NSEC(a) ((a) * (1000L /  UR8_MHZ))   ---*/
#define CLK_TO_NSEC(a) (((signed long)(a) * (1000L / 20)) /  (UR8_MHZ / 20))  /*--- ggT(1000, 180) = 20 ---*/
#define CLK_TO_USEC(a) ((a) / UR8_MHZ )
#define CLK_TO_MSEC(a) ((a) / UR8_MHZ / 1000)
#define USEC_TO_CLK(a) ((a) * UR8_MHZ)

#else/*--- #if !defined(CPUFREQ_VARIABLE)  ---*/
static unsigned int cpufreq_mode;
#define UR8_MHZ_1    (300L / 2)
#define UR8_MHZ_2    (240L / 2)    
static signed long cpufreq_table[] = {
    UR8_MHZ,
    UR8_MHZ_1,
    UR8_MHZ_2
};
#if (UR8_MHZ_1 == (300L / 2)) && (UR8_MHZ_2 == (240L / 2)) 
#define CLK_TO_NSEC(a) ((cpufreq_mode == 0) ?  (((signed long)(a) * (1000L / 20)) / (UR8_MHZ   / 20)) : \
                        (cpufreq_mode == 1) ?  (((signed long)(a) * (1000L / 50)) / (UR8_MHZ_1 / 50)) : \
                                               (((signed long)(a) * (1000L / 40)) / (UR8_MHZ_2 / 40)))
#else/*--- #if defined(UR8_MHZ_1 == 300L && UR8_MHZ_2 == 200)  ---*/
#define CLK_TO_NSEC(a) ((((signed long)(a) * 1000L)) / cpufreq_table[cpufreq_mode])
#endif
#define CLK_TO_USEC(a) ((a) / cpufreq_table[cpufreq_mode])
#define CLK_TO_MSEC(a) ((a) / cpufreq_table[cpufreq_mode] / 1000)
#define USEC_TO_CLK(a) ((cpufreq_mode == 0) ? ((a) * UR8_MHZ)    :\
                        (cpufreq_mode == 1) ? ((a) * UR8_MHZ_1)  :\
                                              ((a) * UR8_MHZ_2))
#endif/*--- #else ---*//*--- #if !defined(CPUFREQ_VARIABLE)  ---*/

#define DECT_SYNCGPIO       CONFIG_AVM_DECT_SYNC
#define DECTFAILCNTDOWN     300

void timer_dectsynchandler_end(void);

struct _dectsynchandler {
    enum { resetmode, 
           inactive, 
           invalid_clk, 
           init_dectsync, 
           calibration_mode, 
           calibration_mode2, 
           calibrate_timerirq_delay, 
           calibrate_timerirq, 
           calibrate_timerirq_high, 
           timerset_ok, 
           wlantraffic_mode } state; 
    unsigned long lowedgecycles;        /*--- absolute Zeit: letzte Lowflanke ---*/
    unsigned long lowcycles;            /*--- Laenge des Lowpegels  ---*/
    unsigned long highcycles;           /*--- Laenge des Highpegels ---*/
    unsigned int wastepercent;
    unsigned long waste_idle;           /*--- Zeit, die der Irq blockiert ---*/
    /*--- unsigned long drift_failed; ---*/
    unsigned long synclost;             /*--- Aufsummierung Lowflanke nicht gefdunen ---*/
    int failcntdown;
    int lastwasgood;                    /*--- Flag ob mit letztem Interrupt Lowflanke gefunden wurde ---*/
    unsigned long periodjiffies;
    unsigned long periodcycle;
    int goodcnt, periodcnt, prefcycle;
    signed   long drift;
    unsigned long violation;
    unsigned long timeout;
    unsigned long latency;
    unsigned long goodpercent;
    unsigned long last_wlan_traffic;
    unsigned int  run;
    unsigned int  clk;
    int ignore_run;
    int start;
    void *prefetch_code;
    size_t prefetch_len;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    struct hrtimer hr_timer;
#endif/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32) ---*/
} gdectsync;

/*--- #define DECTSYNC_STAT ---*/
#if defined(DECTSYNC_STAT)
struct _generic_stat {
    signed long cnt; 
    signed long avg;
    signed long min;
    signed long max;
};

struct _dectsynstat {
    unsigned long startjiffies;
    struct _generic_stat idle_waste;
    unsigned long idlewastecycle;
    struct _generic_stat idle_until_irq;
    struct _generic_stat idle_until_low;

    struct _generic_stat run;
    struct _generic_stat prefcycle;

    unsigned int notimer_set_ok_endcycle;
    unsigned long notimer_set_ok_wasteidle;
    unsigned long notimer_set_ok_wasterun;

    struct _generic_stat waste_notimer_set_ok;
    struct _generic_stat notimer_set_ok;

    unsigned int firsthighlevel_failed;
    struct _generic_stat firsthighlevel;
    unsigned int firstlowedge_failed;
    struct _generic_stat firstlowedge;

    unsigned int highedge_search;
    struct _generic_stat highedge;

    unsigned long initial_cycles;
    struct _generic_stat initialtime;
    struct _generic_stat latency;
    struct _generic_stat timeout;
    struct _generic_stat wait_for_highlevel;
    struct _generic_stat wait_for_lowlevel;
    struct _generic_stat lastlowedgediff;
    struct _generic_stat lowcycles;
    struct _generic_stat highcycles;
    struct _generic_stat drift;
    struct _generic_stat ok_ret;
    struct _generic_stat retrigger_ret;

    unsigned int irqlast;
    struct _generic_stat irqexpire;
    struct _generic_stat irqdiff;
} gdectsyncstat;

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void init_generic_stat(struct _generic_stat *pgstat) {
    pgstat->min = LONG_MAX;
    pgstat->max = LONG_MIN;
    pgstat->cnt = 0;
    pgstat->avg = 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void generic_stat(struct _generic_stat *pgstat, signed long val) {
    if(pgstat->cnt == 0) {
        init_generic_stat(pgstat);
    }
    if(val > pgstat->max) pgstat->max = val;
    if(val < pgstat->min) pgstat->min = val;
    pgstat->avg += val;
    pgstat->cnt++;
}
/*--------------------------------------------------------------------------------*\
 * timebase: 0 unveraendert, 1: clk ->  usec, 2: clk -> nsec
 * reset: Statistik ruecksetzen
\*--------------------------------------------------------------------------------*/
void display_generic_stat(char *prefix, struct _generic_stat *pgstat, unsigned int timebase, unsigned int reset) {
    struct _generic_stat gstat;
    long flags = LOCK();
    signed long cnt = pgstat->cnt;
    if(cnt == 0) {
        UNLOCK(flags);
        return;
    }
    memcpy(&gstat,pgstat, sizeof(gstat));
    if(reset) {
        pgstat->cnt = 0;
    }
    UNLOCK(flags);
    switch(timebase) {
        case 0: 
            printk("%s[%ld] min=%ld max=%ld avg=%ld\n", prefix, cnt, gstat.min, gstat.max, gstat.avg / cnt);
            break;
        case 1: 
            printk("%s[%ld] min=%ld max=%ld avg=%ld usec\n", prefix, cnt, CLK_TO_USEC(gstat.min), CLK_TO_USEC(gstat.max), CLK_TO_USEC(gstat.avg / cnt));
            break;
        case 2: 
            printk("%s[%ld] min=%ld max=%ld avg=%ld nsec\n", prefix, cnt, CLK_TO_NSEC(gstat.min), CLK_TO_NSEC(gstat.max), CLK_TO_NSEC(gstat.avg / cnt));
            break;
    }
}
#endif/*--- #if defined(DECTSYNC_STAT) ---*/

static unsigned long usec_runtable[6] = {
    5000,  /*--- usec -> 16 %  ---*/
    5000,  /*--- usec -> 32 %  ---*/
    5000,  /*--- usec -> 48 %  ---*/
    4000,  /*--- usec -> 64 %  ---*/
    3000,  /*--- usec -> 80 %  ---*/
    1000   /*--- usec -> 96 %  ---*/
};
static unsigned long runtable[6];

/*--------------------------------------------------------------------------------*\
 * ret: 1 error
 * initialisiert runtable
\*--------------------------------------------------------------------------------*/
static inline int cpufreq_init(unsigned int freq) {
    unsigned int i;
    freq = (freq / 2 / 1000000);
#if defined(CPUFREQ_VARIABLE)
    for(i = 0 ; i < ARRAY_EL(cpufreq_table); i++) {
        if((signed long)freq == cpufreq_table[i]) {
            break;
        }
    }
    if(i >= ARRAY_EL(cpufreq_table)) {
        return 1;
    }
    cpufreq_mode = i;
    /*--- printk("cpufreq_mode %u\n", cpufreq_mode); ---*/
#else/*--- #if defined(CPUFREQ_VARIABLE) ---*/
    if(freq != UR8_MHZ){
        return 1;
    }
#endif
    for(i = 0 ; i < ARRAY_EL(runtable); i++) {
        runtable[i] = USEC_TO_CLK(usec_runtable[i]);
        /*--- printk("%ld -> %ld\n",  usec_runtable[i], runtable[i]); ---*/
    }
    return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void get_wlantraffic(unsigned long *rx_bytes, unsigned long *tx_bytes) {
    struct net_device_stats *pnetdevicestat;
    struct net_device *pnetdevice = dev_get_by_name(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
                                                    &init_net,
#endif
                                                    "ath0");

    *rx_bytes = 0;
    *tx_bytes = 0;
    if(pnetdevice == NULL) {
        return;
    }

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 28)
    if(pnetdevice->get_stats == NULL) {
        dev_put(pnetdevice);
        return;
    }
    pnetdevicestat = pnetdevice->get_stats(pnetdevice);
#else  /*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 28) ---*/
    if((pnetdevice->netdev_ops == NULL) || (pnetdevice->netdev_ops->ndo_get_stats == NULL)) {
        dev_put(pnetdevice);
        return;
    }
    pnetdevicestat = pnetdevice->netdev_ops->ndo_get_stats(pnetdevice);
#endif /*--- #else ---*/  /*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 28) ---*/

    if(pnetdevicestat == NULL) {
        dev_put(pnetdevice);
        return;
    }
    /*--- printk("NetStat rx %ld tx %ld\n", pnetdevicestat->rx_bytes, pnetdevicestat->tx_bytes); ---*/
    *rx_bytes = pnetdevicestat->rx_bytes; 
    *tx_bytes = pnetdevicestat->tx_bytes;
    dev_put(pnetdevice);
}
#if defined(DECTSYNC_STAT)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void dectsyncstat(void) {
    display_generic_stat("---> irqdiff", &gdectsyncstat.irqdiff, 1, 1);
    display_generic_stat("expire", &gdectsyncstat.irqexpire, 1, 1);
    display_generic_stat("time until low (msec)", &gdectsyncstat.idle_until_low, 0, 1);
    display_generic_stat("waste in idle (absolut)", &gdectsyncstat.idle_waste, 1, 1);
    display_generic_stat("time until irqset (msec)", &gdectsyncstat.idle_until_irq, 0, 1);
    display_generic_stat("firsthighlevel cnt", &gdectsyncstat.firsthighlevel, 0, 1);
    display_generic_stat("firstlowedge cnt", &gdectsyncstat.firstlowedge, 0, 1);
    display_generic_stat("highedge cnt", &gdectsyncstat.highedge, 0, 1);
    display_generic_stat("calibrate_timerirq until timerset_ok", &gdectsyncstat.initialtime, 1, 1);
    display_generic_stat("latency", &gdectsyncstat.latency, 1, 1);
    display_generic_stat("timeout", &gdectsyncstat.timeout, 1, 1);
    display_generic_stat("waitforhighlevel", &gdectsyncstat.wait_for_highlevel,1 , 1);
    display_generic_stat("waitforlowlevel", &gdectsyncstat.wait_for_lowlevel, 1, 1);
    display_generic_stat("lastlowedgediff", &gdectsyncstat.lastlowedgediff, 1, 1);
    display_generic_stat("usec before irq", &gdectsyncstat.prefcycle, 1, 1);
    display_generic_stat("lowcycles", &gdectsyncstat.lowcycles, 1, 1);
    display_generic_stat("highcycles", &gdectsyncstat.highcycles, 1, 1);
    display_generic_stat("drift", &gdectsyncstat.drift, 2, 1);
    display_generic_stat("ok_ret", &gdectsyncstat.ok_ret, 1, 1);
    display_generic_stat("retrigger_ret", &gdectsyncstat.retrigger_ret, 1, 1);
    display_generic_stat("no timerset_ok(absolut)", &gdectsyncstat.notimer_set_ok, 1, 1);
    display_generic_stat("block in no timerset_ok(absolut)", &gdectsyncstat.waste_notimer_set_ok, 1, 1);
    display_generic_stat("run", &gdectsyncstat.run, 0, 1);
    display_generic_stat("usec before irq", &gdectsyncstat.prefcycle, 1, 1);
}
#endif/*--- #if defined(DECTSYNC_STAT) ---*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static enum hrtimer_restart dectsynchtimer_handler(struct hrtimer *timer __attribute__((unused))) {
    unsigned long actcycles;
    unsigned int expire; 
    ktime_t ktime, now;

    actcycles = get_cycles();
    now = hrtimer_cb_get_time(&gdectsync.hr_timer);

    if((expire = timer_dectsynchandler(USEC_TO_CLK(10 * 1000))) == 0 ) {
        DBG_DS_TRC("<[%lu]p HRTIMER_NORESTART state=%d>", jiffies, gdectsync.state);
#if defined(DECTSYNC_STAT)
        dectsyncstat();
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
        return HRTIMER_NORESTART;
    }
    actcycles = TIME_DIFF(get_cycles(), actcycles);
    ktime = ktime_set(0, CLK_TO_NSEC(expire + actcycles)); /*--- relative wakeup ---*/
    hrtimer_forward(&gdectsync.hr_timer, now, ktime);
    return HRTIMER_RESTART;
}
#endif/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32) ---*/

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned int check_irq_set(unsigned int irq) {
    struct _irq_hw *IRQ = (struct _irq_hw *)UR8_IRQ_CTRL_BASE;
    unsigned int _irq = irq - (MIPS_EXCEPTION_OFFSET);
    return(IRQ->status_set_reg[_irq / 32] & (1 << (_irq % 32)));
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
#if defined(CONFIG_UR8_CLOCK_SWITCH)
static unsigned int lclock_notify(enum _avm_clock_id clock_id, unsigned int new_clk) {
    /*--- printk("[dectsync]%s %d\n", __func__, new_clk); ---*/
    if(gdectsync.state == inactive) {
        return 0;
    }
    if(clock_id != avm_clock_id_cpu) {
        return 0;
    }
    if(gdectsync.clk ==  (new_clk / 2 / 1000000)) {
        /*--- keine Aenderung der clk -> ignorieren ---*/
        return 0;
    }
    if(cpufreq_init(new_clk)) {
        printk("[dectsync]invalid clock %u -> inactive\n", new_clk);
        gdectsync.state = invalid_clk;
    } else {
        if(gdectsync.state < calibrate_timerirq) {
            gdectsync.state = init_dectsync;
        } else {
            gdectsync.state = calibrate_timerirq;
        }
        gdectsync.clk   = new_clk / 2 / 1000000;
    }
    return 0;
}
#endif /*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/
/*--------------------------------------------------------------------------------*\
 * start = 0: nur ignore_run setzen
\*--------------------------------------------------------------------------------*/
void start_dectsync(int start, int ignore_run) {
    long flags;
    gdectsync.ignore_run = ignore_run;
    flags = LOCK();
    if(start && (gdectsync.state != timerset_ok)) {
        gdectsync.state = init_dectsync;
    }
    UNLOCK(flags);
}
/*--------------------------------------------------------------------------------*\
 * liefert waste, bekommt run
\*--------------------------------------------------------------------------------*/
int updaterun_dectsync(int run) {
    gdectsync.run = run;
    return gdectsync.wastepercent;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int display_dectsync(int loadcontrol){
    if(gdectsync.state < calibrate_timerirq) {
        return 1;
    }
    printk(KERN_ERR"[%lu]idle(%d MHz) loadcntrl=%d: %d %%(%d %%) lost %ld wp=%d %% good=%lu.%02lu %% - v/l/t=%lu/%lu/%lu low=%lu us drift=%ld ns\n",
       jiffies,
       gdectsync.clk * 2,
       loadcontrol,
       100 - gdectsync.run, gdectsync.run, 
       gdectsync.synclost,
       gdectsync.wastepercent,
       gdectsync.goodpercent / 100, gdectsync.goodpercent % 100,
       gdectsync.violation, gdectsync.latency, gdectsync.timeout,
       CLK_TO_USEC(gdectsync.lowcycles),
       CLK_TO_NSEC(gdectsync.drift)
       );

    return 0;
}
/*--------------------------------------------------------------------------------*\
 * im Idle-Kontext
\*--------------------------------------------------------------------------------*/
int idle_dectsynchandler(int inactiv) {
    volatile struct _hw_gpio *hw_gpio = (struct _hw_gpio *)UR8_GPIO_BASE;
    int flags = 0;
    register unsigned long start_time, high, periodcnt,  wlan_diff, tdiff;
    unsigned long rx_wlan_bytes, tx_wlan_bytes;

    if(inactiv) {
        flags = LOCK();
        gdectsync.state = resetmode;
    }
    switch(gdectsync.state) {
        case resetmode:
            DBG_DS_ERR("[dectsync] inactive\n");
            avm_gpio_ctrl(DECT_SYNCGPIO, GPIO_PIN, GPIO_OUTPUT_PIN);
            avm_gpio_out_bit(DECT_SYNCGPIO, 0);
            gdectsync.violation = gdectsync.latency = gdectsync.periodcnt = gdectsync.timeout = 0;
            gdectsync.state = inactive;
            if(gdectsync.start == 0) {
                unsigned long clk;
                gdectsync.start =  1;
                gdectsync.prefetch_code = (void *)timer_dectsynchandler;
                gdectsync.prefetch_len = (size_t)timer_dectsynchandler_end - (size_t)timer_dectsynchandler;
                printk("prefetch_code: %p: len=%d\n", gdectsync.prefetch_code, gdectsync.prefetch_len);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
                hrtimer_init(&gdectsync.hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
                gdectsync.hr_timer.function = dectsynchtimer_handler;
#endif/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32) ---*/
#if defined(CONFIG_UR8_CLOCK_SWITCH)
                clk = avm_get_clock_notify(avm_clock_id_cpu, lclock_notify);
#else /*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/
                clk = avm_get_clock(avm_clock_id_cpu);
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_UR8_CLOCK_SWITCH) ---*/
                if(cpufreq_init(clk)) {
                    printk("[dectsync]invalid clock %lu -> inactive\n", clk);
                    gdectsync.state = invalid_clk;
                    break;
                }
                gdectsync.clk   = clk / 2 / 1000000;
            }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
            hrtimer_cancel(&gdectsync.hr_timer);
#endif/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32) ---*/
            break;
        case invalid_clk:
        case inactive:
            break;
        case init_dectsync:
            DBG_DS_TRC("[%lu][dectsync] start\n", jiffies);
            avm_gpio_ctrl(DECT_SYNCGPIO, GPIO_PIN, GPIO_INPUT_PIN);
            gdectsync.state         = calibration_mode;
            gdectsync.periodjiffies = jiffies;
            gdectsync.wastepercent  = 0; 
            break;
        case calibration_mode:
#if defined(DECTSYNC_STAT)
            if(gdectsyncstat.startjiffies == 0) {
                gdectsyncstat.startjiffies = jiffies;
            }
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
            flags = LOCK();
            high = DECT_SYNC_CHECK();
            if(high == 0) {
                /*--- ist nicht nur konstant '1' ---*/
                gdectsync.state = calibration_mode2;
#if defined(DECTSYNC_STAT)
                generic_stat(&gdectsyncstat.idle_until_low, (TIME_DIFF(jiffies, gdectsyncstat.startjiffies) * 1000) / HZ);
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
            }
            UNLOCK(flags);
            break;
        case calibration_mode2:
            if(gdectsync.run > 80) {
                break;
            }
            flags = LOCK();
            start_time = get_cycles();
            /*--- kurz draufschauen ob Highpegel auffindbar ---*/
            do {
                high = DECT_SYNC_CHECK();
                tdiff = TIME_DIFF(get_cycles(), start_time);
            } while((high == 0) && (tdiff < USEC_TO_CLK(400)));
            if(high == 0) {
#if defined(DECTSYNC_STAT)
                gdectsyncstat.idlewastecycle += TIME_DIFF(get_cycles(), start_time);
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                UNLOCK(flags);
                break;
            }
            do {
                high  = DECT_SYNC_CHECK();
                tdiff = TIME_DIFF(get_cycles(), start_time);
            } while((high) && (tdiff < USEC_TO_CLK(2000)));
            if(tdiff > 100) {
                /*--- Highphase zumindest nicht nur sporadisch -> in 2 Sekunden geht es los ---*/
                gdectsync.failcntdown   = DECTFAILCNTDOWN;
                gdectsync.state         = calibrate_timerirq_delay;
                gdectsync.periodjiffies = jiffies;
#if defined(DECTSYNC_STAT)
                generic_stat(&gdectsyncstat.idle_waste, gdectsyncstat.idlewastecycle);
                gdectsyncstat.idlewastecycle = 0;
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
            }
#if defined(DECTSYNC_STAT)
            gdectsyncstat.idlewastecycle += TIME_DIFF(get_cycles(), start_time);
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
            UNLOCK(flags);
            break;
        case calibrate_timerirq_delay:
            if(TIME_DIFF(jiffies, gdectsync.periodjiffies) > (HZ * 2)) {
                flags = LOCK();
                gdectsync.periodjiffies = jiffies;
                gdectsync.state         = calibrate_timerirq;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
                {
                ktime_t ktime = ktime_set(0, 10 * 1000 * 1000); /*--- wakeup in 10 msec ---*/
                hrtimer_start(&gdectsync.hr_timer, ktime, HRTIMER_MODE_REL);
                }
#endif
#if defined(DECTSYNC_STAT)
                generic_stat(&gdectsyncstat.idle_until_irq, (TIME_DIFF(jiffies, gdectsyncstat.startjiffies) * 1000) / HZ);
                gdectsyncstat.startjiffies = 0;
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                UNLOCK(flags);
            }
            break;
        case timerset_ok: 
            flags = LOCK();
            /*--- hier mal schauen, ob wir hier wieder einen Highpegel messen koennen ---*/
            if(gdectsync.lastwasgood && DECT_SYNC_CHECK()) {
                unsigned long lowcycles;
                /*--- wir haben High-Pegel detektiert ---*/
                lowcycles = TIME_DIFF(get_cycles(), gdectsync.lowedgecycles);
#if defined(DEBUG_DECTSYNC) & 0
                if(((CLK_TO_USEC(lowcycles) + 127) & ~0xFF) != ((CLK_TO_USEC(gdectsync.lowcycles) + 127) & ~0xFF)) {
                    DBG_DS_TRC("[%lu][dectsync]idle %d MHz: %ld -> %ld\n", jiffies, gdectsync.clk * 2, CLK_TO_USEC(gdectsync.lowcycles), CLK_TO_USEC(lowcycles));
                }
#endif/*--- #if defined(DEBUG_DECTSYNC) ---*/
                /*--- if(lowcycles > USEC_TO_CLK(5000)) { ---*/
                /*---    gdectsync.lowcycles = lowcycles; ---*/
                /*--- } ---*/
            }
            periodcnt =  gdectsync.periodcnt;
            if((TIME_DIFF(jiffies, gdectsync.periodjiffies) >=  10 * HZ) && periodcnt) {
               struct timeval tv;
               unsigned long tmsec;
#if defined(DECTSYNC_STAT)
               dectsyncstat();
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
               jiffies_to_timeval(TIME_DIFF(jiffies, gdectsync.periodjiffies), &tv);
               tmsec = (tv.tv_sec * 1000 + tv.tv_usec / 1000) / 100;
               gdectsync.goodpercent   = ((gdectsync.goodcnt * 10 * 1000) + 1) / periodcnt;
               gdectsync.wastepercent  = CLK_TO_MSEC(gdectsync.waste_idle) / tmsec;
               DBG_DS_TRC("[%lu][%lu][dectsync]waste %d %%(good=%lu.%02lu %%) low=%ld usec v/l/t=%lu/%lu/%lu\n", 
                                                                     CLK_TO_MSEC(TIME_DIFF(get_cycles(), gdectsync.periodcycle)),
                                                                     jiffies,
                                                                     gdectsync.wastepercent,
                                                                     gdectsync.goodpercent / 100, gdectsync.goodpercent % 100, 
                                                                     CLK_TO_USEC(gdectsync.lowcycles), 
                                                                     gdectsync.violation,
                                                                     gdectsync.latency, gdectsync.timeout);

                gdectsync.periodjiffies = jiffies;
                gdectsync.periodcycle   = get_cycles();
                gdectsync.goodcnt       = 0;
                gdectsync.periodcnt     = 0;
                gdectsync.waste_idle    = 0;
                UNLOCK(flags);
                get_wlantraffic(&rx_wlan_bytes, &tx_wlan_bytes);
                if(gdectsync.last_wlan_traffic) {
                    wlan_diff = (rx_wlan_bytes + tx_wlan_bytes) - gdectsync.last_wlan_traffic;
                    if(avm_power_disp_loadrate & 1) {
                        printk(KERN_ERR"wlan-traffic(rx+tx): %lu kbit/s\n", wlan_diff * 8 / 10 / 1000);
                    }
                    if(wlan_diff > KBIT_TO_BYTES(25000 * 2, 10)) {
                        gdectsync.periodjiffies = jiffies;
                        gdectsync.periodcnt     = 0;
                        gdectsync.state         = wlantraffic_mode;
                        gdectsync.last_wlan_traffic = rx_wlan_bytes + tx_wlan_bytes;
                    }
                }
                gdectsync.last_wlan_traffic = rx_wlan_bytes + tx_wlan_bytes;
            } else {
                UNLOCK(flags);
            }
            break;
        case wlantraffic_mode:
            gdectsync.goodcnt = 0;
            if(TIME_DIFF(jiffies, gdectsync.periodjiffies ) < (10 * HZ)) {
                break;
            }
            gdectsync.periodjiffies = jiffies;
            if(gdectsync.run > 80) {
                break;
            }
            get_wlantraffic(&rx_wlan_bytes, &tx_wlan_bytes);
            if(gdectsync.last_wlan_traffic) {
                wlan_diff = (rx_wlan_bytes + tx_wlan_bytes) - gdectsync.last_wlan_traffic;
                if(avm_power_disp_loadrate & 1) {
                    printk(KERN_ERR"wlan-traffic(rx+tx): %lu kbit/s\n", wlan_diff * 8 / 10 / 1000);
                }
                if(wlan_diff > KBIT_TO_BYTES(25000 * 2, 10)) {
                    gdectsync.last_wlan_traffic = rx_wlan_bytes + tx_wlan_bytes;
                    break;
                }
            }
            gdectsync.state = calibration_mode;
            break;
        default: 
            break;
    }
    if(inactiv) {
        UNLOCK(flags);
    }
    return 0;
}
/*--------------------------------------------------------------------------------*\
 * Im TimerIrq-Kontext                 
 * ret: 0 Irq aus, sonst direkt expire-Value
 *      mit HighPrec-Timer: relativ, falls in Timer-Routine absolut 
\*--------------------------------------------------------------------------------*/
unsigned long timer_dectsynchandler(unsigned long cycles_per_unit){
    volatile struct _hw_gpio *hw_gpio = (struct _hw_gpio *)UR8_GPIO_BASE;
    register enum { wait_for_highlevel = 0, high_level_detect, extraordinary_lowlevel, low_edge_found } p_state = wait_for_highlevel;
    register unsigned long level, highcycle_start;
    unsigned long start_time;
    unsigned long ret = cycles_per_unit, edgediff, violation;
    unsigned long cycles_per_slots, cycles_per_run, cycles_erg; 

    start_time = get_cycles();
    level = DECT_SYNC_CHECK();
    prefetch_range(gdectsync.prefetch_code, gdectsync.prefetch_len);
    prefetch_range(&gdectsync, sizeof(gdectsync));
#if defined(DECTSYNC_STAT)
    prefetch_range(&gdectsyncstat, sizeof(gdectsyncstat));
    if(gdectsyncstat.irqlast != 0) {
        generic_stat(&gdectsyncstat.irqdiff, TIME_DIFF(start_time, gdectsyncstat.irqlast));
    }
    gdectsyncstat.irqlast = start_time | 1;
#endif/*--- #if defined(DECTSYNC_STAT) ---*/

    if(level) {
        /*--- mmmh schon ein Highpegel taste dich an low ran ---*/
        highcycle_start = start_time - USEC_TO_CLK(50);
        violation       = 1;
    } else {
        violation       = 0;
        highcycle_start = 0;
    }
    switch(gdectsync.state) {
        /*-------------------------------*\
        \*-------------------------------*/
        case calibrate_timerirq:
#if defined(DECTSYNC_STAT)
            if(gdectsyncstat.initial_cycles == 0) {
                gdectsyncstat.initial_cycles = start_time | 1;
            }
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
            if(level == 0) {
                if(gdectsync.failcntdown) {
                    gdectsync.failcntdown--;
                    if(gdectsync.failcntdown == 0) {
                        gdectsync.state = calibration_mode;
                        break;
                    }
                }
                ret = cycles_per_unit + USEC_TO_CLK(800);
#if defined(DECTSYNC_STAT)
                gdectsyncstat.firsthighlevel_failed++;
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                break;
            }
#if defined(DECTSYNC_STAT)
            generic_stat(&gdectsyncstat.firsthighlevel, gdectsyncstat.firsthighlevel_failed);
            gdectsyncstat.firsthighlevel_failed = 0;
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
            highcycle_start = get_cycles();
            while(TIME_DIFF(get_cycles(), start_time) < USEC_TO_CLK(5000)) {
                level = DECT_SYNC_CHECK();
                if(level == 0) {
                    /*--- Lowflanke gefunden ---*/
#if defined(DECTSYNC_STAT)
                    generic_stat(&gdectsyncstat.firstlowedge, gdectsyncstat.firstlowedge_failed);
                    gdectsyncstat.firstlowedge_failed = 0;
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                    gdectsync.lowedgecycles = get_cycles();
                    gdectsync.state         = calibrate_timerirq_high;
                    /*--- Highflanke suchen ---*/
                    ret = cycles_per_unit - TIME_DIFF(gdectsync.lowedgecycles, highcycle_start) - USEC_TO_CLK(800);
                    break;
                }
            }
#if defined(DECTSYNC_STAT)
            gdectsyncstat.firstlowedge_failed++;
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
            break;
        /*-------------------------------*\
        \*-------------------------------*/
        case calibrate_timerirq_high:
            level = DECT_SYNC_CHECK();
            if(level) {
                /*--- weiter Highflanke suchen, lowedge hoffentlich identisch ---*/
                gdectsync.lowedgecycles += cycles_per_unit;
                ret = cycles_per_unit - USEC_TO_CLK(800);
#if defined(DECTSYNC_STAT)
                gdectsyncstat.highedge_search++;
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                break;
            }
#if defined(DECTSYNC_STAT)
            generic_stat(&gdectsyncstat.highedge, gdectsyncstat.highedge_search);
            gdectsyncstat.highedge_search = 0;
            generic_stat(&gdectsyncstat.initialtime, TIME_DIFF(get_cycles(), gdectsyncstat.initial_cycles));
            gdectsyncstat.initial_cycles = 0;
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
            /*--- kein break; ---*/
        /*-------------------------------*\
        \*-------------------------------*/
        case timerset_ok:
            edgediff =  TIME_DIFF(get_cycles(), gdectsync.lowedgecycles);  /*--- Zeit seit letzer Lowflanke ---*/
#if defined(DECTSYNC_STAT)
           generic_stat(&gdectsyncstat.latency, edgediff);
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
            if(gdectsync.lastwasgood) {
                if(edgediff < gdectsync.lowcycles - USEC_TO_CLK(2200)) {
                    ret = gdectsync.lowcycles - USEC_TO_CLK(2200) - edgediff;
#if defined(DECTSYNC_STAT)
                   generic_stat(&gdectsyncstat.retrigger_ret, ret);
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                    break;
                }
            }
            while(TIME_DIFF(get_cycles(), start_time) < USEC_TO_CLK(5000)) {
                level = DECT_SYNC_CHECK();
                switch(p_state) {
                    /*-------------------------------*\
                    \*-------------------------------*/
                    case  wait_for_highlevel:
                        if(level == 0) {
                            break;
                        }
                        if(highcycle_start == 0) {
                            highcycle_start = get_cycles();
#if defined(DECTSYNC_STAT)
                            generic_stat(&gdectsyncstat.wait_for_highlevel, TIME_DIFF(highcycle_start, start_time));
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                        }
                        p_state = high_level_detect;
                        /*--- kein break ---*/
                    /*-------------------------------*\
                    \*-------------------------------*/
                    case high_level_detect:
                        if(level == 0) {
                            int drift, slow;
                            unsigned long lowcycles;
                            unsigned long act_cycles = get_cycles();
                            /*--- Lowflanke gefunden: ---*/
#if defined(DECTSYNC_STAT)
                            generic_stat(&gdectsyncstat.wait_for_lowlevel, TIME_DIFF(act_cycles, highcycle_start));
                            generic_stat(&gdectsyncstat.lastlowedgediff, TIME_DIFF(act_cycles, gdectsync.lowedgecycles));
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                            if(TIME_DIFF(act_cycles, gdectsync.lowedgecycles) > (unsigned long)cycles_per_unit + USEC_TO_CLK(100)){
                                /*--- letzte lowflanke zu weit entfernt ->  neu suchen ---*/
                                gdectsync.state = calibrate_timerirq;
                                p_state = extraordinary_lowlevel;
                                gdectsync.latency++;
                                break;
                            }
                            lowcycles = TIME_DIFF(highcycle_start, gdectsync.lowedgecycles);
                            if((violation == 0) || (lowcycles < gdectsync.lowcycles)) {
                                /*--- nur uebernehmen wenn wirklich der ganze Highpegel gemessen/ lowcycles kleiner wird ---*/
                                gdectsync.lowcycles     = lowcycles;
                                gdectsync.highcycles    = TIME_DIFF(act_cycles, highcycle_start);
                            }
                            gdectsync.lowedgecycles = act_cycles;
                            if(gdectsync.lowcycles + gdectsync.highcycles > (unsigned long)cycles_per_unit){
                                drift =  gdectsync.lowcycles + gdectsync.highcycles - cycles_per_unit;
                                slow  = 0;
                            } else {
                                drift =  cycles_per_unit - (gdectsync.lowcycles + gdectsync.highcycles);
                                slow  = 1;
                            }
#if defined(DECTSYNC_STAT)
                            generic_stat(&gdectsyncstat.lowcycles, gdectsync.lowcycles);
                            generic_stat(&gdectsyncstat.highcycles, gdectsync.highcycles);
                            generic_stat(&gdectsyncstat.drift, slow ? -drift : drift);
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                            if(drift > USEC_TO_CLK(100)) {
                                /*--- inkonsistent ---*/
                                gdectsync.state = calibrate_timerirq;
                                /*--- gdectsync.drift_failed++; ---*/
                                p_state = extraordinary_lowlevel;
                                gdectsync.latency++;
                                break;
                            }
                            gdectsync.state = timerset_ok;
                            p_state         = low_edge_found;
                            gdectsync.drift = slow ? -drift : drift;
                            if(violation == 0) {
                                gdectsync.goodcnt++;
                                if(gdectsync.prefcycle > USEC_TO_CLK(1)) {
                                    gdectsync.prefcycle -= USEC_TO_CLK(1) / 4;
                                }
                            } else {
                                if(gdectsync.prefcycle < USEC_TO_CLK(450)) {
                                    /*--- immer noch Violation: Irq-Trigger weiter vorziehen ---*/
                                    gdectsync.prefcycle += USEC_TO_CLK(50);
                                }
                            }
#if defined(DECTSYNC_STAT)
                            generic_stat(&gdectsyncstat.prefcycle, gdectsync.prefcycle);
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                            /*--- Hauptkonkurrent pcmlink: im "idle" braucht pcmlink ca. 140 usec: also ca. 200 usec "Reserve" einplanen ---*/
                            cycles_per_slots = gdectsync.highcycles + USEC_TO_CLK(200) + gdectsync.prefcycle;
#if defined(DECTSYNC_STAT)
                            generic_stat(&gdectsyncstat.run, gdectsync.run);
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                            if(gdectsync.ignore_run == 0) {
                                cycles_per_run  = runtable[gdectsync.run / 16];
                            } else {
                                cycles_per_run = cycles_per_unit / 2;
                            }
                            cycles_erg = min(cycles_per_run, cycles_per_slots);
                            ret = cycles_per_unit - gdectsync.drift - cycles_erg;
#if defined(DECTSYNC_STAT)
                            generic_stat(&gdectsyncstat.ok_ret, ret);
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                        }
                        break;
                    /*-------------------------------*\
                    \*-------------------------------*/
                    default:
                        break;

                }
                if(p_state > high_level_detect) {
                    break;
                }
            }
            gdectsync.lastwasgood = 0;
            if(p_state == wait_for_highlevel) {
                gdectsync.timeout++;
                gdectsync.state =  calibrate_timerirq;
            } else if(p_state == high_level_detect) {
                gdectsync.timeout++;
#if defined(DECTSYNC_STAT)
                generic_stat(&gdectsyncstat.timeout, TIME_DIFF(get_cycles(), gdectsync.lowedgecycles));
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
                gdectsync.lowedgecycles = get_cycles() + USEC_TO_CLK(200);
            } else if(p_state == low_edge_found) {
                gdectsync.lastwasgood = 1;
                gdectsync.failcntdown = DECTFAILCNTDOWN;
            }
            if(gdectsync.lastwasgood == 0) {
                gdectsync.synclost++;
            }
            break;
        default:
            ret = 0;
            break;
    }
#if defined(DECTSYNC_STAT)
    if(gdectsync.state != timerset_ok) {
        gdectsyncstat.notimer_set_ok_wasteidle += TIME_DIFF(get_cycles(), start_time);
        if(gdectsyncstat.notimer_set_ok_endcycle) {
            gdectsyncstat.notimer_set_ok_wasterun += TIME_DIFF(start_time, gdectsyncstat.notimer_set_ok_endcycle);
        }
        gdectsyncstat.notimer_set_ok_endcycle = get_cycles() | 1;
    } else if(gdectsyncstat.notimer_set_ok_wasteidle) {
        /*--- Zeit insgesamt fuer den Initialisierungsprozess: ---*/
        generic_stat(&gdectsyncstat.notimer_set_ok, gdectsyncstat.notimer_set_ok_wasterun + gdectsyncstat.notimer_set_ok_wasteidle);
        /*--- Zeit die blockiert wurde ---*/
        generic_stat(&gdectsyncstat.waste_notimer_set_ok, gdectsyncstat.notimer_set_ok_wasteidle);
        gdectsyncstat.notimer_set_ok_endcycle   = 0;
        gdectsyncstat.notimer_set_ok_wasteidle  = 0;
    }
#endif/*--- #if defined(DECTSYNC_STAT) ---*/
    gdectsync.violation += violation;
    gdectsync.periodcnt++;
    gdectsync.waste_idle += TIME_DIFF(get_cycles(), start_time);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    if(ret) {
        ret += get_cycles();
    }
#endif/*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32) ---*/
    return ret;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void timer_dectsynchandler_end(void){
}
#endif/*--- #if defined(DECTSYNC_PATCH)  ---*/
