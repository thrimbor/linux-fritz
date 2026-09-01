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

#if !defined(TANTOS_H)
#define TANTOS_H


/* Registers of Tantos-0G */

typedef struct {
    unsigned short token_r   : 11;
    unsigned short reserved0 :  5;
} tantos_port_egress_control;

#define TANTOS_MAX_PORTS        7
typedef struct {
    struct {
        struct {
            unsigned short link        :  1;
            unsigned short speed       :  1;
            unsigned short reserved0   :  1;
            unsigned short duplex      :  1;
            unsigned short flowcontrol :  1;
            unsigned short reserved1   : 11;
        } PS;
        struct {
            unsigned short RMWFQ     : 1;
            unsigned short FLD       : 1;
            unsigned short FLP       : 1;
            unsigned short reserved0 : 5;
            unsigned short TPE       : 1;
            unsigned short IPVLAN    : 1;
            unsigned short SPE       : 1;
            unsigned short VPE       : 1;
            unsigned short IPOVTU    : 1;
            unsigned short TCPE      : 1;
            unsigned short SPS       : 2;
        } PBC;
        struct {
            unsigned short reserved : 1;
            unsigned short IFNTE    : 1;
            unsigned short PAS      : 2;
            unsigned short IPMO     : 2;
            unsigned short PM       : 1;
            unsigned short PPPOEP   : 1;
            unsigned short MNA024   : 5;
            unsigned short IMTE     : 1;
            unsigned short LD       : 1;
            unsigned short AD       : 1;
        } PEC;
        struct {
            unsigned short DVPM     : 7;
            unsigned short BYPASS   : 1;
            unsigned short VMCE     : 1;
            unsigned short AOVTP    : 1;
            unsigned short VSD      : 1;
            unsigned short VC       : 1;
            unsigned short reserved : 1;
            unsigned short TBVE     : 1;
            unsigned short DFID     : 2;
        } PBVM;
        struct {
            unsigned short PVID    : 12;
            unsigned short PVTAGMP :  1;
            unsigned short PPE     :  1;
            unsigned short PP      :  2;
        } PDVID;
        tantos_port_egress_control PECSQ[4];
        tantos_port_egress_control PECWQ[4];
        struct {
            unsigned short token_r    : 11;
            unsigned short timer_tick :  2;
            unsigned short reserved0  :  3;
        } PICR;
        unsigned short reserved[0x20 - 0x0e];
    } port[TANTOS_MAX_PORTS];
} tantos_ports_struct;
extern tantos_ports_struct *tantos_ports;

/*--- typedef struct { ---*/
    /*--- struct { ---*/
        /*--- unsigned short VID : 12; ---*/
        /*--- unsigned short VP  :  3; ---*/
        /*--- unsigned short VV  :  1; ---*/
    /*--- } VFL; ---*/
    /*--- struct { ---*/
        /*--- unsigned short M   : 7; ---*/
        /*--- unsigned short TM  : 7; ---*/
        /*--- unsigned short FID : 2; ---*/
    /*--- } VFH; ---*/
/*--- } tantos_vlan_filter; ---*/

typedef struct {
    union {
        struct {
            unsigned short VID : 12;
            unsigned short VP  :  3;
            unsigned short VV  :  1;
        } Bits;
        unsigned short Register;
    } VFL;
    union {
        struct {
            unsigned short M   : 7;
            unsigned short TM  : 7;
            unsigned short FID : 2;
        } Bits;
        unsigned short Register;
    } VFH;
} tantos_vlan_filter;

typedef struct {
    unsigned short PQ0 : 2;
    unsigned short PQ1 : 2;
    unsigned short PQ2 : 2;
    unsigned short PQ3 : 2;
    unsigned short PQ4 : 2;
    unsigned short PQ5 : 2;
    unsigned short PQ6 : 2;
    unsigned short PQ7 : 2;
} tantos_diff_serv_mapping;

typedef struct {
    unsigned short TUPF;
    struct {
        unsigned short PRANGE   : 8;
        unsigned short COMP     : 2;
        unsigned short TUPF     : 2;
        unsigned short ATUF     : 2;
        unsigned short reserved : 2;
    } TUPR;
} tantos_tcpudp_port_filter;

