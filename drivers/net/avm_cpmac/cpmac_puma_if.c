/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2008,2009,...,2011 AVM GmbH <fritzbox_info@avm.de>
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

#if defined(CONFIG_ARCH_PUMA5)

#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#ifdef CONFIG_AVM_RTP_TIMESTAMP
#include <linux/rtp_timestamp.h>
#endif
#include "cpmac_if.h"
#include "cpmac_main.h"
#include "cpmac_puma_if.h"
#include "cpphy_types.h"
#include "cpmac_debug.h"
#include "cpphy_ar8216.h"
#include "cpphy_mdio.h"
#include "cpphy_const.h"
#include "cpphy_if.h"
#include "cpmac_eth.h"
#include <asm/mach_avm.h>
#include "cpphy_mgmt.h"
#include <mach/hw_gpio.h>
#include <linux/ar_reg.h>
#ifdef CONFIG_INET_LRO 
#include <linux/inet_lro.h>
#endif
#include "../avalanche_cpgmac_f/mdio.h"
#include "../avalanche_cpgmac_f/cpgmac_f_NetLx.h"

#if defined(CONFIG_IP_MULTICAST_FASTFORWARD)
extern struct mcfw_netdriver *cpmac_mcfw_netdrv;
extern int cpmac_mcfw_sourceid;
#endif /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/

/* TODO list */
/* - Get MAC address from environment and assign it to the PHY (from cpphy_if_init) */
 
#define CPMAC_PUMA_MAGPIE_PORT    5

/*------------------------------------------------------------------------------------------*\
 * Global resources
\*------------------------------------------------------------------------------------------*/
#define CPMAC_MAGPIE_RESET (32u + 12u)
#define CPMAC_MAGPIE_MDIO  (32u + 13u)
#define CPMAC_MAGPIE_MDC   (32u + 14u)
static struct resource cpmac_resource_mdio_switch_gpio = {
    .name = "CPMAC MAGPIE MDIO switch",
    .flags = IORESOURCE_IO,
    .start = CPMAC_MAGPIE_RESET,
    .end   = CPMAC_MAGPIE_MDC
};

static struct mdio_regs *MDIO = (struct mdio_regs *)&(*(volatile unsigned int *) AVALANCHE_MDIO_BASE);
static atomic_t last_mdio_target;
cpmac_priv_t puma_cpmac_priv;
void *puma_phy_dev = NULL;
extern cpmac_global_t cpmac_global;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_puma_init(struct net_device *dev __attribute__ ((unused)),
                     void *phy_dev) {
    DEB_TRC("[%s]\n", __FUNCTION__);
    if(puma_phy_dev) {
        DEB_ERR("[%s] Attention! phy_dev already set!\n", __FUNCTION__);
    } else {
        puma_phy_dev = phy_dev;
        atomic_set(&last_mdio_target, CPMAC_MDIO_TARGET_NONE);
    }
    cpmac_global.cpphy[0].cpmac_priv = &puma_cpmac_priv;

    if(cpmac_global.cpphy[0].config->type == CPMAC_PHY_TYPE_AR8316) {
        if(request_resource(&gpio_resource, &cpmac_resource_mdio_switch_gpio)) {
            DEB_ERR("[%s] GPIO resource not available!\n", __FUNCTION__);
        }
        local_irq_disable();
        avm_gpio_ctrl(CPMAC_MAGPIE_MDC, GPIO_PIN, GPIO_OUTPUT_PIN);
        avm_gpio_out_bit(CPMAC_MAGPIE_MDC, 0);
        avm_gpio_ctrl(CPMAC_MAGPIE_MDIO, GPIO_PIN, GPIO_OUTPUT_PIN);
        avm_gpio_out_bit(CPMAC_MAGPIE_MDIO, 1);
        avm_gpio_ctrl(CPMAC_MAGPIE_RESET, GPIO_PIN, GPIO_OUTPUT_PIN);
        avm_gpio_out_bit(CPMAC_MAGPIE_RESET, 0);
        local_irq_enable();
        msleep(1000);
    }
    cpmac_global.cpphy[0].mdio.switch_config.mdio_target = CPMAC_MDIO_TARGET_SWITCH;

    cpmac_global.cpphy[0].f->cpmac_transfer_startstop = cpmac_puma_transfer_startstop;

    cpphy_mdio_init(&cpmac_global.cpphy[0]);

    if(cpmac_global.cpphy[0].config->type == CPMAC_PHY_TYPE_AR8316) {
        ar_ar8316_mac5_enable(&cpmac_global.cpphy[0].mdio, 0);
    }
}
EXPORT_SYMBOL(cpmac_puma_init);


