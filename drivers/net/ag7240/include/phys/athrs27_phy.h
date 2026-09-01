/*
 * Copyright (c) 2008, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ATHRS27_PHY_H
#define _ATHRS27_PHY_H


/*****************/
/* PHY Registers */
/*****************/
#define S27_PHY_CONTROL                                 0
#define     PHY_CTRL_SOFTWARE_RESET           (1 << 15)
#define     PHY_CTRL_SPEED_LSB                (1 << 13)
#define     PHY_CTRL_AUTONEGOTIATION_ENABLE   (1 << 12)
#define     PHY_CTRL_RESTART_AUTONEGOTIATION  (1 <<  9)
#define     PHY_CTRL_SPEED_FULL_DUPLEX        (1 <<  8)
#define     PHY_CTRL_SPEED_MSB                (1 <<  6)

#define S27_PHY_STATUS                                  1
#define     PHY_STATUS_AUTO_NEG_DONE          (1 <<  5)

#define S27_PHY_ID1                                     2
#define S27_PHY_ID2                                     3
#define S27_PHY_AUTONEG_ADVERT                          4
#define     PHY_ADVERTISE_NEXT_PAGE           (1 << 15)
#define     PHY_ADVERTISE_ASYM_PAUSE          (1 << 11)
#define     PHY_ADVERTISE_PAUSE               (1 << 10)
#define     PHY_ADVERTISE_100FULL             (1 <<  8)
#define     PHY_ADVERTISE_100HALF             (1 <<  7)
#define     PHY_ADVERTISE_10FULL              (1 <<  6)
#define     PHY_ADVERTISE_10HALF              (1 <<  5)

#define S27_PHY_LINK_PARTNER_ABILITY                    5
#define     PHY_LINK_100BASETX_FULL_DUPLEX    (1 <<  8)
#define     PHY_LINK_100BASETX                (1 <<  7)
#define     PHY_LINK_10BASETX_FULL_DUPLEX     (1 <<  6)
#define     PHY_LINK_10BASETX                 (1 <<  5)

#define S27_PHY_AUTONEG_EXPANSION                       6
#define S27_PHY_NEXT_PAGE_TRANSMIT                      7
#define S27_PHY_LINK_PARTNER_NEXT_PAGE                  8
#define S27_PHY_1000BASET_CONTROL                       9
#define     PHY_ADVERTISE_1000FULL            (1 <<  9)
#define     PHY_ADVERTISE_1000HALF		      (1 <<  8)

#define S27_PHY_1000BASET_STATUS                        10
#define S27_PHY_MMD_CTRL_REG			                13
#define S27_PHY_MMD_DATA_REG			                14
#define S27_PHY_FUNC_CONTROL                            16
#define S27_PHY_SPEC_STATUS                             17
#define     PHY_STATUS_LINK_MASK            (3 << 14)
#define     PHY_STATUS_LINK_SHIFT            14
#define     PHY_STATUS_FULL_DUPLEX          (1 << 13)
#define     PHY_STATUS_LINK_PASS            (1 << 10)
#define     PHY_STATUS_RESOVLED             (1 << 11)
#define     PHY_STATUS_LINK_10M			    0
#define     PHY_STATUS_LINK_100M		    1
#define     PHY_STATUS_LINK_1000M			2

#define S27_PHY_INTR_ENABLE                             18
#define S27_PHY_INTR_STATUS                             19
#define     PHY_INT_STATUS_JABBER           (1 <<  0)
#define     PHY_INT_STATUS_POLARITY         (1 <<  1)
#define     PHY_INT_STATUS_WIRE_DOWNGRADE   (1 <<  5)
#define     PHY_INT_STATUS_MDI_CROSSOVER    (1 <<  6)
#define     PHY_INT_STATUS_FIFO_ERROR       (1 <<  7)
#define     PHY_INT_STATUS_CARRIER          (1 <<  8)
#define     PHY_INT_STATUS_SYMBOLERROR      (1 <<  9)
#define     PHY_INT_STATUS_LINK_CHANGED     (1 << 10)
#define     PHY_INT_STATUS_AUTONEG          (1 << 11)
#define     PHY_INT_STATUS_PAGERCV          (1 << 12)
#define     PHY_INT_STATUS_SPEED            (1 << 14)
#define     PHY_INT_STATUS_AUTONEG_ERR      (1 << 15)

#define     S27_PHY_STATUS_INTS (PHY_INT_STATUS_SPEED | PHY_INT_STATUS_AUTONEG | PHY_INT_STATUS_LINK_CHANGED)

