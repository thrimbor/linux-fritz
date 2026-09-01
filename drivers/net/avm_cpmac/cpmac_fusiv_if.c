/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2008,2009,2010 AVM GmbH <fritzbox_info@avm.de>
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

#if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)

#include <linux/delay.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#ifdef CONFIG_AVM_RTP_TIMESTAMP
#include <linux/rtp_timestamp.h>
#endif
#include "cpmac_if.h"
#include "cpmac_main.h"
#include "cpmac_fusiv_if.h"
#include "cpphy_types.h"
#include "cpmac_debug.h"
#include "cpphy_ar8216.h"
#include "cpphy_mdio.h"
#include "cpphy_if.h"
#include "cpmac_eth.h"
#include <asm/mach_avm.h>
#include "cpphy_mgmt.h"

#include <netpro/hostconfig.h>

/* TODO list */
/* - Get MAC address from environment and assign it to the PHY (from cpphy_if_init) */
 
extern cpmac_global_t cpmac_global;
extern unsigned int cpmac_devices_installed;
struct semaphore mdio_semaphore;
EXPORT_SYMBOL(mdio_semaphore);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_set_vx180_port(void                   *pPort,
                          unsigned int            instance,
                          fusiv_mdio_read_t       mdio_read_ptr,
                          fusiv_mdio_write_t      mdio_write_ptr,
                          fusiv_set_port_speed_t  set_port_speed) {
    cpmac_priv_t *cpmac_priv;

    DEB_INFO("[%s] Initializing for instance %u\n", __FUNCTION__, instance);
    cpmac_priv = cpmac_global.cpphy[instance].cpmac_priv;
    assert(cpmac_priv); /* This must be set, otherwise something must have gone badly wrong */
    cpmac_priv->fusiv.port = pPort;
    cpmac_priv->fusiv.mdio_read = mdio_read_ptr;
    cpmac_priv->fusiv.mdio_write = mdio_write_ptr;
    cpmac_priv->fusiv.set_port_speed = set_port_speed;
    if(instance == 0) {
        init_MUTEX(&mdio_semaphore);
    }
    cpphy_mdio_init(&cpmac_global.cpphy[instance]);
}
EXPORT_SYMBOL(cpmac_set_vx180_port);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_set_vx180_dev(struct net_device *dev) {
    cpmac_priv_t *cpmac_priv;

    cpmac_priv = kzalloc(sizeof(cpmac_priv_t), GFP_KERNEL);
    cpmac_global.cpphy[cpmac_devices_installed].cpmac_priv = cpmac_priv;
    assert(cpmac_priv); /* Can not work without memory */
    cpmac_priv->owner = dev;
    cpmac_devices_installed++;
#   ifdef CONFIG_AVM_PA
    {
        struct avm_pa_pid_cfg cfg;

        snprintf(cfg.name, sizeof(cfg.name), "%s", dev->name);
        cfg.framing = avm_pa_framing_dev;
        cfg.default_mtu = 1500;
        cfg.tx_func = cpmac_pa_dev_transmit;
        cfg.tx_arg = dev;
        if(avm_pa_dev_pid_register(AVM_PA_DEVINFO(dev), &cfg) < 0) {
            printk(KERN_ERR "%s: failed to register PA PID\n", cfg.name);
		} else {
		   struct avm_pa_pid_hwinfo hwinfo;
		   memset(&hwinfo, 0, sizeof(hwinfo));
		   if (strcmp(dev->name, "eth0") == 0)
		      hwinfo.apId = MAC2_ID; /* 7340 */
		   else
		      hwinfo.apId = MAC1_ID;
		   if (avm_pa_pid_set_hwinfo(dev->avm_pa.devinfo.pid_handle, &hwinfo) < 0)
			  printk(KERN_ERR "%s: failed to set HW Info for PID %d", 
					 cfg.name, dev->avm_pa.devinfo.pid_handle);
		}
    }
	if (strncmp(dev->name, "cpmac", 5) != 0) {  /* z.B. 7340 */
        struct avm_pa_vpid_cfg cfg;
        snprintf(cfg.name, sizeof(cfg.name), "%s", dev->name);
        cfg.v4_mtu = 1500;
        cfg.v6_mtu = 1500;
        if(avm_pa_dev_vpid_register(AVM_PA_DEVINFO(dev), &cfg) < 0)
            printk(KERN_ERR "%s: failed to register PA VPID\n", cfg.name);

    }
#   endif /*--- #ifdef CONFIG_AVM_PA ---*/
    DEB_INFO("[%s] Instance %u started\n", __FUNCTION__, cpmac_devices_installed);
}
EXPORT_SYMBOL(cpmac_set_vx180_dev);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_mdio_user_access_read(cpphy_mdio_t  *mdio,
                                         unsigned short regadr,
                                         unsigned short phyadr) {
    return mdio->cpmac_priv->fusiv.mdio_read(mdio->cpmac_priv->fusiv.port, regadr, phyadr);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_mdio_user_access_write(cpphy_mdio_t  *mdio,
                                  unsigned short regadr,
                                  unsigned short phyadr,
                                  unsigned short data) {
    mdio->cpmac_priv->fusiv.mdio_write(mdio->cpmac_priv->fusiv.port, regadr, phyadr, data);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ath_mdio_write(cpphy_mdio_t *mdio, unsigned short address, unsigned short value) {
    unsigned short reg_addr;

    reg_addr = (address & 0xfffffffc);
    mdio_write(mdio, 0x18, 0, (reg_addr >> 9) & 0x1ff);
    mdio_write(mdio, 0x10 | ((reg_addr >> 6) & 0x7), ((reg_addr >> 1) & 0x1F) + 1, value >> 16);
    mdio_write(mdio, 0x10 | ((reg_addr >> 6) & 0x7), (reg_addr >> 1) & 0x1F, value & 0xFFFF);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_fusiv_set_fusiv_functions(void (*fusiv_transfer_startstop)(unsigned int start)) {
    cpmac_global.cpphy[0].f->cpmac_transfer_startstop = fusiv_transfer_startstop;
}
EXPORT_SYMBOL(cpmac_fusiv_set_fusiv_functions);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_fusiv_open(unsigned int instance) {
    DEB_TRC("[%s] instance %u\n", __FUNCTION__, instance);
    cpmac_global.cpphy[instance].is_open = 1;

    /* TODO Find cleaner solution for the power on */
    cpphy_mgmt_work_stop(&cpmac_global.cpphy[0].mdio);
    cpphy_mgmt_global_power_set(CPPHY_POWER_GLOBAL_ON);
    cpphy_mgmt_process_global_power(&cpmac_global.cpphy[0].mdio);
    cpphy_mgmt_work_start(&cpmac_global.cpphy[0].mdio);
    cpphy_mgmt_work_schedule(&cpmac_global.cpphy[0].mdio, CPMAC_WORK_TICK, 0);
}
EXPORT_SYMBOL(cpmac_fusiv_open);


/*------------------------------------------------------------------------------------------*\
 * TODO: The receive functions here and in cpmac_if.c should be unified
\*------------------------------------------------------------------------------------------*/
void cpmac_fusiv_if_rx2(struct sk_buff *skb, struct mcfw_netdriver *mcfwdrv) {
    cpphy_mdio_t *mdio = &cpmac_global.cpphy[0].mdio;
    unsigned char port, is_wan = 0;
    struct net_device *p_dev;
#   if defined(CONFIG_IP_MULTICAST_FASTFORWARD)
#   if(LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28))
    int hlen = skb->data - skb->mac.raw;
#   else /*--- #if(LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)) ---*/
    int hlen = skb->data - skb_mac_header(skb);
#   endif /*--- #else ---*/ /*--- #if(LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)) ---*/
#   endif /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/

#   if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_DEBUG
    skb_push(skb, hlen);
    DEB_DEBUG("[%s] %s: (%u) %*pB ... \n", 
              __FUNCTION__,
              skb->dev->name,
              skb->len,
              min(24u, skb->len),
              skb->data); 
    skb_pull(skb, hlen);
#   endif /*--- #if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_DEBUG ---*/

    /* AVM_PA call is now in FUSIV driver */

    if(likely(mdio->f->get_phy_port)) {
        port = mdio->f->get_phy_port(mdio, skb);
        if(unlikely(port == 100)) {
            return; /* skb reception postponed */
        }
        assert(port < AVM_CPMAC_MAX_PORTS);
        is_wan = (port == mdio->switch_config.wanport);
        if(likely(cpmac_global.cpphy[0].mdio.switch_config.devices > 0)) {
            p_dev = mdio->switch_config.device[mdio->switch_config.map_port_to_dev[port]].net_device;
            ((struct cpmac_devinfo *) (p_dev->priv))->stats.rx_packets++;
            ((struct cpmac_devinfo *) (p_dev->priv))->stats.rx_bytes += skb->len;
            skb->dev = p_dev;
        }
        /*--- DEB_DEBUG("[%s] p%u - %u (%s): %u %*pB ... \n", ---*/ 
                /*--- __FUNCTION__, ---*/
                /*--- port, ---*/ 
                /*--- mdio->switch_config.map_port_to_dev[port], ---*/
                /*--- skb->dev->name, ---*/
                /*--- skb->len, ---*/
                /*--- min(24u, skb->len), ---*/
                /*--- skb->data); ---*/
    }

    if(is_wan && mdio->switch_config.wan_receive_function) {
        mdio->switch_config.wan_receive_function(skb);
    } else {
#       if defined(CONFIG_IP_MULTICAST_FASTFORWARD)
        if(mcfwdrv != NULL) {
            skb_push(skb, hlen);
            mcfw_snoop_recv(mcfwdrv, (skb->uniq_id >> 24) & 0xff, skb);
            skb_pull(skb, hlen);
        }
#       endif /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/
        netif_rx(skb);
    }
}
EXPORT_SYMBOL(cpmac_fusiv_if_rx2);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int cpmac_fusiv_ioctl(struct net_device *dev __attribute__ ((unused)),
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
EXPORT_SYMBOL(cpmac_fusiv_ioctl);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned short cpmac_fusiv_portset_to_vlan(mcfw_portset portset) {
    unsigned short vid;
    portset &= AVM_CPMAC_MAX_PORTSET;
    vid = cpmac_global.cpphy[0].mdio.switch_config.map_portset_out[portset];
    if(vid == 0) {
        DEB_ERR("[%s] The portset %#x spans more than one device!\n", __FUNCTION__, (unsigned int) portset);
    }
    return vid;
}
EXPORT_SYMBOL(cpmac_fusiv_portset_to_vlan);


/*------------------------------------------------------------------------------------------*\
 * Functions for working with different device names                                        *
\*------------------------------------------------------------------------------------------*/
char *fusiv_device_name_get(unsigned int device_number) {
    return cpmac_device_name_get(device_number);
}
EXPORT_SYMBOL(fusiv_device_name_get);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int fusiv_device_name_cmp(char *name, unsigned int device_number) {
    return cpmac_device_name_cmp(name, device_number);
}
EXPORT_SYMBOL(fusiv_device_name_cmp);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int fusiv_get_number_of_instances(void) {
    return cpmac_get_number_of_instances();
}
EXPORT_SYMBOL(fusiv_get_number_of_instances);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_fusiv_set_port_speed(unsigned char port,
                                enum _avm_event_ethernet_speed speed,
                                unsigned char fullduplex) {
    static enum _avm_event_ethernet_speed old_speed[2] = {avm_event_ethernet_speed_items, 
                                                          avm_event_ethernet_speed_items};
    unsigned char speed_bits;

    if(unlikely(port > 1)) {
        DEB_WARN("[%s] Illegal port %u given!\n", __FUNCTION__, port);
        return;
    }
    if(likely(old_speed[port] == speed)) {
        DEB_DEBUG("[%s] No speed change on port %u\n", __FUNCTION__, port);
        return;
    }
    switch(speed) {
        case avm_event_ethernet_speed_10M:
            speed_bits = 0; break;
        case avm_event_ethernet_speed_100M:
            speed_bits = 1; break;
        case avm_event_ethernet_speed_1G:
            speed_bits = 2; break;
        case avm_event_ethernet_speed_no_link:
        default:
            DEB_WARN("[%s] Illegal speed value given.\n", __FUNCTION__);
            return;
    }

    old_speed[port] = speed;
    cpmac_global.cpphy[cpmac_global.map_port_to_instance[port]].cpmac_priv->fusiv.set_port_speed(port, speed_bits, fullduplex);
}

#endif /*--- #if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) ---*/