#if defined(CONFIG_ARCH_PUMA5)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_puma_set_dev(struct net_device *dev) {
    DEB_TRC("[%s] '%s'\n", __FUNCTION__, dev->name);
    puma_cpmac_priv.owner = dev;
}

struct net_device *cpmac_puma_get_dev(void) {
   return puma_cpmac_priv.owner;
}

EXPORT_SYMBOL(cpmac_puma_set_dev);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_magpie_reset(enum avm_cpmac_magpie_reset value) {
    cpphy_mdio_t *mdio = &cpmac_global.cpphy[0].mdio;

    if(cpmac_global.cpphy[0].config->type != CPMAC_PHY_TYPE_AR8316) {
        return;
    }

    if(   (value == CPMAC_MAGPIE_RESET_ON)
       || (value == CPMAC_MAGPIE_RESET_PULSE)) {
        DEB_INFOTRC("[%s] Putting Magpie into reset\n", __FUNCTION__);
        mdio->switch_config.magpie_reset = CPMAC_MAGPIE_RESET_ON;
        avm_gpio_out_bit(CPMAC_MAGPIE_RESET, 0);
        ar_ar8316_mac5_enable(mdio, 0);
        msleep(1000);
    }
    if(   (value == CPMAC_MAGPIE_RESET_OFF)
       || (value == CPMAC_MAGPIE_RESET_PULSE)) {
        DEB_INFOTRC("[%s] Getting Magpie out of reset\n", __FUNCTION__);
        ar_ar8316_mac5_enable(mdio, 1);
        msleep(500);
        avm_gpio_out_bit(CPMAC_MAGPIE_RESET, 1);
        msleep(500);
        mdio->switch_config.magpie_reset = CPMAC_MAGPIE_RESET_OFF;
    }
}
EXPORT_SYMBOL(cpmac_magpie_reset);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int cpmac_puma_mdio_select(cpphy_mdio_t                *mdio, 
                                           enum avm_cpmac_mdio_targets  target) {
    mdio->switch_config.mdio_target = CPMAC_MDIO_TARGET_NONE;

    if(   (target == CPMAC_MDIO_TARGET_MAGPIE)
       && (mdio->switch_config.magpie_reset == CPMAC_MAGPIE_RESET_ON)) {
        DEB_WARN("[%s] Magpie still in reset. Not switching!\n", __FUNCTION__);
        return 1;
    }

    MDIO->control.Bits.enable = 0;
    while(!MDIO->control.Bits.idle) {
        schedule();
    }
    switch(target) {
        case CPMAC_MDIO_TARGET_SWITCH:
            schedule();
            avm_gpio_out_bit(CPMAC_MAGPIE_MDIO, 1);
            schedule();
            avm_gpio_out_bit(CPMAC_MAGPIE_MDC,  0);
            schedule();
            MDIO->control.Bits.enable = 1;
            mdio->switch_config.mdio_target = CPMAC_MDIO_TARGET_SWITCH;
            DEB_INFOTRC("[%s] Selected switch as MDIO target\n", __FUNCTION__);
            break;
        case CPMAC_MDIO_TARGET_MAGPIE:
            schedule();
            avm_gpio_out_bit(CPMAC_MAGPIE_MDC,  1);
            schedule();
            avm_gpio_out_bit(CPMAC_MAGPIE_MDIO, 0);
            schedule();
            MDIO->control.Bits.enable = 1;
            mdio->switch_config.mdio_target = CPMAC_MDIO_TARGET_MAGPIE;
            DEB_INFOTRC("[%s] Selected Magpie as MDIO target\n", __FUNCTION__);
            break;
        default:
            DEB_ERR("[%s] Illegal target %u given!\n", __FUNCTION__, target);
            break;
    }
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpmac_puma_mdio_unlock(cpphy_mdio_t *mdio) {
    atomic_set(&last_mdio_target, CPMAC_MDIO_TARGET_NONE);
    up(&mdio->semaphore);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int cpmac_puma_mdio_lock(cpphy_mdio_t                *mdio,
                                         enum avm_cpmac_mdio_targets  target) {
    unsigned int lock_tries = 0, ret = 0;
    unsigned int sleep_time = 10;

    while(down_trylock(&mdio->semaphore)) {
        msleep(sleep_time);
        sleep_time += 5;
        lock_tries++;
        if(lock_tries > 50) {
            enum avm_cpmac_mdio_targets last_target = atomic_read(&last_mdio_target);
            DEB_WARN("[%s] mdio semaphore unavailable (%s tried to get it from %s)\n", 
                     __FUNCTION__,
                     (target == CPMAC_MDIO_TARGET_MAGPIE) ? "magpie" : "switch",
                     (last_target == CPMAC_MDIO_TARGET_MAGPIE) ? "magpie" : 
                     (last_target == CPMAC_MDIO_TARGET_SWITCH) ? "magpie" : "none");
            return 1;
        }
    }
    if(   (target == CPMAC_MDIO_TARGET_MAGPIE)
       && (mdio->switch_config.magpie_reset == CPMAC_MAGPIE_RESET_ON)) {
        DEB_WARN("[%s] Magpie still in reset. Not locking!\n", __FUNCTION__);
        cpmac_puma_mdio_unlock(mdio);
        return 1;
    }

    atomic_set(&last_mdio_target, target);
    if(mdio->switch_config.mdio_target != target) {
        ret = cpmac_puma_mdio_select(mdio, target);
        if(ret) {
            cpmac_puma_mdio_unlock(mdio);
        }
    }
    return ret;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_magpie_mdio_write(unsigned short regadr,
                             unsigned short phyadr,
                             unsigned short data) {
    cpphy_mdio_t *mdio = &cpmac_global.cpphy[0].mdio;

    if(cpmac_puma_mdio_lock(mdio, CPMAC_MDIO_TARGET_MAGPIE)) {
        return;
    }
    puma_mdio_write(puma_phy_dev, regadr, phyadr, data);
    cpmac_puma_mdio_unlock(mdio);
}
EXPORT_SYMBOL(cpmac_magpie_mdio_write);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpmac_magpie_mdio_read(unsigned short regadr,
                                    unsigned short phyadr) {
    unsigned int result;
    cpphy_mdio_t *mdio = &cpmac_global.cpphy[0].mdio;

    if(cpmac_puma_mdio_lock(mdio, CPMAC_MDIO_TARGET_MAGPIE)) {
        return 0;
    }
    result = puma_mdio_read(puma_phy_dev, regadr, phyadr);
    cpmac_puma_mdio_unlock(mdio);
    return result;
}
EXPORT_SYMBOL(cpmac_magpie_mdio_read);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mdio_user_access_write(cpphy_mdio_t  *mdio __attribute__ ((unused)),
                                  unsigned short regadr,
                                  unsigned short phyadr,
                                  unsigned short data) {
    mdio = &cpmac_global.cpphy[0].mdio;

    if(cpmac_puma_mdio_lock(mdio, CPMAC_MDIO_TARGET_SWITCH)) {
        return;
    }
    puma_mdio_write(puma_phy_dev, regadr, phyadr, data);
    cpmac_puma_mdio_unlock(mdio);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mdio_user_access_read(cpphy_mdio_t  *mdio __attribute__ ((unused)),
                                         unsigned short regadr,
                                         unsigned short phyadr) {
    unsigned int result;
    mdio = &cpmac_global.cpphy[0].mdio;

    if(cpmac_puma_mdio_lock(mdio, CPMAC_MDIO_TARGET_SWITCH)) {
        return 0;
    }
    result = puma_mdio_read(puma_phy_dev, regadr, phyadr);
    cpmac_puma_mdio_unlock(mdio);
    return result;
}
#endif /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_puma_transfer_startstop(unsigned int start) {
    cpmac_global.cpphy[0].mdio.switch_config.is_being_configured = start ? 0 : 1;
    if(puma_cpmac_priv.owner) {
        if(start) {
            netif_start_queue(puma_cpmac_priv.owner);
        } else {
            netif_stop_queue(puma_cpmac_priv.owner);
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_puma_set_puma_functions(void (*puma_transfer_startstop)(unsigned int start) __attribute__ ((unused))) {
    cpmac_global.cpphy[0].f->cpmac_transfer_startstop = puma_transfer_startstop;
}
EXPORT_SYMBOL(cpmac_puma_set_puma_functions);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_puma_open(void) {
    DEB_INFOTRC("[%s]\n", __FUNCTION__);

    if(cpmac_global.cpphy[0].is_open == 0) {
        cpphy_mdio_t *mdio = &cpmac_global.cpphy[0].mdio;

        cpmac_global.cpphy[0].is_open = 1;

        /* TODO Find cleaner solution for the power on */
        cpphy_mgmt_work_stop(&cpmac_global.cpphy[0].mdio);
        cpphy_mgmt_global_power_set(CPPHY_POWER_GLOBAL_ON);
        cpphy_mgmt_process_global_power(&cpmac_global.cpphy[0].mdio);
        cpphy_mgmt_work_start(&cpmac_global.cpphy[0].mdio);
        cpphy_mgmt_work_schedule(&cpmac_global.cpphy[0].mdio, 0, 0);

        if(strcmp(puma_cpmac_priv.owner->name, "eth0") == 0) {
            mdio->switch_config.devices = 1;
            mdio->switch_config.device[0].net_device = puma_cpmac_priv.owner;
            mdio->switch_config.device[0].portset = 0x1;
        }
    }
}
EXPORT_SYMBOL(cpmac_puma_open);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_puma_close(void) {
    DEB_INFOTRC("[%s]\n", __FUNCTION__);

    cpmac_global.cpphy[0].is_open = 0;

    /* TODO Find cleaner solution for the power on */
    cpphy_mgmt_work_stop(&cpmac_global.cpphy[0].mdio);
    cpphy_mgmt_global_power_set(CPPHY_POWER_GLOBAL_SET_OFF);
    cpphy_mgmt_process_global_power(&cpmac_global.cpphy[0].mdio);
    cpphy_mgmt_work_start(&cpmac_global.cpphy[0].mdio);
    cpphy_mgmt_work_schedule(&cpmac_global.cpphy[0].mdio, 0, 0);
}
EXPORT_SYMBOL(cpmac_puma_close);

#ifdef CONFIG_TI_DEVICE_PROTOCOL_HANDLING
extern int ti_protocol_handler (struct net_device* dev, struct sk_buff *skb);
#endif

/*------------------------------------------------------------------------------------------*\
 * TODO: The receive functions here and in cpmac_if.c should be unified
\*------------------------------------------------------------------------------------------*/
void cpmac_puma_if_rx(struct sk_buff *skb) {
    cpphy_mdio_t *mdio = &cpmac_global.cpphy[0].mdio;
    unsigned char port = 0xff, is_wan = 0;
    struct net_device *p_dev;
    signed int skb_count;

#   if defined(CONFIG_IP_MULTICAST_FASTFORWARD)
#   if(LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28))
    int hlen = skb->data - skb->mac.raw;
#   else /*--- #if(LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)) ---*/
    int hlen = skb->data - skb_mac_header(skb);
#   endif /*--- #else ---*/ /*--- #if(LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)) ---*/
#   endif /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/

    if(mdio->switch_config.is_being_configured) {
        CpmacNetDevice *hDDA = netdev_priv(puma_cpmac_priv.owner);
        struct net_device_stats *p_netDevStats = &hDDA->netDevStats;
        p_netDevStats->rx_dropped++;
        dev_kfree_skb_any(skb);
        return;
    }
    
#   ifdef CONFIG_AVM_RTP_TIMESTAMP
    skb_push(skb, hlen);
    rtp_timestamp_insert_in_skb(skb);
    skb_pull(skb, hlen);
#   endif     

    skb_count = atomic_read(&skbs_in_use);
#   ifdef CONFIG_AVM_PA
    if(likely(skb_count < (signed int) CPPHY_TRAFFIC_THROTTLE_MIN)) {
        if(avm_pa_dev_receive(AVM_PA_DEVINFO(puma_cpmac_priv.owner), skb) == 0)
            return; /* skb accelerated */
    }
#   endif /*--- #ifdef CONFIG_AVM_PA ---*/
    if(mdio->f->get_phy_port) {
        port = mdio->f->get_phy_port(mdio, skb);
        if(port == 100) {
            return; /* skb reception postponed */
        }
        assert(port < AVM_CPMAC_MAX_PORTS);

        if(unlikely(skb_count >= (signed int) CPPHY_TRAFFIC_THROTTLE_MIN)) {
            enum cpmac_traffic_classes class = CPMAC_TRAFFIC_OTHER;

            if(unlikely(skb_count > (signed int) CPPHY_TRAFFIC_THROTTLE_MAX)) {
                skb_count = CPPHY_TRAFFIC_THROTTLE_MAX;
            }

            if(   (port == CPMAC_PUMA_MAGPIE_PORT)
               && (cpmac_global.traffic_classify.wlan_classify != NULL)) {
                class = cpmac_global.traffic_classify.wlan_classify(skb);
                if(class > CPMAC_TRAFFIC_OTHER) {
                    class = CPMAC_TRAFFIC_OTHER;
                }
            }
            if(class != CPMAC_TRAFFIC_ESSENTIAL) {
                cpmac_global.traffic_classify.packet_counter[class]++;
                if(cpmac_global.traffic_classify.packet_counter[class] >= (((  (signed int) CPPHY_TRAFFIC_THROTTLE_MAX
                                                                             - skb_count
                                                                             - (500 * ((signed int) class - 1))
                                                                            ) / 50 + 1))) {
                    CpmacNetDevice *hDDA = netdev_priv(puma_cpmac_priv.owner);
                    struct net_device_stats *p_netDevStats = &hDDA->netDevStats;
                    p_netDevStats->rx_dropped++;
                    dev_kfree_skb_any(skb);
                    cpmac_global.traffic_classify.packet_counter[class] = 0;
                    return;
                }
            }
#           ifdef CONFIG_AVM_PA
            /* Receive with AVM_PA in case of special handling in high load situation */
            if(avm_pa_dev_receive(AVM_PA_DEVINFO(puma_cpmac_priv.owner), skb) == 0)
                return; /* skb accelerated */
#           endif /*--- #ifdef CONFIG_AVM_PA ---*/
        }

        is_wan = (port == mdio->switch_config.wanport);
        if(port == CPMAC_PUMA_MAGPIE_PORT) {
            skb->uniq_id &= 0xffffff;
        } else if(!is_wan) {
            skb->input_dev = puma_cpmac_priv.owner;
        }
#       if defined(CONFIG_TI_DEVICE_PROTOCOL_HANDLING)
        if(port != CPMAC_PUMA_MAGPIE_PORT) {
            p_dev = skb->dev = puma_cpmac_priv.owner;
            if(ti_protocol_handler (p_dev, skb) < 0) {
                printk(KERN_DEBUG "%s: packet from %s stolen by %pF\n",
                        __FUNCTION__,
                        p_dev->name,
                        p_dev->packet_handler);
                /* Device Specific Protocol handler has "captured" the packet
                 * and does not want to send it up the networking stack; so 
                 * return immediately. */
                return;
            }
        }
#       endif /*--- #if defined(CONFIG_TI_DEVICE_PROTOCOL_HANDLING) ---*/
        if(likely(cpmac_global.cpphy[0].mdio.switch_config.devices > 0)) {
            p_dev = mdio->switch_config.device[mdio->switch_config.map_port_to_dev[port]].net_device;
            ((struct cpmac_devinfo *) (p_dev->priv))->stats.rx_packets++;
            ((struct cpmac_devinfo *) (p_dev->priv))->stats.rx_bytes += skb->len;
            skb->dev = p_dev;
        } else {
            skb->dev = puma_cpmac_priv.owner;
        }
        DEB_DEBUG("[%s] p%u - dev %u (%p) (%s): %u %*pB ... \n", 
        __FUNCTION__,
        port, 
        mdio->switch_config.map_port_to_dev[port],
        skb->dev,
        skb->dev->name,
        skb->len, 
        min(24u, skb->len), 
        skb->data);
    }

    if((port < AVM_CPMAC_MAX_PORTS) && (cpmac_global.ffw_rx_ptr[port] != NULL)) {
        DEB_DEBUG("[%s] fast forwarding packet for port %u\n", __FUNCTION__, port);
        cpmac_global.ffw_rx_ptr[port](skb);
    } else {
#       if defined(CONFIG_IP_MULTICAST_FASTFORWARD)
        if(port == 0xff) {
            port = 0;
        }
        skb_push(skb, hlen);
        mcfw_snoop_recv(cpmac_mcfw_netdrv, port, skb);
        skb_pull(skb, hlen);
#       endif /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/
#       ifdef CONFIG_INET_LRO 
        if(skb->dev->features & NETIF_F_LRO) {
            CpmacNetDevice *hDDA = netdev_priv(skb->dev);
            lro_receive_skb(&hDDA->lro_mgr, skb, (void *) hDDA);
        } else {
            netif_receive_skb(skb);
        }
#       else /*--- #ifdef CONFIG_INET_LRO ---*/ 
        netif_receive_skb(skb);
#       endif /*--- #else ---*/ /*--- #ifdef CONFIG_INET_LRO ---*/ 
    }
}
EXPORT_SYMBOL(cpmac_puma_if_rx);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int cpmac_puma_ioctl(struct net_device *dev __attribute__ ((unused)),
                     struct ifreq *ifr,
                     int cmd __attribute__ ((unused))) {
    struct avm_cpmac_ioctl_struct ioctl_struct;
    cpmac_err_t err;

    if(copy_from_user(&ioctl_struct, ifr->ifr_data, sizeof(ioctl_struct))) {
        DEB_ERR("[%s] copy from user failed\n", __FUNCTION__);
        return -EFAULT;
    }
    err = cpphy_switch_ioctl(&cpmac_global.cpphy[0].mdio, &ioctl_struct);
    switch(err) {
        case CPMAC_ERR_NOERR:
            break;
        case CPMAC_ERR_NO_VLAN_POSSIBLE:
        case CPMAC_ERR_CHAN_NOT_OPEN:
            return -EIO;
        default:
            return -EINVAL;
    }
    if(copy_to_user(ifr->ifr_data, &ioctl_struct, sizeof(ioctl_struct))) {
        DEB_ERR("[%s] copy to user failed\n", __FUNCTION__);
        return -EFAULT;
    }
    return 0;
}
EXPORT_SYMBOL(cpmac_puma_ioctl);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpmac_register_magpie_receive(cpmac_fast_forward_rx_func_ptr magpie_receive) {
    return cpphy_switch_register_ffw_port(magpie_receive, CPMAC_PUMA_MAGPIE_PORT);
}
EXPORT_SYMBOL(cpmac_register_magpie_receive);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_unregister_magpie_receive(void) {
    cpphy_switch_unregister_ffw_port(CPMAC_PUMA_MAGPIE_PORT);
}
EXPORT_SYMBOL(cpmac_unregister_magpie_receive);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpmac_register_wlan_classify(cpmac_wlan_classify_func_ptr wlan_classify) {
    DEB_INFO("[%s] Registering wlan_classify 0x%p (%pF)\n", __FUNCTION__, wlan_classify, wlan_classify);
    cpmac_global.traffic_classify.wlan_classify = wlan_classify;
    return 0;
}
EXPORT_SYMBOL(cpmac_register_wlan_classify);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_unregister_wlan_classify(void) {
    DEB_INFO("[%s] Unregistering wlan_classify\n", __FUNCTION__);
    cpmac_global.traffic_classify.wlan_classify = NULL;
}
EXPORT_SYMBOL(cpmac_unregister_wlan_classify);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_puma_printk(const char *text __attribute__ ((unused)),
                       unsigned int line __attribute__ ((unused))) {
    DEB_TEST("[%s] %s - %u\n", __FUNCTION__, text, line);
}

#endif /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/

