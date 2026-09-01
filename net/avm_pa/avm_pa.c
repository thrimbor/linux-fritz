#define AVM_PA_VERSION "4.1.4 2013-11-27"
/*
 * Packet Accelerator Interface
 *
 * vim:set expandtab shiftwidth=3 softtabstop=3:
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
 *
 * PID  - pheripheral ID
 *        Identifies a low level device, may be a network driver or
 *        for ATM, every VCC has its own PID
 * VPID - virtual pheripheral ID
 *        Is assigned to a network device or a virtual network device
 *
 * Examples:
 *   ATA Mode:
 *      Name      NetDev  VirtualNetDev  PID    VPID
 *      cpmac0    yes     no             yes    yes (cpmac0)
 *      eth0      yes     no             no     yes (cpmac0)
 *      ath0      yes     no             yes    yes (ath0)
 *      internet  no      yes            no     yes (cpmac0)
 *      voip      no      yes            no     yes (cpmac0)
 *   DSL Mode (one PVCs):
 *      Name      NetDev  VirtualNetDev  PID    VPID
 *      cpmac0    yes     no             yes    yes (cpmac0)
 *      eth0      yes     no             no     yes (cpmac0)
 *      ath0      yes     no             yes    yes (ath0)
 *      vcc0      no      yes            yes    yes (vcc0)
 *      internet  no      yes            no     yes (vcc0)
 *      voip      no      yes            no     yes (vcc0)
 *   DSL Mode (two PVCs):
 *      Name      NetDev  VirtualNetDev  PID    VPID
 *      cpmac0    yes     no             yes    yes (cpmac0)
 *      eth0      yes     no             no     yes (cpmac0)
 *      ath0      yes     no             yes    yes (ath0)
 *      vcc0      no      yes            yes    yes (vcc0)
 *      vcc1      no      yes            yes    yes (vcc1)
 *      internet  no      yes            no     yes (vcc0)
 *      voip      no      yes            no     yes (vcc1)
 *   VDSL Mode:
 *      Name      NetDev  VirtualNetDev  PID    VPID
 *      cpmac0    yes     no             yes    yes (cpmac0)
 *      eth0      yes     no             no     yes (cpmac0)
 *      ath0      yes     no             yes    yes (ath0)
 *      vdsl      no      yes            yes    yes (vdsl)
 *      internet  no      yes            no     yes (vdsl)
 *      voip      no      yes            no     yes (vdsl)
 *
 *   Sessions can have four states:
 *   - FREE   : session on sess_lru[AVM_PA_LRU_FREE]
 *   - CREATE : session is on no lru
 *   - ACTIVE : session on sess_lru[AVM_PA_LRU_ACTIVE] and in hashtable
 *   - DEAD   : session on sess_lru[AVM_PA_LRU_DEAD]
 *
 *   FREE   -> pa_session_alloc()    -> CREATE
 *   CREATE -> pa_session_activate() -> ACTIVE
 *   ACTIVE -> pa_kill_session()     -> DEAD
 *   DEAD   -> pa_session_gc()       -> FREE
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/jhash.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <asm/unaligned.h>
#include <net/checksum.h>
#include <net/pkt_sched.h>
#include <linux/pkt_sched.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
#define PSCHED_TICKS2NS(x)  PSCHED_US2NS(x)
#define PSCHED_NS2TICKS(x)  PSCHED_NS2US(x)
#endif
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/avm_pa.h>
#include <linux/avm_pa_hw.h>
#include <linux/avm_pa_intern.h>
#ifdef CONFIG_AVM_POWERMETER
#include <linux/avm_power.h>
#endif
#include <linux/kallsyms.h> // sprint_symbol()
#include <linux/miscdevice.h>

/* ------------------------------------------------------------------------ */

#include <asm/cputime.h>
#include <linux/kernel_stat.h>

#ifndef cputime_to_msecs
#define cputime_to_msecs(__ct)      jiffies_to_msecs(__ct)
#endif
#ifndef msecs_to_cputime
#define msecs_to_cputime(__msecs)   msecs_to_jiffies(__msecs)
#endif

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif
#ifndef arch_idle_time
#define arch_idle_time(cpu) 0
#endif

/* ------------------------------------------------------------------------ */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
static inline struct dst_entry *skb_dst(const struct sk_buff *skb)
{
   return skb->dst;
}

static inline void skb_dst_set(struct sk_buff *skb, struct dst_entry *dst)
{
   skb->dst = dst;
}
#endif

/* ------------------------------------------------------------------------ */

#define AVM_PA_TRACE              1  /* 0: off */
#define AVM_PA_TOKSTATS           0
#define AVM_PA_UNALIGNED_CHECK    0

#define AVM_PA_AVOID_UNALIGNED    1

#if AVM_PA_AVOID_UNALIGNED
#define PA_IPHLEN(iph)          (((((u8 *)iph)[0])&0xf)<<2)
#define PA_TCP_FIN(tcph)        (((u8 *)tcph)[13]&0x01)
#define PA_TCP_SYN(tcph)        (((u8 *)tcph)[13]&0x02)
#define PA_TCP_RST(tcph)        (((u8 *)tcph)[13]&0x04)
#define PA_TCP_PSH(tcph)        (((u8 *)tcph)[13]&0x08)
#define PA_TCP_ACK(tcph)        (((u8 *)tcph)[13]&0x10)
#define PA_TCP_FIN_OR_RST(tcph) (((u8 *)tcph)[13]&0x05)
#else
#define PA_IPHLEN(iph)          ((iph)->ihl<<2)
#define PA_TCP_FIN(tcph)        ((tcph)->fin)
#define PA_TCP_SYN(tcph)        ((tcph)->syn)
#define PA_TCP_RST(tcph)        ((tcph)->rst)
#define PA_TCP_PSH(tcph)        ((tcph)->psh)
#define PA_TCP_ACK(tcph)        ((tcph)->ack)
#define PA_TCP_FIN_OR_RST(tcph) ((tcph)->fin || (tcph)->rst)
#endif
/* ------------------------------------------------------------------------ */

static inline void set_ip_checksum(struct iphdr *iph)
{
   int iphlen = PA_IPHLEN(iph);
   iph->check = 0;
   iph->check = csum_fold(csum_partial((unsigned char *)iph, iphlen, 0));
}

static inline void set_udp_checksum(struct iphdr *iph, struct udphdr *udph)
{
   unsigned short len = ntohs(udph->len);
   __wsum sum;

   udph->check = 0;
   sum = csum_partial((unsigned char *)udph, len, 0);
   udph->check = csum_tcpudp_magic(iph->saddr, iph->daddr,
                                   len, IPPROTO_UDP, sum);
   if (udph->check == 0)
      udph->check = CSUM_MANGLED_0;
}

static inline void set_udpv6_checksum(struct ipv6hdr *ipv6h,
                                      struct udphdr *udph)
{
   unsigned short len = ntohs(udph->len);
   __wsum sum;
     
   udph->check = 0;
   sum = csum_partial((unsigned char *)udph, len, 0);
   udph->check = csum_ipv6_magic(&ipv6h->saddr, &ipv6h->daddr,
                                 len, IPPROTO_UDP, sum);
   if (udph->check == 0)
      udph->check = CSUM_MANGLED_0;
}

/* ------------------------------------------------------------------------ */

static inline int rand(void)
{
   int x;
   get_random_bytes(&x, sizeof(x));
   return x;
}

#define PKT_DATA(pkt)   (pkt)->data
#define PKT_LEN(pkt)   (pkt)->len
#define PKT_PULL(pkt, len) skb_pull(pkt, len)
#define PKT_PUSH(pkt, len) skb_push(pkt, len)
#define PKT_ALLOC(len) pa_alloc_skb(len)
#define PKT_FREE(pkt)  dev_kfree_skb_any(pkt)
#define PKT_COPY(pkt)  skb_copy(pkt, GFP_ATOMIC)
#define PKT_TRIM(pkt, len)  skb_trim(pkt, len)

static inline struct sk_buff *pa_alloc_skb(unsigned len)
{
   struct sk_buff *skb;
   skb = alloc_skb(len+128+128, GFP_ATOMIC);
   if (skb) {
      skb_reserve(skb, 128);
      skb_put(skb, len);
   }
   return skb;
}

static int pa_printk(void *type, const char *format, ...)
#ifdef __GNUC__
        __attribute__ ((__format__(__printf__, 2, 3)))
#endif
;

static int pa_printk(void *type, const char *format, ...)
{
   va_list args;
   int rc;

   va_start(args, format);
   if (type) printk("%s", (char *)type);
   rc = vprintk(format, args);
   va_end(args);
   return rc;
}

/* ------------------------------------------------------------------------ */

#define constant_htons(x)   __constant_htons(x)

#undef IPPROTO_IPENCAP
#define IPPROTO_IPENCAP 4
#ifndef IPPROTO_L2TP
#define IPPROTO_L2TP    115
#endif

/*
 * Accelerating of L2TPv3 only works with
 * pseudowire ethernet or ethernet vlan
 * and default l2-specific header.
 */

/* ------------------------------------------------------------------------ */

#define AVM_PA_USE_IRQLOCK

static DEFINE_RWLOCK(avm_pa_lock);

#ifdef AVM_PA_USE_IRQLOCK
#define AVM_PA_LOCK_DECLARE unsigned long flags
#define AVM_PA_WRITE_LOCK() write_lock_irqsave(&avm_pa_lock, flags)
#define AVM_PA_READ_LOCK()  read_lock_irqsave(&avm_pa_lock, flags)
#define AVM_PA_WRITE_UNLOCK() write_unlock_irqrestore(&avm_pa_lock, flags)
#define AVM_PA_READ_UNLOCK() read_unlock_irqrestore(&avm_pa_lock, flags)
#else
#define AVM_PA_LOCK_DECLARE
#define AVM_PA_WRITE_LOCK() write_lock_bh(&avm_pa_lock)
#define AVM_PA_READ_LOCK()  read_lock_bh(&avm_pa_lock)
#define AVM_PA_WRITE_UNLOCK() write_unlock_bh(&avm_pa_lock)
#define AVM_PA_READ_UNLOCK() read_unlock_bh(&avm_pa_lock)
#endif

/* ------------------------------------------------------------------------ */

#define AVM_PA_GC_TIMEOUT               1 /* secs */
#define AVM_PA_LC_TIMEOUT               2 /* secs */
#define AVM_PA_TRAFFIC_IDLE_TBFDISABLE 10 /* secs */

/* ------------------------------------------------------------------------ */

#define AVM_PA_MAX_TBF_QUEUE_LEN     128
#define AVM_PA_MAX_IRQ_QUEUE_LEN      64

#define AVM_PA_DEFAULT_MAXRATE          5000
#define AVM_PA_MINRATE                  1000
#define AVM_PA_DEFAULT_PKTBUFFER        1024
#define AVM_PA_DEFAULT_PKTPEAK           256
#define AVM_PA_DEFAULT_TELEPHONY_REDUCE   65

#define AVM_PA_EST_DEFAULT_IDX                0 /* 0 - 5 => 0.25sec - 8sec */
#define AVM_PA_EST_DEFAULT_EWMA_LOG           3 /* 1 - 31 */
#define AVM_PA_CPUTIME_EST_DEFAULT_IDX        2 /* 0 - 5 => 0.25sec - 8sec */
#define AVM_PA_CPUTIME_EST_DEFAULT_EWMA_LOG   1 /* 1 - 31 */

#define AVM_PA_CPUTIME_IRQ_MSWIN_LOW            300 /* ms/s */
#define AVM_PA_CPUTIME_IRQ_MSWIN_HIGH           400 /* ms/s */
#define AVM_PA_CPUTIME_IDLE_MSWIN_LOW           10  /* ms/s */
#define AVM_PA_CPUTIME_IDLE_MSWIN_HIGH          50  /* ms/s */

#define AVM_PA_PRIOACK_THRESH_PKTS   40   /* wait for X packets to do the TCP-ACK check */
#define AVM_PA_PRIOACK_RATIO         70   /* % of packets have to be TCP-ACKs for positive check */
#define AVM_PA_PRIOACK_PACKET_SIZE   120  /* packet-size to recognize a packet as TCP-ACK */
#define AVM_PA_PRIOACK_PRIORITY      4    /* priority for TCP-ACK packets */

/* ------------------------------------------------------------------------ */

struct avm_pa_pid {
   struct avm_pa_pid_cfg      cfg;
   struct avm_pa_pid_ecfg     ecfg;
   avm_pid_handle             pid_handle;
   avm_pid_handle             ingress_pid_handle;
   enum avm_pa_framing        ingress_framing;
   enum avm_pa_framing        egress_framing;
   struct avm_pa_session     *hash_sess[CONFIG_AVM_PA_MAX_SESSION];
   struct avm_pa_bsession    *hash_bsess[CONFIG_AVM_PA_MAX_SESSION];
   struct avm_pa_pid_hwinfo  *hw;
   /* channel acceleration via hw */
   int                        rx_channel_activated:1,
                              tx_channel_activated:1,
                              rx_channel_stopped:1;
   /* stats */
   u32                        tx_pkts;
};

struct avm_pa_vpid {
   struct avm_pa_vpid_cfg   cfg;
   avm_vpid_handle          vpid_handle;
   struct avm_pa_vpid_stats stats;
   struct avm_pa_hw_stats   hwstats[AVM_PA_MAX_PRIOS];
};

struct avm_pa_est {
   unsigned                  idx;
   unsigned                  ewma_log;
   u32                       last_packets;
   u32                       avpps;
};

struct avm_pa_cputime_est {
   unsigned                  idx;
   unsigned                  ewma_log;
   cputime64_t               last_cputime;
   cputime_t                 avtps;
};

struct avm_pa_tbf
{
   struct hrtimer timer;
   u32            buffer;
   u32            pbuffer;
   u32            pkttime;
   long           tokens;
   long           ptokens;
   psched_time_t  t_c;
};

struct avm_pa_global {
   int                       disabled;
   int                       fw_disabled;
   int                       misc_is_open; /* means fw_disabled */
   int                       dbgcapture;
   int                       dbgsession;
   int                       dbgnosession;
   int                       dbgtrace;
   int                       dbgmatch;
   int                       dbgcputime;
   int                       dbgprioack;
   unsigned long             tcp_timeout_secs;
   unsigned long             fin_timeout_secs;
   unsigned long             udp_timeout_secs;
   unsigned long             echo_timeout_secs;
   unsigned long             bridge_timeout_secs;
   struct avm_pa_pid         pid_array[CONFIG_AVM_PA_MAX_PID];
   struct avm_pa_vpid        vpid_array[CONFIG_AVM_PA_MAX_VPID];
   struct avm_pa_session     sess_array[CONFIG_AVM_PA_MAX_SESSION];
   struct avm_pa_session_lru sess_lru[AVM_PA_LRU_MAX];
   struct avm_pa_bsession    bsess_array[CONFIG_AVM_PA_MAX_SESSION];
   struct avm_pa_macaddr     macaddr_array[CONFIG_AVM_PA_MAX_SESSION];
   struct avm_pa_macaddr    *macaddr_hash[CONFIG_AVM_PA_MAX_SESSION];
   struct avm_pa_stats       stats;

   struct timer_list         gc_timer;
   struct sk_buff_head       irqqueue;
   struct tasklet_struct     irqtasklet;

   /* packet rate estimater */
   int                       est_idx;
   int                       ewma_log;
   struct timer_list         est_timer;
   struct avm_pa_est         rx_est;
   struct avm_pa_est         fw_est;
   struct avm_pa_est         overlimit_est;
   /* cputime estimater */
   int                       cputime_est_idx;
   int                       cputime_ewma_log;
   struct timer_list         cputime_est_timer;
   struct avm_pa_cputime_est cputime_user_est;
   struct avm_pa_cputime_est cputime_idle_est;
   struct avm_pa_cputime_est cputime_irq_est;
   /* tbf for packets per second */
   int                       load_control;
#define LOADCONTROL_OFF       0x00
#define LOADCONTROL_POWER     0x01
#define LOADCONTROL_IRQ       0x02
#define LOADCONTROL_POWERIRQ  (LOADCONTROL_POWER|LOADCONTROL_IRQ)
#define LOADCONTROL_IDLE      0x04
   int                       load_reduce;
   int                       telephony_active;
   unsigned                  telephony_reduce;   
   int                       tbf_enabled;
   unsigned                  irq_mswin_low;   /* max irq ms/s */
   unsigned                  irq_mswin_high;  /* overload irq ms/s */
   unsigned                  idle_mswin_low;   /* overload idle ms/s */
   unsigned                  idle_mswin_high;  /* good idle ms/s */
   unsigned                  maxrate;     /* pkt/s at load_reduce == 0 */
   unsigned                  rate;        /* pkt/s */
   unsigned                  pktbuffer;   /* # pkts */
   unsigned                  pktpeak;     /* # pkts */
   struct avm_pa_tbf         tbf;
   struct sk_buff_head       tbfqueue;
   struct tasklet_struct     tbftasklet;
   struct task_struct       *task;
   struct timer_list         lc_timer;
   u32                       lc_overlimit; /* rx_overlimit at last tick */
#ifdef CONFIG_AVM_POWERMETER
   void                     *load_control_handle;
#endif
   /* ... */
   int                       tok_pos;
#define TOK_SAMLES  64
   int                       tok_state[TOK_SAMLES];
   unsigned                  tok_overtime[TOK_SAMLES];
   unsigned                  tok_rate[TOK_SAMLES];
   unsigned                  tok_pps[TOK_SAMLES];
   unsigned long             tok_overlimit[TOK_SAMLES];
   unsigned                  prioack_enabled;
   unsigned                  prioack_packet_size;
   unsigned                  prioack_thresh_packets;
   unsigned                  prioack_ratio;
   unsigned                  prioack_priority;
   unsigned                  prioack_acks;
   unsigned                  prioack_accl_acks;
   struct avm_hardware_pa    hardware_pa;
   int                       hw_ppa_disabled;
} pa_glob = {
   .disabled = 0,
   .fw_disabled = 0,
   .dbgcapture = 0,
   .dbgsession = 0,
   .dbgnosession = 0,
   .dbgtrace = 0,
   .dbgmatch = 0,
   .dbgcputime = 0,
   .dbgprioack = 0,
   .tcp_timeout_secs = 30,
   .fin_timeout_secs = 0,
   .udp_timeout_secs = 10,
   .echo_timeout_secs = 3,
   .bridge_timeout_secs = 30,
   .load_control = LOADCONTROL_IDLE,
   .telephony_reduce = AVM_PA_DEFAULT_TELEPHONY_REDUCE,
   .irq_mswin_low = AVM_PA_CPUTIME_IRQ_MSWIN_LOW,
   .irq_mswin_high = AVM_PA_CPUTIME_IRQ_MSWIN_HIGH,
   .idle_mswin_low = AVM_PA_CPUTIME_IDLE_MSWIN_LOW,
   .idle_mswin_high = AVM_PA_CPUTIME_IDLE_MSWIN_HIGH,
   .maxrate = AVM_PA_DEFAULT_MAXRATE,
   .rate = AVM_PA_DEFAULT_MAXRATE,
   .pktbuffer = AVM_PA_DEFAULT_PKTBUFFER,
   .pktpeak = AVM_PA_DEFAULT_PKTPEAK,
   .est_idx = AVM_PA_EST_DEFAULT_IDX,
   .ewma_log = AVM_PA_EST_DEFAULT_EWMA_LOG,
   .cputime_est_idx = AVM_PA_CPUTIME_EST_DEFAULT_IDX,
   .cputime_ewma_log = AVM_PA_CPUTIME_EST_DEFAULT_EWMA_LOG,
   .prioack_enabled = 1,
   .prioack_packet_size = AVM_PA_PRIOACK_PACKET_SIZE,
   .prioack_thresh_packets = AVM_PA_PRIOACK_THRESH_PKTS,
   .prioack_ratio = AVM_PA_PRIOACK_RATIO,
   .prioack_priority = AVM_PA_PRIOACK_PRIORITY,
   .prioack_acks = 0,
   .prioack_accl_acks = 0,
};


#define PA_PID(ctx, handle)     (&ctx->pid_array[(handle)%CONFIG_AVM_PA_MAX_PID])
#define PA_VPID(ctx, handle)    (&ctx->vpid_array[(handle)%CONFIG_AVM_PA_MAX_VPID])
#define PA_SESSION(ctx, handle) (&ctx->sess_array[(handle)%CONFIG_AVM_PA_MAX_SESSION])
#define PA_BSESSION(ctx, handle) (&ctx->bsess_array[(handle)%CONFIG_AVM_PA_MAX_SESSION])
typedef int pa_fprintf(void *, const char *, ...)
#ifdef __GNUC__
        __attribute__ ((__format__(__printf__, 2, 3)))
#endif
        ;

/* ------------------------------------------------------------------------ */

static void pa_kill_sessions_with_destmac(struct avm_pa_macaddr *destmac);
static void pa_kill_session(struct avm_pa_session *session,
                                   const char *why);
static void pa_show_session(struct avm_pa_session *session,
                            pa_fprintf fprintffunc, void *arg);
static int avm_pa_pid_receive(avm_pid_handle pid_handle, PKT *pkt);

/* ------------------------------------------------------------------------ */
/* -------- utilities ----------------------------------------------------- */
/* ------------------------------------------------------------------------ */

static const char *rc2str(int rc)
{
   switch (rc) {
      case AVM_PA_RX_BROADCAST       : return "is broadcast";
      case AVM_PA_RX_TTL             : return "ttl/hoplimit <= 1";
      case AVM_PA_RX_FRAGMENT        : return "is fragment";
      case AVM_PA_RX_BYPASS          : return "bypass";
      case AVM_PA_RX_OK              : return "ok";
      case AVM_PA_RX_ACCELERATED     : return "accelerated";
      case AVM_PA_RX_ERROR_STATE     : return "state machine problem ?";
      case AVM_PA_RX_ERROR_LEN       : return "packet too short";
      case AVM_PA_RX_ERROR_IPVERSION : return "illegal ip version";
      case AVM_PA_RX_ERROR_MATCH     : return "too much header";
      case AVM_PA_RX_ERROR_HDR       : return "too much ip header";
   }
   return "???";
}

static const char *framing2str(enum avm_pa_framing framing)
{
   switch (framing) {
     case avm_pa_framing_ether: return "ether";
     case avm_pa_framing_ppp: return "ppp";
     case avm_pa_framing_ip: return "ip";
     case avm_pa_framing_dev: return "dev";
     case avm_pa_framing_ptype: return "local";
     case avm_pa_framing_llcsnap: return "llcsnap";
   }
   return "undef";
}

static const char *in6_addr2str(const void *cp, char *buf, size_t size)
{
    const struct in6_addr *s = (const struct in6_addr *)cp;
    snprintf(buf, size, "%x:%x:%x:%x:%x:%x:%x:%x",
                    ntohs(s->s6_addr16[0]), ntohs(s->s6_addr16[1]),
                    ntohs(s->s6_addr16[2]), ntohs(s->s6_addr16[3]),
                    ntohs(s->s6_addr16[4]), ntohs(s->s6_addr16[5]),
                    ntohs(s->s6_addr16[6]), ntohs(s->s6_addr16[7]));
    return buf;
}

static const char *in_addr2str(const void *cp, char *buf, size_t size)
{
    const unsigned char *s = (const unsigned char *)cp;
    snprintf(buf, size, "%d.%d.%d.%d", s[0], s[1], s[2], s[3]);
    return buf;
}

