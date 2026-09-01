/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,2007,2008,2009,2010 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifndef _XMAC_H_
#define _XMAC_H_

#define CHANNEL_MAX                 8
#define NO_BUFFER_OFFSET            0

/*--- Bits im Buffer ---*/
#define BD_SOP                  (1<<31)
#define BD_EOP                  (1<<30)
#define BD_OWNS                 (1<<29)
#define BD_EOQ                  (1<<28)
#define BD_PCRC                 (1<<26)

#define CONTROL_TX              (BD_OWNS+BD_SOP+BD_EOP)
#define CONTROL_RX              (BD_OWNS)

/*--- Offsets der Register ---*/
#define TXIDVER_OFF             0x000
#define TXCTL_OFF               0x004
#define TXTEARDOWN_OFF          0x008
#define RXCTL_OFF               0x014
#define RXTEARDOWN_OFF          0x018

#define RX_MBP_EN_OFF           0x100
#define RX_UNICAST_SET_OFF      0x104
#define RX_UNICAST_CLR_OFF      0x108
#define RXMAXLEN_OFF            0x10C
#define RXBUFFER_OFFSET_OFF     0x110
#define RXFILTERLOWTHRESH_OFF   0x114

#define MAC_CTL_OFF             0x160
#define MAC_STATUS_OFF          0x164
#define MAC_EMCTL_OFF           0x168

#define MAC_HASH1_OFF           0x1D8
#define MAC_HASH2_OFF           0x1DC
#define BOFFTEST_OFF            0x1E0
#define TRACE_TEST_OFF          0x1E4
#define RX_PAUSE_OFF            0x1E8
#define TX_PAUSE_OFF            0x1EC

#define TX_DMA_HDP_OFF          0x600
#define TX0_DMA_HDP_OFF         0x600
#define TX1_DMA_HDP_OFF         0x604
#define TX2_DMA_HDP_OFF         0x608
#define TX3_DMA_HDP_OFF         0x60C
#define TX4_DMA_HDP_OFF         0x610
#define TX5_DMA_HDP_OFF         0x614
#define TX6_DMA_HDP_OFF         0x618
#define TX7_DMA_HDP_OFF         0x61C

#define RX_DMA_HDP_OFF          0x620
#define RX0_DMA_HDP_OFF         0x620
#define RX1_DMA_HDP_OFF         0x624
#define RX2_DMA_HDP_OFF         0x628
#define RX3_DMA_HDP_OFF         0x62C
#define RX4_DMA_HDP_OFF         0x620
#define RX5_DMA_HDP_OFF         0x624
#define RX6_DMA_HDP_OFF         0x628
#define RX7_DMA_HDP_OFF         0x62C


/*--- defines for Sangam & Ohio ---*/
/*--- Register Address/bit defines. ---*/
#define CPMAC_R_TX_DMAHDP           (*(volatile unsigned *)(CPMAC_BASE + TX_DMA_HDP_OFF))
#define CPMAC_R_RX_DMAHDP           (*(volatile unsigned *)(CPMAC_BASE + RX_DMA_HDP_OFF))

#define CPMAC_R_RX_CTRL             (*(volatile unsigned *)(CPMAC_BASE + RXCTL_OFF))
#define CPMAC_R_TX_CTRL             (*(volatile unsigned *)(CPMAC_BASE + TXCTL_OFF))

#define CPMAC_R_RX_MBP              (*(volatile unsigned *)(CPMAC_BASE + RX_MBP_EN_OFF))
#define MBP_RX_MULT_CH_0            (1<<0)
#define MBP_RX_MULT_CH_1            (1<<1)
#define MBP_RX_MULT_CH_2            (1<<2)
#define MBP_RX_MULT_EN              (1<<5)
#define MBP_BROAD_CH_0              (1<<8)
#define MBP_BROAD_CH_1              (1<<9)
#define MBP_BROAD_CH_2              (1<<10)
#define MBP_BROAD_EN                (1<<13)
#define MBP_RX_PROM_CH_0            (1<<16)
#define MBP_RX_PROM_CH_1            (1<<17)
#define MBP_RX_PROM_CH_2            (1<<18)
#define MBP_CAF_EN                  (1<<21)
#define MBP_CEF_EN                  (1<<22)
#define MBP_CSF_EN                  (1<<23)
#define MBP_CMF_EN                  (1<<24)
#define MBP_RX_NO_CHAIN             (1<<28)
#define MBP_QOS_EN                  (1<<29)
#define MBP_PASS_CRC                (1<<30)

