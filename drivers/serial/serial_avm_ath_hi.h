
#ifndef SERIAL_AVM_ATH_HI_H
#define SERIAL_AVM_ATH_HI_H

void uart_avm_post_setup_ports(void);

union avm_ath_hi_regs_data {
    struct _avm_ath_hi_regs_data {
        volatile unsigned int reserved : 22;
        volatile unsigned int tx_csr : 1;
        volatile unsigned int rx_csr : 1;
        volatile unsigned int tx_rx_data : 8;
    } Bits;
    volatile unsigned int Register;
};


union avm_ath_hi_regs_config {
    struct _avm_ath_hi_regs_config {
        unsigned int reserved1 : 16;     /*--- 31:16  ---*/
        volatile unsigned int rx_busy : 1;        /*---  15   ---*/
        volatile unsigned int tx_busy : 1;        /*---  14   ---*/
        volatile unsigned int irq_en : 1;         /*---  13   ---*/
        volatile unsigned int irq_status : 1;     /*---  12   ---*/
        volatile unsigned int tx_break : 1;       /*---  11   ---*/
        volatile unsigned int rx_break : 1;       /*---  10   ---*/
        volatile unsigned int tx_ready : 1;       /*---   9   ---*/
        volatile unsigned int tx_ready_oride : 1; /*---   8   ---*/
        volatile unsigned int rx_ready_oride : 1; /*---   7   ---*/
        unsigned int reserved2 : 1;      /*---   6   ---*/
        volatile unsigned int flow_control : 2;   /*--- 5:4   ---*/
        volatile unsigned int interface_mode : 2; /*--- 3:2   ---*/
        volatile unsigned int parity_mode : 2;    /*--- 1:0   ---*/
    } Bits;
    volatile unsigned int Register;
};

union avm_ath_hi_regs_clock {
    struct _avm_ath_hi_regs_clock {
        unsigned int reserved : 8;
        volatile unsigned int clock_scale : 8;
        volatile unsigned int clock_step  : 16;
    } Bits;
    volatile unsigned int Register;
};

union avm_ath_hi_regs_irq_status {
    struct _avm_ath_hi_regs_irq_status {
        unsigned int reserved1 : 22;     /*--- 31:10  ---*/
        volatile unsigned int tx_empty : 1;     /*--- 9  ---*/
        volatile unsigned int rx_full : 1;     /*--- 8  ---*/
        volatile unsigned int rx_break_off : 1;     /*--- 7  ---*/
        volatile unsigned int rx_break_on : 1;     /*--- 6  ---*/
        volatile unsigned int rx_parity_err : 1;     /*--- 5  ---*/
        volatile unsigned int tx_overflow_err : 1;     /*--- 4  ---*/
        volatile unsigned int rx_overflow_err : 1;     /*---  3 ---*/
        volatile unsigned int rx_framing_err : 1;     /*--- 2  ---*/
        volatile unsigned int tx_ready_int : 1;     /*--- 1  ---*/
        volatile unsigned int rx_valid_int : 1;     /*--- 0  ---*/
    } Bits;
    volatile unsigned int Register;
};

struct avm_ath_hi_regs {
    volatile union avm_ath_hi_regs_data data;   /*--- offset 0x00 ---*/
    volatile union avm_ath_hi_regs_config config;   /*--- offset 0x04 ---*/
    volatile union avm_ath_hi_regs_clock clock;   /*--- offset 0x08 ---*/
    volatile union avm_ath_hi_regs_irq_status status;   /*--- offset 0x0C ---*/
    volatile union avm_ath_hi_regs_irq_status enable;   /*--- offset 0x10 ---*/
};

#endif
