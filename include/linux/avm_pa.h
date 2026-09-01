/*
 *     Definitions for AVM packet acceleration
 *
 * Copyright (c) 2011-2013 AVM GmbH <info@avm.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed and/or modified under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _LINUX_AVM_PA_H
#define _LINUX_AVM_PA_H

#include <linux/types.h>

#ifdef __KERNEL__
struct sk_buff;
#define PKT struct sk_buff
#endif

/* ------------------------------------------------------------------------ */
#define AVM_PA_HAS_LISP_SUPPORT
/* ------------------------------------------------------------------------ */

typedef unsigned char  avm_pid_handle;     /* 1 - AVM_PA_MAX_PID */
typedef unsigned char  avm_vpid_handle;    /* 1 - AVM_PA_MAX_VPID */
typedef unsigned short avm_session_handle; /* 1 - AVM_PA_MAX_SESSION */

struct avm_pa_dev_info {
   avm_pid_handle  pid_handle;
   avm_vpid_handle vpid_handle;
};

struct avm_pa_stats {
   unsigned short nsessions;
   unsigned short nbsessions;
   unsigned short maxsessions;
   /* packet/sec */
   u32 rx_pps;
   u32 fw_pps;
   u32 overlimit_pps;
   /* in avm_pa_pid_receive */
   u32 rx_pkts;
   u32 rx_bypass;
   u32 rx_ttl;
   u32 rx_broadcast;
   u32 rx_search;
   u32 rx_match;
   u32 rx_lispchanged;
   u32 rx_df;
   u32 rx_mod;
   u32 rx_overlimit;
   u32 rx_dropped;
   u32 rx_filtered;
   u32 rx_irq;
   u32 rx_irqdropped;
   u32 rx_headroom_too_small;
   u32 rx_realloc_headroom_failed;
   u32 fw_local;
   /* in avm_pa_pid_snoop_transmit */
   u32 tx_accelerated;
   u32 tx_local;
   u32 tx_already;
   u32 tx_bypass;
   u32 tx_sess_error;
   u32 tx_sess_ok;
   u32 tx_sess_exists;
   u32 tx_egress_error;
   u32 tx_egress_ok;
   /* in avm_pa_add_local_session */
   u32 local_sess_error;
   u32 local_sess_ok;
   u32 local_sess_exists;
   /* in pa_transmit() */
   u32 fw_pkts;
   u32 fw_frags;
   u32 fw_drop;
   u32 fw_fail;
   u32 fw_frag_fail;
   u32 fw_drop_gone;
   /* in avm_pa_tbf_schedule() */
   u32 tbf_schedule;
   u32 tbf_reschedule;
   /* in session_gc() */
   u32 sess_timedout;
   u32 sess_flushed;
   u32 sess_pidchanged;
   /* rx channel */
   u32 rx_channel_no_rx_slow;
   u32 rx_channel_stopped;
   /* tx channel */
   u32 tx_channel_dropped;
   /* cputime per second */
   u32 userms;
   u32 idlems;
   u32 irqms;
};

enum avm_pa_framing {
   avm_pa_framing_ether = 1,    /* start with ethernet header */
   avm_pa_framing_ppp = 2,      /* start with ppp header */
   avm_pa_framing_ip = 3,       /* start with ipv4/ipv6 header */
   avm_pa_framing_dev = 4,      /* start after ethhdr, check skb->protocol */
   avm_pa_framing_ptype = 5,    /* packet_type */
   avm_pa_framing_llcsnap = 6,  /* llc snap (WLAN) */
};

#define AVM_PA_MAX_NAME 32

struct avm_pa_pid_cfg {
   char                   name[AVM_PA_MAX_NAME];
   enum avm_pa_framing    framing; 
   u16                    default_mtu;
   void                 (*tx_func)(void *arg, PKT *pkt);
   void                  *tx_arg;
   struct packet_type    *ptype; /* avm_pa_framing_ptype */
};

