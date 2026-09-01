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

#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/serial_8250.h>
#include <linux/console.h>
#include <linux/irq.h>
#include <linux/delay.h>

#include <asm/serial.h>
#include <asm/io.h>
#include <asm/wbflush.h>
#include <asm/time.h>
#include <asm/mipsregs.h>
#include <ikan6850.h>

extern void (*_machine_restart)(char *);
extern void (*_machine_halt)(void);
extern void (*pm_power_off)(void);
extern void ad6843_startTimer(struct irqaction *irq);
extern void fusiv_init_IRQ(void);
#ifdef CONFIG_PCI
extern int  fusiv_pci_setup(void);
#endif

static void fusiv_mips32_configure(void);

void  (*softResetPreparation4Vox150_ptr)( void ) =NULL;
#ifdef CONFIG_FUSIV_VX180
void  (*softResetVoiceDriver_ptr)( void ) =NULL;
#endif
#ifdef CONFIG_ATM
unsigned char glo_uncmp_buffer [512*1024];
#endif

#define PCI_RST_CONTROL	*(volatile unsigned long *)(KSEG1ADDR(0x19050700))
#define REBOOT_CTRL	*(volatile unsigned long *)(KSEG1ADDR(0x19000000))

static void ad6843_restart( char *command )
{
	/* Do a good, solid PCI bus reset */
	PCI_RST_CONTROL = 0x1;
	mdelay(1);
	PCI_RST_CONTROL = 0x3;
	mdelay(1);
	PCI_RST_CONTROL = 0x1;
	mdelay(1);
#if 0 // Linux-2.6.18
        if(softResetPreparation4Vox150_ptr == NULL)
        {
                printk("\n Error:softResetPreparation4Vox150_ptr. Please restart MANUALLY...\n");
        }
        else
        {
                (*softResetPreparation4Vox150_ptr)( );
        }
#endif
#if (defined (CONFIG_FUSIV_MIPS_BASED_VOICE) && CONFIG_FUSIV_MIPS_BASED_VOICE ) || ( defined(CONFIG_FUSIV_DSP_BASED_VOICE) && CONFIG_FUSIV_DSP_BASED_VOICE )
#ifdef CONFIG_FUSIV_VX180
        if(softResetVoiceDriver_ptr != NULL)
        {
	  (*softResetVoiceDriver_ptr)();
        }
        mdelay(10);
#endif
#endif
     if((*(unsigned int*)0xb900003c) == 0x6850) // vx180
     {
       int reset_value = 0x1 << 15;

       printk("\ndisabling IRQ's and enabling reset bit in all AP's\n");

        /* reset all AP's -- enable reset bit in AP control reg's */
       *(volatile unsigned int *)0xb9110300 = reset_value; /* GEMAC1 AP */
       *(volatile unsigned int *)0xb9150300 = reset_value; /* GEMAC2 AP */
       *(volatile unsigned int *)0xb9190300 = reset_value; /* VDSL AP   */
#if defined(CONFIG_FUSIV_KERNEL_PERI_AP) || defined(CONFIG_FUSIV_KERNEL_PERI_AP_MODULE)
       *(volatile unsigned int *)0xb91a0300 = reset_value; /* PERI AP   */
#endif
       *(volatile unsigned int *)0xb9100300 = reset_value; /* SEC AP    */
       mdelay(5);
       *(volatile unsigned int *)0xb9210300 = reset_value; /* BMU AP    */
       mdelay(5);

        /* Disable Interrupt Request (Timer 0) -- simple reset will do this*/
       *(volatile unsigned int *)0xb9070004 = 0x0;
        /* Disable Timer (TIMER0) */
       *(volatile unsigned int *)0xb9070000 = 0x1 << 9;
       mdelay(2);
     }
	REBOOT_CTRL = 0x1;
	while ( 1 );
}

static void ad6843_halt( void )
{
	printk("AD6843 Halted\n");
	while ( 1 );
}
// extern void register_console(struct console *);
// extern struct console ad6843_console;

