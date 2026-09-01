/*
 * Multicast Fastforward from multicast source to destination ports
 *
 * Copyright (C) 2006 AVM GmbH
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/if_pppox.h>
#include <linux/igmp.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>
#include <linux/proc_fs.h>
#include <linux/mcfastforward.h>

#ifndef MULTICAST
#define MULTICAST(x)        (((x) & htonl(0xf0000000)) == htonl(0xe0000000))
#endif
#ifndef LOCAL_MCAST
#define LOCAL_MCAST(x)      (((x) & htonl(0xFFFFFF00)) == htonl(0xE0000000))
#endif
#ifndef LOCALAREA_MCAST
#define LOCALAREA_MCAST(x)  (((x) & htonl(0xFFFF0000)) == htonl(0xEFFF0000))
#endif

/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */

#define RTP_HDR_MAX_CSRC   16

struct rtphdr {
#if defined (__BIG_ENDIAN_BITFIELD)
   u8    version:2,
	     padbit:1,
	     extbit:1,
	     cc:4;
   u8    markbit:1,
	     paytype:7;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
   u8    cc:4,
	     extbit:1,
	     padbit:1,
	     version:2;
   u8    paytype:7,
         markbit:1;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
   u16   seq_number;
   u32   timestamp;
   u32   ssrc;
   u32   csrc[RTP_HDR_MAX_CSRC];
};

/* --------------------------------------------------------------------- */

#define DEFAULT_ROBUSTNESS                      2
#define DEFAULT_QUERY_INTERVAL                  125
#define DEFAULT_QUERY_RESPONSE_INTERVAL         10

#define DEFAULT_GROUP_INTERVAL  \
	(DEFAULT_ROBUSTNESS*DEFAULT_QUERY_INTERVAL)+DEFAULT_QUERY_RESPONSE_INTERVAL

/* --------------------------------------------------------------------- */

#define MAX_NETDRIVER	   16
#define MCGROUP_HASHSIZ   256
#define MCFW_MAXSOURCE     16
#define MCFW_MAXSOURCENAME 32

/* --------------------------------------------------------------------- */
/* -------- internal types --------------------------------------------- */
/* --------------------------------------------------------------------- */

struct mcfilter {
   unsigned mode;    /* MCAST_INCLUDE || MCAST_EXCLUDE */
   unsigned n;
   u32     *slist;
   unsigned nmem;  /* memory allocated in slist */
};

struct mcgroupmember {
   struct mcgroupmember  *next;
   struct mcfw_netdriver *drv;
   u32                    ipaddr;
   u8                     ethaddr[ETH_ALEN];
   int                    port;
   int                    checking;
   struct mcfilter        filter;
   struct timer_list      timer;
};

struct mcroute {
   int              forward;
   unsigned         n;
   mcfw_portset     portset;
   mcfw_memberinfo *members;
   unsigned         nmem;
};

struct mcsource {
   struct mcsource *next;
   int              inuse;
   struct mcgroup  *grp;
   u32              ipaddr;
   int              forward;
   int              nroutes;
   struct mcroute   routes[MAX_NETDRIVER];
   /* acct */
   int                 no_rtp;
   int                 running;
   int                 errcnt;
   u16                 seq;
   u32                 ssrc;
   u32                 msecs;
   struct mcsourceacct acct;
};

struct mcgroup {
   struct mcgroup       *link; /* hashtab */
   u32                   group;
   u8                    groupmac[ETH_ALEN];
   struct timer_list     checking_timer;
   unsigned long         group_interval; /* in jiffies */
   struct mcgroupmember *members;
   struct mcsource      *sources;
   struct mcgroupacct    acct;
};

/* --------------------------------------------------------------------- */
/* -------- global valiables ------------------------------------------- */
/* --------------------------------------------------------------------- */

static struct mcfw_glob {
   spinlock_t             group_lock;
   unsigned long          robustness;
   unsigned long          query_interval; /* # secs */
   unsigned long          group_interval; /* # secs */
   struct mcgroup        *grouphashtab[MCGROUP_HASHSIZ];
   struct mcfw_netdriver *drivers[MAX_NETDRIVER];
   struct mcfw_statistic  stat;
   struct mcsource       *source_for_delete;
   void (*group_acct_cb)(struct mcgroupacct *p);
   void (*source_acct_cb)(struct mcsourceacct *p);
   int                    debug;
} mcfwglob = {
   group_lock: SPIN_LOCK_UNLOCKED,
   robustness: DEFAULT_ROBUSTNESS,
   query_interval: DEFAULT_QUERY_INTERVAL,
   group_interval: DEFAULT_GROUP_INTERVAL,
   debug: 0,
};

/* --------------------------------------------------------------------- */
/* --------- forward declarations -------------------------------------- */
/* --------------------------------------------------------------------- */

static void mcgroup_del(u32 group, int fast);
static void calc_for_source(struct mcgroup *grp, struct mcsource *source);

/* --------------------------------------------------------------------- */
/* -------- mcfw_portset ----------------------------------------------- */
/* --------------------------------------------------------------------- */

static char *portset2str(mcfw_portset *p, char *buf, size_t size)
{
   char *s = buf;
   size_t len = 0;
   int i;

   for (i=0; i < MCFW_MAX_PORT; i++) {
      if (mcfw_portset_port_is_in_set(p, i)) {
		 snprintf(s, size-len, "%s%d", s != buf ? "," : "", i);
		 s += strlen(s);
	  }
   }
   *s = 0;
   return buf;
}

/* --------------------------------------------------------------------- */
/* -------- struct mcfilter -------------------------------------------- */
/* --------------------------------------------------------------------- */

#define MCFILTER_SRC_INCRNUM 4

static char *_filter2str(unsigned mode,
                         unsigned n, 
						 u32 *slist,
						 char *buf,
						 size_t size)
{
   char *s = buf;
   char *end = buf+size-1;
   int i;
   if (mode == MCAST_INCLUDE) {
	  snprintf(s, end-s, "INCLUDE(");
   } else if (mode == MCAST_EXCLUDE) {
	  snprintf(s, end-s, "EXCLUDE(");
   } else {
	  snprintf(s, end-s, "(");
   }
   s += strlen(s);
   for (i=0; i < n; i++) {
	  if (i != 0) *s++ = ',';
	  if (end-s < 20) {
		 snprintf(s, end-s, "...");
         s += strlen(s);
		 break;
	  }
	  snprintf(s, end-s, "%u.%u.%u.%u", NIPQUAD(slist[i]));  
      s += strlen(s);
   }
   snprintf(s, end-s, ")");
   return buf;
}

static inline char *mcfilter2str(struct mcfilter *p, char *buf, size_t size)
{
   return _filter2str(p->mode, p->n, p->slist, buf, size);
}

static int mcfilter_extent(struct mcfilter *p, unsigned nmem)
{
   if (nmem > p->nmem) {
	  u32 *slist;
	  if ((slist = (u32 *)kmalloc(nmem*sizeof(u32), GFP_ATOMIC)) == 0) {
		 printk(KERN_ERR "mcfw: filter_extent: kmalloc failed");
		 return -1;
	  }
	  memset(slist, 0, nmem*sizeof(u32));
	  if (p->n)
	     memcpy(slist, p->slist, p->n*sizeof(u32));
	  if (p->slist) kfree(p->slist);
	  p->slist = slist;
      p->nmem = nmem;
   }
   return 0;
}

static int mcfilter_init(struct mcfilter *p,
				         unsigned mode,
				         unsigned n, u32 *slist)
{
   p->mode = mode;
   if (mcfilter_extent(p, n) < 0) 
	  return -1;
   if (n && slist) {
	  memcpy(p->slist, slist, n*sizeof(u32));
      p->n = n;
   } else {
      p->n = 0;
   }
   return 0;
}

static int mcfilter_set(struct mcfilter *p,
                        unsigned mode,
						unsigned n,
						u32 *slist)
{
   p->n = 0;
   if (mcfilter_extent(p, n) < 0)
	  return -1;
   if (n) memcpy(p->slist, slist, n*sizeof(u32));
   p->mode = mode;
   p->n = n;
   return 0;
}

static void mcfilter_reset(struct mcfilter *p)
{
   if (p->slist) kfree(p->slist);
   p->slist = 0;
   p->n = p->nmem = 0;
}

static int mcfilter_src_exists(struct mcfilter *p, u32 source)
{
   unsigned i;
   for (i=0; i < p->n; i++) {
	  if (p->slist[i] == source)
		 return 1;
   }
   return 0;
}

