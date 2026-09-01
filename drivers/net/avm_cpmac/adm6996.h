/******************************************************************************
     Copyright (c) 2004, Infineon Technologies.  All rights reserved.
     Copyright (C) 2006,2007,2008,2009,2010 AVM GmbH <fritzbox_info@avm.de>

                               No Warranty
   Because the program is licensed free of charge, there is no warranty for
   the program, to the extent permitted by applicable law.  Except when
   otherwise stated in writing the copyright holders and/or other parties
   provide the program "as is" without warranty of any kind, either
   expressed or implied, including, but not limited to, the implied
   warranties of merchantability and fitness for a particular purpose. The
   entire risk as to the quality and performance of the program is with
   you.  should the program prove defective, you assume the cost of all
   necessary servicing, repair or correction.

   In no event unless required by applicable law or agreed to in writing
   will any copyright holder, or any other party who may modify and/or
   redistribute the program as permitted above, be liable to you for
   damages, including any general, special, incidental or consequential
   damages arising out of the use or inability to use the program
   (including but not limited to loss of data or data being rendered
   inaccurate or losses sustained by you or third parties or a failure of
   the program to operate with any other programs), even if such holder or
   other party has been advised of the possibility of such damages.
 *****************************************************************************/
#ifndef _ADM6996_H_
#define _ADM6996_H_

/* command codes */
#define ADM_SW_SMI_READ     0x02
#define ADM_SW_SMI_WRITE    0x01
#define ADM_SW_SMI_START    0x01

#define ADM_SW_EEPROM_WRITE     0x01
#define ADM_SW_EEPROM_WRITE_ENABLE  0x03
#define ADM_SW_EEPROM_WRITE_DISABLE 0x00
#define EEPROM_TYPE     8   /* for 93C66 */

/* bit masks */
#define ADM_SW_BIT_MASK_1   0x00000001
#define ADM_SW_BIT_MASK_2   0x00000002
#define ADM_SW_BIT_MASK_4   0x00000008
#define ADM_SW_BIT_MASK_10  0x00000200
#define ADM_SW_BIT_MASK_16  0x00008000
#define ADM_SW_BIT_MASK_32  0x80000000

/* delay timers */
#define ADM_SW_MDC_DOWN_DELAY   5
#define ADM_SW_MDC_UP_DELAY     5
#define ADM_SW_CS_DELAY         5

/* MDIO modes */
#define ADM_SW_MDIO_OUTPUT  0
#define ADM_SW_MDIO_INPUT   1

/* PHY power mode */
#define ADM_PHYS_OFF 0
#define ADM_PHYS_ON  1

/*  ADMTEK 6996LC 16 BIT Mode Register Definitions                          */
#define REG_ADM_LC_SIGNATURE    0x00
#define REG_ADM_LC_PORT0_CONF   0x1
#define REG_ADM_LC_PORT1_CONF   0x3
#define REG_ADM_LC_PORT2_CONF   0x5
#define REG_ADM_LC_PORT3_CONF   0x7
#define REG_ADM_LC_PORT4_CONF   0x8
#define REG_ADM_LC_PORT5_CONF   0x9
  #define ADM_CONF_FLOW_CONTROL            (1 <<  0)
  #define ADM_CONF_AUTO_NEGOTIATION        (1 <<  1)
  #define ADM_CONF_SPEED_100               (1 <<  2)
  #define ADM_CONF_DUPLEX_FULL             (1 <<  3)
  #define ADM_CONF_OUTPUT_PKT_TAG          (1 <<  4)
  #define ADM_CONF_PORT_DISABLE            (1 <<  5)
  #define ADM_CONF_PORT_BASE_PRIO_ENABLE   (1 <<  6)
  #define ADM_CONF_PORT_BASE_PRIO          (3 <<  7)
  #define ADM_CONF_PORT_VLAN_ID            (15 << 10)
  #define ADM_CONF_SELECT_FX               (1 << 14)
  #define ADM_CONF_AUTO_MDIX               (1 << 15)
#define REG_ADM_LC_VLAN_MODE    0x11
#define REG_ADM_LC_MAC_LOCK     0x12
#define REG_ADM_LC_VLAN0_CONF   0x13
#define REG_ADM_LC_PORT0_PVID   0x28
#define REG_ADM_LC_PORT1_PVID   0x29
#define REG_ADM_LC_PORT2_PVID   0x2a
#define REG_ADM_LC_PORT34_PVID  0x2b
#define REG_ADM_LC_RACP         0x2c
  #define ADM_CONF_RACP_P5VID_11_4         (0xff << 0)
  #define ADM_CONF_RACP_TAG_SHIFT          (0x07 << 8)
  #define ADM_CONF_RACP_TAG_SHIFT_SHIFT    8
