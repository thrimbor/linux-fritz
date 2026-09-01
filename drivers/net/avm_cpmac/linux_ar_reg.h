/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2008,...,2013 AVM GmbH <fritzbox_info@avm.de>
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

/****************************************************************************
**      AR8216 support
*****************************************************************************/
#ifndef _INC_AR_REG
#define _INC_AR_REG

/*------------------------------------------------------------------------------------------*\
 * global Register 32bit
\*------------------------------------------------------------------------------------------*/
#define AR8216_GLOBAL_DEVICE_ID         0x00
#define AR8216_DEVICE_ID                0x0101
#define AR8226_DEVICE_ID                0x0201
#define AR8316_DEVICE_ID                0x1001

#define AR8316_GLOBAL_PWR_ON_STRAP      0x08
#  define AR8316_GLOBAL_PWR_ON_STRAP_RESERVED             0x000e1b20
#  define AR8316_GLOBAL_PWR_ON_STRAP_MAC0_GMII_EN         (1u <<  0)
#  define AR8316_GLOBAL_PWR_ON_STRAP_MAC0_RGMII_EN        (1u <<  1)
#  define AR8316_GLOBAL_PWR_ON_STRAP_PHY4_GMII_EN         (1u <<  2)
#  define AR8316_GLOBAL_PWR_ON_STRAP_PHY4_RGMII_EN        (1u <<  3)
#  define AR8316_GLOBAL_PWR_ON_STRAP_MAC0_MAC_MODE        (1u <<  4)
#  define AR8316_GLOBAL_PWR_ON_STRAP_RGMII_RXCLK_DELAY_EN (1u <<  6)
#  define AR8316_GLOBAL_PWR_ON_STRAP_RGMII_TXCLK_DELAY_EN (1u <<  7)
#  define AR8316_GLOBAL_PWR_ON_STRAP_MAC5_MAC_MODE        (1u << 14)
#  define AR8316_GLOBAL_PWR_ON_STRAP_MAC5_PHY_MODE        (1u << 15)
#  define AR8316_GLOBAL_PWR_ON_STRAP_TXDELAY_S0           (1u << 21)
#  define AR8316_GLOBAL_PWR_ON_STRAP_TXDELAY_S1           (1u << 22)
#  define AR8316_GLOBAL_PWR_ON_STRAP_RXDELAY_S0           (1u << 23)
#  define AR8316_GLOBAL_PWR_ON_STRAP_LED_OPEN_EN          (1u << 24)
#  define AR8316_GLOBAL_PWR_ON_STRAP_SPI_EN               (1u << 25)
#  define AR8316_GLOBAL_PWR_ON_STRAP_RXDELAY_S1           (1u << 26)
#  define AR8316_GLOBAL_PWR_ON_STRAP_POWER_ON_SEL         (1u << 31)
#define AR8216_GLOBAL_INTERRUPT         0x10
#define AR8216_GLOBAL_INTERRUPTMASK     0x14
#define AR8216_GLOBAL_MAC_ADDRESS_HIGH  0x20
#define AR8216_GLOBAL_MAC_ADDRESS_LOW   0x24
#define AR8316_FLOOD_MASK               0x2c
#  define AR8316_FLOOD_MASK_UNI_FLOOD_SHIFT              0
#  define AR8316_FLOOD_MASK_UNI_FLOOD_MASK               (((1u << 6)-1)<< AR8316_FLOOD_MASK_UNI_FLOOD_SHIFT)               
#  define AR8316_FLOOD_MASK_UNI_FLOOD_VALUE(value)       (((value)<< AR8316_FLOOD_MASK_UNI_FLOOD_SHIFT) & AR8316_FLOOD_MASK_UNI_FLOOD_MASK)           
#  define AR8316_FLOOD_MASK_IGMP_JOIN_LEAVE_SHIFT        8
#  define AR8316_FLOOD_MASK_IGMP_JOIN_LEAVE_MASK         (((1u << 6)-1)<< AR8316_FLOOD_MASK_IGMP_JOIN_LEAVE_SHIFT)               
#  define AR8316_FLOOD_MASK_IGMP_JOIN_LEAVE_VALUE(value) (((value)<< AR8316_FLOOD_MASK_IGMP_JOIN_LEAVE_SHIFT) & AR8316_FLOOD_MASK_IGMP_JOIN_LEAVE_MASK)           
#  define AR8316_FLOOD_MASK_MULTI_FLOOD_SHIFT            16
#  define AR8316_FLOOD_MASK_MULTI_FLOOD_MASK             (((1u << 6)-1)<< AR8316_FLOOD_MASK_MULTI_FLOOD_SHIFT)               
#  define AR8316_FLOOD_MASK_MULTI_FLOOD_VALUE(value)     (((value)<< AR8316_FLOOD_MASK_MULTI_FLOOD_SHIFT) & AR8316_FLOOD_MASK_MULTI_FLOOD_MASK)           
#  define AR8316_FLOOD_MASK_ARL_UNI_LEAKY_EN             (1u << 24)
#  define AR8316_FLOOD_MASK_ARL_MULTI_LEAKY_EN           (1u << 25)
#  define AR8316_FLOOD_MASK_BROAD_TO_CPU_EN              (1u << 26)
#define AR8216_GLOBAL_CONTROL           0x30

#define AR8316_QM_CONTROL               0x3C
#  define AR8316_QM_CONTROL_IGMP_COPY_EN                  (1u << 11)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AR8216_GLOBAL_VLAN_TABLE_1      0x40
#define VLAN_TABLE_1_VT_FUNC_SHIFT          0
#define VLAN_TABLE_1_VT_FUNC_MASK           (((1u << 3) - 1) << VLAN_TABLE_1_VT_FUNC_SHIFT)               
#define VLAN_TABLE_1_VT_FUNC_VALUE(value)   (((value) << VLAN_TABLE_1_VT_FUNC_SHIFT) & VLAN_TABLE_1_VT_FUNC_MASK)           
#  define VLAN_TABLE_FUNC_NOOP              0u
#  define VLAN_TABLE_FUNC_FLUSH_ALL         1u
#  define VLAN_TABLE_FUNC_LOAD_ENTRY        2u
#  define VLAN_TABLE_FUNC_PURGE_ENTRY       3u
#  define VLAN_TABLE_FUNC_REMOVE_PORT       4u
#  define VLAN_TABLE_FUNC_READ_NEXT_ENTRY   5u
#  define VLAN_TABLE_FUNC_READ_ONE_ENTRY    6u
#define VLAN_TABLE_1_VT_BUSY                (1u << 3)
#define VLAN_TABLE_1_VT_FULL_VIO            (1u << 4)
#define VLAN_TABLE_1_VT_PORT_NUM_SHIFT      8
#define VLAN_TABLE_1_VT_PORT_NUM_MASK       (((1u << 4)-1)<< VLAN_TABLE_1_VT_PORT_NUM_SHIFT)               
#define VLAN_TABLE_1_VT_PORT_NUM_VALUE(value) (((value)<< VLAN_TABLE_1_VT_PORT_NUM_SHIFT) & VLAN_TABLE_1_VT_PORT_NUM_MASK)           
#define VLAN_TABLE_1_VID_SHIFT              16
#define VLAN_TABLE_1_VID_MASK               (((1u << 12)-1)<< VLAN_TABLE_1_VID_SHIFT)               
#define VLAN_TABLE_1_VID_VALUE(value)       (((value)<< VLAN_TABLE_1_VID_SHIFT) & VLAN_TABLE_1_VID_MASK)           
#define VLAN_TABLE_1_VT_PRI_SHIFT           28
#define VLAN_TABLE_1_VT_PRI_MASK            (((1u << 3)-1)<< VLAN_TABLE_1_VT_PRI_SHIFT)               
#define VLAN_TABLE_1_VT_PRI_VALUE(value)    (((value)<< VLAN_TABLE_1_VT_PRI_SHIFT) & VLAN_TABLE_1_VT_PRI_MASK)           
#define VLAN_TABLE_1_VT_PRI_EN              (1u << 31)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AR8216_GLOBAL_VLAN_TABLE_2      0x44
#define VLAN_TABLE_2_VID_MEM_SHIFT          0
#define VLAN_TABLE_2_VID_MEM_MASK           (((1<<10)-1)<< VLAN_TABLE_2_VID_MEM_SHIFT)               
#define VLAN_TABLE_2_VID_MEM_VALUE(value)   (((value)<< VLAN_TABLE_2_VID_MEM_SHIFT) & VLAN_TABLE_2_VID_MEM_MASK)           
#define VLAN_TABLE_2_VT_VALID               (1u << 11)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AR8216_GLOBAL_AR_1              0x50
#define AR_1_AT_ADDR_BYTE4_SHIFT          24
#define AR_1_AT_ADDR_BYTE4_MASK           (((1u << 8)-1)<< AR_1_AT_ADDR_BYTE4_SHIFT)               
#define AR_1_AT_ADDR_BYTE4_VALUE(value)   (((value)<< AR_1_AT_ADDR_BYTE4_SHIFT) & AR_1_AT_ADDR_BYTE4_MASK)           

#define AR_1_AT_ADDR_BYTE5_SHIFT          16
#define AR_1_AT_ADDR_BYTE5_MASK           (((1u << 8)-1)<< AR_1_AT_ADDR_BYTE5_SHIFT)               
#define AR_1_AT_ADDR_BYTE5_VALUE(value)   (((value)<< AR_1_AT_ADDR_BYTE5_SHIFT) & AR_1_AT_ADDR_BYTE5_MASK)           

#define AR_1_AR_FULL_VIO                  (1u << 12)

#define AR_1_AT_PORT_NUM_SHIFT            8
#define AR_1_AT_PORT_NUM_MASK             (((1<<4)-1)<< AR_1_AT_PORT_NUM_SHIFT)               
#define AR_1_AT_PORT_NUM_VALUE(value)     (((value)<< AR_1_AT_PORT_NUM_SHIFT) & AR_1_AT_PORT_NUM_MASK)           

#define AR_1_AT_BUSY                      (1u << 3)

#define AR_1_AT_FUNC_SHIFT                0
#define AR_1_AT_FUNC_MASK                 (((1<<3)-1)<< AR_1_AT_FUNC_SHIFT)               
#define AR_1_AT_FUNC_VALUE(value)         (((value)<< AR_1_AT_FUNC_SHIFT) & AR_1_AT_FUNC_MASK)           
#  define AR_1_AT_FUNC_VALUE_NOP            0
#  define AR_1_AT_FUNC_VALUE_FLUSH_ALL      1
#  define AR_1_AT_FUNC_VALUE_LOAD           2
#  define AR_1_AT_FUNC_VALUE_PURGE          3
#  define AR_1_AT_FUNC_VALUE_FLUSH_UNLOCKED 4
#  define AR_1_AT_FUNC_VALUE_FLUSH_PORT     5
#  define AR_1_AT_FUNC_VALUE_NEXT           6

/*--- #define _SHIFT          0 ---*/
/*--- #define _MASK           (((1<<10)-1)<< _SHIFT) ---*/               
/*--- #define _VALUE(value)   (((value)<< _SHIFT) & _MASK) ---*/           

