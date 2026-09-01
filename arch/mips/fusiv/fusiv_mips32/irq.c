/*
 * Copyright (C) 2006  Ikanos Communications. All rights reserved.
 * The information and source code contained herein is the property
 * of Ikanos Communications.
 */

/*
 * BRIEF MODULE DESCRIPTION
 *	ADI 6843 interrupt routines.
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/bitops.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/delay.h>

#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <ikan6850.h>

#include <linux/kernel_stat.h>

#undef DEBUG_IRQ
#ifdef DEBUG_IRQ
/* note: prints function name for you */
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

 #define ALL_INTS (IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4 | IE_IRQ5)
// #define ALL_INTS (IE_IRQ2 | IE_IRQ5)

/* Priority Array
 * 5 = highest, 0 = Lowest.
 * Please note that timer1 should always be priority 5 since the low
 * level assembly code hardwires CAUSEF_IP7 to go to the timer ISR.
 */


static unsigned char ikan6850_prio[38] = {
        0,      /* DMA = 0 */
        0,      /* ARB1 */
        0,      /* SCU */
        1,      /* MIPS_TIMER */
        0,      /* GPIO2 */
        3,      /* DBUSA_ARB  */
        2,      /* UART1 = 6 */
        0,      /* SPI */
        0,      /* GPIO */
        5,      /* Timer 1 */
        0,      /* Timer 2 */
        0,      /* Timer 3 */
        0,      /* DBUSB */
        3,      /* GIGE2 */
        3,      /* GIGE1 */
        0,      /* VDSP_IDMA2 = 15 */
        0,      /* VDSP_IDMA1 */
        3,      /* DSP0_FL0 */
#if defined(CONFIG_FUSIV_DSP_BASED_VOICE_V1_ARCH) && CONFIG_FUSIV_DSP_BASED_VOICE_V1_ARCH
        0,      /* DSP0_FL1 */
#else
        3,      /* DSP0_FL1 */
#endif
        0,      /* DSP0_PF0 */
        0,      /* DSP0_PF1 */
        3,      /* DSP1_FL0 */
#if defined(CONFIG_FUSIV_DSP_BASED_VOICE_V1_ARCH) && CONFIG_FUSIV_DSP_BASED_VOICE_V1_ARCH
        0,      /* DSP1_FL1 */
#else
        3,      /* DSP1_FL1 */
#endif
        0,      /* DSP1_PF0 */
        0,      /* DSP1_PF1 */
        3,      /* PCI  = 25 */
        0,      /* VDSL_PHY */
        0,      /* DBUSC */
        0,      /* SPA */
        0,      /* UART2 */
        0,      /* WAP_GIGE   */
        4,      /* BMU_GIGE */
        0,      /* AP DMA */
        0,      /* MIPS_SW0 */
        0,      /* MIPS_SW1 */
        0,      /* USBH  */
        0,      /* VDSL_AP */
	0	/* MIPS_PC */
};

static volatile ipc_t *ipc = (volatile ipc_t *)KSEG1ADDR(0x19050000);

#ifdef CONFIG_KGDB
extern void breakpoint(void);
#endif

extern void set_debug_traps(void);
extern irq_cpustat_t irq_stat [NR_CPUS];
// extern asmlinkage void adi_6843_irq(void);
unsigned int local_bh_count[NR_CPUS];
unsigned int local_irq_count[NR_CPUS];

static unsigned int startup_irq(unsigned int irq);
static inline void end_irq(unsigned int irq_nr);
static inline void local_enable_irq(unsigned int irq_nr);
// static inline void local_disable_irq(unsigned int irq_nr);
inline void local_disable_irq(unsigned int irq_nr);
static inline void ack_irq(unsigned int irq_nr);
static inline void mask_irq(unsigned int irq_nr);
static inline void mask_ack_irq(unsigned int irq_nr);
static inline void unmask_irq(unsigned int irq_nr);
static inline void eoi_irq(unsigned int irq_nr);

static inline void local_enable_irq(unsigned int irq_nr)
{
	ipc->cpe_prio[irq_nr >> 2] &= ~(0xff << ((irq_nr & 0x3) * 8));
	ipc->cpe_prio[irq_nr >> 2] |= (ikan6850_prio[irq_nr] << 
					((irq_nr & 0x3) * 8));
	/* Ensure the write before we go back */
//	wbflush();
}

// static inline void local_disable_irq(unsigned int irq_nr)
inline void local_disable_irq(unsigned int irq_nr)
{
	ipc->cpe_prio[irq_nr >> 2] |=  (0xff << ((irq_nr & 0x3) * 8));
	/* Ensure the write before we go back */
//	wbflush();
}