#define AVM_PA_PID_ECFG_VERSION 2
struct avm_pa_pid_ecfg {
    int           version;
	/* version 0 */
#define AVM_PA_PID_FLAG_NONE                  0x00000000
#define AVM_PA_PID_FLAG_NO_PID_CHANGED_CHECK  0x00000001
#define AVM_PA_PID_FLAG_HSTART_ON_INGRESS     0x00000002
#define AVM_PA_PID_FLAG_HSTART_ON_EGRESS      0x00000004
    unsigned long flags;
	/* version 1 */
	unsigned int  cb_start;
	unsigned int  cb_len;
	/* version 2 */
	void  (*rx_slow)(void *arg, PKT *pkt);
    void  *rx_slow_arg;
};

struct avm_pa_vpid_cfg {
   char                name[AVM_PA_MAX_NAME];
   u16                 v4_mtu;
   u16                 v6_mtu;
};

#define AVM_PA_IS_UNICAST    0 /* ((u32 *)(&stats->rx_unicast_pkt))[0] */
#define AVM_PA_IS_MULTICAST  1 /* ((u32 *)(&stats->rx_unicast_pkt))[1] */
#define AVM_PA_IS_BROADCAST  2 /* ((u32 *)(&stats->rx_unicast_pkt))[2] */

struct avm_pa_vpid_stats
{
	u32 rx_unicast_pkt;    /* ((u32 *)(&stats->rx_unicast_pkt))[0] */
	u32 rx_multicast_pkt;  /* ((u32 *)(&stats->rx_unicast_pkt))[1] */
	u32 rx_broadcast_pkt;  /* ((u32 *)(&stats->rx_unicast_pkt))[2] */
	u64 rx_bytes;
	u32 rx_discard;
	u32 tx_unicast_pkt;    /* ((u32 *)(&stats->tx_unicast_pkt))[0] */
	u32 tx_multicast_pkt;  /* ((u32 *)(&stats->tx_unicast_pkt))[1] */
	u32 tx_broadcast_pkt;  /* ((u32 *)(&stats->tx_unicast_pkt))[2] */
	u64 tx_bytes;
	u32 tx_error;
	u32 tx_discard;
};

#define AVM_PA_MAX_PRIOS       10
#define AVM_PA_HW_STATS

struct avm_pa_hw_stats {
	u64 tx_bytes;
	u32 tx_pkts;
};


int avm_pa_init(void);
void avm_pa_exit(void);

/*
 * avm_pa_get_stats()
 *    get global stats
 */
void avm_pa_get_stats(struct avm_pa_stats *stats);

/*
 * avm_pa_reset_stats()
 *    reset global stats
 */
void avm_pa_reset_stats(void);

/*
 * avm_pa_dev_init()
 *    initialize dev_info
 * should be called in
 * - net/core/dev.c: alloc_netdev_mq() just before call to setup()
 */
void avm_pa_dev_init(struct avm_pa_dev_info *devinfo);

/*
 * avm_pa_dev_pid_register_with_ingress()
 *    register a pid for dev, with an other pid that will be used on ingress
 * returns:
 *    0: pid_handle registered
 *   <0: no handle left to register pid
 */
int avm_pa_dev_pid_register_with_ingress(struct avm_pa_dev_info *devinfo,
                                         struct avm_pa_pid_cfg *cfg,
                                         avm_pid_handle ingress_pid_handle);
/*
 * avm_pa_dev_pid_register()
 *    register a pid for dev
 * returns:
 *    0: pid_handle registered
 *   <0: no handle left to register pid
 */
int avm_pa_dev_pid_register(struct avm_pa_dev_info *devinfo,
                        struct avm_pa_pid_cfg *cfg);

/*
 * avm_pa_dev_pidhandle_register_with_ingress()
 *    register a pid for dev, with an other pid that will be used on ingress
 * returns:
 *    0: pid_handle registered
 *   <0: no handle left to register pid
 */
