/*
 * RTP Timestamp
 *
 * vim:set expandtab shiftwidth=3 softtabstop=3:
 *
 * Copyright (c) 2012-2013 AVM GmbH <info@avm.de>
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
 
#include <linux/version.h>
#include <linux/types.h>
#include <linux/jhash.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <asm/unaligned.h>
#include <net/checksum.h>
#include <net/pkt_sched.h>
#include <linux/hrtimer.h>
#include <linux/kallsyms.h> // sprint_symbol()
#include <linux/miscdevice.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <linux/rtp_timestamp.h>

#include <linux/ctype.h>


#define MAGIC_TIMESTAMP_START           0x434D5453UL
#define MAGIC_TIMESTAMP_END           (~0x434D5453UL)

#define MAGIC_TIMESTAMP_START_HOST      0x4D435354UL
#define MAGIC_TIMESTAMP_END_HOST      (~0x4D435354UL)

#define MAX_STAT_INSTANCE             8

#define constant_htons(x)   __constant_htons(x)

/* ------------------------------------------------------------------------ */

#define RTP_TS_USE_IRQLOCK

static DEFINE_RWLOCK(rtp_ts_lock);

#ifdef RTP_TS_USE_IRQLOCK
#define RTP_TS_LOCK_DECLARE unsigned long flags
#define RTP_TS_WRITE_LOCK() write_lock_irqsave(&rtp_ts_lock, flags)
#define RTP_TS_READ_LOCK()  read_lock_irqsave(&rtp_ts_lock, flags)
#define RTP_TS_WRITE_UNLOCK() write_unlock_irqrestore(&rtp_ts_lock, flags)
#define RTP_TS_READ_UNLOCK() read_unlock_irqrestore(&rtp_ts_lock, flags)
#else
#define RTP_TS_LOCK_DECLARE
#define RTP_TS_WRITE_LOCK() write_lock_bh(&rtp_ts_lock)
#define RTP_TS_READ_LOCK()  read_lock_bh(&rtp_ts_lock)
#define RTP_TS_WRITE_UNLOCK() write_unlock_bh(&rtp_ts_lock)
#define RTP_TS_READ_UNLOCK() read_unlock_bh(&rtp_ts_lock)
#endif

/**********************************************************/
/* network header defines */
/**********************************************************/

struct vlanhdr {
    u16 vlan_tci;
#define VLAN_ID(p)   (ntohs((p)->vlan_tci) & 0xfff)
#define VLAN_PRIO(p) (ntohs((p)->vlan_tci) >> 13)
#define VLAN_CFI(p)  ((ntohs((p)->vlan_tci) & 0x1000) ? 1 : 0)
    u16 vlan_proto;
};

/**********************************************************/

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

/**********************************************************/

#define RTP_HDR_MAX_CSRC   16

struct rtp_header {
#ifdef __BIG_ENDIAN
   u8    version:2,
         padbit:1,
         extbit:1,
         cc:4;
   u8    markbit:1,
         paytype:7;
#else
   u8    cc:4,
         extbit:1,
         padbit:1,
         version:2;
   u8    paytype:7,
         markbit:1;
#endif
   u16   seq_number;
   u32   timestamp;
   u32   ssrc;
   u32   csrc[RTP_HDR_MAX_CSRC];
};

/**********************************************************/

struct _magic_timestamp {
   unsigned int tsmstart;
   struct timeval tv;
   unsigned int tsmend;
} __attribute__((packed));


static void rtp_timestamp_proc_init(void);

/**********************************************************************/
/* local variables */
/**********************************************************************/

static struct rtp_timestamp_global {
   int                       disabled;
   int                       proc_inited;
   struct _rtp_timestamp_stat timestamp_stat[MAX_STAT_INSTANCE];
} rtp_timestamp_glob = {
   .disabled = 0,
   .proc_inited = 0,
};


/**********************************************************************/
/* local functions */
/**********************************************************************/
#define IPHLEN(iph)          (((((u8 *)iph)[0])&0xf)<<2)
static inline void set_ip_checksum(struct iphdr *iph)
{
   int iphlen = IPHLEN(iph);
   iph->check = 0;
   iph->check = csum_fold(csum_partial((unsigned char *)iph, iphlen, 0));
}

static inline void set_udp_checksum(struct iphdr *iph)
{
   int iphlen = IPHLEN(iph);
   struct udphdr *udph = (struct udphdr *)(((char *)iph)+iphlen);
   u16 len = ntohs(iph->tot_len)-iphlen; /* ntohs(udph->len); */
   unsigned int sum;

   udph->check = 0;
   
   sum = csum_partial((unsigned char *)udph, len, 0);
   sum = csum_tcpudp_magic(iph->saddr, iph->daddr, len, iph->protocol, sum);
   udph->check = sum ? sum : 0xffff;
}

#if defined(CONFIG_IPV6) 
static int _ip6_is_ext_hdr(u8 nexthdr)
{
   return    nexthdr == IPPROTO_HOPOPTS || nexthdr == IPPROTO_ROUTING
          || nexthdr == IPPROTO_FRAGMENT || nexthdr == IPPROTO_NONE
          || nexthdr == IPPROTO_ESP || nexthdr == IPPROTO_AH
          || nexthdr == IPPROTO_DSTOPTS;
}

