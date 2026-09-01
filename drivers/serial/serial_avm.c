/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2012 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation version 2 of the License.
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
#if defined(CONFIG_SERIAL_AVM_ASC_CONSOLE) || defined(CONFIG_SERIAL_AVM_8250_CONSOLE) || defined(SERIAL_AVM_ATH_HI_CONSOLE)
#define AVM_SERIAL_CONSOLE_SUPPORT
#endif/*--- #if defined(CONFIG_SERIAL_AVM_ASC_CONSOLE) || defined(CONFIG_SERIAL_AVM_8250_CONSOLE) || defined(SERIAL_AVM_ATH_HI_CONSOLE) ---*/

#if defined(AVM_SERIAL_CONSOLE_SUPPORT)
/*--- wichtiges define fuer "serial_core.h" !!! ---*/
#define SUPPORT_SYSRQ                 1
#endif

#include <linux/version.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/device.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/delay.h>
#if defined(CONFIG_CPU_FREQ)
#include <linux/cpufreq.h>
#endif/*--- #if defined(CONFIG_CPU_FREQ) ---*/
#include <linux/ioport.h>

/*--- #define DEBUG_UART_AVM ---*/

#include "serial_avm.h"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32) 
	#define UART_INFO info
	#define S_UART_INFO struct uart_info
#else
	#define UART_INFO state
	#define S_UART_INFO struct uart_state
#endif

#if defined (CONFIG_MIPS)
	#define ASM_MIPS_ONLY(x) asm(x)
#else
	#define ASM_MIPS_ONLY(x)
#endif

#define SERIAL_UART_AVM_NAME	"ttyS"
#define SERIAL_UART_AVM_MAJOR	TTY_MAJOR
#define SERIAL_UART_AVM_MINOR	64

#if defined(DEBUG_UART_AVM)
	#define UART_ASSERT(a)   if(!(a)) { printk(KERN_ERR"UART_ASSERT(%s) %s:%u\n", #a,  __FILE__, __LINE__); for(;;); }
	#define UART_PRINTK(...) printk(__VA_ARGS__)
#else
	#define UART_ASSERT(a)
	#define UART_PRINTK(...)
#endif

/*--------------------------------------------------------------------------------*\
 * HAL for various uarts
\*--------------------------------------------------------------------------------*/
static int proc_serial_avm_open(struct inode *inode, struct file *file);
static unsigned int uart_avm_tx_empty(struct uart_port *port);
static unsigned int uart_avm_get_mctrl(struct uart_port *port);
static void uart_avm_set_mctrl(struct uart_port *port, unsigned int mctrl);
static void uart_avm_stop_tx(struct uart_port *port);
static void uart_avm_start_tx(struct uart_port *port);
static void uart_avm_stop_rx(struct uart_port *port);
static void uart_avm_enable_ms(struct uart_port *port);
static void uart_avm_break_ctl(struct uart_port *port, int break_state);
static int uart_avm_startup(struct uart_port *port);
static void uart_avm_shutdown(struct uart_port *port);
static void uart_avm_set_termios(struct uart_port *port, struct ktermios *termios, struct ktermios *old);
static const char *uart_avm_type(struct uart_port *port);
static int uart_avm_request_port(struct uart_port *port);
static void uart_avm_release_port(struct uart_port *port);
static int uart_avm_verify_port(struct uart_port *port, struct serial_struct *ser);
static void uart_avm_config_port(struct uart_port *port, int flags);;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_SERIAL_AVM_8250)
#include "serial_avm_8250.c"
#endif
#if defined(CONFIG_SERIAL_AVM_ATH_HI)
#include "serial_avm_ath_hi.c"
#endif /*--- #if defined(CONFIG_SERIAL_AVM_ATH_HI) ---*/
#if defined(CONFIG_SERIAL_AVM_ASC)
#include "serial_avm_asc.c"
#endif /*--- #if defined(CONFIG_SERIAL_AVM_ATH_HI) ---*/