int avm_pa_dev_pidhandle_register_with_ingress(struct avm_pa_dev_info *devinfo,
                                               avm_pid_handle pid_handle,
                                               struct avm_pa_pid_cfg *cfg,
                                               avm_pid_handle ingress_pid_handle);
/*
 * avm_pa_dev_pidhandle_register()
 *    register a pid for dev, use fixed pid_handle
 * returns:
 *    pid_handle != 0
 *       0: pid_handle registered
 *      <0: different pid_handle already registered for devinfo
 *    pid_handle == 0
 *       0: pid_handle registered
 *      <0: no handle left to register pid
 */
int avm_pa_dev_pidhandle_register(struct avm_pa_dev_info *devinfo,
								  avm_pid_handle pid_handle,
                                  struct avm_pa_pid_cfg *cfg);

/*
 * avm_pa_pid_set_ecfg()
 *    set extended config for a pid
 * returns:
 *       0: extended config set
 *      -1: pid_handle not registered
 *      -2: cb save parameter illegal
 */
int avm_pa_pid_set_ecfg(avm_pid_handle pid_handle,
                        struct avm_pa_pid_ecfg *ecfg);

/*
 * avm_pa_pid_set_framing()
 *    set framing for a pid
 * returns:
 *       0: framing set
 *      -1: pid_handle not registered
 *      -2: illegal ingress_framing
 *      -3: illegal egress_framing
 */
#define AVM_PA_PID_SET_FRAMING_IMPLEMENTED
int avm_pa_pid_set_framing(avm_pid_handle pid_handle,
                           enum avm_pa_framing ingress_framing, 
                           enum avm_pa_framing egress_framing); 
/*
 * avm_pa_dev_pid_set_hwinfo()
 *    set hw info for a registered pid for dev
 * returns:
 *     0: hw info set
 *    <0: pid not registered
 */

#ifdef CONFIG_IFX_PPA
#define AVMNET_DEVICE_IFXPPA_ETH_WAN 				 (1 << 10)
#define AVMNET_DEVICE_IFXPPA_ETH_LAN 				 (1 << 11)
#define AVMNET_DEVICE_IFXPPA_PTM_WAN 				 (1 << 12)
#define AVMNET_DEVICE_IFXPPA_ATM_WAN			     (1 << 13)
#define AVMNET_DEVICE_IFXPPA_VIRT_RX 			     (1 << 14)
#define AVMNET_DEVICE_IFXPPA_VIRT_TX 			     (1 << 15)
#endif

#ifdef CONFIG_MACH_FUSIV
#define AVM_PA_HWINFO_HAS_APID
#endif
#define AVM_PA_HWINFO_HAS_ATMVCC
struct avm_pa_pid_hwinfo {
   void *atmvcc; /* points to struct atm_vcc for ATM */
   struct avm_pa_virt_rx_dev *virt_rx_dev;
   struct avm_pa_virt_tx_dev *virt_tx_dev;
#ifdef CONFIG_IFX_PPA
   int mac_nr;
   int flags;
#endif
#ifdef CONFIG_MACH_FUSIV
   unsigned char apId;
#endif
};


/*
 * set hwinfo for pid, hwinfo will be copied
 */
int avm_pa_pid_set_hwinfo(avm_pid_handle pid_handle,
                          struct avm_pa_pid_hwinfo *hw);
struct avm_pa_pid_hwinfo *avm_pa_pid_get_hwinfo(avm_pid_handle pid_handle);

/*
 * call after all pid parameters set
 */
int avm_pa_pid_activate_hw_accelaration(avm_pid_handle pid_handle);

/*
 * avm_pa_dev_vpid_register()
 *    register a vpid for dev
 * returns:
 *    0: vpid registered
 *   <0: no handle left to register vpid
 */
