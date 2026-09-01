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

#include "athrs_mac.h"
#include "athrs_phy.h"
#include "athrs_phy_ctrl.h"
#include <linux/workqueue.h>
#include <asm/mach-atheros/atheros_gpio.h>

#define MODULE_NAME "ATHRSF2_PHY"

#define ATHR_LAN_PORT_VLAN          1
#define ATHR_WAN_PORT_VLAN          2
#define ENET_UNIT_LAN 1
#define ENET_UNIT_WAN 0

#define TRUE    1
#define FALSE   0

#define ATHR_PHY0_ADDR   0x0
#define ATHR_PHY1_ADDR   0x1
#define ATHR_PHY2_ADDR   0x2
#define ATHR_PHY3_ADDR   0x3
#define ATHR_PHY4_ADDR   0x4
#define ATHR_PHY5_ADDR   0x5

/*--- #define F2_PHY_DEBUG ---*/ 
#ifdef F2_PHY_DEBUG
#define DPRINTF_PHY(_fmt,...) do {         \
                printk(MODULE_NAME":"_fmt, __VA_ARGS__);      \
} while (0)
#else
#define DPRINTF_PHY(_fmt,...) 
#endif

/*
 * Track per-PHY port information.
 */
typedef struct {
    BOOL   isEnetPort;       /* normal enet port */
    BOOL   isPhyAlive;       /* last known state of link */
    int    ethUnit;          /* MAC associated with this phy port */
    uint32_t phyBase;
    uint32_t phyAddr;          /* PHY registers associated with this phy port */
    uint32_t VLANTableSetting; /* Value to be written to VLAN table */
} athrPhyInfo_t;

/*
 * Per-PHY information, indexed by PHY unit number.
 */

static athrPhyInfo_t athrPhyInfo[] = {
    {TRUE,  
     FALSE,
     ENET_UNIT_WAN,
     0,
     ATHR_PHY0_ADDR,
     ATHR_LAN_PORT_VLAN
    }
};

#define ATHR_IS_ENET_PORT(phyUnit) (athrPhyInfo[phyUnit].isEnetPort)
#define ATHR_IS_PHY_ALIVE(phyUnit) (athrPhyInfo[phyUnit].isPhyAlive)
#define ATHR_ETHUNIT(phyUnit) (athrPhyInfo[phyUnit].ethUnit)
#define ATHR_PHYBASE(phyUnit) (athrPhyInfo[phyUnit].phyBase)
#define ATHR_PHYADDR(phyUnit) (athrPhyInfo[phyUnit].phyAddr)
#define ATHR_VLAN_TABLE_SETTING(phyUnit) (athrPhyInfo[phyUnit].VLANTableSetting)

#define ATHR_IS_ETHUNIT(phyUnit, ethUnit) \
            (ATHR_IS_ENET_PORT(phyUnit) && ATHR_ETHUNIT(phyUnit) == (ethUnit))

#define ATHR_IS_WAN_PORT(phyUnit) (!(ATHR_ETHUNIT(phyUnit)==ENET_UNIT_LAN))
 
static void f2mii_work_handler(struct work_struct *work);
struct workqueue_struct *f2mii_workqueue;
DECLARE_WORK(f2mii_work, f2mii_work_handler);

