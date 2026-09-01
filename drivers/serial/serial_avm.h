#ifndef __serial_avm_h__
#define __serial_avm_h__

#define PROC_FILE_NAME_LEN 64

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct avm_serial_priv {
    unsigned int porttype;
    const char   *portname;
    void *(*uart_avm_base)(struct uart_port *port, unsigned int *basesize);
    int  (*uart_avm_setup_port)(struct uart_port *port, int *is_console);
    void (*uart_avm_startup)(struct uart_port *);
    int (*uart_avm_setup_irq)(struct uart_port *, unsigned int on);

    void (*uart_avm_enable_tx_irq)(struct avm_serial_priv *);
    void (*uart_avm_disable_tx_irq)(struct avm_serial_priv *);
    void (*uart_avm_enable_rx_irq)(struct avm_serial_priv *);
    void (*uart_avm_disable_rx_irq)(struct avm_serial_priv *);

    void (*uart_avm_rx_chars)(struct uart_port *, unsigned int *);
    void (*uart_avm_tx_chars)(struct uart_port *);

    unsigned int (*uart_avm_tx_empty)(struct uart_port *);
    void (*uart_avm_set_baudrate)(struct uart_port *);
    void (*uart_avm_set_wordsize)(struct uart_port *, int );

    void (*uart_avm_console_putchar)(struct uart_port *, int );
    int  (*proc_serial_avm_show)(struct seq_file *, void *);

    char *uart_avm_name;

#if defined(DEBUG_UART_AVM)
	u32					 stop_tx_cnt;
	u32					 start_tx_cnt;
#endif
    u32 baudval;
    void *port_specificdata;
    volatile int tx_enabled;
    volatile int rx_enabled;

    struct proc_dir_entry *proc_entry;
    char proc_file_name[PROC_FILE_NAME_LEN];
}; 

void uart_avm_start_rx(struct uart_port *port);
#define ARRAY_ELEMENTS(a) (sizeof(a) / sizeof((a)[0]))

#endif/*--- #ifndef __serial_avm_h__ ---*/