#define REG_ADM_LC_PHY_RESET    0x2f
#define REG_ADM_LC_MISC_CONF    0x30
  #define ADM_CONF_MISC_MCEB               (1 <<  5)
  #define ADM_CONF_MISC_B                  (1 <<  7)
  #define ADM_CONF_MISC_DP                 (1 <<  8)
  #define ADM_CONF_MISC_DHCOL              (1 <<  9)
  #define ADM_CONF_MISC_P4                 (1 << 12)
#define REG_ADM_LC_BNDWDH_CTL0  0x31
#define REG_ADM_LC_BNDWDH_CTL1  0x32
#define REG_ADM_LC_BNDWDH_CTL_ENA   0x33

#define REG_ADM_LC_VLAN_FILTER0_LOW  0x40
#define REG_ADM_LC_VLAN_FILTER0_HIGH 0x41

#define REG_ADM_LC_IE                      0x9A
#define REG_ADM_LC_IS                      0x9B
  #define ADM_IE_LTADIE                    (1 << 8)
  /* ADM_IE_PSecIE to be defined */
  #define ADM_IE_COIE                      (1 << 1)
  #define ADM_IE_PStatIE                   (1 << 0)

#define REG_ADM_LC_ID                      0xa0
#define REG_ADM_LC_ID2                     0xa1
#define REG_ADM_LC_STATUS0                 0xa2
  #define ADM_LC_STATUS0_PORT0_LINKUP      (1 <<  0)
  #define ADM_LC_STATUS0_PORT0_SPEED       (1 <<  1)
  #define ADM_LC_STATUS0_PORT0_DUPLEX      (1 <<  2)
  #define ADM_LC_STATUS0_PORT0_FLOWCONTROL (1 <<  3)
  #define ADM_LC_STATUS0_PORT1_LINKUP      (1 <<  8)
  #define ADM_LC_STATUS0_PORT1_SPEED       (1 <<  9)
  #define ADM_LC_STATUS0_PORT1_DUPLEX      (1 << 10)
  #define ADM_LC_STATUS0_PORT1_FLOWCONTROL (1 << 11)
#define REG_ADM_LC_STATUS1                 0xa3
  #define ADM_LC_STATUS1_PORT2_LINKUP      (1 <<  0)
  #define ADM_LC_STATUS1_PORT2_SPEED       (1 <<  1)
  #define ADM_LC_STATUS1_PORT2_DUPLEX      (1 <<  2)
  #define ADM_LC_STATUS1_PORT2_FLOWCONTROL (1 <<  3)
  #define ADM_LC_STATUS1_PORT3_LINKUP      (1 <<  8)
  #define ADM_LC_STATUS1_PORT3_SPEED       (1 <<  9)
  #define ADM_LC_STATUS1_PORT3_DUPLEX      (1 << 10)
  #define ADM_LC_STATUS1_PORT3_FLOWCONTROL (1 << 11)
  #define ADM_LC_STATUS1_PORT4_LINKUP      (1 << 12)
  #define ADM_LC_STATUS1_PORT4_SPEED       (1 << 13)
  #define ADM_LC_STATUS1_PORT4_DUPLEX      (1 << 14)
  #define ADM_LC_STATUS1_PORT4_FLOWCONTROL (1 << 15)
#define REG_ADM_LC_STATUS2                 0xa4
  #define ADM_LC_STATUS2_PORT5_LINKUP      (1 <<  0)
  #define ADM_LC_STATUS2_PORT5_SPEED       (1 <<  1)
  #define ADM_LC_STATUS2_PORT5_DUPLEX      (1 <<  3)
  #define ADM_LC_STATUS2_PORT5_FLOWCONTROL (1 <<  4)

