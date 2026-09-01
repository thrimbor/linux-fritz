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

#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <asm/atomic.h>
#include <asm/mach_avm.h>
#include <asm/io.h>
#if defined(CONFIG_ARCH_PUMA5)
#include <mach/hw_gpio.h>
#endif /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/

#if defined(CONFIG_AVM_LED_EVENTS) || defined(CONFIG_AVM_LED_EVENTS_MODULE)
#include <linux/avm_led_event.h>
#endif /*--- #if defined(CONFIG_AVM_LED_EVENTS) || defined(CONFIG_AVM_LED_EVENTS_MODULE) ---*/
#if defined(CONFIG_AVM_LED)
#include <linux/avm_led.h>
#endif /*--- #if defined(CONFIG_AVM_LED) ---*/

#include <linux/jiffies.h>
#if defined(CONFIG_AVM_POWER)
#include <linux/avm_power.h>
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/

#if !defined(CONFIG_NETCHIP_ADM69961)
#define CONFIG_NETCHIP_ADM69961
#endif

#include <linux/avm_event.h>
#include <linux/avm_cpmac.h>
#include <linux/adm_reg.h>
#include "cpmac_if.h"
#include "cpmac_const.h"
#include "cpmac_debug.h"
#include "cpphy_const.h"
#include "cpphy_types.h"
#include "cpphy_mdio.h"
#include "adm6996.h"
#include "tantos.h"
#include "cpphy_adm6996.h"
#include "cpphy_ar8216.h"
#include "cpphy_mgmt.h"
#include <linux/ar_reg.h>
#if defined(CONFIG_MIPS_UR8)
#include <asm/mach-ur8/hw_nwss.h>
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/
#include "cpphy_if.h"
#include "cpphy_if_g.h"
#include "cpmac_eth.h"
#include "cpmac_puma_if.h"

#if CPMAC_DEBUG_LEVEL & (CPMAC_DEBUG_LEVEL_ERROR | CPMAC_DEBUG_LEVEL_WARNING | CPMAC_DEBUG_LEVEL_INFOTRACE)
static const char *cpmac_work_names[] = {
                                         "CPMAC_WORK_TICK",
                                         "CPMAC_WORK_UPDATE_MAC_TABLE", 
                                         "CPMAC_WORK_SWITCH_DUMP",
                                         "CPMAC_WORK_TOGGLE_PPPOA",
                                         "CPMAC_WORK_TOGGLE_VLAN",
                                         "CPMAC_WORK_PORT_MIRROR",
                                         "CPMAC_WORK_DEBUG"
};
#endif /*--- #if CPMAC_DEBUG_LEVEL & (CPMAC_DEBUG_LEVEL_ERROR | CPMAC_DEBUG_LEVEL_WARNING | CPMAC_DEBUG_LEVEL_INFOTRACE) ---*/