/*--- #define _SHIFT          0 ---*/
/*--- #define _MASK           (((1<<10)-1)<< _SHIFT) ---*/               
/*--- #define _VALUE(value)   (((value)<< _SHIFT) & _MASK) ---*/           

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AR8216_GLOBAL_AR_2              0x54
#define AR_2_AT_ADDR_BYTE0_SHIFT          24
#define AR_2_AT_ADDR_BYTE0_MASK           (((1u << 8)-1) << AR_2_AT_ADDR_BYTE0_SHIFT)               
#define AR_2_AT_ADDR_BYTE0_VALUE(value)   (((value) << AR_2_AT_ADDR_BYTE0_SHIFT) & AR_2_AT_ADDR_BYTE0_MASK)           

#define AR_2_AT_ADDR_BYTE1_SHIFT          16 
#define AR_2_AT_ADDR_BYTE1_MASK           (((1u << 8)-1) << AR_2_AT_ADDR_BYTE1_SHIFT)               
#define AR_2_AT_ADDR_BYTE1_VALUE(value)   (((value) << AR_2_AT_ADDR_BYTE1_SHIFT) & AR_2_AT_ADDR_BYTE1_MASK)           

#define AR_2_AT_ADDR_BYTE2_SHIFT          8
#define AR_2_AT_ADDR_BYTE2_MASK           (((1u << 8)-1) << AR_2_AT_ADDR_BYTE2_SHIFT)               
#define AR_2_AT_ADDR_BYTE2_VALUE(value)   (((value) << AR_2_AT_ADDR_BYTE2_SHIFT) & AR_2_AT_ADDR_BYTE2_MASK)           

#define AR_2_AT_ADDR_BYTE3_SHIFT          0
#define AR_2_AT_ADDR_BYTE3_MASK           (((1u << 8)-1) << AR_2_AT_ADDR_BYTE3_SHIFT)               
#define AR_2_AT_ADDR_BYTE3_VALUE(value)   (((value) << AR_2_AT_ADDR_BYTE3_SHIFT) & AR_2_AT_ADDR_BYTE3_MASK)           

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AR8216_GLOBAL_AR_3              0x58
#define AR_3_DES_PORT_SHIFT             0
#define AR_3_DES_PORT_MASK              (((1<<6)-1)<< AR_3_DES_PORT_SHIFT)               
#define AR_3_DES_PORT_VALUE(value)      (((value)<< AR_3_DES_PORT_SHIFT) & AR_3_DES_PORT_MASK)           

#define AR_3_AT_PRIORITY_SHIFT          10
#define AR_3_AT_PRIORITY_MASK           (((1<<2)-1)<< AR_3_AT_PRIORITY_SHIFT)               
#define AR_3_AT_PRIORITY_VALUE(value)   (((value)<< AR_3_AT_PRIORITY_SHIFT) & AR_3_AT_PRIORITY_MASK)           

#define AR_3_AT_PRIORITY_EN             (1u << 12)
#define AR_3_AT_MIRROR_EN               (1u << 13)

#define AR_3_AT_STATUS_SHIFT            14
#define AR_3_AT_STATUS_MASK             (((1<<2)-1)<< AR_3_AT_STATUS_SHIFT)               
#define AR_3_AT_STATUS_VALUE(value)     (((value)<< AR_3_AT_STATUS_SHIFT) & AR_3_AT_STATUS_MASK)           

#define AR_3_SA_DROP_EN              (1u << 16)
#define AR_3_REDIRECT_TO_CPU         (1u << 25)
#define AR_3_COPY_TO_CPU             (1u << 26)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AR8316_GLOBAL_AR_0              0x50
#define AR8316_GLOBAL_AR_0_ADDR_BYTE4_SHIFT         24
#define AR8316_GLOBAL_AR_0_ADDR_BYTE4_MASK          (((1u << 8)-1) << AR8316_GLOBAL_AR_0_ADDR_BYTE4_SHIFT)               
#define AR8316_GLOBAL_AR_0_ADDR_BYTE4_VALUE(value)  (((value) << AR8316_GLOBAL_AR_0_ADDR_BYTE4_SHIFT) & AR8316_GLOBAL_AR_0_ADDR_BYTE4_MASK)           

#define AR8316_GLOBAL_AR_0_ADDR_BYTE5_SHIFT         16
#define AR8316_GLOBAL_AR_0_ADDR_BYTE5_MASK          (((1u << 8)-1) << AR8316_GLOBAL_AR_0_ADDR_BYTE5_SHIFT)               
#define AR8316_GLOBAL_AR_0_ADDR_BYTE5_VALUE(value)  (((value) << AR8316_GLOBAL_AR_0_ADDR_BYTE5_SHIFT) & AR8316_GLOBAL_AR_0_ADDR_BYTE5_MASK)           

#define AR8316_GLOBAL_AR_0_AR_FULL_VIO              (1u << 12)

#define AR8316_GLOBAL_AR_0_AT_PORT_NUM_SHIFT        8
#define AR8316_GLOBAL_AR_0_AT_PORT_NUM_MASK         (((1 << 4)-1) << AR8316_GLOBAL_AR_0_AT_PORT_NUM_SHIFT)               
#define AR8316_GLOBAL_AR_0_AT_PORT_NUM_VALUE(value) (((value) << AR8316_GLOBAL_AR_0_AT_PORT_NUM_SHIFT) & AR8316_GLOBAL_AR_0_AT_PORT_NUM_MASK)           

#define AR8316_GLOBAL_AR_0_FLUSH_STATIC_EN              (1u << 4)
#define AR8316_GLOBAL_AR_0_AT_BUSY                      (1u << 3)

#define AR8316_GLOBAL_AR_0_AT_FUNC_SHIFT                0
#define AR8316_GLOBAL_AR_0_AT_FUNC_MASK                 (((1 << 3)-1) << AR8316_GLOBAL_AR_0_AT_FUNC_SHIFT)               
#define AR8316_GLOBAL_AR_0_AT_FUNC_VALUE(value)         (((value) << AR8316_GLOBAL_AR_0_AT_FUNC_SHIFT) & AR8316_GLOBAL_AR_0_AT_FUNC_MASK)           
#  define AR8316_GLOBAL_AR_0_AT_FUNC_VALUE_NOP            0
#  define AR8316_GLOBAL_AR_0_AT_FUNC_VALUE_FLUSH_ALL      1
#  define AR8316_GLOBAL_AR_0_AT_FUNC_VALUE_LOAD           2
#  define AR8316_GLOBAL_AR_0_AT_FUNC_VALUE_PURGE          3
#  define AR8316_GLOBAL_AR_0_AT_FUNC_VALUE_FLUSH_UNLOCKED 4
#  define AR8316_GLOBAL_AR_0_AT_FUNC_VALUE_FLUSH_PORT     5
#  define AR8316_GLOBAL_AR_0_AT_FUNC_VALUE_NEXT           6
#  define AR8316_GLOBAL_AR_0_AT_FUNC_VALUE_SEARCH_MAC     7


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AR8316_GLOBAL_AR_1              0x54
#define AR8316_GLOBAL_AR_1_ADDR_BYTE0_SHIFT          24
#define AR8316_GLOBAL_AR_1_ADDR_BYTE0_MASK           (((1u << 8)-1) << AR8316_GLOBAL_AR_1_ADDR_BYTE0_SHIFT)               
#define AR8316_GLOBAL_AR_1_ADDR_BYTE0_VALUE(value)   (((value) << AR8316_GLOBAL_AR_1_ADDR_BYTE0_SHIFT) & AR8316_GLOBAL_AR_1_ADDR_BYTE0_MASK)           

#define AR8316_GLOBAL_AR_1_ADDR_BYTE1_SHIFT          16 
#define AR8316_GLOBAL_AR_1_ADDR_BYTE1_MASK           (((1u << 8)-1) << AR8316_GLOBAL_AR_1_ADDR_BYTE1_SHIFT)               
#define AR8316_GLOBAL_AR_1_ADDR_BYTE1_VALUE(value)   (((value) << AR8316_GLOBAL_AR_1_ADDR_BYTE1_SHIFT) & AR8316_GLOBAL_AR_1_ADDR_BYTE1_MASK)           

#define AR8316_GLOBAL_AR_1_ADDR_BYTE2_SHIFT          8
#define AR8316_GLOBAL_AR_1_ADDR_BYTE2_MASK           (((1u << 8)-1) << AR8316_GLOBAL_AR_1_ADDR_BYTE2_SHIFT)               
#define AR8316_GLOBAL_AR_1_ADDR_BYTE2_VALUE(value)   (((value) << AR8316_GLOBAL_AR_1_ADDR_BYTE2_SHIFT) & AR8316_GLOBAL_AR_1_ADDR_BYTE2_MASK)           

#define AR8316_GLOBAL_AR_1_ADDR_BYTE3_SHIFT          0
#define AR8316_GLOBAL_AR_1_ADDR_BYTE3_MASK           (((1u << 8)-1) << AR8316_GLOBAL_AR_1_ADDR_BYTE3_SHIFT)               
#define AR8316_GLOBAL_AR_1_ADDR_BYTE3_VALUE(value)   (((value) << AR8316_GLOBAL_AR_1_ADDR_BYTE3_SHIFT) & AR8316_GLOBAL_AR_1_ADDR_BYTE3_MASK)           

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AR8316_GLOBAL_AR_2              0x58
#define AR8316_GLOBAL_AR_2_DES_PORT_SHIFT             0
#define AR8316_GLOBAL_AR_2_DES_PORT_MASK              (((1<<6)-1)<< AR8316_GLOBAL_AR_2_DES_PORT_SHIFT)               
#define AR8316_GLOBAL_AR_2_DES_PORT_VALUE(value)      (((value)<< AR8316_GLOBAL_AR_2_DES_PORT_SHIFT) & AR8316_GLOBAL_AR_2_DES_PORT_MASK)           

#define AR8316_GLOBAL_AR_2_AT_PRIORITY_SHIFT          10
#define AR8316_GLOBAL_AR_2_AT_PRIORITY_MASK           (((1<<2)-1)<< AR8316_GLOBAL_AR_2_AT_PRIORITY_SHIFT)               
#define AR8316_GLOBAL_AR_2_AT_PRIORITY_VALUE(value)   (((value)<< AR8316_GLOBAL_AR_2_AT_PRIORITY_SHIFT) & AR8316_GLOBAL_AR_2_AT_PRIORITY_MASK)           

#define AR8316_GLOBAL_AR_2_AT_PRIORITY_EN             (1u << 12)
#define AR8316_GLOBAL_AR_2_AT_MIRROR_EN               (1u << 13)
#define AR8316_GLOBAL_AR_2_SA_DROP_EN                 (1u << 14)
#define AR8316_GLOBAL_AR_2_MAC_CLONE                  (1u << 15)

#define AR8316_GLOBAL_AR_2_AT_STATUS_SHIFT            16
#define AR8316_GLOBAL_AR_2_AT_STATUS_MASK             (((1<<4)-1)<< AR8316_GLOBAL_AR_2_AT_STATUS_SHIFT)               
#define AR8316_GLOBAL_AR_2_AT_STATUS_VALUE(value)     (((value) << AR8316_GLOBAL_AR_2_AT_STATUS_SHIFT) & AR8316_GLOBAL_AR_2_AT_STATUS_MASK)           
#  define AR8316_GLOBAL_AR_2_AT_STATUS_VALUE_EMPTY    0u
#  define AR8316_GLOBAL_AR_2_AT_STATUS_VALUE_DYNAMIC  1u
#  define AR8316_GLOBAL_AR_2_AT_STATUS_VALUE_IS_DYNAMIC(value) ((value >= 1u) && (value <= 7u))
#  define AR8316_GLOBAL_AR_2_AT_STATUS_VALUE_STATIC   0xf

