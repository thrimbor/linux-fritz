/*
 * L2TPv3 ethernet pseudowire driver
 *
 * Copyright (c) 2008,2009,2010 Katalix Systems Ltd
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/hash.h>
#include <linux/l2tp.h>
#include <linux/in.h>
#include <linux/etherdevice.h>
#include <linux/spinlock.h>
#include <net/sock.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/inet_common.h>
#include <net/inet_hashtables.h>
#include <net/tcp_states.h>
#include <net/protocol.h>
#include <net/xfrm.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>

#include "l2tp_core.h"

/* Default device name. May be overridden by name specified by user */
#define L2TP_ETH_DEV_NAME	"l2tpeth%d"

/* via netdev_priv() */
struct l2tp_eth {
	struct net_device	*dev;
	struct sock		*tunnel_sock;
	struct l2tp_session	*session;
	struct list_head	list;
	atomic_long_t		tx_bytes;
	atomic_long_t		tx_packets;
	atomic_long_t		tx_dropped;
	atomic_long_t		rx_bytes;
	atomic_long_t		rx_packets;
	atomic_long_t		rx_errors;
	atomic_long_t		rx_dropped;
};

/* via l2tp_session_priv() */
struct l2tp_eth_sess {
	struct net_device	*dev;
	u16                  mtu;
	u16                  mss;
};

/* per-net private data for this module */
static unsigned int l2tp_eth_net_id;
struct l2tp_eth_net {
	struct list_head l2tp_eth_dev_list;
	spinlock_t l2tp_eth_lock;
};

static inline struct l2tp_eth_net *l2tp_eth_pernet(struct net *net)
{
	return net_generic(net, l2tp_eth_net_id);
}

static int l2tp_eth_dev_init(struct net_device *dev)
{
	struct l2tp_eth *priv = netdev_priv(dev);

	priv->dev = dev;
	eth_hw_addr_random(dev);
	memset(&dev->broadcast[0], 0xff, 6);
	return 0;
}

static void l2tp_eth_dev_uninit(struct net_device *dev)
{
	struct l2tp_eth *priv = netdev_priv(dev);
	struct l2tp_eth_net *pn = l2tp_eth_pernet(dev_net(dev));

	spin_lock(&pn->l2tp_eth_lock);
	list_del_init(&priv->list);
	spin_unlock(&pn->l2tp_eth_lock);
}

static inline unsigned int
optlen(const u_int8_t *opt, unsigned int offset)
{
	/* Beware zero-length options: make finite progress */
	if (opt[offset] <= TCPOPT_NOP || opt[offset+1] == 0)
		return 1;
	else
		return opt[offset+1];
}

#define INTERNET_MAX_ENCAP	(40+8+8)

static void l2tp_eth_calc_mss(struct l2tp_session *session)
{
	struct l2tp_eth_sess *spriv = l2tp_session_priv(session);
	unsigned iphlen, udphlen;
#ifdef CONFIG_IPV6
	if (session->tunnel->sock->sk_family == PF_INET6)
		iphlen = sizeof(struct ipv6hdr);
	else
#endif
		iphlen = sizeof(struct iphdr);
	if (session->tunnel->encap == L2TP_ENCAPTYPE_UDP)
		udphlen = sizeof(struct udphdr);
	else
		udphlen = 0;
	spriv->mtu = session->mtu;
	spriv->mss = spriv->mtu
		       - INTERNET_MAX_ENCAP
	           - sizeof(struct ethhdr)
		       - iphlen
		       - udphlen
		       - session->hdr_len
		       - sizeof(struct iphdr)
		       - sizeof(struct tcphdr);
}

static
struct sk_buff *patchmss(struct l2tp_session *session, struct sk_buff *skb)
{
	struct l2tp_eth_sess *spriv;
	struct ethhdr *ethh;
	struct iphdr *iph;
	struct tcphdr *tcph;
	int tcphoff;
	__be16 newlen, oldval;
	unsigned int i, tcplen;
	u8 *opt;

	ethh = (struct ethhdr *)skb->data;
	if (ethh->h_proto != __constant_htons(ETH_P_IP))
		return skb;
	iph = (struct iphdr *)(skb->data+ETH_HLEN);
	if (iph->protocol != IPPROTO_TCP)
	   return skb;
	if (iph->frag_off & __constant_htons(IP_MF|IP_OFFSET))
	   return skb;
	tcphoff = iph->ihl << 2;
	tcph = (struct tcphdr *)(((unsigned char *)iph) + tcphoff);
	if (likely(!tcph->syn))
	   return skb;