static athr_gmac_t *athrf2_macs[2];

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void athr_auto_neg(int phyUnit) { 

    int timeout = 100;
    uint16_t phyHwStatus,phyBase,phyAddr;

    phyBase = ATHR_PHYBASE(phyUnit);
    phyAddr = ATHR_PHYADDR(phyUnit);

    phy_reg_write(phyBase, phyAddr, ATHR_PHY_CONTROL, PHY_CTRL_SOFTWARE_RESET | PHY_CTRL_AUTONEGOTIATION_ENABLE);

    /*
     * After the phy is reset, it takes a little while before
     * it can respond properly.
     */

    DPRINTF_PHY("Wait for Autoneg to complete\n");

    do {
        phyHwStatus = phy_reg_read(phyBase, phyAddr, ATHR_PHY_STATUS);
        DPRINTF_PHY("{%s} phyHwStatus 0x%x\n", __func__, phyHwStatus);
    } while((phyHwStatus & PHY_STATUS_AUTO_NEG_IN_PROCESS) && timeout--);

    /*
     * Wait up to 3 seconds for ALL associated PHYs to finish
     * autonegotiation.  The only way we get out of here sooner is
     * if ALL PHYs are connected AND finish autonegotiation.
     */
    timeout = 20;
    for (;;) {
        phyHwStatus = phy_reg_read(phyBase,phyAddr, ATHR_PHY_CONTROL);

        if (ATHR_RESET_DONE(phyHwStatus)) {
            printk(MODULE_NAME": Port %d, Neg Success\n", phyUnit);
            break;
        }
        if (timeout == 0) {
            printk(MODULE_NAME": Port %d, Negogiation timeout\n", phyUnit);
            break;
        }
        if (--timeout == 0) {
            printk(MODULE_NAME": Port %d, Negogiation timeout\n", phyUnit);
            break;
        }

        mdelay(150);
    }

    DPRINTF_PHY(": unit %d phy addr %x ", phyBase, phyAddr);
}

/******************************************************************************
*
* athr_phy_is_link_alive - test to see if the specified link is alive
*
* RETURNS:
*    TRUE  --> link is alive
*    FALSE --> link is down
*/
BOOL athr_phy_is_link_alive(int ethUnit __attribute__((unused))) {

    return ATHR_IS_PHY_ALIVE(0);
}

/******************************************************************************
*
* athr_phy_setup - reset and setup the PHY associated with
* the specified MAC unit number.
*
* Resets the associated PHY port.
*
* RETURNS:
*    TRUE  --> associated PHY is alive
*    FALSE --> no LINKs on this ethernet unit
*/
int athr_phy_setup(athr_gmac_t *mac) {

    int ethUnit = mac->mac_unit;

    DPRINTF_PHY("[%s] ethUint %d\n", __func__, ethUnit);

    mdelay(150);
    ath_avm_gpio_out_bit(18, 0);        /*--- reset ext. PHY before init RGMII- Pins ---*/
    mdelay(150);
    ath_avm_gpio_out_bit(18, 1);        /*--- reset ext. PHY before init RGMII- Pins ---*/
    mdelay(150);

    printk("{%s} PHY-ID 0x%x 0x%x\n", __func__, phy_reg_read(ethUnit, ATHR_PHY0_ADDR, ATHR_PHY_ID1), phy_reg_read(ethUnit, ATHR_PHY0_ADDR, ATHR_PHY_ID2));

    phy_reg_write(ethUnit, ATHR_PHY0_ADDR, 0x1d, 0);           /*--- disable PHY rx-Delay ---*/
    phy_reg_write(ethUnit, ATHR_PHY0_ADDR, 0x1e, 0x2ee);
    phy_reg_write(ethUnit, ATHR_PHY0_ADDR, 0x1d, 5);           /*--- enable PHY tx-Delay ---*/
    phy_reg_write(ethUnit, ATHR_PHY0_ADDR, 0x1e, 0x2d47);

    athr_auto_neg(ATHR_PHY0_ADDR);

    return 0;

}

/******************************************************************************
*
* athr_phy_is_fdx - Determines whether the phy ports associated with the
* specified device are FULL or HALF duplex.
*
* RETURNS:
*    1 --> FULL
*    0 --> HALF
*/
int athr_phy_is_fdx(int ethUnit __attribute__((unused)),int phyUnit) {

    uint32_t  phyBase;
    uint32_t  phyAddr;
    uint16_t  phyHwStatus;
    int       ii = 200;

    DPRINTF_PHY("[%s] ethUint %d phyUnit %d\n", __func__, ethUnit, phyUnit);

    if (athr_phy_is_link_alive(phyUnit)) {

        phyBase = ATHR_PHYBASE(phyUnit);
        phyAddr = ATHR_PHYADDR(phyUnit);

        do {
            phyHwStatus = phy_reg_read(phyBase,phyAddr,ATHR_PHY_SPEC_STATUS);
        } while((!(phyHwStatus & ATHR_STATUS_RESOVLED)) && --ii);

        if (phyHwStatus & ATHER_STATUS_FULL_DUPLEX) {
            return TRUE;
        }
    }
    return FALSE;
}

