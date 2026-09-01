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
#include <asm/mach_avm.h>
#include "wyatt_earp.h"

#define WE_LIFE         1

#define UARTA_BASE      0xB9020000

#define SIO0_LS   (*(volatile char *)(UARTA_BASE + 0x14))
#define SIO0_TDAT (*(volatile char *)(UARTA_BASE + 0x00))
#define SIO_LS_TE            0x20    /* Transmit Holding empty      */
#define SIO_LS_TI            0x40    /* Transmitter empty (IDLE)    */
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
#if defined(WE_DEBUG)
    while(*chr) {
        if (*chr == '\n')   /*--- um VT52 Mode benutzen zu können ---*/
            sio_putc('\r');
        sio_putc(*chr++);
    }
#endif /*--- #if defined(WE_DEBUG) ---*/
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int printk_dummy(const char * fmt, ...) {
    fmt = fmt;
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void (*wyatt_earp_turn_off_leds)(void (*)(char *));
void (*wyatt_earp_unload_xilinx)(void (*)(char *));

EXPORT_SYMBOL(wyatt_earp_turn_off_leds);
EXPORT_SYMBOL(wyatt_earp_unload_xilinx);

/* #define WYATT_EARP_GPIO     20 */
#define WYATT_EARP_GPIO     13

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _hw_gpio_irqhandle *Wyatt_Earp_Irq_Handle;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int Wyatt_Earp_GPIO(unsigned int mask) {
    Wyatt_Earp(0x7FF);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void Wyatt_Earp_Init_GPIO(void) {
    avm_gpio_ctrl(WYATT_EARP_GPIO, GPIO_PIN, GPIO_INPUT_PIN);
    Wyatt_Earp_Irq_Handle = avm_gpio_request_irq(1 << WYATT_EARP_GPIO,
                                                 GPIO_ACTIVE_LOW ,
                                                 GPIO_EDGE_SENSITIVE, 
                                                 Wyatt_Earp_GPIO);
    avm_gpio_enable_irq(Wyatt_Earp_Irq_Handle);

    DBG_WE_TRC(("[Wyatt_Earp] GPIO interrupt on gpio\n"));
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void write_value(unsigned int base, unsigned int offset, unsigned int value) {
    *((volatile unsigned int *)(base + offset)) = value;
};


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void Wyatt_Earp(int Mask) {
    int state = 0;
    unsigned reboot_count = 100;
    /*--- int i; ---*/
    /*--- unsigned long addr = (unsigned int)Wyatt_Earp & ~0xF; ---*/

    if(Mask) {
#if !defined(WE_DEBUG)
    if(Mask & 0x800){
        set_printk(printk_dummy);
    }
#endif/*--- #if defined(WE_DEBUG) ---*/
        DBG_WE_TRC(("disable_irq\n"));
        local_irq_disable();
    }
    DBG_WE_TRC(("Wyatt_Earp\n"));

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    {
#define array_size(a)       (sizeof(a) / sizeof(a[0]))
        unsigned int i;
        unsigned int gigabit_ethernet_mac_base[] = {
                0xB9110000,
                0xB9150000 
        };
        unsigned int ap_base[] = {
            0xB9260300, /* GEMAC1 */
            0xB9270300, /* GEMAC2 */
            /*--- 0xB9190000, ---*/ /* DSL */
            /*--- 0xB9210000, ---*/ /* BMU */
            0xB91A0300, /* Reripheral */
            0xB9100300  /* Security */
        };

        /*--- alles resetten was wir nicht mehr benötigen ---*/
        if(Mask & 0x0C00) {  /*--- watchdog deaktivieren ---*/
            DBG_WE_TRC(("WATCHDOG: service\n"));
            write_value(0xB9000000, 0x08, (0xFFFC << 16) | (0xA << 8) /* Serive watchdog */);
            if(Mask & 0x0400) {  /*--- watchdog deaktivieren ---*/
                DBG_WE_TRC(("WATCHDOG: off\n"));
                write_value(0xB9000000, 0x08, (0xFFFC << 16) | (1 << 0) /* Disable */);
            }
        }

        if(Mask & 0x0001) {
            if(wyatt_earp_turn_off_leds) {
                (*wyatt_earp_turn_off_leds)(sio_puts);
                DBG_WE_TRC(("LED: off\n"));
            }
        }

        if(Mask & 0x0002) {
            /*--- APs resetten ---*/
            DBG_WE_TRC(("APs: off\n"));
            for(i = 0 ; i < array_size(ap_base) ; i++) {
                write_value(ap_base[i], 0x0000, (1 <<15));
            }
        }

        if(Mask & 0x0004) {
            /*--- Voice DSP und Sport resetten ---*/
            DBG_WE_TRC(("DSPs: off\n"));
            write_value(0xB9050000, 0x300, (1 << 9) | (1 << 25));  /* DSP 0 und 1 in reset */
            write_value(0xB9000000, 0x080, 0); /* Sport 0 und 1 deactivieren */
        }

        if(Mask & 0x0008) {
            /*--- PCI resetten ---*/
            DBG_WE_TRC(("PCIs: off\n"));
            write_value(0xB9050000, 0x700, (1 << 0) | (1 << 1));
        }

        if(Mask & 0x0010) {
            /*--- Gigabit Ethernet MAC resetten ---*/
            DBG_WE_TRC(("GMAC: off\n"));
            for(i = 0 ; i < array_size(gigabit_ethernet_mac_base) ; i++) {
                write_value(gigabit_ethernet_mac_base[i], 0x089C, (1 << 1) /* soft reset */);
                write_value(gigabit_ethernet_mac_base[i], 0x08A8, (1 << 15) | (1 << 14));
            }
        }

        if(Mask & 0x0020) {
            /*--- USB resetten ---*/
            DBG_WE_TRC(("USB2: off\n"));
            write_value(0xB9000000, 0xDC, (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5)); /* USB2 deaktivieren */
            /*--- Bit 0 noch einmal prüfen: USB2_MISC_CTL Register */
            DBG_WE_TRC(("USB1: off\n"));
            write_value(0xB9230000, 0x58, (1 << 8) /* USB Port 1 reset */);
            DBG_WE_TRC(("USB0: off\n"));
            write_value(0xB9230000, 0x54, (1 << 8) /* USB Port 0 reset */);
            DBG_WE_TRC(("USB_HOST: off\n"));
            write_value(0xB9230000, 0x10, (1 << 1) /* Host controller reset */);
        }

        if(Mask & 0x0040) {
            /*--- 5 Volt abschalten GPIO 10 auf 0 ---*/
            DBG_WE_TRC(("5V: off\n"));
            avm_gpio_out_bit(10, 0);
        }

        if(Mask & 0x0080) {
            /*--- switch abschalten GPIO 15 auf 0 ---*/
            DBG_WE_TRC(("Switch: off\n"));
            avm_gpio_out_bit(15, 0);
        }

        if(Mask & 0x0100) {
            if(wyatt_earp_unload_xilinx) {
                (*wyatt_earp_unload_xilinx)(sio_puts);
                DBG_WE_TRC(("Xilinx: off\n"));
            }
        }

        if(Mask & 0x0200) {
            /*--- clock abschalten ---*/
            DBG_WE_TRC(("Clocks: off\n"));
            write_value(0xB9000000, 0xCC, 
                            /*--- (1 << 0) | ---*/  /*--- PCI_CLK_EN          0 ---*/
                            (1 << 1) |  /*--- VPHY_CLK_EN         1 ---*/
                            (1 << 2) |  /*--- EBI_CLK_EN          2 ---*/
                            /*--- (1 << 3) | ---*/  /*--- GIGE1_CLK_EN        3 ---*/
                            /*--- (1 << 4) | ---*/  /*--- GIGE1_APU_CLK_EN 4 ---*/
                            /*--- (1 << 5) | ---*/  /*--- GIGE2_CLK_EN        5 ---*/
                            /*--- (1 << 6) | ---*/  /*--- GIGE2_APU_CLK_EN 6 ---*/
                            (1 << 7) |  /*--- VAP_SYS_CLK_EN      7 ---*/
                            (1 << 8) |  /*--- VAP_APU_CLK_EN      8 ---*/
                            /*--- (1 << 9) | ---*/  /*--- BMU_APU_CLK_EN      9 ---*/
                            /*--- (1 << 10) | ---*/  /*--- BMU_SYS_CLK_EN      10 ---*/
                            /*--- (1 << 11) | ---*/  /*--- WAP_APU_CLK_EN      11 ---*/
                            /*--- (1 << 12) | ---*/  /*--- WAP_SYS_CLK_EN      12 ---*/
                            /*--- (1 << 13) | ---*/  /*--- USBH_CLK_EN         13 ---*/
                            /*--- (1 << 14) | ---*/  /*--- SAP_APU_CLK_EN      14 ---*/
                            /*--- (1 << 15) | ---*/  /*--- SAP_SYS_CLK_EN      15 ---*/
                            0);
        }
    }
    DBG_WE_TRC(("Off\n"));

    if(Mask & 0x8000) {
        local_irq_enable();
        return;
    }

    DBG_WE_TRC(("Idle for ever \n"));
    /*--- CPU (MIPS)	idle-Mode ---*/
    for(;Mask;) {
        char buffer[50];
        volatile unsigned int j = 0;
        volatile unsigned int *pj = &j;
        if(Mask & 0x0800) {  /*--- watchdog deaktivieren ---*/
            write_value(0xB9000000, 0x08, (0xFFFC << 16) | (0xA << 8) /* Serive watchdog */);
        }
        sprintf(buffer, "[%04d]", reboot_count--);
        sio_putc(buffer[0]);
        sio_putc(buffer[1]);
        sio_putc(buffer[2]);
        sio_putc(buffer[3]);
        sio_putc(buffer[4]);
        sio_putc(buffer[5]);
        while(*pj < 100000) {
            (*pj)++;
        }
        *pj = 0;
        sio_putc('\b');
        sio_putc('\b');
        sio_putc('\b');
        sio_putc('\b');
        sio_putc('\b');
        sio_putc('\b');
        if(reboot_count == 0) {
            avm_gpio_ctrl(30, GPIO_PIN, GPIO_OUTPUT_PIN);
            avm_gpio_out_bit(30, 0);
            panic("reboot after dying gasp");
        }
    }
}