/* Do fusiv-specific initialization in here */
int plat_setup(void)
{
	struct uart_port req;


//	board_timer_setup = ad6843_startTimer;
	_machine_restart = ad6843_restart;
	_machine_halt = ad6843_halt;
	pm_power_off = ad6843_halt;   

#if CONFIG_SERIAL_8250_CONSOLE_UART == 0
    #define CONSOLE_UART_INT  UART1_INT    
    #define CONSOLE_UART_ADDR ADI_6843_UART0_ADDR
    #define CONSOLE_UART_FIFO 1
#else     
    #define CONSOLE_UART_INT  UART2_INT    
    #define CONSOLE_UART_ADDR ADI_6843_UART1_ADDR
    #define CONSOLE_UART_FIFO 16
#endif
	/* Setup serial port */
	memset(&req, 0, sizeof(req));
	req.line = 0;
	req.type = PORT_16450;
	req.irq = CONSOLE_UART_INT;
	req.flags = ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST;
	/* req.uartclk = 4*1000*1000 /16/6; */
	req.uartclk = 166000000 /16/6;
	req.iotype = SERIAL_IO_MEM;
	req.membase = (unsigned char *)((CONSOLE_UART_ADDR));
	req.mapbase = (unsigned long)(CONSOLE_UART_ADDR);
	req.regshift = 2;
	req.fifosize = CONSOLE_UART_FIFO;
#ifdef CONFIG_SERIAL_8250
	early_serial_setup(&req);
#endif 
#ifdef CONFIG_PCI	
	set_io_port_base(0);
#endif 
//	register_console( &ad6843_console );

	/* Core specific tunings */
	fusiv_mips32_configure();

	return 0;
}

void __init plat_mem_setup(void)
{

}

/* MIPS32 Core configuration and tuning */
static void fusiv_mips32_configure(void) {

         /* 
          * Configure Cache Coherency policy for KSEG0:
          * Cacheable, non-coherent, write-back, read and write allocate
          */ 
#if CONFIG_FUSIV_VX180_WRITE_BACK
	change_c0_config(CONF_CM_CMASK, CONF_CM_CACHABLE_NONCOHERENT);
#else
#warning Cache-Coherency-Mode (Cache-Write-Through) maybe not configured properly (depending on Urlader init)
#endif
}

const char *get_system_type(void)
{
	return "Ikanos Fusiv Core";
}

void __init arch_init_irq(void)
{
	fusiv_init_IRQ();
}

#if defined(CONFIG_FUSIV_KERNEL_PROFILER_MODULE) && CONFIG_FUSIV_KERNEL_PROFILER_MODULE

int (*loggerFunction)(unsigned long event) = NULL;

int loggerProfile(unsigned long event)
{
  if(loggerFunction != NULL)
    return loggerFunction(event);
  return -1;
}
void  loggerRegFunction( int (*func)(unsigned long event))
{
  loggerFunction = func;
}

EXPORT_SYMBOL(loggerRegFunction);
#endif
EXPORT_SYMBOL(softResetPreparation4Vox150_ptr);
#ifdef CONFIG_FUSIV_VX180
EXPORT_SYMBOL(softResetVoiceDriver_ptr);
#endif
#ifdef CONFIG_ATM
EXPORT_SYMBOL(glo_uncmp_buffer);
#endif


#define REBOOT_STATUS	*(volatile unsigned long *)(KSEG1ADDR(0x19000000) + 0x4)
unsigned int avm_reset_status(void) {

    unsigned int status = REBOOT_STATUS;

    switch (status) {
        case 0:         /*--- Software-Reset ---*/
            return 1;
        case 2:         /*--- Watchdog-Reset ---*/
            return 2;
        case 3:         /*--- POR ---*/
            return 0;
        default;
            return 0xff;    /*--- unknown Status ---*/
    }

}

EXPORT_SYMBOL(avm_reset_status);


/*--- Kernel-Schnittstellen-Funktion für das LED-Modul ---*/
enum _led_event { /* DUMMY DEFINITION */ LastEvent = 0 };
int (*led_event_action)(int, enum _led_event , unsigned int ) = NULL;
EXPORT_SYMBOL(led_event_action);

