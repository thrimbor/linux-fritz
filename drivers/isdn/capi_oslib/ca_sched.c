#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <asm/atomic.h>
#include <linux/semaphore.h>
#include <linux/new_capi.h>
#include <linux/capi_oslib.h>
#include <linux/ioport.h>
#include <linux/workqueue.h>
#if defined(CONFIG_AR9) || defined(CONFIG_VR9)
#include <irq.h>
#endif /*--- #if defined(CONFIG_AR9) || defined(CONFIG_VR9) ---*/
#include "debug.h"
#include "host.h"
#include "ca.h"


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(USE_TASKLETS)
struct tasklet_struct scheduler_tasklet;
struct tasklet_struct *p_scheduler_tasklet = NULL;
#endif /*--- #if defined(USE_TASKLETS) ---*/
#if defined(USE_WORKQUEUES)
struct workqueue_struct *p_scheduler_workqueue = NULL;
struct work_struct scheduler_work;
#elif defined(USE_THREAD)
static struct _capi_scheduler {
    struct task_struct *thread_pid;
    struct completion on_exit;
    wait_queue_head_t wait_queue;
    volatile unsigned int trigger;
    void (*scheduler)(unsigned long data);
} capi_sched;
static int capi_oslib_thread (void *data);
#endif /*--- #if defined(USE_WORKQUEUES) ---*/


void (*delic_tasklet_func)(unsigned long );
void (*delic_enable_irq)( void );
void (*delic_disable_irq)( void );

struct tasklet_struct delic_tasklet;
struct tasklet_struct *p_delic_tasklet = NULL;

static atomic_t capi_oslib_scheduler_enabled;
static atomic_t	capi_oslib_triggered;
static atomic_t capi_oslib_crit_level;
static void capi_oslib_scheduler (unsigned long data);

#if !defined(USE_THREAD)
static struct timer_list capi_oslib_scheduler_timer;
/*--------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------*/
void capi_oslib_scheduler_timer_stop(void) {
    del_timer(&capi_oslib_scheduler_timer);
}

/*--------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------*/
void capi_oslib_scheduler_timer_start(void) {
    capi_oslib_scheduler_timer.data = 0;
    capi_oslib_scheduler_timer.expires = jiffies + (HZ / 100); /*--- 10 ms ---*/
    if (timer_pending(&capi_oslib_scheduler_timer)) {
        mod_timer(&capi_oslib_scheduler_timer, capi_oslib_scheduler_timer.expires);
    } else {
        add_timer(&capi_oslib_scheduler_timer);
    }
}
#endif/*--- #if !defined(USE_THREAD) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void capi_oslib_trigger_scheduler(void) {
	if (atomic_read (&capi_oslib_scheduler_enabled)) {
        if(atomic_inc_return(&capi_oslib_triggered) != 1) {
            return;
        }
#if defined(CAPIOSLIB_CHECK_LATENCY)
        /*--- capi_check_latency(0, (char *)__func__, 1); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
#if defined(USE_TASKLETS)
        if(p_scheduler_tasklet)
            tasklet_schedule(p_scheduler_tasklet);
#endif /*--- #if defined(USE_TASKLETS) ---*/
#if defined(USE_WORKQUEUES)
        if(p_scheduler_workqueue)
            queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, p_scheduler_workqueue, &scheduler_work);
#endif /*--- #if defined(USE_WORKQUEUES) ---*/
#if defined(USE_THREAD)
        {
        struct _capi_scheduler *pcapi_sched = &capi_sched;
        pcapi_sched->trigger++;
        /*--- DBG_TRC("<trigger: %d>", pcapi_sched->trigger); ---*/
        wake_up_interruptible(&pcapi_sched->wait_queue);
        }
#endif/*--- #if defined(USE_THREAD) ---*/
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void os_trigger_scheduler(void) {
    /*--- DEB_TRACE("os_trigger_scheduler\n"); ---*/
    capi_oslib_trigger_scheduler();
}
EXPORT_SYMBOL(os_trigger_scheduler);

#if !defined(USE_THREAD)
/*--------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------*/
static void capi_oslib_scheduler_timer_handler(unsigned long nr __attribute__((unused))) {
	capi_oslib_scheduler_timer_stop();
	capi_oslib_scheduler_timer_start();
	capi_oslib_trigger_scheduler();
}

