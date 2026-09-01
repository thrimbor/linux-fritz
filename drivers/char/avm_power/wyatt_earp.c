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

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#include <linux/autoconf.h>
#if !defined(CONFIG_MIPS_UR8)
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/file.h>
#include <linux/avm_power.h>
/*--- #include <asm/semaphore.h> ---*/
#include <asm/errno.h>
#include "avm_power.h"
#include <linux/sched.h>
#include <linux/pm.h>
#include <asm/io.h>
#include "wyatt_earp.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mips-boards/ohio.h>
#include <asm/mips-boards/ohio_clk.h>
#include <asm/mach-ohio/hw_uart.h>
#include <asm/mach-ohio/hw_reset.h>
#include <asm/mach-ohio/hw_emif.h>
#include <asm/mach_avm.h>
/* Reset Control bits */
#define UART0_RESET		    (1 <<  0)
#define UART1_RESET		    (1 <<  1)
#define IIC_RESET  		    (1 <<  2)
#define TIMER0_RESET  	    (1 <<  3)
#define TIMER1_RESET  	    (1 <<  4)
#define RESERVED5_RESET     (1 <<  5)
#define GPIO_RESET 		    (1 <<  6)
#define ADSLSS_RESET  		(1 <<  7)
#define USB_RESET  		    (1 <<  8)
#define SAR_RESET  		    (1 <<  9)
#define RESERVED10_RESET  	(1 << 10)
#define VDMAVT_RESET  		(1 << 11)
#define FSER_RESET  		(1 << 12)
#define RESERVED13_RESET  	(1 << 13)
#define RESERVED14_RESET  	(1 << 14)
#define RESERVED15_RESET 	(1 << 15)
#define VLYNQ1_RESET  		(1 << 16)
#define EMACA_RESET 	    (1 << 17)
#define DMA_RESET           (1 << 18)
#define BIST_RESET          (1 << 19)
#define VLYNQ0_RESET        (1 << 20)
#define EMACB_RESET      	(1 << 21)
#define MDIO_RESET	    	(1 << 22)
#define ADSLSS_DSP_RESET    (1 << 23)
#define RESERVED24_RESET    (1 << 24)
#define RESERVED25_RESET    (1 << 25)
#define EMAC_PHY_RESET  	(1 << 26)

#else /*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/
#include <asm/avalanche/avalanche_map.h>
#include <asm/avalanche/sangam/hw_reset.h>
#include <asm/avalanche/sangam/hw_emif.h>
#include <asm/avalanche/sangam/hw_uart.h>
#endif/*--- #else ---*/ /*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/

int SuspendTimer(int state);
int SuspendGPIOs(int state);
int MinimizeClocks(int state);
int MinimizeCoreVoltage(int state);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
struct EMIF_register_memory_map *EMIF_addr = (struct EMIF_register_memory_map *) OHIO_EMIF_BASE;
#else/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/
struct EMIF_register_memory_map *EMIF_addr = (struct EMIF_register_memory_map *) AVALANCHE_EMIF_CONTROL_BASE;
#endif/*--- #else ---*//*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define KSEG_MSK		  0xE0000000
#define KSEG1BASE		  0xA0000000

#define MYKSEG1(addr)               (((unsigned int)(addr) & ~KSEG_MSK) | KSEG1BASE)

#define UARTA_BASE      (MYKSEG1(0x08610e00))
#define RESET_BASE 		(MYKSEG1(0x08611600))

#if 0
#ifdef EB
#define BEO 3
#else
#define BEO 0
#endif


#define SIO0__BASE    	UARTA_BASE 
#define SIO0__OFFSET   	4

#define SIO0_LS   (*(volatile char *)(SIO0__BASE+(SIO0__OFFSET*5)+BEO))
#define SIO0_TDAT (*(volatile char *)(SIO0__BASE+(SIO0__OFFSET*0)+BEO))
#define SIO_LS_TE            0x20    /* Transmit Holding empty      */
#define SIO_LS_TI            0x40    /* Transmitter empty (IDLE)    */
#endif

#if defined(WE_DEBUG)
#define WE_LIFE    
#endif/*--- #if defined(WE_LIFE) ---*/