static const char *mac2str(const void *cp, char *buf, size_t size)
{
    const unsigned char *mac = (const unsigned char *)cp;
    snprintf(buf, size, "%02X:%02X:%02X:%02X:%02X:%02X",
                        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

static const char *pkttype2str(u16 pkttype, char *buf, size_t size)
{
   char *p = buf;
   char *end = p + size;

   if (pkttype == AVM_PA_PKTTYPE_NONE) {
      snprintf(p, end-p, "none");
      return buf;
   }

   switch (pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) {
      case AVM_PA_PKTTYPE_IPV6ENCAP:
         snprintf(p, end-p, "IPv6+");
         p += strlen(p);
         break;
      case AVM_PA_PKTTYPE_IPV4ENCAP:
         snprintf(p, end-p, "IPv4+");
         p += strlen(p);
         break;
   }
   if (pkttype & AVM_PA_PKTTYPE_LISP) {
      snprintf(p, end-p, "LISP+");
      p += strlen(p);
   }
   if (pkttype & AVM_PA_PKTTYPE_L2TP) {
      snprintf(p, end-p, "L2TP+");
      p += strlen(p);
   }
   switch (pkttype & AVM_PA_PKTTYPE_IP_MASK) {
      case AVM_PA_PKTTYPE_IPV6:
         snprintf(p, end-p, "IPv6");
         p += strlen(p);
         break;
      case AVM_PA_PKTTYPE_IPV4:
         snprintf(p, end-p, "IPv4");
         p += strlen(p);
         break;
   }
   if (pkttype & AVM_PA_PKTTYPE_PROTO_MASK) {
      switch (pkttype & AVM_PA_PKTTYPE_PROTO_MASK) {
         case IPPROTO_UDP:
            snprintf(p, end-p, "+UDP");
            p += strlen(p);
            break;
         case IPPROTO_TCP:
            snprintf(p, end-p, "+TCP");
            p += strlen(p);
            break;
         case IPPROTO_ICMP:
            snprintf(p, end-p, "+ICMP");
            p += strlen(p);
            break;
         case IPPROTO_ICMPV6:
            snprintf(p, end-p, "+ICMPV6");
            p += strlen(p);
            break;
         default:
            snprintf(p, end-p, "+P%u", pkttype & AVM_PA_PKTTYPE_PROTO_MASK);
            p += strlen(p);
            break;
      }
   }
   return buf;
}

static char *data2hex(void *data, int datalen,
                      char *buf, int bufsiz)
{
   static char hexchars[] = "0123456789ABCDEF";
   unsigned char *databuf = (unsigned char *)data;
   char *s = buf;
   char *end = buf+bufsiz;
   int i;

   snprintf(s, end-s, "%d: ", datalen);
   s += strlen(s);
   
   for (i=0; i < datalen && s + 3 < end; i ++) {
      *s++ = hexchars[(databuf[i] >> 4) & 0xf];
      *s++ = hexchars[databuf[i] & 0xf];
   }
   *s = 0;
   return buf;
}

static char *pidflags2str(unsigned long flags, char *buf, int bufsiz)
{
   char *s = buf;
   char *end = s + bufsiz;
   buf[0] = 0;
   if (flags & AVM_PA_PID_FLAG_NO_PID_CHANGED_CHECK) {
      snprintf(s, end-s, "%sno_pid_changed_check", s == buf ? "" : ",");
      s += strlen(s);
   }
   if (flags & AVM_PA_PID_FLAG_HSTART_ON_INGRESS) {
      snprintf(s, end-s, "%shstart_on_ingress", s == buf ? "" : ",");
      s += strlen(s);
   }
   if (flags & AVM_PA_PID_FLAG_HSTART_ON_EGRESS) {
      snprintf(s, end-s, "%shstart_on_egress", s == buf ? "" : ",");
      s += strlen(s);
   }
   if (s == buf)
      snprintf(s, end-s, "none");
   return buf;
}

/* ------------------------------------------------------------------------ */
/* -------- parsing of packets -------------------------------------------- */
/* ------------------------------------------------------------------------ */

#define HDRCOPY(info)   ((info)->hdrcopy+(info)->hdroff)
#define LISPDATAHDR(info) (HDRCOPY(info)+(info)->lisp_offset)

static inline void pa_reset_match(struct avm_pa_pkt_match *info)
{
   info->nmatch = 0;
   info->casttype = AVM_PA_IS_UNICAST;
   info->fragok = 0;
   info->fin = 0;
   info->syn = 0;
   info->psh = 0;
   info->ack = 0;
   info->pkttype = AVM_PA_PKTTYPE_NONE;
   info->pppoe_offset = AVM_PA_OFFSET_NOT_SET;
   info->encap_offset = AVM_PA_OFFSET_NOT_SET;
   info->lisp_offset = AVM_PA_OFFSET_NOT_SET;
   info->ip_offset = AVM_PA_OFFSET_NOT_SET;
   info->hdroff = 0;
   info->hdrlen = 0;
}

static inline void pa_change_to_bridge_match(struct avm_pa_pkt_match *info)
{
   int i;
   for (i = 0; i < info->nmatch && info->match[i].type != AVM_PA_ETH; i++)
     ;
   if (i < info->nmatch)
      info->nmatch = i+1;
}

static inline int pa_add_match(struct avm_pa_pkt_match *info,
                               unsigned char offset, unsigned char type)
{
   if (info->nmatch < AVM_PA_MAX_MATCH) {
      info->match[info->nmatch].offset = offset;
      info->match[info->nmatch].type = type;
      info->nmatch++;
      return 0;
   }
   return -1;
}

static int set_pkt_match(enum avm_pa_framing framing,
                         unsigned int hstart,
                         PKT *pkt,
                         struct avm_pa_pkt_match *info,
                         int ffaspkt)
{
#define RETURN(retval) do { ret = retval; goto out; } while (0)
   int ret = AVM_PA_RX_ERROR_LEN;
   int state = 0;
   u8 *data, *p, *end;
   u32 daddr;
   u16 ethproto = 0;
   u8 ipproto = 0;
   int ttl = 0;

   data = PKT_DATA(pkt);
   end = data + PKT_LEN(pkt);
   data += hstart;

   switch (framing) {
      case avm_pa_framing_ip:
         if ((data[0] & 0xf0) == 0x40 && (data[0] & 0x0f) >= 5) {
            state = AVM_PA_IPV4;
            break;
         }
         if ((data[0] & 0xf0) == 0x60) {
            state = AVM_PA_IPV6;
            break;
         }
         return AVM_PA_RX_ERROR_IPVERSION;
      case avm_pa_framing_ppp:
         state = AVM_PA_PPP;
         break;
      case avm_pa_framing_ether:
         state = AVM_PA_ETH;
         break;
      case avm_pa_framing_dev:
         state = AVM_PA_ETH;
         data = (u8 *)eth_hdr(pkt);
         break;
      case avm_pa_framing_ptype:
         data = (u8 *)skb_network_header(pkt);
         if (pkt->protocol == constant_htons(ETH_P_IP)) {
            state = AVM_PA_IPV4;
         } else if (pkt->protocol == constant_htons(ETH_P_IPV6)) {
            state = AVM_PA_IPV6;
         } else {
            return AVM_PA_RX_BYPASS;
         }
         break;
      case avm_pa_framing_llcsnap:
         state = AVM_PA_LLC_SNAP;
         break;

   }
   if (end - data > AVM_PA_MAX_HEADER - AVM_PA_MAX_HDROFF)
      end = data + AVM_PA_MAX_HEADER - AVM_PA_MAX_HDROFF;
   p = data;

   while (p < end) {
      hdrunion_t *hdr = (hdrunion_t *)p;
      int offset = p-data;
      switch (state) {
         case AVM_PA_ETH:
            if (pa_add_match(info, offset, AVM_PA_ETH) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            p += sizeof(struct ethhdr);
            state = AVM_PA_ETH_PROTO;
            ethproto = hdr->ethh.h_proto;
            continue;

         case AVM_PA_VLAN:
            if (pa_add_match(info, offset, AVM_PA_VLAN) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            p += sizeof(struct vlanhdr);
            state = AVM_PA_ETH_PROTO;
            ethproto = hdr->vlanh.vlan_proto;
            if (hdr->ethh.h_dest[0] & 1) {
               if (hdr->ethh.h_dest[0] == 0xff) {
                  info->casttype = AVM_PA_IS_BROADCAST;
                  RETURN(AVM_PA_RX_BYPASS);
               } else {
                  info->casttype = AVM_PA_IS_MULTICAST;
               }
            }
            continue;

         case AVM_PA_ETH_PROTO:
            switch (ethproto) {
               case constant_htons(ETH_P_PPP_SESS):
                  state = AVM_PA_PPPOE;
                  continue;
               case constant_htons(ETH_P_IP):
                  state = AVM_PA_IPV4;
                  continue;
               case constant_htons(ETH_P_IPV6):
                  state = AVM_PA_IPV6;
                  continue;
               case constant_htons(ETH_P_8021Q):
                  state = AVM_PA_VLAN;
                  continue;
            }
            RETURN(AVM_PA_RX_BYPASS);

         case AVM_PA_PPPOE:
            if (pa_add_match(info, offset, AVM_PA_PPPOE) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            p += sizeof(struct pppoehdr);
            info->pppoe_offset = offset;
            state = AVM_PA_PPP;
            continue;

         case AVM_PA_PPP:
            if (p[0] == 0) {
               p++;
               offset++;
            }
            if (p[0] == 0x21) {
               if (pa_add_match(info, offset, AVM_PA_PPP) < 0)
                  RETURN(AVM_PA_RX_ERROR_MATCH);
               p++;
               state = AVM_PA_IPV4;
               continue;
            }
            if (p[0] == 0x57) {
               if (pa_add_match(info, offset, AVM_PA_PPP) < 0)
                  RETURN(AVM_PA_RX_ERROR_MATCH);
               p++;
               state = AVM_PA_IPV6;
               continue;
            }
            RETURN(AVM_PA_RX_BYPASS);

         case AVM_PA_IPV4:
            if (hdr->iph.version != 4)
               RETURN(AVM_PA_RX_ERROR_IPVERSION);
            if (pa_add_match(info, offset, AVM_PA_IPV4) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            ttl = hdr->iph.ttl;
            p += PA_IPHLEN(&hdr->iph);
            if (hdr->iph.frag_off & constant_htons(IP_OFFSET))
               RETURN(AVM_PA_RX_FRAGMENT);
            if ((hdr->iph.frag_off & constant_htons(IP_MF)) && !ffaspkt)
               RETURN(AVM_PA_RX_FRAGMENT);
            daddr = get_unaligned(&hdr->iph.daddr);
            if (ipv4_is_lbcast(daddr)) {
               info->casttype = AVM_PA_IS_BROADCAST;
               RETURN(AVM_PA_RX_BYPASS);
            } else if (ipv4_is_multicast(daddr))  {
               info->casttype = AVM_PA_IS_MULTICAST;
            }
            if ((hdr->iph.frag_off & constant_htons(IP_DF)) == 0)
               info->fragok = 1;
            if (hdr->iph.protocol == IPPROTO_IPV6) {
               if (info->pkttype != AVM_PA_PKTTYPE_NONE) 
                  RETURN(AVM_PA_RX_ERROR_HDR);
               info->pkttype |= AVM_PA_PKTTYPE_IPV4ENCAP;
               info->encap_offset = offset;
               state = AVM_PA_IPV6;
               continue;
            }
            if (hdr->iph.protocol == IPPROTO_IPENCAP) {
               if (info->pkttype != AVM_PA_PKTTYPE_NONE) 
                  RETURN(AVM_PA_RX_ERROR_HDR);
               info->pkttype |= AVM_PA_PKTTYPE_IPV4ENCAP;
               info->encap_offset = offset;
               state = AVM_PA_IPV4;
               continue;
            }
            info->pkttype |= AVM_PA_PKTTYPE_IPV4;
            info->ip_offset = offset;
            state = AVM_PA_IP_PROTO;
            ipproto = hdr->iph.protocol;
            if ((offset & 0x3) && info->hdroff == 0)
               info->hdroff = 4 - (offset & 0x3);
            continue;

         case AVM_PA_IPV6:
            if (hdr->ipv6h.version != 6)
               RETURN(AVM_PA_RX_ERROR_IPVERSION);
            if (pa_add_match(info, offset, AVM_PA_IPV6) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            ttl = hdr->ipv6h.hop_limit;
            p += sizeof(struct ipv6hdr);
            if (hdr->ipv6h.daddr.s6_addr[0] == 0xff)
               info->casttype = AVM_PA_IS_MULTICAST;
            if (hdr->ipv6h.nexthdr == IPPROTO_IPV6) {
               if (info->pkttype != AVM_PA_PKTTYPE_NONE) 
                  RETURN(AVM_PA_RX_ERROR_HDR);
               info->pkttype |= AVM_PA_PKTTYPE_IPV6ENCAP;
               info->encap_offset = offset;
               state = AVM_PA_IPV6;
               continue;
            }
            if (hdr->ipv6h.nexthdr == IPPROTO_IPENCAP) {
               if (info->pkttype != AVM_PA_PKTTYPE_NONE) 
                  RETURN(AVM_PA_RX_ERROR_HDR);
               info->pkttype |= AVM_PA_PKTTYPE_IPV6ENCAP;
               info->encap_offset = offset;
               state = AVM_PA_IPV4;
               continue;
            }
            if (hdr->ipv6h.nexthdr == IPPROTO_FRAGMENT) {
               struct ipv6fraghdr *fragh = (struct ipv6fraghdr *)p;
               info->pkttype |= AVM_PA_PKTTYPE_IPV6;
               info->ip_offset = offset;
               if (fragh->frag_off & constant_htons(IP6_OFFSET)) 
                  RETURN(AVM_PA_RX_FRAGMENT);
               if ((fragh->frag_off & constant_htons(IP6_MF)) && !ffaspkt)
                  RETURN(AVM_PA_RX_FRAGMENT);
               p += sizeof(struct ipv6fraghdr);
               state = AVM_PA_IP_PROTO;
               ipproto = fragh->nexthdr;
            }
            info->pkttype |= AVM_PA_PKTTYPE_IPV6;
            info->ip_offset = offset;
            state = AVM_PA_IP_PROTO;
            ipproto = hdr->ipv6h.nexthdr;
            if ((offset & 0x3) && info->hdroff == 0)
               info->hdroff = 4 - (offset & 0x3);
            continue;

         case AVM_PA_IP_PROTO:
            switch (ipproto) {
               case IPPROTO_TCP:
                  if (PA_TCP_FIN_OR_RST(&hdr->tcph)) 
                     info->fin = 1;
                  if (PA_TCP_SYN(&hdr->tcph))
                     info->syn = 1;
                  if (PA_TCP_ACK(&hdr->tcph))
                     info->ack = 1;
                  if (PA_TCP_PSH(&hdr->tcph))
                     info->psh = 1;
                  /* no break */
               case IPPROTO_UDP:
                  info->pkttype |= ipproto;
                  if (p + 2*sizeof(u16) > end)
                     RETURN(AVM_PA_RX_ERROR_LEN);
                  if (pa_add_match(info, offset, AVM_PA_PORTS) < 0)
                     RETURN(AVM_PA_RX_ERROR_MATCH);
                  if (hdr->udph.dest == constant_htons(4341)) {
                     if (p + sizeof(struct udphdr) > end)
                        RETURN(AVM_PA_RX_ERROR_LEN);
                     p += sizeof(struct udphdr);
                     state = AVM_PA_LISP;
                     continue;
                  }
                  p += 2*sizeof(u16);
                  RETURN(AVM_PA_RX_OK);
               case IPPROTO_ICMP:
                  info->pkttype |= ipproto;
                  if (p + sizeof(struct icmphdr) > end)
                     RETURN(AVM_PA_RX_ERROR_LEN);
                  if (   hdr->icmph.type == ICMP_ECHO
                      || hdr->icmph.type == ICMP_ECHOREPLY) {
                     if (pa_add_match(info, offset, AVM_PA_ICMPV4) < 0)
                        RETURN(AVM_PA_RX_ERROR_MATCH);
                     p += sizeof(struct icmphdr);
                     RETURN(AVM_PA_RX_OK);
                  }
                  break;
               case IPPROTO_ICMPV6:
                  info->pkttype |= ipproto;
                  if (p + sizeof(struct icmp6hdr) > end)
                     RETURN(AVM_PA_RX_ERROR_LEN);
                  if (   hdr->icmpv6h.icmp6_type == ICMPV6_ECHO_REQUEST
                      || hdr->icmpv6h.icmp6_type == ICMPV6_ECHO_REPLY) {
                     if (pa_add_match(info, offset, AVM_PA_ICMPV6) < 0)
                        RETURN(AVM_PA_RX_ERROR_MATCH);
                     p += sizeof(struct icmp6hdr);
                     RETURN(AVM_PA_RX_OK);
                  }
                  break;
               case IPPROTO_L2TP:
                  if (AVM_PA_PKTTYPE_IPENCAP_VERSION(info->pkttype))
                    RETURN(AVM_PA_RX_OK);
                  if (p + L2TP_DATAHDR_SIZE > end)
                     RETURN(AVM_PA_RX_ERROR_LEN);
                  info->encap_offset = info->ip_offset;
                  p += L2TP_DATAHDR_SIZE;
                  if (pa_add_match(info, offset, AVM_PA_L2TP) < 0)
                     RETURN(AVM_PA_RX_ERROR_MATCH);
                  info->pkttype = AVM_PA_PKTTYPE_IP2IPENCAP_VERSION(info->pkttype);
                  info->pkttype |= AVM_PA_PKTTYPE_L2TP;
                  state = AVM_PA_ETH;
                  continue;
            }
            RETURN(AVM_PA_RX_BYPASS);

         case AVM_PA_LLC_SNAP:
            if (   hdr->llcsnap.dsap  != 0xAA
                || hdr->llcsnap.ssap  != 0xAA
                || hdr->llcsnap.ui    != 0x03)
               /* not checking:
                * RFC1042_SNAP 0x00,0x00,0x00
                * BTEP_SNAP    0x00,0x00,0xf8
                */
               RETURN(AVM_PA_RX_BYPASS);

            if (pa_add_match(info, offset, AVM_PA_LLC_SNAP) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            p += sizeof(struct llc_snap_hdr);
            state = AVM_PA_ETH_PROTO;
            ethproto = get_unaligned(&hdr->llcsnap.type);
            continue;

         case AVM_PA_LISP:
           if (AVM_PA_PKTTYPE_IPENCAP_VERSION(info->pkttype))
              RETURN(AVM_PA_RX_OK);
            if (p + LISP_DATAHDR_SIZE > end)
               RETURN(AVM_PA_RX_ERROR_LEN);
            info->encap_offset = info->ip_offset;
            info->lisp_offset = offset;
            p += LISP_DATAHDR_SIZE;
            hdr = (hdrunion_t *)p;
            if (hdr->iph.version == 4)
               state = AVM_PA_IPV4;
            else if (hdr->iph.version == 6)
               state = AVM_PA_IPV6;
            else
               RETURN(AVM_PA_RX_OK); /* not a lisp packet */
            if (pa_add_match(info, offset, AVM_PA_LISP) < 0)
               RETURN(AVM_PA_RX_ERROR_MATCH);
            info->pkttype = AVM_PA_PKTTYPE_IP2IPENCAP_VERSION(info->pkttype);
            info->pkttype |= AVM_PA_PKTTYPE_LISP;
            continue;

         default:
            RETURN(AVM_PA_RX_ERROR_STATE);
      }
   }
out:
   if (ret == AVM_PA_RX_OK && ttl <= 1)
      ret = AVM_PA_RX_TTL;
   if (ret == AVM_PA_RX_OK || pa_glob.dbgmatch) {
      info->hdrlen = p-data;
      memcpy(HDRCOPY(info), data, info->hdrlen);
   }
   return ret;
#undef RETURN
}

static inline void pa_match_set_hash(struct avm_pa_pkt_match *info)
{
   int i;
   info->hash = 0;

   for (i = 0 ; i < info->nmatch; i++) {
      struct avm_pa_match_info *p = &info->match[i];
      hdrunion_t *hdr = (hdrunion_t *)(HDRCOPY(info)+p->offset);
      switch (p->type) {
         case AVM_PA_IPV4:
#if AVM_PA_UNALIGNED_CHECK
            if (((unsigned long)&hdr->iph.saddr) & 0x3)
               if (net_ratelimit())
                  printk(KERN_INFO "avm_pa: unaligned access %p (ipv4)\n",
                                   &hdr->iph.saddr);
#endif
            info->hash ^= hdr->iph.saddr;
            info->hash ^= hdr->iph.daddr;
            info->hash ^= hdr->iph.protocol;
            info->hash ^= hdr->iph.tos;
            break;
         case AVM_PA_IPV6:
#if AVM_PA_UNALIGNED_CHECK
            if (((unsigned long)&hdr->ipv6h.saddr.s6_addr32[2]) & 0x3)
               if (net_ratelimit())
                  printk(KERN_INFO "avm_pa: unaligned access %p (ipv6)\n",
                                   &hdr->ipv6h.saddr.s6_addr32[2]);
#endif
            //info->hash ^= hdr->ipv6h.saddr.s6_addr32[0];
            //info->hash ^= hdr->ipv6h.saddr.s6_addr32[1];
            info->hash ^= hdr->ipv6h.saddr.s6_addr32[2];
            info->hash ^= hdr->ipv6h.saddr.s6_addr32[3];
            //info->hash ^= hdr->ipv6h.daddr.s6_addr32[0];
            //info->hash ^= hdr->ipv6h.daddr.s6_addr32[1];
            info->hash ^= hdr->ipv6h.daddr.s6_addr32[2];
            info->hash ^= hdr->ipv6h.daddr.s6_addr32[3];
            info->hash ^= hdr->ipv6h.nexthdr;
            break;
         case AVM_PA_PORTS:
            info->hash ^= hdr->ports[0];
            info->hash ^= hdr->ports[1];
            break;
         case AVM_PA_ICMPV4:
         case AVM_PA_ICMPV6:
            info->hash ^= hdr->ports[0]; /* type + code */
            info->hash ^= hdr->ports[2]; /* id */
            break;
      }
   }
   info->hash = (info->hash >> 16) ^ (info->hash & 0xffff);
   info->hash = (info->hash >> 8) ^ (info->hash & 0xff);
   info->hash %= CONFIG_AVM_PA_MAX_SESSION;
}

static int pa_set_pkt_match(enum avm_pa_framing framing,
                            unsigned int hstart,
                            PKT *pkt,
                            struct avm_pa_pkt_match *match,
                            int ffaspkt)
{
   int rc;
   pa_reset_match(match);
   rc = set_pkt_match(framing, hstart, pkt, match, ffaspkt);
   if (rc == AVM_PA_RX_OK)
      pa_match_set_hash(match);
   return rc;
}

static inline int pa_match_cmp(struct avm_pa_pkt_match *a1,
                               struct avm_pa_pkt_match *a2)
{
   struct avm_pa_match_info *p = 0;
   hdrunion_t *h1, *h2;
   int rc;
   int i;

   rc = (int)a1->nmatch - (int)a2->nmatch;
   if (rc) return rc;
   rc = memcmp(&a1->match, &a2->match,
               a1->nmatch*sizeof(struct avm_pa_match_info));
   if (rc) return rc;
   for (i = a1->nmatch-1; i >= 0; i--) {
      p = &a1->match[i];
      h1 = (hdrunion_t *)(HDRCOPY(a1)+p->offset);
      h2 = (hdrunion_t *)(HDRCOPY(a2)+p->offset);
      switch (p->type) {
         case AVM_PA_ETH:
            rc = memcmp(&h1->ethh, &h2->ethh, sizeof(struct ethhdr));
            if (rc) goto out;
            break;
         case AVM_PA_VLAN:
            rc = (int)VLAN_ID(&h1->vlanh) - (int)VLAN_ID(&h2->vlanh);
            if (rc) goto out;
            break;
         case AVM_PA_PPPOE:
            rc = (int)h1->pppoeh.sid - (int)h2->pppoeh.sid;
            if (rc) goto out;
            break;
         case AVM_PA_PPP:
            rc = (int)h1->ppph[0] - (int)h2->ppph[0];
            if (rc) goto out;
            break;
         case AVM_PA_IPV4:
            rc = (int)h1->iph.protocol - (int)h2->iph.protocol;
            if (rc) goto out;
            rc = (int)h1->iph.tos - (int)h2->iph.tos;
            if (rc) goto out;
            rc = (int)h1->iph.daddr - (int)h2->iph.daddr;
            if (rc) goto out;
            rc = (int)h1->iph.saddr - (int)h2->iph.saddr;
            if (rc) goto out;
            break;
         case AVM_PA_IPV6:
            rc = (int)h1->ipv6h.nexthdr - (int)h2->ipv6h.nexthdr;
            if (rc) goto out;
            rc = memcmp(&h1->ipv6h.daddr, &h2->ipv6h.daddr,
                        sizeof(struct in6_addr));
            if (rc) goto out;
            rc = memcmp(&h1->ipv6h.saddr, &h2->ipv6h.saddr,
                        sizeof(struct in6_addr));
            break;
         case AVM_PA_PORTS:
            rc = (int)h1->ports[0] - (int)h2->ports[0]; /* source */
            if (rc) goto out;
            rc = (int)h1->ports[1] - (int)h2->ports[1]; /* dest */
            if (rc) goto out;
            break;
         case AVM_PA_ICMPV4:
         case AVM_PA_ICMPV6:
            rc = (int)h1->ports[0] - (int)h2->ports[0]; /* type + code */
            if (rc) goto out;
            rc = (int)h1->ports[2] - (int)h2->ports[2]; /* id */
            if (rc) goto out;
            break;
         case AVM_PA_LLC_SNAP:
            rc = (int)h1->llcsnap.type - (int)h2->llcsnap.type;
            if (rc) goto out;
            break;
         case AVM_PA_L2TP:
            rc = (int)h1->l2tp.session_id - (int)h2->l2tp.session_id;
            if (rc) goto out;
            break;
      }
   }
out:
   return rc;
}

static void pa_show_pkt_match(struct avm_pa_pkt_match *match,
                              int is_bridged, u16 egress_pkttype,
                              pa_fprintf fprintffunc, void *arg)

{
   char buf[128];
   char *prompt;
   unsigned n;

   prompt = "Hash";
   (*fprintffunc)(arg, "%-10s: %lu\n", prompt, (unsigned long)match->hash);

   prompt = "PktType";
   if (is_bridged) {
      pkttype2str(match->pkttype & AVM_PA_PKTTYPE_IP_MASK, buf, sizeof(buf));
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   } else {
      if (egress_pkttype && egress_pkttype != match->pkttype) {
         size_t half = sizeof(buf)/2;
         pkttype2str(match->pkttype, buf, half);
         pkttype2str(egress_pkttype, buf+half, half);
         (*fprintffunc)(arg, "%-10s: %s -> %s\n", prompt, buf, buf+half);
      } else {
         pkttype2str(match->pkttype, buf, sizeof(buf));
         (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
      }
   }

   if (match->nmatch && !is_bridged) {
      prompt = "FragOk";
      snprintf(buf, sizeof(buf), "%u", match->fragok);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
      prompt = "Syn";
      snprintf(buf, sizeof(buf), "%u", match->syn);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
      prompt = "Fin";
      snprintf(buf, sizeof(buf), "%u", match->fin);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }

   for (n=0; n < match->nmatch; n++) {
      struct avm_pa_match_info *p = match->match+n;
      hdrunion_t *hdr = (hdrunion_t *)(HDRCOPY(match)+p->offset);
      char *s = buf;
      char *end = buf+sizeof(buf);
      prompt = 0;
      switch (p->type) {
         case AVM_PA_ETH:
            prompt = "Eth Hdr DS";
            mac2str(&hdr->ethh.h_dest, s, end-s); s += strlen(s);
            *s++ = ' ';
            mac2str(&hdr->ethh.h_source, s, end-s); s += strlen(s);
            *s++ = ' ';
            snprintf(s, end-s, "%04X", ntohs(hdr->ethh.h_proto));
            break;
         case AVM_PA_VLAN:
            prompt = "Vlan ID";
            snprintf(buf, sizeof(buf), "%d", VLAN_ID(&hdr->vlanh));
            break;
         case AVM_PA_PPPOE:
            prompt = "PPPoE Sid";
            snprintf(buf, sizeof(buf), "%04X", ntohs(hdr->pppoeh.sid));
            break;
         case AVM_PA_PPP:
            prompt = "PPP Proto";
            snprintf(buf, sizeof(buf), "%02X", hdr->ppph[0]);
            break;
         case AVM_PA_IPV4:
            prompt = "IPv4 Hdr";
            in_addr2str(&hdr->iph.saddr, s, end-s); s += strlen(s);
            *s++ = ' ';
            in_addr2str(&hdr->iph.daddr, s, end-s); s += strlen(s);
            *s++ = ' ';
            snprintf(s, end-s, "0x%02x", hdr->iph.tos);
            *s++ = ' ';
            snprintf(s, end-s, "%d", hdr->iph.protocol);
            break;
         case AVM_PA_IPV6:
            prompt = "IPv6 Hdr";
            in6_addr2str(&hdr->ipv6h.saddr, s, end-s); s += strlen(s);
            *s++ = ' ';
            in6_addr2str(&hdr->ipv6h.daddr, s, end-s); s += strlen(s);
            *s++ = ' ';
            snprintf(s, end-s, "%d", hdr->ipv6h.nexthdr);
            break;
         case AVM_PA_PORTS:
            prompt = "Ports";
            snprintf(buf, sizeof(buf), "%d -> %d",
                     ntohs(hdr->ports[0]), ntohs(hdr->ports[1]));
            break;
         case AVM_PA_ICMPV4:
            switch (hdr->icmph.type) {
               case ICMP_ECHOREPLY: 
                  prompt = "ICMPv4";
                  snprintf(buf, sizeof(buf), "echo reply id=%hu",
                           hdr->icmph.un.echo.id);
                  break;
               case ICMP_ECHO:
                  prompt = "ICMPv4";
                  snprintf(buf, sizeof(buf), "echo request id=%hu",
                           hdr->icmph.un.echo.id);
                  break;
               default:
                  strcpy(buf, "??????");
                  break;
            }
            break;
         case AVM_PA_ICMPV6:
            prompt = "ICMPv6";
            switch (hdr->icmpv6h.icmp6_type) {
               case ICMPV6_ECHO_REQUEST:
                  snprintf(buf, sizeof(buf), "echo request id=%hu",
                           hdr->icmpv6h.icmp6_identifier);
                  break;
               case ICMPV6_ECHO_REPLY:
                  snprintf(buf, sizeof(buf), "echo reply id=%hu",
                           hdr->icmpv6h.icmp6_identifier);
                  break;
               default:
                  strcpy(buf, "??????");
                  break;
            }
            break;
         case AVM_PA_LLC_SNAP:
            prompt = "LLC SNAP";
            snprintf(buf, sizeof(buf), "%04X", ntohs(hdr->llcsnap.type));
            break;
         case AVM_PA_LISP:
            prompt = "LISP";
            snprintf(buf, sizeof(buf), "data header");
            break;
         case AVM_PA_L2TP:
            prompt = "L2TP";
            snprintf(buf, sizeof(buf), "session %lu",
                     (unsigned long)ntohl(hdr->l2tp.session_id));
            break;
      }
      if (prompt)
         (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
}

static void pa_show_pkt_info(struct avm_pa_pkt_info *info,
                             pa_fprintf fprintffunc, void *arg)
{
   struct avm_pa_global *ctx = &pa_glob;
   char buf[64];
   char *prompt;

   prompt = "In Pid";
   snprintf(buf, sizeof(buf), "%d (%s)",
            info->ingress_pid_handle,
            PA_PID(ctx, info->ingress_pid_handle)->cfg.name);
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);

   if (info->ingress_vpid_handle) {
      prompt = "In VPid";
      snprintf(buf, sizeof(buf), "%d (%s)",
               info->ingress_vpid_handle,
               PA_VPID(ctx, info->ingress_vpid_handle)->cfg.name);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (info->egress_vpid_handle) {
      prompt = "Out VPid";
      snprintf(buf, sizeof(buf), "%d (%s)",
               info->egress_vpid_handle,
               PA_VPID(ctx, info->egress_vpid_handle)->cfg.name);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }

   if (info->routed) {
      (*fprintffunc)(arg, "%-10s: %s\n", "Routed", "yes");
   }

   pa_show_pkt_match(&info->match, 0, 0, fprintffunc, arg);
}

/* ------------------------------------------------------------------------ */
/* -------- mod rec ------------------------------------------------------- */
/* ------------------------------------------------------------------------ */

/*
 * From RFC 1624 Incremental Internet Checksum
 *
 * HC  - old checksum in header
 * HC' - new checksum in header
 * m   - old value of a 16-bit field
 * m'  - new value of a 16-bit field
 * HC' = ~(~HC + ~m + m') --    [Eqn. 3]
 * HC' = HC - ~m - m'     --    [Eqn. 4]
 *
 *
 * csum_unfold(): be16 -> u32
 * 
 * M   = ~m + m';
 *
 * we use Eqn.3, because we precalculate M.
 * csum_fold(): add the carries
 *
 * HC' = ~csum_fold((~csum_unfold(HC) + ~m + m'));
 *
 * HC' = ~csum_fold(csum_add(~csum_unfold(HC), M);
 *
 */

static inline u32 hcsum_add(u32 sum, u32 addend)
{
   sum += addend;
   if (sum < addend) sum++; /* skip -0 */
   return sum; // + (sum < addend);
}

static inline u32 hcsum_prepare(u16 sum)
{
   return (u16)(~sum);
}

static inline u32 hcsum_u32(u32 sum, u32 from, u32 to)
{
   sum = hcsum_add(sum, ~from);
   sum = hcsum_add(sum, to);
   return sum;
}

static inline u32 hcsum_u16(u32 sum, u16 from, u16 to)
{
   sum = hcsum_u32(sum, from, to);
   return sum;
}

static inline u16 hcsum_fold(u32 sum)
{
   while (sum >> 16)
      sum = (sum & 0xffff) + (sum >> 16);
   return sum;
}

static inline u16 hcsum_finish(u32 sum)
{
   return ~hcsum_fold(sum);
}

static int pa_set_v4_mod_rec(struct avm_pa_v4_mod_rec *mod,
                             int update_ttl, u8 *in, u8 *out)
{
   struct iphdr *iiph = (struct iphdr *)in;
   struct iphdr *oiph = (struct iphdr *)out;
   u32 l3_check = 0;
   u32 l4_check;
   int isicmp = 0;

   mod->flags = 0;

   mod->saddr = oiph->saddr;
   if (iiph->saddr != oiph->saddr) {
      mod->flags |= AVM_PA_V4_MOD_SADDR|AVM_PA_V4_MOD_IPHDR_CSUM;
      l3_check = hcsum_u32(l3_check, iiph->saddr, oiph->saddr);
   }

   mod->daddr = oiph->daddr;
   if (iiph->daddr != oiph->daddr) {
      mod->flags |= AVM_PA_V4_MOD_DADDR|AVM_PA_V4_MOD_IPHDR_CSUM;
      l3_check = hcsum_u32(l3_check, iiph->daddr, oiph->daddr);
   }

   l4_check = l3_check;

   mod->tos = oiph->tos;
   if (iiph->tos != oiph->tos) {
      mod->flags |= AVM_PA_V4_MOD_TOS|AVM_PA_V4_MOD_IPHDR_CSUM;
      l3_check = hcsum_u16(l3_check, htons(iiph->tos), htons(oiph->tos));
   }

   if (update_ttl) {
      mod->flags |= AVM_PA_V4_MOD_UPDATE_TTL|AVM_PA_V4_MOD_IPHDR_CSUM;
      l3_check = hcsum_u16(l3_check, constant_htons(0x0100), 0x0000);
   }

   mod->l3crc_update = hcsum_fold(l3_check);

   switch (iiph->protocol) {
      case IPPROTO_TCP:
         mod->l4crc_offset = offsetof(struct tcphdr, check);
         break;
      case IPPROTO_UDP:
         mod->l4crc_offset = offsetof(struct udphdr, check);
         break;
      case IPPROTO_ICMP:
#ifdef _LINUX_ICMP_H
         mod->l4crc_offset = offsetof(struct icmphdr, checksum);
#else
         mod->l4crc_offset = offsetof(struct icmphdr, check);
#endif
         isicmp = 1;
         break;
      default:
         mod->l4crc_offset = 0;
         break;
   }
   mod->l4crc_update = 0;
   if (mod->l4crc_offset) {
      u16 *iports = (u16 *)(in + PA_IPHLEN(iiph));
      u16 *oports = (u16 *)(out + PA_IPHLEN(oiph));
      if (isicmp) {
         l4_check = 0;
         mod->id = oports[2];
         if (iports[2] != oports[2]) {
            mod->flags |= AVM_PA_V4_MOD_ICMPID|AVM_PA_V4_MOD_PROTOHDR_CSUM;
            l4_check = hcsum_u16(l4_check, iports[2], oports[2]);
         }
      } else {
         if (mod->flags & AVM_PA_V4_MOD_ADDR)
            mod->flags |= AVM_PA_V4_MOD_PROTOHDR_CSUM;
         mod->sport = oports[0];
         if (iports[0] != oports[0]) {
            mod->flags |= AVM_PA_V4_MOD_SPORT|AVM_PA_V4_MOD_PROTOHDR_CSUM;
            l4_check = hcsum_u16(l4_check, iports[0], oports[0]);
         }
         mod->dport = oports[1];
         if (iports[1] != oports[1]) {
            mod->flags |= AVM_PA_V4_MOD_DPORT|AVM_PA_V4_MOD_PROTOHDR_CSUM;
            l4_check = hcsum_u16(l4_check, iports[1], oports[1]);
         }
      }
      mod->l4crc_update = hcsum_fold(l4_check);
   }
   return mod->flags != 0;
}

static void pa_do_v4_mod_rec(struct avm_pa_v4_mod_rec *mod, u8 *data)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct iphdr *iph = (struct iphdr *)data;
   u16 *ports = (u16 *)(data + PA_IPHLEN(iph));
   u32 sum;
   u16 csum;

   ctx->stats.rx_mod++;

   if (((unsigned long)iph) & 0x3) {
      memcpy(&iph->saddr, &mod->saddr, 2*sizeof(u32));
   } else {
      iph->saddr = mod->saddr;
      iph->daddr = mod->daddr;
   }
   iph->tos = mod->tos;
   if (mod->flags & AVM_PA_V4_MOD_UPDATE_TTL)
      iph->ttl--;
   sum = hcsum_prepare(iph->check);
   iph->check = hcsum_finish(hcsum_add(sum, mod->l3crc_update));

   if (mod->flags & AVM_PA_V4_MOD_PORT) {
      ports[0] = mod->sport;
      ports[1] = mod->dport;
   } else if (mod->flags & AVM_PA_V4_MOD_ICMPID) {
      ports[2] = mod->id;
   }
   csum = ports[mod->l4crc_offset>>1];
   if (csum || iph->protocol != IPPROTO_UDP) {
      sum = hcsum_prepare(csum);
      ports[mod->l4crc_offset>>1] = hcsum_finish(hcsum_add(sum, mod->l4crc_update));
   }
}

static void pa_show_v4_mod_rec(struct avm_pa_v4_mod_rec *mod,
                               pa_fprintf fprintffunc, void *arg)

{
   char buf[64];
   char *prompt;

   if (mod->flags & AVM_PA_V4_MOD_SADDR) {
      prompt = "*IPv4 Src";
      in_addr2str(&mod->saddr, buf, sizeof(buf));
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->flags & AVM_PA_V4_MOD_DADDR) {
      prompt = "*IPv4 Dst";
      in_addr2str(&mod->daddr, buf, sizeof(buf));
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->flags & AVM_PA_V4_MOD_TOS) {
      prompt = "*IPv4 Tos";
      snprintf(buf, sizeof(buf), "0x%02x", mod->tos);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->flags & AVM_PA_V4_MOD_UPDATE_TTL) {
      prompt = "*IPv4 TTL";
      snprintf(buf, sizeof(buf), "decrease");
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->flags & AVM_PA_V4_MOD_IPHDR_CSUM) {
      prompt = "*L3 Sum";
      snprintf(buf, sizeof(buf), "0x%02x", mod->l3crc_update);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->flags &  AVM_PA_V4_MOD_SPORT) {
      prompt = "*Src Port";
      snprintf(buf, sizeof(buf), "%d", ntohs(mod->sport));
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->flags &  AVM_PA_V4_MOD_DPORT) {
      prompt = "*Dst Port";
      snprintf(buf, sizeof(buf), "%d", ntohs(mod->dport));
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->flags & AVM_PA_V4_MOD_ICMPID) {
      prompt = "*ICMP Id";
      snprintf(buf, sizeof(buf), "%d", ntohs(mod->id));
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->flags & AVM_PA_V4_MOD_PROTOHDR_CSUM) {
      prompt = "*L4 Sum";
      snprintf(buf, sizeof(buf), "0x%02x", mod->l4crc_update);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
}

/* ------------------------------------------------------------------------ */

static void pa_show_mod_rec(struct avm_pa_mod_rec *mod,
                            pa_fprintf fprintffunc, void *arg)

{
   char buf[256];
   char *prompt;

   prompt = "Hdrlen";
   snprintf(buf, sizeof(buf), "%u", (unsigned)mod->hdrlen);
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);

   if (mod->ipversion) {
      prompt = "IP version";
      snprintf(buf, sizeof(buf), "%u", (unsigned)mod->ipversion);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }

   if (mod->pull_l2_len) {
      prompt = "L2 pull";
      snprintf(buf, sizeof(buf), "%d", mod->pull_l2_len);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->pull_encap_len) {
      prompt = "Encap pull";
      snprintf(buf, sizeof(buf), "%d", mod->pull_encap_len);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->push_ipversion) {
      prompt = "Push IPv";
      snprintf(buf, sizeof(buf), "%u", (unsigned)mod->push_ipversion);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->push_udpoffset) {
      prompt = "Push UDP";
      snprintf(buf, sizeof(buf), "%u", (unsigned)mod->push_udpoffset);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   if (mod->push_encap_len) {
      prompt = "Encap push";
      data2hex(HDRCOPY(mod)+mod->push_l2_len, mod->push_encap_len,
                  buf, sizeof(buf));
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }

   prompt = "SKB proto";
   snprintf(buf, sizeof(buf), "%04x", (unsigned)ntohs(mod->protocol));
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   
   pa_show_v4_mod_rec(&mod->v4_mod, fprintffunc, arg);

   if (mod->v6_decrease_hop_limit) {
      prompt = "IPv6 ttl";
      snprintf(buf, sizeof(buf), "decrease");
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
}

static int pa_egress_precheck(struct avm_pa_pid *pid,
                              PKT *pkt,
                              struct avm_pa_pkt_match *ingress,
                              struct avm_pa_pkt_match *egress)
{
   unsigned int hstart;
   if (pid->ecfg.flags & AVM_PA_PID_FLAG_HSTART_ON_EGRESS)
      hstart = AVM_PKT_INFO(pkt)->hstart;
   else
      hstart = 0;
   if (pa_set_pkt_match(pid->egress_framing, hstart,
                        pkt, egress, 1) != AVM_PA_RX_OK)
      return -1;
   if (AVM_PA_PKTTYPE_BASE_EQ(egress->pkttype, ingress->pkttype))
      return 0;
   return -1;
}

static int pa_calc_modify(struct avm_pa_session *session,
                          struct avm_pa_pkt_match *ingress,
                          struct avm_pa_pkt_match *egress)
{
   /*
    * Precondition: AVM_PA_PKTTYPE_BASE_EQ(egress->pkttype, ingress->pkttype)
    */
   struct avm_pa_mod_rec *mod = &session->mod;
   int change = 0;

   mod->hdrlen = egress->hdrlen;
   mod->hdroff = egress->hdroff;
   memcpy(HDRCOPY(mod), HDRCOPY(egress), mod->hdrlen);
   mod->protocol = 0;
   mod->pkttype = egress->pkttype;
   if (AVM_PA_PKTTYPE_EQ(ingress->pkttype, egress->pkttype)) {
      mod->pull_encap_len = 0;
      if (ingress->encap_offset == AVM_PA_OFFSET_NOT_SET) {
         /* no tunnel, egress->encap_offset also not set */
         mod->pull_l2_len = ingress->ip_offset;
         mod->pull_encap_len = 0;
         mod->ipversion = AVM_PA_PKTTYPE_IP_VERSION(egress->pkttype);
         mod->push_encap_len = 0;
         mod->push_ipversion = 0;
         mod->push_l2_len = egress->ip_offset;
      } else {
         /* untouched tunnel, egress->encap_offset also set */
         mod->pull_l2_len = ingress->encap_offset;
         mod->pull_encap_len = 0;
         mod->ipversion = AVM_PA_PKTTYPE_IPENCAP_VERSION(egress->pkttype);
         mod->push_encap_len = 0;
         mod->push_ipversion = 0;
         mod->push_l2_len = egress->encap_offset;
      }
   } else { /* AVM_PA_PKTTYPE_BASE_EQ because of precheck */
      change++;
      if (ingress->encap_offset == AVM_PA_OFFSET_NOT_SET) {
         /* no tunnel header on input */
         mod->pull_l2_len = ingress->ip_offset;
         mod->pull_encap_len = 0;
         mod->ipversion = AVM_PA_PKTTYPE_IP_VERSION(ingress->pkttype);
      } else {
         /* tunnel header on input */
         mod->pull_l2_len = ingress->encap_offset;
         mod->pull_encap_len = ingress->ip_offset - ingress->encap_offset;
         mod->ipversion = AVM_PA_PKTTYPE_IP_VERSION(ingress->pkttype);
      }
      if (egress->encap_offset == AVM_PA_OFFSET_NOT_SET) {
         mod->push_encap_len = 0;
         mod->push_ipversion = 0;
         mod->push_l2_len = egress->ip_offset;
      } else {
         mod->push_encap_len = egress->ip_offset - egress->encap_offset;
         mod->push_ipversion = AVM_PA_PKTTYPE_IPENCAP_VERSION(egress->pkttype);
         mod->push_l2_len = egress->encap_offset;
      }
   }
   if (mod->push_ipversion) {
      change++;
      if (mod->push_ipversion == 4)
         mod->protocol = constant_htons(ETH_P_IP);
      else if (mod->push_ipversion == 6)
         mod->protocol = constant_htons(ETH_P_IPV6);
      if (egress->lisp_offset != AVM_PA_OFFSET_NOT_SET) {
         mod->push_udpoffset = egress->lisp_offset - egress->encap_offset;
         mod->push_udpoffset -= sizeof(struct udphdr);
      }
   } else {
      if (mod->ipversion == 4)
         mod->protocol = constant_htons(ETH_P_IP);
      else if (mod->ipversion == 6)
         mod->protocol = constant_htons(ETH_P_IPV6);
      mod->push_udpoffset = 0;
   }
   if (mod->ipversion == 4) {
      int ingress_offset = mod->pull_l2_len + mod->pull_encap_len;
      int egress_offset = mod->push_l2_len + mod->push_encap_len;
      if (pa_set_v4_mod_rec(&mod->v4_mod, session->routed,
                            HDRCOPY(ingress)+ingress_offset,
                            HDRCOPY(mod)+egress_offset))
         change++;
   } else if (mod->ipversion == 6) {
      if (session->routed) {
         mod->v6_decrease_hop_limit = 1;
         change++;
      }
   }
   return change;
}

static u8 casttype2pkt_type[] = {
   PACKET_HOST,
   PACKET_MULTICAST,
   PACKET_BROADCAST
};

static inline void _pa_transmit(struct avm_pa_egress *egress, PKT *pkt, int in_hw)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, egress->pid_handle);
   unsigned int prio  = egress->output.priority;
   prio = (prio & TC_H_MIN_MASK);
   if (prio >= AVM_PA_MAX_PRIOS) prio = AVM_PA_MAX_PRIOS - 1;

   if (egress->vpid_handle) {
      struct avm_pa_vpid *vpid = PA_VPID(ctx, egress->vpid_handle);
      ((u32 *)(&vpid->stats.tx_unicast_pkt))[egress->match.casttype]++;
      vpid->stats.tx_bytes += PKT_LEN(pkt);
      if (in_hw) {
         vpid->hwstats[prio].tx_pkts++;
         vpid->hwstats[prio].tx_bytes += PKT_LEN(pkt);
      }
   }
   AVM_PKT_INFO(pkt)->is_accelerated = 1;
   egress->tx_pkts++;
   pid->tx_pkts++;

   if (egress->not_local) {
      if (egress->push_l2_len) {
         memcpy(PKT_PUSH(pkt, egress->push_l2_len),
                HDRCOPY(&egress->match), egress->push_l2_len);
         if (egress->pppoe_offset != AVM_PA_OFFSET_NOT_SET) {
            unsigned char *data = PKT_DATA(pkt) + egress->pppoe_offset;
            struct pppoehdr *pppoehdr = (struct pppoehdr *)data;
            pppoehdr->length = htons(PKT_LEN(pkt) - egress->pppoe_hdrlen);
         }
      }
      if ((egress->match.pkttype & AVM_PA_PKTTYPE_PROTO_MASK) == IPPROTO_TCP
            && PKT_LEN(pkt) < ctx->prioack_packet_size)
         egress->tcpack_pkts++;

      pkt->priority = egress->output.priority;
      pkt->tc_index = egress->output.tc_index;

      if (pid->ecfg.cb_len) {
         memcpy(&pkt->cb[pid->ecfg.cb_start],
                egress->output.cb, pid->ecfg.cb_len);
      }
      pkt->uniq_id &= 0xffffff;
      pkt->uniq_id |= ((unsigned long)egress->output.cpmac_prio) << 24;

      pkt->pkt_type = PACKET_OUTGOING;
      pkt->ip_summed = CHECKSUM_NONE;
      skb_reset_mac_header(pkt);
      skb_set_network_header(pkt, egress->push_l2_len);
#if AVM_PA_TRACE
      if (ctx->dbgtrace)
         pa_printk(KERN_DEBUG, "avm_pa: %lu - _pa_transmit(%s)\n",
                               pkt->uniq_id & 0xffffff, pid->cfg.name);
#endif
      (*pid->cfg.tx_func)(pid->cfg.tx_arg, pkt);
   } else {
      struct packet_type *ptype = pid->cfg.ptype;
      skb_set_network_header(pkt, 0);
      pkt->pkt_type = casttype2pkt_type[egress->match.casttype];
      if (egress->local.dst)
         skb_dst_set(pkt, dst_clone(egress->local.dst));
      pkt->dev = egress->local.dev;
      ctx->stats.fw_local++;
      (*ptype->func)(pkt, pkt->dev, ptype, 0);
   }
}

static inline u16 calc_frag_size(u16 mtu, u16 len)
{
   u16 frag_num = len/mtu;
   u16 frag_size;
   if (len % mtu) frag_num ++; 
   frag_size = len / frag_num;
   if (frag_size & 7) { /* mod 8 */
      if (frag_num > 1 && 
         (((frag_num - 1)*(frag_size & 7) + frag_size ) > mtu)) {
         frag_num++;
         frag_size = len / frag_num;
      }
   }
   frag_size = frag_size & ~7; /* multiple of 8 */
   return frag_size;
}

static void zero_fragment_options(struct iphdr *iph)
{
   unsigned char *p = (unsigned char *)(iph+1);
   unsigned char *e = p + PA_IPHLEN(iph);
   unsigned char olen;
   while (p < e) {
      if (*p == IPOPT_EOL) {
         return;
      } else if (*p == IPOPT_NOP) {
         p++;
      } else {
         olen = *p;
         if (olen < 2 || p+olen > e)
            return;
         if (!IPOPT_COPIED(*p))
            memset(p, IPOPT_NOP, olen);
         p += olen;
      }
   }
}

static void pa_fragment_ipv4(struct avm_pa_egress *egress, u16 omtu, PKT *pkt, int in_hw)
{
   struct avm_pa_global *ctx = &pa_glob;
   u16 iphlen, len, left, mtu, offset, mf, frag_size = 0;
   unsigned char *data;
   struct iphdr *iph;

   iph = (struct iphdr *)PKT_DATA(pkt);
   iphlen = (u16)PA_IPHLEN(iph);

   mtu = (u16)((omtu - iphlen) & ~7); /* set mtu to multiple of 8 */
   left = (u16)(PKT_LEN(pkt) - iphlen);
   data = PKT_DATA(pkt) + iphlen;

   offset = (u16)((ntohs(iph->frag_off) & IP_OFFSET) << 3);
   mf = (u16)(iph->frag_off & constant_htons(IP_MF));

   frag_size = calc_frag_size(mtu, left);

   while (left > 0) {
      struct iphdr *niph;
      PKT *npkt;
      if (left > mtu) len = frag_size; /* prevent to small fragments */
      else len = left;
      if ((npkt = PKT_ALLOC(iphlen+len)) == 0) {
         PKT_FREE(pkt);
         ctx->stats.fw_frag_fail++;
         return;
      }
      npkt->protocol = pkt->protocol;
      memcpy(PKT_DATA(npkt), PKT_DATA(pkt), iphlen);
      memcpy(PKT_DATA(npkt) + iphlen, data, len);
      niph = (struct iphdr *)PKT_DATA(npkt);
      niph->frag_off = htons((u16)(offset >> 3));
      left -= len;
      if (offset == 0) zero_fragment_options(iph);
      if (left > 0 || mf)
         niph->frag_off |= constant_htons(IP_MF);
      data += len;
      offset += len;
      niph->tot_len = htons((u16)(iphlen+len));
      set_ip_checksum(niph);
      ctx->stats.fw_frags++;
      _pa_transmit(egress, npkt, in_hw);
   }
   PKT_FREE(pkt);
}

static void pa_fragment_ipv6(struct avm_pa_egress *egress, u16 omtu, PKT *pkt, int in_hw)
{
   struct avm_pa_global *ctx = &pa_glob;
   u16 phlen, hlen, nhlen, len, left, mtu, offset, frag_size = 0;
   struct ipv6hdr *ipv6h;
   unsigned char *data;
   u32 id;
   
   ipv6h = (struct ipv6hdr *)pkt->data;
   phlen = sizeof(struct ipv6hdr) + sizeof(struct ipv6fraghdr);
   hlen = (u16)sizeof(struct ipv6hdr);
   nhlen = (u16)hlen + sizeof(struct ipv6fraghdr);

   /* set mtu to multiple of 8 */
   mtu = (u16)((omtu - phlen) & ~7);
   left = (u16)(pkt->len - hlen);
   data = pkt->data + hlen;

   frag_size = calc_frag_size(mtu, left);

   offset = 0;
   id = rand();

   while (left > 0) {
      struct ipv6fraghdr *fragh;
      struct ipv6hdr *nipv6h;
      PKT *npkt;

      if (left > mtu) len = frag_size; /* prevent to small fragments */
      else len = left;
      if ((npkt = PKT_ALLOC(nhlen+len)) == 0) {
         PKT_FREE(pkt);
         ctx->stats.fw_frag_fail++;
         return;
      }
      npkt->protocol = pkt->protocol;
      memcpy(PKT_DATA(npkt), PKT_DATA(pkt), hlen);
      memcpy(PKT_DATA(npkt) + nhlen, data, len);
      nipv6h = (struct ipv6hdr *)PKT_DATA(npkt);
      fragh = (struct ipv6fraghdr *)(nipv6h + 1);
      memcpy(nipv6h, ipv6h, sizeof(struct ipv6hdr));
      fragh->nexthdr = nipv6h->nexthdr;
      nipv6h->nexthdr = IPPROTO_FRAGMENT;
      fragh->reserved = 0;
      fragh->frag_off = htons((u16)offset);
      fragh->identification = id;
      left -= len;
      if (left > 0)
         fragh->frag_off |= constant_htons(IP6_MF);
      data += len;
      offset += len;
      nipv6h->payload_len = htons((u16)(sizeof(struct ipv6fraghdr)+len));
      ctx->stats.fw_frags++;
      _pa_transmit(egress, npkt, in_hw);
   }
   PKT_FREE(pkt);
}

static void pa_transmit(struct avm_pa_egress *egress, PKT *pkt, int in_hw)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, egress->pid_handle);

   if (pid->pid_handle != egress->pid_handle) {
      PKT_FREE(pkt);
      ctx->stats.fw_drop++;
      return;
   }
   if (PKT_LEN(pkt) > egress->mtu) {
      if (pkt->protocol == constant_htons(ETH_P_IP)) {
         pa_fragment_ipv4(egress, egress->mtu, pkt, in_hw);
         return;
      } else if (pkt->protocol == constant_htons(ETH_P_IPV6)) {
         pa_fragment_ipv6(egress, egress->mtu, pkt, in_hw);
         return;
      }
   }
   ctx->stats.fw_pkts++;
   _pa_transmit(egress, pkt, in_hw);
}

static void
_pa_do_modify_and_send(struct avm_pa_session *session, PKT *pkt)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_mod_rec *mod = &session->mod;
   struct avm_pa_egress *egress;
   int negress;
   PKT *npkt;
   
   if (session->ingress_vpid_handle) {
      struct avm_pa_vpid *vpid = PA_VPID(ctx, session->ingress_vpid_handle);
      ((u32 *)(&vpid->stats.rx_unicast_pkt))[session->ingress.casttype]++;
      vpid->stats.rx_bytes += PKT_LEN(pkt);
   }

   if (mod->pull_l2_len)
      PKT_PULL(pkt, mod->pull_l2_len);
   if (mod->pull_encap_len)
      PKT_PULL(pkt, mod->pull_encap_len);
   if (mod->v4_mod.flags) {
      pa_do_v4_mod_rec(&mod->v4_mod, PKT_DATA(pkt));
   } else if (mod->v6_decrease_hop_limit) {
      struct ipv6hdr *ipv6h = (struct ipv6hdr *)PKT_DATA(pkt);
      ipv6h->hop_limit--;
   }
   if (mod->push_encap_len) {
      unsigned tot_len;
      memcpy(PKT_PUSH(pkt, mod->push_encap_len),
             HDRCOPY(mod)+mod->push_l2_len, mod->push_encap_len);
      tot_len = PKT_LEN(pkt);
      if (mod->push_ipversion == 4) {
         struct iphdr *iph = (struct iphdr *)PKT_DATA(pkt);
         iph->id = rand() & 0xffff;
         iph->tot_len = htons(tot_len);
         set_ip_checksum(iph);
      } else {
         struct ipv6hdr *ipv6h = (struct ipv6hdr *)PKT_DATA(pkt);
         ipv6h->payload_len = htons(tot_len - sizeof(struct ipv6hdr));
      }
      if (mod->push_udpoffset) {
         struct udphdr *udph = (struct udphdr *)(PKT_DATA(pkt)+mod->push_udpoffset);
         udph->len = htons(tot_len - mod->push_udpoffset);
         if (mod->push_ipversion == 4)
            set_udp_checksum((struct iphdr *)PKT_DATA(pkt), udph);
         else
            set_udpv6_checksum((struct ipv6hdr *)PKT_DATA(pkt), udph);
      }
   }
   pkt->protocol = mod->protocol;
   atomic_inc(&session->transmit_in_progress);
   negress = session->negress;
   egress = &session->egress[0];
   for ( ; --negress; egress++) {
      if ((npkt = PKT_COPY(pkt)) != 0)
         pa_transmit(egress, npkt, session->in_hw);
      else
         ctx->stats.fw_fail++;
   }
   pa_transmit(egress, pkt, session->in_hw);
   atomic_dec(&session->transmit_in_progress);
   if (session->timeout == 0)
      pa_kill_session(session, "fast timeout");
}

static void
pa_do_modify_and_send(struct avm_pa_session *session, PKT *pkt)
{
   if (AVM_PKT_INFO(pkt)->already_modified) {
      struct avm_pa_egress *egress;
      egress = &session->egress[AVM_PKT_INFO(pkt)->egress_offset];
      pa_transmit(egress, pkt, session->in_hw);
      return;
   }
   _pa_do_modify_and_send(session, pkt);
}

static int pa_egress_size_check(struct avm_pa_session *session, PKT *pkt)
{
   struct avm_pa_pkt_info *info = AVM_PKT_INFO(pkt);

   if (info->match.fragok)
      return 0;
   if (session->mod.push_encap_len == 0) { /* no tunnel on output */
      struct avm_pa_mod_rec *mod = &session->mod;
      unsigned len = pkt->len - mod->pull_l2_len - mod->pull_encap_len;
      int negress;

      for (negress = 0; negress < session->negress; negress++) {
         struct avm_pa_egress *egress = &session->egress[negress];
         if (len > egress->mtu)
            return -1;
      }
   }
   return 0;
}

/* ------------------------------------------------------------------------ */
/* -------- macaddr management -------------------------------------------- */
/* ------------------------------------------------------------------------ */

static void pa_show_macaddr(struct avm_pa_macaddr *macaddr,
                            pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, macaddr->pid_handle);
   char buf[128];
   char *prompt;
   char *s = buf;
   char *end = buf + sizeof(buf);

   prompt = "Macaddr";
   mac2str(&macaddr->mac, s, end-s); s += strlen(s);
   snprintf(s, end-s, " ref %3lu Pid %2d %s", 
                      macaddr->refcount,
                      macaddr->pid_handle, pid->cfg.name);
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
}

static inline u32 macaddr_hash(unsigned char mac[ETH_ALEN])
{
   u32 h = 0;
   int i;

   for (i=0; i < ETH_ALEN; i++) {
      h += mac[i]; h += (h<<10); h ^= (h>>6);
   }
   h += (h<<3); h ^= (h>>11); h += (h<<15);
   return h;
}

static struct avm_pa_macaddr *
_pa_macaddr_link(unsigned char mac[ETH_ALEN], avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_macaddr *p;
   u32 hash;
   int i;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_WRITE_LOCK();
   
   hash = macaddr_hash(mac);

   for (p = ctx->macaddr_hash[hash%CONFIG_AVM_PA_MAX_SESSION]; p; p = p->link) {
      if (memcmp(p->mac, mac, ETH_ALEN) == 0) {
         p->refcount++;
         p->pid_handle = pid_handle;
         AVM_PA_WRITE_UNLOCK();
         return p;
      }
   }

   for (i=0; i < CONFIG_AVM_PA_MAX_SESSION; i++) {
      p = &ctx->macaddr_array[i];
      if (p->refcount == 0) {
         memcpy(p->mac, mac, ETH_ALEN);
         p->pid_handle = pid_handle;
         p->refcount++;
         p->link = ctx->macaddr_hash[hash%CONFIG_AVM_PA_MAX_SESSION];
         ctx->macaddr_hash[hash%CONFIG_AVM_PA_MAX_SESSION] = p;
         if (ctx->dbgsession) {
            pa_printk(KERN_DEBUG, "\navm_pa: new macaddr:\n");
            pa_show_macaddr(p, pa_printk, KERN_DEBUG);
         }
         AVM_PA_WRITE_UNLOCK();
         return p;
      }
   }
   AVM_PA_WRITE_UNLOCK();
   return 0;
}

static inline struct avm_pa_macaddr *
pa_macaddr_link(unsigned char mac[ETH_ALEN], avm_pid_handle pid_handle)
{
   if (mac[0] & 1)
      return 0;
   return _pa_macaddr_link(mac, pid_handle);
}

static void pa_macaddr_unlink(struct avm_pa_macaddr *destmac)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_macaddr *p, **pp;
   u32 hash;

   if (--destmac->refcount > 0)
      return;

   hash = macaddr_hash(destmac->mac);
   pp = &ctx->macaddr_hash[hash%CONFIG_AVM_PA_MAX_SESSION];
   while ((p = *pp) != 0) {
      if (p == destmac) {
         *pp = p->link;
         if (ctx->dbgsession) {
            pa_printk(KERN_DEBUG, "\navm_pa: delete macaddr:\n");
            pa_show_macaddr(p, pa_printk, KERN_DEBUG);
         }
         memset(p, 0, sizeof(struct avm_pa_macaddr));
         return;
      }
      pp = &p->link;
   }
}

static void pa_check_and_handle_ingress_pid_change(unsigned char mac[ETH_ALEN],
                                                   avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   u32 hash = macaddr_hash(mac);
   struct avm_pa_macaddr *p;
   int pid_changed = 0;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_READ_LOCK();
   
   for (p = ctx->macaddr_hash[hash%CONFIG_AVM_PA_MAX_SESSION]; p; p = p->link) {
      if (memcmp(mac, &p->mac, ETH_ALEN) == 0) {
         if (   p->pid_handle != pid_handle
             && PA_PID(ctx, p->pid_handle)->ingress_pid_handle != pid_handle)
            pid_changed = 1;
         break;
      }
   }

   AVM_PA_READ_UNLOCK();

   if (pid_changed) {
      char buf[128];
      mac2str(mac, buf, sizeof(buf));
      if (net_ratelimit())
         printk(KERN_INFO "avm_pa: pid changed for %s (%d %s -> %d %s)\n",
                          buf,
                          p->pid_handle, PA_PID(ctx, p->pid_handle)->cfg.name,
                          pid_handle, PA_PID(ctx, pid_handle)->cfg.name);
      pa_kill_sessions_with_destmac(p);
   }
}

/* ------------------------------------------------------------------------ */
/* -------- bsession management ------------------------------------------- */
/* ------------------------------------------------------------------------ */

static struct ethhdr *pa_get_ethhdr(enum avm_pa_framing framing, PKT *pkt)
{
   if (framing == avm_pa_framing_ether)
      return (struct ethhdr *)PKT_DATA(pkt);
   if (framing == avm_pa_framing_dev)
      return eth_hdr(pkt);
   return 0;
}

static inline u32 ethh_hash(struct ethhdr *ethh)
{
   return jhash_3words(get_unaligned((u32 *)(&ethh->h_source[2])),
                       get_unaligned((u32 *)(&ethh->h_dest[2])),
                       (u32)ethh->h_proto, 0);
}

static struct avm_pa_session *
pa_bsession_search(struct avm_pa_pid *pid, u32 hash, struct ethhdr *ethh)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_bsession *p;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_READ_LOCK();
   for (p = pid->hash_bsess[hash%CONFIG_AVM_PA_MAX_SESSION]; p; p = p->link) {
      if (memcmp(ethh, &p->ethh, sizeof(struct ethhdr)) == 0)
         break;
   }
   AVM_PA_READ_UNLOCK();
   return p ? PA_SESSION(ctx, p->session_handle) : 0;
}

static struct avm_pa_bsession *
pa_bsession_alloc(u32 hash, struct ethhdr *ethh,
                  avm_session_handle session_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_bsession *p = &ctx->bsess_array[session_handle];
   p->link = 0;
   p->hash = hash%CONFIG_AVM_PA_MAX_SESSION;
   memcpy(&p->ethh, ethh, sizeof(struct ethhdr));
   p->session_handle = session_handle;
   ctx->stats.nbsessions++;
   return p;
}

static void pa_show_bsession(struct avm_pa_bsession *bsession,
                             pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session = PA_SESSION(ctx, bsession->session_handle);
   char buf[128];
   char *prompt;
   char *s, *end = buf + sizeof(buf);

   prompt = "Session";
   snprintf(buf, sizeof(buf), "%d", bsession->session_handle);
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);

   prompt = "In Pid";
   snprintf(buf, sizeof(buf), "%d (%s)",
            session->ingress_pid_handle,
            PA_PID(ctx, session->ingress_pid_handle)->cfg.name);
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);

   prompt = "Hash";
   snprintf(buf, sizeof(buf), "%lu", (unsigned long)bsession->hash);
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);

   s = buf;
   prompt = "Eth Hdr DS";
   mac2str(&bsession->ethh.h_dest, s, end-s); s += strlen(s);
   *s++ = ' ';
   mac2str(&bsession->ethh.h_source, s, end-s); s += strlen(s);
   *s++ = ' ';
   snprintf(s, end-s, "%04X", ntohs(bsession->ethh.h_proto));
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
}

static void pa_bsession_add(struct avm_pa_pid *pid,
                            struct avm_pa_bsession *bsession)
{
   struct avm_pa_global *ctx = &pa_glob;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_WRITE_LOCK();
   
   bsession->link = pid->hash_bsess[bsession->hash];
   pid->hash_bsess[bsession->hash] = bsession;

   AVM_PA_WRITE_UNLOCK();

   if (ctx->dbgsession) {
      pa_printk(KERN_DEBUG, "\navm_pa: new bsession:\n");
      pa_show_bsession(bsession, pa_printk, KERN_DEBUG);
   }
}

static void pa_bsession_delete(struct avm_pa_pid *pid,
                               struct avm_pa_bsession *bsession)
{
   struct avm_pa_bsession **pp, *p;
   
   for (pp = &pid->hash_bsess[bsession->hash]; (p = *pp) != 0; pp = &p->link) {
      if (p == bsession) {
         struct avm_pa_global *ctx = &pa_glob;
         *pp = p->link;
         p->link = 0;
         p->session_handle = 0;
         ctx->stats.nbsessions--;
         break;
      }
   }
}

/* ------------------------------------------------------------------------ */
/* -------- session management -------------------------------------------- */
/* ------------------------------------------------------------------------ */

#define pa_session_search(pid, match) pa_session_hash_search(pid, match)

static struct avm_pa_session *
pa_session_hash_search(struct avm_pa_pid *pid, struct avm_pa_pkt_match *ingress)
{
   struct avm_pa_session *p;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_READ_LOCK();
   for (p = pid->hash_sess[ingress->hash%CONFIG_AVM_PA_MAX_SESSION]; p; p = p->link) {
      if (pa_match_cmp(ingress, &p->ingress) == 0)
         break;
   }
   AVM_PA_READ_UNLOCK();
   return p;
}

static int pa_session_valid(struct avm_pa_session *session)
{
   return session->is_on_lru && session->lru != AVM_PA_LRU_FREE;
}

static struct avm_pa_session *
pa_session_get_unlocked(avm_session_handle session_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session;

   session = PA_SESSION(ctx, session_handle);
   if (!pa_session_valid(session))
      session = 0;
   return session;
}

static struct avm_pa_session *
pa_session_get(avm_session_handle session_handle)
{
   struct avm_pa_session *session;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_READ_LOCK();
   session = pa_session_get_unlocked(session_handle);
   AVM_PA_READ_UNLOCK();
   return session;
}

static void pa_session_hash_insert(struct avm_pa_pid *pid,
                                   struct avm_pa_session *session)
{
   session->link = pid->hash_sess[session->ingress.hash];
   pid->hash_sess[session->ingress.hash] = session;
   session->hashed = 1;
}

static void pa_session_hash_delete(struct avm_pa_pid *pid,
                                   struct avm_pa_session *session)
{
   if (session->hashed) {
      struct avm_pa_session **pp, *p;
      for (pp = &pid->hash_sess[session->ingress.hash];
           (p = *pp) != 0;
           pp = &p->link) {
         if (p == session) {
            *pp = p->link;
            p->link = 0;
            session->hashed = 0;
            break;
         }
      }
   }
}

static void pa_session_lru_delete(struct avm_pa_session *session)
{
   if (session->is_on_lru) {
      struct avm_pa_global *ctx = &pa_glob;
      struct avm_pa_session_lru *lru = &ctx->sess_lru[session->lru];

      if (session->lru_prev) session->lru_prev->lru_next = session->lru_next;
      else lru->lru_head = session->lru_next;
      if (session->lru_next) session->lru_next->lru_prev = session->lru_prev;
      else lru->lru_tail = session->lru_prev;
      BUG_ON(lru->nsessions == 0);
      lru->nsessions--;
      session->lru_next = session->lru_prev = 0;
      session->is_on_lru = 0;
   }
}

static void pa_session_lru_update(int which, struct avm_pa_session *session)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session_lru *lru = &ctx->sess_lru[which];

   if (session->is_on_lru)
      pa_session_lru_delete(session);
   if (lru->lru_tail) {
      session->lru_next = 0;
      session->lru_prev = lru->lru_tail;
      lru->lru_tail->lru_next = session;
      lru->lru_tail = session;
   } else {
      session->lru_next = session->lru_prev = 0;
      lru->lru_head = lru->lru_tail = session;
   }
   lru->nsessions++;
   if (lru->nsessions > lru->maxsessions)
      lru->maxsessions = lru->nsessions;
   session->is_on_lru = 1;
   session->lru = which;
}