static int mcfilter_src_add(struct mcfilter *p, u32 source)
{
   if (mcfilter_src_exists(p, source))
	  return 0;
   if (p->n == p->nmem) {
	  if (mcfilter_extent(p, p->nmem+MCFILTER_SRC_INCRNUM) < 0)
		 return -1;
   }
   p->slist[p->n++] = source;
   return 0;
}

static int mcfilter_src_del(struct mcfilter *p, u32 source)
{
   unsigned i;
   for (i=0; i < p->n; i++) {
	  if (p->slist[i] == source) {
		 int nelem = (p->n-1)-i;
		 memmove(&p->slist[i], &p->slist[i+1], nelem*sizeof(u32));
		 p->n--;
		 return 0;
	  }
   }
   return -1;
}

static int mcfilter_src_add_list(struct mcfilter *p,
                                 unsigned n,
								 u32 *slist)
{
    unsigned i;
	for (i=0; i < n; i++) {
	   if (mcfilter_src_add(p, slist[i]) < 0)
		  return -1;
	}
	return 0;
}

static int mcfilter_src_del_list(struct mcfilter *p,
                                 unsigned n,
								 u32 *slist)
{
    unsigned i;
	for (i=0; i < n; i++) {
	   (void)mcfilter_src_del(p, slist[i]);
	}
	return 0;
}

#if 0
static void mcfilter_swap(struct mcfilter *p1, struct mcfilter *p2)
{
    struct mcfilter filter;
	filter = *p1;
	*p1 = *p2;
	*p2 = filter;
}

static int mcfilter_ne(struct mcfilter *p1, struct mcfilter *p2)
{
   int i;
   if (p1->mode != p2->mode || p1->n != p2->n)
	  return 1;
   for (i=0; i < p1->n; i++) {
	  if (p1->slist[i] != p2->slist[i])
		 return 1;
   }
   return 0;
}
#endif

static int mcfilter_forward_ok(struct mcfilter *p, u32 srcip)
{
   if (p->mode == MCAST_INCLUDE)
      return mcfilter_src_exists(p, srcip);
   else
      return !mcfilter_src_exists(p, srcip);
}

/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */

static void calc_routes(struct mcgroup *grp)
{
   struct mcsource *source;
   for (source = grp->sources; source; source = source->next)
      calc_for_source(grp, source);
}

/* --------------------------------------------------------------------- */
/* -------- struct mcgroupmember --------------------------------------- */
/* --------------------------------------------------------------------- */

static void mcgroupmember_free(struct mcgroup *grp,
                               struct mcgroupmember *p, char *append)
{
   mcfilter_reset(&p->filter);
   del_timer(&p->timer);
   if (mcfwglob.debug)
   printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: %u.%u.%u.%u (%s:%d) %s\n",
	                 NIPQUAD(grp->group), NIPQUAD(p->ipaddr),
	                 p->drv->name, p->port, append);
   kfree(p);
}

static void mcgroupmember_timer_handler(unsigned long arg)
{
   struct mcgroupmember *needle = (struct mcgroupmember *)arg;
   unsigned long flags;
   int i;

   if (mcfwglob.debug)
   printk(KERN_DEBUG "mcfw: member %u.%u.%u.%u: timer elapsed\n",
					NIPQUAD(needle->ipaddr));

   spin_lock_irqsave(&mcfwglob.group_lock, flags);
   for (i = 0; i < MCGROUP_HASHSIZ; i++) {
	  struct mcgroup *grp;
	  for (grp = mcfwglob.grouphashtab[i]; grp; grp = grp->link) {
         struct mcgroupmember **pp = &grp->members;
		 for (pp = &grp->members; *pp != 0; pp = &(*pp)->next) {
			if (*pp == needle) {
			   *pp = needle->next;
               mcgroupmember_free(grp, needle, "timed out");
			   if (grp->members == 0)
				  mcgroup_del(grp->group, 0);
		       else
		          calc_routes(grp);
			   goto out;
			}
		 }
	  }
   }
out:
   spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
}


static int mcgroupmember_change(struct mcgroup *grp,
                                struct mcfw_netdriver *drv,
							    int port,
							    u32 ipaddr,
							    u8 ethaddr[ETH_ALEN],
								int subtype,
								unsigned n,
								u32 *slist)
{
   struct mcgroupmember *p, **pp;
   char buf[128];

   for (pp = &grp->members; (p = *pp) != 0;  pp = &p->next) {
	  if (p->drv == drv && memcmp(p->ethaddr, ethaddr, ETH_ALEN) == 0) {
		 if (p->port != port) {
            if (mcfwglob.debug)
	        printk(KERN_DEBUG "mcfw: %u.%u.%u.%u now on other port %s:%d->%d\n",
			                   NIPQUAD(p->ipaddr), drv->name, p->port, port);
			p->port = port;
		 }
		 if (p->ipaddr != ipaddr) {
            if (mcfwglob.debug)
	        printk(KERN_DEBUG "mcfw: %u.%u.%u.%u changes ip address to %u.%u.%u.%u (%s:%d)\n",
			                   NIPQUAD(p->ipaddr), NIPQUAD(ipaddr),
							   drv->name, p->port);
		    p->ipaddr = ipaddr;
		 }
		 break;
	  }
   }

   if ((p = *pp) == 0) {
      unsigned mode;

	  switch (subtype) {
		 /* 
		  * WIN32 will sent ALLOW_NEW_SOURCES on subscription
		  */
	     case IGMPV3_MODE_IS_INCLUDE:
		 case IGMPV3_CHANGE_TO_INCLUDE:
		 case IGMPV3_ALLOW_NEW_SOURCES:
		    mode = MCAST_INCLUDE;
			break;
	     case IGMPV3_MODE_IS_EXCLUDE:
		 case IGMPV3_CHANGE_TO_EXCLUDE:
		    mode = MCAST_EXCLUDE;
			break;
		 case IGMPV3_BLOCK_OLD_SOURCES:
		 default:
            return -1;
	  }

	  if (mode == MCAST_INCLUDE && n == 0) {
         if (mcfwglob.debug)
	     printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: %u.%u.%u.%u (%s:%d) already deleted\n",
					   NIPQUAD(grp->group), NIPQUAD(ipaddr), drv->name, port);
		 return 0;
	  }
		 
      p = (struct mcgroupmember *)
					  kmalloc(sizeof(struct mcgroupmember), GFP_ATOMIC);
      if (!p) {
		 printk(KERN_ERR "mcfw: group %u.%u.%u.%u: %u.%u.%u.%u (%s:%d) alloc failed\n",
						 NIPQUAD(grp->group), NIPQUAD(ipaddr), drv->name, port);
	     return -1;
      }
	  memset(p, 0, sizeof(struct mcgroupmember));
      p->drv = drv;
      p->port = port;
      p->ipaddr = ipaddr;
	  memcpy(p->ethaddr, ethaddr, ETH_ALEN);
	  mcfilter_init(&p->filter, mode, n, slist);
	  init_timer(&p->timer);
	  p->timer.function = mcgroupmember_timer_handler;
	  p->timer.data = (unsigned long)p;
	  *pp = p;
      if (mcfwglob.debug)
	  printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: %u.%u.%u.%u (%s:%d) added %s\n",
			      NIPQUAD(grp->group), NIPQUAD(ipaddr), drv->name, port,
				  mcfilter2str(&p->filter, buf, sizeof(buf)));
   } else {

	  switch (subtype) {
		 case IGMPV3_MODE_IS_INCLUDE:
		 case IGMPV3_CHANGE_TO_INCLUDE:
			mcfilter_set(&p->filter, MCAST_INCLUDE, n, slist);
			break;
		 case IGMPV3_MODE_IS_EXCLUDE:
		 case IGMPV3_CHANGE_TO_EXCLUDE:
			mcfilter_set(&p->filter, MCAST_EXCLUDE, n, slist);
			break;
		 case IGMPV3_ALLOW_NEW_SOURCES:
			if (p->filter.mode == MCAST_INCLUDE) {
			   mcfilter_src_add_list(&p->filter, n, slist);
			} else {
			   mcfilter_src_del_list(&p->filter, n, slist);
			}
			break;
		 case IGMPV3_BLOCK_OLD_SOURCES:
			if (p->filter.mode == MCAST_INCLUDE) {
			   mcfilter_src_del_list(&p->filter, n, slist);
			} else {
			   mcfilter_src_add_list(&p->filter, n, slist);
			}
			break;
	  }
      if (mcfwglob.debug)
	  printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: %u.%u.%u.%u (%s:%d) refresh %s\n",
			      NIPQUAD(grp->group), NIPQUAD(ipaddr), drv->name, port,
				  mcfilter2str(&p->filter, buf, sizeof(buf)));
   }

   if (p->filter.mode == MCAST_INCLUDE && p->filter.n == 0) {

	  *pp = p->next;
	  mcgroupmember_free(grp, p, "deleted");
	  if (grp->members == 0)
         mcgroup_del(grp->group, 1);
	  else
         calc_routes(grp);

   } else {

	  p->checking = 0;
	  calc_routes(grp);
	  grp->group_interval = mcfwglob.group_interval*HZ;
	  mod_timer(&p->timer, jiffies + grp->group_interval);
      if (mcfwglob.debug)
	  printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: %u.%u.%u.%u: setup timer (%lusecs)\n",
				 NIPQUAD(grp->group), NIPQUAD(p->ipaddr),
				 grp->group_interval/HZ);
   }

   return 0;
}