#define REG_ADM_LC_RXPKTCNT0                  0xa8
#define REG_ADM_LC_RXPKTCNT1                  0xac
#define REG_ADM_LC_RXPKTCNT2                  0xb0
#define REG_ADM_LC_RXPKTCNT3                  0xb4
#define REG_ADM_LC_RXPKTCNT4                  0xb6
#define REG_ADM_LC_RXPKTCNT5                  0xb8
#define REG_ADM_LC_RXPKTBYTECNT0              0xba
#define REG_ADM_LC_RXPKTBYTECNT1              0xbe
#define REG_ADM_LC_RXPKTBYTECNT2              0xc2
#define REG_ADM_LC_RXPKTBYTECNT3              0xc6
#define REG_ADM_LC_RXPKTBYTECNT4              0xc8
#define REG_ADM_LC_RXPKTBYTECNT5              0xca
#define REG_ADM_LC_TXPKTCNT0                  0xcc
#define REG_ADM_LC_TXPKTCNT1                  0xd0
#define REG_ADM_LC_TXPKTCNT2                  0xd4
#define REG_ADM_LC_TXPKTCNT3                  0xd8
#define REG_ADM_LC_TXPKTCNT4                  0xda
#define REG_ADM_LC_TXPKTCNT5                  0xdc
#define REG_ADM_LC_TXPKTBYTECNT0              0xde
#define REG_ADM_LC_TXPKTBYTECNT1              0xe2
#define REG_ADM_LC_TXPKTBYTECNT2              0xe6
#define REG_ADM_LC_TXPKTBYTECNT3              0xea
#define REG_ADM_LC_TXPKTBYTECNT4              0xec
#define REG_ADM_LC_TXPKTBYTECNT5              0xee
#define REG_ADM_LC_COLLISIONCNT0              0xf0
#define REG_ADM_LC_COLLISIONCNT1              0xf4
#define REG_ADM_LC_COLLISIONCNT2              0xf8
#define REG_ADM_LC_COLLISIONCNT3              0xfc
#define REG_ADM_LC_COLLISIONCNT4              0xfe
#define REG_ADM_LC_COLLISIONCNT5              0x100
#define REG_ADM_LC_ERRCNT0                    0x102
#define REG_ADM_LC_ERRCNT1                    0x106
#define REG_ADM_LC_ERRCNT2                    0x10a
#define REG_ADM_LC_ERRCNT3                    0x10e
#define REG_ADM_LC_ERRCNT4                    0x110
#define REG_ADM_LC_ERRCNT5                    0x112

#define REG_ADM_LC_OVERFLOWFLAG0              0x114
  #define ADM_OVERFLOW_PKTCNT_PORT0        (1 <<  0)
  #define ADM_OVERFLOW_PKTCNT_PORT1        (1 <<  2)
  #define ADM_OVERFLOW_PKTCNT_PORT2        (1 <<  4)
  #define ADM_OVERFLOW_PKTCNT_PORT3        (1 <<  6)
  #define ADM_OVERFLOW_PKTCNT_PORT4        (1 <<  7)
  #define ADM_OVERFLOW_PKTCNT_PORT5        (1 <<  8)
  #define ADM_OVERFLOW_PKTBYTECNT_PORT0    (1 <<  9)
  #define ADM_OVERFLOW_PKTBYTECNT_PORT1    (1 << 11)
  #define ADM_OVERFLOW_PKTBYTECNT_PORT2    (1 << 13)
  #define ADM_OVERFLOW_PKTBYTECNT_PORT3    (1 << 15)
  #define ADM_OVERFLOW_PKTBYTECNT_PORT4    (1 << 16)
  #define ADM_OVERFLOW_PKTBYTECNT_PORT5    (1 << 17)

#define REG_ADM_LC_OVERFLOWFLAG2              0x116
#define REG_ADM_LC_OVERFLOWFLAG4              0x118
  #define ADM_OVERFLOW_COLLCNT_PORT0       (1 <<  0)
  #define ADM_OVERFLOW_COLLCNT_PORT1       (1 <<  2)
  #define ADM_OVERFLOW_COLLCNT_PORT2       (1 <<  4)
  #define ADM_OVERFLOW_COLLCNT_PORT3       (1 <<  6)
  #define ADM_OVERFLOW_COLLCNT_PORT4       (1 <<  7)
  #define ADM_OVERFLOW_COLLCNT_PORT5       (1 <<  8)
  #define ADM_OVERFLOW_ERRCNT_PORT0        (1 <<  9)
  #define ADM_OVERFLOW_ERRCNT_PORT1        (1 << 11)
  #define ADM_OVERFLOW_ERRCNT_PORT2        (1 << 13)
  #define ADM_OVERFLOW_ERRCNT_PORT3        (1 << 15)
  #define ADM_OVERFLOW_ERRCNT_PORT4        (1 << 16)
  #define ADM_OVERFLOW_ERRCNT_PORT5        (1 << 17)


/* MAC Table management is the same as for the Tantos. Only offsets differ */