static void pa_session_activate(struct avm_pa_session *session)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, session->ingress_pid_handle);
   AVM_PA_LOCK_DECLARE;

   AVM_PA_WRITE_LOCK();
   pa_session_hash_insert(pid, session);
   pa_session_lru_update(AVM_PA_LRU_ACTIVE, session);
   session->endtime = jiffies + session->timeout;
   AVM_PA_WRITE_UNLOCK();
}

static inline void pa_session_update_unlocked(struct avm_pa_session *session)
{
   BUG_ON(session->is_on_lru == 0);
   if (session->lru == AVM_PA_LRU_ACTIVE) {
      pa_session_lru_update(AVM_PA_LRU_ACTIVE, session);
      session->endtime = jiffies + session->timeout;
   } else {
      /* session flushed */
   }
}

static void pa_session_update(struct avm_pa_session *session)
{
   AVM_PA_LOCK_DECLARE;

   AVM_PA_WRITE_LOCK();
   pa_session_update_unlocked(session);
   AVM_PA_WRITE_UNLOCK();
}

static inline void pa_start_gc_timer(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   mod_timer(&ctx->gc_timer, jiffies + AVM_PA_GC_TIMEOUT*HZ);
}

static void __init avm_pa_init_freelist(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   int i;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_WRITE_LOCK();
   for (i=CONFIG_AVM_PA_MAX_SESSION-1; i > 0; i--) {
      struct avm_pa_session *session = PA_SESSION(ctx, i);
      pa_session_lru_update(AVM_PA_LRU_FREE, session);
   }
   AVM_PA_WRITE_UNLOCK();
}