	spriv = l2tp_session_priv(session);
    if (spriv->mtu != session->mtu)
       l2tp_eth_calc_mss(session);

	opt = (u8 *)tcph;
	for (i = sizeof(struct tcphdr); i < tcph->doff*4; i += optlen(opt, i)) {
		if (opt[i] == TCPOPT_MSS && tcph->doff*4 - i >= TCPOLEN_MSS &&
			opt[i+1] == TCPOLEN_MSS) {
			u16 oldmss;

			oldmss = (opt[i+2] << 8) | opt[i+3];

			if (oldmss <= spriv->mss)
				return skb;

			opt[i+2] = (spriv->mss & 0xff00) >> 8;
			opt[i+3] = spriv->mss & 0x00ff;

			inet_proto_csum_replace2(&tcph->check, skb,
						 htons(oldmss), htons(spriv->mss), 0);
			return skb;
		}
	}

	tcplen = skb->len - tcphoff;

	/*
	 * MSS Option not found ?! add it..
	 */
	if (skb_tailroom(skb) < TCPOLEN_MSS) {
		if (pskb_expand_head(skb, 0,
				     TCPOLEN_MSS - skb_tailroom(skb),
				     GFP_ATOMIC))
			return 0;
		tcph = (struct tcphdr *)(skb_network_header(skb) + tcphoff);
	}

	skb_put(skb, TCPOLEN_MSS);

	opt = (u_int8_t *)tcph + sizeof(struct tcphdr);
	memmove(opt + TCPOLEN_MSS, opt, tcplen - sizeof(struct tcphdr));

	inet_proto_csum_replace2(&tcph->check, skb,
				 htons(tcplen), htons(tcplen + TCPOLEN_MSS), 1);
	opt[0] = TCPOPT_MSS;
	opt[1] = TCPOLEN_MSS;
	opt[2] = (spriv->mss & 0xff00) >> 8;
	opt[3] = spriv->mss & 0x00ff;

	inet_proto_csum_replace4(&tcph->check, skb, 0, *((__be32 *)opt), 0);

	oldval = ((__be16 *)tcph)[6];
	tcph->doff += TCPOLEN_MSS/4;
	inet_proto_csum_replace2(&tcph->check, skb,
				 oldval, ((__be16 *)tcph)[6], 0);
	newlen = htons(ntohs(iph->tot_len) + TCPOLEN_MSS);
	csum_replace2(&iph->check, iph->tot_len, newlen);
	iph->tot_len = newlen;

	return skb;
}

static int l2tp_eth_dev_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct l2tp_eth *priv = netdev_priv(dev);
	struct l2tp_session *session = priv->session;
	unsigned int len = skb->len;
	int ret;

    if (likely((skb = patchmss(session, skb)) != 0))
	   ret = l2tp_xmit_skb(session, skb, session->hdr_len);
    else 
	   ret = NET_XMIT_DROP;

	if (likely(ret == NET_XMIT_SUCCESS)) {
		atomic_long_add(len, &priv->tx_bytes);
		atomic_long_inc(&priv->tx_packets);
    } else {
		atomic_long_inc(&priv->tx_dropped);
	}
	return NETDEV_TX_OK;
}

static struct net_device_stats *l2tp_eth_get_stats(struct net_device *dev)
{
	struct l2tp_eth *priv = netdev_priv(dev);

	dev->stats.tx_bytes   = atomic_long_read(&priv->tx_bytes);
	dev->stats.tx_packets = atomic_long_read(&priv->tx_packets);
	dev->stats.tx_dropped = atomic_long_read(&priv->tx_dropped);
	dev->stats.rx_bytes   = atomic_long_read(&priv->rx_bytes);
	dev->stats.rx_packets = atomic_long_read(&priv->rx_packets);
	dev->stats.rx_errors  = atomic_long_read(&priv->rx_errors);
	dev->stats.rx_dropped  = atomic_long_read(&priv->rx_dropped);
	return &dev->stats;
}


