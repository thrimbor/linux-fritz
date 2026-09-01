/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,...,2013 AVM GmbH <fritzbox_info@avm.de>
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

#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <asm/mach_avm.h>

#if defined(CONFIG_MIPS_OHIO)
#include <asm/mips-boards/ohio.h>
#include <asm/mach-ohio/hw_mdio.h>
#endif/*--- #if defined(CONFIG_MIPS_AR7) ---*/

#if defined(CONFIG_MIPS_AR7)
#include <asm/mach-ar7/hw_mdio.h>
#endif/*--- #if defined(CONFIG_MIPS_AR7) ---*/

#if defined(CONFIG_MIPS_UR8)
#include <asm/mach-ur8/hw_mdio.h>
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/

#if defined(CONFIG_AVM_POWERMETER)
#include <linux/avm_power.h>
#endif /*--- #if defined(CONFIG_AVM_POWERMETER) ---*/

#if !defined(CONFIG_NETCHIP_ADM69961)
#define CONFIG_NETCHIP_ADM69961
#endif /*--- #if !defined(CONFIG_NETCHIP_ADM69961) ---*/
#if !defined(CONFIG_NETCHIP_AR8216)
#define CONFIG_NETCHIP_AR8216
#endif /*--- #if !defined(CONFIG_NETCHIP_AR8216) ---*/
#include <linux/avm_event.h>
#include <linux/avm_cpmac.h>
#include <linux/adm_reg.h>

#include "cpmac_if.h"
#include "cpmac_const.h"
#include "cpmac_debug.h"
#include "cpmac_reg.h"
#include "cpphy_const.h"
#include "cpphy_types.h"
#include "cpphy_mdio.h"
#include "cpphy_mgmt.h"
#include "adm6996.h"
#include "tantos.h"
#include "cpphy_adm6996.h"
#include "cpphy_ar8216.h"
#include "cpmac_puma_if.h"
#include "mii_reg.h"

#if defined(CONFIG_AVM_LED_EVENTS) || defined(CONFIG_AVM_LED_EVENTS_MODULE)
#include <linux/avm_led_event.h>
#endif /*--- #if defined(CONFIG_AVM_LED_EVENTS) || defined(CONFIG_AVM_LED_EVENTS_MODULE) ---*/
#if defined(CONFIG_AVM_LED)
#include <linux/avm_led.h>
#endif /*--- #if defined(CONFIG_AVM_LED) ---*/


