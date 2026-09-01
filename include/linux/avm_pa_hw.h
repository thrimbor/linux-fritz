/*
 *     Definitions for AVM packet acceleration needed for 
 *     hardware support
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
#ifndef _LINUX_AVM_PA_HW_H
#define _LINUX_AVM_PA_HW_H

/* ------------------------------------------------------------------------ */

#define AVM_PA_MAX_EGRESS             4

/* ------------------------------------------------------------------------ */

#define AVM_PA_V4_MOD_SADDR         0x0001
#define AVM_PA_V4_MOD_DADDR         0x0002
#define AVM_PA_V4_MOD_ADDR          (AVM_PA_V4_MOD_SADDR|AVM_PA_V4_MOD_DADDR)
#define AVM_PA_V4_MOD_TOS           0x0004
#define AVM_PA_V4_MOD_UPDATE_TTL    0x0008
#define AVM_PA_V4_MOD_IPHDR         (AVM_PA_V4_MOD_ADDR|AVM_PA_V4_MOD_TOS)
#define AVM_PA_V4_MOD_IPHDR_CSUM    0x0010
#define AVM_PA_V4_MOD_SPORT         0x0020
#define AVM_PA_V4_MOD_DPORT         0x0040
#define AVM_PA_V4_MOD_ICMPID        0x0080
#define AVM_PA_V4_MOD_PORT          (AVM_PA_V4_MOD_SPORT|AVM_PA_V4_MOD_DPORT)
#define AVM_PA_V4_MOD_PROTOHDR_CSUM 0x0100

struct avm_pa_v4_mod_rec {
    u16 flags;
	/* 2 bytes hole */
    u32 saddr; /* saddr + daddr must have same order as in ip header */
    u32 daddr;
    u8  tos;
	/* 1 byte hole */
    u16 l3crc_update;  /* iphdr checksum */
    union {
       u16 sport;
       u16 id;
    };
    u16 dport;
    u16 l4crc_update; /* tcp|udp checksum */
    u16 l4crc_offset; /* offsetof(struct udp|tcphdr, check) */
};

struct avm_pa_mod_rec {
   u8                       hdrcopy[AVM_PA_MAX_HEADER];
   unsigned char            hdroff; /* where hdrcopy starts (alignment) */
   unsigned char            hdrlen;
   unsigned char            pull_l2_len;     /* to strip l2 header */
   unsigned char            pull_encap_len;  /* to strip tunnel header */
   unsigned char            ipversion;
   unsigned char            v6_decrease_hop_limit;
   u16                      pkttype;         /* for pa_show_session */
   struct avm_pa_v4_mod_rec v4_mod;
   unsigned char            push_encap_len;  /* to add tunnel header */
   unsigned char            push_ipversion;
   unsigned char            push_udpoffset;  /* if lisp */
   unsigned char            push_l2_len;
   u16                      protocol;
   /* 2 byte padding */
};

/* ------------------------------------------------------------------------ */

struct avm_pa_macaddr {
   struct avm_pa_macaddr *link;
   unsigned char          mac[ETH_ALEN]; 
   avm_pid_handle         pid_handle;
   /* 1 byte hole */
   unsigned long          refcount;
};

struct avm_pa_egress {
   struct avm_pa_pkt_match         match;
   avm_pid_handle                  pid_handle;
   avm_vpid_handle                 vpid_handle;
   unsigned char                   push_l2_len;
   unsigned char                   pppoe_offset;
   unsigned char                   pppoe_hdrlen; /* L2 up to PPPoE payload */
   u8                              not_local;
   u16                             mtu;
   union {
      struct avm_pa_outputinfo {
         u8                        cpmac_prio;
         u16                       tc_index;
         u32                       priority;
		 u8                        cb[48];
      } output;
      struct avm_pa_localinfo {
         struct net_device        *dev;
         struct dst_entry         *dst;
      } local;
   };
   struct avm_pa_macaddr          *destmac;
   /* statistic */
   u32                             tx_pkts;
   u32                             tcpack_pkts;
};

struct avm_pa_session_lru {
   struct avm_pa_session          *lru_head;
   struct avm_pa_session          *lru_tail;
   unsigned short                  nsessions;
   unsigned short                  maxsessions;
};