/******************************************************************************
*
* athr_phy_speed - Determines the speed of phy ports associated with the
* specified device.
*
* RETURNS:
*               ATHR_PHY_SPEED_10T, ATHR_PHY_SPEED_100T;
*               ATHR_PHY_SPEED_1000T;
*/

int athr_phy_speed(int ethUnit __attribute__((unused)), int phyUnit) {

    uint16_t  phyHwStatus;
    uint32_t  phyBase;
    uint32_t  phyAddr;
    int       ii = 200;

    DPRINTF_PHY("[%s] ethUint %d phyUnit %d\n", __func__, ethUnit, phyUnit);

    if (athr_phy_is_link_alive(phyUnit)) {

        phyBase = ATHR_PHYBASE(phyUnit);
        phyAddr = ATHR_PHYADDR(phyUnit);
        do {
            phyHwStatus = phy_reg_read(phyBase,phyAddr,ATHR_PHY_SPEC_STATUS);
        } while((!(phyHwStatus & PHY_SPEC_STATUS_RESOVLED)) && --ii);

        phyHwStatus = ((phyHwStatus & PHY_SPEC_STATUS_LINK_MASK) >> PHY_SPEC_STATUS_LINK_SHIFT);

        switch(phyHwStatus) {
            case 0:
                return ATHR_PHY_SPEED_10T;
            case 1:
                return ATHR_PHY_SPEED_100T;
            case 2:
                return ATHR_PHY_SPEED_1000T;
            default:
                printk("Unkown speed read!\n");
        }
    }

    //printk("athr_phy_speed: link down, returning 10t\n");
    return ATHR_PHY_SPEED_10T;
}

/*****************************************************************************
*
* athr_phy_is_up -- checks for significant changes in PHY state.
*
* A "significant change" is:
*     dropped link (e.g. ethernet cable unplugged) OR
*     autonegotiation completed + link (e.g. ethernet cable plugged in)
*
* When a PHY is plugged in, phyLinkGained is called.
* When a PHY is unplugged, phyLinkLost is called.
*/