typedef struct {
    unsigned short            reserved0[0x10 - 0x00];         /* Port 0 */
    tantos_vlan_filter        VF0[8];                         /* Filter 0-7 */
    unsigned short            reserved1[0x30 - 0x20];         /* Port 1 */
    tantos_vlan_filter        VF8[8];                         /* Filter 8-15 */
    unsigned short            reserved2[0x50 - 0x40];         /* Port 2 */
    unsigned short            TF[8];                          /* Ethernet type filter */
    tantos_diff_serv_mapping  DM[8];                          /* DiffServMapping */
    unsigned short            reserved3[0x70 - 0x60];         /* Port 3 */
    tantos_tcpudp_port_filter TUP[8];                         /* TCP/UDP port filter/range */
    unsigned short            reserved4[0x90 - 0x80];         /* Port 4 */
    unsigned short            to_be_corrected2[0xa0 - 0x90];  /* FIXME */
    unsigned short            reserved5[0xb0 - 0xa0];         /* Port 5 */
    unsigned short            to_be_corrected3[0xc0 - 0xb0];  /* FIXME */
    unsigned short            reserved6[0xd0 - 0xc0];         /* Port 6 */
    unsigned short            to_be_corrected4[0xe0 - 0xd0];  /* FIXME */
    struct {
        unsigned short DMQ0   : 2;
        unsigned short DMQ1   : 2;
        unsigned short DMQ2   : 2;
        unsigned short DMQ3   : 2;
        unsigned short MPL    : 2;
        unsigned short ATS    : 3;
        unsigned short DPWECH : 1;
        unsigned short PHYBA  : 1;
        unsigned short TSIPGE : 1;
    } SGC1;
    struct {
        unsigned short reserved0 : 3;
        unsigned short PCE       : 1;
        unsigned short PCR       : 1;
        unsigned short reserved1 : 1;
        unsigned short DUPCOLSP  : 1;
        unsigned short RVIDFFF   : 1;
        unsigned short RVID1     : 1;
        unsigned short RVID0     : 1;
        unsigned short reserved2 : 1;
        unsigned short ITENLMT   : 1;
        unsigned short ITRUNK    : 1;
        unsigned short reserved3 : 1;
        unsigned short ICRCCD    : 1;
        unsigned short SE        : 1;
    } SGC2;
    struct {
        unsigned short IGSTA : 1;
        unsigned short CCCRC : 1;
        unsigned short PAST  : 1;
        unsigned short STTE  : 1;
        unsigned short STRE  : 1;
        unsigned short CPN   : 3;
        unsigned short MSA   : 1;
        unsigned short MLA   : 1;
        unsigned short MPA   : 1;
        unsigned short MRA   : 1;
        unsigned short MCA   : 1;
        unsigned short SPN   : 3;
    } CMH;
    unsigned short            to_be_corrected5[0xf5 - 0xe3];  /* FIXME */
    struct {                                                  /* MII Port Control Register */
        unsigned short P4FCE     : 1;
        unsigned short P4DUP     : 1;
        unsigned short P4SPD     : 2;
        unsigned short P5FCE     : 1;
        unsigned short P5DUP     : 1;
        unsigned short P5SPD     : 2;
        unsigned short P6FCE     : 1;
        unsigned short P6DUP     : 1;
        unsigned short P6SPD     : 2;
        unsigned short reserved0 : 4;
    } MIICR;
    unsigned short            to_be_corrected6[0x100 - 0xf6]; /* FIXME */
    struct {                                                  /* Chip identifier 0 */
        unsigned short VN        :  4;
        unsigned short BOND      :  1;
        unsigned short reserved0 : 11;
    } CI0;
    unsigned short CI1;                                       /* Chip identifier 1 */
    struct {                                                  /* Global Status and Hardware Setting Register */
        unsigned short Res0   : 2;
        unsigned short P5M    : 2;
        unsigned short P6M    : 2;
        unsigned short DBBR   : 1;
        unsigned short HIGTBR : 1;
        unsigned short HISTBR : 1;
        unsigned short CTBR   : 1;
        unsigned short LLTBR  : 1;
        unsigned short LTBR   : 1;
        unsigned short Res1   : 4;
    } GSHS;
    unsigned short           reserved7[0x104 - 0x103];
    struct { unsigned short ADDR15_0  : 16; } ATC0;            /* Address Table Control Register 0 */
    struct { unsigned short ADDR31_16 : 16; } ATC1;            /* Address Table Control Register 1 */
    struct { unsigned short ADDR47_32 : 16; } ATC2;            /* Address Table Control Register 2 */
    struct {                                                   /* Address Table Control Register 3 */
        unsigned short FID  : 2;
        unsigned short Res0 : 2;
        unsigned short PMAP : 7;
        unsigned short Res1 : 5;
    } ATC3;
    struct {                                                   /* Address Table Control Register 4 */
        unsigned short ITAT  : 11;
        unsigned short Res0  :  1;
        unsigned short INFOT :  1;
        unsigned short Res1  :  3;
    } ATC4;
    struct {                                                   /* Address Table Control Register 5 */
        unsigned short AC_CMD : 7;                             /* Access control and command are combined to allow usage of easy defines */
#           define TANTOS_MACTABLE_CREATE                  0x07
#           define TANTOS_MACTABLE_OVERWRITE               0x0F
#           define TANTOS_MACTABLE_ERASE                   0x1F
#           define TANTOS_MACTABLE_SEARCH_EMPTY            0x20
#           define TANTOS_MACTABLE_SEARCH_PORT             0x29
#           define TANTOS_MACTABLE_SEARCH_FID              0x2A
#           define TANTOS_MACTABLE_SEARCH_ADDRESS          0x2C
#           define TANTOS_MACTABLE_SEARCH_ADDRESS_FID      0x2E
#           define TANTOS_MACTABLE_SEARCH_ADDRESS_PORT     0x2D
#           define TANTOS_MACTABLE_SEARCH_FID_PORT         0x2B
#           define TANTOS_MACTABLE_SEARCH_ADDRESS_FID_PORT 0x2F
#           define TANTOS_MACTABLE_INITIAL_LOCATION        0x34
#           define TANTOS_MACTABLE_INITIAL_FIRST           0x30
        unsigned short FCE    : 1;
        unsigned short Res    : 8;
    } ATC5;
    struct { unsigned short ADDR15_0  : 16; } ATS0;            /* Address Table Status Register 0 */
    struct { unsigned short ADDR31_16 : 16; } ATS1;            /* Address Table Status Register 1 */
    struct { unsigned short ADDR47_32 : 16; } ATS2;            /* Address Table Status Register 2 */
    struct {                                                   /* Address Table Status Register 3 */
        unsigned short FIDS  : 2;
        unsigned short Res0  : 2;
        unsigned short PMAPS : 7;
        unsigned short Res1  : 5;
    } ATS3;
    struct {                                                   /* Address Table Status Register 4 */
        unsigned short ITATS  : 11;
        unsigned short Res0   :  1;
        unsigned short INFOTS :  1;
        unsigned short OCP    :  1;
        unsigned short BAD    :  1;
        unsigned short Res1   :  1;
    } ATS4;
    struct {                                                   /* Address Table Status Register 5 */
        unsigned short AC_CMD : 7;
        unsigned short FCE    : 1;
        unsigned short Res    : 4;
        unsigned short RSLT   : 3;
#           define TANTOS_MACTABLE_RESULT_OK               0x0
#           define TANTOS_MACTABLE_RESULT_ALL_ENTRIES_USED 0x1
#           define TANTOS_MACTABLE_RESULT_ENTRY_NOT_FOUND  0x2
#           define TANTOS_MACTABLE_RESULT_TEMPORARY_STATE  0x3
#           define TANTOS_MACTABLE_RESULT_ERROR            0x5
        unsigned short BUSY   : 1;
    } ATS5;
    unsigned short           to_be_corrected7[0x11B - 0x110]; /* FIXME */
    struct {
        unsigned short OFFSET : 6;
        unsigned short PORTC  : 3;
        unsigned short CAC    : 2;
#           define TANTOS_COUNTER_READ                0x0
#           define TANTOS_COUNTER_READ_ALL            0x1
#           define TANTOS_COUNTER_RENEW_PORT_COUNTERS 0x2
#           define TANTOS_COUNTER_RENEW_ALL_COUNTERS  0x3
        unsigned short BAS    : 1;
        unsigned short Res    : 4;
    } RCC;
    unsigned short RCSL;
    unsigned short RCSH;
    unsigned short           to_be_corrected8[0x120 - 0x11E]; /* FIXME */
    struct {                                                  /* MII Indirect Access Control */
        unsigned short REGAD  : 5;
        unsigned short PHYAD  : 5;
        unsigned short OP     : 2;                            /* TANTOS_MIIAC_WRITE, TANTOS_MIIAC_READ */
        unsigned short Res    : 3;
        unsigned short MBUSY  : 1;
    } MIIAC;
    unsigned short MIIWD;                                     /* MII Indirect Write Data */
    unsigned short MIIRD;                                     /* MII Indirect Read Data */
} tantos_switch_struct;

