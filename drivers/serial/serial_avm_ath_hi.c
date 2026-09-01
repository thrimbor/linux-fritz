#include "serial_avm_ath_hi.h"
#include "atheros.h"
#include "934x_clock.h"

struct _portath_hi_priv {
    struct avm_ath_hi_regs *regs;
    unsigned int irq;
    unsigned int fifosize;
    unsigned int is_console;
};

struct _portath_hi_priv portath_hi_priv[1] = {
    { regs: (struct avm_ath_hi_regs *)KSEG1ADDR(ATH_HS_UART_BASE), irq:  ATH_MISC_IRQ_HS_UART, fifosize: 4, is_console: 0 },
};

static int initath_hi_once = 0;
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int uart_avm_ath_hi_setup_port(struct uart_port *port, int *is_console) {
    struct avm_serial_priv *port_priv_data = port->private_data;
    struct _portath_hi_priv *priv;

	if (initath_hi_once >= ARRAY_ELEMENTS(portath_hi_priv)){
		return 1;
	}
    port_priv_data->port_specificdata = &portath_hi_priv[initath_hi_once];
    priv = &portath_hi_priv[initath_hi_once];

    *is_console    = priv->is_console;
	port->fifosize = priv->fifosize;
	port->irq      = priv->irq;
	port->type     = PORT_ATHEROS_HI;
	port->uartclk  = ath_uart_freq;

	initath_hi_once++;
    return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void *uart_avm_ath_hi_base(struct uart_port *port, unsigned int *basesize){
    struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;

    if(basesize)*basesize = sizeof(struct avm_ath_hi_regs);
    return (void *)((unsigned int)base_ath_hi & ~KSEG1);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void uart_avm_ath_hi_enable_tx_irq(struct avm_serial_priv *port_priv_data) {
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;
    base_ath_hi->config.Bits.irq_en   = 0;
    base_ath_hi->enable.Bits.tx_empty = 1;
    base_ath_hi->config.Bits.irq_en   = 1;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_ath_hi_disable_tx_irq(struct avm_serial_priv *port_priv_data) {
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;
    base_ath_hi->config.Bits.irq_en   = 0;
    base_ath_hi->enable.Bits.tx_empty = 0;
    base_ath_hi->config.Bits.irq_en   = 1;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_ath_hi_enable_rx_irq(struct avm_serial_priv *port_priv_data) {
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;
    base_ath_hi->config.Bits.irq_en       = 0;
    base_ath_hi->enable.Bits.rx_valid_int = 1;
    base_ath_hi->enable.Bits.rx_full      = 1;
    base_ath_hi->config.Bits.irq_en       = 1;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_ath_hi_disable_rx_irq(struct avm_serial_priv *port_priv_data) {
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;
    base_ath_hi->config.Bits.irq_en       = 0;
    base_ath_hi->enable.Bits.rx_valid_int = 0;
    base_ath_hi->enable.Bits.rx_full      = 1;
    base_ath_hi->config.Bits.irq_en       = 1;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline int uart_avm_ath_hi_read_data_register(struct uart_port *port, struct avm_ath_hi_regs *base_ath_hi, unsigned int *p_ch) {
    struct _avm_ath_hi_regs_data wert = base_ath_hi->data.Bits;
    unsigned int ignore_char = 0;
    /*--- union avm_ath_hi_regs_config config = base_ath_hi->config; ---*/

    /*--- if(config.Bits.rx_busy) { ---*/
        /*--- printk(KERN_ERR "[%s] rx busy 0x%x\n", __FUNCTION__, config.Register); ---*/
        /*--- return -1; ---*/
    /*--- } ---*/

    if(wert.rx_csr) {
        if(base_ath_hi->status.Bits.rx_break_on) {
            port->icount.brk++;
            ignore_char = TTY_BREAK;
        }
        if(base_ath_hi->status.Bits.rx_parity_err) {
            port->icount.parity++;
            ignore_char = TTY_PARITY;
        }
        if(base_ath_hi->status.Bits.rx_overflow_err) {
            port->icount.overrun++;
            *p_ch = wert.tx_rx_data;
            wert.tx_csr = 0;
            wert.rx_csr = 1;
            base_ath_hi->data.Bits = wert;
            ignore_char = TTY_OVERRUN;
        }
        if(base_ath_hi->status.Bits.rx_framing_err) {
            port->icount.frame++;
            ignore_char = TTY_FRAME;
		}
        if(ignore_char == 0) {
		    port->icount.rx++;
            *p_ch = wert.tx_rx_data;
            wert.tx_csr = 0;
            wert.rx_csr = 1;
            base_ath_hi->data.Bits = wert;
            return TTY_NORMAL;
        }
        return ignore_char;
    }
    return -1;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void uart_avm_ath_hi_rx_chars(struct uart_port *port, unsigned int *drop_char){
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;
	struct tty_struct *tty = port->UART_INFO->port.tty;
	unsigned int ch = 0;
	char flag = TTY_NORMAL;
    int status;


    status = uart_avm_ath_hi_read_data_register(port, base_ath_hi, &ch);
    /*--- printk(KERN_ERR "[%s] status 0x%x '%c'\n", __FUNCTION__, status, ch); ---*/

    /*--- if(*drop_char) { ---*/
    /*---     return; ---*/
    /*--- } ---*/

    while(status >= 0) {
        /*--- printk(KERN_ERR "'%c'%s\n", ch, ---*/
                /*--- status == TTY_BREAK ? " TTY_BREAK" : ---*/
                /*--- status == TTY_PARITY ? " TTY_PARITY" : ---*/
                /*--- status == TTY_OVERRUN ? " TTY_OVERRUN" : ---*/
                /*--- status == TTY_FRAME ? " TTY_FRAME" : ---*/
                /*--- status == TTY_NORMAL ? " TTY_NORMAL" : " unknown" ---*/
                /*--- ); ---*/
        if(status == TTY_NORMAL) {

            if (uart_handle_sysrq_char(port, ch)) {
                /*--- printk(KERN_ERR "[%s] sysrq char => ignore_char '%c'\n", __FUNCTION__, ch); ---*/
                goto ignore_char;
            }

            /*--- printk(KERN_ERR "[%s] '%x' '%c'\n", __FUNCTION__, ch, ch); ---*/


            uart_insert_char(port, 0, 0, ch, flag);
            spin_unlock(&port->lock);
            tty_flip_buffer_push(tty);
            spin_lock(&port->lock);
        }

ignore_char:
        status = uart_avm_ath_hi_read_data_register(port, base_ath_hi, &ch);
        /*--- printk(KERN_ERR "[%s] status 0x%x '%c'\n", __FUNCTION__, status, ch); ---*/
	}
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline int uart_avm_ath_hi_write_data_register(struct avm_ath_hi_regs *base_ath_hi, int ch) {
    struct _avm_ath_hi_regs_data wert = base_ath_hi->data.Bits;
    /*--- union avm_ath_hi_regs_config config = base_ath_hi->config; ---*/

    /*--- if(config.Bits.tx_busy) { ---*/
        /*--- printk(KERN_ERR "[%s] tx busy 0x%x\n", __FUNCTION__, config.Register); ---*/
        /*--- return 0; ---*/
    /*--- } ---*/
    if(wert.tx_csr) {
        /*--- printk(KERN_ERR "[%s] '%c' success\n", __FUNCTION__, ch); ---*/
        /*--- printk(KERN_ERR "Tx:'%c'\n", ch); ---*/
        wert.tx_rx_data = ch;
        wert.tx_csr = 1;
        wert.rx_csr = 0;
        base_ath_hi->data.Bits = wert;
        return 1;
    }
    /*--- printk(KERN_ERR "[%s] '%c' failed\n", __FUNCTION__, ch); ---*/
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void uart_avm_ath_hi_tx_chars(struct uart_port *port ){
	struct circ_buf *xmit = &port->UART_INFO->xmit;
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;

    /*--- printk(KERN_ERR "[%s] \n", __FUNCTION__); ---*/

	if (port->x_char) {
        uart_avm_ath_hi_write_data_register(base_ath_hi, (int)(port->x_char)); 
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
    while( uart_avm_ath_hi_write_data_register(base_ath_hi, xmit->buf[xmit->tail])) {
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
static irqreturn_t uart_avm_ath_hi_irq(int irq, void *dev_id) {
	struct uart_port *port = dev_id;
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;
	unsigned int handled = 0;
    union avm_ath_hi_regs_irq_status status = base_ath_hi->status;
    /*--- union avm_ath_hi_regs_config config; ---*/

    /*--- printk(KERN_ERR "--------------------------------------\n"); ---*/

    while(status.Register & base_ath_hi->enable.Register) {

        /*--- printk(KERN_ERR "[%s] interrupt status 0x%x\n", __FUNCTION__, status.Register); ---*/
        /*--- printk(KERN_ERR "[%s] interrupt enable 0x%x\n", __FUNCTION__, base_ath_hi->enable.Register); ---*/
        /*--- printk(KERN_ERR "[%s] uart      config 0x%x\n", __FUNCTION__, base_ath_hi->config.Register); ---*/

        status.Register &= base_ath_hi->enable.Register;

        if(status.Bits.tx_empty) {
            union avm_ath_hi_regs_irq_status ack_status;
            status.Bits.tx_empty = 0;
            ack_status.Register = 0;
            ack_status.Bits.tx_empty = 1;
            base_ath_hi->status = ack_status;

            /*--- printk(KERN_ERR "[%s] TX empty \n", __FUNCTION__); ---*/
            uart_avm_ath_hi_tx_chars(port);
        }

#if 0
        if(base_ath_hi->status.Bits.tx_ready_int) {
            base_ath_hi->status.Bits.tx_ready_int = 1;
            printk(KERN_ERR "[%s] TX ready \n", __FUNCTION__);
            uart_avm_ath_hi_tx_chars(port);
            handled = 1;
        }
#endif
        if(status.Bits.rx_break_off) {
            union avm_ath_hi_regs_irq_status ack_status;
            int drop = 1;
            status.Bits.rx_break_off = 0;
            ack_status.Register = 0;
            ack_status.Bits.rx_break_off = 1;
            uart_avm_ath_hi_rx_chars(port, &drop);

            base_ath_hi->enable.Bits.rx_break_on  = 1;
            base_ath_hi->enable.Bits.rx_break_off = 0;
            base_ath_hi->status = ack_status;

            /*--- printk(KERN_ERR "[%s] break off irq\n", __FUNCTION__); ---*/
        }
        if(status.Bits.rx_break_on) {
            union avm_ath_hi_regs_irq_status ack_status;
            int drop = 1;
            status.Bits.rx_break_on = 0;
            ack_status.Register = 0;
            ack_status.Bits.rx_break_on = 1;
            uart_avm_ath_hi_rx_chars(port, &drop);

            base_ath_hi->enable.Bits.rx_break_on  = 0;
            base_ath_hi->enable.Bits.rx_break_off = 1;
            base_ath_hi->status = ack_status;

            /*--- printk(KERN_ERR "[%s] break on irq\n", __FUNCTION__); ---*/
        }
        if(status.Bits.rx_parity_err) {
            union avm_ath_hi_regs_irq_status ack_status;
            int drop = 1;
            status.Bits.rx_parity_err = 0;
            ack_status.Register = 0;
            ack_status.Bits.rx_parity_err = 1;
            uart_avm_ath_hi_rx_chars(port, &drop);
            base_ath_hi->status = ack_status;

            /*--- printk(KERN_ERR "[%s] parity error irq\n", __FUNCTION__); ---*/
        }
        if(status.Bits.rx_overflow_err) {
            union avm_ath_hi_regs_irq_status ack_status;
            int drop = 1;
            status.Bits.rx_overflow_err = 0;
            ack_status.Register = 0;
            ack_status.Bits.rx_parity_err = 1;
            uart_avm_ath_hi_rx_chars(port, &drop);
            base_ath_hi->status = ack_status;

            /*--- printk(KERN_ERR "[%s] overflow error irq\n", __FUNCTION__); ---*/
        }
        if(status.Bits.rx_framing_err) {
            union avm_ath_hi_regs_irq_status ack_status;
            int drop = 1;
            status.Bits.rx_framing_err = 0;
            ack_status.Register = 0;
            ack_status.Bits.rx_parity_err = 1;
            uart_avm_ath_hi_rx_chars(port, &drop);
            base_ath_hi->status = ack_status;

            /*--- printk(KERN_ERR "[%s] framing error irq\n", __FUNCTION__); ---*/
        }

        if(status.Bits.rx_full) {
            union avm_ath_hi_regs_irq_status ack_status;
            int drop = 0;
            status.Bits.rx_full = 0;
            ack_status.Register = 0;
            ack_status.Bits.rx_full = 1;
            uart_avm_ath_hi_rx_chars(port, &drop);
            base_ath_hi->status = ack_status;

            /*--- printk(KERN_ERR "[%s] RX full \n", __FUNCTION__); ---*/
        }

        if(status.Bits.rx_valid_int) {
            union avm_ath_hi_regs_irq_status ack_status;
            int drop = 0;
            status.Bits.rx_valid_int = 0;
            ack_status.Register = 0;
            ack_status.Bits.rx_valid_int = 1;
            uart_avm_ath_hi_rx_chars(port, &drop);
            base_ath_hi->status = ack_status;

            /*--- printk(KERN_ERR "[%s] RX valid \n", __FUNCTION__); ---*/
        }
        if(status.Register)
            printk(KERN_ERR "[%s] unhandle irq 0x%x\n", __FUNCTION__, status.Register);

        status = base_ath_hi->status;
        handled = 1;
        /*--- printk(KERN_ERR "[%s] uart (end) config 0x%x\n", __FUNCTION__, base_ath_hi->config.Register); ---*/
    }

    /*--- printk(KERN_ERR "[%s] handled %d \n", __FUNCTION__, handled); ---*/
        handled = 1;
	return IRQ_RETVAL(handled);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int uart_avm_ath_hi_tx_empty(struct uart_port *port) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;
	unsigned long flags;
	unsigned int status;
    /*--- printk(KERN_ERR "[%s] \n", __FUNCTION__); ---*/
	spin_lock_irqsave(port->lock, flags);
    /*--- if(base_ath_hi->status.Bits.tx_empty || base_ath_hi->status.Bits.tx_ready_int) { ---*/
    if(base_ath_hi->status.Bits.tx_empty) {
        status = 1;
    }
	spin_unlock_irqrestore(port->lock, flags);
	return status;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int uart_avm_ath_hi_get_serial_clock(void) {
    union _934x_switch_clock_source_control *clk_ctrl = (union _934x_switch_clock_source_control *)((ATH_SWITCH_CLK_CONFIG) | KSEG1);
    /*--- printk(KERN_ERR "[%s] switch_clock_source_control (%p) 0x%x\n", __FUNCTION__, clk_ctrl, clk_ctrl->Register); ---*/
    if(clk_ctrl->Bits.uart1_clk_sel) {
        /*--- printk(KERN_ERR "[%s] uart1 clock is set to 100 MHz\n", __FUNCTION__); ---*/
        return 100 * 1000 * 1000;
    }
    switch(clk_ctrl->Bits.usb_refclk_freq_sel) {
        case usb_refclk_freq_sel_25MHz:
            /*--- printk(KERN_ERR "[%s] uart1 clock is set refclock 25 MHz\n", __FUNCTION__); ---*/
            return 25 * 1000 * 1000;
        case usb_refclk_freq_sel_40MHz:
            /*--- printk(KERN_ERR "[%s] uart1 clock is set refclock 40 MHz\n", __FUNCTION__); ---*/
            return 40 * 1000 * 1000;
        default:
            /*--- printk(KERN_ERR "[%s] uart1 clock is set refclock 0 MHz, unknown refclock select value %d \n", __FUNCTION__, clk_ctrl->Bits.usb_refclk_freq_sel); ---*/
            break;
    }
    return 0;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void uart_avm_ath_hi_set_wordsize(struct uart_port *port __attribute__((unused)), int wordsize __attribute__((unused))) {
/*--- 	struct avm_serial_priv *port_priv_data = port->private_data; ---*/
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void uart_avm_ath_hi_set_baudrate(struct uart_port *port) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;
    int baudClockFreq = port_priv_data->baudval;
    int serialClockFreq = uart_avm_ath_hi_get_serial_clock();

    printk(KERN_ERR "[%s/%d] set baudrate to '%d' baud clock '%d' serial clock '%d'\n", __FUNCTION__, __LINE__, port_priv_data->baudval, baudClockFreq, serialClockFreq );
    base_ath_hi->clock.Bits.clock_scale = ((serialClockFreq >> 17) * 1310) / baudClockFreq;
    base_ath_hi->clock.Bits.clock_step = ((128 * (baudClockFreq / 100) * (base_ath_hi->clock.Bits.clock_scale + 1)) << 10) / (serialClockFreq / 100);
    /*--- printk(KERN_ERR "[%s/%d] scale %d step %d\n", __FUNCTION__, __LINE__, base_ath_hi->clock.Bits.clock_scale, base_ath_hi->clock.Bits.clock_step); ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void uart_avm_ath_hi_console_putchar(struct uart_port *port, int ch) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;

    /*--- printk(KERN_ERR "[%s] \n", __FUNCTION__); ---*/
    uart_avm_ath_hi_write_data_register(base_ath_hi, ch);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int uart_avm_ath_hi_setup_irq(struct uart_port *port, unsigned int on){
	struct avm_serial_priv *port_priv_data = port->private_data;
    if(on) {
        return request_irq(port->irq, uart_avm_ath_hi_irq, IRQF_DISABLED, port_priv_data->uart_avm_name, port);
    }
	disable_irq( port->irq);
	free_irq( port->irq, port);
    return 0;
}   
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_ath_hi_startup(struct uart_port *port) {
	struct avm_serial_priv *port_priv_data = port->private_data;
    struct avm_ath_hi_regs *base_ath_hi = ((struct _portath_hi_priv *)(port_priv_data->port_specificdata))->regs;
    union avm_ath_hi_regs_config config; 
    union avm_ath_hi_regs_irq_status enable;
    union _934x_switch_clock_source_control *clk_ctrl = (union _934x_switch_clock_source_control *)((ATH_SWITCH_CLK_CONFIG) | KSEG1);
    clk_ctrl->Bits.uart1_clk_sel = 1;
    wmb();
    ath_reg_rmw_clear(ATH_RESET, ATH_RESET_UART1);

    enable.Register = 0;
    enable.Bits.tx_empty = 0;
    enable.Bits.rx_full = 0;
    enable.Bits.rx_break_off = 0;
    enable.Bits.rx_break_on = 0;
    enable.Bits.rx_parity_err = 0;
    enable.Bits.tx_overflow_err = 0;
    enable.Bits.rx_overflow_err = 0;
    enable.Bits.rx_framing_err = 0;
    enable.Bits.tx_ready_int = 0;
    enable.Bits.tx_ready_int = 0;
    enable.Bits.rx_valid_int = 0;

    config.Register = 0;
    config.Bits.flow_control = 3; /* reverse RTS/CTS flow control */
    /*--- config.Bits.flow_control = 2; ---*/ /* normal RTS/CTS flow control */
    config.Bits.rx_ready_oride = 0; /*--- kein flow control immer ready ---*/
    config.Bits.tx_ready_oride = 0; /*--- kein flow control immer ready ---*/
    /*------------------------------------------------------------------*\
     * ACHTUNG: Hier ist das Datenblatt falsch: 2 ist DTE Mode zumindest nach dem Applikation sheet
    \*------------------------------------------------------------------*/
    config.Bits.interface_mode = 2; /* DTE mode, tx on TD, rx on RD */
    config.Bits.parity_mode = 0; /* keine parity */
    config.Bits.irq_en = 0;

    wmb();
    base_ath_hi->enable = enable;
    wmb();
    base_ath_hi->config = config;

}