int avm_pa_dev_vpid_register(struct avm_pa_dev_info *devinfo,
                             struct avm_pa_vpid_cfg *cfg);

/*
 * avm_pa_dev_vpid_register_with_tx_func()
 *    register a vpid for dev
 * returns:
 *    0: vpid registered
 *   <0: no handle left to register vpid
 */
int avm_pa_dev_vpid_register_with_tx_func(struct avm_pa_dev_info *devinfo,
                                          struct avm_pa_vpid_cfg *cfg,
                                          void (*tx_func)(void *arg, PKT *pkt),
                                          void *tx_arg);

/*
 * avm_pa_dev_vpidhandle_register()
 *    register a vpid for dev, use fixed vpid_handle
 * returns:
 *    vpid_handle != 0
 *       0: vpid_handle registered
 *      <0: different vpid_handle already registered for devinfo
 *    vpid_handle == 0
 *       0: vpid_handle registered
 *      <0: no handle left to register vpid
 */
int avm_pa_dev_vpidhandle_register(struct avm_pa_dev_info *devinfo,
								   avm_vpid_handle vpid_handle,
                                   struct avm_pa_vpid_cfg *cfg);

/*
 * avm_pa_dev_vpidhandle_register_with_tx_func()
 *    register a vpid for dev, use fixed vpid_handle
 * returns:
 *    vpid_handle != 0
 *       0: vpid_handle registered
 *      <0: different vpid_handle already registered for devinfo
 *    vpid_handle == 0
 *       0: vpid_handle registered
 *      <0: no handle left to register vpid
 */
int avm_pa_dev_vpidhandle_register_with_tx_func(
                                    struct avm_pa_dev_info *devinfo,
                                    avm_vpid_handle vpid_handle,
                                    struct avm_pa_vpid_cfg *cfg,
                                    void (*tx_func)(void *arg, PKT *pkt),
                                    void *tx_arg);

/*
 * avm_pa_dev_unregister()
 *    unregister pid and pid
 * should be called in
 * - net/core/dev.c: netdev_run_todo() just before call to dev->destructor
 */
void avm_pa_dev_unregister(struct avm_pa_dev_info *devinfo);

/*
 * avm_pa_vpid_set_ipv4_mtu
 *    set mtu for ipv4
 */
void avm_pa_dev_set_ipv4_mtu(struct avm_pa_dev_info *devinfo, u16 mtu);

/*
 * avm_pa_vpid_set_ipv6_mtu
 *    set mtu for ipv6
 */
void avm_pa_dev_set_ipv6_mtu(struct avm_pa_dev_info *devinfo, u16 mtu);

/*
 * avm_pa_dev_get_stats()
 *    get vpid stats
 * returns:
 *    < 0: vpid not registered, "stats" set to zero
 *      0: statistic copied to "stats"
 */
int avm_pa_dev_get_stats(struct avm_pa_dev_info *devinfo,
                         struct avm_pa_vpid_stats *stats);

/*
 * avm_pa_dev_reset_stats()
 *    reset vpid stats
 * returns:
 *    < 0: vpid not registered.
 *      0: statistic reseted to zero
 */
int avm_pa_dev_reset_stats(struct avm_pa_dev_info *devinfo);

/*
 * avm_pa_flush_sessions()
 *    flushes all sessions
 */
void avm_pa_flush_sessions(void);

/*
 * avm_pa_flush_lispencap_sessions()
 *    flushes lisp encap sessions
 */
void avm_pa_flush_lispencap_sessions(void);

/*
 * avm_pa_flush_sessions_for_vpid()
 *    flushes all sessions having vpid as ingress or egress
 */
void avm_pa_flush_sessions_for_vpid(avm_vpid_handle vpid_handle);

/*
 * avm_pa_flush_sessions_for_pid()
 *    flushes all sessions having pid as ingress or egress
 */