#define REG_ADM_LC_ATC0                       0x11A /* Addr 15 -  0 */
#define REG_ADM_LC_ATC1                       0x11B /* Addr 31 - 16 */
#define REG_ADM_LC_ATC2                       0x11C /* Addr 47 - 32 */
#define REG_ADM_LC_ATC3                       0x11D
  #define ADM_ATC3_FID_SHIFT                0 
  #define ADM_ATC3_FID_MASK                 (0x3  << ADM_ATC3_FID_SHIFT)
  #define ADM_ATC3_PMAP_SHIFT               4 
  #define ADM_ATC3_PMAP_MASK                (0x7f << ADM_ATC3_PMAP_SHIFT)

#define REG_ADM_LC_ATC4                       0x11E
  #define ADM_ATC4_ITAT_SHIFT               0 
  #define ADM_ATC4_ITAT_MASK                (0x7ff << ADM_ATC4_ITAT_SHIFT)
  #define ADM_ATC4_INFOT_SHIFT              12
  #define ADM_ATC4_INFOT_MASK               (0x1 << ADM_ATC4_INFOT_SHIFT)

#define REG_ADM_LC_ATC5                       0x11F
  #define ADM_ATC5_AC_CMD_SHIFT             0
  #define ADM_ATC5_AC_CMD_MASK              (0x7f << ADM_ATC5_AC_CMD_SHIFT) /* AC_CMD defines are used from Tantos */
  #define ADM_ATC5_FCE_SHIFT                7
  #define ADM_ATC5_FCE_MASK                 (0x1 << ADM_ATC5_FCE_SHIFT)

#define REG_ADM_LC_ATS0                       0x120 /* Addr 15 -  0 */
#define REG_ADM_LC_ATS1                       0x121 /* Addr 31 - 16 */
#define REG_ADM_LC_ATS2                       0x122 /* Addr 47 - 32 */
#define REG_ADM_LC_ATS3                       0x123
  #define ADM_ATS3_FIDS_SHIFT               0 
  #define ADM_ATS3_FIDS_MASK                (0x3  << ADM_ATS3_FIDS_SHIFT)
  #define ADM_ATS3_PMAPS_SHIFT              4 
  #define ADM_ATS3_PMAPS_MASK               (0x7f << ADM_ATS3_PMAPS_SHIFT)

#define REG_ADM_LC_ATS4                       0x124
  #define ADM_ATS4_ITATS_SHIFT               0 
  #define ADM_ATS4_ITATS_MASK                (0x7ff << ADM_ATS4_ITATS_SHIFT)
  #define ADM_ATS4_INFOTS_SHIFT              12
  #define ADM_ATS4_INFOTS_MASK               (0x1 << ADM_ATS4_INFOTS_SHIFT)
  #define ADM_ATS4_OCP_SHIFT                 13
  #define ADM_ATS4_OCP_MASK                  (0x1 << ADM_ATS4_OCP_SHIFT)
  #define ADM_ATS4_BAD_SHIFT                 14
  #define ADM_ATS4_BAD_MASK                  (0x1 << ADM_ATS4_BAD_SHIFT)

#define REG_ADM_LC_ATS5                       0x125
  #define ADM_ATS5_AC_CMD_SHIFT             0
  #define ADM_ATS5_AC_CMD_MASK              (0x7f << ADM_ATS5_AC_CMD_SHIFT) /* AC_CMD defines are used from Tantos */
  #define ADM_ATS5_FCE_SHIFT                7
  #define ADM_ATS5_FCE_MASK                 (0x1 << ADM_ATS5_FCE_SHIFT)
  #define ADM_ATS5_RSLT_SHIFT               12
  #define ADM_ATS5_RSLT_MASK                (0x1 << ADM_ATS5_RSLT_SHIFT)
  #define ADM_ATS5_BUSY_SHIFT               15
  #define ADM_ATS5_BUSY_MASK                (0x1 << ADM_ATS5_BUSY_SHIFT)

#define REG_ADM_LC_PHY_CONTROL_PORT0          0x200
#define REG_ADM_LC_PHY_CONTROL_PORT1          0x220
#define REG_ADM_LC_PHY_CONTROL_PORT2          0x240
#define REG_ADM_LC_PHY_CONTROL_PORT3          0x260
#define REG_ADM_LC_PHY_CONTROL_PORT4          0x280
  #define ADM_PHY_CONTROL_SPEED_MSB        (1 <<  6)
  #define ADM_PHY_CONTROL_COLTST           (1 <<  7)
  #define ADM_PHY_CONTROL_DPLX             (1 <<  8)
  #define ADM_PHY_CONTROL_ANEN_RST         (1 <<  9)
  #define ADM_PHY_CONTROL_ISO              (1 << 10)
  #define ADM_PHY_CONTROL_PDN              (1 << 11)
  #define ADM_PHY_CONTROL_ANEN             (1 << 12)
  #define ADM_PHY_CONTROL_SPEED_LSB        (1 << 13)
  #define ADM_PHY_CONTROL_LPBK             (1 << 14)
  #define ADM_PHY_CONTROL_RST              (1 << 15)