static struct avm_pa_session *pa_session_alloc(struct avm_pa_pkt_match *match)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_WRITE_LOCK();
   if ((session = ctx->sess_lru[AVM_PA_LRU_FREE].lru_head) != 0) {
      pa_session_lru_delete(session);
      memset(session, 0, sizeof(struct avm_pa_session));
      session->session_handle = session - ctx->sess_array;
      session->ingress = *match;
      session->endtime = jiffies;
      switch (match->pkttype & AVM_PA_PKTTYPE_PROTO_MASK) {
         case IPPROTO_TCP:
            session->timeout = ctx->tcp_timeout_secs*HZ;
            break;
         case IPPROTO_UDP:
            session->timeout = ctx->udp_timeout_secs*HZ;
            break;
         case IPPROTO_ICMPV6:
         case IPPROTO_ICMP:
            session->timeout = ctx->echo_timeout_secs*HZ;
            break;
      }
   }
   AVM_PA_WRITE_UNLOCK();
   pa_start_gc_timer();
   return session;
}

static void pa_show_session(struct avm_pa_session *session,
                            pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   unsigned negress;
   char buf[64];
   char *prompt;
   int count;

   prompt = "Session";
   snprintf(buf, sizeof(buf), "%d", session->session_handle);
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);

   {
      char *state;
      prompt = "State";
      if (session->is_on_lru) {
        if (session->lru == AVM_PA_LRU_ACTIVE)
           state = "active";
        else if (session->lru == AVM_PA_LRU_DEAD)
           state = "dead";
        else if (session->lru == AVM_PA_LRU_FREE)
           state = "free";
        else
           state = "BAD STATE";
      } else {
        state = "create";
      }
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, state);
   }
   count = atomic_read(&session->skb_in_irqqueue);
   if (count) {
      prompt = "IRQ queue";
      snprintf(buf, sizeof(buf), "%d", count);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   count = atomic_read(&session->skb_in_tbfqueue);
   if (count) {
      prompt = "TBF queue";
      snprintf(buf, sizeof(buf), "%d", count);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
   count = atomic_read(&session->transmit_in_progress);
   if (count) {
      prompt = "TX active";
      snprintf(buf, sizeof(buf), "%d", count);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }

   prompt = "In Pid";
   snprintf(buf, sizeof(buf), "%d (%s)",
            session->ingress_pid_handle,
            PA_PID(ctx, session->ingress_pid_handle)->cfg.name);
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);

   if (session->ingress_vpid_handle) {
      prompt = "In VPid";
      snprintf(buf, sizeof(buf), "%d (%s)",
               session->ingress_vpid_handle,
               PA_VPID(ctx, session->ingress_vpid_handle)->cfg.name);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }

   if (ctx->hardware_pa.add_session) {
      prompt = "In HW";
      snprintf(buf, sizeof(buf), "%s", session->in_hw ? "yes" : "no");
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }

#ifdef CONFIG_GENERIC_CONNTRACK
   if (session->generic_ct) {
      prompt = "CT dir";
      if (session->generic_ct_dir == GENERIC_CT_DIR_ORIGINAL)
         snprintf(buf, sizeof(buf), "original");
      else
         snprintf(buf, sizeof(buf), "reply");
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
#endif

   prompt = "Realtime";
   snprintf(buf, sizeof(buf), "%s", session->realtime ? "yes" : "no");
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);

   pa_show_pkt_match(&session->ingress,
                     session->bsession != 0, session->mod.pkttype,
                     fprintffunc, arg);

   pa_show_mod_rec(&session->mod, fprintffunc, arg);

   prompt = "Hroom";
   snprintf(buf, sizeof(buf), "%u", (unsigned) session->needed_headroom);
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);

   prompt = "Timeout";
   snprintf(buf, sizeof(buf), "%hu", session->timeout/HZ);
   (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);

   for (negress = 0; negress < session->negress; negress++) {
      struct avm_pa_egress *egress = &session->egress[negress];
      prompt = "Egress";
      snprintf(buf, sizeof(buf), "%d", negress);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
      if (egress->pid_handle) {
         prompt = "Out Pid";
         snprintf(buf, sizeof(buf), "%d (%s)",
                  egress->pid_handle,
                  PA_PID(ctx, egress->pid_handle)->cfg.name);
         (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
      }
      if (egress->vpid_handle) {
         prompt = "Out VPid";
         snprintf(buf, sizeof(buf), "%d (%s)",
                  egress->vpid_handle,
                  PA_VPID(ctx, egress->vpid_handle)->cfg.name);
         (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
      }
      prompt = "Mtu";
      snprintf(buf, sizeof(buf), "%u", (unsigned)egress->mtu);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
      if (egress->push_l2_len) {
         prompt = "L2 push";
         data2hex(HDRCOPY(&egress->match), egress->push_l2_len,
                  buf, sizeof(buf));
         (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
         if (egress->pppoe_offset != AVM_PA_OFFSET_NOT_SET) {
            prompt = "PPPoE off";
            snprintf(buf, sizeof(buf), "%u", (unsigned)egress->pppoe_offset);
            (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
            prompt = "PPPoE hlen";
            snprintf(buf, sizeof(buf), "%u", (unsigned)egress->pppoe_hdrlen);
            (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
         }
      }
      if (egress->destmac)
         pa_show_macaddr(egress->destmac, fprintffunc, arg);

      if (egress->not_local) {
         prompt = "Priority";
         snprintf(buf, sizeof(buf), "%hx:%hx",
                                    TC_H_MAJ(egress->output.priority)>>16,
                                    TC_H_MIN(egress->output.priority));
         (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
         prompt = "TC index";
         snprintf(buf, sizeof(buf), "%u", (unsigned)egress->output.tc_index);
         (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
         prompt = "cpmac prio";
         snprintf(buf, sizeof(buf), "%u", (unsigned)egress->output.cpmac_prio);
         (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
      } else {
         prompt = "Dest";
         sprint_symbol(buf, (unsigned long)egress->local.dst->input);
         (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
         if (egress->local.dev) {
            prompt = "Input Dev";
            snprintf(buf, sizeof(buf), "%s", egress->local.dev->name);
            (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
         }
      }
      prompt = "tx pkts";
      snprintf(buf, sizeof(buf), "%lu", (unsigned long)egress->tx_pkts);
      (*fprintffunc)(arg, "%-10s: %s\n", prompt, buf);
   }
}

static void pa_delete_session(struct avm_pa_session *session)
{
   struct avm_pa_global *ctx = &pa_glob;
   const char *why = session->why_killed ? session->why_killed : "???";
   int j;

   if (ctx->dbgsession) {
      pa_printk(KERN_DEBUG, "\navm_pa: delete session: %s\n", why);
      pa_show_session(session, pa_printk, KERN_DEBUG);
   }

   pa_session_lru_delete(session);

#if AVM_PA_TRACE
   if (ctx->dbgtrace) {
      struct avm_pa_pid *pid = PA_PID(ctx, session->ingress_pid_handle);
      pa_printk(KERN_DEBUG, "avm_pa: delete session %d (%s) %s\n",
                            session->session_handle, pid->cfg.name, why);
   }
#endif
   /*
    * pa_kill_session() has
    * - removed session from hash
    * - removed session from hardware pa
    * - removed session from generic connection tracking
    */
   BUG_ON(session->hashed);
   BUG_ON(session->in_hw);
#ifdef CONFIG_GENERIC_CONNTRACK
   BUG_ON(session->generic_ct);
#endif
   for (j = 0; j < session->negress; j++) {
      struct avm_pa_egress *egress = &session->egress[j];
      if (egress->destmac) {
         pa_macaddr_unlink(egress->destmac);
         egress->destmac = 0;
      }
      if (egress->not_local == 0) {
         if (egress->local.dst) {
            dst_release(egress->local.dst);
            egress->local.dst = 0;
         }
      }
   }
   pa_session_lru_update(AVM_PA_LRU_FREE, session);
}

static void pa_kill_session_unlocked(struct avm_pa_session *session, const char *why)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, session->ingress_pid_handle);

#if AVM_PA_TRACE
   if (ctx->dbgtrace) {
      pa_printk(KERN_DEBUG, "avm_pa: kill session %d (%s) %s\n",
                            session->session_handle, pid->cfg.name, why);
   }
#endif
   if (ctx->dbgsession) {
      pa_printk(KERN_DEBUG, "\navm_pa: kill session: %s\n", why);
      pa_show_session(session, pa_printk, KERN_DEBUG);
   }

   pa_session_lru_delete(session);
   pa_session_hash_delete(pid, session);


   if (session->bsession) {
      if (ctx->dbgsession) {
         pa_printk(KERN_DEBUG, "\navm_pa: delete bsession: %s\n", why);
         pa_show_bsession(session->bsession, pa_printk, KERN_DEBUG);
      }
      pa_bsession_delete(pid, session->bsession);
   }

   if (session->in_hw && ctx->hardware_pa.remove_session) {
      (*ctx->hardware_pa.remove_session)(session);
      session->in_hw = 0;
      session->hw_session = 0;
   }

#ifdef CONFIG_GENERIC_CONNTRACK
   if (session->generic_ct) {
      struct generic_ct *ct = session->generic_ct;
      session->generic_ct = 0;
      generic_ct_sessionid_set(ct, session->generic_ct_dir, (void *)0);
      generic_ct_put(ct);
   }
#endif

   session->why_killed = why;
   pa_session_lru_update(AVM_PA_LRU_DEAD, session);
}

static void pa_kill_session(struct avm_pa_session *session,
                                   const char *why)
{
   AVM_PA_LOCK_DECLARE;

   AVM_PA_WRITE_LOCK();
   pa_kill_session_unlocked(session, why);
   AVM_PA_WRITE_UNLOCK();
}

static void pa_session_prioack_check(struct avm_pa_session *session) {
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_egress *egress = &session->egress[0];

   if (egress->tx_pkts > ctx->prioack_thresh_packets) {

      if (ctx->dbgprioack)
         pa_printk(KERN_DEBUG, "pa_session_tcpack_check: %u%% TCP-ACKs (%u Pkts %u ACKs) \n",
            (egress->tcpack_pkts * 100) / egress->tx_pkts,
            egress->tx_pkts, egress->tcpack_pkts);

      if (((egress->tcpack_pkts * 100) / egress->tx_pkts) > ctx->prioack_ratio) {
         if ((egress->output.priority & TC_H_MIN_MASK) > ctx->prioack_priority) {
            if (ctx->dbgprioack)
               pa_printk(KERN_DEBUG, "pa_session_tcpack_check: ACK-Session recognized, change prio %u -> %u\n", 
                         (egress->output.priority & TC_H_MIN_MASK), ctx->prioack_priority);
            egress->output.priority = 
               TC_H_MAKE(TC_H_MAJ(egress->output.priority), ctx->prioack_priority);
         }
      }

      if (!ctx->hw_ppa_disabled && ctx->hardware_pa.add_session
            && ((*ctx->hardware_pa.add_session)(session) == AVM_PA_TX_SESSION_ADDED))
         session->in_hw = 1;

      session->prioack_check = 0;
   }
}


static void pa_session_gc(int force)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session, *next;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_WRITE_LOCK();

   if (force) {
      while ((session = ctx->sess_lru[AVM_PA_LRU_ACTIVE].lru_head) != 0) {
         pa_kill_session_unlocked(session, "disable");
         ctx->stats.sess_flushed++;
      }
   }

   session = ctx->sess_lru[AVM_PA_LRU_DEAD].lru_head;
   while (session) {
      next = session->lru_next;
      if (   atomic_read(&session->skb_in_irqqueue) == 0
          && atomic_read(&session->skb_in_tbfqueue) == 0
          && atomic_read(&session->transmit_in_progress) == 0) {
         pa_delete_session(session);
      }
      session = next;
   }

   session = ctx->sess_lru[AVM_PA_LRU_ACTIVE].lru_head;
   while (session) {
      next = session->lru_next;

      if (time_is_before_eq_jiffies(session->endtime)) {
         pa_kill_session_unlocked(session, "timeout");
         ctx->stats.sess_timedout++;
      } else if (session->prioack_check) {
         pa_session_prioack_check(session);
      }
      session = next;
   }

   AVM_PA_WRITE_UNLOCK();
}
/* ------------------------------------------------------------------------ */

static void pa_kill_sessions_with_destmac(struct avm_pa_macaddr *destmac)
{
   struct avm_pa_global *ctx = &pa_glob;
   int i,j;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_WRITE_LOCK();
   for (i=1; i < CONFIG_AVM_PA_MAX_SESSION; i++) {
      struct avm_pa_session *session = PA_SESSION(ctx, i);
      for (j = 0; j < session->negress; j++) {
         if (session->egress[j].destmac == destmac) {
            pa_kill_session_unlocked(session, "destmac");
            ctx->stats.sess_pidchanged++;
            break;
         }
      }
   }
   AVM_PA_WRITE_UNLOCK();
}

/* ------------------------------------------------------------------------ */

static void pa_gc_timer_expired (unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   pa_session_gc(0);
   if (   ctx->sess_lru[AVM_PA_LRU_ACTIVE].nsessions
       || ctx->sess_lru[AVM_PA_LRU_DEAD].nsessions)
      pa_start_gc_timer();
}

/* ------------------------------------------------------------------------ */

static void pa_show_stats(pa_fprintf fprintffunc, void *arg)
{
   struct avm_pa_global *ctx = &pa_glob;
   (*fprintffunc)(arg, "BSessions      : %u\n",
                       (unsigned)ctx->stats.nbsessions);
   (*fprintffunc)(arg, "Sessions       : %hu\n",
                       ctx->sess_lru[AVM_PA_LRU_ACTIVE].nsessions);
   (*fprintffunc)(arg, "Max Sessions   : %hu\n",
                       ctx->sess_lru[AVM_PA_LRU_ACTIVE].maxsessions);
   (*fprintffunc)(arg, "Sessions (dead): %hu\n",
                       ctx->sess_lru[AVM_PA_LRU_DEAD].nsessions);
   (*fprintffunc)(arg, "Sessions (free): %hu\n",
                       ctx->sess_lru[AVM_PA_LRU_FREE].nsessions);
   (*fprintffunc)(arg, "Rx packets/sec : %lu\n",
                       (unsigned long)ctx->stats.rx_pps);
   (*fprintffunc)(arg, "Fw packets/sec : %lu\n",
                       (unsigned long)ctx->stats.fw_pps);
   (*fprintffunc)(arg, "Ov packets/sec : %lu\n",
                       (unsigned long)ctx->stats.overlimit_pps);
   (*fprintffunc)(arg, "Rx pakets      : %lu\n",
                       (unsigned long)ctx->stats.rx_pkts);
   (*fprintffunc)(arg, "Rx bypass      : %lu\n",
                       (unsigned long)ctx->stats.rx_bypass);
   (*fprintffunc)(arg, "Rx ttl <= 1    : %lu\n",
                       (unsigned long)ctx->stats.rx_ttl);
   (*fprintffunc)(arg, "Rx broadcast   : %lu\n",
                       (unsigned long)ctx->stats.rx_broadcast);
   (*fprintffunc)(arg, "Rx search      : %lu\n",
                       (unsigned long)ctx->stats.rx_search);
   (*fprintffunc)(arg, "Rx match       : %lu\n",
                       (unsigned long)ctx->stats.rx_match);
   (*fprintffunc)(arg, "Rx lisp changed: %lu\n",
                       (unsigned long)ctx->stats.rx_lispchanged);
   (*fprintffunc)(arg, "Rx df          : %lu\n",
                       (unsigned long)ctx->stats.rx_df);
   (*fprintffunc)(arg, "Rx modified    : %lu\n",
                       (unsigned long)ctx->stats.rx_mod);
   (*fprintffunc)(arg, "Rx overlimit   : %lu\n",
                       (unsigned long)ctx->stats.rx_overlimit);
   (*fprintffunc)(arg, "Rx dropped     : %lu\n",
                       (unsigned long)ctx->stats.rx_dropped);
   (*fprintffunc)(arg, "Rx filtered    : %lu\n",
                       (unsigned long)ctx->stats.rx_filtered);
   (*fprintffunc)(arg, "Rx irq         : %lu\n",
                       (unsigned long)ctx->stats.rx_irq);
   (*fprintffunc)(arg, "Rx irq dropped : %lu\n",
                       (unsigned long)ctx->stats.rx_irqdropped);
   (*fprintffunc)(arg, "Rx hroom       : %lu\n",
                       (unsigned long)ctx->stats.rx_headroom_too_small);
   (*fprintffunc)(arg, "Rx hroom fail  : %lu\n",
                       (unsigned long)ctx->stats.rx_realloc_headroom_failed);
   (*fprintffunc)(arg, "Fw pakets      : %lu\n",
                       (unsigned long)ctx->stats.fw_pkts);
   (*fprintffunc)(arg, "Fw local       : %lu\n",
                       (unsigned long)ctx->stats.fw_local);
   (*fprintffunc)(arg, "Fw frags       : %lu\n",
                       (unsigned long)ctx->stats.fw_frags);
   (*fprintffunc)(arg, "Fw drop        : %lu\n",
                       (unsigned long)ctx->stats.fw_drop);
   (*fprintffunc)(arg, "Fw drop gone   : %lu\n",
                       (unsigned long)ctx->stats.fw_drop_gone);
   (*fprintffunc)(arg, "Fw fail        : %lu\n",
                       (unsigned long)ctx->stats.fw_fail);
   (*fprintffunc)(arg, "Fw frag fail   : %lu\n",
                       (unsigned long)ctx->stats.fw_frag_fail);
   (*fprintffunc)(arg, "Tx accelerated : %lu\n",
                       (unsigned long)ctx->stats.tx_accelerated);
   (*fprintffunc)(arg, "Tx local       : %lu\n",
                       (unsigned long)ctx->stats.tx_local);
   (*fprintffunc)(arg, "Tx already     : %lu\n",
                       (unsigned long)ctx->stats.tx_already);
   (*fprintffunc)(arg, "Tx bypass      : %lu\n",
                       (unsigned long)ctx->stats.tx_bypass);
   (*fprintffunc)(arg, "Tx sess error  : %lu\n",
                       (unsigned long)ctx->stats.tx_sess_error);
   (*fprintffunc)(arg, "Tx sess ok     : %lu\n",
                       (unsigned long)ctx->stats.tx_sess_ok);
   (*fprintffunc)(arg, "Tx sess exists : %lu\n",
                       (unsigned long)ctx->stats.tx_sess_exists);
   (*fprintffunc)(arg, "Tx egress error: %lu\n",
                       (unsigned long)ctx->stats.tx_egress_error);
   (*fprintffunc)(arg, "Tx egress ok   : %lu\n",
                       (unsigned long)ctx->stats.tx_egress_ok);

   (*fprintffunc)(arg, "Loc sess error : %lu\n",
                       (unsigned long)ctx->stats.local_sess_error);
   (*fprintffunc)(arg, "Loc sess ok    : %lu\n",
                       (unsigned long)ctx->stats.local_sess_ok);
   (*fprintffunc)(arg, "Loc sess exists: %lu\n",
                       (unsigned long)ctx->stats.local_sess_exists);

   (*fprintffunc)(arg, "TBF schedule   : %lu\n",
                       (unsigned long)ctx->stats.tbf_schedule);
   (*fprintffunc)(arg, "TBF reschedule : %lu\n",
                       (unsigned long)ctx->stats.tbf_reschedule);
   (*fprintffunc)(arg, "sess flushed   : %lu\n",
                       (unsigned long)ctx->stats.sess_flushed);
   (*fprintffunc)(arg, "sess timedout  : %lu\n",
                       (unsigned long)ctx->stats.sess_timedout);
   (*fprintffunc)(arg, "sess pid change: %lu\n",
                       (unsigned long)ctx->stats.sess_pidchanged);

   (*fprintffunc)(arg, "rxch no rx slow: %lu\n",
                       (unsigned long)ctx->stats.rx_channel_no_rx_slow);
   (*fprintffunc)(arg, "rxch stopped   : %lu\n",
                       (unsigned long)ctx->stats.rx_channel_stopped);
   (*fprintffunc)(arg, "txch dropped   : %lu\n",
                       (unsigned long)ctx->stats.tx_channel_dropped);
   (*fprintffunc)(arg, "user msecs/sec : %lu\n",
                       (unsigned long)ctx->stats.userms);
   (*fprintffunc)(arg, "idle msecs/sec : %lu\n",
                       (unsigned long)ctx->stats.idlems);
   (*fprintffunc)(arg, "irq msecs/sec  : %lu\n",
                       (unsigned long)ctx->stats.irqms);
};

/*------------------------------------------------------------------------ */

static void avm_pa_tbf_schedule(psched_time_t wtime)
{
   struct avm_pa_global *ctx = &pa_glob;
   /* we never wait a second */
   ktime_t time = ktime_set(0, 0);
   time = ktime_add_ns(time, PSCHED_TICKS2NS(wtime));
   if (hrtimer_active(&ctx->tbf.timer)) {
      hrtimer_forward_now(&ctx->tbf.timer, time);
      ctx->stats.tbf_reschedule++;
   } else {
      hrtimer_start(&ctx->tbf.timer, time, HRTIMER_MODE_REL);
      ctx->stats.tbf_schedule++;
   }
}

static int avm_pa_tbf_tx_ok(u32 wanted)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_tbf *q = &ctx->tbf;
   psched_time_t now;
   long toks;
   long ptoks;
   long pkttime = q->pkttime;
   u32 count = 0;

   now = psched_get_time();
   toks = psched_tdiff_bounded(now, q->t_c, q->buffer);
   // toks = now - q->t_c;

   ptoks = toks + q->ptokens;
   if (ptoks > (long)q->pbuffer)
      ptoks = q->pbuffer;

   toks += q->tokens;
   if (toks > (long)q->buffer)
      toks = q->buffer;

   while (   count < wanted
          && ((toks - pkttime) >= 0 || (ptoks - pkttime) >= 0)) {
      ptoks -= pkttime;
      toks -= pkttime;
      count++;
   }

   if (count) {
      q->t_c = now;
      q->tokens = toks;
      q->ptokens = ptoks;
      return count;
   }
   avm_pa_tbf_schedule(max_t(long, -toks, -ptoks));
   return 0;
}

static inline u32 calc_xmittime(unsigned rate, unsigned size)
{
   u64 x64 = NSEC_PER_SEC*(u64)size;
   do_div(x64, rate);
   return (u32)(PSCHED_NS2TICKS((u32)x64));
}

static void avm_pa_tbf_reset(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_tbf *q = &ctx->tbf;
   q->t_c = psched_get_time();
   q->tokens = q->buffer;
   q->ptokens = q->pbuffer;
}

static void avm_pa_tbf_disable(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   ctx->tbf_enabled = 0;
   avm_pa_tbf_reset();
   if (skb_queue_len(&ctx->tbfqueue))
      tasklet_hi_schedule(&ctx->tbftasklet);
}

static void avm_pa_tbf_update(u32 rate, unsigned buffer, unsigned peak)
{
   struct avm_pa_global *ctx = &pa_glob;
   ctx->tbf.buffer = calc_xmittime(rate, buffer);
   ctx->tbf.pbuffer = calc_xmittime(rate, peak);
   ctx->tbf.pkttime = calc_xmittime(rate, 1);
}

static enum hrtimer_restart avm_pa_tbf_restart(struct hrtimer *timer)
{
   struct avm_pa_global *ctx = &pa_glob;
   tasklet_hi_schedule(&ctx->tbftasklet);
   return HRTIMER_NORESTART;
}

static void avm_pa_tbf_init(u32 rate, unsigned buffer, unsigned peak)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct hrtimer *timer = &ctx->tbf.timer;
   if (!hrtimer_active(timer)) {
      hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
      timer->function = avm_pa_tbf_restart;
   }
   avm_pa_tbf_update(rate, buffer, peak);
   avm_pa_tbf_reset();
} 

static void avm_pa_tbf_exit(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct hrtimer *timer = &ctx->tbf.timer;
   hrtimer_cancel(timer);
}

static void avm_pa_tbf_tasklet(unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session;
   struct sk_buff *skb;

   if (ctx->tbf_enabled) {
      u32 len;
      if ((len = skb_queue_len(&ctx->tbfqueue)) > 0) {
         len = avm_pa_tbf_tx_ok(len);
         while (len--) {
            skb = skb_dequeue(&ctx->tbfqueue);
            session = pa_session_get(AVM_PKT_INFO(skb)->session_handle);
            BUG_ON(session == 0);
            if (session) {
               atomic_dec(&session->skb_in_tbfqueue);
               pa_do_modify_and_send(session, skb);
            } else {
               ctx->stats.fw_drop_gone++;
               PKT_FREE(skb);
            }
         }
      }
   } else {
      while ((skb = skb_dequeue(&ctx->tbfqueue)) != 0) {
         session = pa_session_get(AVM_PKT_INFO(skb)->session_handle);
         BUG_ON(session == 0);
         if (session) {
            atomic_dec(&session->skb_in_tbfqueue);
            pa_do_modify_and_send(session, skb);
         } else {
            ctx->stats.fw_drop_gone++;
            PKT_FREE(skb);
         }
      }
   }
}

static inline void avm_pa_tbf_transmit(struct avm_pa_session *session, PKT *pkt)
{
   struct avm_pa_global *ctx = &pa_glob;

   if (session->realtime) {
      pa_do_modify_and_send(session, pkt);
      return;
   }

   if (   skb_queue_len(&ctx->tbfqueue) == 0 
       && (ctx->tbf_enabled == 0 || avm_pa_tbf_tx_ok(1))) {
      pa_do_modify_and_send(session, pkt);
      return;
   }

   AVM_PKT_INFO(pkt)->session_handle = session->session_handle;
   skb_queue_tail(&ctx->tbfqueue, pkt);
   atomic_inc(&session->skb_in_tbfqueue);
   if (ctx->tbf_enabled) {
      ctx->stats.rx_overlimit++;
      if (skb_queue_len(&ctx->tbfqueue) > AVM_PA_MAX_TBF_QUEUE_LEN) {
         if ((pkt = skb_dequeue(&ctx->tbfqueue)) != 0) {
            session = pa_session_get(AVM_PKT_INFO(pkt)->session_handle);
            BUG_ON(session == 0);
            if (session)
               atomic_dec(&session->skb_in_tbfqueue);
            PKT_FREE(pkt);
            ctx->stats.rx_dropped++;
         }
      }
   }
   if (!hrtimer_active(&ctx->tbf.timer))
      tasklet_hi_schedule(&ctx->tbftasklet);
}

/* ------------------------------------------------------------------------ */

#define MAX_TASKLET_PACKETS 32
static void avm_pa_irq_tasklet(unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   int count = MAX_TASKLET_PACKETS;
   struct sk_buff *skb;

   while (count-- > 0 && (skb = skb_dequeue(&ctx->irqqueue)) != 0) {
      struct avm_pa_session *session;
      session = pa_session_get(AVM_PKT_INFO(skb)->session_handle);
      BUG_ON(session == 0);
      if (session) {
         atomic_dec(&session->skb_in_irqqueue);
         avm_pa_tbf_transmit(session, skb);
      } else {
         ctx->stats.fw_drop_gone++;
         PKT_FREE(skb);
      }
   }
   if (skb_queue_len(&ctx->irqqueue))
      tasklet_schedule(&ctx->irqtasklet);
}
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

void avm_pa_rx_channel_suspend(avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
   pid->rx_channel_stopped = 1;
}
EXPORT_SYMBOL(avm_pa_rx_channel_suspend);

void avm_pa_rx_channel_resume(avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
   pid->rx_channel_stopped = 0;
}
EXPORT_SYMBOL(avm_pa_rx_channel_resume);

void avm_pa_rx_channel_packet_not_accelerated(avm_pid_handle pid_handle,
                                           struct sk_buff *skb)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);

   if (avm_pa_pid_receive(pid_handle, skb) == AVM_PA_RX_ACCELERATED)
      return;

   if (likely(pid && pid->ecfg.rx_slow)) {
      (*pid->ecfg.rx_slow)(pid->ecfg.rx_slow_arg, skb);
      return;
   }
   PKT_FREE(skb);
   ctx->stats.rx_channel_no_rx_slow++;
}
EXPORT_SYMBOL(avm_pa_rx_channel_packet_not_accelerated);

void avm_pa_tx_channel_accelerated_packet(avm_pid_handle pid_handle,
                                          avm_session_handle session_handle,
                                          struct sk_buff *skb)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session = pa_session_get(session_handle);
   if (session) {
      int i;
      for (i = 0; i < session->negress; i++) {
         struct avm_pa_egress *egress = &session->egress[i];
         if (egress->pid_handle == pid_handle) {
            skb_reset_mac_header(skb);
            skb_pull(skb, ETH_HLEN);
            AVM_PKT_INFO(skb)->already_modified = 1;
            AVM_PKT_INFO(skb)->egress_offset = i;
            avm_pa_tbf_transmit(session, skb);
            return;
         }
      }
   }
   PKT_FREE(skb);
   ctx->stats.tx_channel_dropped++;
}
EXPORT_SYMBOL(avm_pa_tx_channel_accelerated_packet);

/* ------------------------------------------------------------------------ */
/* -------- exported functions -------------------------------------------- */
/* ------------------------------------------------------------------------ */

void avm_pa_get_stats(struct avm_pa_stats *stats)
{
   struct avm_pa_global *ctx = &pa_glob;
   memcpy(stats, &ctx->stats, sizeof(struct avm_pa_stats));
}
EXPORT_SYMBOL(avm_pa_get_stats);

void avm_pa_reset_stats(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   memset(&ctx->stats, 0, sizeof(struct avm_pa_stats));
}
EXPORT_SYMBOL(avm_pa_reset_stats);

void avm_pa_dev_init(struct avm_pa_dev_info *devinfo)
{
   memset(devinfo, 0, sizeof(struct avm_pa_dev_info));
}
EXPORT_SYMBOL(avm_pa_dev_init);

static int avm_pa_pid_receive(avm_pid_handle pid_handle, PKT *pkt)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
   struct avm_pa_pkt_info *info;
   struct avm_pa_session *session;
   struct ethhdr *ethh;
   unsigned int hstart;
   int rc;

   if (ctx->disabled)
      return AVM_PA_RX_OK;

   info = AVM_PKT_INFO(pkt);
   if (info->ingress_pid_handle)
      return AVM_PA_RX_OK;

   ctx->stats.rx_pkts++;

   if ((ethh = pa_get_ethhdr(pid->ingress_framing, pkt)) != 0) {
      if ((session = pa_bsession_search(pid, ethh_hash(ethh), ethh)) != 0)
         goto accelerate;
      if ((pid->ecfg.flags & AVM_PA_PID_FLAG_NO_PID_CHANGED_CHECK) == 0)
         pa_check_and_handle_ingress_pid_change(ethh->h_source, pid_handle);
   }

   info->ingress_pid_handle = pid_handle;
   info->ingress_vpid_handle = 0;
   info->egress_vpid_handle = 0;
   info->can_be_accelerated = 0;
   info->is_accelerated = 0;
   info->routed = 0;
   info->session_handle = 0;
   if (pid->ecfg.flags & AVM_PA_PID_FLAG_HSTART_ON_INGRESS)
      hstart = AVM_PKT_INFO(pkt)->hstart;
   else
      hstart = 0;
   rc = pa_set_pkt_match(pid->ingress_framing, hstart,
                         pkt, &info->match, 0);

   if (rc == AVM_PA_RX_OK) {
      info->can_be_accelerated = 1;
      ctx->stats.rx_search++;
      if ((session = pa_session_search(pid, &info->match)) == 0) {
         info->ingress_pid_handle = pid_handle;
#if AVM_PA_TRACE
         if (ctx->dbgtrace) {
            pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_receive(%s) - %s\n",
                                  pkt->uniq_id & 0xffffff, pid->cfg.name,
                                  "no session");
            if (ctx->dbgnosession) {
               char buf[64];
               data2hex(PKT_DATA(pkt), PKT_LEN(pkt), buf, sizeof(buf));
               pa_printk(KERN_DEBUG, "%-10s: %s\n", "Data", buf);
               pa_show_pkt_info(info, pa_printk, KERN_DEBUG);
            }
         }
#endif
         return AVM_PA_RX_OK;
      }

      if (info->match.pkttype & AVM_PA_PKTTYPE_LISP) {
         void *slhdr = LISPDATAHDR(&session->ingress);
         void *ilhdr = LISPDATAHDR(&info->match);
         if (memcmp(slhdr, ilhdr, LISP_DATAHDR_SIZE) != 0) {
            pa_kill_session(session, "lisp data header changed");
            ctx->stats.rx_lispchanged++;
            return AVM_PA_RX_OK;
         }
      }

      ctx->stats.rx_match++;

      if (session->egress[0].pid_handle == 0) {
         if (session->ingress_vpid_handle) {
            struct avm_pa_vpid *vpid = PA_VPID(ctx, session->ingress_vpid_handle);
            ((u32 *)(&vpid->stats.rx_unicast_pkt))[session->ingress.casttype]++;
            vpid->stats.rx_bytes += PKT_LEN(pkt);
         }
         ctx->stats.rx_filtered++;
         PKT_FREE(pkt);
         pa_session_update(session);
         return AVM_PA_RX_ACCELERATED;
      }


      if (pa_egress_size_check(session, pkt) < 0) {
         ctx->stats.rx_df++;
         info->ingress_pid_handle = pid_handle;
#if AVM_PA_TRACE
         if (ctx->dbgtrace)
            pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_receive(%s) - %s\n",
                  pkt->uniq_id & 0xffffff, pid->cfg.name,
                  "size problem");
#endif
         return AVM_PA_RX_OK;
      }
      if (info->match.fin) {
         session->timeout = ctx->fin_timeout_secs*HZ;
         if (session->timeout == 0)
            pa_kill_session(session, "fin");
         return AVM_PA_RX_OK;
      }