void avm_pa_flush_sessions_for_pid(avm_pid_handle pid_handle);

/* ------------------------------------------------------------------------ */
/* -------- pkt info structs and definitions ------------------------------ */
/* ------------------------------------------------------------------------ */

#define AVM_PA_MAX_HEADER 128 /* maximum bytes save from header */
#define AVM_PA_MAX_HDROFF   4 /* aligned to 4 bytes */

#define AVM_PA_ETH              0
#define AVM_PA_VLAN             1
#define AVM_PA_PPPOE            2
#define AVM_PA_PPP              3
#define AVM_PA_IPV4             4
#define AVM_PA_IPV6             5
#define AVM_PA_PORTS            6
#define AVM_PA_ICMPV4           7
#define AVM_PA_ICMPV6           8
#define AVM_PA_LLC_SNAP         9
#define AVM_PA_LISP             10
#define AVM_PA_L2TP             11
/* states */
#define AVM_PA_ETH_PROTO        12
#define AVM_PA_IP_PROTO         13

#define AVM_PA_MAX_MATCH        16

struct avm_pa_match_info {
   unsigned char type;
   unsigned char offset;
};

/*
 * IPv6 + IPv4 + UDP => IPv4 + UDP
 *        IPv4 + UDP => IPv4 + UDP
 *
 * IPV6 + UDP + LISP + IPV4 => IPV4
 * IPV6 + UDP + LISP + IPV6 => IPV6
 * IPV4 + UDP + LISP + IPV4 => IPV4
 * IPV4 + UDP + LISP + IPV6 => IPV6
 *
 * IPV6 + L2TP + ETH + IPV4 => IPV4
 * IPV6 + L2TP + ETH + IPV6 => IPV6
 * IPV4 + L2TP + ETH + IPV4 => IPV4
 * IPV4 + L2TP + ETH + IPV6 => IPV6
 */
#define AVM_PA_PKTTYPE_PROTO_MASK    0x00FF
#define AVM_PA_PKTTYPE_NONE          0x0000
#define AVM_PA_PKTTYPE_LISP          0x0100
/*                     IPBIT1        0x0200 */
/*                     IPBIT2        0x0400 */
#define AVM_PA_PKTTYPE_L2TP          0x0800
#define AVM_PA_PKTTYPE_IPV6          0x0600
#define AVM_PA_PKTTYPE_IPV4          0x0400
#define AVM_PA_PKTTYPE_IP_MASK       0x0600
#define AVM_PA_PKTTYPE_IP_VERSION(t) (((t)>>8)&0x6)
/*                     FREE          0x1000 */
/*                     IPENCAPBIT1   0x2000 */
/*                     IPENCAPBIT2   0x4000 */
/*                     FREE          0x8000 */
#define AVM_PA_PKTTYPE_IPV6ENCAP     0x6000
#define AVM_PA_PKTTYPE_IPV4ENCAP     0x4000
#define AVM_PA_PKTTYPE_IPENCAP_MASK  0x6000

#define AVM_PA_PKTTYPE_IPENCAP_VERSION(t) (((t)>>12)&0x6)
#define AVM_PA_IPENCAP_VERSION_PKTTYPE(v) ((v&0x6)<<12)

#define AVM_PA_PKTTYPE_IP2IPENCAP_VERSION(t) AVM_PA_IPENCAP_VERSION_PKTTYPE(AVM_PA_PKTTYPE_IP_VERSION(t))


#define AVM_PA_PKTTYPE_BASE_MASK     (AVM_PA_PKTTYPE_IP_MASK|AVM_PA_PKTTYPE_PROTO_MASK)

#define AVM_PA_PKTTYPE_BASE_EQ(t1,t2) ((t1) & AVM_PA_PKTTYPE_BASE_MASK) == ((t2) & AVM_PA_PKTTYPE_BASE_MASK)
#define AVM_PA_PKTTYPE_EQ(t1,t2) (t1 == t2)