/* port modes */
#define ADM_SW_PORT_FLOWCTL 0x1 /* 802.3x flow control */
#define ADM_SW_PORT_AN      0x2 /* auto negotiation */
#define ADM_SW_PORT_100M    0x4 /* 100M */
#define ADM_SW_PORT_FULL    0x8 /* full duplex */
#define ADM_SW_PORT_TAG     0x10    /* output tag on */
#define ADM_SW_PORT_DISABLE 0x20    /* disable port */
#define ADM_SW_PORT_TOS     0x40    /* TOS first */
#define ADM_SW_PORT_PPRI    0x80    /* port based priority first */
#define ADM_SW_PORT_MDIX    0x8000  /* auto MDIX on */
#define ADM_SW_PORT_PVID_SHIFT  10
#define ADM_SW_PORT_PVID_BITS   4

/* VLAN */
#define ADM_SW_VLAN_PORT0   (1 << 0)
#define ADM_SW_VLAN_PORT1   (1 << 2)
#define ADM_SW_VLAN_PORT2   (1 << 4)
#define ADM_SW_VLAN_PORT3   (1 << 6)
#define ADM_SW_VLAN_PORT4   (1 << 7)
#define ADM_SW_VLAN_PORT5   (1 << 8)


typedef struct {
    unsigned short SIG;
    struct {
        unsigned short FCE       :  1;
        unsigned short ANE       :  1;
        unsigned short SA        :  1;
        unsigned short DA        :  1;
        unsigned short OPTE      :  1;
        unsigned short PD        :  1;
        unsigned short IPVLAN    :  1;
        unsigned short PPE       :  1;
        unsigned short PP        :  2;
        unsigned short PVID3_0   :  1;
        unsigned short SELFX_EE  :  1;
        unsigned short reserved0 :  1;
        unsigned short reserved1;
    } PBC[6];
    struct {
        unsigned short reserved0 : 3;
        unsigned short DFID      : 4;
        unsigned short RVIDFFF   : 1;
        unsigned short reserved1 : 1;
        unsigned short RVID      : 1;
        unsigned short reserved2 : 2;
        unsigned short ERCMPTH   : 4;
    } SC0;
    struct {
        unsigned short NE        : 1;
        unsigned short reserved0 : 5;
        unsigned short TSIE      : 1;
        unsigned short TE        : 1;
        unsigned short CMS       : 1;
        unsigned short reserved1 : 6;
        unsigned short DFEFD     : 1;
    } SC1;
    unsigned short           to_be_corrected0[0x011 - 0x0e]; /* FIXME */
    struct {
        unsigned short ATS       : 2;
        unsigned short IPI       : 1;
        unsigned short QO        : 1;
        unsigned short MCE       : 1;
        unsigned short TBV       : 1;
        unsigned short NSE       : 1;
        unsigned short MPL       : 3;
        unsigned short reserved0 : 6;
    } SC3;
    struct {
        unsigned short O0FL          : 1;
        unsigned short LED_ENABLE    : 1;
        unsigned short O1FL          : 1;
        unsigned short DUAL_COLOR_EE : 1;
        unsigned short O2FL          : 1;
        unsigned short PI            : 1;
        unsigned short O3FL          : 1;
        unsigned short O4FL          : 1;
        unsigned short O5FL          : 1;
        unsigned short reserved0     : 2;
        unsigned short TLE           : 1;
        unsigned short reserved1     : 2;
        unsigned short DCS           : 1;
        unsigned short DP            : 1;
    } SC4;
    struct {
        unsigned short P0        : 1;
        unsigned short reserved0 : 1;
        unsigned short P1        : 1;
        unsigned short reserved1 : 1;
        unsigned short P2        : 1;
        unsigned short reserved2 : 1;
        unsigned short P3        : 1;
        unsigned short reserved3 : 1;
        unsigned short P4        : 1;
        unsigned short P5        : 1;
        unsigned short reserved4 : 7;
    } FORWARD_GROUP[16];
    unsigned short           to_be_corrected1[0x028 - 0x25]; /* FIXME */

    unsigned short           to_be_corrected2[0x286 - 0x28]; /* FIXME */
} adm6996_switch_struct;

#endif
