/*
 * Interrupt dispatcher for ADI6843.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

	.text
	.set	macro
	.set	noat
	.align	5

NESTED(plat_irq_dispatch, PT_SIZE, sp)
/*	SAVE_ALL */
/*	CLI	*/			/* Important: mark KERNEL mode ! */
	.set    at

	mfc0	t0,CP0_CAUSE		/* get pending interrupts, IRQ Bits */
	mfc0	t1,CP0_STATUS		/* get enabled interrupts  IRQ Masks */
	nop
	and	t0,t1			/* isolate allowed ones */

	/* First we check for the GPT-1 Timer IRQ */
	andi	a0, t0, CAUSEF_IP7
	beq	a0, zero, 1f
	
        /* 
	   We got Timer Interrupt from GPT-1, let's directly call the 
           Handler.Remember this do_IRQ for timer does not enable the 
	   interrupts on the CPU as we have asserted SA_INTERRUPT bit 
	   for the timer so we are 100 % sure nothing will go wrong as 
	   long as PIC <-> CPU is concerned.
	*/

	/* We are here to process Time Interrupts */
	move	a0, sp
	jal	fusiv_timer_interrupt
	nop
	j	ret_from_irq
	nop

	
	/* 
	   We are done with Timer Interrupt which is any way God Father , 
	   priority level 5. Now let's see if we have any thing for 
	   poor guys */

        /* 
	   Here we got one or MORE interrupts for other priority levels 
	   One important thing we need to consider here is that one of 
           these IRQ handler may not have SA_INTERRUPT bit asserted so 
	   PIC can again interrupt CPU while we are processing the
	   interrupts, let's say GOD father may come again. We need to
	   be carefull here... Well, see comments in dispatcher .. 
	*/

1:
	andi	a0, t0, CAUSEF_IP6     
	beq	a0, zero, 2f
	li	a0, 4			/* Priority 4 */
	move	a1, sp
	jal	adi_6843_dispatch_irq
	nop
	j	ret_from_irq
	nop

2:
	andi	a0, t0, CAUSEF_IP5
	beq	a0, zero, 3f
	li	a0, 3			/* Priority 3 */
	move	a1, sp
	jal	adi_6843_dispatch_irq
	nop
	j	ret_from_irq
	nop

3:
	andi	a0, t0, CAUSEF_IP4
	beq	a0, zero, 4f
	li	a0, 2			/* Priority 2 */
	move	a1, sp
	jal	adi_6843_dispatch_irq
	nop
	j	ret_from_irq
	nop

4:
	andi	a0, t0, CAUSEF_IP3
	beq	a0, zero, 5f
	li	a0, 1			/* Priority 1 */
	move	a1, sp
	jal	adi_6843_dispatch_irq
	nop
	j	ret_from_irq
	nop

5:
	andi	a0, t0, CAUSEF_IP2
	beq	a0, zero, 6f
	li	a0, 0			/* Priority 0 */
	move	a1, sp
	jal	adi_6843_dispatch_irq
	nop
	j	ret_from_irq
	nop

6:
	move	a0, sp
	j	spurious_interrupt
END(plat_irq_dispatch)
