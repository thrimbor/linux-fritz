/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

#include "serial_avm_8250.h"

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)
struct _port8250_priv {
    struct avm_8250_regs *regs;
    unsigned int irq;
    unsigned int fifosize;
    unsigned int is_console;
};

#if defined(CONFIG_MIPS_UR8) 
#include <asm/addrspace.h>
#include <asm/mach-ur8/ur8_clk.h>

struct _port8250_priv port8250_priv[] = {
    { regs: (struct avm_8250_regs *)UR8_UART0_BASE, irq:  UR8INT_UART0, fifosize: 16, is_console: 1 },
    /*--- zweite Serielle nicht anschalten: (sonst Resourcekonflikt mit Piglet) ---*/
    /*--- { regs: (struct avm_8250_regs *)UR8_UART1_BASE, irq:  UR8INT_UART1, fifosize: 16, is_console: 0 } ---*/
};
#elif defined(CONFIG_MACH_ATHEROS)
#include "atheros.h"
struct _port8250_priv port8250_priv[1] = {
    { regs: (struct avm_8250_regs *)KSEG1ADDR(ATH_UART_BASE), irq:  ATH_MISC_IRQ_UART, fifosize: 16, is_console: 1 },
};
#else
#error: unknown platform 
#endif/*--- #if defined(CONFIG_MIPS_UR8)  ---*/