/* Opcodes for MIIAC operations */
#define TANTOS_MIIAC_SYNC  0x0
#define TANTOS_MIIAC_WRITE 0x1
#define TANTOS_MIIAC_READ  0x2

typedef struct {
    unsigned short CR;
    unsigned short SR;
    unsigned short PHY_IR0;
    unsigned short PHY_IR1;
    unsigned short ANAR;
    unsigned short ANLPA;
    unsigned short ANER;
    unsigned short NPTR;
    unsigned short LPNR;
    unsigned short Reserved[7];
    struct {                                                  /* PHY Configuration register */
        unsigned short DISPMG   : 1;
        unsigned short ENREG8   : 1;
        unsigned short VTHR     : 2;
        unsigned short XOVEN    : 1;
        unsigned short MDIXINI  : 1;
        unsigned short ROMCOD25 : 1;
        unsigned short DRVONEN  : 1;
        unsigned short CONV     : 1;
        unsigned short Reserved : 1;
        unsigned short PWSAVTX  : 1;
        unsigned short VOLT23   : 1;
        unsigned short LBKMD    : 2;
        unsigned short DRVCTRL  : 2;
    } PHY_CR;
    unsigned short P10_CR;
    unsigned short P100_CR;
    unsigned short LCR;
    unsigned short PCR;
    unsigned short PGSR;
    unsigned short PSSR;
    unsigned short RVSR;
} tantos_phy_struct;