#define AR8316_GLOBAL_AR_2_LEAKY_EN                   (1u << 24)
#define AR8316_GLOBAL_AR_2_REDIRECT_TO_CPU            (1u << 25)
#define AR8316_GLOBAL_AR_2_COPY_TO_CPU                (1u << 26)


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AR8216_GLOBAL_AR_CONTROL        0x5C

#define AR_CONTROL_AGE_EN            (1u << 17)
#define AR_CONTROL_AGE_TIME_SHIFT    0
#define AR_CONTROL_AGE_TIME_MASK     (((1u << 16) - 1) << AR_CONTROL_AGE_TIME_SHIFT)               
#define AR_CONTROL_AGE_TIME(value)   (((value) << AR_CONTROL_AGE_TIME_SHIFT) & AR_CONTROL_AGE_TIME_MASK)           

#define AR8216_GLOBAL_IP_PRIORITY_1     0x60
#define AR8216_GLOBAL_IP_PRIORITY_2     0x64
#define AR8216_GLOBAL_IP_PRIORITY_3     0x68
#define AR8216_GLOBAL_IP_PRIORITY_4     0x6C
#define AR8216_GLOBAL_TAG_PRIORITY      0x70
#define AR8216_GLOBAL_CPUPORT           0x78
#  define AR8216_GLOBAL_CPUPORT_CPU_PORT_EN            (1u << 8)
#  define AR8216_GLOBAL_CPUPORT_MIRROR_PORT_NUM_SHIFT  4
#  define AR8216_GLOBAL_CPUPORT_MIRROR_PORT_NUM_MASK   (0xf << AR8216_GLOBAL_CPUPORT_MIRROR_PORT_NUM_SHIFT)
#  define AR8216_GLOBAL_CPUPORT_MIRROR_PORT_NUM(value) (((value) << AR8216_GLOBAL_CPUPORT_MIRROR_PORT_NUM_SHIFT) & AR8216_GLOBAL_CPUPORT_MIRROR_PORT_NUM_MASK)
#define AR8216_GLOBAL_MIB               0x80
#define AR8216_GLOBAL_MDIO_HIGH_ADDR    0x94
#define AR8216_GLOBAL_DEST_IP           0x98

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AR8216_PORT0_CONTROL_BASIS      0x100
#define AR8216_PORT1_CONTROL_BASIS      0x200
#define AR8216_PORT2_CONTROL_BASIS      0x300
#define AR8216_PORT3_CONTROL_BASIS      0x400
#define AR8216_PORT4_CONTROL_BASIS      0x500
#define AR8216_PORT5_CONTROL_BASIS      0x600

/*--- PORT-STATUS ---*/
#define AR8216_PORT_STATUS_OFFSET       0x00
#  define ATH_PORT_STATUS_SPEED_10M           (0u <<  0)
#  define ATH_PORT_STATUS_SPEED_100M          (1u <<  0)
#  define ATH_PORT_STATUS_SPEED_1000M         (2u <<  0)
#  define ATH_PORT_STATUS_TXMACEN             (1u <<  2)
#  define ATH_PORT_STATUS_RXMACEN             (1u <<  3)
#  define ATH_PORT_STATUS_TXFLOWEN            (1u <<  4)
#  define ATH_PORT_STATUS_RXFLOWEN            (1u <<  5)
#  define ATH_PORT_STATUS_FULLDUPLEX          (1u <<  6)
#  define ATH_PORT_STATUS_TX_HALF_FLOW_EN     (1u <<  7)
#  define ATH_PORT_STATUS_PHY_LINKUP          (1u <<  8)
#  define ATH_PORT_STATUS_LINKEN              (1u <<  9)
#  define ATH_PORT_STATUS_LINK_PAUSE          (1u << 10)
#  define ATH_PORT_STATUS_ASYNC_PAUSE         (1u << 11)

/*--- PORT-CONTROL ---*/
#define AR8216_PORT_CONTROL_OFFSET      0x04
#  define ATH_PORT_CTRL_PORTSTATE_MASK        (0x3u << 0)
#  define ATH_PORT_CTRL_PORTSTATE_DISABLE     (0<<0)
#  define ATH_PORT_CTRL_PORTSTATE_BLOCKING    (1)
#  define ATH_PORT_CTRL_PORTSTATE_LISTEN      (2)
#  define ATH_PORT_CTRL_PORTSTATE_LEARNING    (3)
#  define ATH_PORT_CTRL_PORTSTATE_FORWARD     (4)
#  define ATH_PORT_CTRL_LEARN_ONE_LOCK        (1<<7)
#  define ATH_PORT_CTRL_EG_VLAN_MODE_MASK     ((0x3u)<<8)
#  define ATH_PORT_CTRL_EG_VLAN_MODE(x)       ((x)<<8)
#    define ATH_PORT_CTRL_EG_VLAN_MODE_UNMODIFIED  0
#    define ATH_PORT_CTRL_EG_VLAN_MODE_UNTAGGED    1
#    define ATH_PORT_CTRL_EG_VLAN_MODE_TAGGED      2
#  define ATH_PORT_CTRL_IGMP_MLD_EN           (1<<10)
#  define ATH_PORT_CTRL_HEAD_EN               (1<<11)
#  define ATH_PORT_CTRL_MAC_LOOP_BACK         (1<<12)
#  define ATH_PORT_CTRL_SINGLE_VLAN_EN        (1<<13)
#  define ATH_PORT_CTRL_LEARN_EN              (1<<14)
#  define ATH_PORT_CTRL_EG_MIRROR_EN          (1<<16)
#  define ATH_PORT_CTRL_ING_MIRROR_EN         (1<<17)

/*--- PORT-VLAN ---*/
#define AR8216_PORT_VLAN_OFFSET            0x08
#  define ATH_PORT_VLAN_VID_SHIFT             0
#  define ATH_PORT_VLAN_VID_MASK              (((1<<12)-1)<<ATH_PORT_VLAN_VID_SHIFT)               
#  define ATH_PORT_VLAN_VID_VALUE(value)      (((value)<<ATH_PORT_VLAN_VID_SHIFT)&ATH_PORT_VLAN_VID_MASK)           
#  define ATH_PORT_VLAN_FORCE_DEFAULT_VID_EN  (1u << 12)
#  define ATH_PORT_VLAN_VID_MEM_SHIFT         16
#  define ATH_PORT_VLAN_VID_MEM_MASK          (((1<<7)-1)<<ATH_PORT_VLAN_VID_MEM_SHIFT)               
#  define ATH_PORT_VLAN_VID_MEM_VALUE(value)  (((value)<<ATH_PORT_VLAN_VID_MEM_SHIFT)&ATH_PORT_VLAN_VID_MEM_MASK)           
#  define ATH_PORT_VLAN_EG_PRIO_0             (1<<27)
#  define ATH_PORT_VLAN_FORCE_PORT_VLAN_EN    (1u << 26)
#  define ATH_PORT_VLAN_ING_PRI_SHIFT         28
#  define ATH_PORT_VLAN_ING_PRI_MASK          (((1<<2)-1)<<ATH_PORT_VLAN_ING_PRI_SHIFT)               
#  define ATH_PORT_VLAN_ING_PRI_VALUE(value)  (((value)<<ATH_PORT_VLAN_ING_PRI_SHIFT)&ATH_PORT_VLAN_ING_PRI_MASK)           
#  define ATH_PORT_VLAN_8021_QMODE_SHIFT      30
#  define ATH_PORT_VLAN_8021_QMODE_MASK          (((1<<2)-1)<<ATH_PORT_VLAN_8021_QMODE_SHIFT)               
#  define ATH_PORT_VLAN_8021_QMODE_VALUE(value)  (((value)<<ATH_PORT_VLAN_8021_QMODE_SHIFT)&ATH_PORT_VLAN_8021_QMODE_MASK)           
#    define ATH_PORT_VLAN_8021_QMODE_DISABLE  0
#    define ATH_PORT_VLAN_8021_QMODE_FALLBACK 1
#    define ATH_PORT_VLAN_8021_QMODE_CHECK    2
#    define ATH_PORT_VLAN_8021_QMODE_SECURE   3

/*--- PORT-VLAN for AR8226 ---*/
#  define ATH_PORT_VLAN_CVID_SHIFT             16
#  define ATH_PORT_VLAN_CVID_MASK              (((1u << 12) - 1) << ATH_PORT_VLAN_CVID_SHIFT)               
#  define ATH_PORT_VLAN_CVID_VALUE(value)      (((value) << ATH_PORT_VLAN_CVID_SHIFT) & ATH_PORT_VLAN_CVID_MASK)           

