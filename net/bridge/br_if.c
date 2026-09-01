/*
 *	Userspace interface
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/if_arp.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/rtnetlink.h>
#include <linux/if_ether.h>
#include <net/sock.h>
#if defined(CONFIG_FUSIV_KERNEL_AP_2_AP) || defined(CONFIG_FUSIV_KERNEL_AP_2_AP_MODULE)
#include <netpro/apload.h>
#endif /*--- #if defined(CONFIG_FUSIV_VX180) ---*/

#include "br_private.h"
#if defined(CONFIG_FUSIV_KERNEL_AP_2_AP) || defined(CONFIG_FUSIV_KERNEL_AP_2_AP_MODULE)
char (*get_apid_by_name_ptr)(char *name) = NULL;
int (*apResetBridgePorts_ptr)(struct net_device *dev) = NULL;
simResult_t (*apBridgeTable_ptr)( unsigned char, unsigned char, unsigned long) = NULL;
short (*apAddNewBridgePort_ptr)( struct net_bridge_port* p) = NULL;
#endif

#if defined(CONFIG_FUSIV_KERNEL_IGMP_SNOOP) || defined(CONFIG_FUSIV_KERNEL_IGMP_SNOOP_MODULE)
void (*br_dev_mcast_cleanup)(struct net_bridge* br, struct net_device* dev) =
                                                                NULL;
void (*br_mcast_cleanup)(struct net_bridge* br) = NULL;
#endif

#if (defined(CONFIG_FUSIV_KERNEL_PPPOE_PASSTHROUGH) && CONFIG_FUSIV_KERNEL_PPPOE_PASSTHROUGH) || (defined(CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH) && CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH)
int (*checkPPPoERelayIf_ptr)(struct net_bridge *br, struct net_device *dev) = NULL;
#endif

#if defined(CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH) && CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH
int (*check_PPPoERelay_bridge_if_ptr)(struct net_bridge *br) = NULL;
#endif

/*
 * Determine initial path cost based on speed.
 * using recommendations from 802.1d standard
 *
 * Since driver might sleep need to not be holding any locks.
 */
static int port_cost(struct net_device *dev)
{
	if (dev->ethtool_ops && dev->ethtool_ops->get_settings) {
		struct ethtool_cmd ecmd = { .cmd = ETHTOOL_GSET, };

		if (!dev->ethtool_ops->get_settings(dev, &ecmd)) {
			switch(ecmd.speed) {
			case SPEED_10000:
				return 2;
			case SPEED_1000:
				return 4;
			case SPEED_100:
				return 19;
			case SPEED_10:
				return 100;
			}
		}
	}

	/* Old silly heuristics based on name */
	if (!strncmp(dev->name, "lec", 3))
		return 7;

	if (!strncmp(dev->name, "plip", 4))
		return 2500;

	return 100;	/* assume old 10Mbps */
}


/*
 * Check for port carrier transistions.
 * Called from work queue to allow for calling functions that
 * might sleep (such as speed check), and to debounce.
 */
void br_port_carrier_check(struct net_bridge_port *p)
{
	struct net_device *dev = p->dev;
	struct net_bridge *br = p->br;

	if (netif_carrier_ok(dev))
		p->path_cost = port_cost(dev);

	if (netif_running(br->dev)) {
		spin_lock_bh(&br->lock);
		if (netif_carrier_ok(dev)) {
			if (p->state == BR_STATE_DISABLED)
				br_stp_enable_port(p);
		} else {
			if (p->state != BR_STATE_DISABLED)
				br_stp_disable_port(p);
		}
		spin_unlock_bh(&br->lock);
	}
}

static void release_nbp(struct kobject *kobj)
{
	struct net_bridge_port *p
		= container_of(kobj, struct net_bridge_port, kobj);
	kfree(p);
}

static struct kobj_type brport_ktype = {
#ifdef CONFIG_SYSFS
	.sysfs_ops = &brport_sysfs_ops,
#endif
	.release = release_nbp,
};

static void destroy_nbp(struct net_bridge_port *p)
{
	struct net_device *dev = p->dev;

	p->br = NULL;
	p->dev->br_port = NULL;
	p->dev = NULL;
	dev_put(dev);

	kobject_put(&p->kobj);
}

static void destroy_nbp_rcu(struct rcu_head *head)
{
	struct net_bridge_port *p =
			container_of(head, struct net_bridge_port, rcu);
	destroy_nbp(p);
}