#define CPMAC_R_RXUNIS              (*(volatile unsigned *)(CPMAC_BASE + RX_UNICAST_SET_OFF))
#define CPMAC_R_RXUNIC              (*(volatile unsigned *)(CPMAC_BASE + RX_UNICAST_CLR_OFF))
#define CPMAC_R_RX_MAX_LEN          (*(volatile unsigned *)(CPMAC_BASE + RXMAXLEN_OFF))
#define CPMAC_R_RX_BO               (*(volatile unsigned *)(CPMAC_BASE + RXBUFFER_OFFSET_OFF))

#define CPMAC_R_MACCTRL             (*(volatile unsigned *)(CPMAC_BASE + MAC_CTL_OFF))
#define MAC_CTRL_DPLXM              (1<<0)
#define MAC_CTRL_LOOP               (1<<1)
#define MAC_CTRL_MTEST              (1<<2)
#define MAC_CTRL_RX_FLOW_EN         (1<<3)
#define MAC_CTRL_TX_FLOW_EN         (1<<4)
#define MAC_CTRL_MII                (1<<5)
#define MAC_CTRL_TX_PACE            (1<<6)
#define MAC_CTRL_PTYPE              (1<<9)

#define CPMAC_R_MAC_STAT            (*(volatile unsigned *)(CPMAC_BASE + MAC_STATUS_OFF))

#define CPMAC_R_BOFFTEST            (*(volatile unsigned *)(CPMAC_BASE + BOFFTEST_OFF))

