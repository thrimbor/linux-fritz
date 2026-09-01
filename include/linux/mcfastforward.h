/*
 * Multicast Fastforward from multicast source to destination ports
 *
 * Copyright (C) 2006-2009 AVM GmbH
 *
 * Interface for network drivers that will transmit fast forwarded
 * multicast streams:
 *
 * mcfw_netdriver_register():
 *  - called on network driver startup, to register netdriver_mc_transmit
 *    function and private data.
 * mcfw_netdriver_unregister():
 *  - called on network driver shutdown
 *
 * mcfw_snoop_recv() and mcfw_snoop_send():
 *  - these function will handle the IGMP snooping.
 *  - should be called for every packet send or received
 *  - the skb must contains an ethernet packet, with or without VLAN header.
 *
 * Interface for multicast stream source:
 *
 * mcfw_multicast_forward_get_id():
 *    - get id to use as paramter for mcfw_multicast_forward.
 * mcfw_multicast_forwarder_free_id()
 *    - free id got by mcfw_multicast_forward_get_id()
 *
 * mcfw_multicast_forward_ethernet():
 *  - called from cpmac (ATA-Mode) or kdsld (ADSL-Mode) for all packets
 *    received from "wire" with ethernet framing.
 *    return 0: driver should process the packet the normal way
 *    return 1: packet is consumed by function (is udp multicast).
 *              The packet will be send to all ports that want
 *              this multicast stream, using the registered
 *              netdriver_mc_transmit function.
 *    return 2: packet is consumed by function (is udp multicast).
 *              no one want this multicast stream, packet was dropped
 *
 * mcfw_multicast_forward_ip():
 *  - kdsld (ADSL-Mode) for all packets received from "wire" with
 *    ip framing (IP NLPID, IP SNAP, IP raw)
 *    same returncodes as mcfw_multicast_forward_ethernet().
 */

#ifndef __MCFORWARD_H
#define __MCFORWARD_H

#ifdef __KERNEL__

#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/in.h>

#define MCFWDRV_NAMSIZ	16

typedef unsigned long mcfw_portset;
#define MCFW_MAX_PORT	32

typedef struct _mcmemberinfo_ {
  unsigned char ethaddr[ETH_ALEN];
} mcfw_memberinfo;

static inline void mcfw_portset_reset(mcfw_portset *p)
{
   *p = 0;
}

static inline void mcfw_portset_port_add(mcfw_portset *p, int port)
{
   *p |= (1 << port); 
}

static inline void mcfw_portset_port_del(mcfw_portset *p, int port)
{
   *p &= ~(1 << port); 
}

static inline int mcfw_portset_empty(mcfw_portset *p)
{
   return *p == 0;
}

static inline int mcfw_portset_port_is_in_set(mcfw_portset *p, int port)
{
   return *p & (1 << port) ? 1 : 0; 
}

static inline void mcfw_portset_portset_add(mcfw_portset *p, mcfw_portset addset)
{
   *p |= addset;
}

static inline void mcfw_portset_portset_del(mcfw_portset *p, mcfw_portset delset)
{
   *p &= ~delset;
}

struct mcfw_netdriver {
   char name[MCFWDRV_NAMSIZ];
   void (*netdriver_mc_transmit)(void *privatedata,
								 int sourceid,
								 mcfw_portset portset,
								 struct sk_buff *skb);
   void (*netdriver_mc_transmit_single)(void *privatedata,
								        int sourceid,
								        unsigned n,
										mcfw_memberinfo *members,
								        struct sk_buff *skb);
   void *privatedata;

   /* internal */
   int has_singleport;
   int index;
};

int mcfw_netdriver_register(struct mcfw_netdriver *);
void mcfw_netdriver_unregister(struct mcfw_netdriver *);

static inline int mcfw_snoop_is_igmp(struct sk_buff *skb)
{
   struct vlan_ethhdr *hdr = (struct vlan_ethhdr *)skb->data;
   struct iphdr *iph;

   if (hdr->h_vlan_proto == __constant_ntohs(ETH_P_8021Q)) {
	  if (hdr->h_vlan_encapsulated_proto == __constant_ntohs(ETH_P_IP)) {
	     iph = (struct iphdr *)(skb->data+sizeof(struct vlan_ethhdr));
	     return iph->protocol == IPPROTO_IGMP;
	  }
   } else if (hdr->h_vlan_proto == __constant_ntohs(ETH_P_IP)) {
	  iph = (struct iphdr *)(skb->data+sizeof(struct ethhdr));
	  return iph->protocol == IPPROTO_IGMP;
   }
   return 0;
}