/*--- PORT-VLAN2 ---*/
#define AR8226_PORT_VLAN2_OFFSET           0x0C
#  define ATH_PORT_VLAN2_VID_MEM_SHIFT         16
#  define ATH_PORT_VLAN2_VID_MEM_MASK          (((1<<7)-1)<<ATH_PORT_VLAN2_VID_MEM_SHIFT)               
#  define ATH_PORT_VLAN2_VID_MEM_VALUE(value)  (((value)<<ATH_PORT_VLAN2_VID_MEM_SHIFT)&ATH_PORT_VLAN2_VID_MEM_MASK)           
#  define ATH_PORT_VLAN2_8021_QMODE_SHIFT      30
#  define ATH_PORT_VLAN2_8021_QMODE_MASK          (((1<<2)-1)<<ATH_PORT_VLAN2_8021_QMODE_SHIFT)               
#  define ATH_PORT_VLAN2_8021_QMODE_VALUE(value)  (((value)<<ATH_PORT_VLAN2_8021_QMODE_SHIFT)&ATH_PORT_VLAN2_8021_QMODE_MASK)           
#    define ATH_PORT_VLAN2_8021_QMODE_DISABLE  0
#    define ATH_PORT_VLAN2_8021_QMODE_FALLBACK 1
#    define ATH_PORT_VLAN2_8021_QMODE_CHECK    2
#    define ATH_PORT_VLAN2_8021_QMODE_SECURE   3
/*--- PORT-PRIORITY ---*/
#define AR8216_PORT_PRIORITY_OFFSET        0x10


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AR8327_PORT0_PAD_MODE_CTRL      0x04
#  define AR8327_PORT0_MAC_MII_RXCLK_SEL      (1u <<  0)
#  define AR8327_PORT0_MAC_MII_TXCLK_SEL      (1u <<  1)
#  define AR8327_PORT0_MAC_MII_EN             (1u <<  2)
#  define AR8327_PORT0_MAC_GMII_RXCLK_SEL     (1u <<  4)
#  define AR8327_PORT0_MAC_GMII_TXCLK_SEL     (1u <<  5)
#  define AR8327_PORT0_MAC_GMII_EN            (1u <<  6)
#  define AR8327_PORT0_MAC_SGMII_EN           (1u <<  7)
#  define AR8327_PORT0_PHY_MII_RXCLK_SEL      (1u <<  8)
#  define AR8327_PORT0_PHY_MII_TXCLK_SEL      (1u <<  9)
#  define AR8327_PORT0_PHY_MII_EN             (1u << 10)
#  define AR8327_PORT0_PHY_MII_PIPE_RXCLK_SEL (1u << 11)
#  define AR8327_PORT0_PHY_GMII_RXCLK_SEL     (1u << 12)
#  define AR8327_PORT0_PHY_GMII_TXCLK_SEL     (1u << 13)
#  define AR8327_PORT0_PHY_GMII_EN            (1u << 14)
#  define AR8327_PORT0_SGMII_CLK125M_TX_SEL   (1u << 18)
#  define AR8327_PORT0_SGMII_CLK125M_RX_SEL   (1u << 19)
#  define AR8327_PORT0_RGMII_RXCLK_DELAY_SEL_SHIFT         20
#  define AR8327_PORT0_RGMII_RXCLK_DELAY_SEL_MASK          (((1u << 2) - 1) << AR8327_PORT0_RGMII_RXCLK_DELAY_SEL_SHIFT)               
#  define AR8327_PORT0_RGMII_RXCLK_DELAY_SEL_VALUE(value)  (((value) << AR8327_PORT0_RGMII_RXCLK_DELAY_SEL_SHIFT) & AR8327_PORT0_RGMII_RXCLK_DELAY_SEL_MASK)           
#  define AR8327_PORT0_RGMII_TXCLK_DELAY_SEL_SHIFT         22
#  define AR8327_PORT0_RGMII_TXCLK_DELAY_SEL_MASK          (((1u << 2) - 1) << AR8327_PORT0_RGMII_TXCLK_DELAY_SEL_SHIFT)               
#  define AR8327_PORT0_RGMII_TXCLK_DELAY_SEL_VALUE(value)  (((value) << AR8327_PORT0_RGMII_TXCLK_DELAY_SEL_SHIFT) & AR8327_PORT0_RGMII_TXCLK_DELAY_SEL_MASK)           
#  define AR8327_PORT0_MAC_RGMII_RXCLK_SEL    (1u << 24)
#  define AR8327_PORT0_MAC_RGMII_TXCLK_SEL    (1u << 25)
#  define AR8327_PORT0_MAC_RGMII_EN           (1u << 26)

#define AR8327_PORT5_PAD_MODE_CTRL      0x08
#  define AR8327_PORT5_MAC_MII_RXCLK_SEL      (1u <<  0)
#  define AR8327_PORT5_MAC_MII_TXCLK_SEL      (1u <<  1)
#  define AR8327_PORT5_MAC_MII_EN             (1u <<  2)
#  define AR8327_PORT5_PHY_MII_RXCLK_SEL      (1u <<  8)
#  define AR8327_PORT5_PHY_MII_TXCLK_SEL      (1u <<  9)
#  define AR8327_PORT5_PHY_MII_EN             (1u << 10)
#  define AR8327_PORT5_PHY_MII_PIPE_RXCLK_SEL (1u << 11)
#  define AR8327_PORT5_RGMII_RXCLK_DELAY_SEL_SHIFT         20
#  define AR8327_PORT5_RGMII_RXCLK_DELAY_SEL_MASK          (((1u << 2) - 1) << AR8327_PORT5_RGMII_RXCLK_DELAY_SEL_SHIFT)               
#  define AR8327_PORT5_RGMII_RXCLK_DELAY_SEL_VALUE(value)  (((value) << AR8327_PORT5_RGMII_RXCLK_DELAY_SEL_SHIFT) & AR8327_PORT5_RGMII_RXCLK_DELAY_SEL_MASK)           
#  define AR8327_PORT5_RGMII_TXCLK_DELAY_SEL_SHIFT         22
#  define AR8327_PORT5_RGMII_TXCLK_DELAY_SEL_MASK          (((1u << 2) - 1) << AR8327_PORT5_RGMII_TXCLK_DELAY_SEL_SHIFT)               
#  define AR8327_PORT5_RGMII_TXCLK_DELAY_SEL_VALUE(value)  (((value) << AR8327_PORT5_RGMII_TXCLK_DELAY_SEL_SHIFT) & AR8327_PORT5_RGMII_TXCLK_DELAY_SEL_MASK)           
#  define AR8327_PORT5_MAC_RGMII_RXCLK_SEL    (1u << 24)
#  define AR8327_PORT5_MAC_RGMII_TXCLK_SEL    (1u << 25)
#  define AR8327_PORT5_MAC_RGMII_EN           (1u << 26)

#define AR8327_PORT6_PAD_MODE_CTRL      0x0C
#  define AR8327_PORT6_MAC_MII_RXCLK_SEL      (1u <<  0)
#  define AR8327_PORT6_MAC_MII_TXCLK_SEL      (1u <<  1)
#  define AR8327_PORT6_MAC_MII_EN             (1u <<  2)
#  define AR8327_PORT6_MAC_GMII_RXCLK_SEL     (1u <<  4)
#  define AR8327_PORT6_MAC_GMII_TXCLK_SEL     (1u <<  5)
#  define AR8327_PORT6_MAC_GMII_EN            (1u <<  6)
#  define AR8327_PORT6_MAC_SGMII_EN           (1u <<  7)
#  define AR8327_PORT6_PHY_MII_RXCLK_SEL      (1u <<  8)
#  define AR8327_PORT6_PHY_MII_TXCLK_SEL      (1u <<  9)
#  define AR8327_PORT6_PHY_MII_EN             (1u << 10)
#  define AR8327_PORT6_PHY_MII_PIPE_RXCLK_SEL (1u << 11)
#  define AR8327_PORT6_PHY_GMII_RXCLK_SEL     (1u << 12)
#  define AR8327_PORT6_PHY_GMII_TXCLK_SEL     (1u << 13)
#  define AR8327_PORT6_PHY_GMII_EN            (1u << 14)
#  define AR8327_PORT6_PHY4_GMII_EN           (1u << 16)
#  define AR8327_PORT6_PHY4_RGMII_EN          (1u << 17)
#  define AR8327_PORT6_PHY4_MII_EN            (1u << 18)
#  define AR8327_PORT6_RGMII_RXCLK_DELAY_SEL_SHIFT         20
#  define AR8327_PORT6_RGMII_RXCLK_DELAY_SEL_MASK          (((1u << 2) - 1) << AR8327_PORT6_RGMII_RXCLK_DELAY_SEL_SHIFT)               
#  define AR8327_PORT6_RGMII_RXCLK_DELAY_SEL_VALUE(value)  (((value) << AR8327_PORT6_RGMII_RXCLK_DELAY_SEL_SHIFT) & AR8327_PORT6_RGMII_RXCLK_DELAY_SEL_MASK)           
#  define AR8327_PORT6_RGMII_TXCLK_DELAY_SEL_SHIFT         22
#  define AR8327_PORT6_RGMII_TXCLK_DELAY_SEL_MASK          (((1u << 2) - 1) << AR8327_PORT6_RGMII_TXCLK_DELAY_SEL_SHIFT)               
#  define AR8327_PORT6_RGMII_TXCLK_DELAY_SEL_VALUE(value)  (((value) << AR8327_PORT6_RGMII_TXCLK_DELAY_SEL_SHIFT) & AR8327_PORT6_RGMII_TXCLK_DELAY_SEL_MASK)           
#  define AR8327_PORT6_MAC_RGMII_RXCLK_SEL    (1u << 24)
#  define AR8327_PORT6_MAC_RGMII_TXCLK_SEL    (1u << 25)
#  define AR8327_PORT6_MAC_RGMII_EN           (1u << 26)

#define AR8327_PWS                      0x10
#  define AR8327_PWS_LED_OPEN_EN_CSR    (1u << 24)
#  define AR8327_PWS_SPI_EN_CSR         (1u << 25)
#  define AR8327_PWS_INPUT_MODE         (1u << 27)
#  define AR8327_PWS_PACKAGE148_EN      (1u << 30)
#  define AR8327_PWS_POWER_ON_SEL       (1u << 31)

#define AR8327_MODULE_EN                0x30
#  define AR8327_MODULE_EN_MIB_EN       (1u << 0)
#  define AR8327_MODULE_EN_ACL_EN       (1u << 1)
#  define AR8327_MODULE_EN_L3_EN        (1u << 2)

#define AR8327_FRAM_ACK_CTRL0           0x210
#  define AR8327_FRAM_ACK_CTRL0_IGMP_MLD_EN(port)    (1u << (0 + 8 * (port)))
#  define AR8327_FRAM_ACK_CTRL0_IGMP_JOIN_EN(port)   (1u << (1 + 8 * (port)))
#  define AR8327_FRAM_ACK_CTRL0_IGMP_LEAVE_EN(port)  (1u << (2 + 8 * (port)))
#  define AR8327_FRAM_ACK_CTRL0_EAPOL_EN(port)       (1u << (3 + 8 * (port)))
#  define AR8327_FRAM_ACK_CTRL0_DHCP_EN(port)        (1u << (4 + 8 * (port)))
#  define AR8327_FRAM_ACK_CTRL0_ARP_ACK_EN(port)     (1u << (5 + 8 * (port)))
#  define AR8327_FRAM_ACK_CTRL0_ARP_REQ_EN(port)     (1u << (6 + 8 * (port)))

#define AR8327_FRAM_ACK_CTRL1           0x214
#  define AR8327_FRAM_ACK_CTRL1_IGMP_MLD_EN(port)    (1u << (0 + 8 * ((port) - 4)))
#  define AR8327_FRAM_ACK_CTRL1_IGMP_JOIN_EN(port)   (1u << (1 + 8 * ((port) - 4)))
#  define AR8327_FRAM_ACK_CTRL1_IGMP_LEAVE_EN(port)  (1u << (2 + 8 * ((port) - 4)))
#  define AR8327_FRAM_ACK_CTRL1_EAPOL_EN(port)       (1u << (3 + 8 * ((port) - 4)))
#  define AR8327_FRAM_ACK_CTRL1_DHCP_EN(port)        (1u << (4 + 8 * ((port) - 4)))
#  define AR8327_FRAM_ACK_CTRL1_ARP_ACK_EN(port)     (1u << (5 + 8 * ((port) - 4)))
#  define AR8327_FRAM_ACK_CTRL1_ARP_REQ_EN(port)     (1u << (6 + 8 * ((port) - 4)))

