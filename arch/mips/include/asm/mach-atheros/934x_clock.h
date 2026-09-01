#ifndef _934x_clock_h_
#define _934x_clock_h_


enum usb_refclk_freq_sel_value {
    usb_refclk_freq_sel_0_reserved,
    usb_refclk_freq_sel_1_reserved,
    usb_refclk_freq_sel_25MHz,
    usb_refclk_freq_sel_3_reserved,
    usb_refclk_freq_sel_4_reserved,
    usb_refclk_freq_sel_40MHz,
    usb_refclk_freq_sel_6_reserved,
    usb_refclk_freq_sel_7_reserved
};

union _934x_switch_clock_source_control {
    struct __934x_switch_clock_source_control {
        unsigned int reserved1 : 20;
        volatile enum usb_refclk_freq_sel_value usb_refclk_freq_sel : 4;
        volatile unsigned int uart1_clk_sel : 1;
        volatile unsigned int mdio_clk_sel : 1;
        volatile unsigned int oen_clk_125M_pll : 1;
        volatile unsigned int en_pll_top : 1;
        volatile unsigned int ew_enable : 1;
        volatile unsigned int switch_clk_off : 1;
        unsigned int reserved2 : 1;
        volatile unsigned int switch_clk_sel : 1;
    } Bits;
    volatile unsigned int Register;
} __attribute__ ((packed));




#endif /*--- #ifndef _934x_clock_h_ ---*/
