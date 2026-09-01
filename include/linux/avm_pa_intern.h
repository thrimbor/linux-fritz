/*
 * Copyright (c) 2011-2012 AVM GmbH <info@avm.de>
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
#ifndef _LINUX_AVM_PA_INTERN
#define _LINUX_AVM_PA_INTERN

#include <net/ip.h>
#include <net/ipv6.h>
#include <net/icmp.h>

/* ------------------------------------------------------------------------ */

#define ipv6fraghdr frag_hdr 
#define IP6_OFFSET  0xFFF8

/* ------------------------------------------------------------------------ */

/* vlan_id:12   802.1Q 9.3.2.3 */
/* vlan_cfi:1   802.1Q 9.1 e2 (== 0, no E-RIF) */
/* vlan_prio:3  802.1Q Appendix H.2 */

struct vlanhdr {
   u16 vlan_tci;
#define VLAN_ID(p)   (ntohs((p)->vlan_tci) & 0xfff)
#define VLAN_PRIO(p) (ntohs((p)->vlan_tci) >> 13)
#define VLAN_CFI(p)  ((ntohs((p)->vlan_tci) & 0x1000) ? 1 : 0)
   u16 vlan_proto;
};

/* ------------------------------------------------------------------------ */

struct pppoehdr {
#if defined (__BIG_ENDIAN_BITFIELD)
    u8 type:4;
    u8 ver:4;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
    u8 ver:4;
    u8 type:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
    u8 code;
    u16 sid;
    u16 length;
};
#define ETH_P_PPP_SESS ETH_P_PPP_SES

/* ------------------------------------------------------------------------ */

struct llc_snap_hdr {
    /* RFC 1483 LLC/SNAP encapsulation for routed IP PDUs */
	u8  dsap;    /* Destination Service Access Point (0xAA)     */
    u8  ssap;    /* Source Service Access Point      (0xAA)     */
	u8  ui;      /* Unnumbered Information           (0x03)     */
	u8  org[3];  /* Organizational identification    (0x000000) */
	u16 type;    /* Ether type (for IP)              (0x0800)   */
};

struct l2tp_datahdr {
	u32 session_id;
	u32 default_l2_specific_sublayer;
};

/* ------------------------------------------------------------------------ */

union hdrunion {
   struct ethhdr   ethh;
   struct vlanhdr  vlanh;
   struct pppoehdr pppoeh;
   u8              ppph[1];
   struct iphdr    iph;
   struct ipv6hdr  ipv6h;
   u16             ports[2];
   struct tcphdr   tcph;
   struct udphdr   udph;
   struct icmphdr  icmph;
   struct icmp6hdr icmpv6h;
   struct llc_snap_hdr llcsnap;
   struct l2tp_datahdr l2tp;
};

typedef union hdrunion hdrunion_t;

#define LISP_DATAHDR_SIZE	8
#define L2TP_DATAHDR_SIZE	8

/* ------------------------------------------------------------------------ */

#endif /* _LINUX_AVM_PA_INTERN */