int athr_phy_is_up(int ethUnit __attribute__((unused))) {

    uint32_t    phyBase;
    uint32_t    phyAddr;
    uint32_t    status;
    int         ii = 200;

    DPRINTF_PHY("[%s] ethUint %d\n", __func__, ethUnit);

    phyBase = ATHR_PHYBASE(0);
    phyAddr = ATHR_PHYADDR(0);

    do {
        status = phy_reg_read(phyBase, phyAddr, ATHR_PHY_STATUS);
    } while ((status & PHY_STATUS_AUTO_NEG_IN_PROCESS) && --ii);

    status = phy_reg_read(phyBase, phyAddr, ATHR_PHY_SPEC_STATUS);
    if (status & PHY_STATUS_LINK_PASS) {
        ATHR_IS_PHY_ALIVE(0) = TRUE;
    } else {
        ATHR_IS_PHY_ALIVE(0) = FALSE;
    }
    return ATHR_IS_PHY_ALIVE(0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void athrf2_enable_link_intrs(athr_gmac_t *mac __attribute__((unused))) {

	uint32_t phyBase = ATHR_PHYBASE(0);
	uint32_t phyAddr = ATHR_PHYADDR(0);

    DPRINTF_PHY("[%s/%d] mac->mac_unit %d\n", __FUNCTION__, __LINE__, mac->mac_unit);

    /* Enable global PHY link status interrupt */
    phy_reg_write(phyBase, phyAddr, ATHR_PHY_INTR_ENABLE, PHY_STATUS_INTS);

    DPRINTF_PHY("[%s/%d] ATHR_PHY_INTR_ENABLE 0x%x\n", __FUNCTION__, __LINE__, phy_reg_read(phyBase, phyAddr, ATHR_PHY_INTR_ENABLE));

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void athrf2_disable_link_intrs(athr_gmac_t *mac __attribute__((unused))) {

	uint32_t phyBase = ATHR_PHYBASE(0);
	uint32_t phyAddr = ATHR_PHYADDR(0);

    DPRINTF_PHY("[%s/%d] mac->mac_unit %d\n", __FUNCTION__, __LINE__, mac->mac_unit);

    /* disable global PHY link status interrupt */
    phy_reg_write(phyBase, phyAddr, ATHR_PHY_INTR_ENABLE, 0);

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void f2mii_work_handler(struct work_struct *work __attribute__ ((unused))) {

    athr_gmac_t *mac = athrf2_macs[0];

	uint32_t phyBase = ATHR_PHYBASE(0);
	uint32_t phyAddr = ATHR_PHYADDR(0);
    uint16_t status;

	status = phy_reg_read(phyBase, phyAddr, ATHR_PHY_INTR_STATUS);

    DPRINTF_PHY("{%s} phyBase %d phyAddr %d INT-Status 0x%x\n", __FUNCTION__, phyBase, phyAddr, status);

    if (status & PHY_STATUS_INTS) {
        mac->ops->check_link(mac, mac->mac_unit);
    } else  {
        printk(KERN_ERR "<%s: Spurious link interrupt>\n",__func__);
    }

#if defined(ATHR_DUMP_REGISTER)
    athrs_dump_regs();
#endif

    /* clear global link interrupt */
    phy_reg_write(phyBase, phyAddr, ATHR_PHY_INTR_STATUS, status);

    /*--- clear MISC_INT_STATUS !!! ---*/
    ath_reg_rmw_clear(ATH_MISC_INT_STATUS, MIMR_ENET_LINK);

    enable_irq(mac->mac_linkirq);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
irqreturn_t f2_intr(athr_gmac_t *mac) {

    DPRINTF_PHY("{%s} irq %d\n", __func__, mac->mac_linkirq);
    disable_irq_nosync(mac->mac_linkirq);
    queue_work(f2mii_workqueue, &f2mii_work);
    return IRQ_HANDLED;

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int athrf2_reg_init(athr_gmac_t *mac __attribute__((unused))) {

    ath_avm_gpio_out_bit(18, 1);        /*--- reset ext. PHY - before init RGMII- Pins ---*/
    printk ("%s:done\n",__func__);
    
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int athrsf2_register_ops(athr_gmac_t *mac) {

    athr_phy_ops_t *ops = mac->phy;

    printk(KERN_INFO "[%s] mac_unit %d flags 0x%x\n", __FUNCTION__, mac->mac_unit, mac->mac_flags);

    if(!ops)
        ops = kmalloc(sizeof(athr_phy_ops_t), GFP_KERNEL);

    memset(ops,0,sizeof(athr_phy_ops_t));

    ops->mac            =  mac;
    ops->is_up          =  athr_phy_is_up;
    ops->is_alive       =  athr_phy_is_link_alive;
    ops->speed          =  athr_phy_speed;
    ops->is_fdx         =  athr_phy_is_fdx;
    ops->ioctl          =  NULL;
    ops->setup          =  athr_phy_setup;
    ops->stab_wr        =  NULL;
    ops->link_isr       =  f2_intr;
    ops->en_link_intrs  =  athrf2_enable_link_intrs;
    ops->dis_link_intrs =  athrf2_disable_link_intrs;
    ops->read_phy_reg   =  NULL;
    ops->write_phy_reg  =  NULL;
    ops->read_mac_reg   =  NULL;
    ops->write_mac_reg  =  NULL;
    ops->init           =  athrf2_reg_init;
    ops->port_map       =  0x1;

    mac->phy = ops;
    mac->name = MODULE_NAME;

    if (mac->mac_unit == 0) {
        athrf2_macs[0]     = mac;
    } else {
        kfree(ops);
        return -EFAULT;
    }

    f2mii_workqueue = create_singlethread_workqueue("f2mii_workqueue");
    if(f2mii_workqueue == NULL) {
        printk(KERN_ERR "[%s] create_singlethread_workqueue(\"f2mii_workqueue\" failed\n", __FUNCTION__);
        kfree(ops);
        return -EFAULT;
    }

    return 0;
}