static int _skip_exthdr(struct ipv6hdr *ipv6h, int ipproto, unsigned short *plenp)
{
   u8 nexthdr          = ipv6h->nexthdr;
   unsigned short plen = ntohs(ipv6h->payload_len); /* length without ipv6hdr */
   int offset          = sizeof(struct ipv6hdr);

   while (_ip6_is_ext_hdr(nexthdr)) {
      struct ipv6_opt_hdr *hp;
      int hdrlen;

      if (plen < (int)sizeof(struct ipv6_opt_hdr)) {
         return -1;
      }

      /* if wanted nexthdr is found -> break */
      if (nexthdr == ipproto)
         break;

      hp = (struct ipv6_opt_hdr *)(ipv6h + offset);
      if (nexthdr == IPPROTO_AH)
         hdrlen = (hp->hdrlen + 2) << 2;
      else
         hdrlen = (hp->hdrlen + 1) << 3;

      offset += hdrlen; /* offset to next_opt_hdr */
      plen   -= hdrlen; /* payload without this ext-hdr */
      nexthdr = hp->nexthdr;
   }

   *plenp = plen;
   return offset;
}

static void set_ipv6_udp_checksum(struct ipv6hdr *ipv6h)
{
   /* 
    * RFC 2460 - 8.1 Upper-Layer Checksums:
    * ...
    * The Next Header value in the pseudo-header identifies the upper-layer
    * protocol (e.g., 6 for TCP, or 17 for UDP).  It will differ from the
    * Next Header value in the IPv6 header if there are extension headers
    * between the IPv6 header and the upper-layer header.
    *
    * ... the length used in the pseudo-header is the Payload Length from the
    * IPv6 header, minus the length of any extension headers present between
    * the IPv6 header and the upper-layer header.
    */
   unsigned short plen_without_exthdr = 0;
   struct udphdr *udph;
   int offset = _skip_exthdr(ipv6h, IPPROTO_UDP, &plen_without_exthdr);

   udph = (struct udphdr *)(((u8 *)ipv6h) + offset);
   udph->check = 0; 	 
   udph->check = csum_ipv6_magic(&ipv6h->saddr, &ipv6h->daddr,
                                 plen_without_exthdr, IPPROTO_UDP,
                                 csum_partial((u8 *)udph, plen_without_exthdr, 0)); 
}
#endif /* #if defined(CONFIG_IPV6) */



#if 0

struct uhexdumpstyledef {
   unsigned int  bytesperline; /* # bytes shown each line */
   unsigned int  blocksize;    /* insert space after 'blocksize' bytes */
   int           decoration;   /* */
   char         *newline;      /* */
};

static struct uhexdumpstyledef defstyle = { 20, 4, 1, "\n" };
   
int uhexdump(char *returnbuf, int size,
             unsigned char *databuf, int len,
             struct uhexdumpstyledef *style)
{
   int i, reallen, nllen;
   unsigned char *end = (unsigned char *)returnbuf+size-1;
   unsigned char *s;
   static char hexchars[] = "0123456789ABCDEF";
   static char htmlspecial[] = "<>&";
   char *nl;

   if (!style) style = &defstyle;
   if (style->newline) nl = style->newline;
   else nl = "\n";
   nllen = strlen(nl);

   reallen = len;
   s = (unsigned char *)returnbuf;
   len = reallen + (style->bytesperline-(reallen%style->bytesperline));

   for (i=0; i < len && s + 3 < end; i ++) {
      if (style->decoration) {
         if ((i % style->bytesperline) == 0) {
            if (s + 8 >= end)
               break;
            sprintf((char *)s, "%04x ", i);
            s += strlen((char *)s);
         }
      }
      if (style->blocksize) {
         if ((i % style->blocksize) == 0)
            *s++ = ' ';
      }
      if (i < reallen) {
         *s++ = hexchars[(databuf[i] >> 4) & 0xf];
         *s++ = hexchars[databuf[i] & 0xf];
      } else if (style->decoration) {
         *s++ = ' ';
         *s++ = ' ';
      } else {
         break;
      }

      if (i > 0 && ((i+1)%style->bytesperline) == 0) {
         if (style->decoration) {
            unsigned char *xp = (unsigned char *)&databuf[i+1-style->bytesperline];
            unsigned int j;
            if (s + 2 + style->bytesperline + nllen >= end)
               break;
            *s++ = ' ';
            *s++ = ' ';
            for (j = 0; j < style->bytesperline && xp < (unsigned char *)databuf+reallen; j++,xp++) {
               if (   isprint(*xp)
                   && isascii(*xp)
                   && strchr(htmlspecial, *xp) == 0) {
                  *s = *xp;
               } else {
                  *s = '.';
               }
               s++;
            }
         } else {
            if (s + nllen >= end)
               break;
         }
         strcpy((char *)s, nl);
         s += nllen;
      }
   }
   *s = 0;
   return ((char *)s)-returnbuf;
}

static void show_packet(char *s, unsigned char *data, size_t len)
{
   char buf[4096];
   uhexdump(buf, sizeof(buf), data, len, 0);
   printk(KERN_ERR "%s:\n%s\n\n", s, buf);
}
#else /* if 0 */
#define show_packet(s, data, len) do { } while (0)
#endif


static inline void rtp_skb_free(struct sk_buff *skb)
{
   if (skb == 0)
   BUG();

   dev_kfree_skb_any(skb);
}


static struct sk_buff *rtp_skb_resize(struct sk_buff *skb, int tdiff)
{
   if (tdiff != 0) {
      if (tdiff > 0) {
         if (skb_tailroom(skb) < tdiff) {
            struct sk_buff *nskb;
            if ((nskb = skb_copy_expand(skb, skb_headroom(skb),
               tdiff+SKB_DATA_ALIGN(32), GFP_ATOMIC)) == 0) {
               printk(KERN_ERR "skb_resize(%p,%d): skb_copy_expand failed\n",
                      skb, tdiff);
               rtp_skb_free(skb);
               return 0;
            }
            //copy all with memcpy
            memcpy(nskb->cb, skb->cb, sizeof(nskb->cb));
            rtp_skb_free(skb);
            skb = nskb;
         }
         skb_put(skb, (unsigned int)tdiff);
      } else {
         skb_trim(skb, skb->len+tdiff);
      }
   }
   return skb;
}