void _mcfw_snoop_recv(struct mcfw_netdriver *drv,
                     int port, struct sk_buff *skb);
void _mcfw_snoop_send(struct mcfw_netdriver *drv,
                     mcfw_portset portset, struct sk_buff *skb);

static inline int mcfw_snoop_recv(struct mcfw_netdriver *drv,
                                   int port, struct sk_buff *skb)
{
   if (mcfw_snoop_is_igmp(skb)) {
      _mcfw_snoop_recv(drv, port, skb);
	  return 1;
   }
   return 0;
}

static inline int mcfw_snoop_send(struct mcfw_netdriver *drv,
                                   mcfw_portset portset, struct sk_buff *skb)
{
   if (mcfw_snoop_is_igmp(skb)) {
      _mcfw_snoop_send(drv, portset, skb);
	  return 1;
   }
   return 0;
}

int  mcfw_multicast_forward_alloc_id(char *name);
void mcfw_multicast_forward_free_id(int id);

/* kdsld */

#define MCFW_VERSION	2
int mcfw_get_version(void);

int mcfw_multicast_forward_ethernet_del(int sourceid, 
                                        unsigned char mac[ETH_ALEN]);
int mcfw_multicast_forward_ethernet_add(int sourceid,
                                        unsigned char mac[ETH_ALEN],
                                        unsigned short vlanid,
									    unsigned short pppoesid,
										void *iface);

/*
 * returns:
 *   0 - not handled
 *   1 - forwarded
 *   2 - dropped
 */
int mcfw_multicast_forward_ethernet(int sourceid, struct sk_buff *skb);
int mcfw_multicast_forward_ip(int sourceid, struct sk_buff *skb);

struct mcfw_counter {
   u64 npkts;
   u64 nbytes;
};

static inline void mcfw_counter_add(struct sk_buff *skb,
                                    struct mcfw_counter *p)
{
   p->npkts++;
   p->nbytes += skb->len;
}

struct mcfw_iface_rx_statistic {
   struct mcfw_counter forwarded;
   struct mcfw_counter dropped;
};

int mcfw_multicast_get_iface_statistic(int sourceid,
                                       void *iface,
							           struct mcfw_iface_rx_statistic *stat);


struct mcfw_statistic {
   struct mcfw_counter forwarded;
   struct mcfw_counter dropped;
   struct mcfw_counter no_memory;
};

void mcfw_multicast_get_statistic(struct mcfw_statistic *p, int reset);


struct mcgroupacct {
   u32                 group;
   struct timeval      jointime;
   struct timeval      starttime;
   struct timeval      endtime;
   struct mcfw_counter forwarded;
   struct mcfw_counter dropped;
   struct mcfw_counter no_memory;
};

enum mcsourceacct_reason {
   mcsourceacct_reason_start = 0,
   mcsourceacct_reason_stop = 1,
   mcsourceacct_reason_ssrc = 2,
   mcsourceacct_reason_update = 3
};

#define MCFW_MAX_LOG2GAP	10

struct mcsourceacct {
   u32                      group;
   u32                      source;
   u32                      ssrc;
   enum mcsourceacct_reason reason;
   struct timeval           starttime;
   struct timeval           endtime;
   u32                      seqmatch;
   u32                      seqduplicate;
   u32                      seqtoolate;
   u32                      seqwrong;
   u32                      lost;
   u32                      maxlost;
   u32                      mingap;  /* minimum gap */
   u32                      maxgap;  /* maximum gap */
   u32                      sumgap;  /* sum of gaps */
   u32                      sumgap2; /* sum of gap*gap */
   u32                      log2gap[MCFW_MAX_LOG2GAP];
   struct mcfw_counter      forwarded;
};

void mcfw_register_group_acct_cb(void (*cb)(struct mcgroupacct *p));
void mcfw_register_source_acct_cb(void (*cb)(struct mcsourceacct *p));
void mcfw_report_source_acct(void);

#endif /* __KERNEL */
#endif /* __MCFORWARD_H */