/*--------------------------------------------------------------------------------*\
 * hier alle Serial-Konsolen eingetragen 
\*--------------------------------------------------------------------------------*/
static struct avm_serial_priv avm_uarts_priv_data[] = {
#if defined(CONFIG_SERIAL_AVM_8250)
    /*--- bis zu zwei 8250-Ports: ---*/
    {
    porttype:                   PORT_16550A,
    portname:                   "PORT_16550A",
    /*--- base:                       NULL,  ---*//*--- muss in uart_avm_setup_port gesetzt werden ---*/
    /*--- base_size:                  sizeof(struct avm_8250_regs), ---*/
    uart_avm_setup_port:        uart_avm_8250_setup_port,
    uart_avm_base:              uart_avm_8250_base,
    uart_avm_startup:           uart_avm_8250_startup,
    uart_avm_setup_irq:         uart_avm_8250_setup_irq,
    uart_avm_enable_tx_irq:     uart_avm_8250_enable_tx_irq,
    uart_avm_disable_tx_irq:    uart_avm_8250_disable_tx_irq,
    uart_avm_enable_rx_irq:     uart_avm_8250_enable_rx_irq,
    uart_avm_disable_rx_irq:    uart_avm_8250_disable_rx_irq,

    uart_avm_rx_chars:          uart_avm_8250_rx_chars,

    uart_avm_tx_empty:          uart_avm_8250_tx_empty,
    uart_avm_set_baudrate:      uart_avm_8250_set_baudrate,
    uart_avm_set_wordsize:      uart_avm_8250_set_wordsize,

    uart_avm_console_putchar:   uart_avm_8250_console_putchar,

    uart_avm_name:              "AVM Serial (8250)",
    baudval:                    9600,
    port_specificdata:          NULL,
    },
    {
    porttype:                   PORT_16550A,
    portname:                   "PORT_16550A",
    uart_avm_setup_port:        uart_avm_8250_setup_port,
    uart_avm_base:              uart_avm_8250_base,
    uart_avm_startup:           uart_avm_8250_startup,
    uart_avm_setup_irq:         uart_avm_8250_setup_irq,
    uart_avm_enable_tx_irq:     uart_avm_8250_enable_tx_irq,
    uart_avm_disable_tx_irq:    uart_avm_8250_disable_tx_irq,
    uart_avm_enable_rx_irq:     uart_avm_8250_enable_rx_irq,
    uart_avm_disable_rx_irq:    uart_avm_8250_disable_rx_irq,

    uart_avm_rx_chars:          uart_avm_8250_rx_chars,

    uart_avm_tx_empty:          uart_avm_8250_tx_empty,
    uart_avm_set_baudrate:      uart_avm_8250_set_baudrate,
    uart_avm_set_wordsize:      uart_avm_8250_set_wordsize,

    uart_avm_console_putchar:   uart_avm_8250_console_putchar,

    uart_avm_name:              "AVM Serial (8250)",
    baudval:                    9600,
    port_specificdata:          NULL,
    },
#endif/*--- #if defined(CONFIG_SERIAL_AVM_8250) ---*/
#if defined(CONFIG_SERIAL_AVM_ATH_HI)
    {
    porttype:                   PORT_ATHEROS_HI,
    portname:                   "PORT_ATH_HI",
    uart_avm_base:              uart_avm_ath_hi_base,
    uart_avm_setup_port:        uart_avm_ath_hi_setup_port,
    uart_avm_startup:           uart_avm_ath_hi_startup,
    uart_avm_setup_irq:         uart_avm_ath_hi_setup_irq,
    uart_avm_enable_tx_irq:     uart_avm_ath_hi_enable_tx_irq,
    uart_avm_disable_tx_irq:    uart_avm_ath_hi_disable_tx_irq,
    uart_avm_enable_rx_irq:     uart_avm_ath_hi_enable_rx_irq,
    uart_avm_disable_rx_irq:    uart_avm_ath_hi_disable_rx_irq,

    uart_avm_rx_chars:          uart_avm_ath_hi_rx_chars,

    uart_avm_tx_empty:          uart_avm_ath_hi_tx_empty,
    uart_avm_set_baudrate:      uart_avm_ath_hi_set_baudrate,
    uart_avm_set_wordsize:      uart_avm_ath_hi_set_wordsize,

    uart_avm_console_putchar:   uart_avm_ath_hi_console_putchar,

    uart_avm_name: "AVM Serial (ATH HI)",
    baudval: 9600,
    port_specificdata:          NULL,
    },
#endif/*--- #if defined(CONFIG_SERIAL_AVM_ATH_HI) ---*/
#if defined(CONFIG_SERIAL_AVM_ASC)
    /*--- bis zu zwei ASC-Ports: ---*/
    {
    porttype:                   PORT_IFX_ASC,
    portname:                   "PORT_IFX_ASC",
    uart_avm_base:              uart_avm_asc_base,
    uart_avm_setup_port:        uart_avm_asc_setup_port, 
    uart_avm_startup:           uart_avm_asc_startup,
    uart_avm_setup_irq:         uart_avm_asc_setup_irq,
    uart_avm_enable_tx_irq:     uart_avm_asc_enable_tx_irq,
    uart_avm_disable_tx_irq:    uart_avm_asc_disable_tx_irq,
    uart_avm_enable_rx_irq:     uart_avm_asc_enable_rx_irq,
    uart_avm_disable_rx_irq:    uart_avm_asc_disable_rx_irq,

    uart_avm_rx_chars:          uart_avm_asc_rx_chars,

    uart_avm_tx_empty:          uart_avm_asc_tx_empty,
    uart_avm_set_baudrate:      uart_avm_asc_set_baudrate,
    uart_avm_set_wordsize:      uart_avm_asc_set_wordsize,

    uart_avm_console_putchar:   uart_avm_asc_console_putchar,
    proc_serial_avm_show:       uart_avm_asc_serial_avm_show,

    uart_avm_name: "AVM Serial (ASC)",
    baudval: 9600,
    port_specificdata:          NULL,
    },
    {
    porttype:                   PORT_IFX_ASC,
    portname:                   "PORT_IFX_ASC",
    uart_avm_base:              uart_avm_asc_base,
    uart_avm_setup_port:        uart_avm_asc_setup_port, 
    uart_avm_startup:           uart_avm_asc_startup,
    uart_avm_setup_irq:         uart_avm_asc_setup_irq,
    uart_avm_enable_tx_irq:     uart_avm_asc_enable_tx_irq,
    uart_avm_disable_tx_irq:    uart_avm_asc_disable_tx_irq,
    uart_avm_enable_rx_irq:     uart_avm_asc_enable_rx_irq,
    uart_avm_disable_rx_irq:    uart_avm_asc_disable_rx_irq,

    uart_avm_rx_chars:          uart_avm_asc_rx_chars,

    uart_avm_tx_empty:          uart_avm_asc_tx_empty,
    uart_avm_set_baudrate:      uart_avm_asc_set_baudrate,
    uart_avm_set_wordsize:      uart_avm_asc_set_wordsize,

    uart_avm_console_putchar:   uart_avm_asc_console_putchar,
    proc_serial_avm_show:       uart_avm_asc_serial_avm_show,

    uart_avm_name: "AVM Serial (ASC)",
    baudval: 9600,
    port_specificdata:          NULL,
    },
#endif/*--- #if defined(CONFIG_SERIAL_AVM_ASC) ---*/
};
static int uart_reboot(struct notifier_block *nb, unsigned long event, void *buf);
static struct notifier_block uart_notifier = {
	uart_reboot,   /* callback   */
	NULL,         /* next block */
	1             /* priority   */
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

static struct proc_dir_entry *parent_proc_dir = NULL;

static const struct file_operations read_searial_avm_fops =
{
   .open    = proc_serial_avm_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = seq_release,
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

static struct uart_ops uart_avm_ops = {
	.tx_empty	    = uart_avm_tx_empty,
	.get_mctrl	    = uart_avm_get_mctrl,
	.set_mctrl	    = uart_avm_set_mctrl,
	.stop_tx	    = uart_avm_stop_tx,
	.start_tx	    = uart_avm_start_tx,
	.stop_rx	    = uart_avm_stop_rx,
	.enable_ms	    = uart_avm_enable_ms,
	.break_ctl	    = uart_avm_break_ctl,
	.startup	    = uart_avm_startup,
	.shutdown	    = uart_avm_shutdown,
	.set_termios	= uart_avm_set_termios,
	.type			= uart_avm_type,
	.release_port	= uart_avm_release_port,
	.request_port	= uart_avm_request_port,
	.config_port	= uart_avm_config_port,
	.verify_port	= uart_avm_verify_port,

#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char = uart_avm_get_poll_char,
	.poll_put_char = uart_avm_console_putchar,
#endif /*--- CONFIG_CONSOLE_POLL ---*/
};

/* --- die folgenden Dinge werden im architekturspezifischen Teil implementiert bzw. initialisiert --- */ 
int avm_console_uart = 0;
int uart_reboot_in_progress = 0;

static struct uart_driver uart_avm_driver;
struct uart_port uart_avm_port[ARRAY_ELEMENTS(avm_uarts_priv_data)]; 
static DEFINE_SPINLOCK(avm_serial_lock);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

static int proc_serial_avm_open(struct inode *inode, struct file *file) {

    struct avm_serial_priv *port_priv_data = PDE(inode)->data;
    return single_open(file, port_priv_data->proc_serial_avm_show, port_priv_data );
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void uart_avm_stop_tx(struct uart_port *port) {
    unsigned long flags;
	struct avm_serial_priv *port_priv_data = port->private_data;
    spin_lock_irqsave(&avm_serial_lock, flags);
	if (port_priv_data->tx_enabled) {
#if defined(DEBUG_UART_AVM)
		port_priv_data->stop_tx_cnt++;
#endif
        UART_ASSERT(port_priv_data);
        UART_ASSERT(port_priv_data->uart_avm_disable_tx_irq);
        port_priv_data->uart_avm_disable_tx_irq(port_priv_data);
		port_priv_data->tx_enabled = 0;
	}
    spin_unlock_irqrestore(&avm_serial_lock, flags);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void uart_avm_start_tx(struct uart_port *port) {
    unsigned long flags;
    struct avm_serial_priv *port_priv_data = port->private_data;
    spin_lock_irqsave(&avm_serial_lock, flags);
    if ( !port_priv_data->tx_enabled ) {
        UART_ASSERT(port_priv_data);
        UART_ASSERT(port_priv_data->uart_avm_enable_tx_irq);
#if defined(DEBUG_UART_AVM)
        port_priv_data->start_tx_cnt ++;
#endif
        if(!uart_reboot_in_progress) {
            port_priv_data->uart_avm_enable_tx_irq(port_priv_data);
        }
        port_priv_data->tx_enabled = 1;
    }
    spin_unlock_irqrestore(&avm_serial_lock, flags);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void uart_avm_start_rx(struct uart_port *port) {
    unsigned long flags;
    struct avm_serial_priv *port_priv_data = port->private_data;
    spin_lock_irqsave(&avm_serial_lock, flags);
    if ( !port_priv_data->rx_enabled ) {
        UART_ASSERT(port_priv_data);
        UART_ASSERT(port_priv_data->uart_avm_enable_rx_irq);

        if(!uart_reboot_in_progress) {
            port_priv_data->uart_avm_enable_rx_irq(port_priv_data);
        }
        port_priv_data->rx_enabled = 1;
    }
    spin_unlock_irqrestore(&avm_serial_lock, flags);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void uart_avm_stop_rx(struct uart_port *port) {
    unsigned long flags;
	struct avm_serial_priv *port_priv_data = port->private_data;
    spin_lock_irqsave(&avm_serial_lock, flags);
	if (port_priv_data->rx_enabled) {
        UART_ASSERT(port_priv_data);
        UART_ASSERT(port_priv_data->uart_avm_disable_rx_irq);
        port_priv_data ->uart_avm_disable_rx_irq(port_priv_data);
		port_priv_data->rx_enabled = 0;
	}
    spin_unlock_irqrestore(&avm_serial_lock, flags);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_enable_ms(struct uart_port *port) {

}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int uart_avm_tx_empty(struct uart_port *port) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    UART_ASSERT(port_priv_data);
    UART_ASSERT(port_priv_data->uart_avm_tx_empty);
    return port_priv_data->uart_avm_tx_empty(port);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int uart_avm_get_mctrl(struct uart_port *port) {
	/* no modem control lines */
	return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_set_mctrl(struct uart_port *port, unsigned int mctrl) {
    /* no modem control - just return */
    return;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_break_ctl(struct uart_port *port, int break_state) {
    /* no way to send a break */
    return;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int uart_avm_startup(struct uart_port *port){
    int ret = 0;
    unsigned long flags;
	struct avm_serial_priv *port_priv_data = port->private_data;
    UART_PRINTK(KERN_INFO"[%s] port: %d\n", __func__, port->line);

    UART_ASSERT(port_priv_data);
    UART_ASSERT(port_priv_data->uart_avm_setup_irq);
    /*--- setup Irqs ---*/
    ret = port_priv_data->uart_avm_setup_irq(port, 1);

    spin_lock_irqsave(&avm_serial_lock, flags);
    UART_ASSERT(port_priv_data->uart_avm_startup);
    port_priv_data->uart_avm_startup(port);

    UART_ASSERT(port_priv_data->uart_avm_set_baudrate);
    port_priv_data->uart_avm_set_baudrate(port);
	 
	port_priv_data->tx_enabled = 0;
	port_priv_data->rx_enabled = 0;

    spin_unlock_irqrestore(&avm_serial_lock, flags);
    
	/* enable Interrupts */
    uart_avm_start_tx(port);
    uart_avm_start_rx(port);


	UART_PRINTK("[%s]Setup IRQ %d\n", __FUNCTION__, port->irq);
    return ret;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_shutdown(struct uart_port *port) {
    unsigned long flags;
	struct avm_serial_priv *port_priv_data = port->private_data;
    UART_ASSERT(port_priv_data);

    spin_lock_irqsave(&avm_serial_lock, flags);
    UART_ASSERT(port_priv_data->uart_avm_disable_tx_irq);
    UART_ASSERT(port_priv_data->uart_avm_disable_rx_irq);
    port_priv_data->uart_avm_disable_tx_irq(port_priv_data);
    port_priv_data->uart_avm_disable_rx_irq(port_priv_data);
    spin_unlock_irqrestore(&avm_serial_lock, flags);
    
    port_priv_data->uart_avm_setup_irq(port, 0);
    
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_set_termios(struct uart_port *port, struct ktermios *termios, struct ktermios *old) {
    unsigned long flags;
    unsigned int f_size_chg = 1;
    unsigned int f_baudrate_chg = 1;
    unsigned int cflag = termios->c_cflag;
	struct avm_serial_priv *port_priv_data = port->private_data;
    unsigned int wordsize = 0;
    unsigned int baudrate = 0;

    UART_ASSERT(port_priv_data);

	/*
	 * We don't support modem control lines.
	 */
	termios->c_cflag &= ~(HUPCL | CRTSCTS | CMSPAR);
	termios->c_cflag |= CLOCAL;

    /*--- only support baudrate change and word length change ---*/
    if ( old ) {
        if ( (old->c_cflag & CSIZE) == (cflag & CSIZE) ) {
            f_size_chg = 0;
        }
        if ( (old->c_cflag & CBAUD) == (cflag & CBAUD) ) {
            f_baudrate_chg = 0;
        }
    }
    if (f_size_chg || f_baudrate_chg) {

        if (f_size_chg) {
            switch (cflag & CSIZE) {
				case CS5: wordsize = 5 ; break;
				case CS6: wordsize = 6; break;
				case CS7: wordsize = 7; break;
				case CS8: 
                default:  
                    wordsize = 8;
					break;
			}
		}
        if(f_baudrate_chg) {
			baudrate = uart_get_baud_rate(port, termios, old, 0, port->uartclk / 16 ); 
			if (port->UART_INFO && port->UART_INFO->port.tty) {
				struct tty_struct *tty = port->UART_INFO->port.tty;
				tty_encode_baud_rate(tty, baudrate, baudrate);			
			}
        }

		spin_lock_irqsave(&avm_serial_lock, flags);

		/*
		 * Update the per-port timeout.
		 */
		uart_update_timeout(port, termios->c_cflag, baudrate);

        if(f_size_chg) {
            UART_ASSERT(port_priv_data->uart_avm_set_wordsize);
            UART_PRINTK(KERN_INFO"[%s] wordsize=%d\n", __func__, wordsize);
            port_priv_data->uart_avm_set_wordsize(port, wordsize);
        }
        if ( f_baudrate_chg ) {
            port_priv_data->baudval = baudrate;
            UART_PRINTK(KERN_INFO"[%s] baudrate=%d\n", __func__, baudrate);
            UART_ASSERT(port_priv_data->uart_avm_set_baudrate);
            port_priv_data->uart_avm_set_baudrate(port);
        }
		spin_unlock_irqrestore(&avm_serial_lock, flags);
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int uart_avm_setup_ports(void) {
    unsigned int i, act_idx = 0;
    UART_PRINTK(KERN_INFO"[%s]\n", __func__);
    for(i = 0; i < ARRAY_ELEMENTS(avm_uarts_priv_data); i++) {
        struct avm_serial_priv *port_priv_data = &avm_uarts_priv_data[i];

		if (uart_avm_port[i].private_data == NULL ) {
            unsigned int basesize, is_console;
            void *baseio;

            memset(&uart_avm_port[i], 0, sizeof(uart_avm_port[i]));
			uart_avm_port[i].private_data = (char *)port_priv_data;

            UART_ASSERT(port_priv_data->uart_avm_setup_port);
            if(port_priv_data->uart_avm_setup_port(&uart_avm_port[i], &is_console)) {
                /*--- port gibt es nicht ---*/
                uart_avm_port[i].private_data = NULL;
                continue;
            }
            if(is_console) {
                UART_PRINTK(KERN_INFO"[%s] console is %d\n", __func__, act_idx);
                avm_console_uart = act_idx;
            }
            baseio = port_priv_data->uart_avm_base(&uart_avm_port[i], &basesize);
            uart_avm_port[i].ops          = &uart_avm_ops;
            uart_avm_port[i].flags        = ASYNC_BOOT_AUTOCONF;
            uart_avm_port[i].iobase       = (unsigned int)baseio;
            uart_avm_port[i].membase      = ioremap_nocache(uart_avm_port[i].mapbase, basesize);
            uart_avm_port[i].mapbase      = (unsigned long)baseio;
            uart_avm_port[i].iotype       = SERIAL_IO_MEM;
            uart_avm_port[i].line         = act_idx;
        }
        act_idx++;
    }
    return act_idx;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static const char *uart_avm_type(struct uart_port *port) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    return port_priv_data->portname;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_release_port(struct uart_port *port) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    unsigned int basesize;
    void *baseio;

    UART_PRINTK("[%s] release port %d", __FUNCTION__, port->line);
    UART_ASSERT(port_priv_data);
    UART_ASSERT(port_priv_data->uart_avm_base);
    baseio = port_priv_data->uart_avm_base(port, &basesize);
	release_mem_region((resource_size_t)baseio, basesize); 
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int uart_avm_request_port(struct uart_port *port) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    unsigned int res;
    unsigned int basesize;
    void *baseio;
    UART_ASSERT(port_priv_data);
    UART_ASSERT(port_priv_data->uart_avm_base);
    baseio = port_priv_data->uart_avm_base(port, &basesize);
    UART_PRINTK("[%s] request port %d", __FUNCTION__, port->line);
	res = request_mem_region((resource_size_t)baseio, basesize, port_priv_data->uart_avm_name) != NULL ? 0 : -EBUSY; 
    return res;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_config_port(struct uart_port *port, int flags) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    UART_ASSERT(port_priv_data);
    UART_PRINTK(KERN_INFO"[%s] %d: flags=%x type=%d\n", __func__, port->line, flags, port_priv_data->porttype);
	if ( (flags & UART_CONFIG_TYPE)  && (uart_avm_request_port(port) == 0)) {
        port->type = port_priv_data->porttype;
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int uart_avm_verify_port(struct uart_port *port, struct serial_struct *ser) {
	int ret = 0;

    UART_PRINTK(KERN_INFO"%s: ? \n", __func__);
	if (ser->type != PORT_UNKNOWN && 
        ser->type != PORT_16550A && 
        ser->type != PORT_ATHEROS_HI &&
        ser->type != PORT_IFX_ASC) {
		ret = -EINVAL;
    }
	if (ser->irq != port->irq)
		ret = -EINVAL;
    if ( ser->baud_base < 9600 ) 
		ret = -EINVAL;

	return ret;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void uart_avm_console_stop(void) {
	struct uart_port *port = &uart_avm_port[avm_console_uart];
    uart_avm_shutdown(port);
}
EXPORT_SYMBOL(uart_avm_console_stop);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void uart_avm_console_start(void) {
	struct uart_port *port = &uart_avm_port[avm_console_uart];
    uart_avm_startup(port);
}
EXPORT_SYMBOL(uart_avm_console_start);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int uart_reboot(struct notifier_block *nb __attribute__((unused)), unsigned long event, void *buf __attribute__((unused))) {
	int i;
    /*--- printk("[%s] resetting uart...\n", __FUNCTION__); ---*/
	if ((event != SYS_RESTART) && (event != SYS_HALT) && (event != SYS_POWER_OFF)) {
		return (NOTIFY_DONE);
    }

    uart_reboot_in_progress = 1;

	/*--- Aufraeumen ---*/
    for ( i = 0; i < ARRAY_ELEMENTS(uart_avm_port); i++){
        int ch;
        struct avm_serial_priv *port_priv_data = uart_avm_port[i].private_data;
        if(port_priv_data) {
            UART_ASSERT(port_priv_data->uart_avm_disable_rx_irq);
            UART_ASSERT(port_priv_data->uart_avm_disable_tx_irq);
            UART_ASSERT(port_priv_data->uart_avm_rx_chars);
            port_priv_data->uart_avm_disable_rx_irq(port_priv_data);
            port_priv_data->uart_avm_disable_tx_irq(port_priv_data);
            port_priv_data->uart_avm_rx_chars(&uart_avm_port[i], &ch);
        }
    }

    /*--- printk("[%s] done!n", __FUNCTION__); ---*/
	return (NOTIFY_OK);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int __init uart_avm_init(void) {
	int ret;

	uart_avm_driver.nr = uart_avm_setup_ports();

	ret = uart_register_driver(&uart_avm_driver);
	if (ret == 0){
		int i;
		for ( i = 0; i < ARRAY_ELEMENTS(uart_avm_port); i++){
            if(uart_avm_port[i].private_data){

                struct avm_serial_priv *port_priv_data = uart_avm_port[i].private_data;
                uart_add_one_port(&uart_avm_driver, &uart_avm_port[i]);

                if ( port_priv_data->proc_serial_avm_show ) {
                    if ( !parent_proc_dir ){
                        parent_proc_dir = proc_mkdir( "driver/serial_avm", NULL );
                    }
                    snprintf(port_priv_data->proc_file_name, sizeof(port_priv_data->proc_file_name), "port_%d", i);
                    port_priv_data->proc_entry = create_proc_entry(port_priv_data->proc_file_name, S_IRUGO | S_IWUSR, parent_proc_dir);
                    if ( port_priv_data->proc_entry ){
                        port_priv_data->proc_entry->proc_fops = &read_searial_avm_fops;
                        port_priv_data->proc_entry->data = uart_avm_port[i].private_data;
                    }
                }
            }
		}
	}
	return ret;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void __exit uart_avm_exit(void) {
	int i;
	unregister_reboot_notifier(&uart_notifier);
	for ( i = 0; i <  ARRAY_ELEMENTS(uart_avm_port); i++){
        if(uart_avm_port[i].private_data){
          	struct avm_serial_priv *port_priv_data = uart_avm_port[i].private_data;
            uart_remove_one_port(&uart_avm_driver, &uart_avm_port[i]);
            if ( port_priv_data->proc_entry ){
                remove_proc_entry( port_priv_data->proc_entry->name, parent_proc_dir );
            }
        }
        iounmap(uart_avm_port[i].membase);
	}
	uart_unregister_driver(&uart_avm_driver);

	if ( parent_proc_dir )
        remove_proc_entry( parent_proc_dir->name, NULL );


}

#if defined(AVM_SERIAL_CONSOLE_SUPPORT)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_console_write(struct console *co, const char *s, unsigned int count) {
	struct uart_port *port = &uart_avm_port[avm_console_uart];
    struct avm_serial_priv *port_priv_data = port->private_data;

	uart_console_write(port, s, count, port_priv_data->uart_avm_console_putchar); 
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int __init uart_avm_console_setup(struct console *co, char *options) {
	struct uart_port *port = &uart_avm_port[avm_console_uart];
#if defined(CONFIG_MIPS_UR8)
	int baud = 38400;
#else/*--- #if defined(CONFIG_MIPS_UR8) ---*/
	int baud = 115200;
#endif/*--- #else ---*//*--- #if defined(CONFIG_MIPS_UR8) ---*/
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	if (options) {
		uart_parse_options(options, &baud, &parity, &bits, &flow);
    }
	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console uart_avm_console = {
	.name		= SERIAL_UART_AVM_NAME,
	.write		= uart_avm_console_write,
	.device		= uart_console_device,
	.setup		= uart_avm_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &uart_avm_driver,
};

static int __init rs_avm_console_init(void)
{
	uart_avm_setup_ports();
    uart_avm_console.index = avm_console_uart; /*--- sonst funktioniert sysrq nicht ---*/
	register_console(&uart_avm_console);
	return 0;
}
console_initcall(rs_avm_console_init);
#define UART_AVM_CONSOLE &uart_avm_console
#else 
#define UART_AVM_CONSOLE NULL
#endif/*--- #if defined(AVM_SERIAL_CONSOLE_SUPPORT) ---*/

static struct uart_driver uart_avm_driver = {
	.owner			= THIS_MODULE,
	.driver_name	= SERIAL_UART_AVM_NAME,
	.dev_name		= SERIAL_UART_AVM_NAME,	
	.major			= SERIAL_UART_AVM_MAJOR,
	.minor			= SERIAL_UART_AVM_MINOR,
	.cons			= UART_AVM_CONSOLE,
	/*--- .nr wird nach uart_avm_setup_ports() aufruf gesetzt ---*/
};


module_init(uart_avm_init);
module_exit(uart_avm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AVM Serial Driver for various Uarts");

