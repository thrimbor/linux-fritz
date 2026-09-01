#ifndef _hw_uart_h_
#define _hw_uart_h_

extern void uart_avm_console_stop(void);
extern void uart_avm_console_start(void);

struct _hw_uart {
    union _hw_data {
        struct _hw_rx_data {
#ifdef __BIG_ENDIAN
            unsigned int reserved : 24;
            volatile unsigned int data : 8;
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int data : 8;
            unsigned int reserved : 24;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } rx;
        struct _hw_tx_data {
#ifdef __BIG_ENDIAN
            unsigned int reserved : 24;
            volatile unsigned int data : 8;
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int data : 8;
            unsigned int reserved : 24;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } tx;
        volatile unsigned int Register;
    } data;
    union _hw_ie {
        struct __hw_ie {
#ifdef __BIG_ENDIAN
            volatile unsigned int reserved : 28;
            volatile unsigned int eddsi : 1;  /*--- modem status interrupt ---*/
            volatile unsigned int elsi : 1;  /*--- line status interrupt ---*/
            volatile unsigned int etbei : 1;  /*--- transmitter holding register empty ---*/
            volatile unsigned int erbi : 1;  /*--- rx data avail ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int erbi : 1;  /*--- rx data avail ---*/
            volatile unsigned int etbei : 1;  /*--- transmitter holding register empty ---*/
            volatile unsigned int elsi : 1;  /*--- line status interrupt ---*/
            volatile unsigned int eddsi : 1;  /*--- modem status interrupt ---*/
            volatile unsigned int reserved : 4;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } ie;
    union _hw_iir_fcr {
        struct _hw_fcr {    /*--- write only ---*/
#ifdef __BIG_ENDIAN
           volatile unsigned int reserved : 26; 
            volatile unsigned int rxtrg : 2; /*--- rx trigger level for fifo ---*/
            volatile unsigned int dmam : 1;  /*--- dma mode ---*/
            volatile unsigned int txrst : 1; /*--- reset tx ---*/
            volatile unsigned int rxrst : 1;  /*--- reset rx ---*/
            volatile unsigned int fen : 1;  /*--- enable Fifo ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int fen : 1;  /*--- enable Fifo ---*/
            volatile unsigned int rxrst : 1;  /*--- reset rx ---*/
            volatile unsigned int txrst : 1; /*--- reset tx ---*/
            volatile unsigned int dmam : 1;  /*--- dma mode ---*/
            volatile unsigned int rxtrg : 2; /*--- rx trigger level for fifo ---*/
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits_fcr;
        struct _hw_iir {    /*--- read only ---*/
#ifdef __BIG_ENDIAN
            volatile unsigned int reserved2 : 24; 
            volatile unsigned int fifo_en : 1;
            volatile unsigned int reserved : 2;
            volatile unsigned int int_id : 4;
            volatile unsigned int no_int : 1;  /*--- no interrupt pending ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int no_int : 1;  /*--- no interrupt pending ---*/
            volatile unsigned int int_id : 4;
            volatile unsigned int reserved : 2;
            volatile unsigned int fifo_en : 1;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits_iir;
        volatile unsigned int Register;
    } iir_fcr;
    union _hw_lc {
        struct __hw_lc {
#ifdef __BIG_ENDIAN
            volatile unsigned int reserved : 24; 
            volatile unsigned int dlab : 1;
            volatile unsigned int bcb : 1;
            volatile unsigned int spb : 1;
            volatile unsigned int eps : 1;
            volatile unsigned int pen : 1;
            volatile unsigned int stb : 1;
            volatile unsigned int ws : 2;
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int ws : 2;
            volatile unsigned int stb : 1;
            volatile unsigned int pen : 1;
            volatile unsigned int eps : 1;
            volatile unsigned int spb : 1;
            volatile unsigned int bcb : 1;
            volatile unsigned int dlab : 1;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } lc;
    union _hw_mc {
        struct __hw_mc {
#ifdef __BIG_ENDIAN
            volatile unsigned int reserved : 26;
            volatile unsigned int afe : 1;
            volatile unsigned int loop : 1;
            volatile unsigned int out2 : 1;
            volatile unsigned int out1 : 1;
            volatile unsigned int rts : 1;
            volatile unsigned int dtr : 1;
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int dtr : 1;
            volatile unsigned int rts : 1;
            volatile unsigned int out1 : 1;
            volatile unsigned int out2 : 1;
            volatile unsigned int loop : 1;
            volatile unsigned int afe : 1;
            volatile unsigned int reserved : 2;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
        volatile unsigned int data : 8;
        unsigned int reserved : 24;
    } mc;
/* LSR status */
#define SIO_LS_RX            0x01    /* Character ready             */
#define SIO_LS_OE            0x02    /* RX-ERROR: Overrun           */
#define SIO_LS_PE            0x04    /* RX-ERROR: Parity            */
#define SIO_LS_FE            0x08    /* RX-ERROR: Framing (stop bit)*/
#define SIO_LS_BI            0x10    /* 'BREAK' detected            */
#define SIO_LS_TE            0x20    /* Transmit Holding empty      */
#define SIO_LS_TI            0x40    /* Transmitter empty (IDLE)    */
#define SIO_LS_FIFOERR       0x80    /* RX-ERROR: FIFO              */
    union __hw_ls {
        struct _hw_ls {
#ifdef __BIG_ENDIAN
            volatile unsigned int reserved : 24;
            volatile unsigned int fifierr : 1;
            volatile unsigned int ti : 1;
            volatile unsigned int te : 1;
            volatile unsigned int bi : 1;
            volatile unsigned int fe : 1;
            volatile unsigned int pe : 1;
            volatile unsigned int oe : 1;
            volatile unsigned int rx : 1;
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int rx : 1;
            volatile unsigned int oe : 1;
            volatile unsigned int pe : 1;
            volatile unsigned int fe : 1;
            volatile unsigned int bi : 1;
            volatile unsigned int te : 1;
            volatile unsigned int ti : 1;
            volatile unsigned int fifierr : 1;
            volatile unsigned int reserved : 24;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } ls;
/* MSR status */
#define SIO_MS_CTS           0x10    /* Clear to send               */
#define SIO_MS_DSR           0x20    /* Data Set Ready              */
#define SIO_MS_RI            0x40    /* Ring Indicator              */
#define SIO_MS_DCD           0x80    /* Data carrier detect         */
    union __hw_ms {
        struct _hw_ms {
#ifdef __BIG_ENDIAN
            unsigned int reserved2 : 24;
            volatile unsigned int dcd : 1;
            volatile unsigned int ri : 1;
            volatile unsigned int dsr : 1;
            volatile unsigned int cts : 1;
            unsigned int reserved : 4;
#else/*--- #ifdef __BIG_ENDIAN ---*/
            unsigned int reserved : 4;
            volatile unsigned int cts : 1;
            volatile unsigned int dsr : 1;
            volatile unsigned int ri : 1;
            volatile unsigned int dcd : 1;
            unsigned int reserved2 : 24;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } ms;
};
#endif/*--- #ifndef _hw_uart_h_ ---*/