typedef struct {
    unsigned int RxUnicastPacket;
    unsigned int RxBroadcastPacket;
    unsigned int RxMulticastPacket;
    unsigned int RxCRCErrorPacket;
    unsigned int RxUndersizeGoodPacket;
    unsigned int RxOversizeGoodPacket;
    unsigned int RxUndersizeErrorPacket;
    unsigned int RxGoodPausePacket;
    unsigned int RxOversizeErrorPacket;
    unsigned int RxAlignErrorPacket;
    unsigned int FilteredPacket;
    unsigned int RxSize64Packet;
    unsigned int RxSize65_127Packet;
    unsigned int RxSize128_255Packet;
    unsigned int RxSize256_511Packet;
    unsigned int RxSize512_1023Packet;
    unsigned int RxSize1024_1522Packet;
    unsigned int TxUnicastPacket;
    unsigned int TxBroadcastPacket;
    unsigned int TxMulticastPacket;
    unsigned int TxSingleCollision;
    unsigned int TxMultipleCollision;
    unsigned int TxLateCollision;
    unsigned int TxExcessiveCollision;
    unsigned int TxCollision;
    unsigned int TxPausePacket;
    unsigned int TxSize64Packet;
    unsigned int TxSize65_127Packet;
    unsigned int TxSize128_255Packet;
    unsigned int TxSize256_511Packet;
    unsigned int TxSize512_1023Packet;
    unsigned int TxSize1024_1522Packet;
    unsigned int TxDroppedPacket;
    unsigned int RxDroppedPacket;
    unsigned long long RxGoodByte;
    unsigned long long RxBadByte;
    unsigned long long TxGoodByte;
} tantos_counter_struct;

#endif /*--- #if !defined(TANTOS_H) ---*/

