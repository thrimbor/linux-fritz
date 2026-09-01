/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2010 AVM GmbH <fritzbox_info@avm.de>
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
 *   
 *   CPU-depend usage of request_irq, free_irq, enable_irq, disable_irq
\*------------------------------------------------------------------------------------------*/
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif /* AUTOCONF_INCLUDED */
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/serial_core.h>
#include <linux/spinlock.h>
#include <asm/irq.h>
#include <asm/io.h>

#if defined(CONFIG_SMP)
#define IFX_ENABLE_SMP
#endif/*--- #if defined(CONFIG_SMP) ---*/

extern void bsp_enable_irq_on(u32 cpu, u32 irq);
extern void bsp_disable_irq_on(u32 cpu, u32 irq);

extern int real_IFX_GET_CPU_ID(u32 irq);
#ifdef IFX_ENABLE_SMP
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _ifx_irq_on_work {
    enum { t_request_irq_on, t_free_irq_on } type;
    union {
        struct _param_request_irq_on {
            unsigned int irq;
            irq_handler_t handler;
            unsigned long irqflags;
            const char *devname;
            void *dev_id;
        } param_request_irq_on;
        struct _param_free_irq_on {
            unsigned int irq;
            void *dev_id;
        } param_free_irq_on;
    } param;
    
    struct semaphore sema;
    unsigned int retval;
    struct workqueue_struct *workqueue;
    struct work_struct work;
};

#ifdef CONFIG_MIPS_MT_SMTC
#  define IFX_GET_CPU_ID()          cpu_data[smp_processor_id()].vpe_id
#elif defined(CONFIG_MIPS_MT_SMP)
#  define IFX_GET_CPU_ID()          smp_processor_id()
#else
#  define IFX_GET_CPU_ID()          0
#endif

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void ifx_irq_on_startup_work(struct work_struct *data) {
    struct _ifx_irq_on_work *pwork = container_of(data, struct _ifx_irq_on_work, work);
    int retval = 0;

    switch(pwork->type) {
        case t_request_irq_on:
            /*--- printk(KERN_ERR "[%s %d irq: %d]\n", __func__, smp_processor_id(), pwork->param.param_request_irq_on.irq); ---*/
            retval = request_irq(   pwork->param.param_request_irq_on.irq,
                                    pwork->param.param_request_irq_on.handler,
                                    pwork->param.param_request_irq_on.irqflags,
                                    pwork->param.param_request_irq_on.devname,
                                    pwork->param.param_request_irq_on.dev_id
                                    );
#if defined(CONFIG_SMP)
            {
            struct cpumask tmask = CPU_MASK_ALL;
            /*--- cpumask_t all_cpumask = CPU_MASK_ALL; ---*/
            /*--- int cpu; ---*/
            int vpe;
            vpe =  real_IFX_GET_CPU_ID(pwork->param.param_request_irq_on.irq);
#ifdef CONFIG_MIPS_MT_SMTC
#if 0 
             for_each_cpu_mask(cpu, all_cpumask) {
                if ( (cpu_data[cpu].vpe_id != vpe) || !cpu_online(cpu) ) {
                    cpu_clear(cpu, tmask);
                }
             }
#else
             tmask = cpumask_of_cpu(vpe);
#endif
#elif defined(CONFIG_MIPS_MT_SMP)
             tmask = cpumask_of_cpu(vpe);
#endif/*--- #ifdef CONFIG_MIPS_MT_SMTC ---*/
            irq_set_affinity(pwork->param.param_request_irq_on.irq, (const struct cpumask *)&tmask);
            }
#endif/*--- #if defined(CONFIG_SMP) ---*/
            break;
        case t_free_irq_on: 
            free_irq(pwork->param.param_free_irq_on.irq,
                     pwork->param.param_free_irq_on.dev_id);
            break;
    }
    pwork->retval = retval;
    up(&pwork->sema);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline int workprocess(int cpu, struct _ifx_irq_on_work *pwork, const char *funcname) {

    sema_init(&pwork->sema, 0); /*--- nicht betreten ---*/
    /*--- printk(KERN_ERR "[%s %d -> %d] start\n", funcname, smp_processor_id(), cpu); ---*/
    if((pwork->workqueue = create_workqueue(funcname)) == NULL){
        return -ENOMEM;
    }
    INIT_WORK(&pwork->work, ifx_irq_on_startup_work);

	queue_work_on(cpu, pwork->workqueue, &pwork->work);
    down(&pwork->sema);
	destroy_workqueue(pwork->workqueue);
    /*--- printk(KERN_ERR "[%s %d -> %d] done\n", funcname, smp_processor_id(), cpu); ---*/
    return pwork->retval;
}
#define SET_PARAM(_paramtype, _param) ifx_irq_on_work.param._paramtype._param = _param

#endif /*--- #ifdef IFX_ENABLE_SMP ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int request_irq_on(int cpu, unsigned int irq, irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id) {
#ifdef IFX_ENABLE_SMP
    struct _ifx_irq_on_work ifx_irq_on_work;
    ifx_irq_on_work.type  = t_request_irq_on;
    SET_PARAM(param_request_irq_on, irq);
    SET_PARAM(param_request_irq_on, handler);
    SET_PARAM(param_request_irq_on, irqflags);
    SET_PARAM(param_request_irq_on, devname);
    SET_PARAM(param_request_irq_on, dev_id);
    return workprocess(cpu, &ifx_irq_on_work, __func__);
#else
    return request_irq(irq, handler, irqflags, devname, dev_id);
#endif
}
EXPORT_SYMBOL(request_irq_on);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int free_irq_on(int cpu, unsigned int irq, void *dev_id) {
#ifdef IFX_ENABLE_SMP
    struct _ifx_irq_on_work ifx_irq_on_work;
    /*--------------------------------------------------------------------------------*\
        mbahr: wichtiger Fix! Irq noch in diesem Kontext ausschalten:
    \*--------------------------------------------------------------------------------*/
    bsp_disable_irq_on(cpu, irq); 
    ifx_irq_on_work.type  = t_free_irq_on;
    SET_PARAM(param_free_irq_on, irq);
    SET_PARAM(param_free_irq_on, dev_id);
    return workprocess(cpu, &ifx_irq_on_work, __func__);
#else
    free_irq(irq, dev_id);
    return 0;
#endif
}
EXPORT_SYMBOL(free_irq_on);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void enable_irq_on(int cpu, unsigned int irq) {
#ifdef IFX_ENABLE_SMP
    bsp_enable_irq_on(cpu, irq);
#else
    enable_irq(irq);
#endif
}
EXPORT_SYMBOL(enable_irq_on);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void disable_irq_on(int cpu, unsigned int irq) {
#ifdef IFX_ENABLE_SMP
    bsp_disable_irq_on(cpu, irq);
#else
    disable_irq(irq);
#endif
}
EXPORT_SYMBOL(disable_irq_on);