#define CPMAC_R_ST_RX_GOODFRAMES    (*(volatile unsigned *)(CPMAC_BASE + 0x200))
#define CPMAC_R_ST_RX_BROADCAST     (*(volatile unsigned *)(CPMAC_BASE + 0x204))
#define CPMAC_R_ST_RX_MULTICAST     (*(volatile unsigned *)(CPMAC_BASE + 0x208))
#define CPMAC_R_ST_RX_PAUSE         (*(volatile unsigned *)(CPMAC_BASE + 0x20c))
#define CPMAC_R_ST_RX_ERR_CRC       (*(volatile unsigned *)(CPMAC_BASE + 0x210))
#define CPMAC_R_ST_RX_ERR_ALIGNC    (*(volatile unsigned *)(CPMAC_BASE + 0x214))
#define CPMAC_R_ST_RX_OVERSIZED     (*(volatile unsigned *)(CPMAC_BASE + 0x218))
#define CPMAC_R_ST_RX_JABBER        (*(volatile unsigned *)(CPMAC_BASE + 0x21c))
#define CPMAC_R_ST_RX_UNDERSIZED    (*(volatile unsigned *)(CPMAC_BASE + 0x220))
#define CPMAC_R_ST_RX_FRAGMENTS     (*(volatile unsigned *)(CPMAC_BASE + 0x224))
#define CPMAC_R_ST_RX_FILTERED      (*(volatile unsigned *)(CPMAC_BASE + 0x228))
#define CPMAC_R_ST_RX_QOSFILTERED   (*(volatile unsigned *)(CPMAC_BASE + 0x22c))
#define CPMAC_R_ST_RX_OCTETS        (*(volatile unsigned *)(CPMAC_BASE + 0x230))
#define CPMAC_R_ST_TX_GOODFRAMES    (*(volatile unsigned *)(CPMAC_BASE + 0x234))
#define CPMAC_R_ST_TX_BROADCAST     (*(volatile unsigned *)(CPMAC_BASE + 0x238))
#define CPMAC_R_ST_TX_MULTICAST     (*(volatile unsigned *)(CPMAC_BASE + 0x23c))
#define CPMAC_R_ST_TX_PAUSE         (*(volatile unsigned *)(CPMAC_BASE + 0x240))
#define CPMAC_R_ST_TX_DEFFERED      (*(volatile unsigned *)(CPMAC_BASE + 0x244))
#define CPMAC_R_ST_TX_COLLISION     (*(volatile unsigned *)(CPMAC_BASE + 0x248))
#define CPMAC_R_ST_TX_SINGLECOLL    (*(volatile unsigned *)(CPMAC_BASE + 0x24c))
#define CPMAC_R_ST_TX_MULTICOLL     (*(volatile unsigned *)(CPMAC_BASE + 0x250))
#define CPMAC_R_ST_TX_EXCESSCOLL    (*(volatile unsigned *)(CPMAC_BASE + 0x254))
#define CPMAC_R_ST_TX_LATECOLL      (*(volatile unsigned *)(CPMAC_BASE + 0x258))
#define CPMAC_R_ST_TX_UNDERRUN      (*(volatile unsigned *)(CPMAC_BASE + 0x25c))
#define CPMAC_R_ST_TX_CARRSENSEER   (*(volatile unsigned *)(CPMAC_BASE + 0x260))
#define CPMAC_R_ST_TX_OCTETS        (*(volatile unsigned *)(CPMAC_BASE + 0x264))
#define CPMAC_R_ST_TX_OC_64         (*(volatile unsigned *)(CPMAC_BASE + 0x268))
#define CPMAC_R_ST_TX_OC_65_127     (*(volatile unsigned *)(CPMAC_BASE + 0x26c))
#define CPMAC_R_ST_TX_OC_128_255    (*(volatile unsigned *)(CPMAC_BASE + 0x270))
#define CPMAC_R_ST_TX_OC_256_511    (*(volatile unsigned *)(CPMAC_BASE + 0x274))
#define CPMAC_R_ST_TX_OC_512_1023   (*(volatile unsigned *)(CPMAC_BASE + 0x278))
#define CPMAC_R_ST_TX_OC_1024_U     (*(volatile unsigned *)(CPMAC_BASE + 0x27c))
#define CPMAC_R_ST_TX_NETOCTETS     (*(volatile unsigned *)(CPMAC_BASE + 0x280))
#define CPMAC_R_ST_RX_SOF_OVRUN     (*(volatile unsigned *)(CPMAC_BASE + 0x284))
#define CPMAC_R_ST_TX_MOF_OVRUN     (*(volatile unsigned *)(CPMAC_BASE + 0x288))
#define CPMAC_R_ST_TX_DMA_OVRUN     (*(volatile unsigned *)(CPMAC_BASE + 0x28c))

/*--- Statistik ---*/
struct _cpmac_statistic_ {
    volatile unsigned int RxGoodFrames;
    volatile unsigned int RxBroadcastFrames;
    volatile unsigned int RxMulticastFrames;
    volatile unsigned int RxPauseFrames;
    volatile unsigned int RxCRCErrors;
    volatile unsigned int RxAlignCodeErrors;
    volatile unsigned int RxOversizedFrames;
    volatile unsigned int RxJabberFrames;
    volatile unsigned int RxUndersizedFrames;
    volatile unsigned int RxFragments;
    volatile unsigned int RxFilteredFrames;
    volatile unsigned int RxQOSFilteredFrames;
    volatile unsigned int RxOctets;
    volatile unsigned int TxGoodFrames;
    volatile unsigned int TxBroadcastFrames;
    volatile unsigned int TxMulticastFrames;
    volatile unsigned int TxPauseFrames;
    volatile unsigned int TxDeferredFrames;
    volatile unsigned int TxCollisionFrames;
    volatile unsigned int TxSingleCollFrames;
    volatile unsigned int TxMultiCollFrames;
    volatile unsigned int TxExcessiveCollisions;
    volatile unsigned int TxLateCollisions;
    volatile unsigned int TxUnderrun;
    volatile unsigned int TxCarrierSenseErrors;
    volatile unsigned int TxOctets;
    volatile unsigned int Tx64octetFrames;
    volatile unsigned int Tx65t127octetFrames;
    volatile unsigned int Tx128t255octetFrames;
    volatile unsigned int Tx256t511octetFrames;
    volatile unsigned int Tx512t1023octetFrames;
    volatile unsigned int Tx1024tUoctetFrames;
    volatile unsigned int NetOctets;
    volatile unsigned int RxSofOverruns;
    volatile unsigned int RxMofOverruns;
    volatile unsigned int RxDmaOverruns;
};

