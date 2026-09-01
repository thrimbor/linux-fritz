#if defined(CONFIG_SERIAL_IFX_ASC)
#error CONFIG Fehler: entweder CONFIG_SERIAL_IFX_ASC oder CONFIG_SERIAL_UART_AVM
#endif

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/device.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>

#define WORKAROUND_ACK_TIR_ON_ENTRY
#define WORKAROUND_ACK_RIR_ON_ENTRY

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/addrspace.h>
#include <common_routines.h>
#include "serial_avm_asc.h"

static volatile int tx_bugfix_count_irq = 0;
static volatile int tx_irq_correct_count = 0;
static volatile int tx_bugfix_count_enable = 0;
static volatile int err_irq_count = 0;

atomic_t in_rx_irq = ATOMIC_INIT(0);
atomic_t in_tx_irq = ATOMIC_INIT(0);
atomic_t parallel_irq_count = ATOMIC_INIT(0);

static void uart_avm_asc_console_putchar(struct uart_port *port, int ch);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_asc_disable_tx_irq(struct avm_serial_priv *port_priv_data) {
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    ifx_asc_reg_t *asc_reg = priv->base;
    asc_reg->asc_irnen &= ~IFX_ASC_IRQ_LINE_TIR;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

/*
 * is called with spinlock from uart_avm_start_tx
 */
static void uart_avm_asc_enable_tx_irq(struct avm_serial_priv *port_priv_data) {
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    ifx_asc_reg_t *asc_reg = priv->base;

	unsigned int fifocnt, flag = 0;
	fifocnt = (asc_reg->asc_fstat & ASCFSTAT_TXFREEMASK) >> ASCFSTAT_TXFREEOFF;
	if (( fifocnt == 0 ) && !(asc_reg->asc_irnicr & IFX_ASC_IRQ_LINE_TIR) ){
#if defined(WORKAROUND_GEN_MISSING_ENABLE_TX_IRQ)
        asc_reg->asc_irnicr |= IFX_ASC_IRQ_LINE_TIR;
#endif
        tx_bugfix_count_enable ++;
	    flag = 1;
	}

    asc_reg->asc_irnen |= IFX_ASC_IRQ_LINE_TIR;

	if (( fifocnt == 0 ) && !(asc_reg->asc_irnicr & IFX_ASC_IRQ_LINE_TIR) ){
#if defined(WORKAROUND_GEN_MISSING_ENABLE_TX_IRQ)
        asc_reg->asc_irnicr |= IFX_ASC_IRQ_LINE_TIR;
#endif
        tx_bugfix_count_enable ++;
	    flag |= 2;
	}
	if (flag)
	    printk(KERN_ERR "[%s] %#x", __func__,flag);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_asc_disable_rx_irq(struct avm_serial_priv *port_priv_data) {
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    ifx_asc_reg_t *asc_reg = priv->base;
    asc_reg->asc_irnen &= ~IFX_ASC_IRQ_LINE_RIR;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_asc_enable_rx_irq(struct avm_serial_priv *port_priv_data) {
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    ifx_asc_reg_t *asc_reg = priv->base;
    asc_reg->asc_irnen |= IFX_ASC_IRQ_LINE_RIR;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void uart_avm_asc_rx_chars(struct uart_port *port, unsigned int *drop_char){
    struct avm_serial_priv *port_priv_data = port->private_data;
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    ifx_asc_reg_t *asc_reg = priv->base;

    struct uart_state *ustate = port->state;
    struct tty_struct *tty = ustate->port.tty;
    unsigned int ch = 0, rsr = 0, fifocnt;
    char flag;

#if defined(WORKAROUND_ACK_RIR_ON_ENTRY)
    asc_reg->asc_irncr = IFX_ASC_IRQ_LINE_RIR;
#endif

    mb();
    fifocnt = asc_reg->asc_fstat & ASCFSTAT_RXFFLMASK;
	if ( unlikely(priv->clear_be_in_next_irq) ) {
        asc_reg->asc_whbstate = ASCWHBSTATE_CLRBE;
		priv->clear_be_in_next_irq = 0;
	}
    while (fifocnt--) {
        ch = asc_reg->asc_rbuf;
        mb();
        rsr = (asc_reg->asc_state & (ASCSTATE_ANY | ASCSTATE_BE)) | UART_DUMMY_UER_RX;

        /*--- if(*drop_char) { ---*/
        /*---     continue; ---*/
        /*--- } ---*/
        flag = TTY_NORMAL;
        port->icount.rx++;
        priv->rx_bytes++;

        /*
         * Note that the error handling code is
         * out of the main execution path
         */
        if ( unlikely(rsr & (ASCSTATE_ANY | ASCSTATE_BE)) ) {
            if ( (rsr & ASCSTATE_PE) ) {
                port->icount.parity++;
                priv->rx_parity_error++;
                asc_reg->asc_whbstate = ASCWHBSTATE_CLRPE;
                mb();
            }
            else if ( (rsr & ASCSTATE_FE) ) {
                port->icount.frame++;
                priv->rx_frame_error++;
                asc_reg->asc_whbstate = ASCWHBSTATE_CLRFE;
                mb();
            }

            if ( (rsr & ASCSTATE_ROE) ) {
                port->icount.overrun++;
                priv->rx_overrun_error++;
                asc_reg->asc_whbstate = ASCWHBSTATE_CLRROE;
                mb();
            }
            if ( (rsr & ASCSTATE_BE ) ) {
                flag = TTY_BREAK;
                port->icount.brk++;
				priv->clear_be_in_next_irq = 1;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				if (uart_handle_break(port)){
					goto ignore_char;
				}
			}

            rsr &= port->read_status_mask;
			if ( (rsr & ASCMCON_PAL) )
                flag = TTY_PARITY;
            else if ( (rsr & ASCMCON_FEN) )
                flag = TTY_FRAME;
        }
		if (uart_handle_sysrq_char(port, ch)){
			goto ignore_char;
		}
        uart_insert_char(port, rsr, ASCMCON_ROEN, ch, flag);
    }

ignore_char:
    if ( ch != 0 ) //  If the final ch == 0, we will not do tty_flip_buffer_push.
        tty_flip_buffer_push(tty);


#if defined(WORKAROUND_ACK_RIR_ON_LEAVE)
    asc_reg->asc_irncr = IFX_ASC_IRQ_LINE_RIR;
    mb();
#endif
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static irqreturn_t uart_avm_asc_rx_irq(int irq, void *dev_id) {
    unsigned int ch;

    smp_mb__before_atomic_inc();

    if ( atomic_inc_return( &in_rx_irq ) > 1 )
	    BUG(); //reentry

    smp_mb__after_atomic_inc();

	if (atomic_read( &in_tx_irq )){
        smp_mb__before_atomic_inc();
        atomic_inc( &parallel_irq_count );
        smp_mb__after_atomic_inc();
	}

	mb();
	uart_avm_asc_rx_chars((struct uart_port *)dev_id, &ch);

    smp_mb__before_atomic_dec();
    atomic_dec( &in_rx_irq );
    smp_mb__after_atomic_dec();
	return IRQ_HANDLED;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/

static irqreturn_t uart_avm_asc_err_irq(int irq, void *dev_id)
{
    struct uart_port *port = (struct uart_port *)dev_id;
    struct avm_serial_priv *port_priv_data = port->private_data;
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;

    priv->base->asc_whbstate = ASCWHBSTATE_CLRPE | ASCWHBSTATE_CLRFE | ASCWHBSTATE_CLRROE
                            | ASCWHBSTATE_SETRUE | ASCWHBSTATE_SETTOE | ASCWHBSTATE_SETBE;
    err_irq_count ++;

    return IRQ_HANDLED;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_asc_tx_chars(struct uart_port *port){
    struct avm_serial_priv *port_priv_data = port->private_data;
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    struct uart_state *ustate = port->state; 
	struct circ_buf *xmit = &ustate->xmit;
    ifx_asc_reg_t *asc_reg = priv->base;
	int fifocnt;


#if defined(WORKAROUND_ACK_TIR_ON_ENTRY)
	asc_reg->asc_irncr = IFX_ASC_IRQ_LINE_TIR;
	mb();
#endif

	if (port->x_char) {
		uart_avm_asc_console_putchar(port, (int)(port->x_char));
		port->icount.tx++;
		port->x_char = 0;
        return;
	}


	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		uart_avm_stop_tx(port);
        return;
	}

    mb();
	fifocnt = (asc_reg->asc_fstat & ASCFSTAT_TXFREEMASK) >> ASCFSTAT_TXFREEOFF;
	while(fifocnt--) {
		if ( priv->portwidth == 8 ) {
			asc_reg->asc_tbuf = xmit->buf[xmit->tail];
		} else {
			*(((char*)&asc_reg->asc_tbuf) + 3) = xmit->buf[xmit->tail];
		}
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} 

    mb();

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit)) {
		uart_avm_stop_tx(port);
    } else if ((( asc_reg->asc_fstat & ASCFSTAT_TXFREEMASK ) >> ASCFSTAT_TXFREEOFF ) == 0 ) {

        mb();

        if (!( asc_reg->asc_irnicr & IFX_ASC_IRQ_LINE_TIR )){
#if defined(WORKAROUND_GEN_MISSING_TX_IRQ)
            asc_reg->asc_irnicr |= IFX_ASC_IRQ_LINE_TIR;
#endif
            tx_bugfix_count_irq ++;
        } else {
            tx_irq_correct_count ++;
        }

	}

#if defined(WORKAROUND_ACK_TIR_ON_LEAVE)
    mb();
	asc_reg->asc_irncr = IFX_ASC_IRQ_LINE_TIR;
#endif

}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static irqreturn_t uart_avm_asc_tx_irq(int irq, void *dev_id) {

    smp_mb__before_atomic_inc();
	if ( atomic_inc_return( &in_tx_irq ) > 1)
	    BUG(); //reentry
    smp_mb__after_atomic_inc();

	if (atomic_read( &in_rx_irq ) ){
        smp_mb__before_atomic_inc();
        atomic_inc( &parallel_irq_count );
        smp_mb__after_atomic_inc();
	}

	mb();
    uart_avm_asc_tx_chars((struct uart_port *)dev_id);

    smp_mb__before_atomic_dec();
    atomic_dec( &in_tx_irq );
    smp_mb__after_atomic_dec();
	return IRQ_HANDLED;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int uart_avm_asc_setup_irq(struct uart_port *port, unsigned int on){
    struct avm_serial_priv *port_priv_data = port->private_data;
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    ifx_asc_reg_t *asc_reg = priv->base;
    int ret;
    if(on) {
        ret = request_irq(priv->rir, uart_avm_asc_rx_irq, 0, priv->rx_irq_name, port);
        if (ret == 0) {
            ret = request_irq(priv->tir, uart_avm_asc_tx_irq, 0, priv->tx_irq_name, port);
            if (ret) {
                free_irq(priv->rir, port);
            }
            ret = request_irq(priv->eir, uart_avm_asc_err_irq, 0, priv->err_irq_name, port);
            if (ret) {
                free_irq(priv->rir, port);
                free_irq(priv->tir, port);
            }
        }
        return ret;
    }
    /* disable ASC interrupts in module */
    asc_reg->asc_irnen = IFX_ASC_IRQ_LINE_MASK_ALL;
    
	disable_irq_nosync(priv->rir);
    disable_irq_nosync(priv->tir);
    disable_irq_nosync(priv->eir);

	free_irq(priv->tir, port);
	free_irq(priv->rir, port);
	free_irq(priv->eir, port);
    
	/* turn off baudrate generator */
    asc_reg->asc_mcon &= ~ASCMCON_R;
    
	/* flush and then disable the fifos */
    asc_reg->asc_rxfcon = ASCRXFCON_RXFFLU;
    asc_reg->asc_txfcon = ASCTXFCON_TXFFLU;
    return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int uart_avm_asc_tx_empty(struct uart_port *port) {
    struct avm_serial_priv *port_priv_data = port->private_data;
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    /*
     * FSTAT tells exactly how many bytes are in the FIFO.
     * The question is whether we really need to wait for all
     * 16 bytes to be transmitted before reporting that the
     * transmitter is empty.
     */
	return (priv->base->asc_fstat & ASCFSTAT_TXFFLMASK) ? 0 : TIOCSER_TEMT;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_asc_startup(struct uart_port *port) {
    struct avm_serial_priv *port_priv_data = port->private_data;
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    ifx_asc_reg_t *asc_reg = priv->base;
    
#if defined(CONFIG_AR9)
    UART0_PMU_SETUP(IFX_PMU_ENABLE);
#endif /*--- #if defined(CONFIG_AR9) ---*/

    /* disable ASC interrupts in module */
    asc_reg->asc_irnen &= ~IFX_ASC_IRQ_LINE_TBIR;
    /* enable ASC interrupts in module */
    asc_reg->asc_irnen = IFX_ASC_IRQ_LINE_RIR | IFX_ASC_IRQ_LINE_EIR | IFX_ASC_IRQ_LINE_TIR;

	if ( priv->portwidth == 8){
		/* setup fifos and timeout */
		asc_reg->asc_txfcon = 0x101;
		asc_reg->asc_rxfcon = 0xa01;
		asc_reg->asc_eomcon = 0x1040600;
	} else {
		/* setup fifos and timeout */
		asc_reg->asc_txfcon = 0x101;
		asc_reg->asc_rxfcon = 0x1001;
		asc_reg->asc_eomcon = 0x1040300;
	}

	asc_reg->asc_irnicr  = 0x3; //trigger one RX and one TX IRQ
	// asc_reg->asc_irncr = 0x3;

}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void uart_avm_asc_set_baudrate(struct uart_port *port){
    struct avm_serial_priv *port_priv_data = port->private_data;
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    ifx_asc_reg_t *asc_reg = priv->base;
    unsigned int fdv, reload;
    unsigned asc_state;

    get_fdv_and_reload_value(port_priv_data->baudval, &fdv, &reload);
    priv->baudrate = port_priv_data->baudval;
    asc_state = asc_reg->asc_state;
    /* disable receiver */
    asc_reg->asc_whbstate = ASCWHBSTATE_CLRREN;

    /* Now we can write the new baudrate into the register */
    asc_reg->asc_fdv = fdv;
    asc_reg->asc_bg  = reload;
	asc_reg->asc_mcon |= ASCMCON_R;

    /* enable receiver if necessary */
    if ((asc_state & ASCSTATE_REN)) {
        asc_reg->asc_whbstate = ASCWHBSTATE_SETREN;
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void uart_avm_asc_set_wordsize(struct uart_port *port, int wordsize){
    struct avm_serial_priv *port_priv_data = port->private_data;
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    unsigned int word_length;
    ifx_asc_reg_t *asc_reg = priv->base;
    unsigned asc_state;

    asc_state = asc_reg->asc_state;
    if(wordsize == 7 ) {
        word_length = ASCMCON_WLS_7BIT << ASCMCON_WLSOFFSET;
    } else {
        word_length = ASCMCON_WLS_8BIT << ASCMCON_WLSOFFSET;
    }
    /* disable receiver */
    asc_reg->asc_whbstate = ASCWHBSTATE_CLRREN;

    asc_reg->asc_mcon = (asc_reg->asc_mcon & ~(3 << 2)) | word_length;

    /* enable receiver if necessary */
    if ((asc_state & ASCSTATE_REN)) {
        asc_reg->asc_whbstate = ASCWHBSTATE_SETREN;
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void *uart_avm_asc_base(struct uart_port *port, unsigned int *basesize){
    struct avm_serial_priv *port_priv_data = port->private_data;
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;

    if(basesize)*basesize = sizeof(ifx_asc_reg_t);
    return (void *)((unsigned int)priv->base & ~KSEG1);
}
static int initasc_once = 0;
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int uart_avm_asc_setup_port(struct uart_port *port, int *is_console) {
    struct avm_serial_priv *port_priv_data = port->private_data;
	struct ifx_asc_port_priv *priv;

	if (initasc_once >= NR_UARTS){
		return 1;
	}
    *is_console = (initasc_once == ASC_CONSOLE_UART) ? 1 : 0;
    port_priv_data->port_specificdata =  &ifx_asc_port_priv[initasc_once];
    priv = port_priv_data->port_specificdata;
	priv->portwidth = (priv->base->asc_id & ASCID_TX32) ? 32 : 8;

	port->irq      = priv->rir;
    port->uartclk  = ifx_get_fpi_hz();
	port->fifosize = (priv->base->asc_id & ASCID_TXFS_MASK) >> ASCID_TXFS_OFF;
	port->type     = PORT_IFX_ASC;
	initasc_once++;
    return 0;
}
#ifdef CONFIG_CONSOLE_POLL
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int uart_avm_asc_console_getchar(struct uart_port *port) {
	struct ifx_asc_port_priv *priv = port->port_priv_data->port_specificdata;
    ifx_asc_reg_t *asc_reg = priv->base;
	char c;

    while( (asc_reg->asc_fstat & ASCFSTAT_RXFFLMASK) == 0)
		mb();

    c = asc_reg->asc_rbuf;
    return c;
}
#endif // CONFIG_CONSOLE_POLL
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void uart_avm_asc_console_putchar(struct uart_port *port, int ch) {
    struct avm_serial_priv *port_priv_data = port->private_data;
	struct ifx_asc_port_priv *priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;
    ifx_asc_reg_t *asc_reg = priv->base;

    while ( ( asc_reg->asc_fstat & ASCFSTAT_TXFFLMASK) == (IFX_ASC_TXFIFO_FULL << ASCFSTAT_TXFFLOFF) ){
		mb();
	}

    /* We have either portwidth = 8 or portwidth = 32 */
    if(priv->portwidth == 8) {
        asc_reg->asc_tbuf = (char)ch;
    } else {
        *(((char*)&asc_reg->asc_tbuf) + 3) = (char)ch;
    }
    mb();
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void get_fdv_and_reload_value(unsigned int baudrate, unsigned int *fdv, unsigned int *reload) {
    unsigned int fpi_clk = ifx_get_fpi_hz();
    unsigned int baudrate1 = baudrate * 8192;
    unsigned long long baudrate2 = (unsigned long long)baudrate * 1000;
    unsigned long long fdv_over_bg_fpi;
    unsigned long long fdv_over_bg;
    unsigned long long difference;
    unsigned long long min_difference;
    unsigned int bg;

    /* Sanity check first */
    if (baudrate >= (fpi_clk >> 4)) {
        printk(KERN_ERR "%s current fpi clock %u can't provide baudrate %u!!!\n", __func__, fpi_clk, baudrate);
        return;
    }
    //  baudrate = fpiclk * (fdv / 512) / (16 * (bg + 1))

    min_difference = UINT_MAX;
    fdv_over_bg_fpi = baudrate1;
    for ( bg = 1; bg <= 8192; bg++, fdv_over_bg_fpi += baudrate1 ) {
        fdv_over_bg = fdv_over_bg_fpi + fpi_clk / 2;
        do_div(fdv_over_bg, fpi_clk);
        if ( fdv_over_bg <= 512 ) {
            difference = fdv_over_bg * fpi_clk * 1000;
            do_div(difference, 8192 * bg);
            if ( difference < baudrate2 )
                difference = baudrate2 - difference;
            else
                difference -= baudrate2;
            if ( difference < min_difference ) {
                *fdv = (unsigned int)fdv_over_bg & 511;
                *reload = bg - 1;
                min_difference = difference;
            }
            /* Perfect one found */
            if (min_difference == 0) {
                break;
            }
        }
    }
}
EXPORT_SYMBOL(get_fdv_and_reload_value);

/*------------------------------------------------------------------------------------------*\
 * wird von cgu_set_clock benoetigt
\*------------------------------------------------------------------------------------------*/
void ifx_update_asc_clock_settings(void)
{
    ifx_asc_reg_t *asc_reg;
    u32 fdv, reload;
    u32 asc_state;
    int i;

    for ( i = 0; i < initasc_once; i++ ) {
        asc_reg = ifx_asc_port_priv[i].base;

        get_fdv_and_reload_value(ifx_asc_port_priv[i].baudrate, &fdv, &reload);

        asc_state = asc_reg->asc_state;
        /* disable receiver */
        asc_reg->asc_whbstate = ASCWHBSTATE_CLRREN;
        /* now we can write the new baudrate into the register */
        asc_reg->asc_fdv = fdv;
        asc_reg->asc_bg  = reload;
        /* enable receiver if necessary */

        mb();
        if ((asc_state & ASCSTATE_REN)) {
            asc_reg->asc_whbstate = ASCWHBSTATE_SETREN;
        }
        mb();
    }
}
EXPORT_SYMBOL(ifx_update_asc_clock_settings);

/*------------------------------------------------------------------------------------------*\
 * Debugging Proc-Interface
\*------------------------------------------------------------------------------------------*/
static int uart_avm_asc_serial_avm_show(struct seq_file *seq, void *data ) {

    struct avm_serial_priv *port_priv_data = (struct avm_serial_priv *)seq->private;
	struct ifx_asc_port_priv *asc_priv = (struct ifx_asc_port_priv *)port_priv_data->port_specificdata;

	ifx_asc_reg_t *asc_reg = asc_priv->base;

    seq_printf(seq, "--------------------------------------------------------\n");
    seq_printf(seq, " tx_bugfix_count_irq=%d, tx_irq_correct_count=%d\n tx_bugfix_count_enable=%d\n err_irq_count=%d \n",
            tx_bugfix_count_irq, tx_irq_correct_count, tx_bugfix_count_enable, err_irq_count );
    seq_printf(seq, " rx_enabled=%d, tx_enabled=%d, parallel_irq_count=%d\n",
                port_priv_data->rx_enabled, port_priv_data->tx_enabled, atomic_read( &parallel_irq_count ) );
    seq_printf(seq, "--------------------------------------------------------\n\n");

    seq_printf( seq, "   base         = 0x%p \n", asc_reg               );
    seq_printf( seq, "   clc          = %#lx \n", asc_reg->asc_clc      );
    seq_printf( seq, "   pisel        = %#lx \n", asc_reg->asc_pisel    );
    seq_printf( seq, "   id           = %#lx \n", asc_reg->asc_id       );
    seq_printf( seq, "   mcon         = %#lx \n", asc_reg->asc_mcon     );
    seq_printf( seq, "   state        = %#lx \n", asc_reg->asc_state    );
    seq_printf( seq, "   whbstate     = %#lx \n", asc_reg->asc_whbstate );
    seq_printf( seq, "   tbuf         = %#lx \n", asc_reg->asc_tbuf     );
    seq_printf( seq, "   rbuf         = %#lx \n", asc_reg->asc_rbuf     );
    seq_printf( seq, "   abcon        = %#lx \n", asc_reg->asc_abcon    );
    seq_printf( seq, "   abstat       = %#lx \n", asc_reg->asc_abstat   );
    seq_printf( seq, "   whbabcon     = %#lx \n", asc_reg->asc_whbabcon );
    seq_printf( seq, "   whbabstat    = %#lx \n", asc_reg->asc_whbabstat);
    seq_printf( seq, "   rxfcon       = %#lx \n", asc_reg->asc_rxfcon   );
    seq_printf( seq, "   txfcon       = %#lx \n", asc_reg->asc_txfcon   );
    seq_printf( seq, "   fstat        = %#lx \n", asc_reg->asc_fstat    );
    seq_printf( seq, "   bg           = %#lx \n", asc_reg->asc_bg       );
    seq_printf( seq, "   bg_timer     = %#lx \n", asc_reg->asc_bg_timer );
    seq_printf( seq, "   fdv          = %#lx \n", asc_reg->asc_fdv      );
    seq_printf( seq, "   pmw          = %#lx \n", asc_reg->asc_pmw      );
    seq_printf( seq, "   modcon       = %#lx \n", asc_reg->asc_modcon   );
    seq_printf( seq, "   modstat      = %#lx \n", asc_reg->asc_modstat  );
    seq_printf( seq, "   sfcc         = %#lx \n", asc_reg->asc_sfcc     );
    seq_printf( seq, "   eomcon       = %#lx \n", asc_reg->asc_eomcon   );
    seq_printf( seq, "   dmacon       = %#lx \n", asc_reg->asc_dmacon   );
    seq_printf( seq, "   irnen        = %#lx \n", asc_reg->asc_irnen    );
    seq_printf( seq, "   irncr        = %#lx \n", asc_reg->asc_irncr    );
    seq_printf( seq, "   irnicr       = %#lx \n", asc_reg->asc_irnicr   );

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AVM Serial Driver for Infineon ASC Uart");