static void mcgroupmember_delete_all(struct mcgroup *grp)
{
   struct mcgroupmember *p;
   while ((p = grp->members) != 0) {
	  grp->members = p->next;
	  mcgroupmember_free(grp, p, "deleted (all)");
   }
}

/* --------------------------------------------------------------------- */
/* -------- struct mcroute --------------------------------------------- */
/* --------------------------------------------------------------------- */

static int mroute_extent_member(struct mcroute *p, unsigned n)
{
   if (n > p->nmem) {
	  mcfw_memberinfo *members;
	  members = (mcfw_memberinfo *)
			   kmalloc(n*sizeof(mcfw_memberinfo), GFP_ATOMIC);
	  if (members == 0) {
		 printk(KERN_ERR "mcfw: mcroute_extent: kmalloc failed");
		 return -1;
	  }
	  memset(members, 0, n*sizeof(mcfw_memberinfo));
	  if (p->n)
	     memcpy(members, p->members, p->n*sizeof(mcfw_memberinfo));
	  if (p->members) kfree(p->members);
	  p->members = members;
      p->nmem = n;
   }
   return 0;
}

static int mcroute_add_member(struct mcroute *route,
                              struct mcgroupmember *member)
{
   mcfw_memberinfo *info;
   if (mroute_extent_member(route, route->n+1) < 0)
	  return -1;
   info = &route->members[route->n++];
   memcpy(info->ethaddr, member->ethaddr, ETH_ALEN);
   return 0;
}

static void mcroute_clear(struct mcroute *route)
{
   route->forward = 0;
   mcfw_portset_reset(&route->portset);
   route->n = 0;
}

static void mcroute_reset(struct mcroute *route)
{
   mcroute_clear(route);
   if (route->members) {
      kfree(route->members);
	  route->members = 0;
   }
   route->nmem = 0;
}

/* --------------------------------------------------------------------- */
/* -------- struct mcsource -------------------------------------------- */
/* --------------------------------------------------------------------- */

static inline void mcsource_reset_acct(struct mcsource *p)
{
   memset(&p->acct.seqmatch, 0,
		  sizeof(struct mcsourceacct)-offsetof(struct mcsourceacct, seqmatch));
   p->acct.mingap = 0xffffffff;
}

static void mcsource_gen_report(struct mcsource *p,
                                enum mcsourceacct_reason reason,
							    struct timeval *now)
{
   if (mcfwglob.source_acct_cb) {
	  p->acct.endtime = *now;
	  p->acct.ssrc = p->ssrc;
	  p->acct.reason = reason;
	  (*mcfwglob.source_acct_cb)(&p->acct);
	  mcsource_reset_acct(p);
	  p->acct.starttime = *now;
   }
}

static void calc_for_source(struct mcgroup *grp, struct mcsource *source)
{
   struct mcgroupmember *member;
   int i;
   source->nroutes = 0;
   for (i=0; i < MAX_NETDRIVER; i++) {
	  mcroute_clear(&source->routes[i]);
   }
   for (member = grp->members; member; member = member->next) {
      if (mcfilter_forward_ok(&member->filter, source->ipaddr)) {
		 struct mcroute *route = &source->routes[member->drv->index];
		 if (route->forward == 0) {
			route->forward = 1;
            source->nroutes++;
		 }
		 mcfw_portset_port_add(&route->portset, member->port);
		 mcroute_add_member(route, member);
	  }
   }

   if (mcfwglob.debug)
   printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: source %u.%u.%u.%u: %sforward\n",
					NIPQUAD(grp->group), NIPQUAD(source->ipaddr),
					source->nroutes ? "" : "NO ");
   if (mcfwglob.debug) {
   if (source->nroutes) {
	  for (i=0; i < MAX_NETDRIVER; i++) {
		 if (mcfwglob.drivers[i]) {
			struct mcroute *route = &source->routes[i];
			char buf[64];
			int j;

			if (route->n == 0)
			   continue;

			printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: %s: %u member(s) (%s)\n",
							  NIPQUAD(grp->group),
							  mcfwglob.drivers[i]->name, route->n,
							  portset2str(&route->portset, buf, sizeof(buf)));
			for (j=0; j < route->n; j++) {
				printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: %s: %02x:%02x:%02x:%02x:%02x:%02x\n",
							  NIPQUAD(grp->group),
							  mcfwglob.drivers[i]->name,
							  route->members[j].ethaddr[0],
							  route->members[j].ethaddr[1],
							  route->members[j].ethaddr[2],
							  route->members[j].ethaddr[3],
							  route->members[j].ethaddr[4],
							  route->members[j].ethaddr[5]);
			   }
			}
		 }
	  }
   }
}

static struct mcsource *mcsource_find_or_add(struct mcgroup *grp, u32 srcip)
{
   struct mcsource **pp, *p;
   struct timeval tv;
   for (pp = &grp->sources; *pp; pp = &(*pp)->next) {
	  if ((*pp)->ipaddr == srcip)
		 return *pp;
   }
   p = (struct mcsource *)kmalloc(sizeof(struct mcsource), GFP_ATOMIC);
   if (p == 0) {
	  printk(KERN_ERR "mcfw: group %u.%u.%u.%u: add source %u.%u.%u.%u failed\n",
						NIPQUAD(grp->group), NIPQUAD(srcip));
	  return 0;
   }
   do_gettimeofday(&tv);
   if (grp->sources == 0)
	  grp->acct.starttime = tv;
   memset(p, 0, sizeof(struct mcsource));
   p->grp = grp;
   p->ipaddr = srcip;
   p->acct.group = grp->group;
   p->acct.source = srcip;
   p->acct.starttime = tv;
   if (grp->sources == 0)
	  do_gettimeofday(&grp->acct.starttime);
   *pp = p;
   mcsource_gen_report(p, mcsourceacct_reason_start, &tv);
   calc_for_source(grp, p);
   return p;
}

static void mcsource_free(struct mcsource *p)
{
   struct timeval tv;
   int i;
   for (i=0; i < MAX_NETDRIVER; i++) {
	  mcroute_reset(&p->routes[i]);
   }
   do_gettimeofday(&tv);
   mcsource_gen_report(p, mcsourceacct_reason_stop, &tv);
   kfree(p);
}

static inline int after(u16 seq1, u16 seq2)
{
   return (s16)(seq2 - seq1) < 0;
}

#define TIMOFFSET	1242833119