#define AR8327_PORT_VLAN_CTRL0(port)    (0x420 + 8 * (port))
#  define AR8327_PORT_VLAN_CTRL0_DEFAULT_SVID_SHIFT          0
#  define AR8327_PORT_VLAN_CTRL0_DEFAULT_SVID_MASK           (((1u << 12) - 1) << AR8327_PORT_VLAN_CTRL0_DEFAULT_SVID_SHIFT)               
#  define AR8327_PORT_VLAN_CTRL0_DEFAULT_SVID_VALUE(value)   (((value) << AR8327_PORT_VLAN_CTRL0_DEFAULT_SVID_SHIFT) & AR8327_PORT_VLAN_CTRL0_DEFAULT_SVID_MASK)           
#  define AR8327_PORT_VLAN_CTRL0_ING_PORT_SPRI_SHIFT         13
#  define AR8327_PORT_VLAN_CTRL0_ING_PORT_SPRI_MASK          (((1u << 3) - 1) << AR8327_PORT_VLAN_CTRL0_ING_PORT_SPRI_SHIFT)               
#  define AR8327_PORT_VLAN_CTRL0_ING_PORT_SPRI_VALUE(value)  (((value) << AR8327_PORT_VLAN_CTRL0_ING_PORT_SPRI_SHIFT) & AR8327_PORT_VLAN_CTRL0_ING_PORT_SPRI_MASK)           
#  define AR8327_PORT_VLAN_CTRL0_DEFAULT_CVID_SHIFT          16
#  define AR8327_PORT_VLAN_CTRL0_DEFAULT_CVID_MASK           (((1u << 12) - 1) << AR8327_PORT_VLAN_CTRL0_DEFAULT_CVID_SHIFT)               
#  define AR8327_PORT_VLAN_CTRL0_DEFAULT_CVID_VALUE(value)   (((value) << AR8327_PORT_VLAN_CTRL0_DEFAULT_CVID_SHIFT) & AR8327_PORT_VLAN_CTRL0_DEFAULT_CVID_MASK)           
#  define AR8327_PORT_VLAN_CTRL0_ING_PORT_CPRI_SHIFT         29
#  define AR8327_PORT_VLAN_CTRL0_ING_PORT_CPRI_MASK          (((1u << 3) - 1) << AR8327_PORT_VLAN_CTRL0_ING_PORT_CPRI_SHIFT)               
#  define AR8327_PORT_VLAN_CTRL0_ING_PORT_CPRI_VALUE(value)  (((value) << AR8327_PORT_VLAN_CTRL0_ING_PORT_CPRI_SHIFT) & AR8327_PORT_VLAN_CTRL0_ING_PORT_CPRI_MASK)           

#define AR8327_PORT_VLAN_CTRL1(port)         (0x424 + 8 * (port))
#  define AR8327_PORT_VLAN_CTRL1_ING_VLAN_MODE_SHIFT         2
#  define AR8327_PORT_VLAN_CTRL1_ING_VLAN_MODE_MASK          (((1u << 2) - 1) << AR8327_PORT_VLAN_CTRL1_ING_VLAN_MODE_SHIFT)               
#  define AR8327_PORT_VLAN_CTRL1_ING_VLAN_MODE_VALUE(value)  (((value) << AR8327_PORT_VLAN_CTRL1_ING_VLAN_MODE_SHIFT) & AR8327_PORT_VLAN_CTRL1_ING_VLAN_MODE_MASK)           
#    define AR8327_PORT_VLAN_CTRL1_ING_VLAN_MODE_VALUE_ALL_FRAMES    0
#    define AR8327_PORT_VLAN_CTRL1_ING_VLAN_MODE_VALUE_ONLY_TAGGED   1
#    define AR8327_PORT_VLAN_CTRL1_ING_VLAN_MODE_VALUE_ONLY_UNTAGGED 2
#  define AR8327_PORT_VLAN_CTRL1_VLAN_PRI_PROP_EN            (1u <<  4)
#  define AR8327_PORT_VLAN_CTRL1_PORT_CLONE_EN               (1u <<  5)
#  define AR8327_PORT_VLAN_CTRL1_PORT_VLAN_PROP_EN           (1u <<  6)
#  define AR8327_PORT_VLAN_CTRL1_PORT_TLS_MODE_EN            (1u <<  7)
#  define AR8327_PORT_VLAN_CTRL1_FORCE_DEFAULT_VID_EN        (1u <<  8)
#  define AR8327_PORT_VLAN_CTRL1_CORE_PORT_EN                (1u <<  9)
#  define AR8327_PORT_VLAN_CTRL1_SPCHECK_EN                  (1u << 10)
#  define AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_SHIFT          12
#  define AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_MASK           (((1u << 2) - 1) << AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_SHIFT)               
#  define AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_VALUE(value)   (((value) << AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_SHIFT) & AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_MASK)           
#    define AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_VALUE_UNMODIFIED  0
#    define AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_VALUE_UNTAGGED    1
#    define AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_VALUE_TAGGED      2
#    define AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_VALUE_UNTOUCHED   3
#  define AR8327_PORT_VLAN_CTRL1_EG_VLAN_TYPE                (1u << 14)


#define AR8327_ATU_DATA0            0x600
#define AR8327_ATU_DATA1            0x604
#  define AR8327_ATU_DATA1_ATU_HASH_HIGH_ADDR            (1u << 31)
#  define AR8327_ATU_DATA1_ATU_SA_DROP_EN                (1u << 30)
#  define AR8327_ATU_DATA1_ATU_MIRROR_EN                 (1u << 29)
#  define AR8327_ATU_DATA1_ATU_PRI_OVER_EN               (1u << 28)
#  define AR8327_ATU_DATA1_ATU_SVL_ENTRY                 (1u << 27)
#  define AR8327_ATU_DATA1_ATU_PRI_SHIFT                 24
#  define AR8327_ATU_DATA1_ATU_PRI_MASK                  (((1u << 3) - 1) << AR8327_ATU_DATA1_ATU_PRI_SHIFT)               
#  define AR8327_ATU_DATA1_ATU_PRI_VALUE(value)          (((value) << AR8327_ATU_DATA1_ATU_PRI_SHIFT) & AR8327_ATU_DATA1_ATU_PRI_MASK)           
#  define AR8327_ATU_DATA1_ATU_CROSS_PORT_STATE_EN       (1u << 23)
#  define AR8327_ATU_DATA1_ATU_DES_PORT_SHIFT            16
#  define AR8327_ATU_DATA1_ATU_DES_PORT_MASK             (((1u << 7) - 1) << AR8327_ATU_DATA1_ATU_DES_PORT_SHIFT)               
#  define AR8327_ATU_DATA1_ATU_DES_PORT_VALUE(value)     (((value) << AR8327_ATU_DATA1_ATU_DES_PORT_SHIFT) & AR8327_ATU_DATA1_ATU_DES_PORT_MASK)           
#  define AR8327_ATU_DATA1_ATU_MAC_ADDR1_SHIFT           0
#  define AR8327_ATU_DATA1_ATU_MAC_ADDR1_MASK            (((1u << 16) - 1) << AR8327_ATU_DATA1_ATU_MAC_ADDR1_SHIFT)               
#  define AR8327_ATU_DATA1_ATU_MAC_ADDR1_VALUE(value)    (((value) << AR8327_ATU_DATA1_ATU_MAC_ADDR1_SHIFT) & AR8327_ATU_DATA1_ATU_MAC_ADDR1_MASK)           
#define AR8327_ATU_DATA2            0x608
#  define AR8327_ATU_DATA2_ATU_VID_SHIFT                 8
#  define AR8327_ATU_DATA2_ATU_VID_MASK                  (((1u << 12) - 1) << AR8327_ATU_DATA2_ATU_VID_SHIFT)               
#  define AR8327_ATU_DATA2_ATU_VID_VALUE(value)          (((value) << AR8327_ATU_DATA2_ATU_VID_SHIFT) & AR8327_ATU_DATA2_ATU_VID_MASK)           
#  define AR8327_ATU_DATA2_ATU_SHORT_LOOP                (1u << 7)
#  define AR8327_ATU_DATA2_ATU_COPY_TO_CPU               (1u << 6)
#  define AR8327_ATU_DATA2_ATU_REDIRECT_TO_CPU           (1u << 5)
#  define AR8327_ATU_DATA2_ATU_LEAKY_EN                  (1u << 4)
#  define AR8327_ATU_DATA2_ATU_STATUS_SHIFT              0
#  define AR8327_ATU_DATA2_ATU_STATUS_MASK               (((1u << 4) - 1) << AR8327_ATU_DATA2_ATU_STATUS_SHIFT)               
#  define AR8327_ATU_DATA2_ATU_STATUS_VALUE(value)       (((value) << AR8327_ATU_DATA2_ATU_STATUS_SHIFT) & AR8327_ATU_DATA2_ATU_STATUS_MASK)           

#define AR8327_ATU_FUNC             0x60c
#  define AR8327_ATU_FUNC_BUSY                           (1u << 31)
#  define AR8327_ATU_FUNC_TRUNK_PORT_NUM_SHIFT           22
#  define AR8327_ATU_FUNC_TRUNK_PORT_NUM_MASK            (((1u << 3) - 1) << AR8327_ATU_FUNC_TRUNK_PORT_NUM_SHIFT)               
#  define AR8327_ATU_FUNC_TRUNK_PORT_NUM_VALUE(value)    (((value) << AR8327_ATU_FUNC_TRUNK_PORT_NUM_SHIFT) & AR8327_ATU_FUNC_TRUNK_PORT_NUM_MASK)           
#  define AR8327_ATU_FUNC_ATU_INDEX_SHIFT                16
#  define AR8327_ATU_FUNC_ATU_INDEX_MASK                 (((1u << 5) - 1) << AR8327_ATU_FUNC_ATU_INDEX_SHIFT)               
#  define AR8327_ATU_FUNC_ATU_INDEX_VALUE(value)         (((value) << AR8327_ATU_FUNC_ATU_INDEX_SHIFT) & AR8327_ATU_FUNC_ATU_INDEX_MASK)           
#  define AR8327_ATU_FUNC_AT_VID_EN                      (1u << 15)
#  define AR8327_ATU_FUNC_AT_PORT_EN                     (1u << 14)
#  define AR8327_ATU_FUNC_AT_MULTI_EN                    (1u << 13)
#  define AR8327_ATU_FUNC_AT_FULL_VIO                    (1u << 12)
#  define AR8327_ATU_FUNC_AT_PORT_NUM_SHIFT              8
#  define AR8327_ATU_FUNC_AT_PORT_NUM_MASK               (((1u << 4) - 1) << AR8327_ATU_FUNC_AT_PORT_NUM_SHIFT)               
#  define AR8327_ATU_FUNC_AT_PORT_NUM_VALUE(value)       (((value) << AR8327_ATU_FUNC_AT_PORT_NUM_SHIFT) & AR8327_ATU_FUNC_AT_PORT_NUM_MASK)           
#  define AR8327_ATU_FUNC_ATU_TYPE                       (1u <<  5)
#  define AR8327_ATU_FUNC_FLUSH_STATIC_EN                (1u <<  4)
#  define AR8327_ATU_FUNC_FUNC_SHIFT                     0
#  define AR8327_ATU_FUNC_FUNC_MASK                      (((1u << 4) - 1) << AR8327_ATU_FUNC_FUNC_SHIFT)               
#  define AR8327_ATU_FUNC_FUNC_VALUE(value)              (((value)<< AR8327_ATU_FUNC_FUNC_SHIFT) & AR8327_ATU_FUNC_FUNC_MASK)           
#    define AR8327_ATU_FUNC_FUNC_VALUE_NOP               0
#    define AR8327_ATU_FUNC_FUNC_VALUE_FLUSH_ALL         1
#    define AR8327_ATU_FUNC_FUNC_VALUE_LOAD              2
#    define AR8327_ATU_FUNC_FUNC_VALUE_PURGE             3
#    define AR8327_ATU_FUNC_FUNC_VALUE_FLUSH_UNLOCKED    4
#    define AR8327_ATU_FUNC_FUNC_VALUE_FLUSH_PORT        5
#    define AR8327_ATU_FUNC_FUNC_VALUE_NEXT              6
#    define AR8327_ATU_FUNC_FUNC_VALUE_SEARCH_MAC        7
#    define AR8327_ATU_FUNC_FUNC_VALUE_CHANGE_TRUNK_PORT 8