static inline unsigned long extract_le_unaligned_dword(void *_source) {
   unsigned char *source = (unsigned char*)_source;
   return
        ((unsigned long)source[0] <<  0) |
        ((unsigned long)source[1] <<  8) |
        ((unsigned long)source[2] << 16) |
        ((unsigned long)source[3] << 24) |
        0;
}

static inline void *copy_dword_to_le_unaligned(void *_dest, unsigned long source) {
   unsigned char *dest = (unsigned char *)_dest;
   dest[0] = (unsigned char)(source >> 0);
   dest[1] = (unsigned char)(source >> 8);
   dest[2] = (unsigned char)(source >> 16);
   dest[3] = (unsigned char)(source >> 24);
   return dest + sizeof(unsigned long);
}

static inline struct _rtp_timestamp_stat *get_timestamp_session(unsigned short rtp_port) 
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   int i;
   RTP_TS_LOCK_DECLARE;
    
   if (unlikely(!rtp_port)) return 0;
    
   RTP_TS_READ_LOCK();

   for (i = 0; i < MAX_STAT_INSTANCE; i++) {
      if (ctx->timestamp_stat[i].session == rtp_port) {
         RTP_TS_READ_UNLOCK();
         return &ctx->timestamp_stat[i];
      }
   }

   RTP_TS_READ_UNLOCK();
   return 0;
}

static int set_timestamp(struct rtp_header *rtphdr, unsigned int datalen) 
{
   struct timeval tv;
   struct _magic_timestamp *pcmts = (struct _magic_timestamp *)((u8 *)rtphdr+datalen);
   u8 *lenptr;
   RTP_TS_LOCK_DECLARE;

   do_gettimeofday(&tv);

   RTP_TS_WRITE_LOCK();

   rtphdr->padbit = 1;
    
   copy_dword_to_le_unaligned(&(pcmts->tsmstart), MAGIC_TIMESTAMP_START);
   copy_dword_to_le_unaligned(&(pcmts->tv.tv_sec), tv.tv_sec);
   copy_dword_to_le_unaligned(&(pcmts->tv.tv_usec), tv.tv_usec);
   copy_dword_to_le_unaligned(&(pcmts->tsmend), MAGIC_TIMESTAMP_END);
   lenptr = (u8 *)(pcmts+1);
   *lenptr = sizeof(struct _magic_timestamp)+1;

   RTP_TS_WRITE_UNLOCK();

   return (datalen + sizeof(struct _magic_timestamp)+1);
}

static void set_timestamp_stat(struct _rtp_timestamp_stat *stat, signed long val, unsigned short is_outgoing) 
{
   RTP_TS_LOCK_DECLARE;
    
   RTP_TS_WRITE_LOCK();
    
   if (is_outgoing) {
      if(val > stat->outgoing_max) stat->outgoing_max = val;
      if(val < stat->outgoing_min) stat->outgoing_min = val;
      stat->outgoing_sum += val;
      stat->outgoing_quadsum += (val * val);        
      stat->outgoing_cnt++;	
   } else {
      if(val > stat->incoming_max) stat->incoming_max = val;
      if(val < stat->incoming_min) stat->incoming_min = val;
      stat->incoming_sum += val;
      stat->incoming_quadsum += (val * val);        
      stat->incoming_cnt++;
   }
   RTP_TS_WRITE_UNLOCK();
}

static struct _rtp_timestamp_stat *parse_timestamp(unsigned short rtp_port, struct rtp_header *rtphdr, unsigned int datalen, unsigned short is_outgoing) 
{    
   struct _rtp_timestamp_stat *stat;
   struct timeval tvts;
   unsigned int parsed = 0;
   u8 *data = (u8 *)rtphdr;
   struct _magic_timestamp *pcmts = (struct _magic_timestamp *)(data+datalen-sizeof(struct _magic_timestamp)-1);
        
   stat = get_timestamp_session(rtp_port);
   if (!stat || stat->state == ts_stopped) return 0;

   if(   (extract_le_unaligned_dword(&(pcmts->tsmstart)) == MAGIC_TIMESTAMP_START) 
      && (extract_le_unaligned_dword(&(pcmts->tsmend))   == MAGIC_TIMESTAMP_END)) {
      tvts.tv_sec  = extract_le_unaligned_dword(&(pcmts->tv.tv_sec)); 
      tvts.tv_usec = extract_le_unaligned_dword(&(pcmts->tv.tv_usec));
      parsed = 1;
   } else if(   (extract_le_unaligned_dword(&(pcmts->tsmstart)) == MAGIC_TIMESTAMP_START_HOST) 
             && (extract_le_unaligned_dword(&(pcmts->tsmend))   == MAGIC_TIMESTAMP_END_HOST)) {
      char buf[4];
      unsigned char *pdata = (unsigned char *)rtphdr;
      pdata = (unsigned char *)&(pcmts->tv.tv_sec);
      buf[0] = pdata[1], buf[1] = pdata[0], buf[2] = pdata[3], buf[3] = pdata[2];
      tvts.tv_sec  = extract_le_unaligned_dword(&buf); 
      pdata = (unsigned char *)&(pcmts->tv.tv_usec);
      buf[0] = pdata[1], buf[1] = pdata[0], buf[2] = pdata[3], buf[3] = pdata[2];
      tvts.tv_usec = extract_le_unaligned_dword(&buf);
      parsed = 1;
   }
   if(parsed) {
      struct timeval tvact;
      long long te, ta;
      unsigned long tdiff;
      do_gettimeofday(&tvact);
      ta    = ((long long)tvts.tv_sec  * (long long)(1000U * 1000U )) + (long long)tvts.tv_usec;
      te    = ((long long)tvact.tv_sec * (long long)(1000U * 1000U )) + (long long)tvact.tv_usec;
      tdiff = (unsigned long) ((te - ta)) / 1000; /*--- in msec ---*/
      set_timestamp_stat(stat, tdiff, is_outgoing);
      return stat;
   }
   return 0;
}