accelerate:
      pa_session_update(session);

      if (ctx->fw_disabled || ctx->misc_is_open) {
         if (session->timeout == 0)
            pa_kill_session(session, "fast timeout");
#if AVM_PA_TRACE
         if (ctx->dbgtrace)
            pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_receive(%s) - %s\n",
                  pkt->uniq_id & 0xffffff, pid->cfg.name,
                  "forward disabled");
#endif
         return AVM_PA_RX_OK;
      }

      if (pid->ingress_framing == avm_pa_framing_dev)
         PKT_PUSH(pkt, PKT_DATA(pkt) - skb_mac_header(pkt));

      if (skb_headroom(pkt) < session->needed_headroom) {
         struct sk_buff *npkt;
         if (net_ratelimit())
            printk(KERN_ERR "avm_pa: pid %u (%s): headroom %u < %u\n", 
                  pid_handle, pid->cfg.name,
                  skb_headroom(pkt),
                  (unsigned)session->needed_headroom);
         ctx->stats.rx_headroom_too_small++;
         npkt = skb_realloc_headroom(pkt, session->needed_headroom);
         if (npkt == 0) {
            if (net_ratelimit())
               printk(KERN_ERR "avm_pa: pid %u (%s): skb_realloc_headroom(%u) failed\n", 
                     pid_handle, pid->cfg.name,
                     (unsigned)session->needed_headroom);
            ctx->stats.rx_realloc_headroom_failed++;
            /* go slow path */
            return AVM_PA_RX_OK;
         } else {
            kfree_skb(pkt);
            pkt = npkt;
         }
      }

#if AVM_PA_TRACE
      if (ctx->dbgtrace)
         pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_receive(%s) - %s\n",
               pkt->uniq_id & 0xffffff, pid->cfg.name,
               "accelerated");
#endif

      if (in_irq() || irqs_disabled()) {
         if (skb_queue_len(&ctx->irqqueue) > AVM_PA_MAX_IRQ_QUEUE_LEN) {
            ctx->stats.rx_irqdropped++;
            PKT_FREE(pkt);
         } else {
            info = AVM_PKT_INFO(pkt);
            info->session_handle = session->session_handle;
            skb_queue_tail(&ctx->irqqueue, pkt);
            atomic_inc(&session->skb_in_irqqueue);
            ctx->stats.rx_irq++;
            tasklet_schedule(&ctx->irqtasklet);
         }
      } else {
         avm_pa_tbf_transmit(session, pkt);
      }
      return AVM_PA_RX_ACCELERATED;
   }

   if (ctx->dbgmatch) {
      char buf[64];
      pa_printk(KERN_DEBUG, "---------->\n");
      pa_printk(KERN_DEBUG, "%-10s: %d %s\n", "RC", rc, rc2str(rc));
      data2hex(PKT_DATA(pkt), PKT_LEN(pkt), buf, sizeof(buf));
      pa_printk(KERN_DEBUG, "%-10s: %s\n", "Data", buf);
      pa_show_pkt_info(info, pa_printk, KERN_DEBUG);
      pa_printk(KERN_DEBUG, "<----------\n");
   }
   pa_reset_match(&info->match);
   switch (rc) {
      case AVM_PA_RX_TTL:
         ctx->stats.rx_ttl++;
         break;
      case AVM_PA_RX_BROADCAST:
         ctx->stats.rx_broadcast++;
         break;
      default:
         ctx->stats.rx_bypass++;
         break;
   }
#if AVM_PA_TRACE
   if (ctx->dbgtrace)
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_receive(%s) - %s (rc %d)\n",
                            pkt->uniq_id & 0xffffff, pid->cfg.name,
                            "bypass", rc);
#endif
   return rc;
}

static inline void avm_pa_vpid_snoop_receive(avm_vpid_handle handle, PKT *pkt)
{
#if AVM_PA_TRACE
   struct avm_pa_global *ctx = &pa_glob;
   if (ctx->dbgtrace) {
      struct avm_pa_vpid *vpid = PA_VPID(ctx, handle);
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_vpid_snoop_receive(%s)\n",
                            pkt->uniq_id & 0xffffff, vpid->cfg.name);
   }
#endif
   AVM_PKT_INFO(pkt)->ingress_vpid_handle = handle;
}


int avm_pa_dev_receive(struct avm_pa_dev_info *devinfo, PKT *pkt)
{
   int rc = AVM_PA_RX_OK;
   if (devinfo->pid_handle) {
      rc = avm_pa_pid_receive(devinfo->pid_handle, pkt);
      if (rc == AVM_PA_RX_ACCELERATED)
         return rc;
   }
   if (devinfo->vpid_handle) 
      avm_pa_vpid_snoop_receive(devinfo->vpid_handle, pkt);
   return rc;
}
EXPORT_SYMBOL(avm_pa_dev_receive);

int avm_pa_dev_pid_receive(struct avm_pa_dev_info *devinfo, PKT *pkt)
{
   struct avm_pa_global *ctx = &pa_glob;
   int rc = AVM_PA_RX_OK;
   if (devinfo->pid_handle) {
      struct avm_hardware_pa *hwpa = &ctx->hardware_pa;
      if (!ctx->hw_ppa_disabled && hwpa && hwpa->try_to_accelerate) {
         struct avm_pa_pid *pid = PA_PID(ctx, devinfo->pid_handle);
         if (pid->rx_channel_activated) {
            if (pid->rx_channel_stopped == 0) {
               if ((*hwpa->try_to_accelerate)(devinfo->pid_handle, pkt) <= 0)
                  return AVM_PA_RX_STOLEN;
            } else {
               ctx->stats.rx_channel_stopped++;
            }
         }
      }
      rc = avm_pa_pid_receive(devinfo->pid_handle, pkt);
   }
   return rc;
}
EXPORT_SYMBOL(avm_pa_dev_pid_receive);

void avm_pa_dev_vpid_snoop_receive(struct avm_pa_dev_info *devinfo, PKT *pkt)
{
   if (devinfo->vpid_handle) 
      avm_pa_vpid_snoop_receive(devinfo->vpid_handle, pkt);
}
EXPORT_SYMBOL(avm_pa_dev_vpid_snoop_receive);

void avm_pa_mark_routed(PKT *pkt)
{
   AVM_PKT_INFO(pkt)->routed = 1;
#if AVM_PA_TRACE
   if (pa_glob.dbgtrace)
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_mark_routed (ingress %d)\n",
                            pkt->uniq_id & 0xffffff,
                            AVM_PKT_INFO(pkt)->ingress_pid_handle);
#endif
}
EXPORT_SYMBOL(avm_pa_mark_routed);

void avm_pa_use_protocol_specific_session(PKT *pkt)
{
   AVM_PKT_INFO(pkt)->use_protocol_specific = 1;
#if AVM_PA_TRACE
   if (pa_glob.dbgtrace)
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_use_protocol_specific_session (ingress %d)\n",
                            pkt->uniq_id & 0xffffff, 
                            AVM_PKT_INFO(pkt)->ingress_pid_handle);
#endif
}
EXPORT_SYMBOL(avm_pa_use_protocol_specific_session);

void avm_pa_do_not_accelerate(PKT *pkt)
{
   AVM_PKT_INFO(pkt)->can_be_accelerated = 0;
#if AVM_PA_TRACE
   if (pa_glob.dbgtrace)
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_do_not_accelerate\n",
                            pkt->uniq_id & 0xffffff);
#endif
}
EXPORT_SYMBOL(avm_pa_do_not_accelerate);

void avm_pa_set_hstart(PKT *pkt, unsigned int hstart)
{
   AVM_PKT_INFO(pkt)->hstart = hstart;
}
EXPORT_SYMBOL(avm_pa_set_hstart);

static inline void avm_pa_vpid_snoop_transmit(avm_vpid_handle handle, PKT *pkt)
{
   struct avm_pa_pkt_info *info = AVM_PKT_INFO(pkt);
   if (info->egress_vpid_handle == 0)
      info->egress_vpid_handle = handle;
#if AVM_PA_TRACE
   if (pa_glob.dbgtrace) {
      struct avm_pa_global *ctx = &pa_glob;
      struct avm_pa_vpid *vpid = PA_VPID(ctx, handle);
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_vpid_snoop_transmit(%s)\n",
                            pkt->uniq_id & 0xffffff, vpid->cfg.name);
   }
#endif
}

static inline int avm_pa_sock_is_realtime(struct sock *sk)
{
   return sk->sk_protocol == IPPROTO_UDP && sk->sk_tc_index != 0;
}

static inline int avm_pa_pid_snoop_transmit(avm_pid_handle pid_handle,
                                            PKT *pkt, struct sock *sk)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pkt_info *info = AVM_PKT_INFO(pkt);
   struct avm_pa_session *session;
   struct avm_pa_egress *egress;
   struct avm_pa_pkt_match match;
   struct avm_pa_pid *ipid, *epid;
   struct avm_pa_vpid *evpid;
   struct ethhdr *ethh;
   unsigned negress;
   int headroom;
   char buf[64];
   u32 hash = 0;

#if AVM_PA_TRACE
   if (ctx->dbgtrace) {
      epid = PA_PID(ctx, pid_handle);
      pa_printk(KERN_DEBUG, "avm_pa: %lu - avm_pa_pid_snoop_transmit(%s)\n",
                            pkt->uniq_id & 0xffffff, epid->cfg.name);
   }
#endif

   if (ctx->disabled)
      return AVM_PA_TX_OK;

   if (ctx->prioack_enabled && !sk) {
      if (info->match.syn || info->match.fin 
         || (info->match.ack && PKT_LEN(pkt) < ctx->prioack_packet_size)) {
         ctx->prioack_acks++;
         /* change TCP-Ack priority */
         if ((pkt->priority & TC_H_MIN_MASK) > ctx->prioack_priority) {
            pkt->priority = TC_H_MAKE(TC_H_MAJ(pkt->priority), ctx->prioack_priority);
            ctx->prioack_accl_acks++;
         } 
      }
   }

   if (!info->can_be_accelerated) {
      ctx->stats.tx_bypass++;
      return AVM_PA_TX_BYPASS;
   }

   if (sk) { /* input to local system */

      epid = PA_PID(ctx, pid_handle);
      ipid = PA_PID(ctx, info->ingress_pid_handle);
      ethh = 0;

   } else { /* forwarded or bridged */

      if (info->is_accelerated) {
         ctx->stats.tx_accelerated++;
         return AVM_PA_TX_BYPASS;
      }
      if (info->ingress_pid_handle == 0) {
         ctx->stats.tx_local++;
         return AVM_PA_TX_BYPASS;
      }
      if (info->session_handle != 0) {
         ctx->stats.tx_already++;
         return AVM_PA_TX_BYPASS;
      }
      epid = PA_PID(ctx, pid_handle);
      ipid = PA_PID(ctx, info->ingress_pid_handle);

      if ((ethh = pa_get_ethhdr(epid->egress_framing, pkt)) != 0) {
         hash = ethh_hash(ethh);
         if ((session = pa_bsession_search(ipid, hash, ethh)) != 0)
            return AVM_PA_TX_SESSION_EXISTS;
      }
   }

   if (info->match.syn || info->match.fin) {
      ctx->stats.tx_bypass++;
      if (ctx->dbgnosession) {
         pa_printk(KERN_DEBUG, "%-10s: %s\n", "Syn/Fin", "yes");
         data2hex(PKT_DATA(pkt), PKT_LEN(pkt), buf, sizeof(buf));
         pa_printk(KERN_DEBUG, "%-10s: %s\n", "Data", buf);
         pa_show_pkt_info(info, pa_printk, KERN_DEBUG);
      }
      return AVM_PA_TX_BYPASS;
   }

   if (pa_egress_precheck(epid, pkt, &info->match, &match) < 0) {
      ctx->stats.tx_bypass++;
      if (ctx->dbgnosession) {
         pa_printk(KERN_DEBUG, "%-10s: %s\n", "Precheck", "failed");
         data2hex(PKT_DATA(pkt), PKT_LEN(pkt), buf, sizeof(buf));
         pa_printk(KERN_DEBUG, "%-10s: %s\n", "Data", buf);
         pa_show_pkt_info(info, pa_printk, KERN_DEBUG);
         pa_show_pkt_match(&match, 0, 0, pa_printk, KERN_DEBUG);
      }
      return AVM_PA_TX_BYPASS;
   }

   if ((session = pa_session_search(ipid, &info->match)) == 0) {
      if ((session = pa_session_alloc(&info->match)) == 0) {
         pa_session_gc(0); /* try to get space for the new session */
         if ((session = pa_session_alloc(&info->match)) == 0) {
            if (sk) ctx->stats.local_sess_error++;
            else ctx->stats.tx_sess_error++;
            return AVM_PA_TX_ERROR_SESSION;
         }
      }
      /* Session State: CREATE */
      session->ingress_pid_handle = info->ingress_pid_handle;
      session->ingress_vpid_handle = info->ingress_vpid_handle;
      session->routed = info->routed ? 1 : 0;
      session->negress = 0;
      session->bsession = 0;
      egress = &session->egress[session->negress++];
      egress->pid_handle = pid_handle;
      egress->vpid_handle = info->egress_vpid_handle;
      egress->match = match;
      if (sk) {
         if (avm_pa_sock_is_realtime(sk))
            session->realtime = 1;
         egress->not_local = 0;
         egress->local.dev = pkt->dev;
         egress->local.dst = dst_clone(skb_dst(pkt));
      } else {
         egress->not_local = 1;
         egress->output.priority = pkt->priority;
         egress->output.tc_index = pkt->tc_index;
         egress->output.cpmac_prio = (u8)(pkt->uniq_id >> 24);
         if (epid->ecfg.cb_len) {
            memcpy(egress->output.cb,
                   &pkt->cb[epid->ecfg.cb_start], epid->ecfg.cb_len);
         }
      }
      if (ethh)
         egress->destmac = pa_macaddr_link(ethh->h_dest, pid_handle);
      if (   ethh
          && info->routed == 0
          && info->use_protocol_specific == 0
          && pa_match_cmp(&info->match, &match) == 0) {
         session->timeout = ctx->bridge_timeout_secs*HZ;
         session->bsession = pa_bsession_alloc(hash, ethh,
                                               session->session_handle);
         pa_change_to_bridge_match(&session->ingress);
         session->mod.protocol = ethh->h_proto;
         egress->pppoe_offset = AVM_PA_OFFSET_NOT_SET;
         egress->push_l2_len = 0;
         egress->mtu = 0xffff;
         pa_bsession_add(ipid, session->bsession);
      } else {
         (void)pa_calc_modify(session, &info->match, &match);
         if (match.encap_offset == AVM_PA_OFFSET_NOT_SET)
            egress->push_l2_len = match.ip_offset;
         else egress->push_l2_len = match.encap_offset;
         headroom =   (session->mod.push_encap_len + egress->push_l2_len)
                    - (session->mod.pull_l2_len + session->mod.pull_encap_len);
         if (headroom > 0 && headroom > session->needed_headroom)
            session->needed_headroom = headroom;
         egress->pppoe_offset = match.pppoe_offset;
         if (egress->pppoe_offset != AVM_PA_OFFSET_NOT_SET)
            egress->pppoe_hdrlen = egress->pppoe_offset + sizeof(struct pppoehdr);
         egress->mtu = epid->cfg.default_mtu;
         if (egress->vpid_handle) {
            evpid = PA_VPID(ctx, egress->vpid_handle);
            if (session->mod.protocol == constant_htons(ETH_P_IP)) {
               if (evpid->cfg.v4_mtu < egress->mtu)
                  egress->mtu = evpid->cfg.v4_mtu;
            } else if (session->mod.protocol == constant_htons(ETH_P_IPV6)) {
               if (evpid->cfg.v6_mtu < egress->mtu)
                  egress->mtu = evpid->cfg.v6_mtu;
            }
         }
      }
#if AVM_PA_TRACE
      if (ctx->dbgtrace) {
         pa_printk(KERN_DEBUG, "avm_pa: add session %d (%s)\n",
                                session->session_handle, ipid->cfg.name);
      }
#endif
      if (ctx->dbgsession) {
         pa_printk(KERN_DEBUG, "\navm_pa: new session:\n");
         pa_show_session(session, pa_printk, KERN_DEBUG);
      }
#ifdef CONFIG_GENERIC_CONNTRACK
      if (pkt->generic_ct) {
         session->generic_ct = generic_ct_get(pkt->generic_ct);
         session->generic_ct_dir = skb_get_ct_dir(pkt);
         generic_ct_sessionid_set(session->generic_ct,
                                  session->generic_ct_dir,
                                  (void *)(unsigned long)(session->session_handle));
      }
#endif
      if (!ctx->hw_ppa_disabled && ctx->hardware_pa.add_session) {
         if (ctx->prioack_enabled && egress->not_local 
               && (session->mod.pkttype & AVM_PA_PKTTYPE_PROTO_MASK) == IPPROTO_TCP) {
            session->prioack_check = 1; /* add to HW after TCP-ACK-Check */
         } else if ((*ctx->hardware_pa.add_session)(session) == AVM_PA_TX_SESSION_ADDED) {
            session->in_hw = 1;
         }
      }
      pa_session_activate(session);
      /* Session State: ACTIVE */
      if (sk) ctx->stats.local_sess_ok++;
      else ctx->stats.tx_sess_ok++;
      info->session_handle = session->session_handle;
      return AVM_PA_TX_SESSION_ADDED;
   }
   info->session_handle = session->session_handle;
   for (negress = 0; negress < session->negress; negress++) {
      egress = &session->egress[negress];
      if (   egress->pid_handle == pid_handle
          && egress->vpid_handle == info->egress_vpid_handle
          && pa_match_cmp(&egress->match, &match) == 0) {
         if (sk) {
            ctx->stats.local_sess_exists++;
         } else {
            ctx->stats.tx_sess_exists++;
         }
         pa_session_update(session);
         return AVM_PA_TX_SESSION_EXISTS;
      }
   }
   if (session->negress < AVM_PA_MAX_EGRESS) {
      egress = &session->egress[session->negress++];
      egress->pid_handle = pid_handle;
      egress->vpid_handle = info->egress_vpid_handle;
      egress->match = match;
      if (sk) {
         if (avm_pa_sock_is_realtime(sk))
            session->realtime = 1;
         egress->not_local = 0;
         egress->local.dev = pkt->dev;
         egress->local.dst = dst_clone(skb_dst(pkt));
      } else {
         egress->not_local = 1;
         egress->output.priority = pkt->priority;
         egress->output.tc_index = pkt->tc_index;
         egress->output.cpmac_prio = (u8)(pkt->uniq_id >> 24);
      }
      if (ethh)
         egress->destmac = pa_macaddr_link(ethh->h_dest, pid_handle);
      egress->mtu = epid->cfg.default_mtu;
      if (egress->vpid_handle) {
         evpid = PA_VPID(ctx, egress->vpid_handle);
         if (session->mod.protocol == constant_htons(ETH_P_IP)) {
            if (evpid->cfg.v4_mtu < egress->mtu)
               egress->mtu = evpid->cfg.v4_mtu;
         } else if (session->mod.protocol == constant_htons(ETH_P_IPV6)) {
            if (evpid->cfg.v6_mtu < egress->mtu)
               egress->mtu = evpid->cfg.v6_mtu;
         }
      }
      if (match.encap_offset == AVM_PA_OFFSET_NOT_SET)
         egress->push_l2_len = match.ip_offset;
      else egress->push_l2_len = match.encap_offset;
      headroom =   (session->mod.push_encap_len + egress->push_l2_len)
                 - (session->mod.pull_l2_len + session->mod.pull_encap_len);
      if (headroom > 0 && headroom > session->needed_headroom)
         session->needed_headroom = headroom;
      if (session->in_hw) {
         if (ctx->hardware_pa.change_session) {
            if ((*ctx->hardware_pa.change_session)(session) != AVM_PA_TX_EGRESS_ADDED)
               session->in_hw = 0;
         } else {
            (*ctx->hardware_pa.remove_session)(session);
            session->in_hw = 0;
         }
      }
      ctx->stats.tx_egress_ok++;
      if (ctx->dbgsession) {
         pa_printk(KERN_DEBUG, "\navm_pa: new egress:\n");
         pa_show_session(session, pa_printk, KERN_DEBUG);
      }
      return AVM_PA_TX_EGRESS_ADDED;
   }
   ctx->stats.tx_egress_error++;
   return AVM_PA_TX_ERROR_EGRESS;
}