#define AVM_PA_OFFSET_NOT_SET   0xff

struct avm_pa_pkt_match {
   unsigned char               casttype;   /* unicast, multicast, broadcast */
   unsigned char               fragok:1,   /* IPv4 without DF-Bit */
                               fin:1,      /* TCP with fin or rst */
                               syn:1,      /* TCP with syn */
							   ack:1,      /* TCP with ack */
							   psh:1;      /* TCP with psh */
   /* 3 bits hole */
   u16                         pkttype;
   u32                         hash;
   u8                          hdrcopy[AVM_PA_MAX_HEADER];
   struct avm_pa_match_info    match[AVM_PA_MAX_MATCH];
   unsigned char               nmatch;
   unsigned char               hdrlen;
   unsigned char               hdroff; /* where hdrcopy starts (alignment) */
   unsigned char               pppoe_offset; /* offset of pppoe header */
   unsigned char               encap_offset; /* offset of tunnel header */
   unsigned char               lisp_offset;  /* offset of lisp data header */
   unsigned char               ip_offset;    /* offset of (inner) ip header */
   /* 1 byte padding */
};

struct avm_pa_pkt_info {
   avm_pid_handle              ingress_pid_handle;
   avm_vpid_handle             ingress_vpid_handle;
   avm_vpid_handle             egress_vpid_handle;
   avm_pid_handle              ptype_pid_handle;
   avm_session_handle          session_handle;
   u8                          routed;
   u8                          use_protocol_specific;
   u8                          can_be_accelerated;
   u8                          is_accelerated;
   u8                          already_modified;
   u8                          egress_offset;
   struct avm_pa_pkt_match     match;
   unsigned int                hstart;
};

#define AVM_PA_RX_BROADCAST         5  /* is broadcast */
#define AVM_PA_RX_TTL               4  /* ttl/hoplimit is 1 */
#define AVM_PA_RX_FRAGMENT          3  /* is fragment, cannot be accelerated */
#define AVM_PA_RX_BYPASS            2  /* packet cannot be accelerated */
#define AVM_PA_RX_OK                1  /* packet maybe be accelerated */
#define AVM_PA_RX_STOLEN            0  /* packet consumed (accelerated) */
#define AVM_PA_RX_ACCELERATED	    AVM_PA_RX_STOLEN
#define AVM_PA_RX_ERROR_STATE      -1  /* state machine problem ? */
#define AVM_PA_RX_ERROR_LEN        -2  /* packet too short */
#define AVM_PA_RX_ERROR_IPVERSION  -3  /* illegal ip version */
#define AVM_PA_RX_ERROR_MATCH      -4  /* too much header */
#define AVM_PA_RX_ERROR_HDR        -5  /* too much ip header */

/*
 * avm_pa_dev_receive()
 *   if vpid set: remember ingress vpid, for statistics
 *   if pid set:  function tries to accelerate packet
 * returns:
 *   < 0: you can free the packet or send it the slow way to get it dropped
 *     0: packet was accelerated (so it is gone, don't free it)
 *   > 0: send packet the slow way
 * should be called in
 * - net/core/dev.c: netif_receive_skb() before protocol handling
 */
int avm_pa_dev_receive(struct avm_pa_dev_info *devinfo, PKT *pkt);
/*
 * avm_pa_dev_pid_receive()
 *   if pid set:  function tries to accelerate packet
 * returns:
 *   < 0: you can free the packet or send it the slow way to get it dropped
 *     0: packet was accelerated (so it is gone, don't free it)
 *   > 0: send packet the slow way
 * should be called when pid may have rx channel handling
 */
int avm_pa_dev_pid_receive(struct avm_pa_dev_info *devinfo, PKT *pkt);

/*
 * avm_pa_dev_vpid_snoop_receive()
 *   if vpid set: remember ingress vpid, for statistics
 */