static inline void mcsource_check_skb(struct mcsource *p, struct sk_buff *skb)
{
   struct iphdr *iph;
   struct rtphdr *rtph;
   struct timeval tv;
   u32 msecs, gap = 0;
   u16 seq;
   unsigned off;

   skb_get_timestamp(skb, &tv);
   msecs = (tv.tv_sec-TIMOFFSET) * 1000 + tv.tv_usec / 1000;
   if (p->msecs)
	  gap = msecs-p->msecs;
   if (gap < p->acct.mingap) p->acct.mingap = gap;
   if (gap > p->acct.maxgap) p->acct.maxgap = gap;
   off = fls(gap);
   if (off >= MCFW_MAX_LOG2GAP)
	  off = MCFW_MAX_LOG2GAP-1;
   mcfw_counter_add(skb, &p->acct.forwarded);
   p->acct.sumgap += gap;
   p->acct.sumgap2 += gap*gap;
   p->acct.log2gap[off]++;
   p->msecs = msecs;

   if (p->no_rtp)
	  return;

   iph = (struct iphdr *)(skb->data+14);
   rtph = (struct rtphdr *)(((u8 *)iph)+(iph->ihl<<2)+8);

   if (rtph->version != 2)
	  goto no_rtp;

   seq = ntohs(rtph->seq_number);
   if (p->running == 0) {
	  p->ssrc = rtph->ssrc;
	  p->seq = (u16)(seq-1);
	  p->running = 1;
   } else if (rtph->ssrc != p->ssrc) {
	  mcsource_gen_report(p, mcsourceacct_reason_ssrc, &tv);
	  p->ssrc = rtph->ssrc;
	  p->seq = (u16)(seq-1);
   }

   if (seq == p->seq) { /* duplicate packet */
	  p->acct.seqduplicate++;
	  return;
   }
   if (!after(seq, p->seq)) { /* old packet */
	  p->acct.seqtoolate++;
	  return;
   }
   if (seq != (u16)(p->seq + 1)) { /* packets lost */
	  u32 lost = (s16)(seq - p->seq);
	  p->acct.seqwrong++;
	  p->acct.lost += lost;
	  if (lost > p->acct.maxlost)
		 p->acct.maxlost = lost;
	  p->seq = seq;
	  return;
   }
   p->acct.seqmatch++;
   p->seq = seq;
   return;

no_rtp:
   p->no_rtp = 1;
   if (mcfwglob.debug)
   printk(KERN_INFO "mcfw: %u.%u.%u.%u from %u.%u.%u.%u is no rtp\n",
					NIPQUAD(p->grp->group), NIPQUAD(p->ipaddr));
}

static void mcgroupsource_delete_all(struct mcgroup *grp)
{
   struct mcsource *p;
   while ((p = grp->sources) != 0) {
	  grp->sources = p->next;
	  if (p->inuse) {
		 p->next = mcfwglob.source_for_delete;
		 mcfwglob.source_for_delete = p;
	  } else {
	     mcsource_free(p);
	  }
   }
}

/* --------------------------------------------------------------------- */

void mcfw_report_source_acct(void)
{
   if (mcfwglob.source_acct_cb) {
	  struct timeval tv;
	  unsigned long flags;
	  int i;

	  do_gettimeofday(&tv);
	  spin_lock_irqsave(&mcfwglob.group_lock, flags);
	  for (i = 0; i < MCGROUP_HASHSIZ; i++) {
		 struct mcgroup *grp;
		 for (grp = mcfwglob.grouphashtab[i]; grp; grp = grp->link) {
			struct mcsource *p;
			for (p = grp->sources; p; p = p->next)
			   mcsource_gen_report(p, mcsourceacct_reason_update, &tv);
		 }
	  }
	  spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
   }
}

/* --------------------------------------------------------------------- */

static inline int grouphashval(u32 group)
{
#ifdef __BIG_ENDIAN
   return ((group & 0xff) ^ ((group >> 8) & 0xff)) % MCGROUP_HASHSIZ;
#else
   return (((group >> 24) & 0xff) ^ ((group >> 16) & 0xff)) % MCGROUP_HASHSIZ;
#endif
}

static inline struct mcgroup **find_mcgroup(u32 group)
{
   struct mcgroup **pp = &mcfwglob.grouphashtab[grouphashval(group)];
   for ( ; *pp && (*pp)->group != group; pp = &(*pp)->link)
	  ;
   return pp;
}

static void mcgroup_checking_timer_handler(unsigned long arg)
{
   struct mcgroup *grp;
   unsigned long flags;
   int changed = 0;

   if (mcfwglob.debug)
   printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: checking timer elapsed\n",
					NIPQUAD(arg));

   spin_lock_irqsave(&mcfwglob.group_lock, flags);
   if ((grp = *find_mcgroup((u32)arg)) != 0) {
	  struct mcgroupmember **pp, *p;
	  pp = &grp->members;
	  while ((p = *pp) != 0) {
		 if (p->checking) {
			*pp = p->next;
	        mcgroupmember_free(grp, p, "removed");
			changed = 1;
		 } else {
			pp = &p->next;
		 }
	  }
	  if (changed) {
		 if (grp->members == 0) {
            mcgroup_del(grp->group, 0);
		 } else {
		    calc_routes(grp);
		 }
	  }
   }
   spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
}


static struct mcgroup *find_or_add_mcgroup(u32 group)
{
   struct mcgroup **pp, *p;

   if (LOCAL_MCAST(group))
	  return 0;
   if (LOCALAREA_MCAST(group))
	  return 0;

   pp = find_mcgroup(group);
   if ((p = *pp) == 0) {
	  p = (struct mcgroup *)kmalloc(sizeof(struct mcgroup), GFP_ATOMIC);
	  if (p == 0) {
		 printk(KERN_ERR "mcfw: group %u.%u.%u.%u: alloc failed\n",
						 NIPQUAD(group));
		 return 0;
	  }
	  memset(p, 0, sizeof(struct mcgroup));
	  p->group = group;
	  p->groupmac[0] = 0x01;
	  p->groupmac[1] = 0x00;
	  p->groupmac[2] = 0x5e;
	  p->groupmac[3] = (u8)(ntohl(group) >> 16) & 0x7f;
	  p->groupmac[4] = (u8)(ntohl(group) >> 8) & 0xff;
	  p->groupmac[5] = (u8)(ntohl(group)) % 0xff;
	  init_timer(&p->checking_timer);
	  p->checking_timer.function = mcgroup_checking_timer_handler;
	  p->checking_timer.data = group;
	  p->group_interval = mcfwglob.group_interval*HZ;
	  p->acct.group = group;
      do_gettimeofday(&p->acct.jointime);
	  *pp = p;
   }
   return p;
}

static inline void mcgroup_report_acct(struct mcgroup *grp)
{
   if (mcfwglob.group_acct_cb)
      (*mcfwglob.group_acct_cb)(&grp->acct);
}

void mcfw_register_group_acct_cb(void (*cb)(struct mcgroupacct *p))
{
   mcfwglob.group_acct_cb = cb;
}

void mcfw_register_source_acct_cb(void (*cb)(struct mcsourceacct *p))
{
   mcfwglob.source_acct_cb = cb;
}

static void mcgroup_del(u32 group, int fast)
{
   struct mcgroup **pp, *grp;

   pp = find_mcgroup(group);
   if ((grp = *pp) != 0) {
	  *pp = grp->link;
	  del_timer(&grp->checking_timer);
	  mcgroupmember_delete_all(grp);
	  mcgroupsource_delete_all(grp);
	  if (mcfwglob.debug)
	  printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: %sleave\n",
					   NIPQUAD(group), fast ? "fast " : "");
      do_gettimeofday(&grp->acct.endtime);
	  mcgroup_report_acct(grp);
      kfree(grp);
   }
}

/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */

static void mcfw_change_member(struct mcfw_netdriver *drv,
                               int port, 
							   u32 ipaddr,
							   u8 ethaddr[ETH_ALEN],
							   u32 group,
							   int subtype,
							   unsigned n,
							   u32 *slist)
{
   struct mcgroup *grp;
   if ((grp = find_or_add_mcgroup(group)) != 0)
      mcgroupmember_change(grp, drv, port,
						   ipaddr, ethaddr,
						   subtype, n, slist);
}

static void mcfw_add_member(struct mcfw_netdriver *drv,
							int port, 
							u32 ipaddr,
							u8 ethaddr[ETH_ALEN],
							u32 group)
{
   struct mcgroup *grp;
   if ((grp = find_or_add_mcgroup(group)) != 0) {
      mcgroupmember_change(grp, drv, port,
						   ipaddr, ethaddr,
						   IGMPV3_MODE_IS_EXCLUDE, 0, 0);
   }
}

static void mcfw_del_member(struct mcfw_netdriver *drv,
							int port, 
							u32 ipaddr,
							u8 ethaddr[ETH_ALEN],
							u32 group)
{
   struct mcgroup *grp;

   if ((grp = *find_mcgroup(group)) != 0) {
      mcgroupmember_change(grp, drv, port,
						   ipaddr, ethaddr,
						   IGMPV3_MODE_IS_INCLUDE, 0, 0);
   }
}
                            
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */

static void mcfw_query_sent(struct mcfw_netdriver *drv,
                            mcfw_portset portset,
							u32 group, unsigned long maxdelay)
{
   struct mcgroup *grp;
   unsigned long flags;

   if (mcfwglob.debug)
   {
	  char buf[128];
	  printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: query %s:%s %lusec\n",
			   NIPQUAD(group),
			   drv->name, portset2str(&portset, buf, sizeof(buf)), 
			   maxdelay/HZ);
   }

   if (group) {
      spin_lock_irqsave(&mcfwglob.group_lock, flags);
      if ((grp = *find_mcgroup(group)) != 0) {
		 struct mcgroupmember *p;
		 for (p = grp->members; p; p = p->next)
			p->checking = 1;
		 mod_timer(&grp->checking_timer, jiffies + maxdelay + 1);
		 if (mcfwglob.debug)
		 printk(KERN_DEBUG "mcfw: group %u.%u.%u.%u: setup checking timer (%lusecs)\n",
						  NIPQUAD(group), maxdelay/HZ);
	  }
      spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
   }
}

/* --------------------------------------------------------------------- */

static struct iphdr *find_iph(struct sk_buff *skb)
{
   struct vlan_ethhdr *hdr = (struct vlan_ethhdr *)skb->data;

   if (skb->len < sizeof(struct ethhdr))
	  return 0;

   if (hdr->h_vlan_proto == __constant_htons(ETH_P_8021Q)) {
      if (skb->len < sizeof(struct vlan_ethhdr)+sizeof(struct iphdr))
	     return 0;
	  if (hdr->h_vlan_encapsulated_proto == __constant_htons(ETH_P_IP))
	     return (struct iphdr *)(skb->data+sizeof(struct vlan_ethhdr));
   } else if (hdr->h_vlan_proto == __constant_htons(ETH_P_IP)) {
      if (skb->len < sizeof(struct ethhdr)+sizeof(struct iphdr))
	     return 0;
	  return (struct iphdr *)(skb->data+sizeof(struct ethhdr));
   }
   return 0;
}

void _mcfw_snoop_recv(struct mcfw_netdriver *drv,
                     int port, struct sk_buff *skb)
{
   struct ethhdr *ethh;
   struct iphdr *iph;
   struct igmphdr *igmph;
   size_t iphlen;
   size_t len;

   if (skb->len < sizeof(struct ethhdr))
	  return;
   if ((iph = find_iph(skb)) == 0)
	  return;
   len = ((u8 *)iph) - skb->data;
   if (skb->len < len + sizeof(struct iphdr))
	  return;
   iphlen = (iph->ihl<<2);
   if (iphlen < sizeof(struct iphdr))
	  return;
   if (iph->version != 4)
	  return;
   if (iph->protocol != IPPROTO_IGMP)
	  return;
   iph->check = 0;
   iph->check = ip_compute_csum((unsigned char *)iph, iph->ihl<<2);
   igmph = (struct igmphdr *)(((u8 *)iph)+iphlen);
   len = ((u8 *)iph) - skb->data + iphlen;
   if (skb->len < len + sizeof(struct igmphdr))
	  return;
   ethh = (struct ethhdr *)skb->data;

   switch (igmph->type) {
	  case IGMP_HOST_LEAVE_MESSAGE:
		 mcfw_del_member(drv, port,
						 iph->saddr, ethh->h_source,
						 igmph->group);
		 break;
	  case IGMP_HOST_MEMBERSHIP_REPORT:
	  case IGMPV2_HOST_MEMBERSHIP_REPORT:
		 mcfw_add_member(drv, port,
						 iph->saddr, ethh->h_source,
						 igmph->group);
		 break;
	  case IGMPV3_HOST_MEMBERSHIP_REPORT:
		 {
            struct igmpv3_report *hp = (struct igmpv3_report *)igmph;
			struct igmpv3_grec *p = &hp->grec[0];
			size_t offset;
			u16 i;

			len += sizeof(struct igmpv3_report);
            if (skb->len < len) return;

			for (i = 0; i < ntohs(hp->ngrec); i++) {
               len = ((u8 *)p) - skb->data + sizeof(struct igmpv3_grec);
			   if (skb->len < len) return;
			   len += ntohs(p->grec_nsrcs)*sizeof(u32);
			   len += ntohs(p->grec_auxwords)*sizeof(u32);
			   if (skb->len < len) return;
			   switch (p->grec_type) {
				  case IGMPV3_CHANGE_TO_EXCLUDE:
				  case IGMPV3_MODE_IS_EXCLUDE:
			         if (p->grec_nsrcs == 0) {
		                mcfw_add_member(drv, port,
										iph->saddr, ethh->h_source,
										p->grec_mca);
					 } else {
						mcfw_change_member(drv, port,
										   iph->saddr, ethh->h_source,
										   p->grec_mca, p->grec_type,
										   ntohs(p->grec_nsrcs), p->grec_src);
					 }
					 break;
				  case IGMPV3_MODE_IS_INCLUDE:
				  case IGMPV3_CHANGE_TO_INCLUDE:
					 if (p->grec_nsrcs) {
						mcfw_change_member(drv, port,
										   iph->saddr, ethh->h_source,
										   p->grec_mca, p->grec_type,
										   ntohs(p->grec_nsrcs), p->grec_src);
					 } else {
		                mcfw_del_member(drv, port,
										iph->saddr, ethh->h_source,
										p->grec_mca);
					 }
					 break;
				  case IGMPV3_ALLOW_NEW_SOURCES:
				  case IGMPV3_BLOCK_OLD_SOURCES:
					 mcfw_change_member(drv, port,
										iph->saddr, ethh->h_source,
										p->grec_mca, p->grec_type,
										ntohs(p->grec_nsrcs), p->grec_src);
					 break;
			   }
			   offset =   sizeof(struct igmpv3_grec)
						+ ntohs(p->grec_nsrcs)*sizeof(u32)
						+ ntohs(p->grec_auxwords)*sizeof(u32);
			   p = (struct igmpv3_grec *)(((u8 *)p) + offset);
			}
		 }
		 break;
   }
}

void _mcfw_snoop_send(struct mcfw_netdriver *drv,
                     mcfw_portset portset, struct sk_buff *skb)
{
   struct iphdr *iph;
   struct igmphdr *igmph;
   unsigned long maxdelay; /* in 1/10 sec */
   int suppress = 0;
   size_t iphlen;
   u32 group;
   u16 len;

   if (skb->len < sizeof(struct ethhdr))
	  return;
   if ((iph = find_iph(skb)) == 0)
	  return;
   len = ((u8 *)iph) - skb->data;
   if (skb->len < len + sizeof(struct iphdr))
	  return;
   iphlen = (iph->ihl<<2);
   if (iphlen < sizeof(struct iphdr))
	  return;
   if (iph->version != 4)
	  return;
   if (iph->protocol != IPPROTO_IGMP)
	  return;
   iph->check = 0;
   iph->check = ip_compute_csum((unsigned char *)iph, iph->ihl<<2);
   igmph = (struct igmphdr *)(((u8 *)iph)+iphlen);
   len = ((u8 *)iph) - skb->data + iphlen;
   if (skb->len < len + sizeof(struct igmphdr))
	  return;
   group = igmph->group;

   if (igmph->type == IGMP_HOST_MEMBERSHIP_QUERY) {
	  len = ((u8 *)igmph - skb->data);
	  len = ntohs(iph->tot_len) - iphlen;
	  if (len == sizeof(struct igmphdr)) { /* V1 or V2 */
		 if (igmph->code == 0) { /* V1 */
			maxdelay = 100;
			group = 0;
		 } else { /* V2 */
			maxdelay = igmph->code;
		 }
	  } else if (len < sizeof(struct igmpv3_query)) {
		 return; /* illegal packet */
	  } else { /* V3 */
	     struct igmpv3_query *igmpv3h = (struct igmpv3_query *)igmph;
	     unsigned long qqic;
	     unsigned long robustness;
	     /* RFC 3376 4.1.1 */
	     if (igmpv3h->code < 128)
		    maxdelay = igmpv3h->code;
	     else
		    maxdelay = IGMPV3_MRC(igmpv3h->code);

	     /* RFC 3376 4.1.5 */
	     suppress = igmpv3h->suppress;
   
	     /* RFC 3376 4.1.6 */
	     if (igmpv3h->qrv) {
		    robustness = igmpv3h->qrv;
	     } else {
		    robustness = DEFAULT_ROBUSTNESS;
	     }
	     /* RFC 3376 4.1.7 */
	     if (igmpv3h->qqic < 128)
		    qqic = igmpv3h->qqic;
	     else
	        qqic = IGMPV3_MRC(igmpv3h->qqic);

         if (!suppress && group == 0) {
			if (qqic) {
			   mcfwglob.query_interval = qqic;
			} else {
			   mcfwglob.query_interval = DEFAULT_QUERY_INTERVAL;
			}
			mcfwglob.robustness = robustness;
		 }
	  }
	  if (!suppress && group == 0) {
		 mcfwglob.group_interval = 
			 (mcfwglob.robustness * mcfwglob.query_interval) + maxdelay/10 + 1;
	  }
	  mcfw_query_sent(drv, portset, group, maxdelay*(HZ/IGMP_TIMER_SCALE));
   }
}