int avm_pa_dev_snoop_transmit(struct avm_pa_dev_info *devinfo, PKT *pkt)
{
   if (devinfo->vpid_handle) 
      avm_pa_vpid_snoop_transmit(devinfo->vpid_handle, pkt);
   if (devinfo->pid_handle)
      return avm_pa_pid_snoop_transmit(devinfo->pid_handle, pkt, 0);
   return AVM_PA_TX_OK;
}
EXPORT_SYMBOL(avm_pa_dev_snoop_transmit);

void avm_pa_dev_vpid_snoop_transmit(struct avm_pa_dev_info *devinfo, PKT *pkt)
{
   if (devinfo->vpid_handle) 
      avm_pa_vpid_snoop_transmit(devinfo->vpid_handle, pkt);
}
EXPORT_SYMBOL(avm_pa_dev_vpid_snoop_transmit);

void _avm_pa_add_local_session(PKT *pkt, struct sock *sk)
{
   (void)avm_pa_pid_snoop_transmit(AVM_PKT_INFO(pkt)->ptype_pid_handle, pkt, sk);
}
EXPORT_SYMBOL(_avm_pa_add_local_session);

void avm_pa_filter_packet(PKT *pkt)
{
   (void)avm_pa_pid_snoop_transmit(AVM_PKT_INFO(pkt)->ptype_pid_handle, pkt, 0);
}
EXPORT_SYMBOL(avm_pa_filter_packet);

int avm_pa_dev_pidhandle_register_with_ingress(struct avm_pa_dev_info *devinfo,
                                               avm_pid_handle pid_handle,
                                               struct avm_pa_pid_cfg *cfg,
                                               avm_pid_handle ingress_pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   avm_pid_handle n;

   if (devinfo->pid_handle) {
      if (pid_handle && devinfo->pid_handle != pid_handle)
         return -1;
      n = devinfo->pid_handle;
      goto slot_found;
   }
   if (pid_handle) {
      n = pid_handle;
      goto slot_found;
   }
   for (n=1; n < CONFIG_AVM_PA_MAX_PID; n++) {
      if (strncmp(cfg->name, PA_PID(ctx, n)->cfg.name, AVM_PA_MAX_NAME) == 0)
         goto slot_found;
   }
   for (n=1; n < CONFIG_AVM_PA_MAX_PID; n++) {
      if (PA_PID(ctx, n)->pid_handle == 0)
         goto slot_found;
   }
   return -1;

slot_found:
   if (ingress_pid_handle) {
      if (PA_PID(ctx, ingress_pid_handle)->pid_handle != ingress_pid_handle)
         return -1;
      PA_PID(ctx, n)->ingress_pid_handle = ingress_pid_handle;
   } else {
      PA_PID(ctx, n)->ingress_pid_handle = n;
   }
   if (cfg->default_mtu == 0)
      cfg->default_mtu = 1500;
   PA_PID(ctx, n)->pid_handle = n;
   PA_PID(ctx, n)->cfg = *cfg;
   memset(&PA_PID(ctx, n)->ecfg, 0, sizeof(PA_PID(ctx, n)->ecfg));
   PA_PID(ctx, n)->ingress_framing = cfg->framing;
   switch (cfg->framing) {
      case avm_pa_framing_llcsnap:
      case avm_pa_framing_ether:
      case avm_pa_framing_ppp:
      case avm_pa_framing_ip:
         PA_PID(ctx, n)->egress_framing = cfg->framing;
         PA_PID(ctx, n)->cfg.ptype = 0;
         break;
      case avm_pa_framing_dev:
         PA_PID(ctx, n)->egress_framing = avm_pa_framing_ether;
         PA_PID(ctx, n)->cfg.ptype = 0;
         break;
      case avm_pa_framing_ptype:
         PA_PID(ctx, n)->egress_framing = cfg->framing;
         PA_PID(ctx, n)->cfg.tx_func = 0;
         PA_PID(ctx, n)->cfg.tx_arg = 0;
         break;
   }
   PA_PID(ctx, n)->hw = 0;
   devinfo->pid_handle = n;

   return 0;
}
EXPORT_SYMBOL(avm_pa_dev_pidhandle_register_with_ingress);

int avm_pa_dev_pidhandle_register(struct avm_pa_dev_info *devinfo,
                                  avm_pid_handle pid_handle,
                                  struct avm_pa_pid_cfg *cfg)
{
   return avm_pa_dev_pidhandle_register_with_ingress(devinfo, pid_handle, cfg,
                                                     0);
}
EXPORT_SYMBOL(avm_pa_dev_pidhandle_register);

int avm_pa_dev_pid_register_with_ingress(struct avm_pa_dev_info *devinfo,
                                         struct avm_pa_pid_cfg *cfg,
                                         avm_pid_handle ingress_pid_handle)
{
   return avm_pa_dev_pidhandle_register_with_ingress(devinfo, 0, cfg,
                                                     ingress_pid_handle);
}
EXPORT_SYMBOL(avm_pa_dev_pid_register_with_ingress);

int avm_pa_dev_pid_register(struct avm_pa_dev_info *devinfo,
                            struct avm_pa_pid_cfg *cfg)
{
   return avm_pa_dev_pidhandle_register_with_ingress(devinfo, 0, cfg, 0);
}
EXPORT_SYMBOL(avm_pa_dev_pid_register);

int avm_pa_pid_set_ecfg(avm_pid_handle pid_handle,
                        struct avm_pa_pid_ecfg *ecfg)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
   unsigned int cbsize = sizeof(((struct sk_buff *)0)->cb);

   if (pid->pid_handle != pid_handle)
      return -1;
   memset(&pid->ecfg, 0, sizeof(struct avm_pa_pid_ecfg));
   switch (ecfg->version) {
      case 2:
        pid->ecfg.rx_slow = ecfg->rx_slow;
        pid->ecfg.rx_slow_arg = ecfg->rx_slow_arg;
      case 1:
        pid->ecfg.cb_start = ecfg->cb_start;
        pid->ecfg.cb_len = ecfg->cb_len;
      case 0:
        pid->ecfg.flags = ecfg->flags;
   }
   if (pid->ecfg.cb_start + pid->ecfg.cb_len > cbsize)
      return -2;
   pid->ecfg.version = ecfg->version;
   return 0;
}
EXPORT_SYMBOL(avm_pa_pid_set_ecfg);

int avm_pa_pid_set_framing(avm_pid_handle pid_handle,
                           enum avm_pa_framing ingress_framing, 
                           enum avm_pa_framing egress_framing)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);

   if (pid->pid_handle != pid_handle)
      return -1;

   switch (ingress_framing) {
      case avm_pa_framing_llcsnap:
      case avm_pa_framing_ether:
      case avm_pa_framing_ppp:
      case avm_pa_framing_ip:
      case avm_pa_framing_dev:
         pid->ingress_framing = ingress_framing;
         pid->cfg.ptype = 0;
         break;
      case avm_pa_framing_ptype:
         if (pid->ingress_framing != ingress_framing)
            return -2;
         pid->cfg.tx_func = 0;
         pid->cfg.tx_arg = 0;
         break;
   }
   switch (egress_framing) {
      case avm_pa_framing_llcsnap:
      case avm_pa_framing_ether:
      case avm_pa_framing_ppp:
      case avm_pa_framing_ip:
         pid->egress_framing = egress_framing;
         pid->cfg.ptype = 0;
         break;
      case avm_pa_framing_dev:
         pid->egress_framing = avm_pa_framing_ether;
         pid->cfg.ptype = 0;
         break;
      case avm_pa_framing_ptype:
         return -3;
   }
   return 0;
}
EXPORT_SYMBOL(avm_pa_pid_set_framing);

static void pa_show_pids(pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   char buf[128];
   avm_pid_handle n;

   for (n=1; n < CONFIG_AVM_PA_MAX_PID; n++) {
      struct avm_pa_pid *pid = PA_PID(ctx, n);
      if (pid->pid_handle == 0) continue;
      if (pid->ingress_pid_handle == pid->pid_handle) {
         (*fprintffunc)(arg, "PID%-3d: (%5d) %-5s %-5s %10lu %s %s\n", 
                             pid->pid_handle,
                             pid->cfg.default_mtu,
                             framing2str(pid->ingress_framing),
                             framing2str(pid->egress_framing),
                             (unsigned long)pid->tx_pkts,
                             pid->cfg.name,
                             pidflags2str(pid->ecfg.flags, buf, sizeof(buf)));
      } else {
         (*fprintffunc)(arg, "PID%-3d: (%5d) %-5s %-5s %10lu %s (ingress %d %s) %s\n", 
                             pid->pid_handle,
                             pid->cfg.default_mtu,
                             framing2str(pid->ingress_framing),
                             framing2str(pid->egress_framing),
                             (unsigned long)pid->tx_pkts,
                             pid->cfg.name,
                             pid->ingress_pid_handle,
                             PA_PID(ctx, pid->ingress_pid_handle)->cfg.name,
                             pidflags2str(pid->ecfg.flags, buf, sizeof(buf)));
      }
   }
}

int avm_pa_dev_vpidhandle_register(struct avm_pa_dev_info *devinfo,
                                   avm_vpid_handle vpid_handle,
                                   struct avm_pa_vpid_cfg *cfg)
{
   struct avm_pa_global *ctx = &pa_glob;
   avm_vpid_handle n;

   if (devinfo->vpid_handle) {
      if (vpid_handle && devinfo->vpid_handle != vpid_handle)
         return 0;
      n = devinfo->vpid_handle;
      goto slot_found;
   }
   if (vpid_handle) {
      n = vpid_handle;
      goto slot_found;
   }
   for (n=1; n < CONFIG_AVM_PA_MAX_VPID; n++) {
      if (strncmp(cfg->name, PA_VPID(ctx, n)->cfg.name, AVM_PA_MAX_NAME) == 0) {
         goto slot_found;
      }
   }
   for (n=1; n < CONFIG_AVM_PA_MAX_VPID; n++) {
      if (PA_VPID(ctx, n)->vpid_handle == 0)
         goto slot_found;
   }
   return -1;
slot_found:
   if (cfg->v4_mtu == 0)
      cfg->v4_mtu = 1500;
   if (cfg->v6_mtu == 0)
      cfg->v6_mtu = 1500;
   PA_VPID(ctx, n)->vpid_handle = n;
   PA_VPID(ctx, n)->cfg = *cfg;
   memset(&PA_VPID(ctx, n)->stats, 0, sizeof(struct avm_pa_vpid_stats));
   memset(PA_VPID(ctx, n)->hwstats, 0, sizeof(struct avm_pa_hw_stats)*AVM_PA_MAX_PRIOS);
   devinfo->vpid_handle = n;
   return 0;
}
EXPORT_SYMBOL(avm_pa_dev_vpidhandle_register);

int avm_pa_dev_vpid_register(struct avm_pa_dev_info *devinfo,
                             struct avm_pa_vpid_cfg *cfg)
{
   return avm_pa_dev_vpidhandle_register(devinfo, 0, cfg);
}
EXPORT_SYMBOL(avm_pa_dev_vpid_register);

void avm_pa_dev_unregister(struct avm_pa_dev_info *devinfo)
{
   struct avm_pa_global *ctx = &pa_glob;
   (void)avm_pa_dev_reset_stats(devinfo);
   if (devinfo->pid_handle) {
      struct avm_pa_pid *pid = PA_PID(ctx, devinfo->pid_handle);
      if (devinfo->pid_handle == pid->pid_handle) {
         struct avm_hardware_pa *hwpa = &ctx->hardware_pa;
         avm_pid_handle n;
         /* free virtual channels */
         if (pid->tx_channel_activated && hwpa && hwpa->free_tx_channel) {
             hwpa->free_tx_channel( pid->pid_handle );
             pid->tx_channel_activated = 0;
         }
         if (pid->rx_channel_activated && hwpa && hwpa->free_rx_channel) {
             hwpa->free_rx_channel( pid->pid_handle );
             pid->rx_channel_activated = 0;
         }
         if (pid->hw)
             kfree(pid->hw);

         /* keep cfg, for reuse by name */
         pid->pid_handle = 0;
         pid->hw = 0;
         /* check if pid is used as ingress pid */
         for (n=1; n < CONFIG_AVM_PA_MAX_PID; n++) {
            pid = PA_PID(ctx, n);
            if (pid->ingress_pid_handle == devinfo->pid_handle)
                pid->ingress_pid_handle = pid->pid_handle;
         }
         avm_pa_flush_sessions_for_pid(devinfo->pid_handle);
      }
      devinfo->pid_handle = 0;
   }
   if (devinfo->vpid_handle) {
      struct avm_pa_vpid *vpid = PA_VPID(ctx, devinfo->vpid_handle);
      if (devinfo->vpid_handle == vpid->vpid_handle) {
         avm_pa_flush_sessions_for_vpid(devinfo->vpid_handle);
         /* keep cfg, for reuse by name */
         vpid->vpid_handle = 0;
      }
      devinfo->vpid_handle = 0;
   }
}
EXPORT_SYMBOL(avm_pa_dev_unregister);

int avm_pa_pid_set_hwinfo(avm_pid_handle pid_handle,
                          struct avm_pa_pid_hwinfo *hw)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
   if (pid_handle != pid->pid_handle) {
      printk(KERN_ERR "avm_pa_pid_set_hwinfo: pid %u not registered\n", 
                      pid_handle);
      return -1;
   }
   pid->hw = kmalloc(sizeof(struct avm_pa_pid_hwinfo), GFP_KERNEL);
   if ( !pid->hw ) {
      printk(KERN_ERR "avm_pa_pid_set_hwinfo: kmalloc failed\n");
      return -1;
   }
   memcpy(pid->hw, hw, sizeof(struct avm_pa_pid_hwinfo));

   return 0;
}
EXPORT_SYMBOL(avm_pa_pid_set_hwinfo);

struct avm_pa_pid_hwinfo *avm_pa_pid_get_hwinfo(avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
   if (pid_handle != pid->pid_handle) {
      if (net_ratelimit())
         printk(KERN_ERR "avm_pa_pid_get_hwinfo: pid %u not registered\n", 
                         pid_handle);
      return 0;
   }
   pid = PA_PID(ctx, pid_handle);
   return pid->hw;
}
EXPORT_SYMBOL(avm_pa_pid_get_hwinfo);

int avm_pa_pid_activate_hw_accelaration(avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_pid *pid = PA_PID(ctx, pid_handle);
   struct avm_hardware_pa *hwpa;
   if (pid_handle != pid->pid_handle) {
      printk(KERN_ERR "avm_pa_pid_activate_hw_accelaration: pid %u not registered\n", 
                      pid_handle);
      return -1;
   }
   hwpa = &ctx->hardware_pa;
   if (   pid->rx_channel_activated == 0
       && pid->ingress_framing == avm_pa_framing_ether
       && pid->ecfg.rx_slow
       && hwpa->alloc_rx_channel) {
      if ((*hwpa->alloc_rx_channel)(pid_handle) == 0)
         pid->rx_channel_activated = 1;
   }else {
       printk(KERN_ERR "don_t alloc_rx_channel: %d, %d, %pF, %pF\n", pid->rx_channel_activated, pid->ingress_framing,pid->ecfg.rx_slow, hwpa->alloc_rx_channel );
   }
   if (   pid->tx_channel_activated == 0
       && pid->ingress_framing == avm_pa_framing_ether
       && hwpa->alloc_tx_channel) {
      if ((*hwpa->alloc_tx_channel)(pid_handle) == 0)
         pid->tx_channel_activated = 1;
   }
   return 0;
}
EXPORT_SYMBOL(avm_pa_pid_activate_hw_accelaration);

void avm_pa_hardware_session_report(avm_session_handle session_handle,
                                    u32 pkts, u64 bytes)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session;
   struct avm_pa_pid *pid;
   struct avm_pa_vpid *vpid;
   AVM_PA_LOCK_DECLARE;
   int i;


   AVM_PA_WRITE_LOCK();
   if ( (session = pa_session_get_unlocked(session_handle)) == 0 
       || session->lru != AVM_PA_LRU_ACTIVE) {
      AVM_PA_WRITE_UNLOCK();
      if (net_ratelimit())
         printk(KERN_ERR "avm_pa_hardware_session_report: no session %u\n", 
                         session_handle);
      return;
   }

   if (pkts == 0 && bytes == 0) { /* session not used */
      pa_kill_session_unlocked(session, "hw timeout");
      ctx->stats.sess_timedout++;
      AVM_PA_WRITE_UNLOCK();
      return;
   }

   pa_session_update_unlocked(session);
   AVM_PA_WRITE_UNLOCK();

   if (session->ingress_vpid_handle) {
      vpid = PA_VPID(ctx, session->ingress_vpid_handle);
      ((u32 *)(&vpid->stats.rx_unicast_pkt))[session->ingress.casttype] += pkts;
      vpid->stats.rx_bytes += bytes;
   }

   for (i = 0; i < session->negress; i++) {
      struct avm_pa_egress *egress = &session->egress[i];
      unsigned int prio = egress->output.priority;
      prio = (prio & TC_H_MIN_MASK);
      if (prio >= AVM_PA_MAX_PRIOS) prio = AVM_PA_MAX_PRIOS-1;

      if (egress->vpid_handle) {
         vpid = PA_VPID(ctx, egress->vpid_handle);
         ((u32 *)(&vpid->stats.tx_unicast_pkt))[egress->match.casttype] += pkts;
         vpid->stats.tx_bytes += bytes;
         vpid->hwstats[prio].tx_pkts += pkts;
         vpid->hwstats[prio].tx_bytes += bytes;
      }
      egress->tx_pkts += pkts;
      if (egress->pid_handle) {
         pid = PA_PID(ctx, egress->pid_handle);
         pid->tx_pkts += pkts;
      }
   }
}
EXPORT_SYMBOL(avm_pa_hardware_session_report);

void avm_pa_register_hardware_pa(struct avm_hardware_pa *pa_functions)
{
   struct avm_pa_global *ctx = &pa_glob;
   if (pa_functions)
      ctx->hardware_pa = *pa_functions;
   else {
      struct avm_pa_global *ctx = &pa_glob;
      struct avm_pa_session *session, *next;
      AVM_PA_LOCK_DECLARE;

      AVM_PA_WRITE_LOCK();
      /* stop adding hw sessions */
      ctx->hardware_pa.add_session = 0;

      /* kill all sessions in hw pa */
      session = ctx->sess_lru[AVM_PA_LRU_ACTIVE].lru_head;
      while (session) {
         next = session->lru_next;
         if (session->in_hw) {
            pa_kill_session_unlocked(session, "unregister hw pa");
            ctx->stats.sess_flushed++;
         }
         session = next;
      }
      /* remove hw session from dead sessions */
      session = ctx->sess_lru[AVM_PA_LRU_DEAD].lru_head;
      while (session) {
         next = session->lru_next;
         session->in_hw = 0;
         session->hw_session = 0;
         session = next;
      }
      AVM_PA_WRITE_UNLOCK();

      /* delete hw pa functions */
      memset(&ctx->hardware_pa, 0, sizeof(struct avm_hardware_pa));
   }
}

EXPORT_SYMBOL(avm_pa_register_hardware_pa);

/* ------------------------------------------------------------------------ */

static void pa_show_brief(pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   avm_vpid_handle n;
   char *mode;

   (*fprintffunc)(arg, "Version        : %s\n", AVM_PA_VERSION);

   if (ctx->disabled) mode = "disabled";
   else if (ctx->fw_disabled) mode = "testmode";
   else if (ctx->misc_is_open) mode = "capture";
   else mode = "enabled";

   (*fprintffunc)(arg, "State          : %s\n", mode);
   if (ctx->hardware_pa.add_session) {
      mode = ctx->hw_ppa_disabled ? "disabled" : "enable";
      (*fprintffunc)(arg, "HW State       : %s\n", mode);
   }
   (*fprintffunc)(arg, "BSessions      : %u\n",
                       (unsigned)ctx->stats.nbsessions);
   (*fprintffunc)(arg, "Sessions       : %hu\n",
                       ctx->sess_lru[AVM_PA_LRU_ACTIVE].nsessions);
   (*fprintffunc)(arg, "Max Sessions   : %hu\n",
                       ctx->sess_lru[AVM_PA_LRU_ACTIVE].maxsessions);
   (*fprintffunc)(arg, "Sessions (dead): %hu\n",
                       ctx->sess_lru[AVM_PA_LRU_DEAD].nsessions);
   (*fprintffunc)(arg, "Sessions (free): %hu\n",
                       ctx->sess_lru[AVM_PA_LRU_FREE].nsessions);
   (*fprintffunc)(arg, "Queuelen       : %lu\n",
                       (unsigned long)skb_queue_len(&ctx->tbfqueue));
   (*fprintffunc)(arg, "Rx pkts/secs   : %lu\n",
                       (unsigned long)ctx->stats.rx_pps);
   if (ctx->tbf_enabled) {
      (*fprintffunc)(arg, "Limit pkts/sec : %lu\n",
                          (unsigned long)ctx->rate);
   }
   (*fprintffunc)(arg, "Fw pkts/sec    : %lu\n",
                       (unsigned long)ctx->stats.fw_pps);
   (*fprintffunc)(arg, "Ov pkts/sec    : %lu\n",
                       (unsigned long)ctx->stats.overlimit_pps);
   (*fprintffunc)(arg, "Rx pakets      : %lu\n",
                       (unsigned long)ctx->stats.rx_pkts);
   (*fprintffunc)(arg, "Rx bypass      : %lu\n",
                       (unsigned long)ctx->stats.rx_bypass);
   (*fprintffunc)(arg, "Rx ttl <= 1    : %lu\n",
                       (unsigned long)ctx->stats.rx_ttl);
   (*fprintffunc)(arg, "Rx broadcast   : %lu\n",
                       (unsigned long)ctx->stats.rx_broadcast);
   (*fprintffunc)(arg, "Rx search      : %lu\n",
                       (unsigned long)ctx->stats.rx_search);
   (*fprintffunc)(arg, "Rx match       : %lu\n",
                       (unsigned long)ctx->stats.rx_match);
   (*fprintffunc)(arg, "Rx modified    : %lu\n",
                       (unsigned long)ctx->stats.rx_mod);
   (*fprintffunc)(arg, "Fw pakets      : %lu\n",
                       (unsigned long)ctx->stats.fw_pkts);
   (*fprintffunc)(arg, "Fw local       : %lu\n",
                       (unsigned long)ctx->stats.fw_local);
   for (n=1; n < CONFIG_AVM_PA_MAX_VPID; n++) {
      struct avm_pa_vpid *vpid = PA_VPID(ctx, n);
      unsigned long rx, tx;
      if (vpid->vpid_handle == 0) continue;
      rx =   vpid->stats.rx_unicast_pkt
           + vpid->stats.rx_multicast_pkt
           + vpid->stats.rx_broadcast_pkt;
      tx =   vpid->stats.tx_unicast_pkt
           + vpid->stats.tx_multicast_pkt
           + vpid->stats.tx_broadcast_pkt;
      (*fprintffunc)(arg, "VPID%-2d: RX %10lu TX %10lu %s\n", 
                          vpid->vpid_handle, rx, tx, vpid->cfg.name);
   }
}

static void pa_show_status(pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   char *mode;

   if (ctx->disabled) mode = "disabled";
   else if (ctx->fw_disabled) mode = "testmode";
   else if (ctx->misc_is_open) mode = "capture";
   else mode = "enabled";

   (*fprintffunc)(arg, "State          : %s\n", mode);
   if (ctx->hardware_pa.add_session) {
      mode = ctx->hw_ppa_disabled ? "disabled" : "enable";
      (*fprintffunc)(arg, "HW State       : %s\n", mode);
   }
   switch (ctx->load_control) {
      case LOADCONTROL_IDLE: mode = "idle"; break;
      case LOADCONTROL_POWER:   mode = "power"; break;
      case LOADCONTROL_IRQ:      mode = "irq"; break;
      case LOADCONTROL_POWERIRQ: mode = "powerirq"; break;
      default: mode = "????"; break;
   }
   (*fprintffunc)(arg, "Loadcontrol    : %s\n", mode);
   (*fprintffunc)(arg, "IDLE mswin     : %u %u\n",
                       ctx->idle_mswin_low, ctx->idle_mswin_high);
   (*fprintffunc)(arg, "IRQ mswin      : %u %u\n",
                       ctx->irq_mswin_low, ctx->irq_mswin_high);
   (*fprintffunc)(arg, "TelephonyReduce: %u\n", ctx->telephony_reduce);
   (*fprintffunc)(arg, "Maxrate        : %u\n", ctx->maxrate);
   mode = ctx->tbf_enabled ? "enabled" : "disabled";
   (*fprintffunc)(arg, "TBF            : %s\n", mode);
   (*fprintffunc)(arg, "Limit Rate     : %u\n", ctx->rate);
   (*fprintffunc)(arg, "Current Rate   : %lu\n", 
                       (unsigned long)ctx->stats.fw_pps);
   (*fprintffunc)(arg, "user msecs/sec : %lu\n",
                       (unsigned long)ctx->stats.userms);
   (*fprintffunc)(arg, "idle msecs/sec : %lu\n",
                       (unsigned long)ctx->stats.idlems);
   (*fprintffunc)(arg, "irq msecs/sec  : %lu\n",
                       (unsigned long)ctx->stats.irqms);
}

static void pa_show_vpids(pa_fprintf fprintffunc, void *arg)

{
   struct avm_pa_global *ctx = &pa_glob;
   avm_vpid_handle n;

   for (n=1; n < CONFIG_AVM_PA_MAX_VPID; n++) {
      struct avm_pa_vpid *vpid = PA_VPID(ctx, n);
      if (vpid->vpid_handle == 0) continue;
      (*fprintffunc)(arg, "VPID%-2d: %4d/%4d  %s\n", 
                          vpid->vpid_handle,
                          vpid->cfg.v4_mtu,
                          vpid->cfg.v6_mtu,
                          vpid->cfg.name);
      (*fprintffunc)(arg, "       %10s %10s %10s %10s %10s %10s\n",
                               "unicast",
                               "multicast",
                               "broadcast",
                               "discard",
                               "error",
                               "bytes");
      (*fprintffunc)(arg, "  RX   %10lu %10lu %10lu %10lu %10s %10Lu\n",
                               (unsigned long)vpid->stats.rx_unicast_pkt,
                               (unsigned long)vpid->stats.rx_multicast_pkt,
                               (unsigned long)vpid->stats.rx_broadcast_pkt,
                               (unsigned long)vpid->stats.rx_discard,
                               "-",
                               (unsigned long long)vpid->stats.rx_bytes);
      (*fprintffunc)(arg, "  TX   %10lu %10lu %10lu %10lu %10lu %10Lu\n",
                               (unsigned long)vpid->stats.tx_unicast_pkt,
                               (unsigned long)vpid->stats.tx_multicast_pkt,
                               (unsigned long)vpid->stats.tx_broadcast_pkt,
                               (unsigned long)vpid->stats.tx_discard,
                               (unsigned long)vpid->stats.tx_error,
                               (unsigned long long)vpid->stats.tx_bytes);
   }
}

void avm_pa_dev_set_ipv4_mtu(struct avm_pa_dev_info *devinfo, u16 mtu)
{
   if (devinfo->vpid_handle) {
      struct avm_pa_global *ctx = &pa_glob;
      PA_VPID(ctx, devinfo->vpid_handle)->cfg.v4_mtu = mtu;
   }
}
EXPORT_SYMBOL(avm_pa_dev_set_ipv4_mtu);

