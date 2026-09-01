#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/new_capi.h>
#include <linux/capi_oslib.h>
#include <linux/jiffies.h>
#include "debug.h"
#include "ca.h"

static DEFINE_SPINLOCK(ca_timer_lock);
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
volatile struct capi_timer *CA_TIMERS = 0;   /*--- local ---*/
volatile unsigned int CA_TIMER_Anz    = 0;   /*--- local ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int CA_TIMER_NEW(unsigned int MaxTimer) {
    CA_TIMERS     = (struct capi_timer *)CA_MALLOC(sizeof(struct capi_timer) * MaxTimer);
    if (CA_TIMERS == 0) {
        DEB_ERR("malloc failed");
        return 1;
    }
    DEB_INFO("[CA_TIMER_NEW] max %u timers\n", MaxTimer);
    CA_TIMER_Anz  = MaxTimer;
    return 0;
}
EXPORT_SYMBOL(CA_TIMER_NEW);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void CA_TIMER_DELETE (void) {
    unsigned long flags;
    DEB_INFO("[CA_TIMER_DELETE]\n");

    spin_lock_irqsave(&ca_timer_lock, flags);
    CA_TIMER_Anz = 0;
    spin_unlock_irqrestore(&ca_timer_lock, flags);
    if (CA_TIMERS) {
        CA_FREE((void *)CA_TIMERS);
    }
    CA_TIMERS = NULL;
}
EXPORT_SYMBOL(CA_TIMER_DELETE);


/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int CA_TIMER_START(unsigned int index, unsigned int TimeoutValue, unsigned int _Param, enum CA_RESTARTTIMER (*_Func)(unsigned int param)) {
    unsigned long flags;
    if (index >= CA_TIMER_Anz ) 
        return 1;

    spin_lock_irqsave(&ca_timer_lock, flags);
    CA_TIMERS[index].Func  = NULL;
    spin_unlock_irqrestore(&ca_timer_lock, flags);
    CA_TIMERS[index].Start = CA_MSEC();
    CA_TIMERS[index].Tics  = TimeoutValue; 
    CA_TIMERS[index].Param = _Param;
    CA_TIMERS[index].Func  = _Func;
    return 0;
}
EXPORT_SYMBOL(CA_TIMER_START);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int CA_TIMER_STOP (unsigned index) {
    unsigned long flags;
    if (index >= CA_TIMER_Anz ) 
        return 1;
    spin_lock_irqsave(&ca_timer_lock, flags);
    CA_TIMERS[index].Func = NULL;
    spin_unlock_irqrestore(&ca_timer_lock, flags);
    return 0;
}
EXPORT_SYMBOL(CA_TIMER_STOP );

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void CA_TIMER_POLL (void) {
    unsigned long flags;
    unsigned i;
    struct capi_timer *T;
    unsigned int TimerValue;
    enum CA_RESTARTTIMER (*tfunc)(unsigned int param);
    unsigned int tparam;

    if(CA_TIMERS == NULL) 
        return;

    T = (struct capi_timer *)&CA_TIMERS[0];
    TimerValue = CA_MSEC();

    CA_MSEC64();  /* ab und zu muss das gepollt werden */

    for(i = 0 ; i < CA_TIMER_Anz ; i++, T++) {
        spin_lock_irqsave(&ca_timer_lock, flags);
        tfunc  = T->Func;
        tparam = T->Param;
        if((tfunc != NULL) && (TimerValue - T->Start > T->Tics)) {
            spin_unlock_irqrestore(&ca_timer_lock, flags);
            if(tfunc(tparam) == CA_TIMER_RESTART) {
                T->Start = TimerValue = CA_MSEC();
            } else {
                T->Func = NULL;
            }
        } else {
            spin_unlock_irqrestore(&ca_timer_lock, flags);
        }
    }
}

/*-------------------------------------------------------------------------------------*\
    UEberlauf alle 49 Tage  Hz: kann 100, 250, 1000 sein
\*-------------------------------------------------------------------------------------*/
unsigned int CA_MSEC(void) {
#if (HZ < 1000)
    return jiffies * (1000 / HZ);
#define CA_MSEC_OVERFLOW    ((1ULL << 32) * (unsigned long long)(1000 / HZ))
#else
    return jiffies / (HZ / 1000);
#define CA_MSEC_OVERFLOW    ((1ULL << 32) / (unsigned long long)(HZ / 1000))
#endif
}
EXPORT_SYMBOL(CA_MSEC);

/*-------------------------------------------------------------------------------------*\
    UEberlauf alle 49 * 2^32 Tage
\*-------------------------------------------------------------------------------------*/
unsigned long long CA_MSEC64(void) {
    static unsigned int LastTime;
    static unsigned long long Offset;
    unsigned int        Time     = CA_MSEC();
    if(Time < LastTime) /*--- UEberlauf ---*/
        Offset += CA_MSEC_OVERFLOW;
    LastTime = Time;
    return Offset + (unsigned long long)Time;
}
EXPORT_SYMBOL(CA_MSEC64);


