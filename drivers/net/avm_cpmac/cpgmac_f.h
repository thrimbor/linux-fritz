/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2008,2009,2010 AVM GmbH <fritzbox_info@avm.de>
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

#ifndef _CPMAC_H
#define _CPMAC_H

#include "xmac.h"

#define TXINTSTATRAW_OFF        0x170
#define TXINTSTATMASK_OFF       0x174
#define TXINTMASKSET_OFF        0x178
#define TXINTMASKCLEAR_OFF      0x17C
#define MACINVECTOR_OFF         0x180
#define MAC_EOI_VECTOR_OFF      0x184

#define RXINTSTATRAW_OFF        0x190
#define RXINTSTATMASK_OFF       0x194
#define RXINTMASKSET_OFF        0x198
#define RXINTMASKCLEAR_OFF      0x19C
#define MACINTSTATRAW_OFF       0x1A0
#define MACINTSTATMASK_OFF      0x1A4
#define MACINTMASKSET_OFF       0x1A8
#define MACINTMASKCLEAR_OFF     0x1AC

#define MACADDR_LOW_OFF         0x1B0
#define MACADDR0_LOW_OFF        0x1B0
#define MACADDR1_LOW_OFF        0x1B4
#define MACADDR2_LOW_OFF        0x1B8
#define MACADDR3_LOW_OFF        0x1BC
#define MACADDR4_LOW_OFF        0x1C0
#define MACADDR5_LOW_OFF        0x1C4
#define MACADDR6_LOW_OFF        0x1C8
#define MACADDR7_LOW_OFF        0x1CC
#define MACADDR_MID_OFF         0x1D0
#define MACADDR_HIGH_OFF        0x1D4

#define PACKET_TYPE_ETHERNET    0x7

union _cpgmacf_Rx_MBP_Enable {
    volatile unsigned int Reg;
    volatile struct __cpgmacf_Rx_MBP_Enable {
        unsigned int rx_mult_ch         :3;
        unsigned int reserved34         :2;
        unsigned int rx_mult_en         :1;
        unsigned int reserved67         :2;
        unsigned int rx_broad_ch        :3;
        unsigned int reserved1112       :2;
        unsigned int rx_broad_en        :1;
        unsigned int reserved1415       :2;
        unsigned int rx_prom_ch         :3;
        unsigned int reserved1920       :2;
        unsigned int rx_caf_en          :1;
        unsigned int rx_cef_en          :1;
        unsigned int rx_csf_en          :1;
        unsigned int rx_cmf_en          :1;
        unsigned int high_pri_thresh    :3;
        unsigned int reserved2831       :4;
    } Bits;
};

union _cpgmacf_MAC_Stat {
    volatile unsigned int Reg;
    volatile struct __cpgmacf_MAC_Stat {
        unsigned int tx_flow_act    :1;
        unsigned int rx_flow_act    :1;
        unsigned int reserved230    :29;
        unsigned int idle           :1;
    } Bits;
};

union _cpgmacf_MAC_Ctrl {
    volatile unsigned int Reg;
    volatile struct __cpgmacf_MAC_Ctrl {
        unsigned int fullduplex      :1;
        unsigned int loopback        :1;
        unsigned int mtest           :1;
        unsigned int rx_flow_en      :1;
        unsigned int tx_flow_en      :1;
        unsigned int gmii_en         :1;
        unsigned int tx_pace         :1;
        unsigned int gig             :1;
        unsigned int memtest         :1;
        unsigned int reserved9       :1;
        unsigned int tx_short_gap_en :1;
        unsigned int cmd_idle        :1;
        unsigned int reserved1214    :3;
        unsigned int ifctl_a         :1;
        unsigned int ifctl_b         :1;
        unsigned int gig_force       :1;
        unsigned int ext_en          :1;
        unsigned int rx_vlan_en      :1;
        unsigned int reserved2031    :12;
    } Bits;
};

union _cpgmacf_MAC_SrcAddr_l {
    volatile unsigned int Reg;
    volatile struct __cpmac_MAC_SrcAddr_l {
        unsigned int mac_src_15_8   :8;
        unsigned int mac_src_0_7    :8;
        unsigned int reserved       :16;
    } Bits;
};

union _cpgmacf_MAC_SrcAddr_h {
    volatile unsigned int Reg;
    volatile struct __cpmac_MAC_SrcAddr_h {
        unsigned int mac_src_40_47  :8;
        unsigned int mac_src_32_39  :8;
        unsigned int mac_src_24_31  :8;
        unsigned int mac_src_16_23  :8;
    } Bits;
};

/* Ethernet MAC Register Overlay Structure */
struct cpgmac_f_regs {
    volatile unsigned int           ID_VER;
    volatile unsigned int           MAC_IN_VECTOR;
    volatile unsigned int           MAC_EOI_VECTOR;
    volatile unsigned int           MAC_INT_STATRAW;
    volatile unsigned int           MAC_INT_STATMASKED;
    volatile unsigned int           MAC_INT_MASKSET;
    volatile unsigned int           MAC_INT_MASKCLEAR;
    union _cpgmacf_Rx_MBP_Enable    RX_MBP_ENABLE;
    union _cpmac_Rx_UNI             RX_UNICAST_SET;
    union _cpmac_Rx_UNI             RX_UNICAST_CLEAR;
    union _cpmac_Rx_MaxLen          RX_MAX_LEN;
    union _cpgmacf_MAC_Ctrl         MAC_CONTROL;
    union _cpgmacf_MAC_Stat         MAC_STATUS;
    volatile unsigned int           reserved1[1];
    volatile unsigned int           FIFO_CONTROL;
    volatile unsigned int           reserved2[1];
    volatile unsigned int           SOFT_RESET;
    union _cpgmacf_MAC_SrcAddr_l    MAC_SRC_ADDR_LOW;
    union _cpgmacf_MAC_SrcAddr_h    MAC_SRC_ADDR_HIGH;
    volatile unsigned int           MAC_HASH1;
    volatile unsigned int           MAC_HASH2;
    union _cpmac_Bofftest           BOFF_TEST;
    volatile unsigned int           TPACE_TEST;
    volatile unsigned int           RX_PAUSE;
    volatile unsigned int           TX_PAUSE;
    volatile unsigned int           PORT_VLAN;
    volatile unsigned int           RX_FLOW_THRESH;
    volatile unsigned int           reserved3[101];
    struct _cpmac_statistic_        STATISTIC;
    volatile unsigned int           reserved4[28];
    volatile unsigned int           RX_FIFO_TEST[64];
    volatile unsigned int           TX_FIFO_TEST[64];
    union _cpgmacf_MAC_SrcAddr_l    MAC_ADDR_LOW;
    union _cpgmacf_MAC_SrcAddr_h    MAC_ADDR_HIGH;
    volatile unsigned int           MAC_INDEX;
};

#endif /* _CPMAC_H */