static inline void ack_irq(unsigned int irq_nr)
{
	/* We have already done it from our dispatcher code so 
	   nothing to do when kernel calls this . */
	//local_disable_irq(irq_nr);
}
static inline void mask_irq(unsigned int irq_nr)
{
        /* We have already done it from our dispatcher code so
           nothing to do when kernel calls this . */
        //local_disable_irq(irq_nr);
}
static inline void mask_ack_irq(unsigned int irq_nr)
{
        /* We have already done it from our dispatcher code so
           nothing to do when kernel calls this . */
        //local_disable_irq(irq_nr);
}
static inline void unmask_irq(unsigned int irq_nr)
{
        /* We have already done it from our dispatcher code so
           nothing to do when kernel calls this . */
        //local_disable_irq(irq_nr);
}
static inline void eoi_irq(unsigned int irq_nr)
{
        /* We have already done it from our dispatcher code so
           nothing to do when kernel calls this . */
        //local_disable_irq(irq_nr);
}
static inline void end_irq(unsigned int irq_nr)
{
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		local_enable_irq(irq_nr);
}

static unsigned int startup_irq(unsigned int irq_nr)
{
	local_enable_irq(irq_nr);
	return 0;
}

static void shutdown_irq(unsigned int irq_nr)
{
	local_disable_irq(irq_nr);
	return;
}


static struct hw_interrupt_type adi_6843 = {
	"ADI fusiv",
	startup_irq,
	shutdown_irq,
	local_enable_irq,
	local_disable_irq,
	ack_irq,
        mask_irq,
        mask_ack_irq,
        unmask_irq,
        eoi_irq,
	end_irq,
	NULL
};

void adi_6843_dispatch_irq(int prio, struct pt_regs *regs)
{
	/* This is over lovely dispatcher, which gets called from 
	   Interrupt specific code */

	/* Let us put some memory rather then few clocks. Memory is always 
	   cheaper then CPU time . */
	unsigned char IRQCnt1  = 0;
	unsigned char IRQCnt2  = 0;
	unsigned char i;
	unsigned char IRQSet1[32];
	unsigned char IRQSet2[32];
	unsigned long stat;
	int irq;
	stat = ipc->cpe_stat[prio].l;
	if(likely(stat != 0))
        {
	  /* Very first thing to check if there is some thing to do */
	  while (stat) 
	  {
	 	irq = ffs(stat) - 1;
		stat &= ~(1 << irq);
		IRQSet1[IRQCnt1++] = irq;
          }
        }
	stat = ipc->cpe_stat[prio].h;
        if(likely(stat != 0))
        {
          /* Very first thing to check if there is some thing to do */
          while (stat)
          {
                irq = ffs(stat) - 1;
                stat &= ~(1 << irq);
                IRQSet2[IRQCnt2++] = irq + 32;
          }
        }
        
	  /* 
	     We should not call do_IRQ before we acknowldege the 
             Interupt Controller , one or more IRQ handler may not 
             have SA_INTERRUPT bit set and kernel will enable the 
             interrupts (which was set by processor ), which will 
             again drag some body here . 
	  */

	  /* Let us acknowldege Interupt Controller that we got the message */
	  for(i = 0; i<IRQCnt1; i++)
  	  {
            local_disable_irq(IRQSet1[i]);
          }

	  /* Let us acknowldege Interupt Controller that we got the message */
	  for(i = 0; i<IRQCnt2; i++)
  	  {
            local_disable_irq(IRQSet2[i]);
          }
	
	  /* 
	     Now we are safe to enable the interrupts becauase we have 
	     told Interupt Controller that we are all set to go. 
          */
	  for(i = 0; i<IRQCnt1; i++)
          {
	    do_IRQ(IRQSet1[i]);
	  }

	  /* 
             Now we are safe to enable the interrupts becauase we have 
	     told Interupt Controller that we are all set to go. 
          */
	  for(i = 0; i<IRQCnt2; i++)
          {
	    do_IRQ(IRQSet2[i]);
	  }
}

void __init fusiv_init_IRQ(void)
{
	int i;
	unsigned long cp0_status;

	cp0_status = read_c0_status();

	/* Set all interrupt priorities to 0x1f so that they are not routed 
	 * anywhere.
	 */
#if defined(CONFIG_FUSIV_VX200) && CONFIG_FUSIV_VX200
	for (i = 0; i < 8; i++)
	{
		ipc->cpe_prio[i] = 0x1f1f1f1f;
    }
	ipc->ap_prio = 0x1f1f1f1f;
#endif
	for (i = 0; i < 10; i++)
		ipc->cpe_prio[i] = 0x0f0f0f0f;
	/* let us initialize all the IRQs Descriptors. */
	for (i = 0; i <= ADI_6843_MAX_INTS; i++)
	{
		irq_desc[i].chip     = &adi_6843;
		irq_desc[i].status	= IRQ_DISABLED;
		irq_desc[i].action	= 0;
		irq_desc[i].depth	= 1;
		spin_lock_init(&irq_desc[i].lock);
	}
   //  write_c0_cause(0x0);
	clear_c0_status(ALL_INTS);
    set_c0_status(ALL_INTS);


#ifdef CONFIG_KGDB
	/* If local serial I/O used for debug port, enter kgdb at once */
	puts("Waiting for kgdb to connect...");
	set_debug_traps();
	breakpoint();
#endif
	
	
}