#if defined(CONFIG_CPU_FREQ) && defined(CONFIG_MIPS_UR8)
/*--------------------------------------------------------------------------------*\
 * UR8: die CPU-Clk und damit die Peripheral-Clk hat sich geaendert: Baudrate anpassen
\*--------------------------------------------------------------------------------*/
static int cpufreq_notifier(struct notifier_block *nb, unsigned long val, void *data) {
    struct uart_port *port = &uart_avm_port[avm_console_uart];
	struct avm_serial_priv *port_priv_data = port->private_data;
    unsigned long flags;

    if(port_priv_data == NULL) {
        return 0; /*--- kein UART-Setup ---*/
    }
	if (val == CPUFREQ_PRECHANGE) {
    } else if(val == CPUFREQ_POSTCHANGE) {
        struct avm_8250_regs *regs = ((struct _port8250_priv *)port_priv_data->port_specificdata)->regs;
		struct avm_8250_regs_dlab *dlab_regs = (struct avm_8250_regs_dlab*)regs;
        unsigned int quot;
        unsigned int baudrate = 38400; /*--- unsauber: wir gehen fix von 38400 aus ---*/

		port->uartclk = avm_get_clock(avm_clock_id_peripheral);
        quot          = (((port->uartclk) / baudrate) + 8) >> 4; 
        port_priv_data->baudval = quot;
		spin_lock_irqsave(&port->lock, flags);
        /* hardware in DLAB mode */
        regs->lcr |= UART_LCR_DLAB;
        /* die unteren 8 bit in dll */ 
        dlab_regs->dll = (quot & 0xFF);
        /* die oberen 8 bit in dlh */ 
        dlab_regs->dlh = (quot >> 8);

        regs->lcr &= ~(UART_LCR_DLAB);
        /* hardware wieder in normal mode */
        spin_unlock_irqrestore(&port->lock, flags);
        /*--- printk("[%s] peripheral-clk %d\n", __func__, port->uartclk); ---*/
    }
	return 0;
}
static struct notifier_block notifier_cpufreq_block = {
	.notifier_call = cpufreq_notifier
};
#endif/*--- #if defined(CONFIG_CPU_FREQ) && defined(CONFIG_MIPS_UR8) ---*/
static int init8250_once = 0;
/*--------------------------------------------------------------------------------*\
 * ret 0: alloziert
\*--------------------------------------------------------------------------------*/
int uart_avm_8250_setup_port(struct uart_port *port, int *is_console) {
    struct avm_serial_priv *port_priv_data = port->private_data;
    struct _port8250_priv *priv;

	if (init8250_once >= ARRAY_ELEMENTS(port8250_priv)){
		return 1;
	}
    port_priv_data->port_specificdata = &port8250_priv[init8250_once];
    priv = &port8250_priv[init8250_once];

    *is_console = priv->is_console;

	port->fifosize = priv->fifosize;
	port->irq      = priv->irq;
	port->type     = PORT_16550A;
#if defined(CONFIG_MIPS_UR8) 
	port->uartclk  = ur8_get_clock(avm_clock_id_peripheral);
#elif defined(CONFIG_MACH_ATHEROS)
	port->uartclk  = ath_uart_freq;
#else
#error: unknown platform 
#endif/*--- #if defined(CONFIG_MIPS_UR8)  ---*/
	init8250_once++;
    return 0;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void uart_avm_8250_enable_tx_irq(struct avm_serial_priv *port_priv_data) {
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;
    base_8250->ier |= UART_IER_THRI; /* Enable Transmitter Holding Register Empty Interrupt */
    wmb();
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_8250_disable_tx_irq(struct avm_serial_priv *port_priv_data) {
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;
    base_8250->ier &= ~UART_IER_THRI; /* Disable Transmitter Holding Register Empty Interrupt */
    wmb();
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_8250_enable_rx_irq(struct avm_serial_priv *port_priv_data) {
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;
    base_8250->ier |= (UART_IER_RLSI | UART_IER_RDI); /* 0x04 Enable RX Interrupt */
    wmb();
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_8250_disable_rx_irq(struct avm_serial_priv *port_priv_data) {
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;
    base_8250->ier &= ~(UART_IER_RLSI | UART_IER_RDI); /* 0x04 Disable RX Interrupt */
    wmb();
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void uart_avm_8250_rx_chars(struct uart_port *port , unsigned int *lsrp){
	unsigned int lsr = *lsrp;
	int max_count = 256;
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;
	S_UART_INFO *UART_INFO = port->UART_INFO;
	struct tty_struct *tty = UART_INFO->port.tty;
	unsigned int ch = 0;
	char flag;

	while ((max_count-- > 0) && (lsr & (UART_LSR_DR | UART_LSR_BI))) {
		ch = base_8250->rbr_thr;
		flag = TTY_NORMAL;
		port->icount.rx++;

		/*
		 * Note that the error handling code is
		 * out of the main execution path
		 */

		if (unlikely(lsr & UART_LSR_BRK_ERROR_BITS)) {
			/*
			 * For statistics only
			 */
			if (lsr & UART_LSR_BI) {
				lsr &= ~(UART_LSR_FE | UART_LSR_PE);
				port->icount.brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				if (uart_handle_break(port))
					goto ignore_char;
			} else if (lsr & UART_LSR_PE)
				port->icount.parity++;
			else if (lsr & UART_LSR_FE)
				port->icount.frame++;
			if (lsr & UART_LSR_OE)
				port->icount.overrun++;

			/*
			 * Mask off conditions which should be ignored.
			 */
			lsr &= port->read_status_mask;

			if (lsr & UART_LSR_BI) {
				flag = TTY_BREAK;
			} else if (lsr & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (lsr & UART_LSR_FE)
				flag = TTY_FRAME;
		}
		if (uart_handle_sysrq_char(port, ch))
			goto ignore_char;

		uart_insert_char(port, lsr, UART_LSR_OE, ch, flag);
ignore_char:
		lsr = base_8250->lsr; 
	}
	spin_unlock(&port->lock);
	tty_flip_buffer_push(tty);
	spin_lock(&port->lock);
	*lsrp = lsr;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void uart_avm_8250_tx_chars(struct uart_port *port ){
	struct circ_buf *xmit = &port->UART_INFO->xmit;
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;
	int fifocnt;

	if (port->x_char) {
		port_priv_data->uart_avm_console_putchar(port, (int)(port->x_char));
		port->icount.tx++;
		port->x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		uart_avm_stop_tx(port);
		return;
	}

	/* 
	 * Constraint: Funktion wird nur aufgerufen wenn fifo komplett leer, 
	 * somit passen immer fifosize Zeichen in die Queue
	 */
	fifocnt = port->fifosize;
	while(fifocnt--) {
		base_8250->rbr_thr = xmit->buf[xmit->tail];
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} 

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit))
		uart_avm_stop_tx(port);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static irqreturn_t uart_avm_8250_irq(int irq, void *dev_id) {
	struct uart_port *port = dev_id;
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;
	unsigned int handled = 0;
	unsigned int iir;

	iir = base_8250->iir_fcr;

	/* Vorsicht invers: letztes bit ist '0' falls Interrupt anliegt */
	if (!(iir & UART_IIR_NO_INT)) {
		unsigned int lsr;
		lsr = base_8250->lsr; 

		if ( port_priv_data->rx_enabled && (lsr & (UART_LSR_DR | UART_LSR_BI))){
			uart_avm_8250_rx_chars(port, &lsr);
			handled = 1;
		}
		if ( port_priv_data->tx_enabled && (lsr & UART_LSR_THRE)) { 
            /* tx_fifo komplett leer */
            uart_avm_8250_tx_chars(port);
            handled = 1;
        }
    }
	return IRQ_RETVAL(handled);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_8250_startup(struct uart_port *port __attribute__((unused))) {
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int uart_avm_8250_setup_irq(struct uart_port *port, unsigned int on){
	struct avm_serial_priv *port_priv_data = port->private_data;
    if(on) {
        uart_avm_start_rx(port); /*--- hier schon rx-enablen, sonst gibt es unhandled irqs  ---*/
        return request_irq(port->irq, uart_avm_8250_irq, IRQF_DISABLED, port_priv_data->uart_avm_name, port);
    }
	disable_irq( port->irq);
	free_irq( port->irq, port);
    return 0;
}   
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void *uart_avm_8250_base(struct uart_port *port, unsigned int *basesize){
    struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;

    if(basesize)*basesize = sizeof(struct avm_8250_regs);
    return (void *)((unsigned int)base_8250 & ~KSEG1);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int uart_avm_8250_tx_empty(struct uart_port *port) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;
	unsigned long flags;
	unsigned int lsr;
	spin_lock_irqsave(&port->lock, flags);
	lsr = base_8250->lsr; 
	spin_unlock_irqrestore(&port->lock, flags);
	return ((lsr & BOTH_EMPTY) == BOTH_EMPTY ) ? 1 : 0;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void uart_avm_8250_set_wordsize(struct uart_port *port, int wordsize) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;
    unsigned int lcr;

    lcr = base_8250->lcr;
    lcr &= ~0x3;
    lcr |= wordsize - 5;
    base_8250->lcr = lcr;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void uart_avm_8250_set_baudrate(struct uart_port *port) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;

    unsigned int quot = uart_get_divisor(port, port_priv_data->baudval);
	base_8250->iir_fcr = UART_FCR_R_TRIG_11 |UART_FCR_CLEAR_XMIT | UART_FCR_CLEAR_RCVR | UART_FCR_ENABLE_FIFO;
    
    if(quot) {
		struct avm_8250_regs_dlab *dlab_regs = (struct avm_8250_regs_dlab*)base_8250;
        /*--- restauriere evtl. vorhandene Baudrate: ---*/
        /* hardware in DLAB mode */
        base_8250->lcr |= UART_LCR_DLAB;
        /* die unteren 8 bit in dll */ 
        dlab_regs->dll = (quot & 0xFF);
        /* die oberen 8 bit in dlh */ 
        dlab_regs->dlh = (quot >> 8);

        base_8250->lcr &= ~(UART_LCR_DLAB);
        /* hardware wieder in normal mode */
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void uart_avm_8250_console_putchar(struct uart_port *port, int ch) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;
	unsigned int status, tmout = 10000;

	base_8250->rbr_thr = (char)ch;
    ASM_MIPS_ONLY("sync");

	/* Wait up to 10ms for the character(s) to be sent. */
	do {
		status = base_8250->lsr;
		if (--tmout == 0)
			break;
		udelay(1);
	} while ((status & UART_LSR_THRE ) != UART_LSR_THRE );
}

#ifdef CONFIG_CONSOLE_POLL
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int uart_avm_8250_get_poll_char(struct uart_port *port) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_8250_regs *base_8250 = ((struct _port8250_priv *)(port_priv_data->port_specificdata))->regs;
	
    unsigned char lsr = base_8250->lsr;
	while (!(UART_LSR_DR & lsr ))
		lsr = base_8250->lsr;
	return base_8250->rbr_thr;
}
#endif
#if 0
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_console_write(struct console *co, const char *s, unsigned int count) {
    struct uart_port *port = &uart_avm_port[avm_console_uart];
	struct avm_serial_priv *port_priv_data = port->private_data;
    if(port_priv_data == NULL) {  /* HACK fuer early console */
	    uart_console_write(port, s, count, uart_avm_8250_console_putchar); 
    } else {
	    uart_console_write(port, s, count, port_priv_data->uart_avm_console_putchar); 
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int __init uart_avm_console_setup(struct console *co, char *options) {
	struct uart_port *port = &uart_avm_port[avm_console_uart];
	int baud = 38400;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	return uart_set_options(port, co, baud, parity, bits, flow);
}
static struct uart_driver uart_avm_driver;

static struct console uart_avm_console =
{
	.name		= SERIAL_UART_AVM_NAME,
	.write		= uart_avm_console_write,
	.device		= uart_console_device,
	.setup		= uart_avm_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &uart_avm_driver,
};
#endif
#if defined(CONFIG_CPU_FREQ) && defined(CONFIG_MIPS_UR8)
static int __init _avmserial_cpufreq_register_init(void) {
    /*--- printk("[%s]\n", __func__); ---*/
	cpufreq_register_notifier(&notifier_cpufreq_block, CPUFREQ_TRANSITION_NOTIFIER);
	return 0;
}
late_initcall(_avmserial_cpufreq_register_init);
#endif/*--- #if define(CONFIG_CPU_FREQ) && defined(CONFIG_MIPS_UR8) ---*/