void avm_pa_dev_set_ipv6_mtu(struct avm_pa_dev_info *devinfo, u16 mtu)
{
   if (devinfo->vpid_handle) {
      struct avm_pa_global *ctx = &pa_glob;
      PA_VPID(ctx, devinfo->vpid_handle)->cfg.v6_mtu = mtu;
   }
}
EXPORT_SYMBOL(avm_pa_dev_set_ipv6_mtu);

int avm_pa_dev_get_stats(struct avm_pa_dev_info *devinfo,
                         struct avm_pa_vpid_stats *stats)
{
    if (devinfo->vpid_handle) {
       struct avm_pa_global *ctx = &pa_glob;
       struct avm_pa_vpid *vpid = PA_VPID(ctx, devinfo->vpid_handle);
       if (vpid->vpid_handle == devinfo->vpid_handle) {
          memcpy(stats, &vpid->stats, sizeof(struct avm_pa_vpid_stats));
          return 0;
       }
    }
    memset(stats, 0, sizeof(struct avm_pa_vpid_stats));
    return -1;
}
EXPORT_SYMBOL(avm_pa_dev_get_stats);

int avm_pa_dev_get_hw_stats(struct avm_pa_dev_info *devinfo,
                              struct avm_pa_hw_stats *stats,
                              unsigned int prio)
{
    if (prio >= AVM_PA_MAX_PRIOS) return -1;

    if (devinfo->vpid_handle) {
       struct avm_pa_global *ctx = &pa_glob;
       struct avm_pa_vpid *vpid = PA_VPID(ctx, devinfo->vpid_handle);
       if (vpid->vpid_handle == devinfo->vpid_handle) {
          memcpy(stats, &vpid->hwstats[prio], sizeof(struct avm_pa_hw_stats));
          return 0;
       }
    }
    memset(stats, 0, sizeof(struct avm_pa_hw_stats));
    return -1;
}
EXPORT_SYMBOL(avm_pa_dev_get_hw_stats);

int avm_pa_dev_reset_stats(struct avm_pa_dev_info *devinfo)
{
    if (devinfo->vpid_handle) {
       struct avm_pa_global *ctx = &pa_glob;
       struct avm_pa_vpid *vpid = PA_VPID(ctx, devinfo->vpid_handle);
       if (vpid->vpid_handle == devinfo->vpid_handle) {
          memset(&vpid->stats, 0, sizeof(struct avm_pa_vpid_stats));
          memset(vpid->hwstats, 0, sizeof(struct avm_pa_hw_stats)*AVM_PA_MAX_PRIOS);
          return 0;
       }
    }
    return -1;
}
EXPORT_SYMBOL(avm_pa_dev_reset_stats);

void avm_pa_flush_sessions(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_WRITE_LOCK();
   while ((session = ctx->sess_lru[AVM_PA_LRU_ACTIVE].lru_head) != 0) {
      pa_kill_session_unlocked(session, "flush");
      ctx->stats.sess_flushed++;
   }
   AVM_PA_WRITE_UNLOCK();
}
EXPORT_SYMBOL(avm_pa_flush_sessions);

void avm_pa_flush_lispencap_sessions(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session, *next;
   AVM_PA_LOCK_DECLARE;

   AVM_PA_WRITE_LOCK();

   session = ctx->sess_lru[AVM_PA_LRU_ACTIVE].lru_head;
   while (session) {
      next = session->lru_next;
      if (session->mod.pkttype & AVM_PA_PKTTYPE_LISP) {
         pa_kill_session_unlocked(session, "lispencap flush");
         ctx->stats.sess_flushed++;
      }
      session = next;
   }

   AVM_PA_WRITE_UNLOCK();
}
EXPORT_SYMBOL(avm_pa_flush_lispencap_sessions);

void avm_pa_flush_sessions_for_vpid(avm_vpid_handle vpid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session, *next;
   AVM_PA_LOCK_DECLARE;

   if (   vpid_handle == 0
       || PA_VPID(ctx, vpid_handle)->vpid_handle != vpid_handle)
      return;

   AVM_PA_WRITE_LOCK();

   session = ctx->sess_lru[AVM_PA_LRU_ACTIVE].lru_head;
   while (session) {
      next = session->lru_next;
      if (session->ingress_vpid_handle == vpid_handle) {
         pa_kill_session_unlocked(session, "ingress vpid flush");
         ctx->stats.sess_flushed++;
      } else {
         int negress;
         for (negress = 0; negress < session->negress; negress++) {
            struct avm_pa_egress *egress = &session->egress[negress];
            if (egress->vpid_handle == vpid_handle) {
               pa_kill_session_unlocked(session, "egress vpid flush");
               ctx->stats.sess_flushed++;
               break;
            }
         }
      }
      session = next;
   }

   AVM_PA_WRITE_UNLOCK();
}
EXPORT_SYMBOL(avm_pa_flush_sessions_for_vpid);

void avm_pa_flush_sessions_for_pid(avm_pid_handle pid_handle)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session, *next;
   AVM_PA_LOCK_DECLARE;

   if (   pid_handle == 0
       || PA_PID(ctx, pid_handle)->pid_handle != pid_handle)
      return;

   AVM_PA_WRITE_LOCK();

   session = ctx->sess_lru[AVM_PA_LRU_ACTIVE].lru_head;
   while (session) {
      next = session->lru_next;
      if (session->ingress_pid_handle == pid_handle) {
         pa_kill_session_unlocked(session, "ingress pid flush");
         ctx->stats.sess_flushed++;
      } else {
         int negress;
         for (negress = 0; negress < session->negress; negress++) {
            struct avm_pa_egress *egress = &session->egress[negress];
            if (egress->pid_handle == pid_handle) {
               pa_kill_session_unlocked(session, "egress pid flush");
               ctx->stats.sess_flushed++;
               break;
            }
         }
      }
      session = next;
   }

   AVM_PA_WRITE_UNLOCK();
}
EXPORT_SYMBOL(avm_pa_flush_sessions_for_pid);

static void avm_pa_sip_is_active(int state)
{
   struct avm_pa_global *ctx = &pa_glob;
   unsigned rate;

   if (ctx->disabled)
      return;

   if (state) {
      if (ctx->telephony_active == 0) {
         rate = ctx->tbf_enabled ? ctx->rate : ctx->maxrate;
         ctx->rate = rate - (rate*ctx->telephony_reduce)/100;
         ctx->load_control = LOADCONTROL_POWERIRQ;
         avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
         ctx->tbf_enabled = 1;
         printk(KERN_INFO "avm_pa: telephony active%s\n",
                          ctx->rate != rate ? " (reduce)" : "");
      }
      ctx->telephony_active = 1;
   } else {
      if (ctx->telephony_active) {
         ctx->load_control = LOADCONTROL_IDLE;
         avm_pa_tbf_disable();
         printk(KERN_INFO "avm_pa: telephony inactive\n");
      }
      ctx->telephony_active = 0;
   }   
}

void avm_pa_telefon_state(int state)
{
   printk(KERN_INFO "avm_pa: avm_pa_telefon_state\n");
}
EXPORT_SYMBOL(avm_pa_telefon_state);

/* ------------------------------------------------------------------------ */
/* ------- packet rate estimater ------------------------------------------ */
/* ------------------------------------------------------------------------ */

static void avm_pa_est_timer(unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_est *e;
   u32 npackets;
   u32 rate;

   /* fw pkts/s */
   e = &ctx->fw_est;
   npackets = ctx->stats.fw_pkts;
   if (npackets >= e->last_packets) {
      rate = (npackets - e->last_packets)<<(12 - ctx->est_idx);
      e->last_packets = npackets;
      e->avpps += (rate >> e->ewma_log) - (e->avpps >> e->ewma_log);
      ctx->stats.fw_pps = (e->avpps+0x1FF)>>10;
   } else {
      e->last_packets = npackets;
   }
   if (   ctx->load_reduce == 0
       && ctx->stats.fw_pps > ctx->maxrate)
      ctx->maxrate = ctx->stats.fw_pps;

   /* rx pkts/s */
   e = &ctx->rx_est;
   npackets = ctx->stats.rx_pkts;
   if (npackets >= e->last_packets) {
      rate = (npackets - e->last_packets)<<(12 - ctx->est_idx);
      e->last_packets = npackets;
      e->avpps += (rate >> e->ewma_log) - (e->avpps >> e->ewma_log);
      ctx->stats.rx_pps = (e->avpps+0x1FF)>>10;
   } else {
      e->last_packets = npackets;
   }

   /* queued pkts/s */
   e = &ctx->overlimit_est;
   npackets = ctx->stats.rx_overlimit;
   if (npackets >= e->last_packets) {
      rate = (npackets - e->last_packets)<<(12 - ctx->est_idx);
      e->last_packets = npackets;
      e->avpps += (rate >> e->ewma_log) - (e->avpps >> e->ewma_log);
      ctx->stats.overlimit_pps = (e->avpps+0x1FF)>>10;
   } else {
      e->last_packets = npackets;
   }

   mod_timer(&ctx->est_timer, jiffies + ((HZ/4) << ctx->est_idx));
}

static void avm_pa_setup_est(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_est *e;
   del_timer(&ctx->est_timer);
   e = &ctx->fw_est;
   e->ewma_log = ctx->ewma_log;
   e->last_packets = ctx->stats.fw_pkts;
   e = &ctx->rx_est;
   e->ewma_log = ctx->ewma_log;
   e->last_packets = ctx->stats.rx_pkts;
   e = &ctx->overlimit_est;
   e->ewma_log = ctx->ewma_log;
   e->last_packets = ctx->stats.rx_overlimit;

   mod_timer(&ctx->est_timer, jiffies + ((HZ/4) << ctx->est_idx));
}

static void avm_pa_unsetup_est(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   del_timer(&ctx->est_timer);
}

/* ------------------------------------------------------------------------ */
/* -------- cputime estimater --------------------------------------------- */
/* ------------------------------------------------------------------------ */

static inline void avm_pa_get_cputimes(cputime64_t *usertime,
                                       cputime64_t *idletime,
                                       cputime64_t *irqtime)
{
   cputime64_t usersum, idlesum, irqsum;
   int i;

   usersum = idlesum = irqsum = cputime64_zero;
   for_each_possible_cpu(i) {
      usersum = cputime64_add(usersum, kstat_cpu(i).cpustat.user);
      usersum = cputime64_add(usersum, kstat_cpu(i).cpustat.nice);
      usersum = cputime64_add(usersum, kstat_cpu(i).cpustat.system);
      idlesum = cputime64_add(idlesum, kstat_cpu(i).cpustat.idle);
      idlesum = cputime64_add(idlesum, arch_idle_time(i));
      idlesum = cputime64_add(idlesum, kstat_cpu(i).cpustat.iowait);
      irqsum = cputime64_add(irqsum, kstat_cpu(i).cpustat.irq);
      irqsum = cputime64_add(irqsum, kstat_cpu(i).cpustat.softirq);
   }
   irqsum += arch_irq_stat();
   *usertime = usersum;
   *idletime = idlesum;
   *irqtime = irqsum;
}

static void avm_pa_cputime_est_timer(unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_cputime_est *e;
   cputime64_t usersum, idlesum, irqsum;
   cputime64_t cputime;
   u32 rate;
   u32 userdiff = 0;
   u32 idlediff = 0;
   u32 irqdiff = 0;

   avm_pa_get_cputimes(&usersum, &idlesum, &irqsum);

   /* usertime/s */
   e = &ctx->cputime_user_est;
   cputime = usersum;
   if (cputime >= e->last_cputime) {
      userdiff = cputime_to_msecs(cputime - e->last_cputime);
      rate = userdiff<<(12 - ctx->cputime_est_idx);
      e->last_cputime = cputime;
      e->avtps += (rate >> e->ewma_log) - (e->avtps >> e->ewma_log);
      ctx->stats.userms = (e->avtps+0x1FF)>>10;
   } else {
      e->last_cputime = cputime;
   }

   /* idletime/s */
   e = &ctx->cputime_idle_est;
   cputime = idlesum;
   if (cputime >= e->last_cputime) {
      idlediff = cputime_to_msecs(cputime - e->last_cputime);
      rate = idlediff<<(12 - ctx->cputime_est_idx);
      e->last_cputime = cputime;
      e->avtps += (rate >> e->ewma_log) - (e->avtps >> e->ewma_log);
      ctx->stats.idlems = (e->avtps+0x1FF)>>10;
   } else {
      e->last_cputime = cputime;
   }

   /* irqtime/s */
   e = &ctx->cputime_irq_est;
   cputime = irqsum;
   if (cputime >= e->last_cputime) {
      irqdiff = cputime_to_msecs(cputime - e->last_cputime);
      rate = irqdiff<<(12 - ctx->cputime_est_idx);
      e->last_cputime = cputime;
      e->avtps += (rate >> e->ewma_log) - (e->avtps >> e->ewma_log);
      ctx->stats.irqms = (e->avtps+0x1FF)>>10;
   } else {
      e->last_cputime = cputime;
   }

   if (ctx->dbgcputime)
      printk(KERN_INFO "avm_pa: %lu/%lu/%lu (%lu/%lu/%lu)\n",
                       (unsigned long)userdiff,
                       (unsigned long)idlediff,
                       (unsigned long)irqdiff,
                       (unsigned long)ctx->stats.userms,
                       (unsigned long)ctx->stats.idlems,
                       (unsigned long)ctx->stats.irqms);

   mod_timer(&ctx->cputime_est_timer, jiffies + ((HZ/4)<<ctx->cputime_est_idx));
}

static void avm_pa_setup_cputime_est(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_cputime_est *e;
   cputime64_t usersum, idlesum, irqsum;

   del_timer(&ctx->cputime_est_timer);

   avm_pa_get_cputimes(&usersum, &idlesum, &irqsum);
   e = &ctx->cputime_user_est;
   e->ewma_log = ctx->cputime_ewma_log;
   e->last_cputime = cputime_to_msecs(usersum);
   e = &ctx->cputime_idle_est;
   e->ewma_log = ctx->cputime_ewma_log;
   e->last_cputime = cputime_to_msecs(idlesum);
   e = &ctx->cputime_irq_est;
   e->ewma_log = ctx->cputime_ewma_log;
   e->last_cputime = cputime_to_msecs(irqsum);

   mod_timer(&ctx->cputime_est_timer, jiffies + ((HZ/4)<<ctx->cputime_est_idx));
}

static void avm_pa_unsetup_cputime_est(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   del_timer(&ctx->cputime_est_timer);
}

/* ------------------------------------------------------------------------ */
/* -------- value log ----------------------------------------------------- */
/* ------------------------------------------------------------------------ */

#if AVM_PA_TOKSTATS
static int avm_pa_thread(void *reply_data)
{
   struct avm_pa_global *ctx = &pa_glob;
   unsigned long wtime = msecs_to_jiffies(100);
   unsigned long rx_overlimit;

   set_user_nice(current, 19);
   {
      sigset_t blocked;
      sigfillset(&blocked);
      sigprocmask(SIG_BLOCK, &blocked, NULL);
      flush_signals(current);
   }

   rx_overlimit = ctx->stats.rx_overlimit;

   while (!kthread_should_stop()) {
      unsigned long endtime = jiffies + wtime;
      unsigned long overtime;
      unsigned long overlimit;
      unsigned long pps;

      schedule_timeout_interruptible(wtime);
      overlimit = ctx->stats.rx_overlimit - rx_overlimit;
      rx_overlimit = ctx->stats.rx_overlimit;
      overtime = jiffies - endtime;
      pps = ctx->stats.fw_pps;

      ctx->tok_pos = (ctx->tok_pos+1)%TOK_SAMLES;
      ctx->tok_state[ctx->tok_pos] = ctx->load_reduce;
      ctx->tok_overtime[ctx->tok_pos] = overtime;
      ctx->tok_rate[ctx->tok_pos] = ctx->rate;
      ctx->tok_pps[ctx->tok_pos] = pps;
      ctx->tok_overlimit[ctx->tok_pos] = overlimit;
   }
   return 0;
}
#endif

/* ------------------------------------------------------------------------ */

static inline void avm_pa_start_lc_timer(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   if (mod_timer(&ctx->lc_timer, jiffies + AVM_PA_LC_TIMEOUT*HZ) == 0) 
      ctx->lc_overlimit = ctx->stats.rx_overlimit;
}

static inline void avm_pa_stop_lc_timer(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   del_timer(&ctx->lc_timer);
}

static void avm_pa_lc_timer_expired(unsigned long data)
{
   struct avm_pa_global *ctx = &pa_glob;
   u32 overlimit = ctx->stats.rx_overlimit - ctx->lc_overlimit;
   unsigned rate;

   ctx->lc_overlimit = ctx->stats.rx_overlimit;

   if (ctx->load_control & LOADCONTROL_IRQ) {
      if (   ctx->stats.irqms >= ctx->irq_mswin_high
          && ctx->stats.fw_pps > AVM_PA_MINRATE) {
         unsigned percent = 1;
         if (ctx->tbf_enabled == 0) {
            ctx->rate = ctx->maxrate;
            percent = 4;
         }
         rate = ctx->rate;
         rate = rate - (rate*percent)/100;
         ctx->rate = rate;
         avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
         ctx->tbf_enabled = 1;
         printk(KERN_INFO "avm_pa: load reduce 0, rate %u down (pps %lu ov_pps %lu irqms %lu)\n",
                        ctx->rate,
                        (unsigned long)ctx->stats.fw_pps,
                        (unsigned long)ctx->stats.overlimit_pps,
                        (unsigned long)ctx->stats.irqms);
      } else if (   overlimit
                 && ctx->load_reduce == 0
                 && ctx->tbf_enabled
                 && ctx->stats.irqms < ctx->irq_mswin_low) {
         unsigned rate = ctx->rate;
         unsigned percent = 1;
         rate = rate + (rate*percent)/100;
         ctx->rate = rate;
         avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
         printk(KERN_INFO "avm_pa: load reduce 0, rate %u up (pps %lu ov_pps %lu irqms %lu)\n",
                        ctx->rate,
                        (unsigned long)ctx->stats.fw_pps,
                        (unsigned long)ctx->stats.overlimit_pps,
                        (unsigned long)ctx->stats.irqms);
      }
   }
   if (ctx->load_control & LOADCONTROL_IDLE) {
      static unsigned count = 0;
      static unsigned good = 0;
      static unsigned lowcount = 0;
      if (ctx->tbf_enabled) {
         if (ctx->stats.fw_pps > AVM_PA_MINRATE)
            lowcount = 0;
         else lowcount++;
         if (lowcount*AVM_PA_LC_TIMEOUT >= AVM_PA_TRAFFIC_IDLE_TBFDISABLE) {
            avm_pa_tbf_disable();
            printk(KERN_INFO "avm_pa: %d seconds idle, tbf deactivated\n",
                             lowcount*AVM_PA_LC_TIMEOUT);
            lowcount = 0;
         }
      }
      if (   ctx->stats.idlems <= ctx->idle_mswin_low
          && ctx->stats.fw_pps > AVM_PA_MINRATE) {
         unsigned percent;
         if (ctx->tbf_enabled == 0) {
            ctx->rate = ctx->maxrate;
            percent = 5;
         } else if (good) {
            percent = 5;
         } else {
            if (count < 3) percent = 1;
            else if (count < 5) percent = 2;
            else percent = 5;
         }
         good = 0;
         count++;
         rate = ctx->rate;
         rate = rate - (rate*percent)/100;
         ctx->rate = rate;
         avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
         ctx->tbf_enabled = 1;
         printk(KERN_INFO "avm_pa: rate %u down (pps %lu ov_pps %lu idlems %lu count %u)\n",
                        ctx->rate,
                        (unsigned long)ctx->stats.fw_pps,
                        (unsigned long)ctx->stats.overlimit_pps,
                        (unsigned long)ctx->stats.idlems, 
                        count);
      } else {
         count = 0;
         if (   overlimit
             && good
             && ctx->load_reduce == 0
             && ctx->tbf_enabled
             && ctx->stats.idlems > ctx->idle_mswin_high) {
            unsigned rate = ctx->rate;
            unsigned percent = 1;
            rate = rate + (rate*percent)/100;
            ctx->rate = rate;
            avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
            printk(KERN_INFO "avm_pa: rate %u up (pps %lu ov_pps %lu idlems %lu)\n",
                           ctx->rate,
                           (unsigned long)ctx->stats.fw_pps,
                           (unsigned long)ctx->stats.overlimit_pps,
                           (unsigned long)ctx->stats.idlems);
         }
         good++;
      }
   }
   avm_pa_start_lc_timer();
}

#ifdef CONFIG_AVM_POWERMETER
static void avm_pa_load_control_cb(int load_reduce, void *context)
{
   struct avm_pa_global *ctx = &pa_glob;
   unsigned rate;

   if (ctx->disabled || (ctx->load_control & LOADCONTROL_POWER) == 0) {
      ctx->load_reduce = 0;
      return;
   }

   if (load_reduce < 0) load_reduce = 0;
   else if (load_reduce > 10) load_reduce = 10;

   if (load_reduce == 0) {
      if (ctx->load_reduce) {
         printk(KERN_INFO "avm_pa: load reduce %d => %d, rate %u (pps %lu ov_pps %lu)\n",
                       ctx->load_reduce, load_reduce,
                       ctx->rate,
                       (unsigned long)ctx->stats.fw_pps,
                       (unsigned long)ctx->stats.overlimit_pps);
      }
   } else if (ctx->stats.fw_pps > AVM_PA_MINRATE) {
      int change = ctx->load_reduce - load_reduce;
      unsigned percent;
      if (ctx->tbf_enabled == 0)
         ctx->rate = ctx->maxrate;
      rate = ctx->rate;
      if (change <= 0) { /* get worth */
         if (ctx->load_reduce == 0) {
            if (ctx->tbf_enabled)
               percent = (-change)*4;
            else
               percent = (-change)*20;
         } else {
            percent = (-change)*8;
         }
         rate = rate - (rate*percent)/100;
      } else { /* get better */
         percent = change*4;
         rate = rate + (rate*percent)/100;
      }
      printk(KERN_INFO "avm_pa: load reduce %d => %d, rate %u => %u (change %d %u%% pps %lu ov_pps %lu)\n",
                    ctx->load_reduce, load_reduce,
                    ctx->rate, rate,
                    change, percent,
                    (unsigned long)ctx->stats.fw_pps,
                    (unsigned long)ctx->stats.overlimit_pps);
      ctx->rate = rate;
      avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
      ctx->tbf_enabled = 1;
   } else {
      printk(KERN_INFO "avm_pa: load reduce %d => %d, rate %u (pps %lu)\n",
                    ctx->load_reduce, load_reduce,
                    ctx->rate, (unsigned long)ctx->stats.fw_pps);
   }
   ctx->load_reduce = load_reduce;
}
#endif

#if AVM_PA_TOKSTATS
static void pa_show_tstats(pa_fprintf fprintffunc, void *arg)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_tbf *tbf = &ctx->tbf;
   int i = TOK_SAMLES;
   int pos = ctx->tok_pos;

   (*fprintffunc)(arg, "load_reduce %d tbf_enabled %d maxrate %u\n",
                       ctx->load_reduce, ctx->tbf_enabled, 
                       ctx->maxrate);
   (*fprintffunc)(arg, "rate %u buffer %u peak %u\n",
                       ctx->rate, ctx->pktbuffer, ctx->pktpeak);
   (*fprintffunc)(arg, "tbf: buffer %u peak %u pkttime %u tokens %ld/%ld\n",
                       tbf->buffer, tbf->pbuffer, tbf->pkttime,
                       tbf->tokens, tbf->ptokens);

   while (i--) {
      if (--pos < 0) pos = TOK_SAMLES-1;
      (*fprintffunc)(arg, "%d/%u/%u-%u/%lu%s",
                     ctx->tok_state[pos],
                     ctx->tok_overtime[pos],
                     ctx->tok_rate[pos],
                     ctx->tok_pps[pos],
                     ctx->tok_overlimit[pos],
                     i % 8 ? " " : "\n");
   }
}

static void avm_pa_thread_start(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   if (ctx->task == 0) {
      ctx->task = kthread_run(avm_pa_thread, 0, "avm_pa");
      if (IS_ERR(ctx->task)) {
         printk(KERN_CRIT "avm_pa: failed to start task\n");
         ctx->task = 0;
      }
   }
}

static void avm_pa_thread_stop(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   if (ctx->task) {
      (void)kthread_stop(ctx->task);
      ctx->task = 0;
   }
}
#endif

static void avm_pa_enable(void)
{
   struct avm_pa_global *ctx = &pa_glob;
#if AVM_PA_TOKSTATS
   avm_pa_thread_start();
#endif
   avm_pa_setup_est();
   avm_pa_setup_cputime_est();
   avm_pa_tbf_init(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
   avm_pa_start_lc_timer();
}

static void avm_pa_disable(void)
{
   avm_pa_tbf_exit();
#if AVM_PA_TOKSTATS
   avm_pa_thread_stop();
#endif
   avm_pa_unsetup_cputime_est();
   avm_pa_unsetup_est();
   avm_pa_stop_lc_timer();
}

#ifdef CONFIG_PROC_FS
/* ------------------------------------------------------------------------ */
/* -------- procfs functions ---------------------------------------------- */
/* ------------------------------------------------------------------------ */

static int brief_show(struct seq_file *m, void *v)
{
    pa_show_brief((pa_fprintf *)seq_printf, m);
    return 0;
}

static int brief_show_open(struct inode *inode, struct file *file)
{
    return single_open(file, brief_show, PDE(inode)->data);
}

static const struct file_operations brief_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    .owner   = THIS_MODULE,
#endif
    .open    = brief_show_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

/* ------------------------------------------------------------------------ */

static int status_show(struct seq_file *m, void *v)
{
    pa_show_status((pa_fprintf *)seq_printf, m);
    return 0;
}

static int status_show_open(struct inode *inode, struct file *file)
{
    return single_open(file, status_show, PDE(inode)->data);
}

static const struct file_operations status_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    .owner   = THIS_MODULE,
#endif
    .open    = status_show_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

/* ------------------------------------------------------------------------ */

static int stats_show(struct seq_file *m, void *v)
{
    pa_show_stats((pa_fprintf *)seq_printf, m);
    return 0;
}

static int stats_show_open(struct inode *inode, struct file *file)
{
    return single_open(file, stats_show, PDE(inode)->data);
}

static const struct file_operations stats_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    .owner   = THIS_MODULE,
#endif
    .open    = stats_show_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

/* ------------------------------------------------------------------------ */

static int pids_show(struct seq_file *m, void *v)
{
    pa_show_pids((pa_fprintf *)seq_printf, m);
    return 0;
}

static int pids_show_open(struct inode *inode, struct file *file)
{
    return single_open(file, pids_show, PDE(inode)->data);
}

static const struct file_operations pids_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    .owner   = THIS_MODULE,
#endif
    .open    = pids_show_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

/* ------------------------------------------------------------------------ */

static int vpids_show(struct seq_file *m, void *v)
{
    pa_show_vpids((pa_fprintf *)seq_printf, m);
    return 0;
}

static int vpids_show_open(struct inode *inode, struct file *file)
{
    return single_open(file, vpids_show, PDE(inode)->data);
}

static const struct file_operations vpids_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    .owner   = THIS_MODULE,
#endif
    .open    = vpids_show_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

/* ------------------------------------------------------------------------ */

struct handle_iter {
    unsigned short handle;  
};

static inline unsigned short
next_session(struct avm_pa_global *ctx, unsigned short handle)
{
   while (++handle < CONFIG_AVM_PA_MAX_SESSION) {
      if (pa_session_get(handle))
         return handle;
   }
   return 0;
}

static void *sess_show_seq_start(struct seq_file *seq, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   loff_t i;

   if ((it->handle = next_session(ctx, 0)) == 0)
      return 0;
   for (i = 0; i < *pos; i++) {
      if ((it->handle = next_session(ctx, it->handle)) == 0)
         return 0;
   }
   return PA_SESSION(ctx, it->handle);
}

static void *sess_show_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   ++*pos;
   if ((it->handle = next_session(ctx, it->handle)) == 0)
    return 0;
   return PA_SESSION(ctx, it->handle);
}

static void sess_show_seq_stop(struct seq_file *seq, void *v)
{
}

static int sess_show_seq_show(struct seq_file *seq, void *v)
{
   struct avm_pa_global *ctx = &pa_glob;
   const struct handle_iter *it = seq->private;
   seq_printf(seq, "\n");
   pa_show_session(PA_SESSION(ctx, it->handle),
                   (pa_fprintf *)seq_printf, seq);
   return 0;
}

static struct seq_operations sess_show_seq_ops = {
   .start = sess_show_seq_start,
   .next  = sess_show_seq_next,
   .stop  = sess_show_seq_stop,
   .show  = sess_show_seq_show,
};

static int sess_show_open(struct inode *inode, struct file *file)
{
   return seq_open_private(file, &sess_show_seq_ops, sizeof(struct handle_iter));
}

static const struct file_operations sess_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    .owner   = THIS_MODULE,
#endif
    .open    = sess_show_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release_private,
};

/* ------------------------------------------------------------------------ */

static inline unsigned short 
next_bsession(struct avm_pa_global *ctx, unsigned short handle)
{
   while (++handle < CONFIG_AVM_PA_MAX_SESSION) {
      struct avm_pa_session *session;
      if ((session = pa_session_get(handle)) != 0 && session->bsession)
         return handle;
   }
   return 0;
}

static void *bsess_show_seq_start(struct seq_file *seq, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   loff_t i;

   if ((it->handle = next_bsession(ctx, 0)) == 0)
      return 0;
   for (i = 0; i < *pos; i++) {
      if ((it->handle = next_bsession(ctx, it->handle)) == 0)
         return 0;
   }
   return PA_BSESSION(ctx, it->handle);
}

static void *bsess_show_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   ++*pos;
   if ((it->handle = next_bsession(ctx, it->handle)) == 0)
    return 0;
   return PA_BSESSION(ctx, it->handle);
}

static void bsess_show_seq_stop(struct seq_file *seq, void *v)
{
}