union _cpmac_Bofftest {
    volatile unsigned int Reg;
    volatile struct __cpmac_Bofftest {
        unsigned int tx_backoff     :10;
        unsigned int reserved1011   :2;
        unsigned int coll_count     :4;
        unsigned int rndnum         :10;
        unsigned int reserved2631   :6;
    } Bits;
};

union _cpmac_Rx_UNI {
    volatile unsigned int Reg;
    volatile struct __cpmac_Rx_UNI {
        unsigned int ch0  :1;
        unsigned int ch1  :1;
        unsigned int ch2  :1;
        unsigned int ch3  :1;
        unsigned int ch4  :1;
        unsigned int ch5  :1;
        unsigned int ch6  :1;
        unsigned int ch7  :1;
        unsigned int reserved   :24;
    } Bits;
};

union _cpmac_Rx_MaxLen {
    volatile unsigned int Reg;
    volatile struct __cpmac_Rx_MaxLen {
        unsigned int RxMaxLen   :16;
        unsigned int reserved   :16;
    } Bits;
};


union _cpmac_MAC_Stat {
    volatile unsigned int Reg;
    volatile struct __cpmac_MAC_Stat {
        unsigned int tx_flow_act      :1;
        unsigned int rx_flow_act      :1;
        unsigned int rx_qos_act       :1;
        unsigned int reserved37       :5;
        unsigned int rx_err_ch        :3;
        unsigned int reserved11       :1;
        unsigned int rx_host_err_code :4;
        unsigned int reserved19       :1;
        unsigned int tx_host_err_code :4;
        unsigned int reserved2431     :8;
    } Bits;
};

union _cpmac_RTx_Ctrl {
    volatile unsigned int Reg;
    volatile struct __cpmac_RTx_Ctrl {
        unsigned int enable     :1;
        unsigned int reserved   :31;
    } Bits;
};

union _cpmac_Rx_MBP_Enable {
    volatile unsigned int Reg;
    volatile struct __cpmac_Rx_MBP_Enable {
        unsigned int rx_mult_ch   :3;
        unsigned int reserved34   :2;
        unsigned int rx_mult_en   :1;
        unsigned int reserved67   :2;
        unsigned int rx_broad_ch  :3;
        unsigned int reserved1112 :2;
        unsigned int rx_broad_en  :1;
        unsigned int reserved1415 :2;
        unsigned int rx_prom_ch   :3;
        unsigned int reserved1920 :2;
        unsigned int rx_caf_en    :1;
        unsigned int rx_cef_en    :1;
        unsigned int rx_csf_en    :1;
        unsigned int rx_cmf_en    :1;
        unsigned int reserved2527 :3;
        unsigned int rx_no_chain  :1;
        unsigned int rx_qos_en    :1;
        unsigned int rx_pass_crc  :1;
        unsigned int reserved31   :1;
    } Bits;
};

union _cpmac_Rx_Boffset {
    volatile unsigned int Reg;
    volatile struct __cpmac_Rx_Boffset {
        unsigned int Buffer_offset  :16;
        unsigned int reserved       :16;
    } Bits;
};

union _cpmac_MAC_Ctrl {
    volatile unsigned int Reg;
    volatile struct __cpmac_MAC_Ctrl {
        unsigned int fullduplex :1;
        unsigned int loopback   :1;
        unsigned int mtest      :1;
        unsigned int rx_flow_en :1;
        unsigned int tx_flow_en :1;
        unsigned int mii_en     :1;
        unsigned int tx_pace    :1;
        unsigned int reserved78 :2;
        unsigned int tx_ptype   :1;
        unsigned int reserved   :22;
    } Bits;
};

/*--- Function prototypes. ---*/
unsigned short Cpmac_Rnd(void);
unsigned int Cpmac_Init(void);
void cpmac_link_timer(void);
int cpmac_Poll(void);

#endif /*--- #ifndef _XMAC_H_ ---*/