#define S27_PHY_DEBUG_PORT_ADDRESS                      29
#define S27_PHY_DEBUG_PORT_DATA                         30

/*--- ATHR_PHY_CONTROL fields ---*/
#define S27_RESET_DONE(phy_control)     (((phy_control) & (PHY_CTRL_SOFTWARE_RESET)) == 0)
    
/*--- ATHR_PHY STATUS fields ---*/
#define S27_AUTONEG_DONE(ip_phy_status) (((ip_phy_status) & (PHY_STATUS_AUTO_NEG_DONE)) == (PHY_STATUS_AUTO_NEG_DONE))

/* Advertisement register. */
#define S27_ADVERTISE_ALL (PHY_ADVERTISE_ASYM_PAUSE | PHY_ADVERTISE_PAUSE  | \
                           PHY_ADVERTISE_10HALF     | PHY_ADVERTISE_10FULL | \
                           PHY_ADVERTISE_100HALF    | PHY_ADVERTISE_100FULL)

/*------------------------------------------------------------------------------------------*\
 * S27 CSR Registers
\*------------------------------------------------------------------------------------------*/
#define S27_MASK_CTL_REG		                 0x0000
#define     S27_MASK_CTRL_RESET           (1 << 31)
#define S27_OPMODE_REG0                          0x0004
#define S27_OPMODE_REG1                          0x0008
#define S27_OPMODE_REG2                          0x000C
#define S27_PWRSTRAP_REG                         0x0010
#define S27_GLOBAL_INTR_REG                      0x0014
#define     S27_GINT_EEPROM_INT            (1<<0)
#define     S27_GINT_EEPROM_ERR_INT        (1<<1)
#define     S27_GINT_PHY_INT               (1<<2)
#define     S27_GINT_MDIO_INT              (1<<3)
#define     S27_GINT_ARL_DONE_INT          (1<<4)
#define     S27_GINT_ARL_FULL_INT          (1<<5)
#define     S27_GINT_AT_INI_INT            (1<<6)
#define     S27_GINT_QM_INI_INT            (1<<7)
#define     S27_GINT_VT_DONE_INT           (1<<8)
#define     S27_GINT_VT_MEM_INT            (1<<9)
#define     S27_GINT_VT_MIS_VIO_INT        (1<<10)
#define     S27_GINT_BIST_DONE_INT         (1<<11)
#define     S27_GINT_MIB_DONE_INT          (1<<12)
#define     S27_GINT_MIB_INI_INT           (1<<13)
#define     S27_GINT_HW_INI_DONE           (1<<14)
#define     S27_GINT_LOOP_CHECK_INT        (1<<18)
#define S27_GLOBAL_INTR_MASK_REG                 0x0018
#define S27_FLD_MASK_REG                         0x002c
#define S27_FLCTL_REG0				 0x0034
#define S27_FLCTL_REG1				 0x0038
#define S27_ARL_TBL_FUNC_REG0                    0x0050
#define S27_ARL_TBL_FUNC_REG1                    0x0054
#define S27_ARL_TBL_FUNC_REG2                    0x0058
#define S27_ARL_TBL_CTRL_REG                     0x005c
#define S27_TAGPRI_REG                           0x0070
#define S27_CPU_PORT_REGISTER                    0x0078
#define S27_MDIO_CTRL_REGISTER                   0x0098

#define S27_PORT_STATUS_REGISTER0                0x0100 
#define S27_PORT_STATUS_REGISTER1                0x0200
#define S27_PORT_STATUS_REGISTER2                0x0300
#define S27_PORT_STATUS_REGISTER3                0x0400
#define S27_PORT_STATUS_REGISTER4                0x0500
#define S27_PORT_STATUS_REGISTER5                0x0600

#define S27_PORT_CONTROL_REGISTER0               0x0104
#define S27_PORT_CONTROL_REGISTER1               0x0204
#define S27_PORT_CONTROL_REGISTER2               0x0304
#define S27_PORT_CONTROL_REGISTER3               0x0404
#define S27_PORT_CONTROL_REGISTER4               0x0504
#define S27_PORT_CONTROL_REGISTER5               0x0604

#define S27_PORT_BASE_VLAN_REGISTER0             0x0108 
#define S27_PORT_BASE_VLAN_REGISTER1             0x0208
#define S27_PORT_BASE_VLAN_REGISTER2             0x0308
#define S27_PORT_BASE_VLAN_REGISTER3             0x0408
#define S27_PORT_BASE_VLAN_REGISTER4             0x0508
#define S27_PORT_BASE_VLAN_REGISTER5             0x0608

