#ifndef _LANTIQ_11G_PHY_H
#define _LANTIQ_11G_PHY_H

/*****************/
/* PHY Registers */
/*****************/
#define LANTIQ_11G_PHY_CONTROL                 0
#define LANTIQ_11G_PHY_STATUS                  1
#define LANTIQ_11G_PHY_ID1                     2
#define LANTIQ_11G_PHY_ID2                     3
#define LANTIQ_11G_AUTONEG_ADVERT              4
#define LANTIQ_11G_LINK_PARTNER_ABILITY        5
#define LANTIQ_11G_AUTONEG_EXPANSION           6
#define LANTIQ_11G_NEXT_PAGE_TRANSMIT          7
#define LANTIQ_11G_LINK_PARTNER_NEXT_PAGE      8
#define LANTIQ_11G_1000BASET_CONTROL           9
#define LANTIQ_11G_1000BASET_STATUS            10
#define LANTIQ_11G_PHY_SPEC_STATUS             0x18
/*--- #define LANTIQ_11G_PHY_SPEC_CONTROL            0x11 ---*/
/*--- #define LANTIQ_11G_DEBUG_PORT_ADDRESS          29 ---*/
/*--- #define LANTIQ_11G_DEBUG_PORT_DATA             30 ---*/
#define LANTIQ_11G_PHY_INTR_ENABLE             0x19
#define LANTIQ_11G_PHY_INTR_STATUS             0x1A


/* LANTIQ_11G_PHY_CONTROL fields */
#define LANTIQ_11G_CTRL_SOFTWARE_RESET                    0x8000
#define LANTIQ_11G_CTRL_SPEED_LSB                         0x2000
#define LANTIQ_11G_CTRL_AUTONEGOTIATION_ENABLE            0x1000
#define LANTIQ_11G_CTRL_RESTART_AUTONEGOTIATION           0x0200
#define LANTIQ_11G_CTRL_SPEED_FULL_DUPLEX                 0x0100
#define LANTIQ_11G_CTRL_SPEED_MSB                         0x0040

#define LANTIQ_11G_RESET_DONE(phy_control)                   \
    (((phy_control) & (LANTIQ_11G_CTRL_SOFTWARE_RESET)) == 0)
    
/* Phy status fields */
#define LANTIQ_11G_LATCH_LINK_PASS                        0x0004
#define LANTIQ_11G_STATUS_AUTO_NEG_DONE                   0x0020
/*--- #define LANTIQ_11G_STATUS_FULL_DUPLEX                     0x2000 ---*/

#define LANTIQ_11G_AUTONEG_DONE(ip_phy_status)                   \
    (((ip_phy_status) &                                  \
        (LANTIQ_11G_STATUS_AUTO_NEG_DONE)) ==                    \
        (LANTIQ_11G_STATUS_AUTO_NEG_DONE))

/* Link Partner ability */
#define LANTIQ_11G_LINK_100BASETX_FULL_DUPLEX       0x0100
#define LANTIQ_11G_LINK_100BASETX                   0x0080
#define LANTIQ_11G_LINK_10BASETX_FULL_DUPLEX        0x0040
#define LANTIQ_11G_LINK_10BASETX                    0x0020

/* Advertisement register. */
#define LANTIQ_11G_ADVERTISE_NEXT_PAGE              0x8000
#define LANTIQ_11G_ADVERTISE_ASYM_PAUSE             0x0800
#define LANTIQ_11G_ADVERTISE_PAUSE                  0x0400
#define LANTIQ_11G_ADVERTISE_100FULL                0x0100
#define LANTIQ_11G_ADVERTISE_100HALF                0x0080  
#define LANTIQ_11G_ADVERTISE_10FULL                 0x0040  
#define LANTIQ_11G_ADVERTISE_10HALF                 0x0020  

#define LANTIQ_11G_ADVERTISE_ALL (LANTIQ_11G_ADVERTISE_ASYM_PAUSE | LANTIQ_11G_ADVERTISE_PAUSE | \
                            LANTIQ_11G_ADVERTISE_10HALF | LANTIQ_11G_ADVERTISE_10FULL | \
                            LANTIQ_11G_ADVERTISE_100HALF | LANTIQ_11G_ADVERTISE_100FULL)
                       
/* 1000BASET_CONTROL */
#define LANTIQ_11G_ADVERTISE_1000FULL               0x0200

/* Phy Specific status fields */
#define DUPLEX_HALF         0x00
#define DUPLEX_FULL         0x01

#define LANTIQ_11G_STATUS_LINK_MASK                3
#define LANTIQ_11G_STATUS_LINK_SHIFT               0
#define LANTIQ_11G_STATUS_FULL_DUPLEX              (1 << 3)

/*phy debug port  register */
#define LANTIQ_11G_DEBUG_SERDES_REG                5

/* Serdes debug fields */
#define LANTIQ_11G_SERDES_BEACON                   0x0100

#define PHY_LINK_CHANGE_REG 	              0x4
#define PHY_LINK_UP 		              0x400
#define PHY_LINK_DOWN 		              0x800
#define PHY_LINK_DUPLEX_CHANGE 	              0x2000
#define PHY_LINK_SPEED_CHANGE	              0x4000
#define PHY_LINK_INTRS		              (PHY_LINK_UP | PHY_LINK_DOWN | PHY_LINK_DUPLEX_CHANGE | PHY_LINK_SPEED_CHANGE)


#ifndef BOOL
#define BOOL    int
#endif

/*add feature define here*/
//#define FULL_FEATURE

#ifdef CONFIG_LANTIQ_11G_PHY
#undef HEADER_REG_CONF
#undef HEADER_EN
#endif

struct athr_gmac;   /* forward declaration... */

int lantiq_g11_reg_init(void *arg);
int lantiq_g11_phy_is_up(int ethUnit);
int lantiq_g11_phy_is_fdx(int ethUnit, int phyUnit);
int lantiq_g11_phy_speed(int ethUnit, int phyUnit);
BOOL lantiq_g11_phy_is_link_alive(int phyUnit);
int lantiq_g11_phy_setup(void *arg);
unsigned int lantiq_g11_reg_read(unsigned int reg_addr);
void lantiq_g11_reg_write(unsigned int reg_addr, unsigned int reg_val);
int lantiq_g11_ioctl(struct net_device *dev, void *args, int cmd);
int lantiq_11g_register_ops(struct athr_gmac *mac);
int lantiq_11g_in_reset(void *arg);
#endif