/*--------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------*/
void capi_oslib_scheduler_timer_init(void) {
    DEB_INFO("%s\n", __func__);
	init_timer(&capi_oslib_scheduler_timer);
	capi_oslib_scheduler_timer.expires = 0;
	capi_oslib_scheduler_timer.data = 0;
	capi_oslib_scheduler_timer.function = capi_oslib_scheduler_timer_handler;
	capi_oslib_scheduler_timer_start();
}
#endif/*--- #if !defined(USE_THREAD) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void capi_oslib_delic_tasklet_init(void (*tasklet_func) (unsigned long ), unsigned int enable) {
    if(tasklet_func) {
        if(enable) {
            tasklet_init(&delic_tasklet, tasklet_func, 0);
            p_delic_tasklet = &delic_tasklet;
            return;
        } 
        if(p_delic_tasklet) {
            p_delic_tasklet = NULL;
            tasklet_kill(&delic_tasklet);
        }
    }
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void capi_oslib_scheduler_tasklet_init(void (*scheduler_tasklet_func) (unsigned long ), unsigned int enable) {
    if(scheduler_tasklet_func) {
        if(enable) {
#if defined(USE_TASKLETS)
            tasklet_init(&scheduler_tasklet, scheduler_tasklet_func, 0);
            p_scheduler_tasklet = &scheduler_tasklet;
#endif /*--- #if defined(USE_TASKLETS) ---*/
#if defined(USE_WORKQUEUES)
            if(p_scheduler_workqueue == NULL) {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
                p_scheduler_workqueue = create_singlethread_workqueue("capioslib_sched");
                INIT_WORK(&scheduler_work, (void (*)(void *))scheduler_tasklet_func, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
                p_scheduler_workqueue = alloc_workqueue("capi_schedw", WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
#else/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39) ---*/
                p_scheduler_workqueue = create_rt_workqueue("capi_schedw");
#endif
                INIT_WORK(&scheduler_work, (void (*)(struct work_struct *))scheduler_tasklet_func);
            }
#endif /*--- #if defined(USE_WORKQUEUES) ---*/
#if defined(USE_THREAD)
            {
            struct _capi_scheduler *pcapi_sched = &capi_sched;

            init_waitqueue_head(&pcapi_sched->wait_queue);
            init_completion(&pcapi_sched->on_exit);
            pcapi_sched->scheduler = scheduler_tasklet_func;
            kernel_thread(capi_oslib_thread, pcapi_sched, CLONE_SIGHAND);
            }
#endif/*--- #if defined(USE_THREAD) ---*/
            return;
        }
#if defined(USE_TASKLETS)
        if(p_scheduler_tasklet) {
            p_scheduler_tasklet = NULL;
            tasklet_kill(&scheduler_tasklet);
        }
#endif /*--- #if defined(USE_TASKLETS) ---*/
#if defined(USE_WORKQUEUES)
        if(p_scheduler_workqueue) {
            destroy_workqueue(p_scheduler_workqueue);
            p_scheduler_workqueue = NULL;
        }
#endif /*--- #if defined(USE_WORKQUEUES) ---*/
#if defined(USE_THREAD)
        {
        struct _capi_scheduler *pcapi_sched = &capi_sched;
        if(pcapi_sched->thread_pid) {
            send_sig(SIGTERM, pcapi_sched->thread_pid, 1);
            pcapi_sched->thread_pid = NULL;
            wait_for_completion(&pcapi_sched->on_exit);
        }
        }
#endif/*--- #if defined(USE_THREAD) ---*/
    }
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static inline void capi_oslib_enable_scheduler (void) {

	/*--- DEB_INFO("Enabling scheduler...\n"); ---*/
#if defined(USE_TASKLETS)
    if(p_scheduler_tasklet)
	    tasklet_enable (p_scheduler_tasklet);
#endif /*--- #if defined(USE_TASKLETS) ---*/
	atomic_set (&capi_oslib_scheduler_enabled, 1);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void os_enable_scheduler (void) {
    capi_oslib_enable_scheduler();
}
EXPORT_SYMBOL(os_enable_scheduler);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static inline void capi_oslib_disable_scheduler (void) {

	/*--- DEB_INFO("Disabling scheduler...\n"); ---*/
	atomic_set (&capi_oslib_scheduler_enabled, 0);
#if defined(USE_TASKLETS)
    if(p_scheduler_tasklet)
	    tasklet_disable(p_scheduler_tasklet);
#endif /*--- #if defined(USE_TASKLETS) ---*/
} 

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void os_disable_scheduler (void) {
    capi_oslib_disable_scheduler();
}
EXPORT_SYMBOL(os_disable_scheduler);

#if defined(USE_THREAD)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int capi_oslib_thread (void *data) {
    struct _capi_scheduler *pcapi_sched = (struct _capi_scheduler *)data;
    pcapi_sched->thread_pid = current;
    daemonize("capioslib_sched");
    set_user_nice(current, -20);
    allow_signal(SIGTERM); 
    pcapi_sched->scheduler((unsigned long)data);
    return 0;
}
#endif/*--- #if defined(USE_THREAD) ---*/
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void capi_oslib_scheduler (unsigned long data __attribute__((unused))) {
#if defined(USE_THREAD)
    struct _capi_scheduler *pcapi_sched = &capi_sched;
    printk("[%s] init\n", __func__);

    for(;;) {
        int timeout = wait_event_interruptible_timeout(pcapi_sched->wait_queue, pcapi_sched->trigger, HZ / 100);
        if(timeout == -ERESTARTSYS) {
            /* interrupted by signal -> exit */ 
            break;
        }
        pcapi_sched->trigger = 0;
        atomic_inc(&capi_oslib_triggered); /*--- (mindestens einmal durch Timer) ---*/
        while(atomic_read(&capi_oslib_triggered)) {
            if(atomic_read (&capi_oslib_scheduler_enabled)) {
#if defined(CAPIOSLIB_CHECK_LATENCY)
                /*--- capi_check_latency(0, (char *)__func__, 0); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
                CA_TIMER_POLL();
                (void)(*capi_oslib_stack->cm_schedule)();
            }
            atomic_dec(&capi_oslib_triggered);
        }
    }
    pcapi_sched->thread_pid = NULL;
    printk("[%s] exit\n", __func__);
    complete_and_exit(&pcapi_sched->on_exit, 0 );
#else/*--- #if defined(USE_THREAD) ---*/
#if defined(CAPIOSLIB_CHECK_LATENCY)
    /*--- capi_check_latency(0, (char *)__func__, 0); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
    while(atomic_read(&capi_oslib_triggered)) {
		CA_TIMER_POLL();
		(void)(*capi_oslib_stack->cm_schedule)();
        /*--- if(!atomic_dec_and_test(&capi_oslib_triggered)) { ---*/
        /*---     printk("capi_oslib_scheduler retrigger %d\n", atomic_read(&capi_oslib_triggered));    ---*/
        /*--- } ---*/
		atomic_dec(&capi_oslib_triggered);
    }
#endif/*--- #else ---*//*--- #if defined(USE_THREAD) ---*/
} 


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
static irqreturn_t capi_oslib_irq_handler (int irq, void * args, struct pt_regs * regs) {
#else/*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19) ---*/
static irqreturn_t capi_oslib_irq_handler (int irq __attribute__((unused)), void * args) {
#endif/*--- #else ---*//*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19) ---*/

	if (args != NULL) {
		if ((*capi_oslib_stack->cm_handle_events) ()) {
			if (atomic_read (&capi_oslib_scheduler_enabled)) {
#if defined(USE_TASKLETS)
                if(p_scheduler_tasklet)
				    tasklet_schedule (p_scheduler_tasklet);
#endif /*--- #if defined(USE_TASKLETS) ---*/
#if defined(USE_WORKQUEUES)
        		if(p_scheduler_workqueue)
	        	    queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, p_scheduler_workqueue, &scheduler_work);
#endif /*--- #if defined(USE_WORKQUEUES) ---*/
#if defined(USE_THREAD)
                capi_oslib_trigger_scheduler();
#endif/*--- #if defined(USE_THREAD) ---*/
			}
            if(p_delic_tasklet)
                tasklet_schedule(p_delic_tasklet);
            return IRQ_HANDLED;
		}
	}
    return IRQ_NONE;
} /* irq_handler */

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define	IO_RANGE		256

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int capi_oslib_install_card (struct _stack_init_params * card) {
	int result = 0;

	capi_oslib_disable_scheduler ();
    capi_oslib_scheduler_tasklet_init(capi_oslib_scheduler, 1);
    if(capi_oslib_interrupt_library && capi_oslib_interrupt_library->tasklet_func)
        capi_oslib_delic_tasklet_init(capi_oslib_interrupt_library->tasklet_func, 1);

    if(card) {

        if(card->io_addr) {
            request_mem_region (card->io_addr, IO_RANGE, "capi_oslib");
            DEB_INFO(
                "I/O memory range 0x%08x-0x%08x assigned to " "capi_oslib" " driver.\n",
                card->io_addr,
                card->io_addr + IO_RANGE - 1
            );
        }

        if(card->irq_num) {
            result = request_irq (
                    card->irq_num,
                    &capi_oslib_irq_handler,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
                    SA_INTERRUPT,
#else/*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/
                    IRQF_DISABLED,
#endif/*--- #else ---*//*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/
                    "capi_oslib",
                    card
                    );
            if (result) {
                release_mem_region (card->io_addr, IO_RANGE);
                DEB_ERR("irq: %d registration failed\n", card->irq_num );
                return 1;
            } else {
                DEB_INFO("irq: %d successfully registred\n", card->irq_num );
            }
        }
    }
#if !defined(USE_THREAD)
	capi_oslib_scheduler_timer_init();
#endif/*--- #if !defined(USE_THREAD) ---*/
	return 0;
} 

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void capi_oslib_remove_card (struct _stack_init_params* card) {

#if !defined(USE_THREAD)
	capi_oslib_scheduler_timer_stop();
#endif/*--- #if !defined(USE_THREAD) ---*/
	capi_oslib_disable_scheduler ();
    capi_oslib_scheduler_tasklet_init(capi_oslib_scheduler, 0);

    if(capi_oslib_interrupt_library && capi_oslib_interrupt_library->tasklet_func)
        capi_oslib_delic_tasklet_init(capi_oslib_interrupt_library->tasklet_func, 0);

    if(card == NULL)
        return;

    if(card->irq_num) {
        DEB_INFO("Releasing IRQ #%d...\n", card->irq_num);
        free_irq (card->irq_num, card);
    }
    if(card->io_addr) {
        DEB_INFO(
            "Releasing I/O memory range 0x%08x-0x%08x...\n",
            card->io_addr,
            card->io_addr + IO_RANGE - 1
        );
        release_mem_region (card->io_addr, IO_RANGE);
    }
} /* remove_card */
/*--------------------------------------------------------------------------------*\
 * not used: direct setting in isdn_fonx
\*--------------------------------------------------------------------------------*/
void  capi_oslib_init_tasklet_control(void (* tasklet_control)(enum _tasklet_control)){
    if(capi_oslib_stack == NULL) {
        DEB_ERR("capioslib: not initialized\n");
        return;
    }
    if(capi_oslib_stack->cm_ctrl_tasklet && tasklet_control) {
        DEB_ERR("capioslib: cm_ctrl_tasklet already initialized, ignore reinit!\n");
    }
    capi_oslib_stack->cm_ctrl_tasklet = (void *)tasklet_control;
    DEB_INFO("capioslib: cm_ctrl_tasklet with %p initialized\n", tasklet_control);
}
EXPORT_SYMBOL(capi_oslib_init_tasklet_control);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void EnterCritical (void) {

    /*--- printk("[EnterCritical]"); ---*/
    if(capi_oslib_init_params && capi_oslib_init_params->irq_num)
	    disable_irq (capi_oslib_init_params->irq_num);


    /*--- printk("1"); ---*/
    if(p_delic_tasklet)
        tasklet_disable (p_delic_tasklet);
    /*--- printk("2"); ---*/
    if(capi_oslib_stack->cm_ctrl_tasklet)
        (*capi_oslib_stack->cm_ctrl_tasklet)(tasklet_control_enter_critical);
    /*--- printk("3"); ---*/
	atomic_inc (&capi_oslib_crit_level);
    /*--- printk("4\n"); ---*/

}
EXPORT_SYMBOL(EnterCritical);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void _EnterCritical (char *File __attribute__((unused)), int Line __attribute__((unused))) {
    EnterCritical();
}
EXPORT_SYMBOL(_EnterCritical);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void LeaveCritical (void) {

    /*--- printk("[LeaveCritical]"); ---*/
	atomic_dec (&capi_oslib_crit_level);
	BUG_ON( atomic_read(&capi_oslib_crit_level) < 0 );
    /*--- printk("1"); ---*/
    if(capi_oslib_init_params && capi_oslib_init_params->irq_num)
	    enable_irq (capi_oslib_init_params->irq_num);
    /*--- printk("2"); ---*/
    if(p_delic_tasklet)
        tasklet_enable(p_delic_tasklet);
    /*--- printk("3"); ---*/
    if(capi_oslib_stack->cm_ctrl_tasklet)
        (*capi_oslib_stack->cm_ctrl_tasklet)(tasklet_control_leave_critical);
    /*--- printk("4\n"); ---*/
} 
EXPORT_SYMBOL(LeaveCritical);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void _LeaveCritical (char *File __attribute__((unused)), int Line __attribute__((unused))) {
    LeaveCritical();
}
EXPORT_SYMBOL(_LeaveCritical);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if 0
void EnterCacheSensitiveCode (void) {

	(*interface->enter_cache_sensitive_code) ();
} /* EnterCacheSensitiveCode */
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if 0
void LeaveCacheSensitiveCode (void) {

	(*interface->leave_cache_sensitive_code) ();
} /* LeaveCacheSensitiveCode */
#endif