#define S27_RATE_LIMIT_REGISTER0                 0x010C
#define S27_RATE_LIMIT_REGISTER1                 0x020C
#define S27_RATE_LIMIT_REGISTER2                 0x030C
#define S27_RATE_LIMIT_REGISTER3                 0x040C
#define S27_RATE_LIMIT_REGISTER4                 0x050C
#define S27_RATE_LIMIT_REGISTER5                 0x060C

#define S27_RATE_LIMIT1_REGISTER0                0x011c
#define S27_RATE_LIMIT2_REGISTER0                0x0120

/* SWITCH QOS REGISTERS */
#define S27_CPU_PORT                		 0x0078
#define S27_IP_PRI_MAP0             		 0x0060
#define S27_IP_PRI_MAP1             		 0x0064
#define S27_IP_PRI_MAP2             		 0x0068

#define S27_PRI_CTRL_PORT_0             	 0x110 /* CPU PORT */
#define S27_PRI_CTRL_PORT_1             	 0x210
#define S27_PRI_CTRL_PORT_2             	 0x310
#define S27_PRI_CTRL_PORT_3             	 0x410
#define S27_PRI_CTRL_PORT_4             	 0x510
#define S27_PRI_CTRL_PORT_5             	 0x610

#define S27_QOS_PORT_0				 0x110 /* CPU PORT */
#define S27_QOS_PORT_1				 0x210
#define S27_QOS_PORT_2				 0x310
#define S27_QOS_PORT_3				 0x410
#define S27_QOS_PORT_4				 0x510

#define S27_LPI_CTRL_PORT_1			 0x230 /* phy 0 */
#define S27_LPI_CTRL_PORT_2			 0x330 /* phy 1 */
#define S27_LPI_CTRL_PORT_3			 0x430 /* phy 2 */
#define S27_LPI_CTRL_PORT_4			 0x530 /* phy 3 */

/* enable broadcast on CPU port (port 0) */
#define S27_ENABLE_CPU_BROADCAST             (1 << 26)
#define S27_ENABLE_CPU_BCAST_FWD             (1 << 25)

/* For OPMODE_REGs */
/* in OPMODE_REG0 */
#define S27_MAC0_MAC_GMII_EN                     (1 << 6) 
/* in OPMODE_REG1 */
#define S27_PHY4_RMII_EN                         (1 << 29)
#define S27_PHY4_MII_EN                          (1 << 28)
#define S27_MAC5_RGMII_EN                        (1 << 26) 

/* For PORT_STATUS_REGISTERs */
#define S27_PS_FLOW_LINK_EN                      (1 << 12)
#define S27_PS_LINK_ASYN_PAUSE_EN                (1 << 11)
#define S27_PS_LINK_PAUSE_EN                     (1 << 10)
#define S27_PS_LINK_EN                           (1 << 9)
#define S27_PS_LINK                              (1 << 8)
#define S27_PS_TXH_FLOW_EN                       (1 << 7)
#define S27_PS_DUPLEX                            (1 << 6)
#define S27_PS_RX_FLOW_EN                        (1 << 5)
#define S27_PS_TX_FLOW_EN                        (1 << 4)
#define S27_PS_RXMAC_EN                          (1 << 3)
#define S27_PS_TXMAC_EN                          (1 << 2)
#define S27_PS_SPEED_1000                        (1 << 1)
#define S27_PS_SPEED_100                         (1 << 0)
#define S27_PS_SPEED_10                                0

#define S27_PORT_STATUS_DEFAULT                  (S27_PS_FLOW_LINK_EN | S27_PS_LINK_EN | S27_PS_TXH_FLOW_EN)

/* For PORT_CONTROL_REGISTERs */
#define S27_EAPOL_EN                             (1 << 23)
#define S27_ARP_LEAKY_EN                         (1 << 22)
#define S27_IGMP_LEAVE_EN                        (1 << 21)
#define S27_IGMP_JOIN_EN                         (1 << 20)
#define S27_DHCP_EN                              (1 << 19)
#define S27_IPG_DEC_EN                           (1 << 18)
#define S27_ING_MIRROR_EN                        (1 << 17)
#define S27_EG_MIRROT_EN                         (1 << 16)
#define S27_LEARN_EN                             (1 << 14)
#define S27_MAC_LOOP_BACK                        (1 << 12)
#define S27_HEADER_EN              	             (1 << 11)
#define S27_IGMP_MLD_EN                          (1 << 10)
#define S27_LEARN_ONE_LOCK             	         (1 << 7)
#define S27_PORT_LOCK_EN                         (1 << 6)
#define S27_PORT_DROP_EN                         (1 << 5)
#define S27_PORT_MODE_FWD                        0x4