static struct net_device_ops l2tp_eth_netdev_ops = {
	.ndo_init		= l2tp_eth_dev_init,
	.ndo_uninit		= l2tp_eth_dev_uninit,
	.ndo_start_xmit	= l2tp_eth_dev_xmit,
	.ndo_get_stats	= l2tp_eth_get_stats,
};

static void l2tp_eth_dev_setup(struct net_device *dev)
{
	ether_setup(dev);
	dev->features		|= NETIF_F_LLTX;
	dev->netdev_ops		= &l2tp_eth_netdev_ops;
	dev->destructor     = free_netdev;
}

static void l2tp_eth_dev_recv(struct l2tp_session *session, struct sk_buff *skb, int data_len)
{
	struct l2tp_eth_sess *spriv = l2tp_session_priv(session);
	struct net_device *dev = spriv->dev;
	struct l2tp_eth *priv = netdev_priv(dev);

	if (session->debug & L2TP_MSG_DATA) {
		unsigned int length;

		length = min(32u, skb->len);
		if (!pskb_may_pull(skb, length))
			goto error;

		pr_debug("%s: eth recv\n", session->name);
		print_hex_dump_bytes("", DUMP_PREFIX_OFFSET, skb->data, length);
	}

    if ((dev->flags & IFF_UP) == 0)
		goto drop;

	if (!pskb_may_pull(skb, ETH_HLEN))
		goto error;

	secpath_reset(skb);

	/* checksums verified by L2TP */
	skb->ip_summed = CHECKSUM_NONE;

	skb_dst_drop(skb);
	nf_reset(skb);

    skb_orphan(skb);
	skb->dev = dev;
    skb->tstamp.tv64 = 0;
	skb->pkt_type = PACKET_HOST;
	/*
	 * overwrite input device, 2013-07-30 calle@avm.de
	 */
	skb->input_dev = skb->dev;
	skb->iif = skb->dev->ifindex;
    if (unlikely((skb = patchmss(session, skb)) == 0)) {
		atomic_long_inc(&priv->rx_errors);
		return;
    }
	skb->protocol = eth_type_trans(skb, dev);
    if (netif_rx(skb) == NET_RX_SUCCESS) {
		atomic_long_inc(&priv->rx_packets);
		atomic_long_add(data_len, &priv->rx_bytes);
	} else {
		atomic_long_inc(&priv->rx_errors);
	}
	return;

error:
	atomic_long_inc(&priv->rx_errors);
	kfree_skb(skb);
	return;

drop:
	atomic_long_inc(&priv->rx_dropped);
	kfree_skb(skb);
}

static void l2tp_eth_delete(struct l2tp_session *session)
{
	struct l2tp_eth_sess *spriv;
	struct net_device *dev;

	if (session) {
		spriv = l2tp_session_priv(session);
		dev = spriv->dev;
		if (dev) {
			unregister_netdev(dev);
			spriv->dev = NULL;
			module_put(THIS_MODULE);
		}
	}
}

#if defined(CONFIG_L2TP_DEBUGFS) || defined(CONFIG_L2TP_DEBUGFS_MODULE)
static void l2tp_eth_show(struct seq_file *m, void *arg)
{
	struct l2tp_session *session = arg;
	struct l2tp_eth_sess *spriv = l2tp_session_priv(session);
	struct net_device *dev = spriv->dev;

	seq_printf(m, "   interface %s\n", dev->name);
}
#endif