/* Delete port(interface) from bridge is done in two steps.
 * via RCU. First step, marks device as down. That deletes
 * all the timers and stops new packets from flowing through.
 *
 * Final cleanup doesn't occur until after all CPU's finished
 * processing packets.
 *
 * Protected from multiple admin operations by RTNL mutex
 */
static void del_nbp(struct net_bridge_port *p)
{
	struct net_bridge *br = p->br;
	struct net_device *dev = p->dev;

#if defined(CONFIG_FUSIV_KERNEL_AP_2_AP) || defined(CONFIG_FUSIV_KERNEL_AP_2_AP_MODULE)
	char apId = -1;
#endif

#if defined(CONFIG_FUSIV_KERNEL_AP_2_AP) || defined(CONFIG_FUSIV_KERNEL_AP_2_AP_MODULE)
	if(apResetBridgePorts_ptr != NULL)
		(*apResetBridgePorts_ptr)(dev);
#endif

#ifdef CONFIG_IFX_IGMP_SNOOPING
	br_mcast_port_cleanup(p);
#endif

	sysfs_remove_link(br->ifobj, dev->name);

	dev_set_promiscuity(dev, -1);

	spin_lock_bh(&br->lock);
	br_stp_disable_port(p);
	spin_unlock_bh(&br->lock);

	br_ifinfo_notify(RTM_DELLINK, p);

	br_fdb_delete_by_port(br, p, 1);

	list_del_rcu(&p->list);

	rcu_assign_pointer(dev->br_port, NULL);

	kobject_uevent(&p->kobj, KOBJ_REMOVE);
	kobject_del(&p->kobj);

	call_rcu(&p->rcu, destroy_nbp_rcu);

#if defined(CONFIG_FUSIV_KERNEL_AP_2_AP) || defined(CONFIG_FUSIV_KERNEL_AP_2_AP_MODULE)
	if(get_apid_by_name_ptr != NULL) {
		apId = (*get_apid_by_name_ptr)(dev->name);
	} else
		printk("\nbr_del_if: ap2ap_lkm not initialized properly...\n");

	if(apId != -1) {
		if(apBridgeTable_ptr != NULL) {
			(*apBridgeTable_ptr)(apId, 0xDD, 0xff);
		} else
			printk("\nbr_del_if: ap2ap_lkm not initialized properly. ..\n");
        }
        printk("br_del_if() : is called with apId %d -> %s\n",apId,dev->name);
#endif

}

/* called with RTNL */
static void del_br(struct net_bridge *br)
{
	struct net_bridge_port *p, *n;

#if (defined(CONFIG_FUSIV_KERNEL_PPPOE_PASSTHROUGH) && CONFIG_FUSIV_KERNEL_PPPOE_PASSTHROUGH ) || (defined(CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH) && CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH)

	/* Check if any of the bridge interfaces are part of PPPoE Relay
		configuration, if yes, then return */

	if (checkPPPoERelayIf_ptr) {
		if (!checkPPPoERelayIf_ptr(br, NULL)) {
			printk("PPPoE Relay/Passthrough is configured on this bridge...." \
				"First delete PPPoE Relay/Passthrough and then delete the " \
				"bridge configuration....\r\n");
			return;
		}
	}
#endif

	list_for_each_entry_safe(p, n, &br->port_list, list) {
		del_nbp(p);
	}

	del_timer_sync(&br->gc_timer);

#if defined(CONFIG_FUSIV_KERNEL_IGMP_SNOOP) || defined(CONFIG_FUSIV_KERNEL_IGMP_SNOOP_MODULE)
        // For IGMP Snoop cleanup
	if (br->mfdb) {
		if(br_mcast_cleanup)
		br_mcast_cleanup(br);
	}
#endif

	br_sysfs_delbr(br->dev);
	unregister_netdevice(br->dev);
}

static struct net_device *new_bridge_dev(struct net *net, const char *name)
{
	struct net_bridge *br;
	struct net_device *dev;

	dev = alloc_netdev(sizeof(struct net_bridge), name,
			   br_dev_setup);

	if (!dev)
		return NULL;
	dev_net_set(dev, net);

	br = netdev_priv(dev);
	br->dev = dev;

	spin_lock_init(&br->lock);
	INIT_LIST_HEAD(&br->port_list);
	spin_lock_init(&br->hash_lock);