/**********************************************************/
/* exported functions */
/**********************************************************/

void rtp_timestamp_start_stat(unsigned short rtp_port, int remove_stamp_on_egress) 
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   struct _rtp_timestamp_stat *stat;
   int i;
   RTP_TS_LOCK_DECLARE;
   
   if (unlikely(!ctx->proc_inited)) rtp_timestamp_proc_init();
   
   if (unlikely(ctx->disabled)) return;

   RTP_TS_WRITE_LOCK();

   stat = get_timestamp_session(rtp_port);

   if (stat) {
      stat->state = remove_stamp_on_egress ? ts_running_with_stamp_remove : ts_running;
      stat->incoming_min = LONG_MAX;
      stat->incoming_max = LONG_MIN;
      stat->incoming_cnt = 0;
      stat->incoming_sum = 0;
      stat->incoming_quadsum = 0;
      stat->outgoing_min = LONG_MAX;
      stat->outgoing_max = LONG_MIN;
      stat->outgoing_cnt = 0;
      stat->outgoing_sum = 0;
      stat->outgoing_quadsum = 0;
      RTP_TS_WRITE_UNLOCK();
      return;
   }

   for (i = 0; i < MAX_STAT_INSTANCE; i++) {
      if (ctx->timestamp_stat[i].session == 0) {
         ctx->timestamp_stat[i].state = remove_stamp_on_egress ? ts_running_with_stamp_remove : ts_running;
         ctx->timestamp_stat[i].session = rtp_port;       
         ctx->timestamp_stat[i].incoming_min = LONG_MAX;
         ctx->timestamp_stat[i].incoming_max = LONG_MIN;
         ctx->timestamp_stat[i].incoming_cnt = 0;
         ctx->timestamp_stat[i].incoming_sum = 0;
         ctx->timestamp_stat[i].incoming_quadsum = 0;            
         ctx->timestamp_stat[i].outgoing_min = LONG_MAX;
         ctx->timestamp_stat[i].outgoing_max = LONG_MIN;
         ctx->timestamp_stat[i].outgoing_cnt = 0;
         ctx->timestamp_stat[i].outgoing_sum = 0;
         ctx->timestamp_stat[i].outgoing_quadsum = 0;
         RTP_TS_WRITE_UNLOCK();
         return;
      }
   }

   RTP_TS_WRITE_UNLOCK();
}
EXPORT_SYMBOL(rtp_timestamp_start_stat);

void rtp_timestamp_stop_stat(unsigned short rtp_port) 
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   struct _rtp_timestamp_stat *stat;
   RTP_TS_LOCK_DECLARE;
   
   if (unlikely(!ctx->proc_inited)) rtp_timestamp_proc_init();
   
   if (unlikely(ctx->disabled)) return;

   stat = get_timestamp_session(rtp_port);
   if (unlikely(!stat)) return;
   RTP_TS_WRITE_LOCK();
   stat->state = ts_stopped;
   RTP_TS_WRITE_UNLOCK();
}
EXPORT_SYMBOL(rtp_timestamp_stop_stat);


void rtp_timestamp_clear_stat(unsigned short rtp_port) 
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   struct _rtp_timestamp_stat *stat;
   int i;
   RTP_TS_LOCK_DECLARE;
   
   if (unlikely(ctx->disabled)) return;
   RTP_TS_WRITE_LOCK();

   stat = get_timestamp_session(rtp_port);
   if (stat) {
      stat->session = 0;           
      stat->incoming_min = LONG_MAX;
      stat->incoming_max = LONG_MIN;
      stat->incoming_cnt = 0;
      stat->incoming_sum = 0;
      stat->incoming_quadsum = 0;
      stat->outgoing_min = LONG_MAX;
      stat->outgoing_max = LONG_MIN;
      stat->outgoing_cnt = 0;
      stat->outgoing_sum = 0;
      stat->outgoing_quadsum = 0;        
      RTP_TS_WRITE_UNLOCK();
      return;
   }
   if (rtp_port) {
      RTP_TS_WRITE_UNLOCK();
      return;
   }

   /* clear whole statistic */
   for (i = 0; i < MAX_STAT_INSTANCE; i++) {
      ctx->timestamp_stat[i].session = 0;           
      ctx->timestamp_stat[i].incoming_min = LONG_MAX;
      ctx->timestamp_stat[i].incoming_max = LONG_MIN;
      ctx->timestamp_stat[i].incoming_cnt = 0;
      ctx->timestamp_stat[i].incoming_sum = 0;
      ctx->timestamp_stat[i].incoming_quadsum = 0;        
      ctx->timestamp_stat[i].outgoing_min = LONG_MAX;
      ctx->timestamp_stat[i].outgoing_max = LONG_MIN;
      ctx->timestamp_stat[i].outgoing_cnt = 0;
      ctx->timestamp_stat[i].outgoing_sum = 0;
      ctx->timestamp_stat[i].outgoing_quadsum = 0;        
   }

   RTP_TS_WRITE_UNLOCK();
}
EXPORT_SYMBOL(rtp_timestamp_clear_stat);

// insert in kdsld
void rtp_timestamp_insert_in_skb(struct sk_buff *skb)
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   struct _rtp_timestamp_stat *stat;
   u8 *protohdr;
   u16 proto;
   struct ethhdr *ethhdr;
   struct pppoehdr *pppoehdr;
   struct iphdr *iphdr;
#if defined(CONFIG_IPV6)
   struct ipv6hdr *ipv6hdr = 0;
