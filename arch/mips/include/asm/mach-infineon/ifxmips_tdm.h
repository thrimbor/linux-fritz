/*--------------------------------------------------------------------------------*\
 *   Copyright (C) 2010 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
\*--------------------------------------------------------------------------------*/

#ifndef _ifxmipstdm_h__
#define _ifxmipstdm_h__

/*--- #include <ifx_types.h> ---*/
/*--- #include <ifx_regs.h> ---*/
#define TDM_BASE_ADDR                   (KSEG1 | 0x1F100000)
struct _tdm_interface {
    /*--------------------------------------------------------------------------------*\
     PCM Receiver Control Register
        Res0  31:16 rw   Reserved
                         Returns 0 upon read; must be written with 0
        RR    15:0  rw   PCM Channel x Receiver Ready Interrupt Enable
                         0B   Dis, Interrupt disabled
                         1B   En, Interrupt enabled
    \*--------------------------------------------------------------------------------*/
    volatile unsigned int pcm_ctrlr;
    /*--------------------------------------------------------------------------------*\
    PCM Transmitter Control Register
        XR 31:16 rw PCM Channel x Transmitter Ready Interrupt Enable
                    0B  Dis, Interrupt Disabled
                    1B  En, Interrupt Enabled
        XE 15:0  rw PCM Channel x Transmitter Empty Interrupt Enable
                    0B  Dis, Interrupt Disabled
                        En, Interrupt Enabled
    \*--------------------------------------------------------------------------------*/
    volatile unsigned int pcm_ctrlx;
    /*--------------------------------------------------------------------------------*\
    PCM Receiver Status Register
    Res0 31:16 r Reserved 0
                 Returns 0 upon read; must be written with 0.
    RR   15:0  r PCM Channel x Receiver Ready Flags
                 These bits are also connected to port “rr_dma_req”
                 0B   NRdy, Not ready (receive state: “empty”)
                 1B   Rdy, Ready (receive state: “buf_full” or “fifo_full” or ds_buf_full)
    \*--------------------------------------------------------------------------------*/
    volatile unsigned int pcm_statr;
    /*--------------------------------------------------------------------------------*\
    PCM Transmitter Status Register
    XR 31:16 r PCM Channel x Transmitter Ready Flags
               These bits are also connected to port “xr_dma_req”
               0B   NRdy, Not ready (transmit state: “fifo_full” or “ds_buf_full”)
               1B   Rdy, Ready (transmit state: “empty” or “buf_full”)
    XE 15:0  r PCM Channel x Transmitter Empty Flags
               0B   NEmpty, Not empty (transmit state: “buf_full” or “fifo_full” or
                    “ds_buf_full”)
               1B   Empty, Empty (transmit state: “empty”)
    \*--------------------------------------------------------------------------------*/
    volatile unsigned int pcm_statx;
    /*--------------------------------------------------------------------------------*\
    PCM Frequency Configuration Register
    DCL_FREQ 4:0 rw DCL Frequency Configuration
                    This bitfield configures the DCL frequency of the PCM module in case
                    FREQ_EN is set to ´1´. see Table 1
    FREQ_EN  5   rw Frequency Configuration enable
                    These bit enables setting of the DCL frequency of the PCM interface by
                    the register bits DCL_FREQ
                    0B    DCL_FREQ, DCL frequency of the PCM interface configured by the
                          frequency detection module, see Figure 4
                    1B    DCL_REG, DCL frequency of the PCM interface configured by
                          register bits DCL_FREQ
    \*--------------------------------------------------------------------------------*/
    union __pcm_freq {
        struct _pcm_freq {
#ifdef CONFIG_CPU_LITTLE_ENDIAN
            volatile unsigned int dcl_freq:5;
            volatile unsigned int freq_en:1;
            volatile unsigned int reserved:26;
#else /*--- #ifdef CONFIG_CPU_LITTLE_ENDIAN ---*/
            volatile unsigned int reserved:26;
            volatile unsigned int freq_en:1;
            volatile unsigned int dcl_freq:5;
#endif/*--- #else  ---*//*--- #ifdef CONFIG_CPU_LITTLE_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } pcm_freq;
    /*--------------------------------------------------------------------------------*\
    PCM Common Configuration Register
    EN      15   rw PCM Interface Enable
                    0B   Deactive, Both Highways are deactivated automatically.
                    1B   Interface active, time slot channels are enabled regarding PCM Time slot Channel Register PCM_EN.
    DS      14   rw PCM Interface Enable
                    0B   DLY, The double buffer of each channel is optimized for minimal group delay.
                    1B   DATA, The double buffer of each channel is optimized for data
    RES0    13   rw Reserved
                    Returns 0 upon read; must be written with 0.
    AC97_WM 12   rw AC 97 I//F Warm Reset
                    0B   No_RST, No reset
                    1B   RST, Reset
    AC97_CD 11   rw AC 97 I/F Cold Reset
                    0B   No_RST, No reset
                    1B   RST, Reset
    PCMXO   10:8 rw PCM Transmitter Offset
                    0-7B Offset, Offset (to the right) of the transmit time slot-assignment relative to the FSC (number of data periods).
    DCK     7    rw PCM Clocking
                    0B   Sng_Clk, Single clocking
                    1B   Dbl_Clk, Double clocking
    XS      6    rw PCM transmit slope
                    0B   RSNG_EDG, Transmission starts with rising edge of clock
                    1B   FLLG_EDG, Transmission starts with falling edge of clock
    RS      5    rw PCM receive slope
                    0B   FLLG_EDG, Data is sampled at falling edge of clock
                         RSNG_EDG, Data is sampled at rising edge of clock
    ND      4    rw No Drive
                    Available for DCK = 0 only
                    0B    Ent_Clk, Bit 0 is driven during the entire clock period
                    1B    Hlf_Clk, Bit 0 is driven during the first half of the clock period
    SF      3     rw Shift Control
                    Available for DCK = 1 only
                    0B    No_Shft, No shift
                    1B    Shft, Shift (of receive and transmit time slot) by one clock period to the left.
    PCMRO 2:0     rw PCM Receiver Offset
                    0-7B Offset, Offset (to the right) of the receive time slot-assignment relative to the FSC (number of data periods)
    \*--------------------------------------------------------------------------------*/
    union _pcm_cfg {
        struct __pcm_cfg {
#ifdef CONFIG_CPU_LITTLE_ENDIAN
            volatile unsigned int pcmro:3;
            volatile unsigned int sf:1;
            volatile unsigned int nd:1;
            volatile unsigned int rs:1;
            volatile unsigned int xs:1;
            volatile unsigned int dck:1;
            volatile unsigned int pcmxo:3;
            volatile unsigned int ac97_cd:1;
            volatile unsigned int ac97_wm:1;
            volatile unsigned int reserved1:1;
            volatile unsigned int ds:1;
            volatile unsigned int en:1;
            volatile unsigned int reserved:16;
#else /*--- #ifdef CONFIG_CPU_LITTLE_ENDIAN ---*/
            volatile unsigned int reserved:16;
            volatile unsigned int en:1;
            volatile unsigned int ds:1;
            volatile unsigned int reserved1:1;
            volatile unsigned int ac97_wm:1;
            volatile unsigned int ac97_cd:1;
            volatile unsigned int pcmxo:3;
            volatile unsigned int dck:1;
            volatile unsigned int xs:1;
            volatile unsigned int rs:1;
            volatile unsigned int nd:1;
            volatile unsigned int sf:1;
            volatile unsigned int pcmro:3;
#endif/*--- #else  ---*//*--- #ifdef CONFIG_CPU_LITTLE_ENDIAN ---*/
        } Bits;
        volatile unsigned int Register;
    } pcm_cfg;
    /*--------------------------------------------------------------------------------*\
    PCM Enable Register
    Res0 31:16 rw Reserved 0
                  Returns 0 upon read;must be written with 0.
    EN   15:0  rw PCM Channel x Time slot Enable
                  0B   DIS, Time slot-Channel x is disabled
                  1B   En, Time slot-Channel x is enabled
    \*--------------------------------------------------------------------------------*/
    volatile unsigned int pcm_en;
    /*--------------------------------------------------------------------------------*\
    PCM Test Register
    Res0 31:16 rw Reserved
                  Returns 0 upon read;must be written with 0
    CHLE 15:0  rw PCM channel x loop back enable
                  0B   Nrml_Opn, Normal Operation
                  1B   Loop_Bk, Loop back data in receive-FIFOx to transmit FIFOx
    \*--------------------------------------------------------------------------------*/
    volatile unsigned int pcm_test;
    /*--------------------------------------------------------------------------------*\
    PCM Mode Register
    MD0-15 0-15 rw PCM Mode for Channel0-15
           0B  8-Bit, Channel works in 8-bit mode.
           1B  16-Bit, Channel works in 16-bit linear mode.

    \*--------------------------------------------------------------------------------*/
    volatile unsigned int pcm_mode;
    /*--------------------------------------------------------------------------------*\
    PCM Status Register
    CRASHB 15   sc PCM CHRASH highway B
                   Indication of multiple access of PCM transmit time slot of highway B. This
                   bit is set by the hardware and can be acknowledged if the corresponding
                   bit is written with a logical one.
                   0B      Nrml_Mode, Normal mode
                   1B      Mlt_Acc, PCM tx time slot of highway B is accessed multiple
    CRASHA 14   sc PCM CHRASH highway A
                   Indication of multiple access of PCM transmit time slot of highway A. This
                   bit is set by the hardware and can be acknowledged if the corresponding
                   bit is written with a logical one
                   0B      Nrml_Mode, Normal mode
                   1B      Mlt_Acc, PCM tx time slot of highway A is accessed multiple
    Res0   13:4 rw Reserved 0
                   Returns 0 upon read; must be written with 0.
    PCMCLK 3:0  r  PCM clock
                   This field returns the frequency of the pclk clock. This field is equal to the
                   pclk_freq port on the pcm entity
    \*--------------------------------------------------------------------------------*/
    union __pcm_stat {
        volatile unsigned int Register;
        struct _pcm_stat {
#ifdef CONFIG_CPU_LITTLE_ENDIAN
            volatile unsigned int pcmclk:4;
            volatile unsigned int reserved1:10;
            volatile unsigned int crasha:1;
            volatile unsigned int crashb:1;
            volatile unsigned int reserved:16;
#else /*--- #ifdef CONFIG_CPU_LITTLE_ENDIAN ---*/
            volatile unsigned int reserved:16;
            volatile unsigned int crashb:1;
            volatile unsigned int crasha:1;
            volatile unsigned int reserved1:10;
            volatile unsigned int pcmclk:4;
#endif/*--- #else  ---*//*--- #ifdef CONFIG_CPU_LITTLE_ENDIAN ---*/
        } Bits;
    } pcm_stat;
    /*--------------------------------------------------------------------------------*\
     PCM Configuration channe
        RH  24    rw RX Highway Select
                     0B    PCM1, PCM Highway 1
                     1B    PCM2, PCM Highway 2
        RTS 23:16 rw Receive Time slot-Channel x Assignment
                     0 <= RTS <= fPCLK/(fFSC*8)-1 for single clocking
                     0 <= RTS <= fPCLK/(fFSC*16)-1 for double clocking
                     In 16-bit PCM mode 2 consecutive time slots must be used.
        XH  8     rw TX Highway Select
                     0B    PCM1, PCM Highway 1
                     1B    PCM2, PCM Highway 2
        XTS 7:0   rw Transmit Time slot-Channel x Assignment
                     0<= XTS <= fPCLK/(fFSC*8)-1 for single clocking
                     0<= XTS <= fPCLK/(fFSC*16)-1 for double clocking
    \*--------------------------------------------------------------------------------*/
    union _pcm_cfgchan {
        volatile unsigned int Register;
        struct __pcm_cfgchan {
#ifdef CONFIG_CPU_LITTLE_ENDIAN
            volatile unsigned int xts:8;
            volatile unsigned int xh:1;
            volatile unsigned int reserved1:7;
            volatile unsigned int rts:8;
            volatile unsigned int rh:1;
            volatile unsigned int reserved:7;
#else /*--- #ifdef CONFIG_CPU_LITTLE_ENDIAN ---*/
            volatile unsigned int reserved:7;
            volatile unsigned int rh:1;
            volatile unsigned int rts:8;
            volatile unsigned int reserved1:7;
            volatile unsigned int xh:1;
            volatile unsigned int xts:8;
#endif/*--- #else  ---*//*--- #ifdef CONFIG_CPU_LITTLE_ENDIAN ---*/
        } Bits;
    } pcm_cfgchan[16];
#if defined(CONFIG_AR10)
    unsigned int Reserved[6];
#endif/*--- #if defined(CONFIG_AR10) ---*/
    /*--------------------------------------------------------------------------------*\
    PCM Data Register
    PDATA 15:0 rw PCM Receive/Transmit Data
              A write to.the PCM_DATAx inserts the Data into the 2-stage transmit
              register for channel x (Register tx_data_reg1 - Figure 1).
              A read from the PCM_DATAx reads the data from the 2-stage receive
              register of channel x (Register rx_data_reg2 - Figure 2)
              In case of 8-bit PCM(MDX=0) the data is right aligned and zero-extended.
              A register underrun condition caused by a read returns the data last
              written into the register.
              In case of 16-bit PCM(MDX=1) PDATA(15:8) contains the data of time
              slot k and PDATA(7:0) contains the data of time slot k+1.
    \*--------------------------------------------------------------------------------*/
    volatile unsigned int pcm_data[16];
};

#endif/*--- #ifndef _ifxmipstdm_h__ ---*/
