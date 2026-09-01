#ifndef __MCAST_DATA_H_
#define __MCAST_DATA_H_

#include <linux/notifier.h>
#include <linux/mroute.h>


#define	IFX_INCREASE_COUNT 0
#define	IFX_DECREASE_COUNT 1



extern int mode;

#define	IFX_IGMP_PROXY
#define	IFX_SNOOPING
#define IS_PROXY			1
#define IS_SNOOPING		2
#define IFX_ETH_AS_WAN

/* Address used by switch to send query msg on behalf of routers */

#define	SRC_IPADDR		((unsigned long int) 0x00000000)
#define IFX_IFF_EBRIDGE 0x2
#define IFX_ROBUSTNESS_VAR	2
#define IFX_MAX_RESP_TIME	5

/* max delay for response to query (in seconds) */
#define IGMP_QUERY_RESPONSE_INTERVAL	(10 * HZ)	
#define IGMP_GROUP_MEMBERSHIP_INTERVAL	(260 * HZ)	
#define IGMP_ROUTER_INTERVAL			(120 * HZ)
#define IFX_IGMP_QUERY_INTERVAL			120

#define IFX_REPORT_INTERVAL				(60 * HZ)
#define IFX_IGMP_SIZE (sizeof(struct igmphdr)) + (sizeof(struct iphdr))
#define IFX_DEV_SIZE  (sizeof(struct net_device))
#define IGMP_SIZE (sizeof(struct igmphdr)+sizeof(struct iphdr)+4)

#define TEST_GROUP  htonl(0xE9030303L)


#define	 VIF_EMPTY		0
#define  VIF_ACTIVE		1
#define	 VIF_INACTIVE	2

#define	 RET_NEW		0
#define  RET_ACTIVE		1
#define	 RET_INACTIVE	2

#define NO_DEV			0
#define ADDR_NO_MATCH	1
#define ADDR_MATCH 		2

#define TRUE 1

#define FALSE 0


#define IFX_ERROR 		0
#define IFX_XMIT 		1
#define IFX_SENT_SKB	0


#define IFX_PRINT_ADDR23(addr1) \
        ((unsigned char *) &addr1)[0], \
        ((unsigned char *) &addr1)[1], \
        ((unsigned char *) &addr1)[2], \
        ((unsigned char *) &addr1)[3]

#define DEFAULT_METRIC		1	/* default subnet/tunnel metric     */
#define DEFAULT_THRESHOLD	1	/* default subnet/tunnel threshold  */

#define DEFAULT_PHY_RATE_LIMIT  0 	/* default phyint rate limit */
		
#define IFX_MAXMFC		64

#define MAX_PORTS		6

struct mfcCache	{
	__u32 origin;
	__u32 group;
	int vifi;
	int flags;
};
		

struct grouplist {
	__u32 group;
	int cacheFlag;
	struct grouplist *next;
};


struct viftable {
	
	struct vifctl vif;
	int flag;
	char vifname [IFNAMSIZ];
	struct net_device *vifdev;
};





		
struct McastReportTable {
	__u32 groupAddr;
	int gflag;

	struct timer_list		reportTimer;
	struct McastReportTable *next;
	struct McastReportTable *previous;
};

struct DeleteInfo {

	__u32 	groupAddr;

	struct net_device	*pNetDev;

	

};

struct McastMacInfo {
	unsigned char  mcastMac[6];
	unsigned short swArray[MAX_PORTS];
	unsigned long  portMap:6;

	struct McastMacInfo	*next;
	struct McastMacInfo	*previous;
};

struct McastOriginTable {
	
	__u32 origin;
	int vifIndex;
	
	struct McastOriginTable *next;
	struct McastOriginTable *previous;
};
		

struct McastDev{
	unsigned int timerFlag;
	unsigned long 		time;
	struct timer_list	igmpTimer;
	struct net_device	*dev;
	struct McastDev		*next;
	struct McastDev		*previous;
};

struct McastGroupTable{
	__u32 	GroupAddr;
	unsigned int reportFlag;
	int 	count;
	int		queryCount;
	int		queryGroupCount;

	struct timer_list		memberTimer;

	struct McastOriginTable *pOriginHead;
	struct McastOriginTable *pOriginTail;

	struct McastMacInfo		*pMacHead;
	struct McastMacInfo		*pMacTail;

	struct 	McastDev 		*pHead;
	struct 	McastDev 		*pTail;
	struct 	McastGroupTable *next;
	struct 	McastGroupTable *previous;
	
};



struct McastRouterTable{

	__u32 RouterAddr;
	struct net_device	*dev;
	struct timer_list	routerTimer;

	struct  McastRouterTable *next;
	struct  McastRouterTable *previous;
};


#endif /* __MCAST_DATA_H_ */