static int bsess_show_seq_show(struct seq_file *seq, void *v)
{
   struct avm_pa_global *ctx = &pa_glob;
   const struct handle_iter *it = seq->private;
   seq_printf(seq, "\n");
   pa_show_bsession(PA_BSESSION(ctx, it->handle),
                   (pa_fprintf *)seq_printf, seq);
   return 0;
}

static struct seq_operations bsess_show_seq_ops = {
   .start = bsess_show_seq_start,
   .next  = bsess_show_seq_next,
   .stop  = bsess_show_seq_stop,
   .show  = bsess_show_seq_show,
};

static int bsess_show_open(struct inode *inode, struct file *file)
{
   return seq_open_private(file, &bsess_show_seq_ops, sizeof(struct handle_iter));
}


static const struct file_operations bsess_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    .owner   = THIS_MODULE,
#endif
    .open    = bsess_show_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release,
};

/* ------------------------------------------------------------------------ */

static inline int 
next_macaddrhash(struct avm_pa_global *ctx, int idx)
{
   while (++idx < CONFIG_AVM_PA_MAX_SESSION) {
      if (ctx->macaddr_hash[idx])
         return idx;
   }
   return 0;
}

static void *macaddr_show_seq_start(struct seq_file *seq, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   loff_t i;

   if ((it->handle = next_macaddrhash(ctx, -1)) == 0)
      return 0;
   for (i = 0; i < *pos; i++) {
      if ((it->handle = next_macaddrhash(ctx, it->handle)) == 0)
         return 0;
   }
   return ctx->macaddr_hash[it->handle];
}

static void *macaddr_show_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   ++*pos;
   if ((it->handle = next_macaddrhash(ctx, it->handle)) == 0)
      return 0;
   return ctx->macaddr_hash[it->handle];
}

static void macaddr_show_seq_stop(struct seq_file *seq, void *v)
{
}

static int macaddr_show_seq_show(struct seq_file *seq, void *v)
{
   struct avm_pa_global *ctx = &pa_glob;
   const struct handle_iter *it = seq->private;
   struct avm_pa_macaddr *p;
   char buf[128];

   seq_printf(seq, "%3d: ", it->handle);
   for (p = ctx->macaddr_hash[it->handle]; p; p = p->link) {
      mac2str(&p->mac, buf, sizeof(buf));
      seq_printf(seq, " %s (%lu %d/%s)",
                      buf, p->refcount,
                      p->pid_handle,
                      PA_PID(ctx, p->pid_handle)->cfg.name);
   }
   seq_printf(seq, "\n");
   return 0;
}

static struct seq_operations macaddr_show_seq_ops = {
   .start = macaddr_show_seq_start,
   .next  = macaddr_show_seq_next,
   .stop  = macaddr_show_seq_stop,
   .show  = macaddr_show_seq_show,
};

static int macaddr_show_open(struct inode *inode, struct file *file)
{
   return seq_open_private(file, &macaddr_show_seq_ops, sizeof(struct handle_iter));
}

static const struct file_operations macaddr_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = macaddr_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = seq_release,
};

/* ------------------------------------------------------------------------ */

static inline unsigned short
next_pid(struct avm_pa_global *ctx, unsigned short handle)
{
   while (++handle < CONFIG_AVM_PA_MAX_PID) {
      if (PA_PID(ctx, handle)->pid_handle)
         return handle;
   }
   return 0;
}

static void *pid_show_seq_start(struct seq_file *seq, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   loff_t i;

   if ((it->handle = next_pid(ctx, 0)) == 0)
      return 0;
   for (i = 0; i < *pos; i++) {
      if ((it->handle = next_pid(ctx, it->handle)) == 0)
         return 0;
   }
   return PA_PID(ctx, it->handle);
}

static void *pid_show_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct handle_iter *it = seq->private;
   ++*pos;
   if ((it->handle = next_pid(ctx, it->handle)) == 0)
      return 0;
   return PA_PID(ctx, it->handle);
}

static void pid_show_seq_stop(struct seq_file *seq, void *v)
{
}

static int hash_show_seq_show(struct seq_file *seq, void *v)
{
   struct avm_pa_global *ctx = &pa_glob;
   const struct handle_iter *it = seq->private;
   struct avm_pa_pid *pid = PA_PID(ctx, it->handle);
   struct avm_pa_session *p;
   int i;
   seq_printf(seq, "PID%-3d: %s\n",
         it->handle, PA_PID(ctx, it->handle)->cfg.name);
   for (i = 0; i < CONFIG_AVM_PA_MAX_SESSION; i++) {
      if ((p = pid->hash_sess[i]) != 0) {
         seq_printf(seq, "%3d: ", i);
         for (; p; p = p->link)
            seq_printf(seq, " %3d", p->session_handle);
         seq_printf(seq, "\n");
      }
   }
   return 0;
}

static struct seq_operations hash_show_seq_ops = {
   .start = pid_show_seq_start,
   .next  = pid_show_seq_next,
   .stop  = pid_show_seq_stop,
   .show  = hash_show_seq_show,
};

static int hash_show_open(struct inode *inode, struct file *file)
{
   return seq_open_private(file, &hash_show_seq_ops, sizeof(struct handle_iter));
}

static const struct file_operations hash_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = hash_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = seq_release_private,
};

/* ------------------------------------------------------------------------ */

static int prioack_show(struct seq_file *seq, void *v)
{
   struct avm_pa_global *ctx = &pa_glob;
   seq_printf(seq, "State            : %s\n",ctx->prioack_enabled ? "enabled" : "disabled");
   seq_printf(seq, "Packet Threshold : %u\n",ctx->prioack_thresh_packets);
   seq_printf(seq, "Ratio            : %u\n",ctx->prioack_ratio);
   seq_printf(seq, "ACK Size         : %u\n",ctx->prioack_packet_size);
   seq_printf(seq, "ACK Priority     : %u\n",ctx->prioack_priority);
   seq_printf(seq, "Detected ACKs    : %u\n",ctx->prioack_acks);
   seq_printf(seq, "Accelerated ACKs : %u\n",ctx->prioack_accl_acks);
   return 0;
}

static int prioack_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, prioack_show, PDE(inode)->data);
}


static const struct file_operations prioack_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = prioack_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = seq_release_private,
};

/* ------------------------------------------------------------------------ */

#if AVM_PA_TOKSTATS
static int tstats_show(struct seq_file *m, void *v)
{
   pa_show_tstats((pa_fprintf *)seq_printf, m);
   return 0;
}

static int tstats_show_open(struct inode *inode, struct file *file)
{
   return single_open(file, tstats_show, PDE(inode)->data);
}

static const struct file_operations tstats_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
   .owner   = THIS_MODULE,
#endif
   .open    = tstats_show_open,
   .read    = seq_read,
   .llseek  = seq_lseek,
   .release = seq_release,
};
#endif

/* ------------------------------------------------------------------------ */

static void pa_dev_transmit(void *arg, struct sk_buff *skb)
{
   int rc;
   skb->dev = (struct net_device *)arg;
   rc = dev_queue_xmit(skb);
   if (rc != 0 && net_ratelimit()) {
      pa_printk(KERN_ERR, "pa_dev_transmit(%s) %d",
            ((struct net_device *)arg)->name, rc);
   }
}

static int avm_pa_write_cmds (struct file *file, const char *buffer,
      unsigned long count, void *data)
{
   struct avm_pa_global *ctx = &pa_glob;
   char    pp_cmd[100];
   char*   argv[10];
   int     argc = 0;
   char*   ptr_cmd;
   char*   delimitters = " \n\t";
   char*   ptr_next_tok;

   /* Validate the length of data passed. */
   if (count > 100)
      count = 100;

   /* Initialize the buffer before using it. */
   memset ((void *)&pp_cmd[0], 0, sizeof(pp_cmd));
   memset ((void *)&argv[0], 0, sizeof(argv));

   /* Copy from user space. */
   if (copy_from_user (&pp_cmd, buffer, count))
      return -EFAULT;

   ptr_next_tok = &pp_cmd[0];
   ptr_cmd = strsep(&ptr_next_tok, delimitters);
   if (ptr_cmd == NULL)
      return -1;

   do
   {
      argv[argc++] = ptr_cmd;

      if (argc >=10)
      {
         printk(KERN_ERR "avm_pa: too many parameters dropping the command\n");
         return -EIO;
      }

      ptr_cmd = strsep(&ptr_next_tok, delimitters);
      if (ptr_cmd && ptr_cmd[0] == 0)
         ptr_cmd = NULL;
   } while (ptr_cmd != NULL);

   argc--;

   /* enable | disable | testmode */
   if (strcmp(argv[0], "enable") == 0) {
      ctx->fw_disabled = 0;
      ctx->disabled = 0;
      avm_pa_enable();
      printk(KERN_DEBUG "avm_pa: enabled\n");
   } else if (strcmp(argv[0], "disable") == 0) {
      ctx->disabled = 1;
      ctx->fw_disabled = 1;
      avm_pa_disable();
      avm_pa_flush_sessions();
      printk(KERN_DEBUG "avm_pa: disabled\n");
   } else if (strcmp(argv[0], "testmode") == 0) {
      ctx->fw_disabled = 1;
      ctx->disabled = 0;
      avm_pa_disable();
      printk(KERN_DEBUG "avm_pa: testmode\n");

   /* hw_enable | hw_disable */
   } else if (strcmp(argv[0], "hw_enable") == 0) {
      ctx->hw_ppa_disabled = 0;
      printk(KERN_DEBUG "avm_pa: hw enabled\n");
   } else if (strcmp(argv[0], "hw_disable") == 0) {
      ctx->hw_ppa_disabled = 1;
      printk(KERN_DEBUG "avm_pa: hw disabled\n");

   /* flush */
   } else if (strcmp(argv[0], "flush") == 0) {
      if (argv[1]) {
         avm_vpid_handle vpid_handle = simple_strtoul(argv[1], 0, 10);
         if (   vpid_handle
             && PA_VPID(ctx, vpid_handle)->vpid_handle == vpid_handle) {
            avm_pa_flush_sessions_for_vpid(vpid_handle);
            printk(KERN_DEBUG "avm_pa: flush %u\n", (unsigned)vpid_handle);
         } else {
            printk(KERN_DEBUG "avm_pa: flush %s: illegal vpid\n", argv[1]);
         }
      } else {
         avm_pa_flush_sessions();
         printk(KERN_DEBUG "avm_pa: flush\n");
      }

   /* loadcontrol | noloadcontrol */
   } else if (strcmp(argv[0], "loadcontrol") == 0) {
      if (argv[1]) {
         if (strcmp(argv[1], "irq") == 0) {
            ctx->load_control = LOADCONTROL_IRQ;
         } else if (strcmp(argv[1], "idle") == 0) {
            ctx->load_control = LOADCONTROL_IDLE;
         } else if (strcmp(argv[1], "off") == 0) {
            ctx->load_control = LOADCONTROL_OFF;
         } else {
            ctx->load_control = LOADCONTROL_POWERIRQ;
         }
      } else {
         ctx->load_control = LOADCONTROL_POWERIRQ;
      }
      if (   ctx->load_control == LOADCONTROL_OFF 
          || (   (ctx->load_control & LOADCONTROL_POWER)
              && ctx->load_reduce == 0)) {
         avm_pa_tbf_disable();
      } else {
         ctx->rate = ctx->maxrate;
         avm_pa_start_lc_timer();
         if ((ctx->load_control & LOADCONTROL_POWER) && ctx->load_reduce) {
            avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
            ctx->tbf_enabled = 1;
         }
      }
      switch (ctx->load_control) {
         case LOADCONTROL_OFF:
            printk(KERN_DEBUG "avm_pa: loadcontrol off\n");
            break;
         case LOADCONTROL_IRQ:
            printk(KERN_DEBUG "avm_pa: loadcontrol irq\n");
            break;
         case LOADCONTROL_IDLE:
            printk(KERN_DEBUG "avm_pa: loadcontrol idle\n");
            break;
         case LOADCONTROL_POWERIRQ:
            printk(KERN_DEBUG "avm_pa: loadcontrol powerirq\n");
            break;
      }

   } else if (strcmp(argv[0], "noloadcontrol") == 0) {
      ctx->load_control = LOADCONTROL_OFF;
      avm_pa_tbf_disable();
      printk(KERN_DEBUG "avm_pa: loadcontrol off\n");

   /* tbfenable | tbfdisable */
   } else if (strcmp(argv[0], "tbfenable") == 0) {
      ctx->tbf_enabled = 1;
      printk(KERN_DEBUG "avm_pa: tbf enabled\n");
   } else if (strcmp(argv[0], "tbfdisable") == 0) {
      ctx->tbf_enabled = 0;
      printk(KERN_DEBUG "avm_pa: tbf disabled\n");

   /* mswin 800 900 */
   } else if (strcmp(argv[0], "mswin") == 0) {
      unsigned mswin;
      if (argv[1]) {
         mswin = simple_strtoul(argv[1], 0, 10);
         if (mswin > 0) ctx->irq_mswin_low = mswin;
      }
      if (argv[2]) {
         mswin = simple_strtoul(argv[2], 0, 10);
         if (mswin > 0) ctx->irq_mswin_high = mswin;
      }
      printk(KERN_DEBUG "avm_pa: mswin %u %u\n", 
                        ctx->irq_mswin_low, ctx->irq_mswin_high);
   /* idlewin 10 20 */
   } else if (strcmp(argv[0], "idlewin") == 0) {
      unsigned mswin;
      if (argv[1]) {
         mswin = simple_strtoul(argv[1], 0, 10);
         if (mswin > 0) ctx->idle_mswin_low = mswin;
      }
      if (argv[2]) {
         mswin = simple_strtoul(argv[2], 0, 10);
         if (mswin > 0) ctx->idle_mswin_high = mswin;
      }
      printk(KERN_DEBUG "avm_pa: idlewin %u %u\n", 
                        ctx->idle_mswin_low, ctx->idle_mswin_high);
   /* ewma 0-31 */
   } else if (strcmp(argv[0], "ewma") == 0) {
      if (argv[1]) {
         unsigned ewma = simple_strtoul(argv[1], 0, 10);
         if (ewma <= 31) {
            struct avm_pa_cputime_est *e;
            ctx->cputime_ewma_log = ewma;
            e = &ctx->cputime_user_est;
            e->ewma_log = ctx->cputime_ewma_log;
            e = &ctx->cputime_idle_est;
            e->ewma_log = ctx->cputime_ewma_log;
            e = &ctx->cputime_irq_est;
            e->ewma_log = ctx->cputime_ewma_log;
            printk(KERN_DEBUG "avm_pa: ewma %d\n", ctx->cputime_ewma_log);
         }
      }

   /* rate pps */
   } else if (strcmp(argv[0], "rate") == 0) {
      if (argv[1]) {
         unsigned rate = simple_strtoul(argv[1], 0, 10);
         if (rate > 0) {
            ctx->rate = rate;
            ctx->maxrate = rate;
            avm_pa_tbf_update(ctx->rate, ctx->pktbuffer,
                  ctx->pktpeak);
            if (ctx->load_control == 0) {
               if (ctx->tbf_enabled == 0) {
                  ctx->tbf_enabled = 1;
                  avm_pa_tbf_reset();
               }
            }
            printk(KERN_DEBUG "avm_pa: rate %u\n", ctx->rate);
         }
      }

   /* buffer pkts */
   } else if (strcmp(argv[0], "buffer") == 0) {
      if (argv[1]) {
         unsigned pktbuffer = simple_strtoul(argv[1], 0, 10);
         if (pktbuffer > 0) {
            ctx->pktbuffer = pktbuffer;
            avm_pa_tbf_update(ctx->rate, ctx->pktbuffer, ctx->pktpeak);
            printk(KERN_DEBUG "avm_pa: buffer %u\n", ctx->pktbuffer);
         }
      }

   /* peak pkts */
   } else if (strcmp(argv[0], "peak") == 0) {
      if (argv[1]) {
         unsigned peak = simple_strtoul(argv[1], 0, 10);
         if (buffer > 0) {
            ctx->pktpeak = peak;
            avm_pa_tbf_update(ctx->rate, ctx->pktbuffer,
                  ctx->pktpeak);
            printk(KERN_DEBUG "avm_pa: peak %u\n", ctx->pktpeak);
         }
      }
   } else if (strcmp(argv[0], "treduce") == 0) {
      unsigned reduce;
      if (argv[1]) {
         reduce = simple_strtoul(argv[1], 0, 10);
         if (reduce > 0 && reduce <= 80)
            ctx->telephony_reduce = reduce;
      }
      printk(KERN_DEBUG "avm_pa: telephony_reduce %u\n", 
                        ctx->telephony_reduce);
   } else if (strcmp(argv[0], "sipactive") == 0) {
      int sip_is_active;
      if (argv[1]) {
         sip_is_active = simple_strtoul(argv[1], 0, 10);
         avm_pa_sip_is_active(sip_is_active);
         printk(KERN_DEBUG "avm_pa: sip telephony is %sactive\n", 
                           sip_is_active ? "" : "not ");
      }

   /* nodbg */
   } else if (strcmp(argv[0], "nodbg") == 0) {
      ctx->dbgcapture = 0;
      ctx->dbgsession = 0;
      ctx->dbgnosession = 0;
      ctx->dbgtrace = 0;
      ctx->dbgmatch = 0;
      ctx->dbgcputime = 0;
      ctx->dbgprioack = 0;
      printk(KERN_DEBUG "avm_pa: all debugs off\n");

   /* dbgcapture | nodbgcapture */
   } else if (strcmp(argv[0], "dbgcapture") == 0) {
      ctx->dbgcapture = 1;
      ctx->misc_is_open = 0;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);
   } else if (strcmp(argv[0], "nodbgcapture") == 0) {
      ctx->dbgcapture = 0;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);

   /* dbgsession | nodbgsession */
   } else if (strcmp(argv[0], "dbgsession") == 0) {
      ctx->dbgsession = 1;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);
   } else if (strcmp(argv[0], "nodbgsession") == 0) {
      ctx->dbgsession = 0;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);

   /* dbgnosession | nodbgnosession */
   } else if (strcmp(argv[0], "dbgnosession") == 0) {
      ctx->dbgnosession = 1;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);
   } else if (strcmp(argv[0], "nodbgnosession") == 0) {
      ctx->dbgnosession = 0;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);

   /* trace | notrace */
   } else if (strcmp(argv[0], "trace") == 0) {
#if AVM_PA_TRACE
      ctx->dbgtrace = 1;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);
#else
      printk(KERN_ERR "avm_pa: trace not compiled in\n");
#endif
   } else if (strcmp(argv[0], "notrace") == 0) {
      ctx->dbgtrace = 0;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);

   /* dbgmatch | nodbgmatch */
   } else if (strcmp(argv[0], "nodbgmatch") == 0) {
      ctx->dbgmatch = 0;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);
   } else if (strcmp(argv[0], "dbgmatch") == 0) {
      ctx->dbgmatch = 1;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);

   /* dbgcputime | nodbgcputime */
   } else if (strcmp(argv[0], "nodbgcputime") == 0) {
      ctx->dbgcputime = 0;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);
   } else if (strcmp(argv[0], "dbgcputime") == 0) {
      ctx->dbgcputime = 1;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);

   /* dbgprioack | nodbgprioack */
   } else if (strcmp(argv[0], "dbgprioack") == 0) {
      ctx->dbgprioack = 1;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);
   } else if (strcmp(argv[0], "nodbgprioack") == 0) {
      ctx->dbgprioack = 0;
      printk(KERN_DEBUG "avm_pa: %s\n", argv[0]);

   /* pid <device> */
   } else if (strcmp(argv[0], "pid") == 0 && argv[1]) {
      struct net_device *dev = dev_get_by_name(&init_net, argv[1]);

      if (dev) {
         struct avm_pa_pid_cfg cfg;
         snprintf(cfg.name, sizeof(cfg.name), "%s", argv[1]);
         cfg.framing = avm_pa_framing_dev;
         cfg.default_mtu = 1500;
         cfg.tx_func = pa_dev_transmit;
         cfg.tx_arg = dev;
         if (avm_pa_dev_pid_register(AVM_PA_DEVINFO(dev), &cfg) < 0)
            printk(KERN_ERR "%s: failed to register PA PID\n", argv[1]);
         dev_put(dev);
      } else {
         printk(KERN_ERR "avm_pa_write_cmds(pid): dev %s not found\n", argv[1]);
      }

   /* vpid <device> */
   } else if (strcmp(argv[0], "vpid") == 0 && argv[1]) {
      struct net_device *dev = dev_get_by_name(&init_net, argv[1]);

      if (dev) {
         struct avm_pa_vpid_cfg cfg;
         snprintf(cfg.name, sizeof(cfg.name), "%s", argv[1]);
         cfg.v4_mtu = 1500;
         cfg.v6_mtu = 1500;
         if (avm_pa_dev_vpid_register(AVM_PA_DEVINFO(dev), &cfg) < 0)
            printk(KERN_ERR "%s: failed to register PA VPID\n", argv[1]);
         dev_put(dev);
      } else {
         printk(KERN_ERR "avm_pa_write_cmds(vpid): dev %s not found\n", argv[1]);
      }

   /* unreg <device> */
   } else if (strcmp(argv[0], "unreg") == 0 && argv[1]) {
      struct net_device *dev = dev_get_by_name(&init_net, argv[1]);

      if (dev) {
         avm_pa_dev_unregister(AVM_PA_DEVINFO(dev));
         dev_put(dev);
      } else {
         printk(KERN_ERR "avm_pa_write_cmds(unreg): dev %s not found\n", argv[1]);
      }

   /* prioack <enable|disable|psize x|pthresh x|prio x|ratio x> */
   } else if (strcmp(argv[0], "prioack") == 0) {
      unsigned val = 0;

      if (argv[1]) {
         printk(KERN_DEBUG "avm_pa: prioack %s %s\n", 
                           argv[1], argv[2] ? argv[2] : "");
         if (strcmp(argv[1], "enable") == 0) {
            ctx->prioack_enabled = 1;
         } else if (strcmp(argv[1], "disable") == 0) {
            ctx->prioack_enabled = 0;
         } else if (strcmp(argv[1], "psize") == 0) {
            if (argv[2]) val = simple_strtoul(argv[2], 0, 10);
            if (val) ctx->prioack_packet_size = val;
         } else if (strcmp(argv[1], "pthresh") == 0) {
            if (argv[2]) val = simple_strtoul(argv[2], 0, 10);
            if (val) ctx->prioack_thresh_packets = val;
         } else if (strcmp(argv[1], "prio") == 0) {
            if (argv[2]) val = simple_strtoul(argv[2], 0, 10);
            if (val) ctx->prioack_priority = val;
         } else if (strcmp(argv[1], "ratio") == 0) {
            if (argv[2]) val = simple_strtoul(argv[2], 0, 10);
            if (val) ctx->prioack_ratio = val;
         } else {
            printk(KERN_DEBUG "avm_pa: prioack unknown command %s \n (available commands: enable,disable,psize,pthresh,prio,ratio)\n", argv[1]);
         }
      }
   } else {
      printk(KERN_ERR "avm_pa_write_cmds: %s: unknown command\n", argv[0]);
   }

   return count;
}

/* ------------------------------------------------------------------------ */

static struct proc_dir_entry *dir_entry = 0;

static void __init avm_pa_proc_init(void)
{
   struct proc_dir_entry *file_entry;

   dir_entry = proc_net_mkdir(&init_net, "avm_pa", init_net.proc_net);
   if (dir_entry) {
      dir_entry->read_proc = 0;
      dir_entry->write_proc = 0;
   }

   file_entry = create_proc_entry("control", S_IFREG|S_IWUSR, dir_entry);
   if (file_entry) {
      file_entry->data        = NULL;
      file_entry->read_proc  = NULL;
      file_entry->write_proc = avm_pa_write_cmds;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
      file_entry->owner      = THIS_MODULE;
#endif
   }

   file_entry = proc_create("brief", S_IRUGO, dir_entry, &brief_show_fops);
   file_entry = proc_create("status", S_IRUGO, dir_entry, &status_show_fops);
   file_entry = proc_create("stats", S_IRUGO, dir_entry, &stats_show_fops);
   file_entry = proc_create("pids", S_IRUGO, dir_entry, &pids_show_fops);
   file_entry = proc_create("vpids", S_IRUGO, dir_entry, &vpids_show_fops);
   file_entry = proc_create("sessions", S_IRUGO, dir_entry, &sess_show_fops);
   file_entry = proc_create("bsessions", S_IRUGO, dir_entry, &bsess_show_fops);
   file_entry = proc_create("macaddrs", S_IRUGO, dir_entry, &macaddr_show_fops);
#if AVM_PA_TOKSTATS
   file_entry = proc_create("tokstats", S_IRUGO, dir_entry, &tstats_show_fops);
#endif
   file_entry = proc_create("hashes", S_IRUGO, dir_entry, &hash_show_fops);
   file_entry = proc_create("prioack", S_IRUGO, dir_entry, &prioack_show_fops);
}

static void __exit avm_pa_proc_exit(void)
{
   remove_proc_entry("control", dir_entry);
   remove_proc_entry("brief", dir_entry);
   remove_proc_entry("status", dir_entry);
   remove_proc_entry("stats", dir_entry);
   remove_proc_entry("pids", dir_entry);
   remove_proc_entry("vpids", dir_entry);
   remove_proc_entry("sessions", dir_entry);
   remove_proc_entry("bsessions", dir_entry);
   remove_proc_entry("macaddrs", dir_entry);
#if AVM_PA_TOKSTATS
   remove_proc_entry("tokstats", dir_entry);
#endif
   remove_proc_entry("hashes", dir_entry);
   remove_proc_entry("prioack", dir_entry);
   remove_proc_entry("avm_pa", init_net.proc_net);
}
#endif

/* ------------------------------------------------------------------------ */
/* -------- misc device for capture tracking ------------------------------ */
/* ------------------------------------------------------------------------ */

static ssize_t avm_pa_misc_read(struct file *file, char __user *buf,
                                size_t count, loff_t *ppos)
{
   return 0;
}

static unsigned int avm_pa_misc_poll(struct file *file, poll_table *wait)
{
   return 0;
}

static int avm_pa_misc_open(struct inode *inode, struct file *file)
{
   struct avm_pa_global *ctx = &pa_glob;
   if (ctx->dbgcapture == 0)
      ctx->misc_is_open = 1;
   return 0;
}

static int avm_pa_misc_release(struct inode *inode, struct file *file)
{
   struct avm_pa_global *ctx = &pa_glob;
   ctx->misc_is_open = 0;
   return 0;
}


static const struct file_operations avm_pa_misc_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        .owner   =    THIS_MODULE,
#endif
        .llseek  = no_llseek,
        .read    = avm_pa_misc_read,
        .poll    = avm_pa_misc_poll,
        .open    = avm_pa_misc_open,
        .release = avm_pa_misc_release,
};

static struct miscdevice avm_pa_misc_dev = {
        .minor =    MISC_DYNAMIC_MINOR,
        .name =     "avm_pa",
        .fops =     &avm_pa_misc_fops
};

/* ------------------------------------------------------------------------ */
/* -------- init & exit functions ----------------------------------------- */
/* ------------------------------------------------------------------------ */

int __init avm_pa_init(void)
{
   struct avm_pa_global *ctx = &pa_glob;

   setup_timer(&ctx->gc_timer, pa_gc_timer_expired, 0);
   setup_timer(&ctx->est_timer, avm_pa_est_timer, 0);
   setup_timer(&ctx->cputime_est_timer, avm_pa_cputime_est_timer, 0);
   setup_timer(&ctx->lc_timer, avm_pa_lc_timer_expired, 0);
   skb_queue_head_init(&ctx->irqqueue);
   tasklet_init(&ctx->irqtasklet, avm_pa_irq_tasklet, 0);
   skb_queue_head_init(&ctx->tbfqueue);
   tasklet_init(&ctx->tbftasklet, avm_pa_tbf_tasklet, 0);

   printk(KERN_INFO "AVM PA %s\n", AVM_PA_VERSION);
   printk(KERN_INFO "AVM PA skb pktinfo at offset %u size %u\n", 
                    offsetof(struct sk_buff, avm_pa),
                    sizeof(struct avm_pa_pkt_info));

   avm_pa_init_freelist();

   if (ctx->disabled == 0)
      avm_pa_enable();

   if (misc_register(&avm_pa_misc_dev) < 0)
      printk(KERN_ERR "avm_pa: misc_register() failed");

#ifdef CONFIG_PROC_FS
   avm_pa_proc_init();
#endif
#ifdef CONFIG_AVM_POWERMETER
   ctx->load_control_handle =
       avm_powermanager_load_control_register("avm_pa",
                                              avm_pa_load_control_cb,
                                              0);
#endif
   return 0;
}

void __exit avm_pa_exit(void)
{
   struct avm_pa_global *ctx = &pa_glob;
   struct avm_pa_session *session;
   struct sk_buff *skb;

#ifdef CONFIG_AVM_POWERMETER
   if (ctx->load_control_handle) {
      avm_powermanager_load_control_release(ctx->load_control_handle);
      ctx->load_control_handle = 0;
   }
#endif

   ctx->disabled = 1;
   ctx->fw_disabled = 1;
   avm_pa_disable();
   tasklet_kill(&ctx->irqtasklet);
   while ((skb = skb_dequeue(&ctx->irqqueue)) != 0)
      kfree_skb(skb);
   while ((skb = skb_dequeue(&ctx->tbfqueue)) != 0) {
      session = pa_session_get(AVM_PKT_INFO(skb)->session_handle);
      if (session)
         atomic_dec(&session->skb_in_tbfqueue);
      kfree_skb(skb);
   }

   pa_session_gc(1);
   pa_session_gc(1);
#ifdef CONFIG_PROC_FS
   avm_pa_proc_exit();
#endif
   misc_deregister(&avm_pa_misc_dev);
   avm_pa_reset_stats();
}

module_init(avm_pa_init);
module_exit(avm_pa_exit);
