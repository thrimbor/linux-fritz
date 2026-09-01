/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,2007,...,2011 AVM GmbH <fritzbox_info@avm.de>
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

#include <stdarg.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <linux/avm_cpmac.h>
#include <linux/avm_event.h>
#include <linux/in.h>
#include <linux/ip.h>
#ifdef CONFIG_AVM_RTP_TIMESTAMP
#include <linux/rtp_timestamp.h>
#endif
#include "cpmac_if.h"
#include "cpphy_types.h"
#include "cpmac_debug.h"
#include "cpmac_eth.h"
#include "cpphy_mgmt.h"

/* CPMAC_TXPAUSE */
#include "cpmac_reg.h"

#if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)
#include <netpro/hostconfig.h>
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void destroy_mc_list(struct dev_mc_list *mc_list) {
    struct dev_mc_list *dmi = mc_list;
    struct dev_mc_list *next;

    while(dmi) {
        next = dmi->next;
        kfree(dmi);
        dmi = next;
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void copy_mc_list(struct dev_mc_list *mc_list,
                         struct cpmac_devinfo *devinfo) {
    struct dev_mc_list *dmi, *new_dmi;

    destroy_mc_list(devinfo->old_mc_list);
    devinfo->old_mc_list = 0;

    for(dmi = mc_list; dmi != NULL; dmi = dmi->next) {
        new_dmi = kmalloc(sizeof(*new_dmi), GFP_ATOMIC);
        if(new_dmi == NULL) {
            DEB_ERR("[copy_mc_list] cannot allocate memory. "
                    "Multicast may not work properly from now.\n");
            return;
        }

        /* Copy whole structure, then make new 'next' pointer */
        *new_dmi = *dmi;
        new_dmi->next = devinfo->old_mc_list;
        devinfo->old_mc_list = new_dmi;
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void flush_mc_list(struct net_device *dev) {
    struct cpmac_devinfo *devinfo = netdev_priv(dev);
    struct dev_mc_list *dmi = dev->mc_list;

    while(dmi) {
        DEB_TRC("%s: del %.2x:%.2x:%.2x:%.2x:%.2x:%.2x mcast address from vlan interface\n",
               dev->name,
               dmi->dmi_addr[0],
               dmi->dmi_addr[1],
               dmi->dmi_addr[2],
               dmi->dmi_addr[3],
               dmi->dmi_addr[4],
               dmi->dmi_addr[5]);
        dev_mc_delete(dev, dmi->dmi_addr, dmi->dmi_addrlen, 0);
        dmi = dev->mc_list;
    }

    /* dev->mc_list is NULL by the time we get here. */
    destroy_mc_list(devinfo->old_mc_list);
    devinfo->old_mc_list = NULL;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline int dmi_equals(struct dev_mc_list *dmi1,
                             struct dev_mc_list *dmi2) {
    unsigned int len = min(dmi1->dmi_addrlen, (unsigned char) sizeof(dmi1->dmi_addr));

    return (   (dmi1->dmi_addrlen == dmi2->dmi_addrlen)
            && (memcmp(dmi1->dmi_addr, dmi2->dmi_addr, len) == 0));
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int should_add_mc(struct dev_mc_list *dmi, struct dev_mc_list *mc_list) {
    struct dev_mc_list *idmi;

    for(idmi = mc_list; idmi != NULL; ) {
        if(dmi_equals(dmi, idmi)) {
            if(dmi->dmi_users > idmi->dmi_users)
                return 1;
            else
                return 0;
        } else {
            idmi = idmi->next;
        }
    }

    return 1;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpmaceth_set_multicast_list(struct net_device *dev) {
    struct cpmac_devinfo *devinfo = netdev_priv(dev);
    struct dev_mc_list *dmi;
    struct net_device *real_dev;
    int inc;

    real_dev = devinfo->real_dev;

    inc = dev->promiscuity - devinfo->old_promiscuity;
    if(inc) {
        DEB_INFO("%s: dev_set_promiscuity(%s, %d)\n", dev->name, real_dev->name, inc);
        dev_set_promiscuity(real_dev, inc); /* found in dev.c */
        devinfo->old_promiscuity = dev->promiscuity;
    }

    inc = dev->allmulti - devinfo->old_allmulti;
    if(inc) {
        DEB_INFO("%s: dev_set_allmulti(%s, %d)\n", dev->name, real_dev->name, inc);
        dev_set_allmulti(real_dev, inc); /* dev.c */
        devinfo->old_allmulti = dev->allmulti;
    }

    /* looking for addresses to add to master's list */
    for(dmi = dev->mc_list; dmi != NULL; dmi = dmi->next) {
        if(should_add_mc(dmi, devinfo->old_mc_list)) {
            dev_mc_add(real_dev, dmi->dmi_addr, dmi->dmi_addrlen, 0);
            DEB_TRC("%s: add %.2x:%.2x:%.2x:%.2x:%.2x:%.2x mcast address to interface %s\n",
                    dev->name,
                    dmi->dmi_addr[0],
                    dmi->dmi_addr[1],
                    dmi->dmi_addr[2],
                    dmi->dmi_addr[3],
                    dmi->dmi_addr[4],
                    dmi->dmi_addr[5],
                    real_dev->name);
        }
    }

    /* looking for addresses to delete from master's list */
    for(dmi = devinfo->old_mc_list; dmi != NULL; dmi = dmi->next) {
        if(should_add_mc(dmi, dev->mc_list)) {
            /* if we think we should add it to the new list, then we should really
             * delete it from the real list on the underlying device.
             */
            dev_mc_delete(real_dev, dmi->dmi_addr, dmi->dmi_addrlen, 0);
            DEB_TRC("%s: del %.2x:%.2x:%.2x:%.2x:%.2x:%.2x mcast address from interface %s\n",
                    dev->name,
                    dmi->dmi_addr[0],
                    dmi->dmi_addr[1],
                    dmi->dmi_addr[2],
                    dmi->dmi_addr[3],
                    dmi->dmi_addr[4],
                    dmi->dmi_addr[5],
                    real_dev->name);
        }
    }

    /* save multicast list */
    copy_mc_list(dev->mc_list, devinfo);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct net_device_stats *cpmaceth_get_stats(struct net_device *dev) {
    struct cpmac_devinfo *devinfo = netdev_priv(dev);
    return &devinfo->stats;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpmaceth_change_mtu(struct net_device *dev, int new_mtu) {
    struct cpmac_devinfo *devinfo = netdev_priv(dev);
    if(devinfo->real_dev->mtu < (unsigned int) new_mtu)
        return -ERANGE;
    dev->mtu = new_mtu;
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpmaceth_hard_start_xmit(struct sk_buff *skb, struct net_device *dev) {
    struct cpmac_devinfo *devinfo = netdev_priv(dev);
    struct net_device_stats *stats = &devinfo->stats;

#   ifdef CONFIG_AVM_RTP_TIMESTAMP
    rtp_timestamp_trace_session_from_skb_data(skb, 1);
#   endif
#   if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
    if(dev->tx_queue_len) {
        cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(devinfo->real_dev);
        if(atomic_read(&cpmac_priv->cppi->TxPrioQueues.q[0].Free) == 0) {
            if(netif_queue_stopped(devinfo->real_dev)) {
                DEB_TRC("[%s] Base queue stopped, so stop this queue, too.\n", __FUNCTION__);
                netif_stop_queue(dev);
            }
            return NETDEV_TX_BUSY;
        }
    }
#   endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/

    /* use queue 0 */
    skb->uniq_id &= 0xffffff;
    skb->uniq_id |= (CPPHY_PRIO_QUEUE_LAN << 24);

    if(   !CPMAC_VLAN_IS_802_1Q_FRAME(skb->data)
       && (devinfo->VID != 0)) {
        skb = __vlan_put_tag(skb, devinfo->VID);
        if(!skb) {
            stats->tx_dropped++;
            return NETDEV_TX_OK;
        }
    }
#   ifdef CONFIG_AVM_PA
    avm_pa_dev_snoop_transmit(AVM_PA_DEVINFO(dev), skb);
#   endif /*--- #ifdef CONFIG_AVM_PA ----*/

    stats->tx_packets++;
    stats->tx_bytes += skb->len;
    /*--- DEB_TRC("[%s] %s (%u) %*pB ... \n", __FUNCTION__, skb->dev->name, skb->len, min(24u, skb->len), skb->data); ---*/
    skb->dev = devinfo->real_dev;
    dev->trans_start = jiffies;
    return dev_queue_xmit(skb);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpmacwan_hard_start_xmit(struct sk_buff *skb, struct net_device *dev) {
    struct cpmac_devinfo *devinfo = netdev_priv(dev);
    struct net_device_stats *stats = &devinfo->stats;

#   ifdef CONFIG_AVM_RTP_TIMESTAMP
    rtp_timestamp_trace_session_from_skb_data(skb, 1);
#   endif
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8)
    if(dev->tx_queue_len) {
        cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(devinfo->real_dev);
        if(   (atomic_read(&cpmac_priv->cppi->TxPrioQueues.q[1].Free) == 0)
#       if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
           || (CPMAC_TXPAUSE(devinfo->real_dev->base_addr))) {
#       elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
           || (((cpmac_priv_t *) netdev_priv(devinfo->real_dev))->CPGMAC_F->TX_PAUSE)) {
#       else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
           ) {
#       warning "Pause handling for unknown architecture?"
#       endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
            if(netif_queue_stopped(devinfo->real_dev)) {
                DEB_TRC("[%s] Base queue stopped, so stop this queue, too.\n", __FUNCTION__);
                netif_stop_queue(dev);
            }
            return NETDEV_TX_BUSY;
        }
    }
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) ---*/

    /* use queue 1 */
    skb->uniq_id &= 0xffffff;
    skb->uniq_id |= (CPPHY_PRIO_QUEUE_WAN << 24);

    if(!CPMAC_VLAN_IS_802_1Q_FRAME(skb->data)) {
        skb = __vlan_put_tag(skb, devinfo->VID);
        if(unlikely(!skb)) {
            stats->tx_dropped++;
            return NETDEV_TX_OK;
        }
    }
    stats->tx_packets++;
    stats->tx_bytes += skb->len;
    /*--- DEB_TRC("[%s] (%u) %:24B ... \n", __FUNCTION__, skb->len, skb->data, skb->len); ---*/
    skb->dev = devinfo->real_dev;
    dev->trans_start = jiffies;
    return dev_queue_xmit(skb);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpmaceth_open(struct net_device *dev) {
    struct cpmac_devinfo *devinfo = netdev_priv(dev);

    if(!(devinfo->real_dev->flags & IFF_UP))
        return -ENETDOWN;

    DEB_INFO("[%s] %s\n", __FUNCTION__, dev->name);
    netif_carrier_off(dev);
    netif_start_queue(dev);

    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpmaceth_stop(struct net_device *dev) {
    flush_mc_list(dev);
    DEB_INFO("[%s] %s\n", __FUNCTION__, dev->name);

    if(!netif_queue_stopped(dev))
        netif_stop_queue(dev);
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpmaceth_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd) {
    struct cpmac_devinfo *devinfo = netdev_priv(dev);
    struct net_device *real_dev = devinfo->real_dev;
    struct ifreq ifrr;
    int err = -EOPNOTSUPP;

    strncpy(ifrr.ifr_name, real_dev->name, IFNAMSIZ);
    ifrr.ifr_ifru = ifr->ifr_ifru;

    switch(cmd) {
        default:
#           if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
            if(real_dev->netdev_ops && real_dev->netdev_ops->ndo_do_ioctl && netif_device_present(real_dev))
                err = real_dev->netdev_ops->ndo_do_ioctl(real_dev, &ifrr, cmd);
#           else /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
            if(real_dev->do_ioctl && netif_device_present(real_dev))
                err = real_dev->do_ioctl(real_dev, &ifrr, cmd);
#           endif /*--- #else ---*/ /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
            break;
    }

    if(!err) {
        ifr->ifr_ifru = ifrr.ifr_ifru;
    }

    return err;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmacwan_set_wan_keep_tagging(struct net_device *dev __attribute__ ((unused)),
                                   int enable __attribute__ ((unused))) {
    DEB_INFO("[%s] Not used anymore!\n", __FUNCTION__);
}
EXPORT_SYMBOL(cpmacwan_set_wan_keep_tagging);


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
static struct net_device_ops cpmac_ethops = {
        .ndo_get_stats = cpmaceth_get_stats,
        .ndo_open = cpmaceth_open,
        .ndo_stop = cpmaceth_stop,
        .ndo_change_mtu = cpmaceth_change_mtu,
        .ndo_set_multicast_list = cpmaceth_set_multicast_list,
        .ndo_do_ioctl = cpmaceth_ioctl,
        .ndo_start_xmit = cpmaceth_hard_start_xmit
};
static struct net_device_ops cpmac_wanops = {
        .ndo_get_stats = cpmaceth_get_stats,
        .ndo_open = cpmaceth_open,
        .ndo_stop = cpmaceth_stop,
        .ndo_change_mtu = cpmaceth_change_mtu,
        .ndo_set_multicast_list = cpmaceth_set_multicast_list,
        .ndo_do_ioctl = cpmaceth_ioctl,
        .ndo_start_xmit = cpmacwan_hard_start_xmit
};
#endif /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct net_device *cpmac_register_device(cpphy_mdio_t *mdio,
                                                char *name,
                                                unsigned short VID,
                                                int is_wan)
{
    struct net_device *dev, *real_dev = mdio->cpmac_priv->owner;

    dev = alloc_netdev(sizeof(struct cpmac_devinfo), name, ether_setup);
    if(dev) {
        struct cpmac_devinfo *devinfo;
        int ret;
        unsigned char len;

        devinfo = netdev_priv(dev);
        memset(devinfo, 0, sizeof(struct cpmac_devinfo));
        devinfo->pseudo_device = 1;
        devinfo->mdio = mdio;
        devinfo->real_dev = real_dev;
        devinfo->VID = VID;
        devinfo->is_wan = is_wan;
        DEB_INFOTRC("[%s] '%s' with VID %#x (%u)\n", __FUNCTION__, name, VID, VID);

        dev->flags = real_dev->flags;
        dev->flags &= ~(IFF_UP | IFF_RUNNING);
        dev->mtu = real_dev->mtu;
        dev->type = real_dev->type;
        dev->hard_header_len = real_dev->hard_header_len + VLAN_HLEN;
        len = min(real_dev->addr_len, (unsigned char) sizeof(real_dev->broadcast));
        memcpy(dev->broadcast, real_dev->broadcast, len);
        memcpy(dev->dev_addr, real_dev->dev_addr, len);
        dev->addr_len = len;
#       if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
        if(is_wan) {
            dev->netdev_ops = &cpmac_wanops;
        } else {
            dev->netdev_ops = &cpmac_ethops;
        } 
#       else /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
        /* set up method calls */
        dev->change_mtu = cpmaceth_change_mtu;
        dev->open = cpmaceth_open;
        dev->stop = cpmaceth_stop;
        dev->set_multicast_list = cpmaceth_set_multicast_list;
        dev->destructor = free_netdev;
        dev->get_stats = cpmaceth_get_stats;
        dev->do_ioctl = cpmaceth_ioctl;
        if(is_wan) {
            dev->hard_start_xmit = cpmacwan_hard_start_xmit;
        } else {
            dev->hard_start_xmit = cpmaceth_hard_start_xmit;
        }
#       endif /*--- #else ---*/ /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/

        if(is_wan) {
            dev->tx_queue_len = 32;
        } else {
            dev->tx_queue_len = 128;
        }

#       if defined(CONFIG_TI_PACKET_PROCESSOR)
        if(!is_wan) /* no vpid for wan interface */
            memcpy(&dev->vpid_block, &real_dev->vpid_block, sizeof(TI_PP_VPID));
#       endif /*--- #if defined(CONFIG_TI_PACKET_PROCESSOR) ---*/
#       ifdef CONFIG_AVM_PA
        if (is_wan)
        {
             struct avm_pa_pid_cfg cfg;
             snprintf(cfg.name, sizeof(cfg.name), "%s", name);
             cfg.framing = avm_pa_framing_ether;
             cfg.default_mtu = 1500;
             cfg.tx_func = cpmac_pa_dev_transmit;
             cfg.tx_arg = dev;
             if (avm_pa_dev_pid_register_with_ingress(AVM_PA_DEVINFO(dev), &cfg, AVM_PA_DEVINFO(real_dev)->pid_handle) < 0)
                printk(KERN_ERR "%s: failed to register PA PID\n", cfg.name);
#if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)
			 else {
				struct avm_pa_pid_hwinfo hwinfo;
				memset(&hwinfo, 0, sizeof(hwinfo));
				hwinfo.apId = MAC1_ID;
				if (avm_pa_pid_set_hwinfo(dev->avm_pa.devinfo.pid_handle, &hwinfo) < 0)
				   printk(KERN_ERR "%s: failed to set HW Info for PID %d", 
						 cfg.name, dev->avm_pa.devinfo.pid_handle);
			 }
#endif
        }
        {
             struct avm_pa_vpid_cfg cfg;
             snprintf(cfg.name, sizeof(cfg.name), "%s", name);
             cfg.v4_mtu = 1500;
             cfg.v6_mtu = 1500;
             if(avm_pa_dev_vpid_register(AVM_PA_DEVINFO(dev), &cfg) < 0)
                 printk(KERN_ERR "%s: failed to register PA VPID\n", cfg.name);
        }
#       endif /*--- #ifdef CONFIG_AVM_PA ---*/
        ret = register_netdevice(dev);
        if(ret != 0) {
            DEB_ERR("[%s] register_netdev(%s) failed rc = %d\n", __FUNCTION__, dev->name, ret);
            free_netdev(dev);
            dev = 0;
        } else {
            netif_carrier_off(dev);
        }
        cpphy_switch_device_vlan_change_notify(dev->name, VID);
    } else {
        DEB_ERR("[%s] alloc_netdev failed for '%s'!\n", __func__, name);
    }
    return dev;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_unregister_device(struct net_device *dev) {
    cpphy_switch_device_vlan_change_notify(dev->name, 0);
    unregister_netdevice(dev);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_update_device(struct net_device *dev, unsigned short VID) {
    struct cpmac_devinfo *devinfo = netdev_priv(dev);

    devinfo->VID = VID;
    DEB_INFOTRC("[%s] '%s' with VID %#x (%u)\n", __FUNCTION__, dev->name, VID, VID);
    cpphy_switch_device_vlan_change_notify(dev->name, VID);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct net_device *find_or_register_device(cpphy_mdio_t *mdio,
                                           char *name,
                                           unsigned short VID) {
    struct net_device *dev;
    unsigned int i, is_wan = (strcmp(name, "wan") == 0) ? 1 : 0;
    t_cpphy_switch_config *switch_config = &mdio->switch_config;

    if(is_wan) {
        mdio->switch_config.wanport_default_vid = VID;
    }
    for(i = 0; i < AVM_CPMAC_MAX_DEVS; i++) {
        dev = switch_config->olddevs[i];
        if(dev && strcmp(dev->name, name) == 0) {
            DEB_INFOTRC("[%s] Update '%s' with VID %#x (%u)\n", __FUNCTION__, name, VID, VID);
            cpmac_update_device(dev, VID);
            switch_config->olddevs[i] = 0;
            return dev;
        }
    }
    DEB_INFOTRC("Device configuration: Connect outgoing '%s' with VID %#x (%u)\n", name, VID, VID);
    return cpmac_register_device(mdio, name, VID, is_wan);
}