/* --------------------------------------------------------------------- */

struct encapdesc {
   struct encapdesc               *next;
   u8                              mac[ETH_ALEN];
   unsigned short                  vlanid;        /* hostorder */
   u16                             pppoesid;      /* netorder */
   struct mcfw_iface_rx_statistic  stat;
   void                           *iface;
};

static struct mcsourcedata {
   char                            name[MCFW_MAXSOURCENAME];
   atomic_t                        index;
   struct encapdesc               *descs;
   struct mcfw_iface_rx_statistic  stat;
} mcsourcedata[MCFW_MAXSOURCE];

/* --------------------------------------------------------------------- */

int mcfw_multicast_forward_alloc_id(char *name)
{
   int freeid = -1;
   int i;

   if (strlen(name) >= MCFW_MAXSOURCENAME) {
	  printk(KERN_ERR "mcfw_multicast_forward_alloc_id(%s): name too long",
					  name);
	  return -1;
   }

   for (i=0; i < MCFW_MAXSOURCE; i++) {
	  if (strncmp(mcsourcedata[i].name, name, MCFW_MAXSOURCENAME) == 0)
		 return i;
	  if (mcsourcedata[i].name[0] == 0)
		 freeid = i;
   }
   if (freeid < 0) {
	  printk(KERN_ERR "mcfw_multicast_forward_alloc_id(%s): no id left",
					  name);
	  return -1;
   }
   strncpy(mcsourcedata[freeid].name, name, MCFW_MAXSOURCENAME);
   return freeid;
}

void mcfw_multicast_forward_free_id(int id)
{
   unsigned long flags;
   if (id >= 0 && id < MCFW_MAXSOURCE) {
      struct mcsourcedata *dp = &mcsourcedata[id];
      struct encapdesc *p;

      spin_lock_irqsave(&mcfwglob.group_lock, flags);
	  memset(dp->name, 0, MCFW_MAXSOURCENAME);
	  while ((p = dp->descs) != 0) {
		 dp->descs = p->next;
		 kfree(p);
	  }
      spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
   }
}

/* --------------------------------------------------------------------- */

static char *encapdesc2str(char *name, struct encapdesc *p,
                           char *buf, size_t size)
{
   snprintf(buf, size, "%s: %02x:%02x:%02x:%02x:%02x:%02x, vlan %hu, pppoe %hu",
			           name,
			           p->mac[0], p->mac[1], p->mac[2],
					   p->mac[3], p->mac[4], p->mac[5],
			           p->vlanid, p->pppoesid);
   return buf;
}

/* --------------------------------------------------------------------- */

int mcfw_multicast_forward_ethernet_del(int id, unsigned char mac[ETH_ALEN])
{
   unsigned long flags;
   char buf[128];

   if (id >= 0 && id < MCFW_MAXSOURCE) {
      struct mcsourcedata *dp = &mcsourcedata[id];
      struct encapdesc *p,  **pp;

      spin_lock_irqsave(&mcfwglob.group_lock, flags);
	  for (pp = &dp->descs; (p = *pp) != 0; pp = &(*pp)->next) {
	 	 if (memcmp(p->mac, mac, ETH_ALEN) == 0) {
			*pp = p->next;
			break;
		 }
	  }
      spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
	  if (p) {
		 if (mcfwglob.debug)
		 printk(KERN_DEBUG "mcfw: forward disabled (%s)\n",
							encapdesc2str(dp->name, p, buf, sizeof(buf)));
		 kfree(p);
      }
	  return 0;
   }
   printk(KERN_ERR "mcfw: forward del: id %d out of range\n", id);
   return -1;
}

int mcfw_multicast_forward_ethernet_add(int id,
                                        unsigned char mac[ETH_ALEN],
                                        unsigned short vlanid,
										unsigned short pppoesid,
										void *iface)
{
   unsigned long flags;
   char buf[128];

   if (id >= 0 && id < MCFW_MAXSOURCE) {
      struct mcsourcedata *dp = &mcsourcedata[id];
      struct encapdesc *p,  **pp;

      p = (struct encapdesc *)kmalloc(sizeof(struct encapdesc), GFP_ATOMIC);
	  if (p == 0) {
		 printk(KERN_ERR "mcfw: forward add: no memory (%s)\n", dp->name);
		 return -1;
	  }
	  memset(p, 0, sizeof(struct encapdesc));
	  memcpy(p->mac, mac, ETH_ALEN);
	  p->vlanid = vlanid;
	  p->pppoesid = htons(pppoesid);
	  p->iface = iface;
      spin_lock_irqsave(&mcfwglob.group_lock, flags);
	  for (pp = &dp->descs; *pp; pp = &(*pp)->next) {
	 	 if (memcmp((*pp)->mac, mac, ETH_ALEN) == 0) {
			(*pp)->vlanid = vlanid;
			(*pp)->pppoesid = htons(pppoesid);
            spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
            if (mcfwglob.debug)
			printk(KERN_DEBUG "mcfw: forward changed (%s)\n",
							  encapdesc2str(dp->name, p, buf, sizeof(buf)));
			kfree(p);
			return 0;
		 }
	  }
	  *pp = p;
      spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
	  if (mcfwglob.debug)
	  printk(KERN_DEBUG "mcfw: forward enabled (%s)\n",
						encapdesc2str(dp->name, p, buf, sizeof(buf)));
	  return 0;
   }
   printk(KERN_ERR "mcfw: forward add: id %d out of range\n", id);
   return -1;
}

void mcfw_multicast_get_statistic(struct mcfw_statistic *p, int reset)
{
   unsigned long flags;
   spin_lock_irqsave(&mcfwglob.group_lock, flags);
   if (p) *p = mcfwglob.stat;
   if (reset) memset(&mcfwglob.stat, 0, sizeof(struct mcfw_statistic));
   spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
}

static inline int etheraddr_is_multicast(unsigned char mac[ETH_ALEN])
{
   return    mac[0] == 0x01
		  && mac[1] == 0x00
		  && mac[2] == 0x5e
		  && (mac[3] & 0x80) == 0;
}

static inline struct iphdr *
find_iph_for_encap(struct encapdesc *ep, struct sk_buff *skb)
{
   unsigned char *p = skb->data;
   struct pppoe_hdr *pppoeh;
   u16 proto;
   int offset = 0;

   if (ep->vlanid) {
      struct vlan_ethhdr *vlanh = (struct vlan_ethhdr *)p;
	  if (skb->len < offset+sizeof(struct vlan_ethhdr))
		 return 0;
      if (   memcmp(vlanh->h_dest, ep->mac, ETH_ALEN) != 0
		  && !etheraddr_is_multicast(vlanh->h_dest))
		 return 0;
	  if (vlanh->h_vlan_proto != __constant_htons(ETH_P_8021Q))
		 return 0;
      if ((ntohs(vlanh->h_vlan_TCI) & VLAN_VID_MASK) != ep->vlanid)
		 return 0;
	  p += sizeof(struct vlan_ethhdr);
	  offset += sizeof(struct vlan_ethhdr);
	  proto = vlanh->h_vlan_encapsulated_proto;
   } else {
      struct ethhdr *ethh = (struct ethhdr *)p;
	  if (skb->len < offset+sizeof(struct ethhdr))
		 return 0;
      if (   memcmp(ethh->h_dest, ep->mac, ETH_ALEN) != 0
		  && !etheraddr_is_multicast(ethh->h_dest))
		 return 0;
	  p += sizeof(struct ethhdr);
	  proto = ethh->h_proto;
   }
   if (ep->pppoesid) {
	  if (proto != __constant_htons(ETH_P_PPP_SES))
		 return 0;
	  pppoeh = (struct pppoe_hdr *)p;
	  if (skb->len < offset+sizeof(struct pppoe_hdr)+2+sizeof(struct iphdr))
		 return 0;
	  if (pppoeh->sid != ep->pppoesid)
		 return 0;
	  p += sizeof(struct pppoe_hdr);
	  if (p[0] != 0x00 || p[1] != 0x21)
		 return 0;
	  p += 2;
	  return (struct iphdr *)p;
   } else {
	  if (skb->len < offset+sizeof(struct iphdr))
		 return 0;
	  if (proto != __constant_htons(ETH_P_IP))
		 return 0;
	  return (struct iphdr *)p;
   }
}