/* For MDIO_CONTROL */
#define S27_MDIO_BUSY                            (1 << 31)
#define S27_MDIO_MASTER                          (1 << 30)
#define S27_MDIO_CMD_RD                          (1 << 27)
#define S27_MDIO_CMD_WR                          (0 << 27)
#define S27_MDIO_SUP_PRE                         (1 << 26)
#define S27_MDIO_PHY_ADDR                        21
#define S27_MDIO_REG_ADDR                        16

/* For MMD register control */
#define S27_MMD_FUNC_ADDR			(0 << 14)
#define S27_MMD_FUNC_DATA			(1 << 14)
#define S27_MMD_FUNC_DATA_2			(2 << 14)
#define S27_MMD_FUNC_DATA_3			(3 << 14)

/* For 802.3az (LPI) control */
#define S27_LPI_ENABLED			        (1 << 31)
#define S27_LPI_WAKEUP_TIMER			0x20

/* For phyInfo_t azFeature */
#define S27_8023AZ_PHY_ENABLED			(1 << 0)
#define S27_8023AZ_PHY_LINKED                   (1 << 1)

/* For Tag priority */
#define S27_TAGPRI_DEFAULT                       0xFA50

#define S27_QOS_MODE_REGISTER                     0x030
#define S27_QOS_FIXED_PRIORITY                   ((0 << 31) | (0 << 28))
#define S27_QOS_WEIGHTED                         ((1 << 31) | (0 << 28)) /* Fixed weight 8,4,2,1 */
#define S27_QOS_MIXED                            ((1 << 31) | (1 << 28)) /* Q3 for managment; Q2,Q1,Q0 - 4,2,1 */

#ifndef BOOL
#define BOOL    int
#endif
/* enabled the 802.3az feature */
#define S27_8023AZ_FEATURE

struct athr_gmac;   /* forward declaration... */

int athrs27_reg_init(struct athr_gmac *mac);
int athrs27_phy_is_up(int unit);
int athrs27_phy_is_fdx(int unit,int phyUnit);
int athrs27_phy_speed(int unit,int phyUnit);
void athrs27_phy_stab_wr(int phy_id, BOOL phy_up, int phy_speed);
int athrs27_phy_setup(struct athr_gmac *mac);
int athrs27_mdc_check(void);
void athrs27_mac_speed_set(void *arg, int phy_speed);
int athrs27_register_ops(struct athr_gmac *mac);
void athrs27_enable_link_intrs(struct athr_gmac *mac);
void athrs27_disable_link_intrs(struct athr_gmac *mac);
int athrs27_phy_is_link_alive(int phyUnit);
void athrs27_phy_stab_wr(int phy_id, int phy_up, int phy_speed);
unsigned int athrs27_reg_read(unsigned int s27_addr);
void athrs27_reg_write(unsigned int s27_addr, unsigned int s27_write_data);
void s27_wr_phy(int ethUnit,unsigned int phy_addr, unsigned int reg_addr, unsigned int write_data);
unsigned int s27_rd_phy(int ethUnit,unsigned int phy_addr, unsigned int reg_addr);
int athrs27_ioctl(struct net_device *dev,void *args, int cmd);
void athrs27_reg_rmw(unsigned int s27_addr, unsigned int s27_write_data); 

/*
 *  Atheros header defines
 */
#ifndef _ATH_HEADER_CONF
#define _ATH_HEADER_CONF

typedef enum {
    NORMAL_PACKET,
    RESERVED0,
    MIB_1ST,
    RESERVED1,
    RESERVED2,
    READ_WRITE_REG,
    READ_WRITE_REG_ACK,
    RESERVED3
} AT_HEADER_TYPE;

typedef struct {
    uint16_t    reserved0  :2;
    uint16_t    priority   :2;
    uint16_t    type       :4;
    uint16_t    broadcast  :1;
    uint16_t    from_cpu   :1;
    uint16_t    reserved1  :2;
    uint16_t    port_num   :4;
}at_header_t;

#define ATHR_HEADER_LEN 2
#endif // _ATH_HEADER_CONF

#endif
