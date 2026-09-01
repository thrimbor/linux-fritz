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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <asm/mach_avm.h>

#include "athrf1_phy.h"
#include "athrs_phy.h"
#include "athrs_mac.h"
#include "lantiq_11g_phy.h"

#include <asm/prom.h>
#include <linux/workqueue.h>
#include <linux/avm_led_event.h>
#if defined(CONFIG_AVM_CPMAC)
#include "../../avm_cpmac/cpmac_interface.h"
#endif /*--- #if defined(CONFIG_AVM_CPMAC) ---*/

#define MODULE_NAME "LANTIQ_11G_PHY"

#define ag7240_mac_base(_no)    (_no) ? ATH_GE1_BASE    : ATH_GE0_BASE

#define LANTIQ_11G_LAN_PORT_VLAN          1
#define LANTIQ_11G_WAN_PORT_VLAN          2
#define ENET_UNIT_LAN 1
#define ENET_UNIT_WAN 0

#define TRUE    1
#define FALSE   0

#define LANTIQ_11G_PHY_MAX 1
#define LANTIQ_11G_PHY0_ADDR   0x0

#define PHY11G_CHIP_ID                      0x0302
#define PHY11G_CHIP_ID_14                   0xD565
#define PHY11G_SUB_ID                       0x60D1
#define PHY11G_SUB_ID_14                    0xA400

#undef DPRINTF

/*--- #define DEBUG_PHY11G ---*/
#if defined(DEBUG_PHY11G)
#define DPRINTF(...)        printk(KERN_ERR __VA_ARGS__)
#else
#define DPRINTF(...)        
#endif

/*
 * Track per-PHY port information.
 */
union phy_irq_status {
    struct _phy_irq_status {
        unsigned int reserved1 : 16;
        unsigned int wake_on_lan : 1;
        unsigned int msre : 1;
        unsigned int nprx : 1;
        unsigned int nptx : 1;
        unsigned int ane : 1;
        unsigned int anc : 1;
        unsigned int reserved2 : 4;
        unsigned int adsc : 1;
        unsigned int polariy : 1;
        unsigned int mdix : 1;
        unsigned int duplex : 1;
        unsigned int linkspeed : 1;
        unsigned int linkstate : 1;
    } Bits;
    unsigned int reg;
}; 

typedef struct {
    BOOL   isEnetPort;       /* normal enet port */
    BOOL   isPhyAlive;       /* last known state of link */
    uint32_t MdioAddr;
    uint32_t VLANTableSetting; /* Value to be written to VLAN table */
    athr_gmac_t *mac;
} lantiq_11gPhyInfo_t;

/*
 * Per-PHY information, indexed by PHY unit number.
 */
static lantiq_11gPhyInfo_t lantiq_11gPhyInfo[] = {
    {
     .isEnetPort       = TRUE,   /* port 1 -- LAN port 1 */
     .isPhyAlive       = FALSE,
     .MdioAddr         = LANTIQ_11G_PHY0_ADDR,
     .VLANTableSetting = LANTIQ_11G_LAN_PORT_VLAN
    }
};
#define   AG7240_NMACS  (sizeof(lantiq_11gPhyInfo) / sizeof(lantiq_11gPhyInfo[0]))

#define LANTIQ_11G_IS_ENET_PORT(phyUnit) (lantiq_11gPhyInfo[phyUnit].isEnetPort)
#define LANTIQ_11G_IS_PHY_ALIVE(phyUnit) (lantiq_11gPhyInfo[phyUnit].isPhyAlive)
#define LANTIQ_11G_MDIOADDR(phyUnit) (lantiq_11gPhyInfo[phyUnit].MdioAddr)
#define LANTIQ_11G_VLAN_TABLE_SETTING(phyUnit) (lantiq_11gPhyInfo[phyUnit].VLANTableSetting)

static void mii_gphy_work_handler(struct work_struct *work);
struct workqueue_struct *mii_gphy_workqueue;
DECLARE_WORK(mii_gphy_work, mii_gphy_work_handler);