static cpphy_mdio_t *power_mdio; /* FIXME Is there a cleaner solution? */

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_power_init(cpphy_mdio_t *mdio) {
    unsigned int port;

    DEB_TRC("[%s]\n", __FUNCTION__);
    power_mdio = mdio;
    /* Prepare a default timeout */
    cpmac_global.power.timeout = CPMAC_TICK_LENGTH;
    if(!(mdio->Mode & CPPHY_SWITCH_MODE_READ)) {
        /* The switch is not accessible (e.g. ProfiVoIP CPU 2) */
        return;
    }

    /* Initialize the power management for the PHYs */
    cpmac_global.power.setup.time_on  = CPMAC_ADM_PHY_PUP_TO;
    cpmac_global.power.setup.time_off = CPMAC_ADM_PHY_PDN_TO;
    assert(cpmac_global.ports <= AVM_CPMAC_MAX_PORTS);
    if(mdio->switch_config.use_EMV) {
        for(port = 0; port < cpmac_global.ports; port++) {
            if(port < 4) {
                cpmac_global.power.setup.mode[port] = ADM_PHY_POWER_ON;
            } else {
                cpmac_global.power.setup.mode[port] = ADM_PHY_POWER_OFF;
            }
            cpmac_global.power.port_mode[port] = CPPHY_POWER_MODE_LINK_LOST;
        }
        cpmac_global.power.linked_ports = 0xf;
    } else {
        for(port = 0; port < cpmac_global.ports; port++) {
            cpmac_global.power.port_on[port] = 0;
            if(cpmac_global.power.port_modes_possible[port] & (1 << ADM_PHY_POWER_OFF)) {
                DEB_TRC("[%s] Setting port %u to off\n", __FUNCTION__, port);
                cpmac_global.power.setup.mode[port] = ADM_PHY_POWER_OFF;
            } else if(cpmac_global.power.port_modes_possible[port] & (1 << ADM_PHY_POWER_ON)) {
                DEB_TRC("[%s] Setting port %u to on\n", __FUNCTION__, port);
                cpmac_global.power.setup.mode[port] = ADM_PHY_POWER_ON;
            } else {
                DEB_TRC("[%s] Not setting port %u to anything\n", __FUNCTION__, port);
            }
        }
    }
    adm_switch_port_power_config(mdio, &cpmac_global.power.setup, 1);
    /*--- cpphy_mgmt_power(mdio); ---*/
    /* There is no handle from the power management, therefor use it like this. */
#   if defined(CONFIG_AVM_POWER)
    mdio->power_handle = PowerManagmentRegister("ethernet", adm_power_config);
#   endif /*--- #if defined(CONFIG_AVM_POWER) ---*/
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_mgmt_set_led_state(cpphy_mdio_t *mdio, unsigned char port, unsigned char led_on) {
    if(port < CPMAC_MAX_LED_HANDLES) {
        if(led_on != mdio->cpmac_priv->led[port].on) {
            mdio->cpmac_priv->led[port].on = led_on;
            DEB_INFOTRC("Set LAN LED %u to '%s'\n",
                    port,
                    led_on ? "on" : "off");
#           if defined(CONFIG_AVM_LED)
            if(mdio->cpmac_priv->led[port].handle != 0) {
                avm_led_action_with_handle(mdio->cpmac_priv->led[port].handle,
                        led_on ? avm_led_on : avm_led_off);
            }
#           endif /*--- #if defined(CONFIG_AVM_LED) ---*/
#           if defined(CONFIG_AVM_LED_EVENTS) || defined(CONFIG_AVM_LED_EVENTS_MODULE)
            if(led_event_action) {
                (*led_event_action)(LEDCTL_VERSION, event_lan1_active + (port << 1) + (led_on ? 0 : 1), 1);
            }
#           endif /*--- #if defined(CONFIG_AVM_LED_EVENTS) || defined(CONFIG_AVM_LED_EVENTS_MODULE) ---*/
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_device_power_up(unsigned int reset_bit, unsigned char power_on) {
    if(reset_bit != 0xffff) {
#       if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)
        ikan_gpio_out_bit(reset_bit, power_on ? 1 : 0);
#       elif defined(CONFIG_ARCH_PUMA5) /*--- #if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) ---*/
        avm_gpio_ctrl(reset_bit, GPIO_PIN, GPIO_OUTPUT_PIN);
        avm_gpio_out_bit(reset_bit, power_on ? 1 : 0);
#       else /*--- #elif defined(CONFIG_ARCH_PUMA5) ---*/
        if(power_on) {
            avm_take_device_out_of_reset(reset_bit);
        } else {
            avm_put_device_into_reset(reset_bit);
        }
#       endif /*--- #else ---*/ /*--- #elif defined(CONFIG_ARCH_PUMA5) ---*/
    }
}


/*------------------------------------------------------------------------------------------*\
 * Globally switch the PHYs or the switch on or off                                         *
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_global_power(cpphy_mdio_t *mdio __attribute__ ((unused)) /* FIXME */, unsigned char power_on) {
    unsigned char port, instance;

    for(instance = 0; instance < cpmac_global.phys; instance++) {
        mdio = &cpmac_global.cpphy[instance].mdio;

        if(power_on && cpmac_global.cpphy[instance].is_out_of_reset) {
            continue;
        }

        DEB_INFOTRC("[%s] Instance %u: Changing PHY/switch power to %s\n", 
                    __FUNCTION__, instance, power_on ? "on" : "off");

        cpphy_mgmt_device_power_up(mdio->reset_bit, power_on);
        cpmac_global.cpphy[instance].is_out_of_reset = power_on;

#       if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
        MDIO_CR = MDIO_CR_ENABLE | MDIO_CR_CLKDIV((avm_get_clock(avm_clock_id_vbus) / CPPHY_CLK_FREQ) - 1);
        DEB_INFO("[%s], vbus clk: %u\n", __FUNCTION__, avm_get_clock(avm_clock_id_vbus));
#       endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    }

    ssleep(1);

    for(instance = 0; instance < AVM_CPMAC_MAX_PHYS; instance++) {
        mdio = &cpmac_global.cpphy[instance].mdio;

        if(power_on) {
            if(cpmac_global.cpphy[instance].is_open == 0) {
                continue;
            }

            /* Initialize switch */
            if(!cpmac_global.cpphy[instance].init_ran && mdio->f->init_phy) {
                mdio->f->init_phy(mdio);
            }
        } else {
            /* Initialize switch */
            if(cpmac_global.cpphy[instance].init_ran && mdio->f->deinit_phy) {
                mdio->f->deinit_phy(mdio);
            }
        }
        cpmac_global.cpphy[instance].init_ran = power_on;

        for(port = 0; port < mdio->switch_config.ports; port++) {
            cpmac_global.power.port_on[port] = power_on;
            if(   (power_on == 1) 
               && (cpmac_global.power.setup.mode[port] == ADM_PHY_POWER_OFF)
               && mdio->f->switch_port_power) {
                DEB_TRC("[%s] deactivate port %u\n", __FUNCTION__, port);
                mdio->f->switch_port_power(mdio, port, 0);
                cpmac_global.power.port_on[port] = 0;
            }
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_global_power_set(cpmac_power_mode_t mode) {
    DEB_TRC("[%s] Setting global power to %s\n",
            __FUNCTION__,
            (mode == CPPHY_POWER_GLOBAL_NONE)    ? "none" :
            (mode == CPPHY_POWER_GLOBAL_SET_OFF) ? "set off" :
            (mode == CPPHY_POWER_GLOBAL_OFF)     ? "off" :
            (mode == CPPHY_POWER_GLOBAL_ON)      ? "on" : "unknown");
    cpmac_global.global_power = mode;
}


/*------------------------------------------------------------------------------------------*\
 * Return: 0 - continue power management routine
 *         1 - abort further handling
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mgmt_process_global_power(cpphy_mdio_t *mdio) {
    unsigned char port, from = 0, to = mdio->switch_config.ports - 1;

    cpmac_global.power.global_portset = 0;
    switch(cpmac_global.global_power) {
        case CPPHY_POWER_GLOBAL_ON:
            DEB_INFOTRC("[%s] Global powerup active\n", __FUNCTION__);
            cpphy_mgmt_global_power_set(CPPHY_POWER_GLOBAL_NONE);
            cpphy_mgmt_global_power(mdio, 1);
            cpmac_global.power.current_on = 1;
            return 1;
        case CPPHY_POWER_GLOBAL_SET_OFF:
            DEB_INFOTRC("[%s] Global powerdown activating\n", __FUNCTION__);
            cpphy_mgmt_global_power_set(CPPHY_POWER_GLOBAL_OFF);
            cpphy_mgmt_global_power(mdio, 0);
            for(port = from; port <= to; port++) {
                cpphy_mgmt_set_led_state(mdio, port, 0);
            }
            cpmac_global.power.current_on = 0;
            return 1;
        case CPPHY_POWER_GLOBAL_OFF:
            DEB_INFOTRC("[%s] Global powerdown active\n", __FUNCTION__);
            cpmac_global.power.current_on = 0;
            return 1;
        default:
            DEB_ERR("[%s] Internal error: Global power with unknown value %u\n",
                    __FUNCTION__, cpmac_global.global_power);
            break;
    }
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mgmt_check_link_phy(cpphy_mdio_t *mdio) {
    if(cpmac_global.global_power) {
        cpphy_mgmt_process_global_power(mdio);
    }
    return mdio->linked;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mgmt_check_link_dummy(cpphy_mdio_t *mdio __attribute__ ((unused))) {
    return 0x3f;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mgmt_check_link_orig(cpphy_mdio_t *mdio) {
    unsigned char device, port;
#   if defined(CONFIG_AVM_POWERMETER)
    unsigned int portset = 0, powervalue = 100;
#   endif /*--- #if defined(CONFIG_AVM_POWERMETER) ---*/

    assert(mdio->f->check_link != NULL);
    mdio->f->check_link(mdio);

    mdio->linked |= mdio->switch_config.mii_portmask;

    if(mdio->event_update_needed) {
        cpphy_mgmt_event_dataupdate(mdio); /* Linked ports could have changed */
    }

    for(port = 0; port < TANTOS_MAX_PORTS ; port++) {
        unsigned char port_linked = (mdio->linked & (1 << (port + mdio->switch_config.external_port_offset))) ? 1 : 0;
        cpphy_mgmt_set_led_state(mdio, port, port_linked);
#       if defined(CONFIG_AVM_POWERMETER)
        if((port < mdio->switch_config.ports) && (cpmac_global.power.port_on)) {
            powervalue += 10; /* Port is powered */
            if(port_linked) {
                /* Port is linked */
                portset |= (1 << port);
                powervalue += cpmac_global.event_data.port[port].speed100 ? 57 : 90; /* 100 MBit/s? */
            }
        }
#       endif /*--- #if defined(CONFIG_AVM_POWERMETER) ---*/
    }
    cpphy_mgmt_power(mdio);
#   if defined(CONFIG_AVM_POWERMETER)
    PowerManagmentRessourceInfo(powerdevice_ethernet, PM_ETHERNET_PARAM(portset, powervalue));
#   endif /*--- #if defined(CONFIG_AVM_POWERMETER) ---*/

    for(device = 0; device < mdio->switch_config.devices; device++) {
        unsigned int carrier;

        carrier = netif_carrier_ok(mdio->switch_config.device[device].net_device);

        DEB_DEBUG("[cpphy_mgmt_check_link] '%s' carrier %s linked = %#x   %#x = ports\n",
                  mdio->switch_config.device[device].net_device->name, 
                  carrier ? "on " : "off",
                  mdio->linked, mdio->switch_config.device[device].portset);
        if(mdio->linked & mdio->switch_config.device[device].portset) {
            if(!carrier) {
                netif_carrier_on(mdio->switch_config.device[device].net_device);
                DEB_INFOTRC("Switching carrier on for device %s\n", mdio->switch_config.device[device].net_device->name);
            }
        } else if(carrier) {
            netif_carrier_off(mdio->switch_config.device[device].net_device);
            DEB_INFOTRC("Switching carrier off for device %s\n", mdio->switch_config.device[device].net_device->name);
        }
    }

    return mdio->linked;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mgmt_check_link_emv(cpphy_mdio_t *mdio) {
    unsigned int device, port;
#   if defined(CONFIG_AVM_POWERMETER)
    unsigned int portset = 0, powervalue = 100;
#   endif /*--- #if defined(CONFIG_AVM_POWERMETER) ---*/

    assert(mdio->f->check_link != NULL);
    mdio->f->check_link(mdio);

    if(mdio->event_update_needed) {
        cpphy_mgmt_event_dataupdate(mdio); /* Linked ports could have changed */
    }

    for(port = 0; port < TANTOS_MAX_PORTS ; port++) {
        unsigned char port_linked = (mdio->linked & (1 << (port + mdio->switch_config.external_port_offset))) ? 1 : 0;
        if(port < CPMAC_MAX_LED_HANDLES) {
            if(port_linked != mdio->cpmac_priv->led[port].on) {
                mdio->cpmac_priv->led[port].on = port_linked;
                DEB_INFOTRC("Set LAN LED %u to '%s'\n",
                            port,
                            port_linked ? "on" : "off");
#               if defined(CONFIG_AVM_LED)
                if(mdio->cpmac_priv->led[port].handle != 0) {
                    avm_led_action_with_handle(mdio->cpmac_priv->led[port].handle,
                                               port_linked ? avm_led_on : avm_led_off);
                }
#               endif /*--- #if defined(CONFIG_AVM_LED) ---*/
#               if defined(CONFIG_AVM_LED_EVENTS) || defined(CONFIG_AVM_LED_EVENTS_MODULE)
                if(led_event_action) {
                    (*led_event_action)(LEDCTL_VERSION, event_lan1_active + (port << 1) + (port_linked ? 0 : 1), 1);
                }
#               endif /*--- #if defined(CONFIG_AVM_LED_EVENTS) || defined(CONFIG_AVM_LED_EVENTS_MODULE) ---*/
            }
        }
#       if defined(CONFIG_AVM_POWERMETER)
        if((port < mdio->switch_config.ports) && (cpmac_global.power.current_mode == CPPHY_POWER_MODE_ON)) {
            powervalue += 10; /* Port is powered */
            if(port_linked) {
                /* Port is linked */
                portset |= (1 << port);
                powervalue += cpmac_global.event_data.port[port].speed100 ? 57 : 90; /* 100 MBit/s? */
            }
        }
#       endif /*--- #if defined(CONFIG_AVM_POWERMETER) ---*/
    }
    cpphy_mgmt_power(mdio);
#   if defined(CONFIG_AVM_POWERMETER)
    PowerManagmentRessourceInfo(powerdevice_ethernet, PM_ETHERNET_PARAM(portset, powervalue));
#   endif /*--- #if defined(CONFIG_AVM_POWERMETER) ---*/

    for(device = 0; device < mdio->switch_config.devices; device++) {
        unsigned int carrier = netif_carrier_ok(mdio->switch_config.device[device].net_device);

        /*--- DEB_TEST("[cpphy_mgmt_check_link] '%s' carrier %s linked = %#x   %#x = ports\n", ---*/
                 /*--- mdio->switch_config.device[device].net_device->name, ---*/ 
                 /*--- carrier ? "on " : "off", ---*/
                 /*--- mdio->linked, mdio->switch_config.device[device].portset); ---*/
        if(mdio->linked & mdio->switch_config.device[device].portset) {
            if(!carrier) {
                netif_carrier_on(mdio->switch_config.device[device].net_device);
                DEB_INFOTRC("Switching carrier on for device %s\n", mdio->switch_config.device[device].net_device->name);
            }
        } else if(carrier) {
            netif_carrier_off(mdio->switch_config.device[device].net_device);
            DEB_INFOTRC("Switching carrier off for device %s\n", mdio->switch_config.device[device].net_device->name);
        }
    }

    return mdio->linked;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mgmt_check_link(cpphy_mdio_t *mdio) {
    assert(mdio->f->mgmt_check_link != NULL);
    return mdio->f->mgmt_check_link(mdio);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_event_dataupdate(cpphy_mdio_t *mdio __attribute__ ((unused))) {
#   if CPMAC_USE_DEBUG_LEVEL_TRACE
#   define DEBUG_BUFFER_LENGTH 300u
    unsigned int port, written = 0;
    static unsigned char output[DEBUG_BUFFER_LENGTH + 1];

    written += snprintf(output + written, DEBUG_BUFFER_LENGTH - written, "[%s] Port stati: ", __FUNCTION__);
    for(port = 0; port < cpmac_global.ports; port++) {
        written += snprintf(output + written,
                            DEBUG_BUFFER_LENGTH - written,
                            "%u %s%s%s  ", 
                            port,
                            cpmac_global.event_data.port[port].link ? "up" : "down",
                            cpmac_global.event_data.port[port].link 
                            ? ((cpmac_global.event_data.port[port].speed == avm_event_ethernet_speed_1G) ? " 1000" :
                               (cpmac_global.event_data.port[port].speed == avm_event_ethernet_speed_100M) ? " 100" :
                               (cpmac_global.event_data.port[port].speed == avm_event_ethernet_speed_10M) ? " 10" : " ?")
                            : "",
                            cpmac_global.event_data.port[port].link ? (cpmac_global.event_data.port[port].fullduplex ? " FD" : " HD") : "");
    }
    snprintf(output + written, DEBUG_BUFFER_LENGTH - written, "\n");
    DEB_TRC("%s", output);
#   endif /*--- #if CPMAC_USE_DEBUG_LEVEL_TRACE ---*/

    mdio->event_update_needed = 0;

    cpmac_main_event_update();
}


/*------------------------------------------------------------------------------------------*\
 * Switch port power, assume link status is uptodate
\*------------------------------------------------------------------------------------------*/
static void cpphy_mgmt_power_orig(cpphy_mdio_t *mdio) {
    unsigned int port, from = 0, to = mdio->switch_config.ports - 1;

    cpmac_global.power.timeout = CPMAC_TICK_LENGTH;

    /* Switch to different state than last run and use correct timeout*/
    cpmac_global.power.current_on = !cpmac_global.power.current_on;
    /* If global powerdown is needed, disable roundrobin for this run */
    if(cpmac_global.global_power) {
        if(cpphy_mgmt_process_global_power(mdio)) {
            /* Abort processing in power off cases */
            return;
        }
    } else if(cpmac_global.power.roundrobin) {
        /* If we are going to activate a port, it should be the next one */
        if(cpmac_global.power.current_on) {
            cpmac_global.power.current_port = (cpmac_global.power.current_port == 4)
                                              ? 0
                                              : (cpmac_global.power.current_port + 1);
        }
        from = to = cpmac_global.power.current_port;
    }

    /* There is no power saving without write access */
    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE)) {
        return;
    }

    if(cpmac_global.power.powersaving_on_any_port == 0) {
        cpmac_global.power.timeout = CPMAC_TICK_LENGTH;
    } else {
        cpmac_global.power.timeout = (cpmac_global.power.current_on) ? cpmac_global.power.setup.time_on
                                                                     : cpmac_global.power.setup.time_off;
    }

    for(port = from; port <= to; port++) {
        unsigned char turn_on = 1;
        switch(cpmac_global.power.setup.mode[port]) {
            case ADM_PHY_POWER_ON:
                turn_on = 1;
                break;
            case ADM_PHY_POWER_OFF:
                turn_on = 0;
                break;
            case ADM_PHY_POWER_SAVE:
                turn_on = cpmac_global.power.current_on;
                break;
            default:
                DEB_ERR("[%s] Illegal port power state (%#x)!\n", 
                        __FUNCTION__, cpmac_global.power.setup.mode[port]);
                break;
        }
        if((turn_on == 0) && cpmac_global.event_data.port[port].link) { /* with link */
            turn_on = 1;
        }

        if(cpmac_global.power.port_on[port] != turn_on) {
            if(mdio->f->switch_port_power) {
                DEB_DEBUG("[%s] Power %s PHY %u\n", __FUNCTION__, turn_on ? "up" : "down", port);
                mdio->f->switch_port_power(mdio, port, turn_on);
                cpmac_global.power.port_on[port] = turn_on;
            }
        }

#       if defined(POWERMANAGEMENT_THROTTLE_ETH)
        if((turn_on == 1) && power_mdio->f->switch_port_speed_throttle) {
            if(cpmac_global.power.port_throttle[port] != cpmac_global.power.port_throttle_req[port]) {
                cpmac_global.power.port_throttle[port] = cpmac_global.power.port_throttle_req[port];
                if(power_mdio->f->switch_port_speed_throttle != NULL) {
                    power_mdio->f->switch_port_speed_throttle(power_mdio, port, cpmac_global.power.port_throttle[port]);
                }
            }
        }
#       endif /*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/
    }
}

#define NEXT_PORT { cpmac_global.power.current_port = (port >= to) ? from : (port + 1); }
/*------------------------------------------------------------------------------------------*\
 * Switch port power, assume link status is uptodate
\*------------------------------------------------------------------------------------------*/
static void cpphy_mgmt_power_emv(cpphy_mdio_t *mdio) {
    unsigned char port, from = 0, to;

    /* There is no power saving without write access */
    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE)) {
        cpmac_global.power.timeout = CPMAC_TICK_LENGTH;
        return;
    }

    /* Global power modes */
    if(cpmac_global.global_power) {
        DEB_TEST("Global power%s active\n", 
                 (cpmac_global.global_power == CPPHY_POWER_GLOBAL_OFF) ? "off" : "on");
        if(cpmac_global.global_power == CPPHY_POWER_GLOBAL_OFF) {
            /* Driver startup */
            cpmac_global.power.timeout = CPMAC_TICK_LENGTH;
            /*--- ar_switch_port_power_emv(mdio, 0, CPPHY_POWER_MODE_ON_IDLE_MDI); ---*/
            ar_switch_port_power_emv(mdio, 0, CPPHY_POWER_MODE_OFF);
            ar_switch_port_power_emv(mdio, 1, CPPHY_POWER_MODE_OFF);
            ar_switch_port_power_emv(mdio, 2, CPPHY_POWER_MODE_OFF);
            ar_switch_port_power_emv(mdio, 3, CPPHY_POWER_MODE_OFF);
            ar_switch_port_power_emv(mdio, 4, CPPHY_POWER_MODE_ON_IDLE_MDI);
        } else {
            /* Driver activation */
            cpmac_global.power.timeout = CPMAC_ADM_PHY_PUP_TEST_10_TO;
            cpphy_mgmt_global_power_set(CPPHY_POWER_GLOBAL_NONE);
            ar_boot_power_on_emv(mdio);
            ar_switch_port_power_emv(mdio, 0, CPPHY_POWER_MODE_LINK_LOST);
            cpmac_global.power.time_to_wait_to[0] = jiffies;
            ar_switch_port_power_emv(mdio, 1, CPPHY_POWER_MODE_LINK_LOST);
            cpmac_global.power.time_to_wait_to[1] = jiffies + 1 * CPMAC_ADM_PHY_PUP_IDLE_MDI + ((2 * HZ) / 10);
            ar_switch_port_power_emv(mdio, 2, CPPHY_POWER_MODE_LINK_LOST);
            cpmac_global.power.time_to_wait_to[2] = jiffies + 2 * CPMAC_ADM_PHY_PUP_IDLE_MDI + ((4 * HZ) / 10);
            ar_switch_port_power_emv(mdio, 3, CPPHY_POWER_MODE_LINK_LOST);
            cpmac_global.power.time_to_wait_to[3] = jiffies + 3 * CPMAC_ADM_PHY_PUP_IDLE_MDI + ((6 * HZ) / 10);
            cpmac_global.power.current_port = 0;
            mdio->linked = 0xf;
        }
        return;
    }

    /* Normal sequence for all relevant ports */
    from = 0;
    to = 3;
    port = cpmac_global.power.current_port;

    /* Switch to different state than last run and use correct timeout*/
    cpmac_global.power.timeout = CPMAC_ADM_PHY_PUP_TEST_10_TO / 5;
    switch(cpmac_global.power.port_mode[port]) {
        case CPPHY_POWER_MODE_ON:
            if(ar_switch_test_link(mdio, port)) {
                NEXT_PORT;
                break; /* Do not change state as there is a link */
            }
            /* Wait at least CPMAC_ADM_PHY_PUP_LINKLOSS_TO */
            cpmac_global.power.time_to_wait_to[port] = jiffies + CPMAC_ADM_PHY_PUP_LINKLOSS_TO;
            cpmac_global.power.port_mode[port] = CPPHY_POWER_MODE_LINK_LOST;
            NEXT_PORT;
            break;

        case CPPHY_POWER_MODE_LINK_LOST:
            if(time_after(cpmac_global.power.time_to_wait_to[port], jiffies)) {
                NEXT_PORT;
                break;
            }
            mdio->linked &= ~(1 << port);
            /* If there is a link, it will be checked in the ON_MDIX function */
            ar_switch_port_power_emv(mdio, port, CPPHY_POWER_MODE_ON_MDIX);
            cpmac_global.power.time_to_wait_to[port] = jiffies + CPMAC_ADM_PHY_PUP_NORMAL_MDIX;
            DEB_TEST("[cpphy_mgmt_power] ports linked: %#x\n", mdio->linked);
            break;

        case CPPHY_POWER_MODE_ON_MDIX:
            if(time_after(cpmac_global.power.time_to_wait_to[port], jiffies)) {
                NEXT_PORT;
                break;
            }
            /* Do not toggle port 0 if at least one port is linked */
            if((port != 0) || (mdio->linked == 0)) {
                ar_switch_port_power_emv(mdio, port, CPPHY_POWER_MODE_ON_MDI_MLT3);
                cpmac_global.power.time_to_wait_to[port] = jiffies + CPMAC_ADM_PHY_PUP_IDLE_MDI;
            } else {
                if(ar_switch_test_link(mdio, port)) {
                    ar_switch_port_power_emv(mdio, port, CPPHY_POWER_MODE_ON);
                }
                cpmac_global.power.time_to_wait_to[port] = jiffies + CPMAC_ADM_PHY_PUP_NORMAL_MDIX;
            }
            NEXT_PORT;
            break;

        case CPPHY_POWER_MODE_ON_MDI_MLT3:
            if(time_after(cpmac_global.power.time_to_wait_to[port], jiffies)) {
                NEXT_PORT;
                break;
            }
            ar_switch_port_power_emv(mdio, port, CPPHY_POWER_MODE_ON_MDIX);
            cpmac_global.power.time_to_wait_to[port] = jiffies + CPMAC_ADM_PHY_PUP_NORMAL_MDIX;
            NEXT_PORT;
            break;

        default:
            DEB_ERR("[cpphy_mgmt_power] Illegal power mode encountered! (%u)\n", cpmac_global.power.current_mode);
            return;
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_power(cpphy_mdio_t *mdio) {
    if(mdio->switch_config.use_EMV) {
        cpphy_mgmt_power_emv(mdio);
    } else {
        cpphy_mgmt_power_orig(mdio);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_check_powersave_settings(void) {
    unsigned int i;

    cpmac_global.power.powersaving_on_any_port = 0;
    for(i = 0; i < cpmac_global.ports; i++) {
        if(cpmac_global.power.setup.mode[i] == ADM_PHY_POWER_SAVE) {
            cpmac_global.power.powersaving_on_any_port = 1;
        }
    }
    DEB_INFOTRC("[%s] Powersaving on any port: %s\n", 
                __FUNCTION__, cpmac_global.power.powersaving_on_any_port ? "yes" : "no");
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static cpmac_err_t adm_switch_port_power_config_orig(cpphy_mdio_t          *mdio,
                                                     adm_phy_power_setup_t *setup,
                                                     unsigned char          set) {
    unsigned int i;

    if(!set) {
        setup->time_on  = cpmac_global.power.setup.time_on;
        setup->time_off = cpmac_global.power.roundrobin ? (4 * (  cpmac_global.power.setup.time_on
                                                                + cpmac_global.power.setup.time_off))
                                                        : cpmac_global.power.setup.time_off;
        for(i = 0; i < mdio->switch_config.ports; i++) {
            setup->mode[i] = cpmac_global.power.setup.mode[i];
        }
    } else {

        cpmac_global.power.timeout = 0;
        cpmac_global.power.current_on = 1;
        cpmac_global.power.current_port = 0;

        cpmac_global.power.roundrobin = ((setup->time_on * 4) < setup->time_off);
        cpmac_global.power.setup.time_on  = setup->time_on;
        cpmac_global.power.setup.time_off =   cpmac_global.power.roundrobin
                                            ? ((setup->time_off - (4 * setup->time_on)) / 4)
                                            : setup->time_off;
        DEB_INFOTRC("[adm_switch_port_power_config] Power setup: %s with %u on and %u off\n", 
                    cpmac_global.power.roundrobin ? "roundrobin" : "normal",
                    cpmac_global.power.setup.time_on,
                    cpmac_global.power.setup.time_off);
        for(i = 0; i < mdio->switch_config.ports; i++) {
            DEB_INFOTRC("[adm_switch_port_power_config]              Port %u in mode %s\n", 
                        i, (setup->mode[i] == ADM_PHY_POWER_OFF)  ? "ADM_PHY_POWER_OFF" :
                           (setup->mode[i] == ADM_PHY_POWER_SAVE) ? "ADM_PHY_POWER_SAVE" :
                           (setup->mode[i] == ADM_PHY_POWER_ON)   ? "ADM_PHY_POWER_ON" : "unknown");
            cpmac_global.power.setup.mode[i] = setup->mode[i];
        }
        cpphy_mgmt_check_powersave_settings();
    }
    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static cpmac_err_t adm_switch_port_power_config_emv(cpphy_mdio_t          *mdio,
                                                    adm_phy_power_setup_t *setup,
                                                    unsigned char          set) {
    unsigned int i;

    if(!set) {
        setup->time_on  = cpmac_global.power.setup.time_on;
        setup->time_off = cpmac_global.power.roundrobin ? (4 * (  cpmac_global.power.setup.time_on
                                                                + cpmac_global.power.setup.time_off))
                                                        : cpmac_global.power.setup.time_off;
        for(i = 0; i < mdio->switch_config.ports; i++) {
            setup->mode[i] = cpmac_global.power.setup.mode[i];
        }
    } else {

        cpmac_global.power.timeout = 0;
        cpmac_global.power.current_mode = CPPHY_POWER_MODE_ON;
        cpmac_global.power.current_port = 0;

        cpmac_global.power.setup.time_on  = setup->time_on;
        cpmac_global.power.roundrobin = (((CPMAC_ADM_PHY_PUP_MDI_TO + CPMAC_ADM_PHY_PUP_MDIX_TO) * 4) < CPMAC_ADM_PHY_PUP_MUTE_TO);
        cpmac_global.power.setup.time_off =   cpmac_global.power.roundrobin
                                            ? ((CPMAC_ADM_PHY_PUP_MUTE_TO - (4 * (CPMAC_ADM_PHY_PUP_MDI_TO + CPMAC_ADM_PHY_PUP_MDIX_TO))) / 4)
                                            : CPMAC_ADM_PHY_PUP_MUTE_TO;
        DEB_INFOTRC("Power setup: %s with %u on and %u off\n", cpmac_global.power.roundrobin ? "roundrobin" : "normal",
                                                               cpmac_global.power.setup.time_on,
                                                               cpmac_global.power.setup.time_off);
        for(i = 0; i < cpmac_global.ports; i++) {
            cpmac_global.power.setup.mode[i] = setup->mode[i];
        }
        cpphy_mgmt_check_powersave_settings();
        cpphy_mgmt_power(mdio);
    }
    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t adm_switch_port_power_config(cpphy_mdio_t          *mdio,
                                         adm_phy_power_setup_t *setup,
                                         unsigned char          set) {
    cpmac_err_t err;

    if(mdio->switch_config.use_EMV) {
        err = adm_switch_port_power_config_emv(mdio, setup, set);
    } else {
        err = adm_switch_port_power_config_orig(mdio, setup, set);
    }
    cpmac_global.power.setup_asked_for = cpmac_global.power.setup;
    return err;
}


#if defined(CONFIG_AVM_POWER)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int adm_power_config(int state) {
    union _powermanagment_ethernet_state pstate;

    if(power_mdio->switch_config.use_EMV) {
        return 0;
    }

    pstate.Register = state;
    if(pstate.Bits.port >= cpmac_global.ports) {
        DEB_ERR("[%s] tried to set mode for port %u\n", __FUNCTION__, pstate.Bits.port);
        return -1;
    }
    if(pstate.Bits.status >= ADM_PHY_POWER_NUMBER_OF_MODES) {
        DEB_ERR("[%s] tried to set an illegal mode %u\n", __FUNCTION__, pstate.Bits.status);
        return -1;
    }
    cpmac_global.power.setup_asked_for.mode[pstate.Bits.port] = pstate.Bits.status;
    if(!(cpmac_global.power.port_modes_possible[pstate.Bits.port] & (1 << pstate.Bits.status))) {
        if(   (pstate.Bits.status == ADM_PHY_POWER_SAVE)
           && (cpmac_global.power.port_modes_possible[pstate.Bits.port] & (1 << ADM_PHY_POWER_ON))) {
            DEB_WARN("[%s] tried to set a mode %u that is not possible for this port %u. Fallback to mode %u.\n",
                    __FUNCTION__, pstate.Bits.status, pstate.Bits.port, ADM_PHY_POWER_ON);
            pstate.Bits.status = ADM_PHY_POWER_ON;
        } else {
            DEB_ERR("[%s] tried to set a mode %u that is not possible for this port %u.\n",
                    __FUNCTION__, pstate.Bits.status, pstate.Bits.port);
            return -1;
        }
    }
    cpmac_global.power.setup.mode[pstate.Bits.port] = pstate.Bits.status;
#   if defined(POWERMANAGEMENT_THROTTLE_ETH)
    cpmac_global.power.port_throttle_req[pstate.Bits.port] = pstate.Bits.throttle_eth ? 1 : 0;
    DEB_TRC("[%s] sets port %u to mode %s %s\n", 
            __FUNCTION__,
            pstate.Bits.port, 
            (pstate.Bits.status == ADM_PHY_POWER_OFF)  ? "ADM_PHY_POWER_OFF" :
            (pstate.Bits.status == ADM_PHY_POWER_SAVE) ? "ADM_PHY_POWER_SAVE" :
            (pstate.Bits.status == ADM_PHY_POWER_ON)   ? "ADM_PHY_POWER_ON" : "unknown",
            pstate.Bits.throttle_eth ? "throttled" : "");
#   else /*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/
    DEB_TRC("[%s] sets port %u to mode %s\n", 
            __FUNCTION__,
            pstate.Bits.port, 
            (pstate.Bits.status == ADM_PHY_POWER_OFF)  ? "ADM_PHY_POWER_OFF" :
            (pstate.Bits.status == ADM_PHY_POWER_SAVE) ? "ADM_PHY_POWER_SAVE" :
            (pstate.Bits.status == ADM_PHY_POWER_ON)   ? "ADM_PHY_POWER_ON" : "unknown");
#   endif /*--- #else ---*/ /*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/

    cpphy_mgmt_check_powersave_settings();
    return 0;
}
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#   if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19))
static void cpmac_work(void *data) {
    cpphy_mdio_t *mdio = (cpphy_mdio_t *) data;
#   else /*--- #if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)) ---*/
static void cpmac_work(struct work_struct *work) {
    cpphy_mdio_t *mdio = container_of((struct delayed_work *) work, cpphy_mdio_t, cpmac_work);
#   endif /*--- #else ---*/ /*--- #if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)) ---*/
    unsigned long next_run;
    unsigned long next_wait;
    unsigned int something_done, i;

    assert(mdio != NULL);
    do {
        mdio->work_to_do = 0;
        if(mdio->stop_work) {
            DEB_TRC("[%s] Stop work\n", __FUNCTION__);
            break;
        }
        something_done = 0;
        next_run = jiffies + 100000; /* Waiting should be a lot shorter than this */
        for(i = 0; i < CPMAC_WORK_MAX_ITEMS; i++) {
            if(mdio->work[i].active) {
                if(time_after_eq(jiffies, mdio->work[i].next_run)) { 
                    /* It is time to run this item */
                    if(mdio->work[i].item == NULL) {
                        DEB_ERR("[%s] item '%s' (%u) active, but without function!\n", 
                                __FUNCTION__, cpmac_work_names[i], i);
                        continue;
                    }
                    assert(mdio->work[i].item);
                    /*--- DEB_TEST("[%s] Running item %u\n", __FUNCTION__, i); ---*/
#                   if (CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_TRACE)
                    mdio->work[i].number_of_calls++;
                    if(time_after(jiffies, mdio->work[i].last_dump + HZ)) {
                        /*--- DEB_TRC("[%s] Running item '%s' (%u) totaling %u times\n", ---*/
                                /*--- __FUNCTION__, cpmac_work_names[i], i, mdio->work[i].number_of_calls); ---*/
                        mdio->work[i].last_dump = jiffies;
                    }
#                   endif /*--- #if (CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_TRACE) ---*/
                    mdio->work[i].next_run = jiffies + mdio->work[i].item(mdio);
                    /*--- DEB_TEST("[%s] Finished item %u\n", __FUNCTION__, i); ---*/
                    something_done = 1;
                    break; /* Restart queue checking */
                }
                if(time_after(next_run, mdio->work[i].next_run)) {
                    next_run = mdio->work[i].next_run;
                }
            }
        }
        if(!something_done) {
            next_wait = next_run - jiffies;
            if(time_after(next_run, jiffies)) {
                /*--- DEB_TEST("[%s] Waiting for %lu\n", __FUNCTION__, next_wait); ---*/
                wait_event_interruptible_timeout(mdio->waitqueue, mdio->work_to_do, next_wait);
            /*--- } else { ---*/
                /*--- DEB_TEST("[%s] Not waiting\n", __FUNCTION__); ---*/
            }
        }
    } while(1);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_work_start(cpphy_mdio_t *mdio) {
    mdio->stop_work = 0;
#   if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE)
    INIT_DELAYED_WORK(&mdio->cpmac_work, cpmac_work);
#   else /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
    INIT_WORK(&mdio->cpmac_work, cpmac_work, (void *) mdio);
#   endif /*--- #else ---*/ /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
    cancel_delayed_work(&mdio->cpmac_work);
    flush_workqueue(cpmac_global.workqueue);
    queue_delayed_work(cpmac_global.workqueue, &mdio->cpmac_work, 0);
    DEB_TRC("[%s] Start work\n", __FUNCTION__);
}


/*------------------------------------------------------------------------------------------*\
 * Add a function to be managed
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_mgmt_work_add(cpphy_mdio_t        *mdio, 
                                cpmac_work_ident_t   ident,
                                cpmac_work_function  function,
                                unsigned long        initial_timeout) {
    DEB_INFOTRC("[%s] Function for '%s' (%u) with timeout %lu added!\n", 
                __FUNCTION__,
                cpmac_work_names[ident],
                ident,
                initial_timeout);
    if((mdio->work[ident].item != NULL) && (mdio->work[ident].item != function)) {
        DEB_ERR("[%s] Different function for '%s' (%u) already added!\n",
                __FUNCTION__, cpmac_work_names[ident], ident);
        return CPMAC_ERR_INTERNAL;
    }
    mdio->work[ident].item = function;
    if(time_after(mdio->work[ident].next_run, jiffies + initial_timeout)) {
        mdio->work[ident].next_run = jiffies + initial_timeout;
    }
    mdio->work[ident].active = 1;
    mdio->work[ident].last_dump = jiffies;
    mdio->work[ident].number_of_calls = 0;
    mdio->work_to_do = 1;
    wake_up_interruptible_sync(&mdio->waitqueue);
    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
 * Remove a function to be managed
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_mgmt_work_del(cpphy_mdio_t       *mdio, 
                                cpmac_work_ident_t  ident) {
    if(mdio->work[ident].active == 0) {
        DEB_WARN("[%s] Function for '%s' (%u) is not set!\n", 
                 __FUNCTION__, cpmac_work_names[ident], ident);
        return CPMAC_ERR_INTERNAL;
    }
    mdio->work[ident].active = 0;
    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
 * Schedule the work item after the given timeout; 0 means as soon as possible
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_work_schedule(cpphy_mdio_t *mdio, 
                              cpmac_work_ident_t ident, 
                              unsigned long timeout) {
    if(mdio->work[ident].item == NULL) {
        DEB_INFO("[%s] Item %u has no function set. Not scheduled!\n", __FUNCTION__, ident);
        return;
    }
    mdio->work[ident].next_run = jiffies + timeout;
    mdio->work[ident].active = 1;
    mdio->work_to_do = 1;
    wake_up_interruptible_sync(&mdio->waitqueue);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_work_stop(cpphy_mdio_t *mdio) {
    mdio->stop_work = 1;
    mdio->work_to_do = 1;
    wake_up(&mdio->waitqueue);
    flush_workqueue(cpmac_global.workqueue);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_mgmt_init(cpphy_mdio_t *mdio) {
    unsigned int i;

    DEB_TRC("[%s]\n", __FUNCTION__);
    init_waitqueue_head(&mdio->waitqueue);
    mdio->work_to_do = 1;
    for(i = 0; i < CPMAC_MAX_WORK_ITEMS; i++) {
        mdio->work[i].active = 0;
        mdio->work[i].item = NULL;
        mdio->work[i].next_run = 0;
    }
    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_deinit(cpphy_mdio_t *mdio) {
    cpphy_mgmt_work_stop(mdio);
    if(mdio->f->deinit_phy) {
        mdio->f->deinit_phy(mdio);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mgmt_support_data(cpphy_mdio_t *mdio) {
#   if defined(CONFIG_MIPS_UR8) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    cpphy_cppi_t *cppi = mdio->cpmac_priv->cppi;
    unsigned int i;
    cpphy_tcb_t *tcb;
#   endif /*--- #if defined(CONFIG_MIPS_UR8) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/

    local_irq_disable();

    switch_dump(mdio);

#   if defined(CONFIG_MIPS_UR8) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    DEB_SUPPORT("tcb management normal : %u alloced, %u freed\n", 
                cppi->support.tcbs_alloced, 
                cppi->support.tcbs_freed);
    DEB_SUPPORT("tcb management dynamic: %u alloced, %u freed\n", 
                cppi->support.tcbs_alloced_dynamic,
                cppi->support.tcbs_freed_dynamic);
    for(i = 0; i < CPPHY_PRIO_QUEUES; i++) {
        unsigned int count = 0;
        DEB_SUPPORT("tx priority queue %u:\n", i);
        DEB_SUPPORT("    MaxSize       = %u\n", cppi->TxPrioQueues.q[i].MaxSize);
        DEB_SUPPORT("    Free          = %u\n", atomic_read(&cppi->TxPrioQueues.q[i].Free));
        for(tcb = (cpphy_tcb_t *) cppi->TxPrioQueues.q[i].First; tcb != NULL; tcb = (cpphy_tcb_t *) tcb->Next, count++)
            ;
        DEB_SUPPORT("      Counted     = %u\n", count);
        DEB_SUPPORT("    MaxDMAFree    = %u\n", cppi->TxPrioQueues.q[i].MaxDMAFree);
        DEB_SUPPORT("    DMAFree       = %u\n", atomic_read(&cppi->TxPrioQueues.q[i].DMAFree));
        DEB_SUPPORT("    Pause         = %u\n", cppi->TxPrioQueues.q[i].Pause);
        DEB_SUPPORT("    PauseInit     = %u\n", cppi->TxPrioQueues.q[i].PauseInit);
        DEB_SUPPORT("    BytesEnqueued = %u\n", cppi->TxPrioQueues.q[i].BytesEnqueued);
        DEB_SUPPORT("    BytesDequeued = %u\n", cppi->TxPrioQueues.q[i].BytesDequeued);
        DEB_SUPPORT("    First         = %p\n", cppi->TxPrioQueues.q[i].First);
        DEB_SUPPORT("    Last          = %p\n", cppi->TxPrioQueues.q[i].Last);
        DEB_SUPPORT("\n");
    }
#   endif /*--- #if defined(CONFIG_MIPS_UR8) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    DEB_SUPPORT("\n");
    
    local_irq_enable();
#   if defined(CONFIG_MIPS_UR8) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    tasklet_schedule(&mdio->cpmac_priv->tasklet);
#   endif /*--- #if defined(CONFIG_MIPS_UR8) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
}


/*------------------------------------------------------------------------------------------*\
 * Dump the given memory location                                                           *
 * Arguments: ptr    - Memory location                                                      *
 *            size   - size of items to output (1, 2, 4 bytes)                              *
 *            length - number of items to output                                            *
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_mgmt_debug_peek(void *ptr, unsigned int size, unsigned int length) {
#   define PEEK_BUFFER_SIZE 256
    char *buffer;
    unsigned int bufptr, i;

    if(ptr == NULL) {
        DEB_ERR("[%s] NULL pointer!\n", __FUNCTION__);
        return CPMAC_ERR_EXCEEDS_LIMIT;
    }
    buffer = kmalloc(PEEK_BUFFER_SIZE, GFP_KERNEL);
    if(buffer == NULL) {
        DEB_ERR("[%s] Out of memory!\n", __FUNCTION__);
        return CPMAC_ERR_NOMEM;
    }
    bufptr = 0;
    for(i = 1; i <= length; i++) {
        switch(size) {
            case 1:
                bufptr += snprintf(buffer + bufptr, PEEK_BUFFER_SIZE - bufptr, "%02x ", *(((unsigned char *) ptr) + i - 1));
                break;
            case 2:
                bufptr += snprintf(buffer + bufptr, PEEK_BUFFER_SIZE - bufptr, "%04x ", *(((unsigned short *) ptr) + i - 1));
                break;
            case 4:
                bufptr += snprintf(buffer + bufptr, PEEK_BUFFER_SIZE - bufptr, "%08x ", *(((unsigned int *) ptr) + i - 1));
                break;
            default:
                DEB_ERR("[%s] Illegal size (%u) given!\n", __FUNCTION__, size);
                kfree(buffer);
                return CPMAC_ERR_EXCEEDS_LIMIT;
        }
        if(!(i & 0x7)) {
            DEB_SUPPORT("  %s\n", buffer);
            bufptr = 0;
        }
    }
    if(bufptr != 0) {
        DEB_SUPPORT("  %s\n", buffer);
    }
    DEB_SUPPORT("\n");
    kfree(buffer);

    return CPMAC_ERR_NOERR;
#   undef PEEK_BUFFER_SIZE
}


/*------------------------------------------------------------------------------------------*\
 * Dump the given memory location                                                           *
 * Arguments: ptr   - Memory location                                                       *
 *            size  - size of items to write (1, 2, 4 bytes)                                *
 *            value - Value to write                                                        *
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_mgmt_debug_poke(void *ptr, unsigned int size, unsigned int value) {
    if(ptr == NULL) {
        DEB_ERR("[%s] NULL pointer!\n", __FUNCTION__);
        return CPMAC_ERR_EXCEEDS_LIMIT;
    }
    switch(size) {
        case 1:
            *((unsigned char *) ptr) = (unsigned char) value;
            break;
        case 2:
            *((unsigned short *) ptr) = (unsigned short) value;
            break;
        case 4:
            *((unsigned int *) ptr) = value;
            break;
        default:
            DEB_ERR("[%s] Illegal size (%u) given!\n", __FUNCTION__, size);
            return CPMAC_ERR_EXCEEDS_LIMIT;
    }
    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CPMAC_EXTERNAL_COMMAND_DEBUG)
void cpphy_mgmt_debug(cpphy_mdio_t *mdio __attribute__ ((unused)),
                      unsigned int value __attribute__ ((unused))) {
    DEB_SUPPORT("[%s] Switch reset %s\n", __FUNCTION__, value ? "off" : "on");

    cpphy_mgmt_device_power_up(mdio->reset_bit, value);
    /*--- cpphy_mgmt_work_add(mdio, CPMAC_WORK_DEBUG, cpphy_mgmt_debug_work, 1 * HZ); ---*/
}
#endif /*--- #if defined(CPMAC_EXTERNAL_COMMAND_DEBUG) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CPMAC_USE_DEBUG_WORK)
unsigned long cpphy_mgmt_debug_work(cpphy_mdio_t *mdio __attribute__ ((unused))) {
#   define SOC_GetReg32(address, regptr) { regptr = puma5_readl(address); }
#   define CPPI_SR_STAT_BASE 0x03020000
    unsigned int queue, regVal1, regVal2;

    /*--- DEB_SUPPORT("[%s] Queue informations\n", __FUNCTION__); ---*/
    queue = 120;
    SOC_GetReg32((CPPI_SR_STAT_BASE | (queue << 4)), regVal1);
    if(regVal1 != 510) {
        DEB_SUPPORT("Queue[ %3d ]: Free PP APDSP                         Descriptors: %d out of 510\n", queue, regVal1);
    }

    for(queue = 212; queue < 212+2; queue++) {
        SOC_GetReg32((CPPI_SR_STAT_BASE | (queue << 4)), regVal1);
        if(regVal1) {
            SOC_GetReg32(((CPPI_SR_STAT_BASE+4) | (queue << 4)), regVal2);
            DEB_SUPPORT("ETH_TX_Q%d (%3d)                 %5u   %u\n", queue - 212, queue, regVal1, regVal2);
        }
    }

    /* 0x3020B40 RX_HOST_MGMT_Q_ID */
    queue = 180;
    SOC_GetReg32((CPPI_SR_STAT_BASE | (queue << 4)), regVal1);
    if(regVal1) {
        SOC_GetReg32(((CPPI_SR_STAT_BASE+4) | (queue << 4)), regVal2);
        DEB_SUPPORT("HOST_MGMT_Q (%3d)               %5u   %u\n", queue, regVal1, regVal2);
    }

    /* 0x3020B50 RX_HOST_VOICE_Q_ID */
    queue = 181;
    SOC_GetReg32((CPPI_SR_STAT_BASE | (queue << 4)), regVal1);
    if(regVal1) {
        SOC_GetReg32(((CPPI_SR_STAT_BASE+4) | (queue << 4)), regVal2);
        DEB_SUPPORT("HOST_VOICE_Q (%3d)              %5u   %u\n", queue, regVal1, regVal2);
    }

    /* 0x3020B60 RX_HOST_LOW_Q0_ID... 0x3020BD0 RX_HOST_LOW_Q7_ID */
    for(queue = 182; queue <= 189; queue++) {
        SOC_GetReg32((CPPI_SR_STAT_BASE | (queue << 4)), regVal1);
        if(regVal1) {
            SOC_GetReg32(((CPPI_SR_STAT_BASE+4) | (queue << 4)), regVal2);
            DEB_SUPPORT("HOST_BE_Q%d (%3d)                %5u   %u\n", queue - 182, queue, regVal1, regVal2);
        }
    }
    return CPMAC_DEBUG_INTERVAL;
}
#endif /*--- #if defined(CPMAC_USE_DEBUG_WORK) ---*/