	br->bridge_id.prio[0] = 0x80;
	br->bridge_id.prio[1] = 0x00;

	memcpy(br->group_addr, br_group_address, ETH_ALEN);

	br->feature_mask = dev->features;
	br->stp_enabled = BR_NO_STP;
	br->designated_root = br->bridge_id;
	br->root_path_cost = 0;
	br->root_port = 0;
	br->bridge_max_age = br->max_age = 20 * HZ;
	br->bridge_hello_time = br->hello_time = 2 * HZ;
	br->bridge_forward_delay = br->forward_delay = 15 * HZ;
	br->topology_change = 0;
	br->topology_change_detected = 0;
	br->ageing_time = 300 * HZ;

	br_netfilter_rtable_init(br);

#if defined(CONFIG_FUSIV_KERNEL_IGMP_SNOOP) || defined(CONFIG_FUSIV_KERNEL_IGMP_SNOOP_MODULE)
	br->mfdb = NULL; //Init for IGMP Snoop
#endif
#if defined(CONFIG_FUSIV_KERNEL_AP_2_AP) || defined(CONFIG_FUSIV_KERNEL_AP_2_AP_MODULE)
	br->bridgeVlanAddr = 0;
#endif

	INIT_LIST_HEAD(&br->age_list);

	br_stp_timer_init(br);

	return dev;
}

/* find an available port number */
static int find_portno(struct net_bridge *br)
{
	int index;
	struct net_bridge_port *p;
	unsigned long *inuse;

	inuse = kcalloc(BITS_TO_LONGS(BR_MAX_PORTS), sizeof(unsigned long),
			GFP_KERNEL);
	if (!inuse)
		return -ENOMEM;

	set_bit(0, inuse);	/* zero is reserved */
	list_for_each_entry(p, &br->port_list, list) {
		set_bit(p->port_no, inuse);
	}
	index = find_first_zero_bit(inuse, BR_MAX_PORTS);
	kfree(inuse);

	return (index >= BR_MAX_PORTS) ? -EXFULL : index;
}

/* called with RTNL but without bridge lock */
static struct net_bridge_port *new_nbp(struct net_bridge *br,
				       struct net_device *dev)
{
	int index;
	struct net_bridge_port *p;

	index = find_portno(br);
	if (index < 0)
		return ERR_PTR(index);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return ERR_PTR(-ENOMEM);

	p->br = br;
	dev_hold(dev);
	p->dev = dev;
	p->path_cost = port_cost(dev);
	p->priority = 0x8000 >> BR_PORT_BITS;
	p->port_no = index;
	p->flags = 0;
	br_init_port(p);
#ifdef CONFIG_IFX_IGMP_SNOOPING
	br_mcast_port_init(p);
	spin_lock_init(&p->mghash_lock);
#endif
	p->state = BR_STATE_DISABLED;
	br_stp_port_timer_init(p);

	return p;
}

static struct device_type br_type = {
	.name	= "bridge",
};

int br_add_bridge(struct net *net, const char *name)
{
	struct net_device *dev;
	int ret;

	dev = new_bridge_dev(net, name);
	if (!dev)
		return -ENOMEM;

	rtnl_lock();
	if (strchr(dev->name, '%')) {
		ret = dev_alloc_name(dev, dev->name);
		if (ret < 0)
			goto out_free;
	}

	SET_NETDEV_DEVTYPE(dev, &br_type);

	ret = register_netdevice(dev);
	if (ret)
		goto out_free;

	ret = br_sysfs_addbr(dev);
	if (ret)
		unregister_netdevice(dev);
 out:
	rtnl_unlock();
	return ret;

out_free:
	free_netdev(dev);
	goto out;
}

int br_del_bridge(struct net *net, const char *name)
{
	struct net_device *dev;
	int ret = 0;

	rtnl_lock();
	dev = __dev_get_by_name(net, name);
	if (dev == NULL)
		ret =  -ENXIO; 	/* Could not find device */

	else if (!(dev->priv_flags & IFF_EBRIDGE)) {
		/* Attempt to delete non bridge device! */
		ret = -EPERM;
	}

	else if (dev->flags & IFF_UP) {
		/* Not shutdown yet. */
		ret = -EBUSY;
	}

	else
		del_br(netdev_priv(dev));

	rtnl_unlock();
	return ret;
}