#endif
   int iphlen;
   struct udphdr *udphdr;
   RTP_TS_LOCK_DECLARE;

   if (unlikely(ctx->disabled)) return;
   ethhdr = (struct ethhdr *)(skb->data);
   if (!ethhdr) return;
   proto = ethhdr->h_proto;

   if (proto == constant_htons(ETH_P_8021Q)) {
      struct vlanhdr *vlanhdr = (struct vlanhdr *)(ethhdr+1);
      proto = vlanhdr->vlan_proto;
      protohdr = (u8 *)(vlanhdr+1);
   } else {
      proto = ethhdr->h_proto;
      protohdr = (u8 *)(ethhdr+1);
   }
   if (!protohdr) return;
   if (proto == constant_htons(ETH_P_PPP_SESS)) {
      u8 *p;
      pppoehdr = (struct pppoehdr *)(protohdr);
      p = (u8 *)(pppoehdr+1);
      if (p[0] == 0x00 && p[1] == 0x21) {
         iphdr = (struct iphdr *)(p+2);
#if defined(CONFIG_IPV6)
      } else if (p[0] == 0x00 && p[1] == 0x57) {
         ipv6hdr = (struct ipv6hdr *)(p+2);
#endif            
      } else {
         return;
      }
#if defined(CONFIG_IPV6)
   } else if (proto == constant_htons(ETH_P_IPV6)) { 
      ipv6hdr = (struct ipv6hdr *)protohdr;
#endif      
   } else if (proto == constant_htons(ETH_P_IP)) {
      iphdr = (struct iphdr *)protohdr;
   } else {
      return; /* nur pppoe o. rbe */
   }
#if defined(CONFIG_IPV6)
   if (ipv6hdr) {
      if (ipv6hdr->nexthdr != IPPROTO_UDP) return;
      udphdr = (struct udphdr *)(ipv6hdr+1);
   } else {
#endif
      if (iphdr->protocol != IPPROTO_UDP) return;
      iphlen = iphdr->ihl<<2;
      udphdr = (struct udphdr *)(((char *)iphdr)+iphlen);
#if defined(CONFIG_IPV6)
   }
#endif

   RTP_TS_READ_LOCK();
   stat = get_timestamp_session(ntohs(udphdr->dest));
   if (stat && stat->state != ts_stopped) {
      int new_udp_datalen, udp_datalen;
      struct rtp_header *rtphdr = (struct rtp_header *)(udphdr+1);
      rtp_skb_resize(skb, sizeof(struct _magic_timestamp)+1); /* +1 -> len of timestamp */
      udp_datalen  = ntohs(udphdr->len)-sizeof(struct udphdr);
      new_udp_datalen = set_timestamp(rtphdr, udp_datalen);
      if (new_udp_datalen > udp_datalen) {
         int lendiff = new_udp_datalen-udp_datalen;
         udphdr->len = htons(ntohs(udphdr->len)+lendiff);
#if defined(CONFIG_IPV6)
         if (ipv6hdr) {
            ipv6hdr->payload_len = htons(ntohs(ipv6hdr->payload_len)+lendiff);
            set_ipv6_udp_checksum(ipv6hdr); 
         } else {
#endif
            iphdr->tot_len = htons(ntohs(iphdr->tot_len)+lendiff);
            set_udp_checksum(iphdr); 
            set_ip_checksum(iphdr); 
#if defined(CONFIG_IPV6)
         }
#endif

         if (proto == constant_htons(ETH_P_PPP_SESS)) {
            pppoehdr->length = htons(ntohs(pppoehdr->length)+lendiff);
         }
      }
   }
   RTP_TS_READ_UNLOCK();
}
EXPORT_SYMBOL(rtp_timestamp_insert_in_skb);

// insert in capi_codec
int rtp_timestamp_insert_in_rtp(unsigned short rtp_port, unsigned char *data, unsigned int datalen)
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   struct _rtp_timestamp_stat *stat;
   RTP_TS_LOCK_DECLARE;

   if (unlikely(ctx->disabled)) return datalen;

   RTP_TS_READ_LOCK();
   stat = get_timestamp_session(rtp_port);
   if (stat && stat->state != ts_stopped) {
      int ret = set_timestamp((struct rtp_header *)data, datalen);
      RTP_TS_READ_UNLOCK();
      return ret;
   }
   RTP_TS_READ_UNLOCK();
   return datalen; 
}
EXPORT_SYMBOL(rtp_timestamp_insert_in_rtp);

static void _rtp_timestamp_trace_session_from_skb(struct sk_buff *skb, unsigned short on_network_hdr, unsigned short with_ethernet_encap, unsigned short is_outgoing)
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   struct _rtp_timestamp_stat *stat;
   struct ethhdr *ethhdr;
   u16 proto;
   struct pppoehdr *pppoehdr;
   struct iphdr *iphdr;
#if defined(CONFIG_IPV6)
   struct ipv6hdr *ipv6hdr = 0;
#endif
   int iphlen;
   struct udphdr *udphdr;
   struct rtp_header *rtphdr;

   if (unlikely(ctx->disabled)) return;

   if (on_network_hdr && !with_ethernet_encap) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
      iphdr = (struct iphdr *)skb->nh.iph;
#else
      iphdr = (struct iphdr *)skb_network_header(skb);
#endif
      if (!iphdr) return;
#if defined(CONFIG_IPV6)
      if (iphdr->version == 6)
         ipv6hdr = (struct ipv6hdr *)iphdr;