/* FIXME: Temporarily define this here, until the include file is available */
#define TNETD73XX_EPHY_TRIM_VALUE 0xAC
#if !defined(CONFIG_MIPS_UR8)
extern unsigned int cpmac_devices_installed;
#endif


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned short mdio_read(cpphy_mdio_t *mdio, unsigned short phy, unsigned short address) {
    return cpphy_mdio_user_access_read(mdio, address, phy);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned short mdio_write(cpphy_mdio_t  *mdio, 
                          unsigned short phy, 
                          unsigned short address,
                          unsigned short value) {
    cpphy_mdio_user_access_write(mdio, address, phy, value);
    return 0;
}


#if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_mdix_delay(cpphy_mdio_t *mdio) {
    if(mdio->nway_mode & NWAY_AUTOMDIX) {
        mdio->timeout = CPPHY_MDIX_TO + CPPHY_AUTOMDIX_DELAY_MIN / 10;
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_wait_for_access_complete(cpphy_mdio_t *mdio) {
    while(MDIO_USER_ACCESS(mdio->inst) & MDIO_USER_ACCESS_GO) {
        if(unlikely(in_atomic())) {
            /* TODO This is an ugly hack that will harm system stability! At the moment it *\
            \* is necessary for the W721V, but should be fixed ASAP                        */
            /* busy waiting */;
        } else {
            schedule();
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_user_access(cpphy_mdio_t   *mdio,
                                   unsigned int    method,
                                   unsigned short  regadr,
                                   unsigned short  phyadr,
                                   unsigned short  data) {
    unsigned int  control;

    control = MDIO_USER_ACCESS_GO | (method) |
              MDIO_USER_ACCESS_REGADDR(regadr & 0x1f) |
              MDIO_USER_ACCESS_PHYADDR(phyadr & 0x1f) |
              MDIO_USER_ACCESS_DATA(data & 0xffff);
    MDIO_USER_ACCESS(mdio->inst) = control;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mdio_user_access_read(cpphy_mdio_t   *mdio,
                                         unsigned short  regadr,
                                         unsigned short  phyadr) {
    unsigned int value;

    if(down_interruptible(&mdio->semaphore)) {
        DEB_WARN("[%s] Could not obtain semaphore! Running anyway.\n", __FUNCTION__);
        return 0;
    }
    cpphy_mdio_wait_for_access_complete(mdio);  /* Wait until UserAccess ready */
    cpphy_mdio_user_access(mdio, 0, regadr, phyadr, 0); /* no Write Bit */
    cpphy_mdio_wait_for_access_complete(mdio);  /* Wait for Read to complete */

    value = MDIO_USER_ACCESS(mdio->inst) & MDIO_USER_ACCESS_DATA(0xffff);
    up(&mdio->semaphore);
    return value;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mdio_user_access_write(cpphy_mdio_t   *mdio,
                                  unsigned short  regadr,
                                  unsigned short  phyadr,
                                  unsigned short  data) {
    if(down_interruptible(&mdio->semaphore)) {
        DEB_WARN("[%s] Could not obtain semaphore! Running anyway.\n", __FUNCTION__);
        return;
    }
    cpphy_mdio_wait_for_access_complete(mdio);  /* Wait until UserAccess ready */
    cpphy_mdio_user_access(mdio, MDIO_USER_ACCESS_WRITE, regadr, phyadr, data);
    up(&mdio->semaphore);
}


/*------------------------------------------------------------------------------------------*\
 * Function Name : WriteEphyTestRegs                                                        *
 * Purpose       : Program EPHY registers                                                   *
 * Parameters    : PHY number, register ID, register value                                  *
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
static void cpphy_mdio_WriteEphyTestRegs(cpphy_mdio_t *mdio,
                                         unsigned int  MiiPhyNum,
                                         unsigned int  TestCfgRegNum,
                                         unsigned int  CfgValue) {
    /* This sequence will change the PHY to test mode */
    cpphy_mdio_user_access_write(mdio, 20, MiiPhyNum, 0x0000);
    cpphy_mdio_user_access_write(mdio, 20, MiiPhyNum, 0x0400);
    cpphy_mdio_user_access_write(mdio, 20, MiiPhyNum, 0x0000);
    cpphy_mdio_user_access_write(mdio, 20, MiiPhyNum, 0x0400);

    /* Write register value */
    cpphy_mdio_user_access_write(mdio, 23, MiiPhyNum, 0x8100 | (CfgValue & 0xFF));
    cpphy_mdio_user_access_write(mdio, 20, MiiPhyNum, 0x4400 | (TestCfgRegNum & 0xFF));

    /* Back to normal mode */
    cpphy_mdio_user_access_write(mdio, 20, MiiPhyNum, 0x0000);
}
#endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_mdix_update(unsigned int On __attribute__ ((unused))) {
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    avm_gpio_out_bit(GPIO_BIT_MII_MDIX, On ? 1 : 0);
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_timeout(cpphy_mdio_t *mdio) {
    if(mdio->nway_mode & NWAY_AUTOMDIX) {
        /* AutoMdix not supported */
        /* Indicate to Ticks() that MDI/MDIX mode switch is needed */
        mdio->mdix ^= CPPHY_MDIO_FLG_MDIX_ON;    /* toggle MDI / MDIX */
        DEB_INFO("cpphy_mdio_timeout, xchange mdix: %u\n", mdio->mdix);
        cpphy_mdio_mdix_update(mdio->mdix);

        /* Reset state machine to FOUND */
        mdio->state = CPPHY_MDIO_ST_FOUND;
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_set_phy_led(cpphy_mdio_t *mdio) {
#   if defined(CONFIG_AVM_LED)
    if(mdio->cpmac_priv->led[mdio->cpmac_priv->inst].handle != 0) {
        avm_led_action_with_handle(mdio->cpmac_priv->led[mdio->cpmac_priv->inst].handle,
                                   (mdio->state == CPPHY_MDIO_ST_LINKED) ? avm_led_on
                                                                         : avm_led_off);
    }
#   endif /*--- #if defined(CONFIG_AVM_LED) ---*/
#   if defined(CONFIG_AVM_LED_EVENTS) || defined(CONFIG_AVM_LED_EVENTS_MODULE)
    if(led_event_action) {
        (*led_event_action)(LEDCTL_VERSION,   event_lan1_active 
                                            + (mdio->cpmac_priv->inst << 1) 
                                            + (mdio->state == CPPHY_MDIO_ST_LINKED) ? 0 : 1, 1);
    }
#   endif /*--- #if defined(CONFIG_AVM_LED_EVENTS) || defined(CONFIG_AVM_LED_EVENTS_MODULE) ---*/
    if(cpmac_global.event_data.port[mdio->inst].link != (mdio->state == CPPHY_MDIO_ST_LINKED)) {
        /* Nothing to do for low phy when switch is connected to high cpmac */
        /* -> dont register low cpphy driver at cpmac */
        cpmac_global.event_data.port[mdio->inst].link = (mdio->state == CPPHY_MDIO_ST_LINKED);
        cpmac_global.event_data.port[mdio->inst].speed100 = !mdio->slow_speed;
        cpmac_global.event_data.port[mdio->inst].speed =   (!mdio->slow_speed)
                                                         ? avm_event_ethernet_speed_100M
                                                         : avm_event_ethernet_speed_10M;
        cpmac_global.event_data.port[mdio->inst].fullduplex = !mdio->half_duplex;
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_reset_phy(cpphy_mdio_t *mdio) {
    DEB_INFO("cpphy_mdio_reset_phy (%u)\n", mdio->phy_num);
    cpphy_mdio_user_access_write(mdio, PHY_CONTROL_REG, mdio->phy_num, PHY_RESET);

    /* Read control register until Phy Reset is complete */
    while(cpphy_mdio_user_access_read(mdio, PHY_CONTROL_REG, mdio->phy_num) & PHY_RESET);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_check_pdown_state(cpphy_mdio_t *mdio) {
    cpmac_global.power.timeout = 1 * HZ;
    if(mdio->power_down && !(cpphy_mdio_user_access_read(mdio, PHY_GENERIC_STATUS_REG, mdio->phy_num) &
                             PHY_STATUS_MD)) {
        cpphy_mdio_user_access_write(mdio, PHY_CONTROL_REG, mdio->phy_num, PHY_PDOWN);
        mdio->state = CPPHY_MDIO_ST_PDOWN;
    } else if(mdio->timeout) {
        mdio->timeout--;
    } else {
        DEB_INFO("cpphy_mdio_check_pdown_state, tmo\n");
        cpphy_mdio_timeout(mdio);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_finding_state(cpphy_mdio_t *mdio) {
    if(mdio->timeout) {
        mdio->timeout--;
    } else if(MDIO_ALIVE & (1 << mdio->phy_num)) {
        unsigned int power_down_capable;

        /*  Phy Found! */
        mdio->state = CPPHY_MDIO_ST_FOUND;
        /* powerdown for external phy */
        power_down_capable = (cpphy_mdio_user_access_read(mdio, PHY_Identifier_1, mdio->phy_num) == 0x2e) &&
                            ((cpphy_mdio_user_access_read(mdio, PHY_Identifier_2, mdio->phy_num) & 0xfff0) == 0xcc60);
        mdio->power_down = mdio->power_down && power_down_capable;
        DEB_INFO("cpphy_mdio_finding_state, phy_num: %u %u\n", mdio->phy_num, mdio->power_down);
    } else {
        DEB_INFO("cpphy_mdio_finding_state, timed out looking for a Phy (MDIO_ALIVE = %x)\n", MDIO_ALIVE);
    }
}


/*------------------------------------------------------------------------------------------*\
  This algo is almost according to spec of TMS320C6000 DSP
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_found_state(cpphy_mdio_t *mdio) {
    /* Reset the Phy and proceed with auto-negotiation */
    cpphy_mdio_reset_phy(mdio);

#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    /* Fix phy according to TI */
    if(!mdio->switch_config.is_switch) {
        cpphy_mdio_WriteEphyTestRegs(mdio, 31, 0x16, TNETD73XX_EPHY_TRIM_VALUE);
    }
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/

    /* Now setup the MDIOUserPhySel register */
    MDIO_USER_PHY_SELECT(mdio->inst) = mdio->phy_num;

    /* only using NWAY compliant Phy */
    DEB_INFO("cpphy_mdio_found_state, NWAY\n");
    cpphy_mdio_user_access_write(mdio, NWAY_ADVERTIZE_REG, mdio->phy_num, 
                                 (unsigned short) (mdio->nway_mode & ~NWAY_AUTOMDIX));
    cpphy_mdio_user_access_write(mdio, PHY_CONTROL_REG, mdio->phy_num, AUTO_NEGOTIATE_EN);
    cpphy_mdio_user_access_write(mdio, PHY_CONTROL_REG, mdio->phy_num, AUTO_NEGOTIATE_EN|RENEGOTIATE);
    mdio->state = CPPHY_MDIO_ST_NWAY_START;
    mdio->timeout = CPPHY_NWST_TO;
    cpphy_mdio_mdix_delay(mdio);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_pdown_state(cpphy_mdio_t *mdio) {
    if(cpphy_mdio_user_access_read(mdio, PHY_GENERIC_STATUS_REG, mdio->phy_num) & PHY_STATUS_MD) {
        /* Reset state machine to FOUND */
        mdio->state = CPPHY_MDIO_ST_FOUND;
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_nway_start_state(cpphy_mdio_t *mdio) {
    unsigned int PhyMode;

    /* Wait for Negotiation to start */
    PhyMode = cpphy_mdio_user_access_read(mdio, PHY_CONTROL_REG, mdio->phy_num);

    /* auto negotiation has started */
    if(!(PhyMode & RENEGOTIATE)) {
        /*Flush pending latch bits*/
        cpphy_mdio_user_access_read(mdio, PHY_STATUS_REG, mdio->phy_num);
        mdio->timeout = CPPHY_NWDN_TO;
        mdio->state = CPPHY_MDIO_ST_NWAY_WAIT;
        cpphy_mdio_mdix_delay(mdio);  /* If AutoMdix set new delay */
    } else if(mdio->timeout) {
        mdio->timeout--;
    } else {
        cpphy_mdio_timeout(mdio);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_nway_wait_state(cpphy_mdio_t *mdio) {
    unsigned int PhyStatus, NWAYREadvertise, NegMode, i, j;

    PhyStatus = cpphy_mdio_user_access_read(mdio, PHY_STATUS_REG, mdio->phy_num);
    if(PhyStatus & NWAY_COMPLETE) {
        NWAYREadvertise = cpphy_mdio_user_access_read(mdio, NWAY_REMADVERTISE_REG, mdio->phy_num);

        /* Negotiated mode is what we and the remote have in common (limit to fields below) */
        NegMode = (mdio->nway_mode & NWAYREadvertise) & (NWAY_FD100|NWAY_HD100|NWAY_FD10|NWAY_HD10);

        DEB_INFO("cpphy_mdio_nway_wait_state, Phy %u, Status %#x, NegMode %#x, NWAYRE %#x\n",
                mdio->phy_num, PhyStatus, NegMode, NWAYREadvertise);

        if(!NegMode) {
            /* or 10 ?? who knows, Phy is not MII compliant*/
            NegMode = mdio->nway_mode & (NWAY_HD100|NWAY_HD10);
            DEB_WARN("cpphy_mdio_nway_wait_state, Negotiation w/o agreement, default is HD\n");
        }
        /* select 'highest' mode */
        for(j=0x8000, i=0; (i<16) && !(j&NegMode); i++, j>>=1);
        NegMode=j;

        if(NegMode == NWAY_FD100) {
            DEB_INFO("cpphy_mdio_nway_wait_state, FullDuplex 100 Mbps\n");
        } else if(NegMode == NWAY_HD100) {
            mdio->half_duplex = 1;
            DEB_INFO("cpphy_mdio_nway_wait_state, HalfDuplex 100 Mbps\n");
        } else if(NegMode == NWAY_FD10) {
            mdio->slow_speed = 1;
            DEB_INFO("cpphy_mdio_nway_wait_state, FullDuplex 10 Mbps\n");
        } else if(NegMode == NWAY_HD10) {
            mdio->slow_speed = 1;
            mdio->half_duplex = 1;
            DEB_INFO("cpphy_mdio_nway_wait_state, HalfDuplex 10 Mbps\n");
        } else {
            /* should never get here */
            DEB_ERR("cpphy_mdio_nway_wait_state, ill default nway mode\n");
        }
        if(PhyStatus & PHY_LINKED) {
            mdio->state = CPPHY_MDIO_ST_LINKED;
            mdio->linked = 1;
            cpphy_mdio_set_phy_led(mdio);
            cpphy_mgmt_event_dataupdate(mdio); /* Status changed to LINKED */
        } else {
            mdio->state = CPPHY_MDIO_ST_LINK_WAIT;
            /* to check: tmo missing? */
        }
    } else {
        cpphy_mdio_check_pdown_state(mdio);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_link_wait_state(cpphy_mdio_t *mdio) {
    unsigned int PhyStatus;

    PhyStatus = cpphy_mdio_user_access_read(mdio, PHY_STATUS_REG, mdio->phy_num);
    if(PhyStatus & PHY_LINKED) {
        mdio->state = CPPHY_MDIO_ST_LINKED;
        mdio->linked = 1;
        cpphy_mdio_set_phy_led(mdio);
        cpphy_mgmt_event_dataupdate(mdio); /* Status changed to LINKED */
#       if defined(CONFIG_AVM_POWERMETER)
        PowerManagmentRessourceInfo(powerdevice_ethernet, 100);
#       endif /*--- #if defined(CONFIG_AVM_POWERMETER) ---*/
    } else {
        cpphy_mdio_check_pdown_state(mdio);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_linked_state(cpphy_mdio_t *mdio) {
    if(!(MDIO_LINK & (1 << mdio->phy_num))) {
        mdio->linked = 0;
        /* No switch, not linked */
        if(mdio->nway_mode & NWAY_AUTO) {
            mdio->state = CPPHY_MDIO_ST_NWAY_WAIT;
            mdio->timeout = CPPHY_NWDN_TO;
        } else {
            mdio->state = CPPHY_MDIO_ST_LINK_WAIT;
            mdio->timeout = CPPHY_LINK_TO;
        }
#       if defined(CONFIG_AVM_POWERMETER)
        PowerManagmentRessourceInfo(powerdevice_ethernet, 0);
#       endif /*--- #if defined(CONFIG_AVM_POWERMETER) ---*/
        cpphy_mdio_set_phy_led(mdio);
        cpphy_mgmt_event_dataupdate(mdio); /* State changed from LINKED */
        cpphy_mdio_mdix_delay(mdio);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_default_state(cpphy_mdio_t *mdio __attribute__ ((unused))) {
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mdio_phy_tick(cpphy_mdio_t *mdio) {
    unsigned int last_state = mdio->state;
#   if defined(CPU_PROFILE_LOG)
    ohio_simple_profiling_text("cpphy_mdio_phy_tick");
#   endif

    /*--- DEB_TEST("[%s] [%d]: state %u, %s\n", __FUNCTION__, mdio->inst, mdio->state, ---*/ 
             /*--- (mdio->state == CPPHY_MDIO_ST_LINKED) ? "Linked" : "Not Linked"); ---*/
    switch(mdio->state) {
        case CPPHY_MDIO_ST_FINDING:
            cpphy_mdio_finding_state(mdio);
            break;
        case CPPHY_MDIO_ST_FOUND:
            cpphy_mdio_found_state(mdio);
            break;
        case CPPHY_MDIO_ST_PDOWN:
            cpphy_mdio_pdown_state(mdio);
            break;
        case CPPHY_MDIO_ST_NWAY_START:
            cpphy_mdio_nway_start_state(mdio);
            break;
        case CPPHY_MDIO_ST_NWAY_WAIT:
            cpphy_mdio_nway_wait_state(mdio);
            break;
        case CPPHY_MDIO_ST_LINK_WAIT:
            cpphy_mdio_link_wait_state(mdio);
            break;
        case CPPHY_MDIO_ST_LINKED:
            cpphy_mdio_linked_state(mdio);
            break;
        default:
            cpphy_mdio_default_state(mdio);
            break;
    }

    if(last_state != mdio->state) {
#       if defined(CONFIG_MIPS_UR8)
        DEB_INFO("[%s][%d]: state %u -> %u, MACCONTROL 0x%08X, %s\n", __FUNCTION__,
                 mdio->inst, last_state, mdio->state, mdio->cpmac_priv->CPGMAC_F->MAC_CONTROL.Reg,
                 (mdio->state == CPPHY_MDIO_ST_LINKED) ? "Linked" : "Not Linked");
#       elif defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) /*--- #if defined(CONFIG_MIPS_UR8) ---*/
        struct net_device *p_dev = mdio->cpmac_priv->owner;
        /* Phy State Change Detected */
        if((mdio->state == CPPHY_MDIO_ST_LINKED) && !mdio->half_duplex) {
            /* to check: ok to assume always full duplex? */
            CPMAC_MACCONTROL(p_dev->base_addr) |= FULLDUPLEX;
        }
        DEB_INFO("[%s][%d]: state %u -> %u, MACCONTROL 0x%08X, %s\n", __FUNCTION__,
                 mdio->inst, last_state, mdio->state, CPMAC_MACCONTROL(p_dev->base_addr),
                 (mdio->state == CPPHY_MDIO_ST_LINKED) ? "Linked" : "Not Linked");
#       endif /*--- #elif defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/ /*--- #if defined(CONFIG_MIPS_UR8) ---*/

        /* If the state changed, we should check at once for another change */
        if(cpmac_global.power.timeout == CPMAC_TICK_LENGTH) {
            cpmac_global.power.timeout = 0;
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mdio_phy_check_link(cpphy_mdio_t *mdio) {
    if(!(MDIO_LINK & (1 << mdio->phy_num))) {
        /* No switch, not linked */
        if(mdio->nway_mode & NWAY_AUTO) {
            mdio->state = CPPHY_MDIO_ST_NWAY_WAIT;
            mdio->timeout = CPPHY_NWDN_TO;
        } else {
            mdio->state = CPPHY_MDIO_ST_LINK_WAIT;
            mdio->timeout = CPPHY_LINK_TO;
        }
#       if defined(CONFIG_AVM_POWERMETER)
        PowerManagmentRessourceInfo(powerdevice_ethernet, 0);
#       endif /*--- #if defined(CONFIG_AVM_POWERMETER) ---*/
        cpphy_mdio_set_phy_led(mdio);
        cpphy_mgmt_event_dataupdate(mdio); /* State changed from LINKED */
        cpphy_mdio_mdix_delay(mdio);
    }
    cpphy_mdio_phy_tick(mdio);
}


#endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned long cpphy_mdio_tick(cpphy_mdio_t *mdio) {
#   if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
    unsigned int old_linked = mdio->linked;
#   endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/

#   if defined(CPU_PROFILE_LOG)
    ohio_simple_profiling_text("cpphy_mdio_tick");
#   endif
    /* Update link information for switch products, if necessary */
    cpmac_global.power.timeout = CPMAC_TICK_LENGTH;
    cpphy_mgmt_check_link(mdio);
    if(cpmac_global.phys > 1) {
        cpphy_mgmt_check_link(&cpmac_global.cpphy[1].mdio); /* FIXME */
    }
    /* TODO Link management for FUSIV should turn off send queue, if needed */
#   if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
    if(mdio->linked != old_linked) {
        cpmac_main_handle_mdio_status_ind(mdio->cpmac_priv, mdio->linked);
    }
    tasklet_schedule(&mdio->cpmac_priv->tasklet);
#   endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/
    return cpmac_global.power.timeout;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mdio_init_phy(cpphy_mdio_t *mdio __attribute__ ((unused))) {
#   if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
    /* If disabling the power_down for PHYs is needed */
    mdio->power_down = 1;
    mdio->state = CPPHY_MDIO_ST_FINDING;
    mdio->timeout = CPPHY_FIND_TO;
    if(!mdio->high_phy) {
        /* only internal (low) phy can be handled with GPIO_BIT_MII_MDIX! */
        mdio->nway_mode |= NWAY_AUTOMDIX;
    }
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    avm_gpio_ctrl(GPIO_BIT_MII_MDIX, GPIO_PIN, GPIO_OUTPUT_PIN);
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    /* start with assumption: connected to pc with normal cable */
    cpphy_mdio_mdix_update(mdio->mdix);
    avm_gpio_out_bit(30, 0);   /* to check */
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    cpphy_mdio_WriteEphyTestRegs(mdio, 31, 0x16, TNETD73XX_EPHY_TRIM_VALUE);
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
#   endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/
    return;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mdio_init_11G(cpphy_mdio_t *mdio) {
    unsigned short int mdio_address = mdio->phy_num; /* TODO Correct this! */
    unsigned char tries = 0;

    DEB_INFOTRC("[%s] PHY %u, mdio address %u\n", __FUNCTION__, mdio->phy_num, mdio->phy_num);
    cpmac_global.power.port_on[mdio->inst] = 1; /* TODO */

    while(1) {
        if(0xffff != mdio_read(mdio, mdio_address, MII_REG_PHY_ID1)) {
            break;
        }
        tries++;
        if(tries >= 3) {
            panic("[%s] Could not revive PHY after %u tries!\n", __FUNCTION__, tries);
        }
        DEB_WARN("[%s] Resetting PHY %u again\n", __FUNCTION__, mdio->phy_num);
        cpphy_mgmt_device_power_up(mdio->reset_bit, 0);
        ssleep(1);
        cpphy_mgmt_device_power_up(mdio->reset_bit, 1);
        ssleep(1);
    }

    mdio_write(mdio, mdio_address, MII_REG_CONTROL, MII_REG_CONTROL_RESET | MII_REG_CONTROL_AUTONEG_ENABLE);
    msleep(500);
    mdio_write(mdio, mdio_address, MII_REG_AUTONEG_ADV, 0x07e1);
    /* Declare PHY as part of a multi port device. Set MSPT according to Lantiq hint */
    /* (concerning link problems with GBit switches */
    /* Set MSPT according to Lantiq hint (concerning link problems with GBit switches */
    /* and activate GBit FD and HD for now */
    mdio_write(mdio, mdio_address, MII_REG_1000BASET_CTRL,   MII_REG_1000BASET_CTRL_ADV_HD
                                                           | MII_REG_1000BASET_CTRL_ADV_FD
                                                           | MII_REG_1000BASET_CTRL_MSPT);
    /* Enable auto downshift from GBit/s to 100 MBit/s after three autoneg failures */
    mdio_write(mdio, mdio_address,0x14, 0x8006);
    /* Fix against bad links */
    mdio_write(mdio, mdio_address,0x1d, 0x100);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mdio_init_vitesse(cpphy_mdio_t *mdio) {
    unsigned short port = 1 - mdio->inst;
    DEB_INFOTRC("[%s] PHY %u\n", __FUNCTION__, port);
    cpmac_global.power.port_on[port] = 1;

#define GIG_PORT0_BASE   0xb9110000
#define GIG_PORT1_BASE   0xb9150000
#define PORT_CFG_OFF     0x880

    /*--- nicht schön aber selten fix 10MBit Problem ---*/
    if (port) {
        *(volatile unsigned int *)(GIG_PORT1_BASE + PORT_CFG_OFF) &= ~(0x7<<9);
        *(volatile unsigned int *)(GIG_PORT1_BASE + PORT_CFG_OFF) |=  (0x4<<9);
    } else {
        *(volatile unsigned int *)(GIG_PORT0_BASE + PORT_CFG_OFF) &= ~(0x7<<9);
        *(volatile unsigned int *)(GIG_PORT0_BASE + PORT_CFG_OFF) |=  (0x4<<9);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mdio_init_adm7001(cpphy_mdio_t *mdio) {
    unsigned short port = mdio->inst;
    DEB_INFOTRC("[%s] PHY %u\n", __FUNCTION__, port);
    cpmac_global.power.port_on[port] = 1;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mdio_check_link_11G(cpphy_mdio_t *mdio) {
    unsigned short link = 0;
    unsigned char port = mdio->phy_num; /* TODO */
#   if defined(CONFIG_AR9) || defined(CONFIG_ARCH_PUMA5)
    unsigned char speed = 0, fullduplex = 0;
    unsigned int status = 0;
#   endif /*--- #if defined(CONFIG_AR9) || defined(CONFIG_ARCH_PUMA5) ---*/

    DEB_TRC("[%s] Check link port %u (PHY number %u)\n", __FUNCTION__, port, mdio->phy_num);

    if(!cpmac_global.power.port_on[port]) {
        /* Without power, the link information would be wrong */
        link = 0;
        DEB_TRC("[%s] Port %u is powered down\n", __FUNCTION__, port);
    } else {
        link = (mdio_read(mdio, port, 0x01) >> 2) & 0x1;
#       if defined(CONFIG_AR9) || defined(CONFIG_ARCH_PUMA5)
        if(link == 1) {
            /* Check real time link information */
            status = mdio_read(mdio, port, 0x18);
            speed = status & 0x3;
            fullduplex = (status & 0x8) ? 1 : 0;
#           if defined(CONFIG_AR9)
            cpmac_ar9_set_port_speed(port, speed, fullduplex);
#           elif defined(CONFIG_ARCH_PUMA5) /*--- #if defined(CONFIG_AR9) ---*/
            cpmac_puma_set_port_speed(port, speed, fullduplex);
#           endif /*--- #elif defined(CONFIG_ARCH_PUMA5) ---*/
        }
        DEB_DEBUG("[%s] Port %u is %s linked %s (0x1c = %#x)\n", 
                  __FUNCTION__, port, link ? "" : "not", 
                  fullduplex ? "FD" : "HD", status);
#       endif /*--- #if defined(CONFIG_AR9) || defined(CONFIG_ARCH_PUMA5) ---*/
    }
    mdio->linked =   (mdio->linked & ~(1 << port))
                   | (link << port);
    if(cpmac_global.event_data.port[port].link != link) {
        mdio->event_update_needed++;
    }
    cpmac_global.event_data.port[port].link = link;

    return mdio->linked;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mdio_check_link_vitesse(cpphy_mdio_t *mdio) {
    unsigned short link = 0, port = 1 - mdio->inst;
    unsigned char fullduplex = 0;

    DEB_DEBUG("[%s] Check link port %u (PHY nummer %u)\n", __FUNCTION__, port, mdio->phy_num);

    if(!cpmac_global.power.port_on[port]) {
        /* Without power, the link information would be wrong */
        link = 0;
        DEB_TRC("[%s] Port %u is powered down\n", __FUNCTION__, port);
    } else {
        unsigned int status = 0;
        unsigned char speed = 0;

        /* Check real time link information */
        status = mdio_read(&cpmac_global.cpphy[0].mdio, mdio->phy_num, 0x1);
        link = (status & 0x4) ? 1 : 0;
        DEB_TEST("[%s] port %u link status %#x\n", __FUNCTION__, port, status);

        if(link) {
            status = mdio_read(&cpmac_global.cpphy[0].mdio, mdio->phy_num, 0x1c);
            speed = (status >> 3) & 0x3;
            switch(speed) {
                case 0:
                    speed = avm_event_ethernet_speed_10M;
                    break;
                case 1:
                    speed = avm_event_ethernet_speed_100M;
                    break;
                case 2:
                    speed = avm_event_ethernet_speed_1G;
                    break;
                default:
                    DEB_ERR("[%s] Illegal speed value %u from port %u!\n", __FUNCTION__, speed, port);
                    return mdio->linked;
            }
            fullduplex = (status >> 5) & 0x1;
            cpmac_global.event_data.port[port].speed = speed;
            cpmac_global.event_data.port[port].fullduplex = fullduplex;
#           if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)
            cpmac_fusiv_set_port_speed(1 - port, speed, fullduplex);
#           endif /*--- #if defined(CONFIG_FUSIV_VX180)  || defined(CONFIG_FUSIV_VX185)---*/
        }
        /*--- DEB_TRC("[%s] Port %u is %s linked %s (0x1c = %#x)\n", ---*/ 
                /*--- __FUNCTION__, port, link ? "" : "not", ---*/ 
                /*--- fullduplex ? "FD" : "HD", status); ---*/
    }
    /*--- newlink =   (mdio->linked & ~(1 << mdio->switch_config.external_port_offset)) ---*/ 
              /*--- | (link << mdio->switch_config.external_port_offset); ---*/
    mdio->linked =   (mdio->linked & ~(1 << port))
                   | (link << port);
    if(cpmac_global.event_data.port[port].link != link) {
        /* Link changed. Update link status */
        /*--- status = mdio_read(mdio, port, 0x1c); ---*/
        /*--- speed = (status >> 3) & 0x3; ---*/
        /*--- cpmac_global.event_data.port[port].speed100 = (((status & ATHR_PHY_STATUS_SPEED_MASK) >> ATHR_PHY_STATUS_SPEED_SHIFT) == 1) ? 1 : 0; ---*/
        /*--- cpmac_global.event_data.port[port].fullduplex = (status & ATHR_PHY_STATUS_DUPLEX) ? 1 : 0; ---*/
        mdio->event_update_needed++;
    }
    cpmac_global.event_data.port[port].link = link;

    return mdio->linked;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mdio_check_link_adm7001(cpphy_mdio_t *mdio) {
    unsigned short port = mdio->phy_num;
    unsigned char fullduplex = 0, speed = 0, link = 0;

    DEB_DEBUG("[%s] Check link port %u (PHY nummer %u)\n", __FUNCTION__, port, mdio->phy_num);

    if(!cpmac_global.power.port_on[port]) {
        /* Without power, the link information would be wrong */
        mdio->state = CPPHY_MDIO_ST_LINK_WAIT;
        link = 0;
        DEB_TRC("[%s] Port %u is powered down\n", __FUNCTION__, port);
    } else {
        unsigned int status = 0;

        /* Check real time link information */
        status = mdio_read(&cpmac_global.cpphy[0].mdio, port, 0x17);
        link = (status & (1 << 4)) ? 1 : 0;
        DEB_TEST("[%s] port %u link status %#x\n", __FUNCTION__, port, status);

        if(link) {
            mdio->state = CPPHY_MDIO_ST_LINKED;
            if(status & (1 << 5)) {
                speed = avm_event_ethernet_speed_100M;
            } else {
                speed = avm_event_ethernet_speed_10M;
            }
            fullduplex = (status >> 6) & 0x1;
        } else {
            mdio->state = CPPHY_MDIO_ST_LINK_WAIT;
        }
        /*--- DEB_TRC("[%s] Port %u is %s linked %s (0x1c = %#x)\n", ---*/ 
                /*--- __FUNCTION__, port, link ? "" : "not", ---*/ 
                /*--- fullduplex ? "FD" : "HD", status); ---*/
    }
    mdio->linked =   (mdio->linked & ~(1 << port))
                   | (link << port);
    if(   (cpmac_global.event_data.port[port].link == link)
       && (cpmac_global.event_data.port[port].speed == speed)
       && (cpmac_global.event_data.port[port].fullduplex == fullduplex)
       && (speed != avm_event_ethernet_speed_error)) {
        /* No link details changed */
        DEB_DEBUG("[%s] Got port %u update without changes\n", __FUNCTION__, port);
    } else {
        cpmac_global.event_data.port[port].link = link;
        cpmac_global.event_data.port[port].speed = speed;
        cpmac_global.event_data.port[port].fullduplex = fullduplex;
        mdio->event_update_needed++;
    }

    return mdio->linked;
}


/*----------------------------------------------------------------------------------*\
  Start MDIO Negotiation
\*----------------------------------------------------------------------------------*/
void cpphy_mdio_init(cpphy_global_t *cpphy_global) {
    unsigned int port;

    cpphy_mdio_t *mdio = &cpphy_global->mdio;
    cpmac_priv_t *cpmac_priv = cpphy_global->cpmac_priv;

    DEB_TRC("[%s]\n", __func__);

    mdio->linked = 0;
    mdio->cpmac_priv = cpmac_priv;
    for(port = 0; port < CPMAC_MAX_LED_HANDLES; port++) {
        mdio->cpmac_priv->led[port].handle = 0;
        mdio->cpmac_priv->led[port].on = 0;
    }

#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    /*  MII not setup, Setup initial condition  */
    if(cpmac_devices_installed == 1) {
        /* reset only during the first cpphy driver registration */
        /*--- avm_reset_device(MDIO_RESET_BIT, 10); ---*/
        /*--- msleep(2); ---*/ /* wait after device out of reset */
        avm_put_device_into_reset(MDIO_RESET_BIT);
    }

    /*--- MDIO_CR = MDIO_CR_ENABLE | MDIO_CR_CLKDIV((avm_get_clock(avm_clock_id_vbus) / CPPHY_CLK_FREQ) - 1); ---*/
    /*--- DEB_INFO("cpphy_mdio_init, vbus clk: %u\n", avm_get_clock(avm_clock_id_vbus)); ---*/
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/

#ifdef undef
    /* int pacing: chance to reduce (high) number of ints -> better caching + less redundancy */
    /* trick: with reduced clock in prescale register even smaller trigger values */
    /* than 2 Int/ms possible */
    /* currently no better performance */
    ohioint_ctrl_irq_pacing(OHIOINT_CPMAC0, 0, avm_get_clock(avm_clock_id_vbus) / (1000000 * 4 / 2), 2);
#endif

#   if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
    /* design: must contain NWAY_HD100 or NWAY_HD10 */
    mdio->nway_mode = NWAY_AUTO|NWAY_HD10|NWAY_HD100|NWAY_FD10|NWAY_FD100;
#   endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/

    mdio->half_duplex = 0;
    mdio->slow_speed = 0;

    if(mdio->inst == 1) {
        /* to check */
        mdio->phy_num = 1;
    } else {
        /*--- mdio->phy_num = 31; ---*/
        mdio->phy_num = 0;
    }

    init_MUTEX(&mdio->semaphore);
    skb_queue_head_init(&mdio->ar_waiting_list);

    cpphy_mgmt_global_power_set(CPPHY_POWER_GLOBAL_SET_OFF);
    mdio->cpmac_irq = 0;

    if(mdio->inst == 0) {
        cpphy_mgmt_init(mdio);
        cpphy_mgmt_power_init(mdio);
        cpphy_mgmt_work_start(mdio);

        cpphy_mgmt_work_add(mdio, CPMAC_WORK_TICK, cpphy_mdio_tick, 0);
    }
}

