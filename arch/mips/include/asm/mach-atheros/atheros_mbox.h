#ifndef __atheros_mbox__
#define __atheros_mbox__

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union _ath_mbox_fifo_status {
    struct __ath_mbox_fifo_status {
#ifdef __BIG_ENDIAN
        unsigned int reserved1:           29;
        volatile unsigned int empty:       1;       /*--- mbox0 Tx-Fifo is empty ---*/
        volatile unsigned int reserved2:   1;       
        volatile unsigned int full:        1;       /*--- mbox0 Tx-Fifo is full  ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
        volatile unsigned int full:        1;       /*--- mbox0 Tx-Fifo is full  ---*/
        volatile unsigned int reserved1:   1;       
        volatile unsigned int empty:       1;       /*--- mbox0 Tx-Fifo is empty ---*/
        unsigned int reserved2:           29;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
    } Bits;
    volatile unsigned int Register;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union _ath_mbox_slic_fifo_status {
    struct __ath_mbox_slic_fifo_status {
#ifdef __BIG_ENDIAN
        unsigned int reserved:            30;
        volatile unsigned int empty:       1;       /*--- slic mbox Tx-Fifo is empty ---*/
        volatile unsigned int full:        1;       /*--- slic mbox Tx-Fifo is full  ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
        volatile unsigned int full:        1;       /*--- slic mbox Tx-Fifo is full  ---*/
        volatile unsigned int empty:       1;       /*--- slic mbox Tx-Fifo is empty ---*/
        unsigned int reserved:            30;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
    } Bits;
    volatile unsigned int Register;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union _ath_mbox_dma_policy {
    struct __ath_mbox_dma_policy {
#ifdef __BIG_ENDIAN
        unsigned int reserved1:              20;
        volatile unsigned int tx_16bit_swap:  1; /*--- if set, will swap bytes within a 16-bit word in the tx direction ---*/
        volatile unsigned int rx_16bit_swap:  1; /*--- if set, will swap bytes within a 16-bit word in the rx direction ---*/
        volatile unsigned int tx_end_swap:    1; /*--- 1: is set, will swap bytes in a 32 bit word in the rx-direction ---*/
        volatile unsigned int rx_end_swap:    1; /*--- 1: is set, will swap bytes in a 32 bit word in the rx-direction ---*/
        volatile unsigned int tx_fifo_tresh0: 4; /*--- treshhold (32 Bit) for Tx Chain0 ---*/
        volatile unsigned int reserved2:      4;      
#else/*--- #ifdef __BIG_ENDIAN ---*/
        volatile unsigned int reserved2:      4;      
        volatile unsigned int tx_fifo_tresh0: 4; /*--- treshhold (32 Bit) for Tx Chain0 ---*/
        volatile unsigned int rx_end_swap:    1; /*--- if set, will swap bytes in a 32 bit word in the rx-direction ---*/
        volatile unsigned int tx_end_swap:    1; /*--- if set, will swap bytes in a 32 bit word in the rx-direction ---*/
        volatile unsigned int rx_16bit_swap:  1; /*--- if set, will swap bytes within a 16-bit word in the rx direction ---*/
        volatile unsigned int tx_16bit_swap:  1; /*--- if set, will swap bytes within a 16-bit word in the tx direction ---*/
        unsigned int reserved1:               20;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
    } Bits;
    volatile unsigned int Register;
};
/*--------------------------------------------------------------------------------*\
 * Read-only
\*--------------------------------------------------------------------------------*/
union _ath_mbox_frame {
    struct __ath_mbox_frame {
#ifdef __BIG_ENDIAN
        unsigned int reserved1:           29;
        volatile unsigned int rx_eom:      1;  /*--- end of message marker ---*/
        unsigned int reserved2:            1;
        volatile unsigned int rx_som:      1;  /*--- start of message marker   ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
        volatile unsigned int rx_som:      1;    
        unsigned int reserved2:            1;
        volatile unsigned int rx_eom:      1;  
        unsigned int reserved1:           29;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
    } Bits;
    volatile unsigned int Register;
};
/*--------------------------------------------------------------------------------*\
 * Read-only
\*--------------------------------------------------------------------------------*/
union _ath_mbox_slic_frame {
    struct __ath_mbox_slic_frame {
#ifdef __BIG_ENDIAN
        unsigned int reserved:            30;
        volatile unsigned int rx_eom:      1;  /*--- end of message marker ---*/
        volatile unsigned int rx_som:      1;  /*--- start of message marker   ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
        volatile unsigned int rx_som:      1;    
        volatile unsigned int rx_eom:      1;  
        unsigned int reserved:            30;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
    } Bits;
    volatile unsigned int Register;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union _ath_mbox_dma_control {
    struct __ath_mbox_dma_control {
#ifdef __BIG_ENDIAN
        unsigned int reserved:            29;
        volatile unsigned int resume:      1;  
        volatile unsigned int start:       1;   
        volatile unsigned int stop:        1;    
#else/*--- #ifdef __BIG_ENDIAN ---*/
        volatile unsigned int stop:        1;    
        volatile unsigned int start:       1;   
        volatile unsigned int resume:      1;  
        unsigned int reserved:            29;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
    } Bits;
    volatile unsigned int Register;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union _ath_mbox_slic_int {
    struct __ath_mbox_int_status {
#ifdef __BIG_ENDIAN
        unsigned int reserved:                       25;
        volatile unsigned int rx_dma_complete:        1;  
        volatile unsigned int tx_dma_eom_complete:    1;   
        volatile unsigned int tx_dma_complete:        1;    
        volatile unsigned int tx_overflow:            1;    
        volatile unsigned int rx_underflow:           1;    
        volatile unsigned int tx_not_empty:           1;
        volatile unsigned int rx_not_full:            1;
#else/*--- #ifdef __BIG_ENDIAN ---*/
        volatile unsigned int rx_not_full:            1;
        volatile unsigned int tx_not_empty:           1;
        volatile unsigned int rx_underflow:           1;    
        volatile unsigned int tx_overflow:            1;    
        volatile unsigned int tx_dma_complete:        1;    
        volatile unsigned int tx_dma_eom_complete:    1;   
        volatile unsigned int rx_dma_complete:        1;  
        unsigned int reserved:            25;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
    } Bits;
    volatile unsigned int Register;
};
/*--------------------------------------------------------------------------------*\
 * Note: in case of status the higher bit of rx_dma_complete etc. is reserved (zero)
\*--------------------------------------------------------------------------------*/
union _ath_mbox_int {
    struct __ath_mbox_int {
#ifdef __BIG_ENDIAN
        unsigned int reserved:                       20;
        volatile unsigned int rx_dma_complete:        2;    /*--- mbox0/mbox1 for enable  ---*/
        volatile unsigned int tx_dma_eom_complete:    2;   
        volatile unsigned int tx_dma_complete:        2;    
        volatile unsigned int tx_overflow:            1;    
        volatile unsigned int rx_underflow:           1;    
        volatile unsigned int tx_not_empty:           2;
        volatile unsigned int rx_not_full:            2;
#else/*--- #ifdef __BIG_ENDIAN ---*/
        volatile unsigned int rx_not_full:            2;
        volatile unsigned int tx_not_empty:           2;
        volatile unsigned int rx_underflow:           1;    
        volatile unsigned int tx_overflow:            1;    
        volatile unsigned int tx_dma_complete:        2;    
        volatile unsigned int tx_dma_eom_complete:    2;   
        volatile unsigned int rx_dma_complete:        2;  
        unsigned int reserved:                       20;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
    } Bits;
    volatile unsigned int Register;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _ath_mbox_dma_descriptor {
    union _ath_mbox_dma_descriptor_ctrl {
        struct __ath_mbox_dma_descriptor_ctrl {
#ifdef __BIG_ENDIAN
            volatile unsigned int OWN:                 1;    /*--- if set owned by dma ---*/
            volatile unsigned int EOM:                 1;    /*--- End of Message Indicator ---*/
            volatile unsigned int Rsvd:                5;
            volatile unsigned int VUC:                 1;    /*--- VUC Data for audio block---*/
            volatile unsigned int Size:               12;
            volatile unsigned int Length:             12;
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int Length:             12;
            volatile unsigned int Size:               12;
            volatile unsigned int VUC:                 1;    /*--- VUC Data for audio block---*/
            volatile unsigned int Rsvd:                5;
            volatile unsigned int EOM:                 1;    /*--- End of Message Indicator ---*/
            volatile unsigned int OWN:                 1;    /*--- if set owned by dma ---*/
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } Ctrl;
    volatile void *bufptr;
    volatile struct _ath_mbox_dma_descriptor *next;
    unsigned int pad; /*--- aligned descriptor auf 16 Byte - unbedingt notwendig!!! ---*/
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _ath_mbox_base {
    volatile unsigned int fifo_dummy1;                                      /*--- 0x00          ---*/   
    volatile unsigned int fifo_dummy2;                                      /*--- 0x04          ---*/   
    union _ath_mbox_fifo_status fifo_status;                                /*--- 0x08          ---*/   
    union _ath_mbox_slic_fifo_status slic_fifo_status;                      /*--- 0x0C          ---*/   
    union _ath_mbox_dma_policy dma_policy;                                  /*--- 0x10          ---*/   
    union _ath_mbox_dma_policy slic_dma_policy;                             /*--- 0x14          ---*/   
    volatile struct _ath_mbox_dma_descriptor *dma_rx_descriptor_base;       /*--- 0x18 Bit 2-27 ---*/
    union _ath_mbox_dma_control dma_rx_control;                             /*--- 0x1C          ---*/
    volatile struct _ath_mbox_dma_descriptor *dma_tx_descriptor_base;       /*--- 0x20 Bit 2-27 ---*/
    union _ath_mbox_dma_control dma_tx_control;                             /*--- 0x24          ---*/
    volatile struct _ath_mbox_dma_descriptor *slic_dma_rx_descriptor_base;  /*--- 0x28 Bit 2-27 ---*/
    union _ath_mbox_dma_control slic_dma_rx_control;                        /*--- 0x2C          ---*/   
    volatile struct _ath_mbox_dma_descriptor *slic_dma_tx_descriptor_base;  /*--- 0x30 Bit 2-27 ---*/
    union _ath_mbox_dma_control slic_dma_tx_control;                        /*--- 0x34          ---*/   
    union _ath_mbox_frame mbox_frame;                                       /*--- 0x38          ---*/   
    union _ath_mbox_slic_frame slic_mbox_frame;                             /*--- 0x3C          ---*/   
    union _ath_mbox_fifo_timeout {                                          /*--- 0x40          ---*/   
        struct __ath_mbox_fifo_timeout {
#ifdef __BIG_ENDIAN
            unsigned int reserved:            23;
            volatile unsigned int enable:      1;  
            volatile unsigned int value:       8;       /*--- msec  ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int value:       8;       /*--- msec  ---*/
            volatile unsigned int enable:      1;  
            unsigned int reserved:            23;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } fifo_timeout;
    union _ath_mbox_int mbox_int_status;                                /*--- 0x44 read / write-1-clear ---*/
    union _ath_mbox_slic_int slic_mbox_int_status;                      /*--- 0x48 read / write-1-clear ---*/
    union _ath_mbox_int mbox_int_enable;                                /*--- 0x4C          ---*/            
    union _ath_mbox_slic_int slic_mbox_int_enable;                      /*--- 0x50          ---*/   
    unsigned int Reserved;
#define ATH_MBOX_FIFO_RESET_RX(box)             (((box) & 0x3) << 2)
#define ATH_MBOX_FIFO_RESET_TX(box)             (((box) & 0x3) << 0)
    union _ath_mbox_fifo_reset {                                        /*--- 0x58          ---*/   
        struct __ath_mbox_fifo_reset {
#ifdef __BIG_ENDIAN
            unsigned int reserved:                       28;
            volatile unsigned int rx_init:                2;    /*--- mbox0/mbox1 - writing 1 to reset ---*/
            volatile unsigned int tx_init:                2;    /*--- mbox0/mbox1 - writing 1 to reset ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int tx_init:                2;    /*--- mbox0/mbox1 - writing 1 to reset ---*/
            volatile unsigned int rx_init:                2;    /*--- mbox0/mbox1 - writing 1 to reset ---*/
            unsigned int reserved:                       28;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } fifo_reset;
    union _ath_mbox_slic_fifo_reset {                                   /*--- 0x5C          ---*/   
        struct __ath_mbox_slic_fifo_reset {
#define ATH_SLIC_MBOX_FIFO_RESET_RX             (1 << 1)
#define ATH_SLIC_MBOX_FIFO_RESET_TX             (1 << 0)
#ifdef __BIG_ENDIAN
            unsigned int reserved:                       30;
            volatile unsigned int rx_init:                1;    /*--- writing 1 to reset ---*/
            volatile unsigned int tx_init:                1;    /*--- writing 1 to reset ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int tx_init:                1;    /*--- writing 1 to reset ---*/
            volatile unsigned int rx_init:                1;    /*--- writing 1 to reset ---*/
            unsigned int reserved:                       30;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } slic_fifo_reset;
};

#define M_ATH_ADDR(reg)        (&pm->reg.Register)
#define M_ATH_FIELD(reg, bit1) pm->reg.Bits.bit1
#define M_ATH_FIELD2(reg, bit1,bit2) pm->reg.Bits.bit1, pm->reg.Bits.bit2
#define M_ATH_FIELD7(reg, bit1,bit2,bit3,bit4,bit5,bit6, bit7) pm->reg.Bits.bit1, pm->reg.Bits.bit2, pm->reg.Bits.bit3, \
                                                         pm->reg.Bits.bit4, pm->reg.Bits.bit5, pm->reg.Bits.bit6, \
                                                         pm->reg.Bits.bit7

/*--------------------------------------------------------------------------------*\
 * verodert mask: 1 mbox0/1
 * verodert mask: 2 slic_mbox
\*--------------------------------------------------------------------------------*/
static inline void print_ath_mbox_register(const char *prefix, unsigned int mask) {
    struct _ath_mbox_base *pm = (struct _ath_mbox_base *)KSEG1ADDR(ATH_DMA_BASE);
    if(mask & 0x1) {
        printk("%s\n"
           "%p FIFO_STATUS      %08x empty: %x full: %x\n"
           "%p DMA_POLICY       %08x tresh0 %x tx/rx_end_swap %x/%x tx/rx_16bit_swap %x/%x\n"
           "%p RX/TX_DESCRIPTOR %p/%p\n"
           "%p RX/TX_CONTROL    %08x/%08x resume %x/%x start %x/%x stop %x/%x\n"
           "%p MBOX_FRAME       %08x rx_eom %x rx_som %x\n"     
           "%p MBOX_INT_STATUS  %08x rx_dma %x tx_dma_eom %x tx_dma %x tx_ovr %x rx_udr %x tx_not_empty %x rx_not_full %x\n"     
           "%p MBOX_INT_ENABLE  %08x rx_dma %x tx_dma_eom %x tx_dma %x tx_ovr %x rx_udr %x tx_not_empty %x rx_not_full %x\n"     
               ,prefix ? prefix : "",
                M_ATH_ADDR(fifo_status), *M_ATH_ADDR(fifo_status), M_ATH_FIELD2(fifo_status, empty, full),
                M_ATH_ADDR(dma_policy), *M_ATH_ADDR(dma_policy), M_ATH_FIELD(dma_policy, tx_fifo_tresh0),
                M_ATH_FIELD2(dma_policy, tx_end_swap, rx_end_swap),
                M_ATH_FIELD2(dma_policy, tx_16bit_swap, rx_16bit_swap),
                &pm->dma_rx_descriptor_base,
                pm->dma_rx_descriptor_base, pm->dma_tx_descriptor_base,
                M_ATH_ADDR(dma_rx_control), *M_ATH_ADDR(dma_rx_control), *M_ATH_ADDR(dma_tx_control),
                M_ATH_FIELD(dma_rx_control, resume), M_ATH_FIELD(dma_tx_control, resume),
                M_ATH_FIELD(dma_rx_control, start),  M_ATH_FIELD(dma_tx_control, start),
                M_ATH_FIELD(dma_rx_control, stop),   M_ATH_FIELD(dma_tx_control, stop),
                M_ATH_ADDR(mbox_frame), *M_ATH_ADDR(mbox_frame), 
                M_ATH_FIELD2(mbox_frame, rx_eom, rx_som),
                M_ATH_ADDR(mbox_int_status), *M_ATH_ADDR(mbox_int_status),
                M_ATH_FIELD7(mbox_int_status, rx_dma_complete,tx_dma_eom_complete, tx_dma_complete, tx_overflow, rx_underflow,    tx_not_empty, rx_not_full),
                M_ATH_ADDR(mbox_int_enable), *M_ATH_ADDR(mbox_int_enable),
                M_ATH_FIELD7(mbox_int_enable, rx_dma_complete,tx_dma_eom_complete, tx_dma_complete, tx_overflow, rx_underflow,    tx_not_empty, rx_not_full)
            );
    }
    if(mask & 0x2) {
        printk("%s\n"
           "%p SLIC_FIFO_STATUS      %08x empty: %x full: %x\n"
           "%p SLIC_DMA_POLICY       %08x tresh0 %x tx/rx_end_swap %x/%x tx/rx_16bit_swap %x/%x\n"
           "%p SLIC_RX/TX_DESCRIPTOR %p/%p\n"
           "%p SLIC_RX/TX_CONTROL    %08x/%08x resume %x/%x start %x/%x stop %x/%x\n"
           "%p SLIC_MBOX_FRAME       %08x rx_eom %x rx_som %x\n"     
           "%p SLIC_MBOX_INT_STATUS  %08x rx_dma %x tx_dma_eom %x tx_dma %x tx_ovr %x rx_udr %x tx_not_empty %x rx_not_full %x\n"     
           "%p SLIC_MBOX_INT_ENABLE  %08x rx_dma %x tx_dma_eom %x tx_dma %x tx_ovr %x rx_udr %x tx_not_empty %x rx_not_full %x\n"     
               ,prefix ? ((mask & 0x1) ? "" : prefix) : "",
                M_ATH_ADDR(slic_fifo_status), *M_ATH_ADDR(slic_fifo_status),
                M_ATH_FIELD2(slic_fifo_status,empty,full),
                M_ATH_ADDR(slic_dma_policy), *M_ATH_ADDR(slic_dma_policy), M_ATH_FIELD(slic_dma_policy, tx_fifo_tresh0),
                M_ATH_FIELD2(slic_dma_policy, tx_end_swap, rx_end_swap),
                M_ATH_FIELD2(slic_dma_policy, tx_16bit_swap, rx_16bit_swap),
                &pm->slic_dma_rx_descriptor_base,
                pm->slic_dma_rx_descriptor_base, pm->slic_dma_tx_descriptor_base,
                M_ATH_ADDR(slic_dma_rx_control), *M_ATH_ADDR(slic_dma_rx_control), *M_ATH_ADDR(slic_dma_tx_control),
                M_ATH_FIELD(slic_dma_rx_control, resume), M_ATH_FIELD(slic_dma_tx_control, resume),
                M_ATH_FIELD(slic_dma_rx_control, start),  M_ATH_FIELD(slic_dma_tx_control, start),
                M_ATH_FIELD(slic_dma_rx_control, stop),   M_ATH_FIELD(slic_dma_tx_control, stop),
                M_ATH_ADDR(slic_mbox_frame), *M_ATH_ADDR(slic_mbox_frame), 
                M_ATH_FIELD2(slic_mbox_frame, rx_eom, rx_som),
                M_ATH_ADDR(slic_mbox_int_status), *M_ATH_ADDR(slic_mbox_int_status),
                M_ATH_FIELD7(slic_mbox_int_status, rx_dma_complete,tx_dma_eom_complete, tx_dma_complete, tx_overflow, rx_underflow,    tx_not_empty, rx_not_full),
                M_ATH_ADDR(slic_mbox_int_enable), *M_ATH_ADDR(slic_mbox_int_enable),
                M_ATH_FIELD7(slic_mbox_int_enable, rx_dma_complete,tx_dma_eom_complete, tx_dma_complete, tx_overflow, rx_underflow,    tx_not_empty, rx_not_full)
            );
    }
    printk("%p FIFO_TIMEOUT          %08x enable %x value 0x%x\n", M_ATH_ADDR(fifo_timeout), *M_ATH_ADDR(fifo_timeout), M_ATH_FIELD2(fifo_timeout,enable,value)
          );
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void print_dma_descriptor(const char *prefix, struct _ath_mbox_dma_descriptor *pd) {
        printk("%s[%p = %x] OWN %x EOM %x VUC %x Size %d Length %d bufptr %p next %p\n"
               ,prefix ? prefix : "",
               pd, pd->Ctrl.Register, pd->Ctrl.Bits.OWN, pd->Ctrl.Bits.EOM, pd->Ctrl.Bits.VUC, pd->Ctrl.Bits.Size, pd->Ctrl.Bits.Length, pd->bufptr, pd->next);
}
#endif/*--- #ifndef __atheros_mbox__ ---*/