void lantiq_11g_phy_reset(athr_gmac_t * mac);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
uint16_t ag7240_mdio_read(unsigned int addr, uint8_t reg) {

    return athr_gmac_mii_read(0, addr, reg);
}
EXPORT_SYMBOL(ag7240_mdio_read);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ag7240_mdio_write(unsigned int addr, uint8_t reg, uint16_t data) {

    athr_gmac_mii_write(0, addr, reg, data);
}
EXPORT_SYMBOL(ag7240_mdio_write);

/*------------------------------------------------------------------------------------------*\
 * Used for Interrupt Handler
\*------------------------------------------------------------------------------------------*/
irqreturn_t mii_gphy_int_handler(athr_gmac_t *mac) {

    disable_irq_nosync(mac->mac_linkirq);
    queue_work(mii_gphy_workqueue, &mii_gphy_work);
    DPRINTF("[%s] irq %d\n", __FUNCTION__, mac->mac_linkirq);
    return IRQ_HANDLED;

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static uint16_t lantiq_11g_read_irq_status(athr_gmac_t *mac) {
    unsigned int mdio_addr = LANTIQ_11G_MDIOADDR(mac->mac_unit);
    return athr_gmac_mii_read(mac->mac_unit, mdio_addr, LANTIQ_11G_PHY_INTR_STATUS);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static uint16_t lantiq_11g_read_link_status(athr_gmac_t *mac) {
    unsigned int mdio_addr = LANTIQ_11G_MDIOADDR(mac->mac_unit);
    return athr_gmac_mii_read(mac->mac_unit, mdio_addr, LANTIQ_11G_PHY_SPEC_STATUS);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void mii_gphy_work_handler(struct work_struct *work __attribute__ ((unused))) {

    athr_gmac_t *mac = lantiq_11gPhyInfo[0].mac;
#if defined(DEBUG_PHY11G)
    uint32_t mdio_addr = LANTIQ_11G_MDIOADDR(mac->mac_unit);
#endif
    union phy_irq_status  status;

    status.reg = lantiq_11g_read_irq_status(mac);
    DPRINTF("[%s] PHY %d status 0x%x\n", __FUNCTION__, mdio_addr, status.reg);

    /*--- interrupt status bit ---*/
    if(status.Bits.linkstate || status.Bits.linkspeed || status.Bits.duplex || status.Bits.mdix) { 
        mac->ops->check_link(mac, mac->mac_unit);
    }
    enable_irq(mac->mac_linkirq);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void lantiq_11g_enable_linkIntrs(athr_gmac_t *mac) {

    unsigned int mdio_addr = LANTIQ_11G_MDIOADDR(mac->mac_unit);
    DPRINTF("[%s] enable Interrupt\n", __FUNCTION__);

    athr_gmac_mii_write(mac->mac_unit, mdio_addr, LANTIQ_11G_PHY_INTR_ENABLE, (uint16_t)
                (1 << 0) |   /*--- Link State change mask (enable) ---*/
                (1 << 1) |   /*--- Link Speed change mask (enable) ---*/
                (1 << 2) |   /*--- Link Duplex change mask (enable) ---*/
                (1 << 3) |   /*--- Link MDIX change mask (enable) ---*/
                (1 << 4) |   /*--- Link MDI Polarity change mask (enable) ---*/
                /*--- (1 << 5) | ---*/   /*--- Link Speed auto down shift detect mask (enable) ---*/
                /*--- (1 << 10) | ---*/   /*--- auto neg complete mask (enable) ---*/
                /*--- (1 << 11) | ---*/   /*--- auto neg error mask (enable) ---*/
                /*--- (1 << 12) | ---*/   /*--- next page transmit mask (enable) ---*/
                /*--- (1 << 13) | ---*/   /*--- next page receive mask (enable) ---*/
                /*--- (1 << 14) | ---*/   /*--- master slave resolution error mask (enable) ---*/
                /*--- (1 << 15) | ---*/   /*--- wake on LAN Event mask (enable) ---*/
                0);
    DPRINTF("[%s] Lantiq 11G Reg(0x19): 0x%x\n", __FUNCTION__, athr_gmac_mii_read(mac->mac_unit, mdio_addr, LANTIQ_11G_PHY_INTR_ENABLE));
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void lantiq_11g_disable_linkIntrs(athr_gmac_t *mac) {

    unsigned int mdio_addr = LANTIQ_11G_MDIOADDR(mac->mac_unit);
    DPRINTF("[%s] disable Interrupt\n", __FUNCTION__);

    athr_gmac_mii_write(mac->mac_unit, mdio_addr, LANTIQ_11G_PHY_INTR_ENABLE, (uint16_t)0);

}

/*------------------------------------------------------------------------------------------*\
 * lantiq_11g_phy_is_link_alive - test to see if the specified link is alive
 *
 * RETURNS:
 *    TRUE  --> link is alive
 *    FALSE --> link is down
\*------------------------------------------------------------------------------------------*/
int lantiq_11g_phy_is_link_alive(int ethUnit __attribute__ ((unused))) {

    return LANTIQ_11G_IS_PHY_ALIVE(0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
BOOL lantiq_11g_phy_power_control(int ethUnit __attribute__ ((unused)), int power_up)
{
	uint16_t phyHwStatus;
	uint32_t mdio_addr;

	mdio_addr = LANTIQ_11G_MDIOADDR(ethUnit);

    DPRINTF("[%s] mdioAddr 0x%x\n", __FUNCTION__, mdio_addr);

	phyHwStatus = athr_gmac_mii_read(ethUnit, mdio_addr, LANTIQ_11G_PHY_CONTROL);
    phyHwStatus &= ~(1 << 11);
    if(power_up == 0)
        phyHwStatus |= 1 << 11;

	athr_gmac_mii_write(ethUnit, mdio_addr, LANTIQ_11G_PHY_CONTROL, phyHwStatus);

	if (phyHwStatus & (1 << 11)) {
        DPRINTF("[%s] Power DOWN\n", __FUNCTION__);
	    return FALSE;
	}

    DPRINTF("[%s] Power UP\n", __FUNCTION__);
    return TRUE;
}

/******************************************************************************
*
* lantiq_11g_phy_setup - reset and setup the PHY associated with
* the specified MAC unit number.
*
* Resets the associated PHY port.
*
* RETURNS:
*    TRUE  --> associated PHY is alive
*    FALSE --> no LINKs on this ethernet unit
*/

BOOL lantiq_11g_phy_setup(athr_gmac_t *mac) {

    unsigned int mdio_addr = LANTIQ_11G_MDIOADDR(mac->mac_unit);
    uint16_t    phyID;
    uint32_t    timeout = 0x400;

    avm_gpio_out_bit(CONFIG_AR7242_GPIO_RESET_PIN, 0);  /*--- reset external PHY ---*/
    mdelay(200);
    avm_gpio_out_bit(CONFIG_AR7242_GPIO_RESET_PIN, 1);  /*--- release external PHY ---*/

    DPRINTF("[%s] mac->mac_unit %d \n", __FUNCTION__, mac->mac_unit);

    do {
        phyID = athr_gmac_mii_read(mac->mac_unit, mdio_addr, LANTIQ_11G_PHY_ID1);
        mdelay(1);
        DPRINTF(KERN_ERR "[%s] phyID 0x%x\n", __func__, phyID);
    } while ((phyID == 0xFFFF) && --timeout);

    switch (phyID) {
        case PHY11G_CHIP_ID:
            athr_gmac_mii_write(mac->mac_unit, mdio_addr, 0x1d, 0x100);      /*--- spezial Lantiq - TPGDATA = 0x100 ---*/
            athr_gmac_mii_write(mac->mac_unit, mdio_addr, 0x9, 0x700);      /*--- spezial Lantiq - GCTRL.MSPT = 1 ---*/
            printk("[%s] found PHY Version 1.3\n", mac->name);
            break;
        case PHY11G_CHIP_ID_14:
            /*--- setup dcdc-settings ---*/
            athr_gmac_mii_write(mac->mac_unit, mdio_addr, 0x0d, 0x001F);
            athr_gmac_mii_write(mac->mac_unit, mdio_addr, 0x0e, 0x0FC6);
            athr_gmac_mii_write(mac->mac_unit, mdio_addr, 0x0d, 0x401F);
            athr_gmac_mii_write(mac->mac_unit, mdio_addr, 0x0e, 0x0052);
            athr_gmac_mii_write(mac->mac_unit, mdio_addr, 0x0d, 0x0000);
            break;
    }

	return 0;
}

/******************************************************************************
*
* lantiq_11g_phy_is_fdx - Determines whether the phy ports associated with the
* specified device are FULL or HALF duplex.
*
* RETURNS:
*    1 --> FULL
*    0 --> HALF
*/
int lantiq_11g_phy_is_fdx(int ethUnit, int phyUnit __attribute__ ((unused))) {

    athr_gmac_t * mac = lantiq_11gPhyInfo[ethUnit].mac;
    uint32_t    mdio_addr = LANTIQ_11G_MDIOADDR(ethUnit);
    uint32_t    phystatus,linkstatus;

    phystatus = athr_gmac_mii_read(mac->mac_unit, mdio_addr, LANTIQ_11G_PHY_STATUS);     /*--- default-Phy-register ---*/

    if (phystatus & LANTIQ_11G_LATCH_LINK_PASS) {
        linkstatus = lantiq_11g_read_link_status(mac);

        if(linkstatus & LANTIQ_11G_STATUS_FULL_DUPLEX) {
            return TRUE;
        }
    } 
    return FALSE;

}

/******************************************************************************
*
* lantiq_11g_phy_speed - Determines the speed of phy ports associated with the
* specified device.
*
* RETURNS:
*               ATHR_PHY_SPEED_10T, ATHR_PHY_SPEED_100T;
*               ATHR_PHY_SPEED_1000T;
*/
int lantiq_11g_phy_speed(int ethUnit, int phyUnit __attribute__ ((unused))) {

    athr_gmac_t * mac = lantiq_11gPhyInfo[ethUnit].mac;
    uint32_t    mdio_addr = LANTIQ_11G_MDIOADDR(ethUnit);
    uint32_t    phystatus,linkstatus;

    phystatus = athr_gmac_mii_read(mac->mac_unit, mdio_addr, LANTIQ_11G_PHY_STATUS);     /*--- default-Phy-register ---*/

    if (phystatus & LANTIQ_11G_LATCH_LINK_PASS) {

        linkstatus = lantiq_11g_read_link_status(mac);

        switch (linkstatus & LANTIQ_11G_STATUS_LINK_MASK) {
            case 0 :  /*--- speed 10BASE-T ---*/
                DPRINTF("[%s] '%s' speed = %d\n", __FUNCTION__, mac->name, 10);
                return ATHR_PHY_SPEED_10T;
            case 1:	  /*--- speed 100BASE-TX ---*/
                DPRINTF("[%s] '%s' speed = %d\n", __FUNCTION__, mac->name, 100);
                return ATHR_PHY_SPEED_100T;
            case 2:	  /*--- speed 1000BASE-T ---*/
                DPRINTF("[%s] '%s' speed = %d\n", __FUNCTION__, mac->name, 1000);
                return ATHR_PHY_SPEED_1000T;
            default:
                DPRINTF("[%s] '%s' speed = UNKNOWN\n", __FUNCTION__, mac->name);
        }
    }
    return ATHR_PHY_SPEED_UNKNOWN;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void lantiq_11g_phy_reset(athr_gmac_t * mac) {

    lantiq_11g_phy_setup(mac);

    if (mac_has_flag(mac, ETH_LINK_INTERRUPT)) {
        lantiq_11g_enable_linkIntrs(mac);
    }

}

/*****************************************************************************
*
* lantiq_11g_phy_is_up -- checks for significant changes in PHY state.
*
* A "significant change" is:
*     dropped link (e.g. ethernet cable unplugged) OR
*     autonegotiation completed + link (e.g. ethernet cable plugged in)
*
* When a PHY is plugged in, phyLinkGained is called.
* When a PHY is unplugged, phyLinkLost is called.
*/

int lantiq_11g_phy_is_up(int ethUnit) {

	uint16_t status;
	uint32_t mdio_addr;
    uint32_t timeout = 200;

	mdio_addr = LANTIQ_11G_MDIOADDR(ethUnit);

    do {
        status = ag7240_mdio_read(mdio_addr, LANTIQ_11G_PHY_STATUS);     /*--- default-Phy-register ---*/
    } while (!LANTIQ_11G_AUTONEG_DONE(status) && timeout--);

    if (status & LANTIQ_11G_LATCH_LINK_PASS) {
	    LANTIQ_11G_IS_PHY_ALIVE(ethUnit) = TRUE;
	} else {
	    LANTIQ_11G_IS_PHY_ALIVE(ethUnit) = FALSE;
    }

    return LANTIQ_11G_IS_PHY_ALIVE(ethUnit);

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int lantiq_11g_reg_init(athr_gmac_t * mac __attribute__ ((unused))) {
    DPRINTF("[%s] \n", __FUNCTION__);

    avm_gpio_ctrl(CONFIG_AR7242_MDIOINT_PIN, GPIO_PIN, GPIO_INPUT_PIN);      /*--- configure MDIO-INT Pin ---*/
    avm_gpio_ctrl(CONFIG_AR7242_GPIO_RESET_PIN, GPIO_PIN, GPIO_OUTPUT_PIN); 
    avm_gpio_out_bit(CONFIG_AR7242_GPIO_RESET_PIN, 0);                       /*--- reset external PHY ---*/

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int lantiq_11gf1e_ioctl(struct net_device *dev, void *args, int cmd) {

    struct ifreq * ifr = (struct ifreq *) args;
    struct eth_cfg_params *ethcfg;
    athr_gmac_t *mac = ATHR_MAC_PRIV(dev);
	uint32_t mdio_addr = LANTIQ_11G_MDIOADDR(0);

    ethcfg = (struct eth_cfg_params *)ifr->ifr_data;

    printk(KERN_ERR "[%s] ethcfg->phy_reg 0x%x cmd %s\n", __FUNCTION__, ethcfg->phy_reg, cmd == RD_PHY ? "read" : "write");

     if(cmd  == RD_PHY) {
        ethcfg->val = athr_gmac_mii_read(mac->mac_unit, mdio_addr, ethcfg->phy_reg);
    }
    else if(cmd  == WR_PHY) {
        athr_gmac_mii_write(mac->mac_unit,  mdio_addr, ethcfg->phy_reg, ethcfg->val);
    } else
        return -EINVAL;

    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int lantiq_11g_register_ops(athr_gmac_t *mac) 
{

    athr_phy_ops_t *ops = mac->phy;

    printk(KERN_ERR "[%s] register Lantiq 11G driver for Atheros Ethernet Driver\n", __FUNCTION__);

    if(!ops)
        ops = kmalloc(sizeof(athr_phy_ops_t), GFP_KERNEL);

    memset(ops, 0, sizeof(athr_phy_ops_t));

    mac->name = "Lantiq 11G";

    ops->mac            =  mac;
    ops->is_up          =  lantiq_11g_phy_is_up;
    ops->is_alive       =  lantiq_11g_phy_is_link_alive;
    ops->speed          =  lantiq_11g_phy_speed;
    ops->is_fdx         =  lantiq_11g_phy_is_fdx;
    /*--- ops->ioctl          =  lantiq_11gf1e_ioctl; ---*/
    ops->setup          =  lantiq_11g_phy_setup;
    ops->stab_wr        =  NULL;
    ops->link_isr       =  mii_gphy_int_handler;
    ops->en_link_intrs  =  lantiq_11g_enable_linkIntrs;
    ops->dis_link_intrs =  lantiq_11g_disable_linkIntrs;
    ops->read_phy_reg   =  NULL;
    ops->write_phy_reg  =  NULL;
    ops->read_mac_reg   =  NULL;
    ops->write_mac_reg  =  NULL;
    ops->init           =  lantiq_11g_reg_init;

    mac->phy = ops;
    mac->name = "Lantiq PHY11G";

    DPRINTF("[%s] RST_RESET 0x%x GPIO_OUT 0x%x\n", __FUNCTION__,
            *((volatile unsigned int *)0xB806001C),
            *((volatile unsigned int *)0xB8040008));

    lantiq_11gPhyInfo[mac->mac_unit].mac = mac;

    mii_gphy_workqueue = create_singlethread_workqueue("mii_gphy_workqueue");
    if(mii_gphy_workqueue == NULL) {
        printk(KERN_ERR "[%s] create_singlethread_workqueue(\"mii_gphy_workqueue\" failed\n", __FUNCTION__);
        return -EFAULT;
    }

    return 0;
}