#define AR8327_VTU_FUNC0            0x610
#  define AR8327_VTU_FUNC0_VALID                           (1u << 20)
#  define AR8327_VTU_FUNC0_IVL_EN                          (1u << 19)
#  define AR8327_VTU_FUNC0_LEARN_LOOKUP_DIS                (1u << 18)
#  define AR8327_VTU_FUNC0_EG_VLAN_MODE_SHIFT(port)        (4 + 2 * (port))
#  define AR8327_VTU_FUNC0_EG_VLAN_MODE_MASK(port)         (((1u << 2) - 1) << AR8327_VTU_FUNC0_EG_VLAN_MODE_SHIFT(port))               
#  define AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE(port, value) (((value) << AR8327_VTU_FUNC0_EG_VLAN_MODE_SHIFT(port)) & AR8327_VTU_FUNC0_EG_VLAN_MODE_MASK(port))
#    define AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE_UNMODIFIED 0
#    define AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE_UNTAGGED   1
#    define AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE_TAGGED     2
#    define AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE_NOT_MEMBER 3
#  define AR8327_VTU_FUNC0_PRI_OVER_EN                     (1u << 3)
#  define AR8327_VTU_FUNC0_PRI_SHIFT                       0
#  define AR8327_VTU_FUNC0_PRI_MASK                        (((1u << 3) - 1) << AR8327_VTU_FUNC0_VTU_PRI_SHIFT)               
#  define AR8327_VTU_FUNC0_PRI_VALUE(value)                (((value) << AR8327_VTU_FUNC0_VTU_PRI_SHIFT) & AR8327_VTU_FUNC0_VTU_PRI_MASK)           

#define AR8327_VTU_FUNC1            0x614
#  define AR8327_VTU_FUNC1_BUSY                            (1u << 31)
#  define AR8327_VTU_FUNC1_VID_SHIFT                       16
#  define AR8327_VTU_FUNC1_VID_MASK                        (((1u << 12) - 1) << AR8327_VTU_FUNC1_VID_SHIFT)               
#  define AR8327_VTU_FUNC1_VID_VALUE(value)                (((value) << AR8327_VTU_FUNC1_VID_SHIFT) & AR8327_VTU_FUNC1_VID_MASK)           
#  define AR8327_VTU_FUNC1_VT_PORT_NUM_SHIFT               8
#  define AR8327_VTU_FUNC1_VT_PORT_NUM_MASK                (((1u << 4) - 1) << AR8327_VTU_FUNC1_VT_PORT_NUM_SHIFT)               
#  define AR8327_VTU_FUNC1_VT_PORT_NUM_VALUE(value)        (((value) << AR8327_VTU_FUNC1_VT_PORT_NUM_SHIFT) & AR8327_VTU_FUNC1_VT_PORT_NUM_MASK)           
#  define AR8327_VTU_FUNC1_FULL_VIO                        (1u <<  4)
#  define AR8327_VTU_FUNC1_FUNC_SHIFT                      0
#  define AR8327_VTU_FUNC1_FUNC_MASK                       (((1u << 3) - 1) << AR8327_VTU_FUNC1_FUNC_SHIFT)               
#  define AR8327_VTU_FUNC1_FUNC_VALUE(value)               (((value) << AR8327_VTU_FUNC1_FUNC_SHIFT) & AR8327_VTU_FUNC1_FUNC_MASK)           
#    define AR8327_VTU_FUNC1_FUNC_VALUE_NOP                0
#    define AR8327_VTU_FUNC1_FUNC_VALUE_FLUSH_ALL          1
#    define AR8327_VTU_FUNC1_FUNC_VALUE_LOAD_ENTRY         2
#    define AR8327_VTU_FUNC1_FUNC_VALUE_PURGE_ENTRY        3
#    define AR8327_VTU_FUNC1_FUNC_VALUE_REMOVE_PORT        4
#    define AR8327_VTU_FUNC1_FUNC_VALUE_GET_NEXT           5
#    define AR8327_VTU_FUNC1_FUNC_VALUE_READ_ENTRY         6

#define AR8327_ARL_CTRL             0x618
#  define AR8327_ARL_CTRL_AGE_TIME_SHIFT                   0
#  define AR8327_ARL_CTRL_AGE_TIME_MASK                    (((1u << 16) - 1) << AR8327_ARL_CTRL_AGE_TIME_SHIFT)               
#  define AR8327_ARL_CTRL_AGE_TIME_VALUE(value)            (((value) << AR8327_ARL_CTRL_AGE_TIME_SHIFT) & AR8327_ARL_CTRL_AGE_TIME_MASK)           
#  define AR8327_ARL_CTRL_LOOP_CHECK_TIMER_SHIFT           16
#  define AR8327_ARL_CTRL_LOOP_CHECK_TIMER_MASK            (((1u << 3) - 1) << AR8327_ARL_CTRL_LOOP_CHECK_TIMER_SHIFT)               
#  define AR8327_ARL_CTRL_LOOP_CHECK_TIMER_VALUE(value)    (((value) << AR8327_ARL_CTRL_LOOP_CHECK_TIMER_SHIFT) & AR8327_ARL_CTRL_LOOP_CHECK_TIMER_MASK)           
#  define AR8327_ARL_CTRL_AGE_EN                           (1u << 19)
#  define AR8327_ARL_CTRL_IGMP_JOIN_STATUS_SHIFT           20
#  define AR8327_ARL_CTRL_IGMP_JOIN_STATUS_MASK            (((1u << 4) - 1) << AR8327_ARL_CTRL_IGMP_JOIN_STATUS_SHIFT)               
#  define AR8327_ARL_CTRL_IGMP_JOIN_STATUS_VALUE(value)    (((value) << AR8327_ARL_CTRL_IGMP_JOIN_STATUS_SHIFT) & AR8327_ARL_CTRL_IGMP_JOIN_STATUS_MASK)           
#  define AR8327_ARL_CTRL_IGMP_JOIN_PRI_SHIFT              24
#  define AR8327_ARL_CTRL_IGMP_JOIN_PRI_MASK               (((1u << 3) - 1) << AR8327_ARL_CTRL_IGMP_JOIN_PRI_SHIFT)               
#  define AR8327_ARL_CTRL_IGMP_JOIN_PRI_VALUE(value)       (((value) << AR8327_ARL_CTRL_IGMP_JOIN_PRI_SHIFT) & AR8327_ARL_CTRL_IGMP_JOIN_PRI_MASK)           
#  define AR8327_ARL_CTRL_IGMP_JOIN_PRI_REMAP_EN           (1u << 27)
#  define AR8327_ARL_CTRL_IGMP_JOIN_NEW_EN                 (1u << 28)
#  define AR8327_ARL_CTRL_IGMP_JOIN_LEAKY_EN               (1u << 29)
#  define AR8327_ARL_CTRL_LEARN_CHANGE_EN                  (1u << 30)

#define AR8327_GLOBAL_FW_CTRL0      0x620
#  define AR8327_GLOBAL_FW_CTRL0_EAPOL_REDIRECT_EN                        (1u <<  0)
#  define AR8327_GLOBAL_FW_CTRL0_RIP_COPY_EN                              (1u <<  2)
#  define AR8327_GLOBAL_FW_CTRL0_IGMP_COPY_EN                             (1u <<  3)
#  define AR8327_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM_SHIFT                    4
#  define AR8327_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM_MASK                     (((1u << 4) - 1) << AR8327_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM_SHIFT)
#  define AR8327_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM_VALUE(value)             (((value) << AR8327_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM_SHIFT) & AR8327_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM_MASK)
#  define AR8327_GLOBAL_FW_CTRL0_PPPOE_REDIRECT_EN                        (1u <<  8)
#  define AR8327_GLOBAL_FW_CTRL0_CPU_PORT_EN                              (1u << 10)
#  define AR8327_GLOBAL_FW_CTRL0_MANAGE_VID_VIO_DROP_EN                   (1u << 11)
#  define AR8327_GLOBAL_FW_CTRL0_ARL_MULTI_LEAKY_EN                       (1u << 12)
#  define AR8327_GLOBAL_FW_CTRL0_ARL_UNI_LEAKY_EN                         (1u << 13)
#  define AR8327_GLOBAL_FW_CTRL0_IGMP_LEAVE_DROP_EN                       (1u << 14)
#  define AR8327_GLOBAL_FW_CTRL0_NAT_NOT_FOUND_DROP_EN                    (1u << 17)
#  define AR8327_GLOBAL_FW_CTRL0_ARP_REQ_UNI                              (1u << 19)
#  define AR8327_GLOBAL_FW_CTRL0_HASH_MODE_SHIFT                          20
#  define AR8327_GLOBAL_FW_CTRL0_HASH_MODE_MASK                           (((1u << 2) - 1) << AR8327_GLOBAL_FW_CTRL0_HASH_MODE_SHIFT)
#  define AR8327_GLOBAL_FW_CTRL0_HASH_MODE_VALUE(value)                   (((value) << AR8327_GLOBAL_FW_CTRL0_HASH_MODE_SHIFT) & AR8327_GLOBAL_FW_CTRL0_HASH_MODE_MASK)
#    define AR8327_GLOBAL_FW_CTRL0_HASH_MODE_VALUE_CRC_16                 0
#    define AR8327_GLOBAL_FW_CTRL0_HASH_MODE_VALUE_CRC_10                 1
#  define AR8327_GLOBAL_FW_CTRL0_ARP_SP_NOT_FOUND_ACT_SHIFT               22
#  define AR8327_GLOBAL_FW_CTRL0_ARP_SP_NOT_FOUND_ACT_MASK                (((1u << 2) - 1) << AR8327_GLOBAL_FW_CTRL0_ARP_SP_NOT_FOUND_ACT_SHIFT)
#  define AR8327_GLOBAL_FW_CTRL0_ARP_SP_NOT_FOUND_ACT_VALUE(value)        (((value) << AR8327_GLOBAL_FW_CTRL0_ARP_SP_NOT_FOUND_ACT_SHIFT) & AR8327_GLOBAL_FW_CTRL0_ARP_SP_NOT_FOUND_ACT_MASK)
#    define AR8327_GLOBAL_FW_CTRL0_ARP_SP_NOT_FOUND_ACT_VALUE_FORWARD     0
#    define AR8327_GLOBAL_FW_CTRL0_ARP_SP_NOT_FOUND_ACT_VALUE_DROP        1
#    define AR8327_GLOBAL_FW_CTRL0_ARP_SP_NOT_FOUND_ACT_VALUE_TO_CPU      2
#  define AR8327_GLOBAL_FW_CTRL0_SP_NOT_FOUND_ACT_SHIFT                   24
#  define AR8327_GLOBAL_FW_CTRL0_SP_NOT_FOUND_ACT_MASK                    (((1u << 2) - 1) << AR8327_GLOBAL_FW_CTRL0_SP_NOT_FOUND_ACT_SHIFT)
#  define AR8327_GLOBAL_FW_CTRL0_SP_NOT_FOUND_ACT_VALUE(value)            (((value) << AR8327_GLOBAL_FW_CTRL0_SP_NOT_FOUND_ACT_SHIFT) & AR8327_GLOBAL_FW_CTRL0_SP_NOT_FOUND_ACT_MASK)
#    define AR8327_GLOBAL_FW_CTRL0_SP_NOT_FOUND_ACT_VALUE_FORWARD         0
#    define AR8327_GLOBAL_FW_CTRL0_SP_NOT_FOUND_ACT_VALUE_DROP            1
#    define AR8327_GLOBAL_FW_CTRL0_SP_NOT_FOUND_ACT_VALUE_TO_CPU          2
#  define AR8327_GLOBAL_FW_CTRL0_ARP_FORWARD_ACT_SHIFT                    26
#  define AR8327_GLOBAL_FW_CTRL0_ARP_FORWARD_ACT_MASK                     (((1u << 2) - 1) << AR8327_GLOBAL_FW_CTRL0_ARP_FORWARD_ACT_SHIFT)
#  define AR8327_GLOBAL_FW_CTRL0_ARP_FORWARD_ACT_VALUE(value)             (((value) << AR8327_GLOBAL_FW_CTRL0_ARP_FORWARD_ACT_SHIFT) & AR8327_GLOBAL_FW_CTRL0_ARP_FORWARD_ACT_MASK)
#    define AR8327_GLOBAL_FW_CTRL0_ARP_FORWARD_ACT_VALUE_REDIRECT_TO_CPU  0
#    define AR8327_GLOBAL_FW_CTRL0_ARP_FORWARD_ACT_VALUE_COPY_TO_CPU      1
#    define AR8327_GLOBAL_FW_CTRL0_ARP_FORWARD_ACT_VALUE_FORWARD          2