static int l2tp_eth_create(struct net *net, u32 tunnel_id, u32 session_id, u32 peer_session_id, struct l2tp_session_cfg *cfg)
{
	struct net_device *dev;
	char name[IFNAMSIZ];
	struct l2tp_tunnel *tunnel;
	struct l2tp_session *session;
	struct l2tp_eth *priv;
	struct l2tp_eth_sess *spriv;
	int rc;
	struct l2tp_eth_net *pn;

	tunnel = l2tp_tunnel_find(net, tunnel_id);
	if (!tunnel) {
		rc = -ENODEV;
		goto out;
	}

	session = l2tp_session_find(net, tunnel, session_id);
	if (session) {
		rc = -EEXIST;
		goto out;
	}

	if (cfg->ifname) {
		dev = dev_get_by_name(net, cfg->ifname);
		if (dev) {
			dev_put(dev);
			rc = -EEXIST;
			goto out;
		}
		strlcpy(name, cfg->ifname, IFNAMSIZ);
	} else
		strcpy(name, L2TP_ETH_DEV_NAME);

	session = l2tp_session_create(sizeof(*spriv), tunnel, session_id,
				      peer_session_id, cfg);
	if (!session) {
		rc = -ENOMEM;
		goto out;
	}

	dev = alloc_netdev(sizeof(*priv), name, l2tp_eth_dev_setup);
	if (!dev) {
		rc = -ENOMEM;
		goto out_del_session;
	}

	dev_net_set(dev, net);
	if (session->mtu == 0)
		session->mtu = dev->mtu - session->hdr_len;
	dev->mtu = session->mtu;
	dev->needed_headroom += session->hdr_len;

	priv = netdev_priv(dev);
	priv->dev = dev;
	priv->session = session;
	INIT_LIST_HEAD(&priv->list);

	priv->tunnel_sock = tunnel->sock;
	session->recv_skb = l2tp_eth_dev_recv;
	session->session_close = l2tp_eth_delete;
#if defined(CONFIG_L2TP_DEBUGFS) || defined(CONFIG_L2TP_DEBUGFS_MODULE)
	session->show = l2tp_eth_show;
#endif

	spriv = l2tp_session_priv(session);
	spriv->dev = dev;
    l2tp_eth_calc_mss(session);

	rc = register_netdev(dev);
	if (rc < 0)
		goto out_del_dev;

	__module_get(THIS_MODULE);
	/* Must be done after register_netdev() */
	strlcpy(session->ifname, dev->name, IFNAMSIZ);

	pn = l2tp_eth_pernet(dev_net(dev));
	spin_lock(&pn->l2tp_eth_lock);
	list_add(&priv->list, &pn->l2tp_eth_dev_list);
	spin_unlock(&pn->l2tp_eth_lock);

	return 0;

out_del_dev:
	free_netdev(dev);
	spriv->dev = NULL;
out_del_session:
	l2tp_session_delete(session);
out:
	return rc;
}

static __net_init int l2tp_eth_init_net(struct net *net)
{

	struct l2tp_eth_net *pn;
	int err;

	pn = kzalloc(sizeof(*pn), GFP_KERNEL);
	if (!pn)
		return -ENOMEM;

	err = net_assign_generic(net, l2tp_eth_net_id, pn);
	if (err)
		goto out;

	INIT_LIST_HEAD(&pn->l2tp_eth_dev_list);
	spin_lock_init(&pn->l2tp_eth_lock);

	return 0;
out:
    kfree(pn);
	return err;
}

static __net_exit void l2tp_eth_exit_net(struct net *net)
{
	struct l2tp_eth_net *pn;

	pn = net_generic(net, l2tp_eth_net_id);
	/*
	 * if someone has cached our net then
	 * further net_generic call will return NULL
	 */
	net_assign_generic(net, l2tp_eth_net_id, NULL);
	kfree(pn);
}

static struct pernet_operations l2tp_eth_net_ops = {
	.init = l2tp_eth_init_net,
	.exit = l2tp_eth_exit_net,
};


static const struct l2tp_nl_cmd_ops l2tp_eth_nl_cmd_ops = {
	.session_create	= l2tp_eth_create,
	.session_delete	= l2tp_session_delete,
};


static int __init l2tp_eth_init(void)
{
	int err = 0;

	err = l2tp_nl_register_ops(L2TP_PWTYPE_ETH, &l2tp_eth_nl_cmd_ops);
	if (err)
		goto out;

	err = register_pernet_gen_device(&l2tp_eth_net_id, &l2tp_eth_net_ops);
	if (err)
		goto out_unreg;

	pr_info("L2TP ethernet pseudowire support (L2TPv3)\n");

	return 0;

out_unreg:
	l2tp_nl_unregister_ops(L2TP_PWTYPE_ETH);
out:
	return err;
}

static void __exit l2tp_eth_exit(void)
{
	unregister_pernet_gen_device(l2tp_eth_net_id, &l2tp_eth_net_ops);
	l2tp_nl_unregister_ops(L2TP_PWTYPE_ETH);
}

module_init(l2tp_eth_init);
module_exit(l2tp_eth_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("James Chapman <jchapman@katalix.com>");
MODULE_DESCRIPTION("L2TP ethernet pseudowire driver");
MODULE_VERSION("1.0");
