#ifndef __atheros_slic__
#define __atheros_slic__

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _ath_slic_base {
    volatile unsigned int slic_slot;          /*--- Bit 0:6 Number of slots ---*/
    volatile unsigned int slic_clock_control; /*--- Bit 0:5 Divider of AUDIO_PLL_CLOCK ---*/
    union _slic_ctrl {
        struct __slic_ctrl {
#ifdef __BIG_ENDIAN
            unsigned int reserved2:            28;
            volatile unsigned int clk_en:       1;
            volatile unsigned int master_slave: 1; /*--- 0: slave mode ---*/
            volatile unsigned int slic_en:      1;
            unsigned int reserved:              1;
#else/*--- #ifdef __BIG_ENDIAN ---*/
            unsigned int reserved:              1;
            volatile unsigned int slic_en:      1;
            volatile unsigned int master_slave: 1; /*--- 0: slave mode ---*/
            volatile unsigned int clk_en:       1;
            unsigned int reserved2:            28;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } slic_ctrl;
    volatile unsigned int slic_tx_slots1;
    volatile unsigned int slic_tx_slots2;
    volatile unsigned int slic_rx_slots1;
    volatile unsigned int slic_rx_slots2;
/*--------------------------------------------------------------------------------*\
 * txdata_fs_sync_ext + txdata_fs_sync: 
 * 0 Tx data will be sent at the first posedge after fs
 * 1 Tx data will be sent at the first negedge after fs
 * 2 Tx data will be sent at the second posedge after fs
 * 3 Tx data will be sent at the second negedge after fs
 * 4 Tx data will be sent at the third posedge after fs
 * 5 Tx data will be sent at the third negedge after fs
 *
 * rxdata_sample_pos_ext + rxdata_sample_pos: 
 * 0 Rx data will be sampled at the second posedge after fs
 * 1 Rx data will be sampled at the second negedge after fs
 * 2 Rx data will be sampled at the third posedge after fs
 * 3 Rx data will be sampled at the third negedge after fs
 * 4 Rx data will be sampled at the fourth posedge after fs
 * 5 Rx data will be sampled at the first posedge after fs
\*--------------------------------------------------------------------------------*/
    union _slic_timing_ctrl {
        struct __slic_timing_ctrl {
#ifdef __BIG_ENDIAN
            unsigned int reserved:                  20;
            volatile unsigned int rxdata_sample_pos_ext:1; /*--- Bit 3 correspondend with rxdata_sample_pos ---*/
            volatile unsigned int txdata_fs_sync_ext:1; /*--- Bit 3 correspondend with txdata_fs_sync ---*/
            volatile unsigned int dataoen_always:    1; /*--- 0: the DATA_OEN is present for enabled slots, 1: the DATA_OEN is high for all slots ---*/
            volatile unsigned int rxdata_sample_pos: 2; /*--- control when data will be sampled (see upper)---*/
            volatile unsigned int txdata_fs_sync:    2; /*--- control when data will be sampled (see upper) ---*/
            volatile unsigned int long_fsclks:       3; /*--- specify number of clks for wich is fs high ---*/
            volatile unsigned int fs_pos:            1; /*--- 0: Send FS at the negative edge of CLK, 1: Send FS at the postive edge of CLK ---*/
            volatile unsigned int long_fs:           1; /*--- 0: Fs is high for a half bit clock, 1: FS is high for more than 1 BIT_CLK ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int long_fs:           1; /*--- 0: Fs is high for a half bit clock, 1: FS is high for more than 1 BIT_CLK ---*/
            volatile unsigned int fs_pos:            1; /*--- 0: Send FS at the negative edge of CLK, 1: Send FS at the postive edge of CLK ---*/
            volatile unsigned int long_fsclks:       3; /*--- specify number of clks for wich is fs high ---*/
            volatile unsigned int txdata_fs_sync:    2; /*--- control when data will be sampled ---*/
            volatile unsigned int rxdata_sample_pos: 2; /*--- control when data will be sampled ---*/
            volatile unsigned int dataoen_always:    1; /*--- 0: the DATA_OEN is present for enabled slots, 1: the DATA_OEN is high for all slots ---*/
            volatile unsigned int txdata_fs_sync_ext:1; /*--- Bit 3 correspondend with txdata_fs_sync ---*/
            volatile unsigned int rxdata_sample_pos_ext :1; /*--- Bit 3 correspondend with rxdata_fs_sync ---*/
            unsigned int reserved:                  20;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } slic_timing_ctrl;
    union _slic_intr {
        struct __slic_intr {
#ifdef __BIG_ENDIAN
            unsigned int reserved:                  26;
            volatile unsigned int status:            1; /*--- 0: No interrupts, 1: unexepted fs received ---*/
            volatile unsigned int reserved1:         4;
            volatile unsigned int mask:              1; /*--- 0: Indicates the unexpected Framesync interrupt is MASKED 1: Indicates the interrupt is enabled ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int mask:              1; /*--- 0: Indicates the unexpected Framesync interrupt is MASKED 1: Indicates the interrupt is enabled ---*/
            volatile unsigned int reserved1:         4;
            volatile unsigned int status:            1; /*--- 0: No interrupts, 1: unexepted fs received ---*/
            unsigned int reserved:                  26;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } slic_intr;
    union _slic_swap {
        struct __slic_swap {
#ifdef __BIG_ENDIAN
            unsigned int reserved:                  30;
            volatile unsigned int rx_data:           1; /*--- 1: swap rx bytes ---*/
            volatile unsigned int tx_data:           1; /*--- 1: swap tx bytes ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
            volatile unsigned int tx_data:           1; /*--- 1: swap tx bytes ---*/
            volatile unsigned int rx_data:           1; /*--- 1: swap rx bytes ---*/
            unsigned int reserved:                  30;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } slic_swap;
};
#endif/*--- #ifndef __atheros_slic__ ---*/