void avm_pa_dev_vpid_snoop_receive(struct avm_pa_dev_info *devinfo, PKT *pkt);

/*
 * avm_pa_mark_routed()
 *   mark packet as routed
 * should be called in
 * - net/ipv4/ip_forward.c: ip_forward_finish()
 * - net/ipv6/ip6_output.c: ip6_forward_finish()
 */
void avm_pa_mark_routed(PKT *pkt);

/*
 * avm_pa_use_protocol_specific_session()
 *   if this packet creats a session, it should be
 *   a protocol specific session (no bridge session).
 * should be called when protocol specific filting
 * or protocol specific queuing is done on datapath.
 */
void avm_pa_use_protocol_specific_session(PKT *pkt);

/*
 * avm_pa_do_not_accelerate()
 *    mark packet to not create a session
 * should be called
 * - for packets manipulate by ALG (FTP-Control, tftp, ...)
 * - in net/ipv4/mcfastforward.c: mcfw_multicast_forward() (for now)
 * - in net/bridge/br_forward.c: for flooded packets
 */
void avm_pa_do_not_accelerate(PKT *pkt);


/*
 * avm_pa_set_hstart()
 *   store offset where encap header starts
 * should be called, when hstart != 0 before calling
 *   avm_pa_dev_receive()
 *   avm_pa_dev_snoop_transmit()
 */
void avm_pa_set_hstart(PKT *pkt, unsigned int hstart);

#define AVM_PA_TX_BYPASS            4  /* packet cannot be accelerated */
#define AVM_PA_TX_SESSION_EXISTS    3  /* session already exists */
#define AVM_PA_TX_EGRESS_ADDED      2  /* egress in session added */
#define AVM_PA_TX_SESSION_ADDED     1  /* session added */
#define AVM_PA_TX_OK                0  /* nothing done */
#define AVM_PA_TX_ERROR_SESSION    -1  /* session creation failed */
#define AVM_PA_TX_ERROR_EGRESS     -2  /* egress creation failed */

/*
 * avm_pa_dev_snoop_transmit()
 *   if vpid set: remember egress vpid, for statistics
 *   if pid set:  try to create a session for this packet
 * returns:
 *   > 0: some action done
 *     0: nothing done or egress vpid remembered
 *   < 0: some error
 * should be called in
 * - net/core/dev.c: dev_queue_xmit() before picking tx queue
 *   if dev has an pid set.
 */
int avm_pa_dev_snoop_transmit(struct avm_pa_dev_info *devinfo, PKT *pkt);

/*
 * avm_pa_dev_vpid_snoop_transmit()
 *   if vpid set: remember egress vpid, for statistics
 */
void avm_pa_dev_vpid_snoop_transmit(struct avm_pa_dev_info *devinfo, PKT *pkt);

/*
 * avm_pa_add_local_session()
 *   try to create a local session for this packet
 * - in include/net/inet_hashtables.h: __inet_lookup_skb()
 * - in include/net/inet6_hashtables.h: __inet6_lookup_skb()
 * - in net/ipv4/udp.c: __udp4_lib_lookup_skb()
 * - in net/ipv6/udp.c: __udp6_lib_lookup_skb()
 */
void _avm_pa_add_local_session(PKT *pkt, struct sock *sk);

void avm_pa_filter_packet(PKT *pkt);

/*
 * avm_pa_dev_get_hw_stats()
 * get upstream statistics for HW accelerated packets
 * @prio: upstream priority
 */
int avm_pa_dev_get_hw_stats(struct avm_pa_dev_info *devinfo,
                            struct avm_pa_hw_stats *stats, 
							unsigned int prio);

/*
 * avm_pa_telefon_state()
 *   notify avm pa about telephony acitivity.
 *   state == 0, no phone calls active
 *   state != 0, phone calls active or in progress
 */
void avm_pa_telefon_state(int state);

#endif /* _LINUX_AVM_PA_H */