/* MTU of the bridge pseudo-device: ETH_DATA_LEN or the minimum of the ports */
int br_min_mtu(const struct net_bridge *br)
{
	const struct net_bridge_port *p;
	int mtu = 0;

	ASSERT_RTNL();

	if (list_empty(&br->port_list))
		mtu = ETH_DATA_LEN;
	else {
		list_for_each_entry(p, &br->port_list, list) {
			if (!mtu  || p->dev->mtu < mtu)
				mtu = p->dev->mtu;
		}
	}
	return mtu;
}

/*
 * Recomputes features using slave's features
 */
void br_features_recompute(struct net_bridge *br)
{
	struct net_bridge_port *p;
	unsigned long features, mask;

	features = mask = br->feature_mask;
	if (list_empty(&br->port_list))
		goto done;

	features &= ~NETIF_F_ONE_FOR_ALL;

	list_for_each_entry(p, &br->port_list, list) {
		features = netdev_increment_features(features,
						     p->dev->features, mask);
	}

done:
	br->dev->features = netdev_fix_features(features, NULL);
}

/* called with RTNL */
int br_add_if(struct net_bridge *br, struct net_device *dev)
{
	struct net_bridge_port *p;
	int err = 0;
#if defined(CONFIG_FUSIV_KERNEL_AP_2_AP) || defined(CONFIG_FUSIV_KERNEL_AP_2_AP_MODULE)
	int ret = 0;
#endif

	/* Don't allow bridging non-ethernet like devices */
	if ((dev->flags & IFF_LOOPBACK) ||
	    dev->type != ARPHRD_ETHER || dev->addr_len != ETH_ALEN)
		return -EINVAL;

	/* No bridging of bridges */
	if (dev->netdev_ops->ndo_start_xmit == br_dev_xmit)
		return -ELOOP;

	/* Device is already being bridged */
	if (dev->br_port != NULL)
		return -EBUSY;

#if defined(CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH) && CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH
	/* Check if any of the bridge interfaces are part of PPPoE Relay
	   configuration, if yes, then return */
	if (check_PPPoERelay_bridge_if_ptr) {
		if (!check_PPPoERelay_bridge_if_ptr(br)) {
			printk("PPPoE Relay is configured on this bridge [%s]....\n" \
				"First delete PPPoE Relay and then add the " \
				"interface %s to bridge....\r\n",br->dev->name, dev->name);
			/* returning 0 even on a failure case, as the function
                           is getting called twice if FAILURE is returned...*/
			return 0;
           }
       }
#endif

	p = new_nbp(br, dev);
	if (IS_ERR(p))
		return PTR_ERR(p);

	err = dev_set_promiscuity(dev, 1);
	if (err)
		goto put_back;

	err = kobject_init_and_add(&p->kobj, &brport_ktype, &(dev->dev.kobj),
				   SYSFS_BRIDGE_PORT_ATTR);
	if (err)
		goto err0;

	err = br_fdb_insert(br, p, dev->dev_addr);
	if (err)
		goto err1;

	err = br_sysfs_addif(p);
	if (err)
		goto err2;

	rcu_assign_pointer(dev->br_port, p);
	dev_disable_lro(dev);

	list_add_rcu(&p->list, &br->port_list);

	spin_lock_bh(&br->lock);
	if (!br->automatic_mac_disabled) {
		br_stp_recalculate_bridge_id(br);
	} else {
		/*
		 * AVM: be sure that the bridge id is still in the fdb
		 * note the the port in the fdb-entry will not be overwritten with 0,
		 * if the entry exists
		 */
		br_fdb_insert(br, 0, br->bridge_id.addr);
	}
	br_features_recompute(br);

	if ((dev->flags & IFF_UP) && netif_carrier_ok(dev) &&
	    (br->dev->flags & IFF_UP))
		br_stp_enable_port(p);
	spin_unlock_bh(&br->lock);

	br_ifinfo_notify(RTM_NEWLINK, p);

	dev_set_mtu(br->dev, br_min_mtu(br));

	kobject_uevent(&p->kobj, KOBJ_ADD);

#if defined(CONFIG_FUSIV_KERNEL_AP_2_AP) || defined(CONFIG_FUSIV_KERNEL_AP_2_AP_MODULE)
	if(apAddNewBridgePort_ptr != NULL)
		ret = (*apAddNewBridgePort_ptr)(p);
#endif
#ifdef CONFIG_IFX_IGMP_SNOOPING
	br_ifinfo_notify(RTM_NEWLINK, p);
#endif

	/*
	 * AVM/RSP 20100408
	 * Raise needed_headroom to the maximum of all interfaces in the bridge
	 */
	if (dev->needed_headroom > br->dev->needed_headroom) 
		br->dev->needed_headroom = dev->needed_headroom;

	return 0;
err2:
	br_fdb_delete_by_port(br, p, 1);
err1:
	kobject_put(&p->kobj);
	p = NULL; /* kobject_put frees */
err0:
	dev_set_promiscuity(dev, -1);
put_back:
	dev_put(dev);
	kfree(p);
	return err;
}