#endif
      proto = skb->protocol;
   } else { /* on skb->data */
      void *data;
      if (on_network_hdr && with_ethernet_encap) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
         data = (void *)skb->nh.iph;
#else
         data = (void *)skb_network_header(skb);
#endif
      if (!data) return;
         ethhdr = (struct ethhdr *)(data-sizeof(struct ethhdr)-sizeof(struct pppoehdr)-2/*PPP*/);
      } else {
         ethhdr = (struct ethhdr *)skb->data;
      }
      if (!ethhdr) return;
      proto = ethhdr->h_proto;
      if (proto == constant_htons(ETH_P_8021Q)) {
         struct vlanhdr *vlanhdr = (struct vlanhdr *)(ethhdr+1);
         proto = vlanhdr->vlan_proto;
         data = vlanhdr+1;
      } else {
         data = ethhdr+1;
      }
      if (proto == constant_htons(ETH_P_PPP_SESS)) {
         u8 *p;
         pppoehdr = (struct pppoehdr *)data;
         p = (u8 *)(pppoehdr+1);
         if (p[0] == 0x00 && p[1] == 0x21) {
            iphdr = (struct iphdr *)(p+2);
#if defined(CONFIG_IPV6)
         } else if (p[0] == 0x00 && p[1] == 0x57) {
            ipv6hdr = (struct ipv6hdr *)(p+2);
#endif
         } else {
            return;
         }
#if defined(CONFIG_IPV6)
      } else if (proto == constant_htons(ETH_P_IPV6)) { 
         ipv6hdr = (struct ipv6hdr *)data;
#endif
      } else if (proto == constant_htons(ETH_P_IP)) {
         iphdr = (struct iphdr *)data;
      } else {
         return; /* only pppoe-session o. rbe */    
      }
   }
#if defined(CONFIG_IPV6)
   if (ipv6hdr) {
      if (ipv6hdr->nexthdr !=  IPPROTO_UDP) return;
      udphdr = (struct udphdr *)(ipv6hdr+1);
   } else {
#endif
      if (iphdr->protocol != IPPROTO_UDP) return;
      iphlen = iphdr->ihl<<2;
      udphdr = (struct udphdr *)(((char *)iphdr)+iphlen);
#if defined(CONFIG_IPV6)
   }
#endif

   rtphdr = (struct rtp_header *)(udphdr+1);
   stat = parse_timestamp(ntohs(udphdr->source), rtphdr, (ntohs(udphdr->len)-sizeof(struct udphdr)), is_outgoing);
   if (stat && stat->state == ts_running_with_stamp_remove) {      
      if (rtphdr->padbit) {
         int lendiff = sizeof(struct _magic_timestamp)+1; /* +1 -> len of timestamp */
         rtphdr->padbit = 0;
         rtp_skb_resize(skb, -lendiff); 
         udphdr->len = htons(ntohs(udphdr->len)-lendiff);
#if defined(CONFIG_IPV6)
         if (ipv6hdr) {
            ipv6hdr->payload_len = htons(ntohs(ipv6hdr->payload_len)-lendiff);
            set_ipv6_udp_checksum(ipv6hdr); 
         } else {
#endif
            iphdr->tot_len = htons(ntohs(iphdr->tot_len)-lendiff);
            set_udp_checksum(iphdr); 
            set_ip_checksum(iphdr); 
#if defined(CONFIG_IPV6)
         }
#endif
         if (   proto == constant_htons(ETH_P_PPP_SESS)
             && (   on_network_hdr == 0 
                 || (on_network_hdr && with_ethernet_encap))) {
            pppoehdr->length = htons(ntohs(pppoehdr->length)-lendiff);
         }
      }
   }
}

void rtp_timestamp_trace_session_from_skb_data(struct sk_buff *skb, unsigned short is_outgoing)
{
   _rtp_timestamp_trace_session_from_skb(skb, 0, 0, is_outgoing);
}
EXPORT_SYMBOL(rtp_timestamp_trace_session_from_skb_data);

void rtp_timestamp_trace_session_from_skb_network(struct sk_buff *skb, unsigned short with_ethernet_encap, unsigned short is_outgoing)
{
   _rtp_timestamp_trace_session_from_skb(skb, 1, with_ethernet_encap, is_outgoing);
}
EXPORT_SYMBOL(rtp_timestamp_trace_session_from_skb_network);

void rtp_timestamp_trace_session_from_rtp(unsigned short rtp_port, unsigned char *data, unsigned int *datalen, unsigned short is_outgoing)
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   struct _rtp_timestamp_stat *stat;
   struct rtp_header *rtphdr;

   if (unlikely(ctx->disabled)) return;

   rtphdr = (struct rtp_header *)data;
   stat = parse_timestamp(rtp_port, rtphdr, *datalen, is_outgoing);
   if (stat) {
      if (rtphdr->padbit) {
         rtphdr->padbit = 0;
         *datalen -= (sizeof(struct _magic_timestamp)+1);
      }
   }
}
EXPORT_SYMBOL(rtp_timestamp_trace_session_from_rtp);


void rtp_timestamp_read_stat(unsigned short rtp_port, struct _rtp_timestamp_stat *rtp_ts_stat)
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   struct _rtp_timestamp_stat *stat;
   RTP_TS_LOCK_DECLARE;

   if (unlikely(ctx->disabled)) return;

   RTP_TS_READ_LOCK();
   stat = get_timestamp_session(rtp_port);
   if (stat) {
      rtp_ts_stat->incoming_cnt = stat->incoming_cnt; 
      rtp_ts_stat->incoming_sum = stat->incoming_sum;
      rtp_ts_stat->incoming_quadsum = stat->incoming_quadsum;        
      rtp_ts_stat->incoming_min = stat->incoming_min;
      rtp_ts_stat->incoming_max = stat->incoming_max;
      rtp_ts_stat->outgoing_cnt = stat->outgoing_cnt; 
      rtp_ts_stat->outgoing_sum = stat->outgoing_sum;
      rtp_ts_stat->outgoing_quadsum = stat->outgoing_quadsum;        
      rtp_ts_stat->outgoing_min = stat->outgoing_min;
      rtp_ts_stat->outgoing_max = stat->outgoing_max;
   }
   RTP_TS_READ_UNLOCK();
}
EXPORT_SYMBOL(rtp_timestamp_read_stat);