#define AR8327_GLOBAL_FW_CTRL1      0x624
#  define AR8327_GLOBAL_FW_CTRL1_UNI_FLOOD_DP_SHIFT              0
#  define AR8327_GLOBAL_FW_CTRL1_UNI_FLOOD_DP_MASK               (((1u << 7) - 1) << AR8327_GLOBAL_FW_CTRL1_UNI_FLOOD_DP_SHIFT)
#  define AR8327_GLOBAL_FW_CTRL1_UNI_FLOOD_DP_VALUE(value)       (((value) << AR8327_GLOBAL_FW_CTRL1_UNI_FLOOD_DP_SHIFT) & AR8327_GLOBAL_FW_CTRL1_UNI_FLOOD_DP_MASK)
#  define AR8327_GLOBAL_FW_CTRL1_MULTI_FLOOD_DP_SHIFT            8
#  define AR8327_GLOBAL_FW_CTRL1_MULTI_FLOOD_DP_MASK             (((1u << 7) - 1) << AR8327_GLOBAL_FW_CTRL1_MULTI_FLOOD_DP_SHIFT)
#  define AR8327_GLOBAL_FW_CTRL1_MULTI_FLOOD_DP_VALUE(value)     (((value) << AR8327_GLOBAL_FW_CTRL1_MULTI_FLOOD_DP_SHIFT) & AR8327_GLOBAL_FW_CTRL1_MULTI_FLOOD_DP_MASK)
#  define AR8327_GLOBAL_FW_CTRL1_BROAD_DP_SHIFT                  16
#  define AR8327_GLOBAL_FW_CTRL1_BROAD_DP_MASK                   (((1u << 7) - 1) << AR8327_GLOBAL_FW_CTRL1_BROAD_DP_SHIFT)
#  define AR8327_GLOBAL_FW_CTRL1_BROAD_DP_VALUE(value)           (((value) << AR8327_GLOBAL_FW_CTRL1_BROAD_DP_SHIFT) & AR8327_GLOBAL_FW_CTRL1_BROAD_DP_MASK)
#  define AR8327_GLOBAL_FW_CTRL1_IGMP_JOIN_LEAVE_DP_SHIFT        24
#  define AR8327_GLOBAL_FW_CTRL1_IGMP_JOIN_LEAVE_DP_MASK         (((1u << 7) - 1) << AR8327_GLOBAL_FW_CTRL1_IGMP_JOIN_LEAVE_DP_SHIFT)
#  define AR8327_GLOBAL_FW_CTRL1_IGMP_JOIN_LEAVE_DP_VALUE(value) (((value) << AR8327_GLOBAL_FW_CTRL1_IGMP_JOIN_LEAVE_DP_SHIFT) & AR8327_GLOBAL_FW_CTRL1_IGMP_JOIN_LEAVE_DP_MASK)

/* union ar8327_port_status for ports */
#define AR8327_PORT_STATUS(port)                           (0x7c + 4 * (port))
#define AR8327_PORT_LOOKUP_CTRL(port)                      (0x660 + 12 * (port))
#define AR8327_PORT_PRI_CTRL(port)                         (0x664 + 12 * (port))
#define AR8327_PORT_HOL_CTRL1(port)                        (0x974 + 8 * (port))

enum _ar_rate_limit {
    ar_rate_limit_128kbits,
    ar_rate_limit_256kbits,
    ar_rate_limit_512kbits,
    ar_rate_limit_1mbits,
    ar_rate_limit_2mbits,
    ar_rate_limit_4mbits,
    ar_rate_limit_8mbits,
    ar_rate_limit_16mbits,
    ar_rate_limit_32mbits,
    ar_rate_limit_64mbits
};

enum _link_speed { 
    link_speed_10M   = 0,
    link_speed_100M  = 1,
    link_speed_1000M = 2,
    link_speed_error = 3
};

enum ar8327_port_state {
    ar8327_port_state_disable   = 0,
    ar8327_port_state_blocking  = 1,
    ar8327_port_state_listening = 2,
    ar8327_port_state_learning  = 3,
    ar8327_port_state_forward   = 4
};

enum ar8327_vlan_mode {
    ar8327_vlan_mode_disable  = 0,
    ar8327_vlan_mode_fallback = 1,
    ar8327_vlan_mode_check    = 2,
    ar8327_vlan_mode_secure   = 3
};

#if defined(__LITTLE_ENDIAN_BITFIELD)
union ar8327_port_status { 
    struct { 
        enum _link_speed speed               : 2;
        unsigned int txmac_enable            : 1;
        unsigned int rxmac_enable            : 1;
        unsigned int tx_flow_enable          : 1;
        unsigned int rx_flow_enable          : 1;
        unsigned int full_duplex             : 1;
        unsigned int tx_half_flow_enable     : 1;
        unsigned int link                    : 1;
        unsigned int link_mode_enable        : 1;
        unsigned int auto_tx_flow_enable     : 1;
        unsigned int auto_rx_flow_enable     : 1;
        unsigned int flow_link_enable        : 1;
        unsigned int reserved                : (31 - 12);
    } bits;
    unsigned int reg;
};

union ar8327_port_lookup_ctrl {
    struct {
        unsigned int           port_vid_mem       : 7;
        unsigned int           /* reserved */     : 1;
        enum ar8327_vlan_mode  vlan_mode          : 2;
        unsigned int           force_port_vlan_en : 1;
        unsigned int           /* reserved */     : 5;
        enum ar8327_port_state port_state         : 3;
        unsigned int           /* reserved */     : 1;
        unsigned int           learn_en           : 1;
        unsigned int           port_loopback_en   : 1;
        unsigned int           /* reserved */     : 3;
        unsigned int           ing_mirror_en      : 1;
        unsigned int           arp_leaky_en       : 1;
        unsigned int           multi_leaky_en     : 1;
        unsigned int           uni_leaky_en       : 1;
        unsigned int           /* reserved */     : 2;
        unsigned int           multicast_drop_en  : 1;
    } bits;
    unsigned int reg;
};

union ar8327_port_pri_ctrl {
    struct {
        unsigned int /* reserved */      :  2;
        unsigned int ip_pri_sel          :  2;
        unsigned int vlan_pri_sel        :  2;
        unsigned int da_pri_sel          :  2;
        unsigned int /* reserved */      :  8;
        unsigned int ip_pri_en           :  1;
        unsigned int vlan_pri_en         :  1;
        unsigned int da_pri_en           :  1;
        unsigned int /* reserved */      :  1;
        unsigned int eg_mac_base_vlan_en :  1;
        unsigned int /* reserved */      : 11;
    } bits;
    unsigned int reg;
};

union ar8327_port_hol_ctrl1 {
    struct {
        unsigned int ing_buf_num           :  4;
        unsigned int /* reserved */        :  2;
        unsigned int eg_pri_queue_ctrl_en  :  1;
        unsigned int eg_port_queue_ctrl_en :  1;
        unsigned int /* reserved */        :  8;
        unsigned int eg_mirror_en          :  1;
        unsigned int /* reserved */        : 15;
    } bits;
    unsigned int reg;
};

struct ar_port_status { 
    enum _link_speed speed               : 2;
    unsigned int txmac_enable            : 1;
    unsigned int rxmac_enable            : 1;
    unsigned int tx_flow_enable          : 1;
    unsigned int rx_flow_enable          : 1;
    unsigned int full_duplex             : 1;
    unsigned int reserved1               : 1;
    unsigned int link_status             : 1;
    unsigned int link_mode_enable        : 1;
    unsigned int link_pause_enable       : 1;
    unsigned int link_async_pause_enable : 1;
    unsigned int reserved2               : (31 - 11);
};

struct ar_port_control {
    unsigned int port_state            : 3;
    unsigned int reserved1             : 1;
    unsigned int learn_one_lock        : 1;
    unsigned int egress_VLAN_mode      : 1;
    unsigned int igmp_mld_enable       : 1;
    unsigned int mac_loop_back         : 1;
    unsigned int single_VLAN_enable    : 1;
    unsigned int learn_enable          : 1;
    unsigned int reserved2             : 1;
    unsigned int egress_mirror_enable  : 1;
    unsigned int ingress_mirror_enable : 1;
    unsigned int reserved              : (31-18);
};

struct ar_port_VLAN {
    unsigned int port_VID          : 12;
    unsigned int reserved1         : 4;
    unsigned int port_VID_mem      : (25 - 15);
    unsigned int reserved2         : 1;
    unsigned int ingress_port_prio : 2;
    unsigned int _802_1Q_mode      : 2;
};

struct ar_rate_limit {
    unsigned int        reserved3                 : (31 - 25);
    unsigned int        egress_rate_limit_enable  : 1;
    unsigned int        ingress_rate_limit_enable : 1;
    unsigned int        reserved2                 : (23 - 19);
    enum _ar_rate_limit egree_rate                : 4;
    unsigned int        reserved1                 : (15 - 4);
    enum _ar_rate_limit ingree_rate               : 4;
};

struct ar_priority_control {
    unsigned int port_pri_sel : 2;
    unsigned int ip_pri_sel   : 2;
    unsigned int vlan_pri_sel : 2;
    unsigned int da_pri_sel   : 2;
    unsigned int reserved1    : (15 - 7);
    unsigned int ip_pri_en    : 1;
    unsigned int vlan_pri_en  : 1;
    unsigned int da_pri_en    : 1;
    unsigned int port_pri_en  : 1;
    unsigned int reserved2    : (31 - 20);
};