static int mcfw_multicast_forward(int sourceid,
                                  struct sk_buff *skb,
								  struct iphdr *iph,
								  struct mcfw_iface_rx_statistic *statp)
{
   struct mcsource *source;
   struct mcgroup *grp;
   unsigned long flags;
   struct ethhdr *ethh;
   int i, size;
   int count;

   if (   iph->protocol != IPPROTO_UDP
	   || !MULTICAST(iph->daddr)
	   || LOCAL_MCAST(iph->daddr)
	   || LOCALAREA_MCAST(iph->daddr))
      return 0;

   spin_lock_irqsave(&mcfwglob.group_lock, flags);
   grp = *find_mcgroup(iph->daddr);
   if (grp == 0)
	  goto drop;
   source = mcsource_find_or_add(grp, iph->saddr);
   if (source == 0 || source->nroutes == 0)
	  goto drop;

#ifdef CONFIG_AVM_PA
   avm_pa_do_not_accelerate(skb);
#endif

   mcfw_counter_add(skb, &statp->forwarded);

   /*
	* prepare skb
	*/
   size = ((u8 *)iph) - skb->data - sizeof(struct ethhdr);

   if (size) {
	  if (size > 0) {
		 skb_pull(skb, size);
	  } else {
		 size = -size;
		 if (skb_headroom(skb) < size) {
			struct sk_buff *nskb;
			nskb = skb_realloc_headroom(skb, size+VLAN_HLEN);
			if (!nskb) goto drop;
			skb = nskb;
			dev_kfree_skb_any(skb);
		 }
		 skb_push(skb, (unsigned int)size);
	  }
   }
   ethh = (struct ethhdr *)skb->data;
   ethh->h_proto = __constant_htons(ETH_P_IP);
   memcpy(ethh->h_dest, grp->groupmac, ETH_ALEN);

   mcfw_counter_add(skb, &mcfwglob.stat.forwarded);
   mcfw_counter_add(skb, &grp->acct.forwarded);

   source->inuse++; 
   count = source->nroutes;
   spin_unlock_irqrestore(&mcfwglob.group_lock, flags);

   mcsource_check_skb(source, skb);
   for (i = 0; count && i < MAX_NETDRIVER; i++) {
	  struct mcroute *route = &source->routes[i];
	  struct mcfw_netdriver *drv;

	  if (route->forward == 0)
		 continue;
	  if ((drv = mcfwglob.drivers[i]) == 0)
		 continue;
	  if (drv->has_singleport) {
		 if (drv->netdriver_mc_transmit_single) {
	        struct sk_buff *tskb = skb;
			if (--count) {
			   if ((tskb = skb_copy(skb, GFP_ATOMIC)) == 0) {
				  mcfw_counter_add(skb, &grp->acct.no_memory);
				  mcfw_counter_add(skb, &mcfwglob.stat.no_memory);
				  continue;
			   }
			}
			(*drv->netdriver_mc_transmit_single)(
									   drv->privatedata,
									   sourceid,
									   route->n,
									   route->members,
									   tskb);
		 }
	  } else if (drv->netdriver_mc_transmit) {
		 struct sk_buff *tskb = skb;
		 if (--count) {
			if ((tskb = skb_copy(skb, GFP_ATOMIC)) == 0) {
			   mcfw_counter_add(skb, &grp->acct.no_memory);
			   mcfw_counter_add(skb, &mcfwglob.stat.no_memory);
			   continue;
			}
		 }
		 (*drv->netdriver_mc_transmit)(
									drv->privatedata,
									sourceid,
									route->portset,
									tskb);
	  }
   }
   if (count)
	  kfree_skb(skb);

   spin_lock_irqsave(&mcfwglob.group_lock, flags);
   source->inuse--;
   while (mcfwglob.source_for_delete) {
	  source = mcfwglob.source_for_delete;
	  mcfwglob.source_for_delete = source->next;
	  mcsource_free(source);
   }
   spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
   return 1;

drop:
   mcfw_counter_add(skb, &statp->dropped);
   if (grp) mcfw_counter_add(skb, &grp->acct.dropped);
   mcfw_counter_add(skb, &mcfwglob.stat.dropped);
   spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
   dev_kfree_skb_any(skb);
   return 2;
}

int mcfw_multicast_forward_ethernet(int id, struct sk_buff *skb)
{
   struct mcsourcedata *dp;
   struct encapdesc *p;
   struct iphdr *iph = 0;

   if (id < 0 || id >= MCFW_MAXSOURCE)
	  return 0;

   dp = &mcsourcedata[id];
   for (p = dp->descs; p; p = p->next) {
      if ((iph = find_iph_for_encap(p, skb)) != 0)
		 break;
   }
   if (!iph)
	  return 0;
   return mcfw_multicast_forward(id, skb, iph, &p->stat);
}

int mcfw_multicast_forward_ip(int id, struct sk_buff *skb)
{
   struct iphdr *iph = (struct iphdr *)skb->data;
   struct mcsourcedata *dp;

   if (id < 0 || id >= MCFW_MAXSOURCE)
	  return 0;

   dp = &mcsourcedata[id];
   return mcfw_multicast_forward(id, skb, iph, &dp->stat);
}

int mcfw_multicast_get_iface_statistic(int id,
                                       void *iface,
							           struct mcfw_iface_rx_statistic *stat)
{
   struct mcsourcedata *dp;
   struct encapdesc *p;
   unsigned long flags;

   if (id < 0 || id >= MCFW_MAXSOURCE)
	  return -1;
   
   spin_lock_irqsave(&mcfwglob.group_lock, flags);
   dp = &mcsourcedata[id];
   for (p = dp->descs; p; p = p->next) {
	  if (p->iface == iface) {
		 *stat = p->stat;
		 break;
	  }
   }
   if (p == 0)
	  *stat = dp->stat;
   spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
   return 0;
}

int mcfw_get_version(void)
{
   return MCFW_VERSION;
}

/* --------------------------------------------------------------------- */
/* --------- register/unregister functions ----------------------------- */
/* --------------------------------------------------------------------- */

int mcfw_netdriver_register(struct mcfw_netdriver *drv)
{
   int i;
   for (i = 0; i < MAX_NETDRIVER; i++) {
	  if (drv == mcfwglob.drivers[i]) 
		 return -EEXIST;
   }
   for (i = 0; i < MAX_NETDRIVER; i++) {
	  if (mcfwglob.drivers[i] == 0) {
		 mcfwglob.drivers[i] = drv;
		 drv->index = i;
		 drv->has_singleport = drv->netdriver_mc_transmit_single ? 1 : 0;
		 return 0;
	  }
   }
   return -ENFILE;
}

void mcfw_netdriver_unregister(struct mcfw_netdriver *drv)
{
   unsigned long flags;
   int i;

   for (i = 0; i < MAX_NETDRIVER; i++) {
	  if (drv == mcfwglob.drivers[i]) {
		 mcfwglob.drivers[i] = 0;
		 break;
	  }
   }
   spin_lock_irqsave(&mcfwglob.group_lock, flags);
   for (i = 0; i < MCGROUP_HASHSIZ; i++) {
	  struct mcgroup *grp, *next;
	  grp = mcfwglob.grouphashtab[i];
      while (grp) {
         struct mcgroupmember *p, **pp = &grp->members;
         next = grp->link;
		 pp = &grp->members;
		 while ((p = *pp) != 0) {
			if (p->drv == drv) {
			   *pp = p->next;
               mcgroupmember_free(grp, p, "drv unregister");
			} else {
			   pp = &p->next;
			}
		 }
         if (grp->members == 0) {
            mcgroup_del(grp->group, 0);
         } else {
            calc_routes(grp);
         }
         grp = next;
	  }
   }
   spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
}