extern void (*cpu_wait)(void);
#if defined(WE_DEBUG)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void sio_putc(char chr) {
    while((SIO0_LS & SIO_LS_TE) == 0)
        ;
    SIO0_TDAT = chr;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void sio_flush(void) {
    while((SIO0_LS & SIO_LS_TI) == 0)
        ;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void sio_puts(char *chr) {
    while(*chr) {
        if (*chr == '\n')   /*--- um VT52 Mode benutzen zu können ---*/
            sio_putc('\r');
        sio_putc(*chr++);
    }
}
#endif/*--- #if defined(WE_DEBUG) ---*/

#define Index_Store_Data_I	0x1c
#define Index_Store_Data_D	0x1d
#if 0
unsigned GlobalAddress;
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#define lock_icache() {\
    unsigned int *endaddress = &GlobalAddress; \
    unsigned int *address = &GlobalAddress; \
    GlobalAddress = (unsigned int)Wyatt_Earp; \
        __asm__ __volatile__ (              \
            ".set noreorder             \n"             \
            ".set noat                  \n"             \
            ".set push                  \n"             \
            ".set mips4                 \n"             \
            "     cache  0x1c, 0(%0)    \n" /* lock instruction cache */                \
            ".set mips0                 \n"             \
            ".set pop                   \n"             \
            :                                          \
            : "r" (address), "r" (endaddress)          \
            ); \
}
#endif
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static inline void lock_icache_line(unsigned long addr) {
#if 0
	__asm__ __volatile__(
		"    .set push                \n"
		"    .set noreorder           \n"
		"    .set mips4               \n"
		"     lw    $0,   (%0)         \n"  
		"     cache  0x1c, 0(%0)    \n" /* lock instruction cache */
        :
		: "r" (addr));
#endif
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int printk_dummy(const char * fmt, ...) {
    fmt = fmt;
    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
 *
 * ADSL2+ erfordert DYING-GASP, deshalb alle Module soweit moeglich schlafen legen  
    Sequenz 'Wyatt Earp':
        TIATM	 -> kann nicht resetet werden: andererer Powerstate (veranlasst DSP zum 'letzten Hauch') (Revison Frage 3)
        WLAN	 -> WLAN-Reset per GPIO/per VLYNC-Kommando (hardwareabhängig) -> muss der WLAN-Treiber machen  (gibt 'brutal'- und VLYNC-Komando-Hardware) (Frage 4)
        LED's  -> abschalten
        USB	 -> gegebenfalls USB-Spannung abschalten -> momentan gibt es noch kein Design, was dies ermöglicht (Frage 5)
        ETHERNET -> abschalten
    wenn WLAN erfolgreich:
        VLYNC	 -> VLYNC-Reset (Kernel)
        IRQ's aus	(geht nicht bevor WLAN ausgeht) (Revison Frage 2)
        ISDN	 -> ISDN-Reset (hardwareabhängig: Xilinx, DECT, UBIK, ...)
        XILINX -> soweit möglich: entladen
        TIMER  -> abschalten
        GPIO	 -> abschalten
        CLOCKS -> minimale Frequenz
        Corespannung reduzieren
        Code in den Cache locken,  dann: 	
        RAM		Powerdown
        ROM		Powerdown
        CPU (MIPS)	idle-Mode

    Mitteilung erfolgt über Mailbox-System des TIATM
*-----------------------------------------------------------------------------------------------*/
void Wyatt_Earp(int Mask) {
    int state = 0;
    /*--- int i; ---*/
    /*--- unsigned long addr = (unsigned int)Wyatt_Earp & ~0xF; ---*/
    if(Mask) {
#if !defined(WE_DEBUG)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    if(Mask & 0x800){
        set_printk(printk_dummy);
    }
#endif/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/
#endif/*--- #if defined(WE_DEBUG) ---*/
        local_irq_disable();
    }
    DBG_WE_TRC(("Wyatt_Earp\n"));
    if(Mask) {
        local_irq_disable();
        DBG_WE_TRC(("IRQ OFF\n"));
    } else {
        printk("Wyatt Earp:\n");
        printk("\t0x0001: WLAN\n");
        printk("\t0x0002: LED\n");
        printk("\t0x0004: USB\n");
        printk("\t0x0008: Ethernet\n");
        printk("\t0x0010: ISDN\n");
        printk("\t0x0020: VLYNC\n");
        printk("\t0x0040: Piglet (disabled)\n");
        printk("\t0x0080: Timer\n");
        printk("\t0x0100: Minimize clocks\n");
        printk("\t0x0200: Minimize core voltage\n");
        printk("\t0x0400: (unused)\n");
        printk("\t0x0800: (no exit procedure)\n");
        printk("\t0x1000: Reset peripherie\n");
        printk("\t0x2000: select Standby mode\n");
        printk("\t0x4000: select Off mode\n");
        printk("\t0x8000: (exit procedure)\n");
        return;
    }
    if(Mask & 1)    SuspendWLAN(state);
    if(Mask & 2)    SuspendLEDs(state);
    if(Mask & 4)    SuspendUSB(state);
    if(Mask & 8)    SuspendEthernet(state);
    if(Mask & 0x10) SuspendISDN(state);
    if(Mask & 0x21) SuspendVLYNC(state);
    /*--- if(Mask & 0x40) SuspendXILINX(state); ---*/
    if(Mask & 0x80) SuspendTimer(state);
    if(Mask & 0x100)MinimizeClocks(state);
    if(Mask & 0x200)MinimizeCoreVoltage(state);
    if((Mask & 0x800) == 0){
        local_irq_enable();
        /*--- printk("RESET_PRCR  %x\n", RESET_PRCR); ---*/
        return;
    }
#if defined(WE_DEBUG)
    sio_flush();
#endif/*--- #if !defined(WE_DEBUG) ---*/
    goto CachePreLoad;
CachePreLoadReady:
    /*--- i-Code im Cache locken ---*/ 	
    /*--- lock_icache(); ---*/
#if 0
    for(i = 0; i < 1024; i += 16) {
        lock_icache_line(addr + i);
    }
#endif
    /*--- Reset-Register außer DSP alles aus ---*/
    /*--- Momentan an: UART0, GPIO, DSL, SAR, EMACA, DMA, VLINQ0, EMACB, MDIO ---*/
    /*--- nicht auschalten?!: GPIO, SAR ---*/
    if(Mask & 0x1000)
    RESET_PRCR  = RESET_PRCR & (
#if defined(WE_LIFE)
                  UART0_RESET   |
#endif/*--- #if !defined(WE_DEBUG) ---*/
                  /*--- UART1_RESET	| ---*/	
                  /*--- IIC_RESET  	| ---*/	  
                  /*--- TIMER0_RESET  | ---*/
                  /*--- TIMER1_RESET  | ---*/ 	  
                  GPIO_RESET 	|               
                  ADSLSS_RESET  |	
                  /*--- USB_RESET  	| ---*/
                  SAR_RESET  	|	

                  /*--- VDMAVT_RESET  | ---*/
                  /*--- FSER_RESET  	| ---*/
                  /*--- VLYNQ1_RESET  | ---*/
                  /*--- EMACA_RESET 	| ---*/

                  /*--- DMA_RESET     | ---*/ 
                  /*--- BIST_RESET    | ---*/ 
                  /*--- VLYNQ0_RESET  | ---*/ 

                  /*--- EMACB_RESET   | ---*/ 
                  /*--- MDIO_RESET	| ---*/
                  ADSLSS_DSP_RESET | 
                  /*--- EMAC_PHY_RESET; ---*/
                  0);

    if(Mask & 0x2000) {
        EMIF_addr->SDRAMBankCR.Bits.sr = 1; 
        *((volatile unsigned int *)0xa8610a00) = (*((volatile unsigned int *)0xa8610a00) & ~(3 << 30)) | (2 << 30);
    }
    if(Mask & 0x4000) {
        *((volatile unsigned int *)0xa8610a00) = (*((volatile unsigned int *)0xa8610a00) & ~(3 << 30)) | (3 << 30);
    }
    if(Mask & 0x8000) {
        local_irq_enable();
        return;
    }

    /*--- CPU (MIPS)	idle-Mode ---*/
    for(;Mask;) {
#if defined(WE_LIFE)
        SIO0_TDAT = 's';
        while(state--);
        state = 10000000;
#else/*--- #if defined(WE_DEBUG) ---*/
        __asm__ __volatile__ ("wait \n");      
#endif/*--- #else ---*//*--- #if defined(WE_DEBUG) ---*/
    }
CachePreLoad:
    goto CachePreLoadReady;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int SuspendTimer(int state){
    return 0;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int SuspendGPIOs(int state){
    return 0;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int MinimizeClocks(int state) {
#if defined(CONFIG_MIPS_OHIO)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    volatile unsigned int *system_pll_div1=(volatile unsigned int *)(CLOCK_BASE+0x118);
    DBG_WE_TRC(("MinimizeClocks\n"));
    /* Loesche Ratio-Bits im SystemDivider*/                  	
    *system_pll_div1&=~(0x1F);
    /* Setze Divider auf 10 => Geht davon aus, dass Standardeinstellung PreDiv=0 PLL=9 eingestellt ist.*/
    *system_pll_div1|=9;
#endif/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/
#elif defined(CONFIG_MIPS_UR8)/*--- #if defined(CONFIG_MIPS_OHIO) ---*/
#endif/*--- #elif defined(CONFIG_MIPS_UR8) ---*//*--- #if defined(CONFIG_MIPS_OHIO) ---*/
    return 0;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int MinimizeCoreVoltage(int state){
#if 0
#ifdef CONFIG_MIPS_OHIO
    /*
     * The following code is used to boost CODEC voltage for Sangam250 by setting
     * Buck Trim Bits in CTRL2. For Ohio250, the setting of Buck Trim Bits need
     * to be set in datapump code because each reset of CODEC will clean these
     * Buck Trim Bits.
     */
    struct _adsl_lock lock;
    unsigned int rc;
	
	/* Hole Basisaddresse der CodeRegister auf dem DSP */
    rc = dslhal_support_hostDspAddressTranslate((unsigned int)0x02040000, &lock);
    if (rc== DSLHAL_ERROR_ADDRESS_TRANSLATE)
    {
        dprintf(1, "dslhal_support_hostDspAddressTranslate failed\n");
        return DSLHAL_ERROR_ADDRESS_TRANSLATE;
    }

	//Setze Password (CodecRegister+103), um Buck digital Trim setzten zu können.
	DSLHAL_REG8(rc+103)= 6;
	
	DSLHAL_REG8(rc+0x11) &= ~(0xF<<4); /* set Buck switcher Bits to Zero */
    /*Sascha: Änderung der Spannung*/
    /*--- if ( boost ){ ---*/
    	/* set Buck switcher to 1,65V => 
    	 * Wird bei retrain wieder auf 0 gesetzt, daher Buck Digital Trim auf Null.
    	 * Dadurch wird der default (1,65V) aus trim block in PM benutzt. PDF S.24*/ 
   		/* DSLHAL_REG8(rc+0x11) |= ((0x7<<5)|(1<<4)); */
        /*--- printk("#### boostVoltage: DSP Core voltage set to 1,65 V ####\n"); ---*/
    /*--- }else { ---*/ 
    DSLHAL_REG8(rc+0x11) |= (1<<4); /* set Buck switcher to 1,5V (000) */
    DBG_WE_TRC(("#### boostVoltage: DSP Core voltage set to 1,5 V ####\n"));
    /*--- } ---*/
    shim_osClockWait(64);
    DBG_WE_TRC((5,"Set Buck Voltage for DSP\n"));
    DBG_WE_TRC((6,"Current Contents of PRCR: 0x%x\n",(unsigned int)DSLHAL_REG32(REG_PRCR)));
	
	//DEBUG_ATM("[tiatm: %s] BuckSwitcherRegister=0x%02X\n",DSLHAL_REG8(rc+0x11));
	/* Loesche Password: Zugriff ist nicht mehr erlaubt.*/
	DSLHAL_REG8(rc+103)= 0;
	
    DBG_WE_TRC((4," dslhal_support_unresetDslSubsystem done\n"));

#endif/*--- #ifdef CONFIG_MIPS_OHIO ---*/
#endif
    return 0;
}
#endif/*--- #if !defined(CONFIG_MIPS_UR8) ---*/