#elif defined (__BIG_ENDIAN_BITFIELD)
union ar8327_port_status { 
    struct { 
        unsigned int /* reserved */          : (31 - 12);
        unsigned int flow_link_enable        : 1;
        unsigned int auto_rx_flow_enable     : 1;
        unsigned int auto_tx_flow_enable     : 1;
        unsigned int link_mode_enable        : 1;
        unsigned int link                    : 1;
        unsigned int tx_half_flow_enable     : 1;
        unsigned int full_duplex             : 1;
        unsigned int rx_flow_enable          : 1;
        unsigned int tx_flow_enable          : 1;
        unsigned int rxmac_enable            : 1;
        unsigned int txmac_enable            : 1;
        enum _link_speed speed               : 2;
    } bits;
    unsigned int reg;
};

union ar8327_port_lookup_ctrl {
    struct {
        unsigned int           multicast_drop_en  : 1;
        unsigned int           /* reserved */     : 2;
        unsigned int           uni_leaky_en       : 1;
        unsigned int           multi_leaky_en     : 1;
        unsigned int           arp_leaky_en       : 1;
        unsigned int           ing_mirror_en      : 1;
        unsigned int           /* reserved */     : 3;
        unsigned int           port_loopback_en   : 1;
        unsigned int           learn_en           : 1;
        unsigned int           /* reserved */     : 1;
        enum ar8327_port_state port_state         : 3;
        unsigned int           /* reserved */     : 5;
        unsigned int           force_port_vlan_en : 1;
        enum ar8327_vlan_mode  vlan_mode          : 2;
        unsigned int           /* reserved */     : 1;
        unsigned int           port_vid_mem       : 7;
    } bits;
    unsigned int reg;
};

union ar8327_port_pri_ctrl {
    struct {
        unsigned int /* reserved */      : 11;
        unsigned int eg_mac_base_vlan_en :  1;
        unsigned int /* reserved */      :  1;
        unsigned int da_pri_en           :  1;
        unsigned int vlan_pri_en         :  1;
        unsigned int ip_pri_en           :  1;
        unsigned int /* reserved */      :  8;
        unsigned int da_pri_sel          :  2;
        unsigned int vlan_pri_sel        :  2;
        unsigned int ip_pri_sel          :  2;
        unsigned int /* reserved */      :  2;
    } bits;
    unsigned int reg;
};

union ar8327_port_hol_ctrl1 {
    struct {
        unsigned int /* reserved */        : 15;
        unsigned int eg_mirror_en          :  1;
        unsigned int /* reserved */        :  8;
        unsigned int eg_port_queue_ctrl_en :  1;
        unsigned int eg_pri_queue_ctrl_en  :  1;
        unsigned int /* reserved */        :  2;
        unsigned int ing_buf_num           :  4;
    } bits;
    unsigned int reg;
};

struct ar_port_status { 
    unsigned int reserved2               : (31 - 11);
    unsigned int link_async_pause_enable : 1;
    unsigned int link_pause_enable       : 1;
    unsigned int link_mode_enable        : 1;
    unsigned int link_status             : 1;
    unsigned int reserved1               : 1;
    unsigned int full_duplex             : 1;
    unsigned int rx_flow_enable          : 1;
    unsigned int tx_flow_enable          : 1;
    unsigned int rxmac_enable            : 1;
    unsigned int txmac_enable            : 1;
    enum _link_speed speed               : 2;
};

struct ar_port_control {
    unsigned int reserved              : (31-18);
    unsigned int ingress_mirror_enable : 1;
    unsigned int egress_mirror_enable  : 1;
    unsigned int reserved2             : 1;
    unsigned int learn_enable          : 1;
    unsigned int single_VLAN_enable    : 1;
    unsigned int mac_loop_back         : 1;
    unsigned int igmp_mld_enable       : 1;
    unsigned int egress_VLAN_mode      : 1;
    unsigned int learn_one_lock        : 1;
    unsigned int reserved1             : 1;
    unsigned int port_state            : 3;
};

struct ar_port_VLAN {
    unsigned int _802_1Q_mode      : 2;
    unsigned int ingress_port_prio : 2;
    unsigned int reserved2         : 1;
    unsigned int port_VID_mem      : (25 - 15);
    unsigned int reserved1         : 4;
    unsigned int port_VID          : 12;
};

struct ar_rate_limit {
    enum _ar_rate_limit ingree_rate               : 4;
    unsigned int        reserved1                 : (15 - 4);
    enum _ar_rate_limit egree_rate                : 4;
    unsigned int        reserved2                 : (23 - 19);
    unsigned int        ingress_rate_limit_enable : 1;
    unsigned int        egress_rate_limit_enable  : 1;
    unsigned int        reserved3                 : (31 - 25);
};

struct ar_priority_control {
    unsigned int reserved2    : (31 - 20);
    unsigned int port_pri_en  : 1;
    unsigned int da_pri_en    : 1;
    unsigned int vlan_pri_en  : 1;
    unsigned int ip_pri_en    : 1;
    unsigned int reserved1    : (15 - 7);
    unsigned int da_pri_sel   : 2;
    unsigned int vlan_pri_sel : 2;
    unsigned int ip_pri_sel   : 2;
    unsigned int port_pri_sel : 2;
};

#else /* No endianess */
#error	"Please fix <asm/byteorder.h>"
#endif /* endianess */


struct ar_struct {
    struct ar_port {  /* offsets 0x100, 0x200, 0x300 .... */
        struct ar_port_status status;
        struct ar_port_control control;
        struct ar_port_VLAN port_VLAN;
        struct ar_rate_limit rate_limit;
        struct ar_priority_control priority_control;
    } port[6];
};


/*------------------------------------------------------------------------------------------*\
 * MII-Register 16bit
\*------------------------------------------------------------------------------------------*/
#define AR8216_MAX_PHYS                 4

#define AR8216_MII_CONTROL              0x00

/*--- ATHR_PHY_CONTROL fields ---*/
#define AR8216_PHYCTRL_SW_RESET        (1<<15)  
#define AR8216_PHYCTRL_LOOPBACK        (1<<14)  
#define AR8216_PHYCTRL_SPEEDSEL_LSB    (1<<13) 
#define AR8216_PHYCTRL_AUTONEG_EN      (1<<12)
#define AR8216_PHYCTRL_POWERDOWN       (1<<11)
#define AR8216_PHYCTRL_ISOLATE         (1<<10)
#define AR8216_PHYCTRL_AUTONEG_RESTART (1<<9)
#define AR8216_PHYCTRL_FULLDUPLEX      (1<<8)
#define AR8216_PHYCTRL_COLLISION_TEST  (1<<7)
#define AR8216_PHYCTRL_SPEEDSEL_MSB    (1<<6)

#define AR8216_MII_STATUS               0x01
#define AR8216_MII_PHY_ID1              0x02
#define AR8216_MII_PHY_ID2              0x03
#define AR8216_MII_AUTO_NEG_ADV         0x04

/*--- Advertisement register. ---*/
#define ATHR_ADVERTISE_NEXT_PAGE              0x8000
#define ATHR_ADVERTISE_ASYM_PAUSE             0x0800
#define ATHR_ADVERTISE_PAUSE                  0x0400
#define ATHR_ADVERTISE_100FULL                0x0100
#define ATHR_ADVERTISE_100HALF                0x0080  
#define ATHR_ADVERTISE_10FULL                 0x0040  
#define ATHR_ADVERTISE_10HALF                 0x0020  

#define ATHR_ADVERTISE_ALL (ATHR_ADVERTISE_10HALF | ATHR_ADVERTISE_10FULL | \
                            ATHR_ADVERTISE_100HALF | ATHR_ADVERTISE_100FULL | \
			                ATHR_ADVERTISE_ASYM_PAUSE | ATHR_ADVERTISE_PAUSE)


#define AR8216_MII_LINK_PARTNER         0x05
#define AR8216_MII_AUTO_NEG_EXP         0x06
#define AR8316_MII_1000BASET_CTRL       0x09
#define AR8216_MII_FUNCTION_CTRL        0x10
#define AR8216_MII_PHY_STATUS           0x11

/*--- PHY-Specific Status ---*/
#define ATHR_PHY_STATUS_JABBER              (1<<0)
#define ATHR_PHY_STATUS_POLARITY            (1<<1)
#define ATHR_PHY_STATUS_RX_PAUSE_EN         (1<<2)
#define ATHR_PHY_STATUS_TX_PAUSE_EN         (1<<3)
#define ATHR_PHY_STATUS_ENERGY_DETECT       (1<<4)
#define ATHR_PHY_STATUS_SMART_SPEED_DOWN    (1<<5)
#define ATHR_PHY_STATUS_MDI_CROSSOVER       (1<<6)
#define ATHR_PHY_STATUS_LINK                (1<<10)
#define ATHR_PHY_STATUS_SPEED_DUPLEX        (1<<11)
#define ATHR_PHY_STATUS_PAGE_RECEIVED       (1<<12)
#define ATHR_PHY_STATUS_DUPLEX              (1<<13)
#define ATHR_PHY_STATUS_SPEED_SHIFT         14
#define ATHR_PHY_STATUS_SPEED_MASK          (3<<14)


#define AR8216_MII_INT_ENABLE           0x12
#define AR8216_MII_INT_STATUS           0x13
#  define AR8216_MII_INT_STATUS_JABBER           (1u <<  0)
#  define AR8216_MII_INT_STATUS_POLARITY         (1u <<  1)
#  define AR8216_MII_INT_STATUS_ENERGY_DETECT    (1u <<  4)
#  define AR8216_MII_INT_STATUS_SMARTSPEED       (1u <<  5)
#  define AR8216_MII_INT_STATUS_MDI_CROSSOVER    (1u <<  6)
#  define AR8216_MII_INT_STATUS_FIFO_ERROR       (1u <<  7)
#  define AR8216_MII_INT_STATUS_FALSE_CARRIER    (1u <<  8)
#  define AR8216_MII_INT_STATUS_SYMBOL_ERROR     (1u <<  9)
#  define AR8216_MII_INT_STATUS_LINK_CHANGE      (1u << 10)
#  define AR8216_MII_INT_STATUS_AUTONEG_COMPLETE (1u << 11)
#  define AR8216_MII_INT_STATUS_PAGE_RECEIVED    (1u << 12)
#  define AR8216_MII_INT_STATUS_DUPLEX_CHANGED   (1u << 13)
#  define AR8216_MII_INT_STATUS_SPEED_CHANGED    (1u << 14)
#  define AR8216_MII_INT_STATUS_AUTONEG_ERROR    (1u << 15)
#define AR8216_MII_EXT_PHY_CTRL         0x14
#define AR8216_MII_RX_ERROR_CNT         0x15
#define AR8216_MII_VIRT_CABLE_TST       0x16
#define AR8216_MII_LED_CTRL             0x17
#define AR8216_MII_LED_MANUAL           0x18
#define AR8216_MII_VIRT_CABLE_STATUS    0x1C
#define AR8216_MII_DEBUG_ADDR           0x1D
#define AR8216_MII_DEBUG_DATA           0x1E

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- extern struct ar_struct ar_serial_register; ---*/

#endif /*--- #define _INC_AR_REG ---*/