struct avm_pa_bsession {
   struct avm_pa_bsession *link;
   u32                     hash;
   struct ethhdr           ethh;
   avm_session_handle      session_handle;
};

#define AVM_PA_LRU_ACTIVE  0
#define AVM_PA_LRU_DEAD    1
#define AVM_PA_LRU_FREE    2
#define AVM_PA_LRU_MAX     3

struct avm_pa_session {
   struct avm_pa_session          *link;
   struct avm_pa_session          *lru_next;
   struct avm_pa_session          *lru_prev;
   avm_session_handle              session_handle;
   u8                              routed:1,
                                   hashed:1,
                                   is_on_lru:1,
                                   in_hw:1,
                                   realtime:1,
                                   prioack_check:1;
   /* 3 bits hole */
   u8                              lru; /* valid if is_on_lru == 1 */
   u8                              needed_headroom;
   /* 3 byte hole */
   atomic_t                        transmit_in_progress;
   atomic_t                        skb_in_tbfqueue;
   atomic_t                        skb_in_irqqueue;
   avm_pid_handle                  ingress_pid_handle;
   avm_vpid_handle                 ingress_vpid_handle;
   unsigned short                  timeout;
   unsigned long                   endtime;
   struct avm_pa_pkt_match         ingress;  /* key */
   struct avm_pa_mod_rec           mod;
   struct avm_pa_egress            egress[AVM_PA_MAX_EGRESS];
   unsigned                        negress;
   struct avm_pa_bsession         *bsession;
   void                           *hw_session; /* only used my harware pa */
   const char                     *why_killed;
#ifdef CONFIG_GENERIC_CONNTRACK
   struct generic_ct              *generic_ct;
   enum generic_ct_dir             generic_ct_dir;
#endif
};

/* ------------------------------------------------------------------------ */

#define AVM_PA_HW_FEATURE_DSLITE    0x00000001
#define AVM_PA_HW_FEATURE_6TO4      0x00000002
#define AVM_PA_HW_FEATURE_L2TP      0x00000004

#define ALLOC_VIRT_DEV_FAILED         -1
#define FREE_VIRT_DEV_FAILED          -1

void avm_pa_rx_channel_suspend(avm_pid_handle pid_handle);
void avm_pa_rx_channel_resume(avm_pid_handle pid_handle);
void avm_pa_rx_channel_packet_not_accelerated(avm_pid_handle pid_handle, struct sk_buff *skb);
void avm_pa_tx_channel_accelerated_packet(avm_pid_handle pid_handle, avm_session_handle session_handle, struct sk_buff *skb);

struct avm_pa_virt_rx_dev {
   avm_pid_handle   pid_handle;
   /* 3 bytes hole */

   /* HW Accelerator private */
   unsigned long  hw_dev_id;
   unsigned long  hw_pkt_try_to_acc;
   unsigned long  hw_pkt_try_to_acc_dropped;
   unsigned long  hw_pkt_try_to_acc_virt_chan_not_ready;
   unsigned long  hw_pkt_slow_cnt;
};

struct avm_pa_virt_tx_dev {
   avm_pid_handle   pid_handle;
   /* 3 bytes hole */

   /* HW Accelerator private */
   unsigned long    hw_dev_id;
   unsigned long    hw_pkt_tx;
   unsigned long    hw_pkt_tx_session_lookup_failed;
};

struct avm_hardware_pa {
    unsigned long features;
    int (*add_session)( struct avm_pa_session *avm_session );
    int (*change_session)( struct avm_pa_session *avm_session );
    int (*remove_session)( struct avm_pa_session *avm_session );

    /* virtual device handling */
    int  (*alloc_rx_channel)(avm_pid_handle pid_handle);
    int  (*alloc_tx_channel)(avm_pid_handle pid_handle);
    int  (*free_rx_channel)(avm_pid_handle pid_handle);
    int  (*free_tx_channel)(avm_pid_handle pid_handle);
    int  (*try_to_accelerate)(avm_pid_handle pid_handle, struct sk_buff *skb);
};

void avm_pa_register_hardware_pa( struct avm_hardware_pa *pa_functions );

void avm_pa_hardware_session_report( avm_session_handle session_id,
                                    u32 pkts, u64 bytes );

/* ------------------------------------------------------------------------ */

#endif /* _LINUX_AVM_PA_HW_H */