/* --------------------------------------------------------------------- */

EXPORT_SYMBOL(_mcfw_snoop_recv);
EXPORT_SYMBOL(_mcfw_snoop_send);
EXPORT_SYMBOL(mcfw_multicast_forward_alloc_id);
EXPORT_SYMBOL(mcfw_multicast_forward_free_id);
EXPORT_SYMBOL(mcfw_multicast_forward_ethernet_add);
EXPORT_SYMBOL(mcfw_multicast_forward_ethernet_del);
EXPORT_SYMBOL(mcfw_multicast_get_iface_statistic);
EXPORT_SYMBOL(mcfw_multicast_get_statistic);
EXPORT_SYMBOL(mcfw_multicast_forward_ethernet);
EXPORT_SYMBOL(mcfw_multicast_forward_ip);
EXPORT_SYMBOL(mcfw_register_group_acct_cb);
EXPORT_SYMBOL(mcfw_register_source_acct_cb);
EXPORT_SYMBOL(mcfw_report_source_acct);
EXPORT_SYMBOL(mcfw_get_version);
EXPORT_SYMBOL(mcfw_netdriver_register);
EXPORT_SYMBOL(mcfw_netdriver_unregister);

/* --------------------------------------------------------------------- */
/* --------------------------------------------------------------------- */

static int mcfw_write_cmds (struct file *file, const char *buffer, unsigned long count, void *data)
{
	char    pp_cmd[100];
	char*	argv[10];
	int		argc = 0;
	char*	ptr_cmd;
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
			printk(KERN_ERR "mcfw: too many parameters dropping the command\n");
			return -EIO;
		}

		ptr_cmd = strsep(&ptr_next_tok, delimitters);
	} while (ptr_cmd != NULL);

	argc--;

    /* Display Command Handlers */
    if (strcmp(argv[0], "debug") == 0) {
	   mcfwglob.debug = 1;
	   printk(KERN_DEBUG "mcfw: debug on\n");
    } else if (strcmp(argv[0], "nodebug") == 0) {
	   mcfwglob.debug = 0;
	   printk(KERN_DEBUG "mcfw: debug off\n");
	}

	return count;
}

/* --------------------------------------------------------------------- */

static int drivers_show(struct seq_file *m, void *v)
{
   int count = 0;
   int i;

   for (i=0; i < MAX_NETDRIVER; i++) {
      struct mcfw_netdriver *p = mcfwglob.drivers[i];
	  if (p) {
	     count++;
		 if (p->has_singleport)
			seq_printf(m,  "%s: single port\n", p->name);
		 else
			seq_printf(m,  "%s: multiple ports\n", p->name);
	  }
   }
   if (count == 0)
	  seq_printf(m,  "No drivers registered\n");
   return 0;
}

static int drivers_show_open(struct inode *inode, struct file *file)
{
    return single_open(file, drivers_show, PDE(inode)->data);
}

static const struct file_operations driver_show_fops = {
	.owner   = THIS_MODULE,
	.open    = drivers_show_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

/* --------------------------------------------------------------------- */

static inline u64 timeval2msec(struct timeval *tv)
{
   return tv->tv_sec * (u64)1000 + tv->tv_usec / 1000;
}

static int group_show(struct seq_file *m, void *v)
{
   struct timeval etime;
   unsigned long flags;
   int i;

   do_gettimeofday(&etime);

   spin_lock_irqsave(&mcfwglob.group_lock, flags);
   for (i = 0; i < MCGROUP_HASHSIZ; i++) {
	  struct mcgroup *grp;
	  for (grp = mcfwglob.grouphashtab[i]; grp; grp = grp->link) {
	      struct mcgroupacct *ap = &grp->acct;
		  struct mcgroupmember *mp;
		  struct mcsource *sp;
		  u64 duration, delay, mbits, npkts;
		  unsigned long durationms, delayms, bits, npktrem;
		  char buf[128];

		  if (ap->starttime.tv_sec == 0) {
		     delay = 0;
			 delayms = 0;
			 duration = 0;
			 durationms = 0;
			 mbits = 0;
			 bits = 0;
			 npkts = 0;
			 npktrem = 0;
		  } else {
			 delay = timeval2msec(&ap->starttime)-timeval2msec(&ap->jointime);
		     duration = timeval2msec(&etime)-timeval2msec(&ap->starttime);
			 delayms = do_div(delay, 1000);
		     durationms = do_div(duration, 1000);

			 if (duration) {
		     mbits = ap->forwarded.nbytes;
		     do_div(mbits, duration);
		     do_div(mbits, 125);
		     bits = do_div(mbits, 1000);
			 npkts = ap->forwarded.npkts*1000;
			 do_div(npkts, duration);
			 npktrem = do_div(npkts, 1000);
			 } else {
			    mbits = 0;
			    bits = 0;
			    npkts = 0;
			    npktrem = 0;
			 }
		  }
		  seq_printf(m,  "%u.%u.%u.%u: %lu.%03lu Mbits/s %lu.%03lu pkts/s delay %lu.%03lusec duration %lu.%03lusec\n",
		                 NIPQUAD(grp->group),
						 (unsigned long)mbits, bits,
						 (unsigned long)npkts, npktrem,
		                 (unsigned long)delay, delayms,
						 (unsigned long)duration, durationms);
		  for (sp = grp->sources; sp; sp = sp->next) {
		     seq_printf(m, "   < %u.%u.%u.%u %sforward",
			                     NIPQUAD(sp->ipaddr),
								 sp->nroutes ? "" : "NO ");
			 if (sp->no_rtp) {
				seq_printf(m, "\n");
             } else {
				seq_printf(m, " (RTP");
			    if (sp->acct.seqduplicate)
				   seq_printf(m, " dup %lu", (unsigned long)sp->acct.seqduplicate);
			    if (sp->acct.seqtoolate)
				   seq_printf(m, " late %lu", (unsigned long)sp->acct.seqtoolate);
			    if (sp->acct.seqwrong)
				   seq_printf(m, " wrong %lu", (unsigned long)sp->acct.seqwrong);
			    if (sp->acct.lost)
				   seq_printf(m, " lost %lu", (unsigned long)sp->acct.lost);
			    if (sp->acct.maxlost)
				   seq_printf(m, " maxlost %lu", (unsigned long)sp->acct.lost);
				seq_printf(m, ")\n");
			 }
		  }
		  for (mp = grp->members; mp; mp = mp->next) {
		     seq_printf(m, "   > %u.%u.%u.%u (%s:%d) %s\n",
			               NIPQUAD(mp->ipaddr),
						   mp->drv->name, mp->port,
					       mcfilter2str(&mp->filter, buf, sizeof(buf)));
		  }
	  }
   }
   spin_unlock_irqrestore(&mcfwglob.group_lock, flags);
   return 0;
}

static int group_show_open(struct inode *inode, struct file *file)
{
    return single_open(file, group_show, PDE(inode)->data);
}

static const struct file_operations group_show_fops = {
	.owner   = THIS_MODULE,
	.open    = group_show_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

/* --------------------------------------------------------------------- */

static struct proc_dir_entry *dir_entry = 0;

/* --------------------------------------------------------------------- */

static int __init mcfw_init_module(void)
{
   struct proc_dir_entry *file_entry;

   dir_entry = proc_net_mkdir(&init_net, "mcfastforward", init_net.proc_net);
   if (dir_entry) {
	  dir_entry->read_proc = 0;
      dir_entry->write_proc = 0;
   }

   file_entry = create_proc_entry("control", S_IFREG|S_IWUSR, dir_entry);
   if (file_entry) {
	  file_entry->data  	  = NULL;
	  file_entry->read_proc  = NULL;
	  file_entry->write_proc = mcfw_write_cmds;
   } 

   file_entry = proc_create("driver", S_IRUGO, dir_entry, &driver_show_fops);
   file_entry = proc_create("groups", S_IRUGO, dir_entry, &group_show_fops);

	printk(KERN_INFO "mcfw: IGMPv3 fast forwarding\n");

	return 0;
}

static void __exit mcfw_exit_module(void)
{
   remove_proc_entry("control", dir_entry);
   remove_proc_entry("driver", dir_entry);
   remove_proc_entry("groups", dir_entry);
   remove_proc_entry("mcfastforward", init_net.proc_net);
}

module_init(mcfw_init_module);
module_exit(mcfw_exit_module);