int get_padding_len(void)
{
   return (sizeof(struct _magic_timestamp) +1);
}
EXPORT_SYMBOL(get_padding_len);

/*****************************************************************/
/*****************************************************************/


typedef int ts_fprintf(void *, const char *, ...)
#ifdef __GNUC__
        __attribute__ ((__format__(__printf__, 2, 3)))
#endif
        ;


static struct proc_dir_entry *dir_entry = 0;


static int rtp_timestamp_write_cmds (struct file *file, const char *buffer,
                                     unsigned long count, void *data)
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   char    ts_cmd[100];
   char*   argv[10];
   int     argc = 0;
   char*   ptr_cmd;
   char*   delimitters = " \n\t";
   char*   ptr_next_tok;
   RTP_TS_LOCK_DECLARE;

   /* Validate the length of data passed. */
   if (count > 100)
      count = 100;

   /* Initialize the buffer before using it. */
   memset ((void *)&ts_cmd[0], 0, sizeof(ts_cmd));
   memset ((void *)&argv[0], 0, sizeof(argv));

   /* Copy from user space. */
   if (copy_from_user (&ts_cmd, buffer, count))
      return -EFAULT;

   ptr_next_tok = &ts_cmd[0];
   ptr_cmd = strsep(&ptr_next_tok, delimitters);
   if (ptr_cmd == NULL)
      return -1;

   do {
      argv[argc++] = ptr_cmd;

      if (argc >=10) {
         printk(KERN_ERR "rtp_timestamp: too many parameters dropping the command\n");
         return -EIO;
      }

      ptr_cmd = strsep(&ptr_next_tok, delimitters);
      if (ptr_cmd && ptr_cmd[0] == 0)
         ptr_cmd = NULL;
   } while (ptr_cmd != NULL);

   argc--;

   RTP_TS_WRITE_LOCK();
   /* enable | disable generic */
   if (strcmp(argv[0], "enable") == 0) {
      ctx->disabled = 0;
      printk(KERN_DEBUG "rtp_timestamp: enabled\n");
   } else if (strcmp(argv[0], "disable") == 0) {
      ctx->disabled = 1;
      printk(KERN_DEBUG "rtp_timestamp: disabled\n");
   } else if (strcmp(argv[0], "start") == 0) {
      if (argc >= 1 && argv[1]) {
         unsigned short rtp_port = simple_strtoul(argv[1], 0, 10);
         int remove_stamp = 0;
         if (argc >= 2 && argv[2] && !strcmp(argv[2], "withoutstamp"))
            remove_stamp = 1;
         printk(KERN_DEBUG "rtp_timestamp_start_stat: session=%d, remove_stamp=%d\n", rtp_port, remove_stamp);
         rtp_timestamp_start_stat(rtp_port, remove_stamp);
      }
   } else if (strcmp(argv[0], "stop") == 0) {
      if (argc >= 1 && argv[1]) {
         unsigned short rtp_port = simple_strtoul(argv[1], 0, 10);
         printk(KERN_DEBUG "rtp_timestamp_stop_stat: session=%d\n", rtp_port);
         rtp_timestamp_stop_stat(rtp_port);
      }
   } else if (strcmp(argv[0], "clear") == 0) {
      printk(KERN_DEBUG "rtp_timestamp: clear\n");
      if (argc >= 1 && argv[1]) {
         unsigned short rtp_port = simple_strtoul(argv[1], 0, 10);
         printk(KERN_DEBUG "rtp_timestamp_stop_stat: rtp_port=%d\n", rtp_port);
         rtp_timestamp_clear_stat(rtp_port);
      } else {
         printk(KERN_DEBUG "rtp_timestamp_stop_stat: rtp_port=0\n");
         rtp_timestamp_clear_stat(0);
      }
   } else {
      printk(KERN_ERR "rtp_timestamp_pa_write_cmds: %s: unknown command\n", argv[0]);
   }

   RTP_TS_WRITE_UNLOCK();

   return count;
}


/* ------------------------------------------------------------------------ */
static void rtp_timestamp_show_status(ts_fprintf fprintffunc, void *arg)

{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   char *mode;
   int i;
   RTP_TS_LOCK_DECLARE;

   if (ctx->disabled) mode = "disabled";
   else mode = "enabled";
   (*fprintffunc)(arg, "State          : %s\n", mode);

   if (ctx->disabled) return;

   RTP_TS_READ_LOCK();

   for (i = 0; i < MAX_STAT_INSTANCE; i++) {
      if (ctx->timestamp_stat[i].session == 0) continue;
      (*fprintffunc)(arg, "session %u\n", ctx->timestamp_stat[i].session);
      (*fprintffunc)(arg, "   incoming_min %ld\n", ctx->timestamp_stat[i].incoming_min);
      (*fprintffunc)(arg, "   incoming_max %ld\n", ctx->timestamp_stat[i].incoming_max);
      (*fprintffunc)(arg, "   incoming_cnt %ld\n", ctx->timestamp_stat[i].incoming_cnt);
      (*fprintffunc)(arg, "   incoming_sum %ld\n", ctx->timestamp_stat[i].incoming_sum);      
      (*fprintffunc)(arg, "   incoming_quadsum %ld\n", ctx->timestamp_stat[i].incoming_quadsum);      
      if (ctx->timestamp_stat[i].incoming_cnt)
         (*fprintffunc)(arg, "   incoming_avg %ld\n", ctx->timestamp_stat[i].incoming_sum/ctx->timestamp_stat[i].incoming_cnt);
      else
         (*fprintffunc)(arg, "   incoming_avg     ---\n");
      (*fprintffunc)(arg, "------------------\n");
      (*fprintffunc)(arg, "   outgoing_min %ld\n", ctx->timestamp_stat[i].outgoing_min);
      (*fprintffunc)(arg, "   outgoing_max %ld\n", ctx->timestamp_stat[i].outgoing_max);
      (*fprintffunc)(arg, "   outgoing_cnt %ld\n", ctx->timestamp_stat[i].outgoing_cnt);
      (*fprintffunc)(arg, "   outgoing_sum %ld\n", ctx->timestamp_stat[i].outgoing_sum);      
      (*fprintffunc)(arg, "   outgoing_quadsum %ld\n", ctx->timestamp_stat[i].outgoing_quadsum);              
      if (ctx->timestamp_stat[i].outgoing_cnt)
         (*fprintffunc)(arg, "   outgoing_avg %ld\n", ctx->timestamp_stat[i].outgoing_sum/ctx->timestamp_stat[i].outgoing_cnt);
      else
         (*fprintffunc)(arg, "   outgoing_avg     ---\n");
      (*fprintffunc)(arg, "\n");
   }

   RTP_TS_READ_UNLOCK();
}

