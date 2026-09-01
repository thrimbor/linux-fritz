/*
 * Copyright (C) 2006  Ikanos Communications. All rights reserved.
 * The information and source code contained herein is the property
 * of Ikanos Communications.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

/***
 * AD6843 Control Processor Timer Routines
 ***/

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <asm/io.h>
#include <asm/hardirq.h>
#include <ikan6850.h>
#include <timer.h>
#include <exports.h>
#include <linux/clockchips.h>

void fusiv_timer_interrupt(struct pt_regs *regs);
irqreturn_t pit_irq_handler(int irq, void *regs);

static void fusiv_set_mode(enum clock_event_mode mode,
                          struct clock_event_device *evt);

static struct clock_event_device fusiv_clockevent_device = {
	.name           = "fusiv-pit",
	.features       = CLOCK_EVT_FEAT_PERIODIC,
	.rating         = 200,
	.set_mode       = fusiv_set_mode,
};

struct irqaction fusiv_pit_action = {
        .handler        = pit_irq_handler,
        .flags          = IRQF_DISABLED,
        .name           = "fusiv_pit",
        .dev_id         = NULL,
};

static volatile struct timer_regs *tregs = 
			(volatile struct timer_regs *)KSEG1ADDR(0x19070000);

static void fusiv_set_mode(enum clock_event_mode mode,
                          struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
	{
		break;
	}
	default:
		break;
	}	
}

void fusiv_timer_interrupt(struct pt_regs *regs)
{
	u16 temp;

	/* find out who has interrupted us */
	temp = tregs->status & 0x1;
	if(temp)  
	{
		/* acknowledge the interrupt(s) */
		tregs->status = temp;
		do_IRQ(GPT1_INT);
	}
	
	return;
}

irqreturn_t pit_irq_handler(int irq, void *regs)
{
	struct clock_event_device *cd = &fusiv_clockevent_device;

	cd->event_handler(cd);

	return IRQ_HANDLED;
}

/* In linux, the clock is set to a tick rate of 100 hz. */
void plat_time_init(void)
{
	unsigned long period;
	unsigned long width;
	unsigned long flags;
	struct irqaction *irq = &fusiv_pit_action;
	struct clock_event_device *cd = &fusiv_clockevent_device;

	unsigned int cpu = smp_processor_id();
	
	cd->cpumask             = cpumask_of_cpu(cpu);

	/* Lock Local Interrupts */
	local_irq_save(flags);

	tregs->status = 0x0200;

        /* I want to make sure that for timer interrupt, SA_INTERRUPT bit is always there
	   other my PIC will trouble me as I am handling timer interrupt seperately from
	   other priority level interrupts. Good Luck Timer.
	   Well, I know this FLAG is already asserted in kernel/time.c but still I want to
	   make 100% sure it is there because this is the point where I am programming my
	   IRQ so let it be MUST. 
	 */
	irq->flags = IRQF_DISABLED;
	period = 166*1000*1000 / HZ;
	width = period / 2;

	/* This configures the timer as follows:
	 * PWM_OUT Mode
	 * Positive Active Pulse
	 * Count to end of Period
	 * Interrupt Request Enable
	 */
	
	tregs->config = 0x001d;

	/* Write top half of period register */
	tregs->period_hi = period >> 16;

	/* Write bottom half of period register */
	tregs->period_lo = (period & 0xffff);

	/* Write top half of width register */
	tregs->width_hi = width >> 16;
    
	/* Write bottom half of width register */
	tregs->width_lo = (width & 0xffff);

	/* Set TIMEN0 and clear any lingering interrupts */
	tregs->status = 0x0101;
	
	/* This seems to be mandatory for 'jiffies' updation even if we dont 
         * call do_IRQ in timer interrupt handler
         */
	setup_irq(GPT1_INT, irq);
	
	cd->mult = 1;
	clockevents_register_device(cd);

	/* Restore Interrupts */
	local_irq_restore(flags);


}