/* called with RTNL */
int br_del_if(struct net_bridge *br, struct net_device *dev)
{
	struct net_bridge_port *p = dev->br_port;

#if (defined(CONFIG_FUSIV_KERNEL_PPPOE_PASSTHROUGH) && CONFIG_FUSIV_KERNEL_PPPOE_PASSTHROUGH) || (defined(CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH) && CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH)

	/* Check if any of the bridge interfaces are part of PPPoE Relay
	   configuration, if yes, then return */
	if (checkPPPoERelayIf_ptr) {
		if (!checkPPPoERelayIf_ptr(br, dev)) {
			printk("PPPoE Relay/Passthrough is configured on this interface...." \
				"First delete PPPoE Relay/Passthrough and then delete the " \
				"bridge configuration....\r\n");
			/* returning 0 even on a failure case, as the function
				is getting called twice if FAILURE is returned...*/
			return 0;
           }
       }
#endif

	if (!p || p->br != br)
		return -EINVAL;

#if defined(CONFIG_FUSIV_KERNEL_IGMP_SNOOP) || defined(CONFIG_FUSIV_KERNEL_IGMP_SNOOP_MODULE)
	// Cleaning for IGMP Snooping
	if (br->mfdb) {
		if (br_dev_mcast_cleanup)
		br_dev_mcast_cleanup(br , dev);
	} 
#endif
	del_nbp(p);

	spin_lock_bh(&br->lock);
	if (!br->automatic_mac_disabled) {
		br_stp_recalculate_bridge_id(br);
	} else {
		/*
		 * AVM: be sure that the bridge id is still in the fdb
		 * note the the port in the fdb-entry will not be overwritten with 0,
		 * if the entry exists
		 */
		br_fdb_insert(br, 0, br->bridge_id.addr);
	}
	br_features_recompute(br);
	spin_unlock_bh(&br->lock);
#ifdef CONFIG_IFX_IGMP_SNOOPING
	br_ifinfo_notify(RTM_DELLINK, p);
#endif

	return 0;
}

void br_net_exit(struct net *net)
{
	struct net_device *dev;

	rtnl_lock();
restart:
	for_each_netdev(net, dev) {
		if (dev->priv_flags & IFF_EBRIDGE) {
			del_br(netdev_priv(dev));
			goto restart;
		}
	}
	rtnl_unlock();

}

#if (defined(CONFIG_FUSIV_KERNEL_PPPOE_PASSTHROUGH) && CONFIG_FUSIV_KERNEL_PPPOE_PASSTHROUGH) || (defined(CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH) && CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH)
EXPORT_SYMBOL(checkPPPoERelayIf_ptr);
#endif

#if defined(CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH) && CONFIG_FUSIV_KERNEL_PPPOE_RELAY_FASTPATH
EXPORT_SYMBOL(check_PPPoERelay_bridge_if_ptr);
#endif

#if defined(CONFIG_FUSIV_KERNEL_IGMP_SNOOP) || defined(CONFIG_FUSIV_KERNEL_IGMP_SNOOP_MODULE)
EXPORT_SYMBOL(br_dev_mcast_cleanup);
EXPORT_SYMBOL(br_mcast_cleanup);
#endif

#if defined(CONFIG_FUSIV_KERNEL_AP_2_AP) || defined(CONFIG_FUSIV_KERNEL_AP_2_AP_MODULE)
EXPORT_SYMBOL(apResetBridgePorts_ptr);
EXPORT_SYMBOL(get_apid_by_name_ptr);
EXPORT_SYMBOL(apBridgeTable_ptr);
EXPORT_SYMBOL(apAddNewBridgePort_ptr);
#endif