static int status_show(struct seq_file *m, void *v)
{
   rtp_timestamp_show_status((ts_fprintf *)seq_printf, m);
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
static void rtp_timestamp_show_stats(ts_fprintf fprintffunc, void *arg)
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   int i;
   RTP_TS_LOCK_DECLARE;

   if (ctx->disabled) return;

   RTP_TS_READ_LOCK();

   for (i = 0; i < MAX_STAT_INSTANCE; i++) {
      if (ctx->timestamp_stat[i].session == 0) continue;
      (*fprintffunc)(arg, "session %u\n", ctx->timestamp_stat[i].session);
      (*fprintffunc)(arg, "   incoming_min %ld\n", ctx->timestamp_stat[i].incoming_min);
      (*fprintffunc)(arg, "   incoming_max %ld\n", ctx->timestamp_stat[i].incoming_max);
      (*fprintffunc)(arg, "   incoming_cnt %ld\n", ctx->timestamp_stat[i].incoming_cnt);
      (*fprintffunc)(arg, "   incoming_sum %ld\n", ctx->timestamp_stat[i].incoming_sum);
      (*fprintffunc)(arg, "   incoming_quadsum %ld\n", ctx->timestamp_stat[i].incoming_quadsum);
      if (ctx->timestamp_stat[i].incoming_cnt)
         (*fprintffunc)(arg, "   incoming_avg %ld\n", ctx->timestamp_stat[i].incoming_sum/ctx->timestamp_stat[i].incoming_cnt);
      else
         (*fprintffunc)(arg, "   incoming_avg ---\n");
      (*fprintffunc)(arg, "------------------\n");
      (*fprintffunc)(arg, "   outgoing_min %ld\n", ctx->timestamp_stat[i].outgoing_min);
      (*fprintffunc)(arg, "   outgoing_max %ld\n", ctx->timestamp_stat[i].outgoing_max);
      (*fprintffunc)(arg, "   outgoing_cnt %ld\n", ctx->timestamp_stat[i].outgoing_cnt);
      (*fprintffunc)(arg, "   outgoing_sum %ld\n", ctx->timestamp_stat[i].outgoing_sum);      
      (*fprintffunc)(arg, "   outgoing_quadsum %ld\n", ctx->timestamp_stat[i].outgoing_quadsum);              
      if (ctx->timestamp_stat[i].outgoing_cnt)
         (*fprintffunc)(arg, "   outgoing_avg %ld\n", ctx->timestamp_stat[i].outgoing_sum/ctx->timestamp_stat[i].outgoing_cnt);
      else
         (*fprintffunc)(arg, "   outgoing_avg ---\n");
      (*fprintffunc)(arg, "\n");
   }

   RTP_TS_READ_UNLOCK();
}

static int stats_show(struct seq_file *m, void *v)
{
   rtp_timestamp_show_stats((ts_fprintf *)seq_printf, m);
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



static void rtp_timestamp_proc_init(void)
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   struct proc_dir_entry *file_entry;
   RTP_TS_LOCK_DECLARE;

   if (ctx->proc_inited == 1) return;
   RTP_TS_WRITE_LOCK();

   dir_entry = proc_net_mkdir(&init_net, "rtp_timestamp", init_net.proc_net);
   if (dir_entry) {
      dir_entry->read_proc = 0;
      dir_entry->write_proc = 0;
   }

   file_entry = create_proc_entry("control", S_IFREG|S_IWUSR, dir_entry);
   if (file_entry) {
      file_entry->data        = NULL;
      file_entry->read_proc  = NULL;
      file_entry->write_proc = rtp_timestamp_write_cmds;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
      file_entry->owner      = THIS_MODULE;
#endif
   }
   file_entry = proc_create("status", S_IRUGO, dir_entry, &status_show_fops);
   file_entry = proc_create("stats", S_IRUGO, dir_entry, &stats_show_fops);
   ctx->proc_inited = 1;
   RTP_TS_WRITE_UNLOCK();
}

static void rtp_timestamp_proc_exit(void)
{
   struct rtp_timestamp_global *ctx = &rtp_timestamp_glob;
   RTP_TS_LOCK_DECLARE;

   if (ctx->proc_inited == 0) return;

   RTP_TS_WRITE_LOCK();
   remove_proc_entry("control", dir_entry);
   remove_proc_entry("status", dir_entry);
   remove_proc_entry("stats", dir_entry);
   ctx->proc_inited = 0;
   RTP_TS_WRITE_UNLOCK();
}


void __init rtp_timestamp_init(void)
{
   rtp_timestamp_proc_init();
}

void __exit rtp_timestamp_exit(void)
{
   rtp_timestamp_proc_exit();
}

module_init(rtp_timestamp_init);
module_exit(rtp_timestamp_exit);
