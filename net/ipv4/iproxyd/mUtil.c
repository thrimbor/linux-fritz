#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/in.h>			/* Protocol Numbers */
#include <linux/slab.h> 		/* kmalloc */
#include <linux/igmp.h> 		/* struct igmphdr */
#include <linux/if_ether.h>		/* struct ethhdr */
#include <linux/if_vlan.h>		/* struct vehdr */
#include <linux/rtnetlink.h>	/* RT_SCOPE_HOST */
#include <linux/if_pppox.h>		/* pppoe_hdr */
#include <net/sock.h>			/* struct pppox_opt */
#include <linux/spinlock.h>		/* write_lock_bh */

#include <net/checksum.h> /* checksum calculations */

#include <linux/timer.h> /* struct timer_list */
#include <net/ip.h>

#include <linux/rtnetlink.h>

#include "../net/bridge/br_private.h"

#include "mUtil.h"
#include "mData.h"

extern int ifx_is_dev_bridge (struct net_device *dev);
extern int ifx_add_vif (struct vifctl *vif, int flag);
extern int ifx_del_vif (int vifi);
extern int ifx_mfc_add (struct mfcctl *, int flag);
extern int ifx_mfc_del (struct mfcctl *);

#ifdef CONFIG_ADM6996_SUPPORT
extern int adm_process_mac_table_request (unsigned int cmd, struct _MACENTRY_ *);
#endif // CONFIG_ADM6996_SUPPORT

#ifdef CONFIG_ADM6996_SUPPORT_MODULE
extern int adm_process_add_upnp_group (void);
#endif // CONFIG_ADM6996_SUPPORT

extern struct 	McastGroupTable *pGroupTableHead;
extern struct 	McastGroupTable *pGroupTableTail;
extern char 	upstreamIfName[IFNAMSIZ];
extern char     switchIfName[IFNAMSIZ];
extern struct McastRouterTable *pRouterTableHead;
extern struct McastRouterTable *pRouterTableTail;

extern int hw_snoop;
extern int gLatency;
extern int debug;

static struct viftable   gviftable [MAXVIFS];
static int gNumVifs;

struct timer_list queryTimer; /* timer list for General query interval */

// #define SW_PHYPORT_DEBUG

#ifdef SW_PHYPORT_DEBUG
static inline void dump_skb(u32 len, char * data)
{
	int i;
	for(i=0;i<len;i++){	
		printk("%2.2x ",(u8)(data[i]));
		if (i % 16 == 15)
			printk("\n");
	}
	printk("\n");
}
#else
#define dump_skb(a,b)
#endif

int ifx_check_for_group_entry
	(
		__u32 groupAddr
	)
{
	struct 	McastGroupTable *pGTemp		= NULL;
	int count = 0;

	pGTemp = ifx_search_group_table(groupAddr, &count);
	if ( pGTemp == NULL )
	{
		return 0;
	}
	return 1;

}


int ifx_check_dev_entry
	(
	   	struct net_device  *Dev	,
		__u32 groupAddr
	)
{
	
	struct 	McastGroupTable *pGTemp		= NULL;
	struct 	McastDev		*pMDev	= NULL;
	int count = 0;


	if(Dev == NULL) {
		return 0;
	}

	pGTemp = ifx_search_group_table(groupAddr, &count);
	if ( pGTemp == NULL )
	{
		return 0;
	}


	for( pMDev = pGTemp->pHead; pMDev != NULL ; pMDev  = pMDev->next )
	{
		if( pMDev->dev != NULL)
		{
			if( strcmp( pMDev->dev->name, Dev->name) == 0)
				return 1;
		}
	}	
	return 0;	

}

int ifx_send_leave_report (u32 groupAddr)
{
	struct sk_buff *skb	= NULL;
	struct iphdr *iph;
	struct igmphdr *ih;
	struct net_device  *dev	= ifx_find_device_by_name (upstreamIfName);
	int hh_len;
	__u32 devip = 0;

	if (debug) {
		printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
	}
	if (dev == NULL){
		return 0;
	}
	hh_len = (dev->hard_header_len + 15) & ~15;

	skb=alloc_skb(IGMP_SIZE + hh_len /*+15*/ , GFP_ATOMIC);
	if (skb == NULL) {
		return 0;
	}

	// skb->dst = &rt->u.dst; // ??
	skb_reserve(skb, hh_len);
			

	skb->nh.iph = iph = (struct iphdr *)skb_put(skb, sizeof(struct iphdr)+4);
	memset(iph, 0, sizeof(struct iphdr));

	iph->version  = 4;
	iph->ihl      = (sizeof(struct iphdr)+4)>>2;
	iph->tos      = 0;
	iph->frag_off = htons(IP_DF);
	iph->ttl      = 1;
	iph->daddr    = IGMP_ALL_ROUTER;
#ifdef IFX_IGMP_PROXY
	if(mode == IS_PROXY){
		devip = ifx_find_dev_ip_addr (dev);
		iph->saddr    = devip; 
	}
#endif
	iph->protocol = IPPROTO_IGMP;
	iph->tot_len  = htons(IGMP_SIZE);
	//iph->check = ip_fast_csum((unsigned char *) iph, iph->ihl);
	//ip_select_ident(iph, &rt->u.dst, NULL);
	((u8*)&iph[1])[0] = IPOPT_RA;
	((u8*)&iph[1])[1] = 4;
	((u8*)&iph[1])[2] = 0;
	((u8*)&iph[1])[3] = 0;
	ip_send_check(iph);

	ih = (struct igmphdr *)skb_put(skb, sizeof(struct igmphdr));
	memset(ih, 0, sizeof(struct igmphdr));

	ih->type=IGMP_HOST_LEAVE_MESSAGE;
	ih->code=0; 
	ih->csum=0;
	ih->group=ntohl(groupAddr);
	ih->csum=ip_compute_csum((void *)ih, sizeof(struct igmphdr));

	skb->dev = dev;
	skb->protocol = htons(ETH_P_IP);

	ifx_send_skb_to_upstream_device (skb);
	return 0;

}

void ifx_send_general_query_callback (int queryInterval)
{
	struct sk_buff *skb	= NULL;
	struct iphdr *iph;
	struct igmphdr *ih;
	struct net_device  *dev	= ifx_find_device_by_name (upstreamIfName);
	int hh_len;
	u32	dst = IGMP_ALL_HOSTS;
	__u32 devip = 0;

	if (debug) {
		printk ("[%s]:[%d] queryInterval:%d\n", __FUNCTION__, __LINE__, queryInterval);
	}
	if (dev == NULL){
		return;
	}
	hh_len = (dev->hard_header_len + 15) & ~15;

	skb=alloc_skb(IGMP_SIZE + hh_len /*+15*/ , GFP_ATOMIC);
	if (skb == NULL) {
		return;
	}

	skb_reserve(skb, hh_len);
			

	skb->nh.iph = iph = (struct iphdr *)skb_put(skb, sizeof(struct iphdr)+4);
	memset(iph, 0, sizeof(struct iphdr));

	iph->version  = 4;
	iph->ihl      = (sizeof(struct iphdr)+4)>>2;
	iph->tos      = 0;
	iph->frag_off = htons(IP_DF);
	iph->ttl      = 1;
	iph->daddr    = dst;
#ifdef IFX_IGMP_PROXY
	if(mode == IS_PROXY){
		devip = ifx_find_dev_ip_addr (dev);
		iph->saddr    = devip; 
	}
#endif
	iph->protocol = IPPROTO_IGMP;
	iph->tot_len  = htons(IGMP_SIZE);
	//iph->check = ip_fast_csum((unsigned char *) iph, iph->ihl);
	//ip_select_ident(iph, &rt->u.dst, NULL);
	((u8*)&iph[1])[0] = IPOPT_RA;
	((u8*)&iph[1])[1] = 4;
	((u8*)&iph[1])[2] = 0;
	((u8*)&iph[1])[3] = 0;
	ip_send_check(iph);

	ih = (struct igmphdr *)skb_put(skb, sizeof(struct igmphdr));
	memset(ih, 0, sizeof(struct igmphdr));

	ih->type=IGMP_HOST_MEMBERSHIP_QUERY;
	ih->code=100; // max. resp. time = 10 secs
	ih->csum=0;
	ih->group=0;
	ih->csum=ip_compute_csum((void *)ih, sizeof(struct igmphdr));

	skb->dev = dev;
	skb->protocol = htons(ETH_P_IP);

	ifx_refresh_report_flag();
	ifx_send_skb_to_every_downstream_device (skb);

	init_timer (&queryTimer);
	queryTimer.expires  	= jiffies + queryInterval;
	queryTimer.data   	= (unsigned long) queryInterval;
	queryTimer.function	= (void *) &ifx_send_general_query_callback;
	add_timer (&queryTimer);
	
#ifdef CONFIG_ADM6996_SUPPORT_MODULE
	/* 801111: Santosh - add a entry into switch membership table for UPnP group */
	if (hw_snoop) {
		adm_process_add_upnp_group ();
	}
#endif
	return;
}

void ifx_router_timer_callback
	(
		unsigned long data
	)
{
	__u32 routerAddr = (__u32 )data;

	ifx_delete_router_table (routerAddr);

}

int ifx_mcast_router_table_add 
	(
	 	__u32 routerAddr,
		struct net_device *dev
		
	)
{

	struct 	McastRouterTable 	*pGTemp;

	for( pGTemp = pRouterTableHead; pGTemp; pGTemp = pGTemp->next ){
		if( pGTemp->RouterAddr == routerAddr ){
			return 0;
		}
	}
	
	pGTemp 	= (struct McastRouterTable *)	kmalloc( sizeof(struct McastRouterTable), GFP_ATOMIC /* GFP_KERNEL */ ); /* Use GFP_ATOMIC instead */

	if(pGTemp == NULL){
		if (debug) {
			printk("[%s]:[%d] - kmalloc failed for pGTemp!\n", __FUNCTION__, __LINE__);
		}
		return 0;
	}

	
	pGTemp->RouterAddr	= routerAddr;
	pGTemp->dev			= dev;
	pGTemp->next		= NULL;
	pGTemp->previous	= pRouterTableTail;

	if( pRouterTableTail == NULL){
		pRouterTableHead = pGTemp;
		pRouterTableTail = pGTemp;
	}
	else{
		pRouterTableTail->next		= pGTemp;
		pRouterTableTail			= pGTemp;
	}

	init_timer (&pGTemp->routerTimer);

	pGTemp->routerTimer.expires  	= jiffies + IGMP_ROUTER_INTERVAL;
	pGTemp->routerTimer.data   		= (unsigned long) routerAddr;
	pGTemp->routerTimer.function	= (void *) &ifx_router_timer_callback;

	add_timer (&pGTemp->routerTimer);
	
	return 1;
	
}

int ifx_mcast_router_table_search (struct net_device *Dev)
{
	struct McastRouterTable *pGTemp;

    if (Dev == NULL)
       return 0;

	for( pGTemp = pRouterTableHead; pGTemp != NULL ; pGTemp = pGTemp->next )
	{
		if (pGTemp->dev != NULL)
		{
			if( strcmp(pGTemp->dev->name, Dev->name) == 0)
				return 1;
		}
    }
	return 0;
}

int ifx_router_device_event (struct net_device *dev)
{
	struct McastRouterTable *pGTemp;

	if (dev == NULL)
		return 0;

	for( pGTemp = pRouterTableHead; pGTemp != NULL ; pGTemp = pGTemp->next )
	{
		if (pGTemp->dev == NULL)
			continue;

		if( strcmp( pGTemp->dev->name, dev->name) == 0)
		{
			pGTemp->dev = NULL;
			return 1;
		}
	}
	return 0;	
}


void ifx_print_mcast_router_table (void)
{
	struct McastRouterTable *pGTemp;
	__u32 routerAddr;

	for( pGTemp = pRouterTableHead; pGTemp != NULL ; pGTemp = pGTemp->next ){
		routerAddr = pGTemp->RouterAddr; 	
		if (debug) {
			printk("\n IFX_PRINT: ifx_print_McastRouterTable: RouterAddr = %u.%u.%u.%u ", IFX_PRINT_ADDR23(routerAddr));
		
			if (pGTemp->dev == NULL){
				printk ("IFX_PRINT: ifx_print_McastRouterTable: Device = NULL\n");
			}
			else {
				printk("\n IFX_PRINT: ifx_print_McastRouterTable: Device     = %s", pGTemp->dev->name );
			}
		}
	}

}

int ifx_count_mcast_router_table (void)
{
	
	struct McastRouterTable *pGTemp;
	int count = 0;

	for( pGTemp = pRouterTableHead; pGTemp != NULL ; pGTemp = pGTemp->next )
	{
		count++;

	}
	return count;
	
}

int ifx_send_skb_to_mcast_router 
	(
	 	struct sk_buff *skb
	)
{
	struct sk_buff *clonedSkb = NULL;
	struct McastRouterTable *pGTemp;
	struct net_device *dev;
	int count = 0;

	//ifx_print_mcast_router_table ();

	count = ifx_count_mcast_router_table();	

	if( count == 0 )
		goto DROP;

	for( pGTemp = pRouterTableHead; pGTemp != NULL ; pGTemp = pGTemp->next )
	{
		if (pGTemp->dev == NULL)
			continue;

		dev	= pGTemp->dev;

		if(!strcmp(dev->name, "lo")) 
			continue;			
		
		if(!strcmp(skb->dev->name,dev->name))
			continue;

		if(ifx_is_dev_bridge (dev))
			continue;

		else
		{

			clonedSkb = skb_clone (skb, GFP_ATOMIC);
			if(clonedSkb == NULL){
				if (debug) {
					printk("[%s]:[%d] - clonedSkb Failed \n", __FUNCTION__, __LINE__);
				}
				goto DROP;
			}
			
			ifx_send_skb_by_dev(clonedSkb, dev);
			
		}
			
	}

	return IFX_XMIT;

DROP:
	if (debug) {
		printk("[%s]:[%d] - Packet dropped \n", __FUNCTION__, __LINE__);
	}
	kfree_skb(skb);
	return IFX_ERROR;
	
}

void ifx_refresh_report_flag (void)
{
	struct 	McastGroupTable *pGTemp;
	
	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next ){
		pGTemp->reportFlag = FALSE;
	}
	
}

__u32 ifx_find_dev_ip_addr 
	(
	 	struct net_device *dev
	 )
{
	struct in_device *ip;
	struct in_ifaddr *in;
	__u32 addr;

    if (dev == NULL)
       return 0;

	ip = dev->ip_ptr;

	if( (ip == NULL) || ((in = ip->ifa_list) == NULL ))
	{
		if (debug) {
			printk ("[%s]:[%d] - Device not assigned IP address\n", __FUNCTION__, __LINE__);
		}
		return 0;
	}

    if(strncmp(dev->name, "ppp", 3) == 0)
        addr = in->ifa_local;
    else 
	addr = in->ifa_address;

	return addr;
}



void ifx_group_timer_cleanup (void)
{
	struct 	McastGroupTable 	*pGTemp, *pNTemp;
	struct 	McastDev		*pMDev		= NULL;
	struct sk_buff *skb = NULL;
	struct DeleteInfo *pDInfo = NULL; 
	int ret = 0;

	/** Disable timer interrupt - ensure no race between cleanup of timer and interrupt */
	if (timer_pending(&queryTimer))
	{
		ret = del_timer(&queryTimer);
	}
	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pNTemp )
	{
		pNTemp = pGTemp->next;
		for( pMDev = pGTemp->pHead; pMDev != NULL ; pMDev  = pMDev->next )
		{
			if(pMDev->timerFlag == TRUE)
			{
				if( timer_pending (&pMDev->igmpTimer))
				{
					skb = (struct sk_buff *) (pMDev->igmpTimer.data);
					if (debug) {
						printk ("[%s]:[%d] - deleting igmp timers\n", __FUNCTION__, __LINE__);
					}
					ret = del_timer ( &pMDev->igmpTimer );
					if (ret) 
					{ /* Timer was deleted from list and not fired */
						if (skb != NULL)
						{
							if (debug) {
								printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
							}
							kfree_skb(skb);
						}
					}

				}
			}
		}	
	}

	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next )
	{
		if( timer_pending (&pGTemp->memberTimer))
		{
			pDInfo = (struct DeleteInfo *) (pGTemp->memberTimer.data);
			ret = del_timer ( &pGTemp->memberTimer );
			if (ret && (pDInfo != NULL))
			{
				if (debug) {
					printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
				}
				kfree (pDInfo);				
			}
		}
	}	
}

int ifx_delete_member_timer(struct McastGroupTable *pGTemp) 
{
	struct DeleteInfo *pDInfo = NULL; 
	int ret = 0;

	if (pGTemp == NULL) {
		return -1;
	}
	if( timer_pending (&pGTemp->memberTimer))
	{
		pDInfo = (struct DeleteInfo *) (pGTemp->memberTimer.data);
		ret = del_timer ( &pGTemp->memberTimer );
		if (ret && (pDInfo != NULL))
		{
			if (debug) {
				printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
			}
			kfree (pDInfo);				
		}
	}
	return ret;
}

int ifx_delete_router_table
	(
	 	__u32 routerAddr
	)
{
	struct 	McastRouterTable 	*pRTemp;
	
	for ( pRTemp = pRouterTableHead; pRTemp != NULL; pRTemp = pRTemp->next )
	{
		if( pRTemp->RouterAddr == routerAddr )
		{
			if( (pRTemp == pRouterTableHead) && (pRTemp == pRouterTableTail) )
			{
				kfree (pRTemp);
				pRouterTableHead = NULL;
				pRouterTableTail = NULL;
			}
			else if (pRTemp == pRouterTableHead)
			{
				pRouterTableHead = pRTemp->next;
				pRouterTableHead->previous = NULL;
				kfree (pRTemp);
			}
			else if (pRTemp == pRouterTableTail)
			{
				pRouterTableTail = pRTemp->previous;
				pRouterTableTail->next = NULL;
				kfree (pRTemp);
			}
			else
			{
				pRTemp->previous->next = pRTemp->next;
				pRTemp->next->previous = pRTemp->previous;
				kfree (pRTemp);
			}
			return 1;

		}
	}
	
	return 0;
	
}


void ifx_change_src_ip(struct sk_buff **skb)
{
	struct iphdr *iph ;
		
	if (debug) {
		printk("[%s]:[%d] - changing ip->saddr to 0.0.0.0 \n", __FUNCTION__, __LINE__);
	}
        iph = (struct iphdr *)((*skb)->data);
	iph->saddr = SRC_IPADDR;

	ip_send_check(iph);
}

int ifx_check_report_flag
	(
		__u32	groupAddr
	)
{
	struct 	McastGroupTable 	*pGTemp;

	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next ){
		if( (pGTemp->GroupAddr == groupAddr))
		{ 
			if((pGTemp->reportFlag == TRUE))
			{
				if (debug) {
					printk("[%s]:[%d] - igmp report already sent \n", __FUNCTION__, __LINE__);
				}
				return 0;
			}
			else
			{

				pGTemp->reportFlag = TRUE;
				if (debug) {
					printk("[%s]:[%d] - igmp report to be sent \n", __FUNCTION__, __LINE__);
				}
				return 1;
			}
		}

	}
	return 0;

}


void ifx_membership_timer_expire_callback (unsigned long data)
{
	struct DeleteInfo *pDInfo = (struct DeleteInfo *)data;
	struct 	McastGroupTable 	*pGTemp;
	struct 	McastDev		*pMDev		= NULL;
	struct sk_buff *skb = NULL;
	__u32	groupAddr;
	int ret = 0;

	 groupAddr = pDInfo->groupAddr;


	if (debug) {
		printk ("[%s]:[%d] Before delete \n", __FUNCTION__, __LINE__);
		ifx_print_McastGroupTable();
	}
	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next )
	{
		if( (pGTemp->GroupAddr == groupAddr))
		{
			for( pMDev = pGTemp->pHead; pMDev != NULL ; pMDev  = pMDev->next )
			{
				if(pMDev->timerFlag == TRUE)
				{
					if( timer_pending (&pMDev->igmpTimer))
					{
						if (debug) {
							printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
						}
						skb = (struct sk_buff *) (pMDev->igmpTimer.data);
						ret = del_timer ( &pMDev->igmpTimer );
						if (ret) 
						{ /* Timer was deleted from list and not fired */
							if (skb != NULL)
							{
								kfree_skb(skb);
							}
						}
					}
				}
			}
		}
	
	}
#if 0
	if (pGTemp == NULL) {
		return;	
	}
#endif
	if (pDInfo->pNetDev == NULL)
		ifx_mcast_group_table_delete (ntohl(pDInfo->groupAddr), NULL , NULL);
	else
	{
		ifx_mcast_group_table_delete (ntohl(pDInfo->groupAddr), pDInfo->pNetDev->name ,pDInfo->pNetDev);
	}
	if (debug) {
		printk ("[%s]:[%d] After delete \n", __FUNCTION__, __LINE__);
		ifx_print_McastGroupTable();
	}
	ifx_send_leave_report (groupAddr);
    /* TODO: free pDInfo ?? */
}


void ifx_igmp_timer_expire_callback (unsigned long data)
{
	struct sk_buff  *skb		= (struct sk_buff *)data;
	struct	iphdr	*pIpH		= (struct iphdr *)(skb->data);
	struct	igmphdr	*pIgmpH;
	struct net_device *dev		= NULL;
	struct net_device *Dev		= NULL;
	struct 	McastGroupTable 	*pGTemp;
	struct 	McastDev		*pMDev 		= NULL;	
	__u32	groupAddr, srcIp;
	int n, count = 0;
	
	if ((skb == NULL) || (skb->dev == NULL)) {
		return;
	}
	dev = skb->dev;


	n = (int)pIpH->ihl;
	n *= 4;
	pIgmpH = (struct igmphdr *) (skb->data + n );

	groupAddr = ntohl(pIgmpH->group);

	if (debug) {
		printk ("[%s]:[%d] Before delete \n", __FUNCTION__, __LINE__);
		ifx_print_McastGroupTable();
	}

	ifx_mcast_group_table_delete( groupAddr, dev->name, dev);

	if (debug) {
		printk ("[%s]:[%d] After delete \n", __FUNCTION__, __LINE__);
		ifx_print_McastGroupTable();
	}

	pGTemp = ifx_search_group_table(groupAddr, &count);
	if ( pGTemp == NULL ) /* None in the group table */
	{
		/* Modify the src IP address before sending out */
		Dev = ifx_find_device_by_name (upstreamIfName);
		srcIp = ifx_find_dev_ip_addr (Dev);
		if (srcIp == 0)
		{
			ifx_change_src_ip(&skb);
		}
		else
		{
			pIpH->saddr = htonl(srcIp);
			ip_send_check(pIpH);
		}
		ifx_send_skb_to_upstream_device(skb);
		return;
	}
			
	for( pMDev = pGTemp->pHead; pMDev; pMDev = pMDev->next )	
	{
		if ((count == 1) && (pMDev->dev != NULL)) {
			/* TODO: exclude device properly */
			if((!strcmp(pMDev->dev->name, "lo")) ||
			   (!strcmp(pMDev->dev->name, "MEI_PHY")) ||
			   (!strncmp(pMDev->dev->name, "br", 2)))

			{
			   /* Modify the src IP address before sending out */
				Dev = ifx_find_device_by_name (upstreamIfName);
				srcIp = ifx_find_dev_ip_addr (Dev);
				if (srcIp == 0)
				{
					ifx_change_src_ip(&skb);
				}
				else
				{
					pIpH->saddr = htonl(srcIp);
					ip_send_check(pIpH);
				}
				if (debug) {
					printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
				}
				ifx_send_skb_to_upstream_device(skb);
			}
			else {
				if (debug) {
					printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
				}
				kfree_skb (skb);
				return;
			}
		}
	}

#ifdef IFX_SNOOPING
	if(mode == IS_SNOOPING)
	{
		ifx_send_skb_to_mcast_router(skb);
	}
#endif
	
	return;
}

int ifx_send_query_to_group_device (struct sk_buff *skb )
{
	struct		ethhdr		*pEthH;
	struct	iphdr	*pIpH		=	(struct iphdr *)(skb->data);
	struct	igmphdr	*pIgmpH = NULL;
	struct net_device *dev = NULL;
	__u32 devip, groupaddr;
	int n = 0,i = 0;

	
	n = (int)pIpH->ihl;
	n *= 4;
	pIgmpH = (struct igmphdr *) (skb->data + n );
	
	groupaddr = ntohl(pIgmpH->group);

#ifdef IFX_IGMP_PROXY
	if(mode == IS_PROXY){

		dev = ifx_find_device_by_name (upstreamIfName);
		if (dev == NULL)
		{
			pIpH->saddr    = SRC_IPADDR; // src = 0.0.0.0
		}
		else
		{
			devip = ifx_find_dev_ip_addr (dev);
			pIpH->saddr    = devip; 
		}
	}
#endif

#ifdef IFX_SNOOPING
	if(mode == IS_SNOOPING)
	{
			pIpH->saddr    = SRC_IPADDR; // src = 0.0.0.0
	}
#endif


	pIpH->ttl      = 1;
	pIpH->daddr    = groupaddr;
	ip_send_check(pIpH);
	n = (int)pIpH->ihl;
	n *= 4;
 	pIgmpH = (struct igmphdr *) (skb->data + n );


	pIgmpH->type	= IGMP_HOST_MEMBERSHIP_QUERY;
 	pIgmpH->code	= gLatency; // RFC 2236, Group-specific query, Max Resp Time = 1 sec
	pIgmpH->csum	= 0;
	pIgmpH->group	= groupaddr;
	pIgmpH->csum	= ip_compute_csum((void *)pIgmpH, sizeof(struct igmphdr));


#ifdef IFX_IGMP_PROXY
	if(mode == IS_PROXY){
		pEthH		= (struct ethhdr *)(skb->data - 14);
		mappingIpToEthMcast(groupaddr, pEthH->h_dest);

		if (dev) {
			for(i=0; i<6; i++){
				pEthH->h_source[i] = dev->dev_addr[i];
			}
		}

		ifx_send_skb_to_group_device(skb, groupaddr);
	}
#endif

#ifdef IFX_SNOOPING
	if(mode == IS_SNOOPING)
	{
		ifx_send_skb_to_group_device(skb, groupaddr);
	}
#endif

	return IFX_SENT_SKB;

}

struct net_device *ifx_find_device_by_name
	(
		char *ifName
	)
{
	struct net_device *dev;
	
	for(dev = dev_base; dev; dev = dev->next){
		/* Debugging Printing */
		
		if( !strcmp(dev->name, ifName) ) /* Found */
			return dev;
	}
	return NULL;
}



int ifx_send_skb_to_every_downstream_device
	(
		struct sk_buff *skb
	)
{
	int count;
	struct net_device *dev = NULL;
	struct sk_buff *clonedSkb;
		
	count = ifx_count_downstream_dev();
	if ( count == 0 )
		goto DROP;
		
	
	for(dev = dev_base; dev; dev = dev->next){

		if( !ifx_is_downstream_dev(dev->name) ){ /* Not downstream dev */
			continue;
		}
		
		/* TODO: Find proper way to exclude all non-packet path interfaces ? */
		if(!strcmp(dev->name,"MEI_PHY"))
			continue;
		if(!strncmp(dev->name,"sit", 3))
			continue;

		if(count > 1){
			clonedSkb = skb_clone (skb, GFP_ATOMIC);
			if(clonedSkb == NULL){
				if (debug) {
					printk ("[%s]:[%d] - failed to cloneSkb !!\n", __FUNCTION__, __LINE__);
				}
				goto DROP;
			}
			ifx_send_skb_by_dev(clonedSkb, dev);
		}
		else if (count == 1)
			ifx_send_skb_by_dev(skb, dev);
		else
			return 1;
		count--;
	}

	return 1;

DROP:
	if (debug) {
		printk ("[%s]:[%d] - Packet dropped \n", __FUNCTION__, __LINE__);
	}
	kfree_skb(skb);
	return 0;
}


int ifx_process_mcast_routing_in_proxy
	(
	 	struct sk_buff    *skb,
		struct net_device *Dev,
		__u32 groupAddr
	)
{
	int 	count				= 0;
	struct 	net_device 		*dev 	= NULL;
	struct 	McastGroupTable *pGTable	= NULL;
	struct 	McastDev		*pMDev	= NULL;	
	
	if(!strcmp(Dev->name, upstreamIfName))
	{
		// if(ifx_is_dev_bridge(Dev))
		return 1;
		
	}
	else
	{
		pGTable = ifx_search_group_table(groupAddr, &count);
	   	if ( pGTable == NULL )
		{
			return 0;
		}
			
		for( pMDev = pGTable->pHead; pMDev; pMDev = pMDev->next )	
		{
			if (pMDev->dev == NULL)
				return 0;
				
		    dev = pMDev->dev;
		    if(!strcmp(Dev->name, dev->name))
			{
				return 1;	    
			}

		}
	}
	return 0;
		
}



int ifx_send_skb_to_upstream_device
	(
		struct sk_buff *skb
	)
{
	struct sk_buff *copySkb = NULL;

	if (debug) {
		printk ("[%s]:[%d]\n", __FUNCTION__, __LINE__); 
	}
	if (hw_snoop)
	{
		if (skb->dev && !strcmp(skb->dev->name, switchIfName))
		{
			goto lbl_skip_switch_send;
		}
		copySkb = skb_copy (skb, GFP_ATOMIC);
		if(copySkb == NULL){
			if (debug) {
				printk ("[%s]:[%d] - failed to copySkb !!\n", __FUNCTION__, __LINE__);
			}
			goto lbl_skip_switch_send;
		}
		ifx_send_skb_by_ifname(copySkb, switchIfName);
	}
lbl_skip_switch_send:
	ifx_send_skb_by_ifname(skb, upstreamIfName);
	if (debug) {
		printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
	}
	return 0;
}

int ifx_count_downstream_dev
	(
		void
	)
{
	int i = 0;
	struct net_device *dev;
	
	for(dev = dev_base; dev; dev = dev->next){
		if( !ifx_is_downstream_dev(dev->name) )
			continue;

		/* TODO: find a proper way to exclude devices */
		if(!strcmp(dev->name,"MEI_PHY"))
			continue;
		
		if(!strcmp(dev->name, "br0"))
			continue;

		i++;
	}
	return i;
}

int ifx_send_skb_by_ifname
	(
		struct sk_buff *skb,
		char *ifName	
	)
{
	struct net_device *dev = NULL;
	
	dev = ifx_find_device_by_name(ifName);
	if(dev == NULL){
		if (debug) {
			printk ("[%s]:[%d] - Cannot find interface name!!\n", __FUNCTION__, __LINE__);
		}
		kfree_skb(skb);
		return 0;
	}
	
	if( ifx_send_skb_by_dev(skb, dev) == 1 )
		return 1;
	else
		return 0;
}

int ifx_setup_eth_hdr(
	struct sk_buff *skb,
	struct net_device *dev
	)
{
	int i=0;
	struct 	iphdr   	*pIpH;
	struct	ethhdr		*pEthH;

	/* 
   	 * In case of VLAN header, dev->hard_header_len is 18 (ETH header length + 4 bytes VLAN header ),
	 * whereas in normal cases (pure ETH interface) its 14 bytes. At this point we expect ETH hdr + IP packet + payload.   
	 */
	pEthH	= (struct ethhdr *)(skb->data - ETH_HLEN);
	pIpH	= (struct iphdr *)(skb->data);

	if (debug) { 
		printk ("[%s]:[%d] srcmac: %02X:%02X:%02X:%02X:%02X:%02X dstmac: %02X:%02X:%02X:%02X:%02X:%02X dev:%s\n",
			__FUNCTION__, __LINE__,pEthH->h_source[0],pEthH->h_source[1],pEthH->h_source[2],pEthH->h_source[3],
			pEthH->h_source[4],pEthH->h_source[5], pEthH->h_dest[0],pEthH->h_dest[1],
			pEthH->h_dest[2],pEthH->h_dest[3],pEthH->h_dest[4],pEthH->h_dest[5], dev->name);
	}
	pEthH->h_proto = __constant_htons(ETH_P_IP);
	skb->protocol = __constant_htons(ETH_P_IP);	
	mappingIpToEthMcast(pIpH->daddr, pEthH->h_dest);
	/* Modify the src MAC address before sending out*/
	if (dev){
		for(i=0; i<6; i++){
			pEthH->h_source[i] = dev->dev_addr[i];
		}
	}
	if (debug) { 
		printk ("[%s]:[%d] srcmac: %02X:%02X:%02X:%02X:%02X:%02X dstmac: %02X:%02X:%02X:%02X:%02X:%02X dev:%s\n",
			__FUNCTION__, __LINE__,pEthH->h_source[0],pEthH->h_source[1],pEthH->h_source[2],pEthH->h_source[3],
			pEthH->h_source[4],pEthH->h_source[5], pEthH->h_dest[0],pEthH->h_dest[1],
			pEthH->h_dest[2],pEthH->h_dest[3],pEthH->h_dest[4],pEthH->h_dest[5], dev->name);
	}
	return 0;
}

int ifx_send_skb_by_dev
	(
		struct sk_buff *skb,
		struct net_device *dev	
	)
{
	
	if (dev == NULL){
		goto DROP;
	}	

	/* TODO: handle wlan, usb & other possible interfaces */

	switch (dev->name[0]){
	case 'p': 

		skb->dev = dev;
		dev_queue_xmit(skb);
		break;
	case 'n': 
#ifdef IFX_IGMP_PROXY
		ifx_setup_eth_hdr(skb, dev);
#endif
		skb->dev = dev;
		/* 
   		 * In case of VLAN header, dev->hard_header_len is 18 (ETH header length + 4 bytes VLAN header ),
	 	 * whereas in normal cases (pure ETH interface) its 14 bytes. At this point we expect ETH hdr + IP packet + payload.   
		 */
		skb_push(skb, ETH_HLEN );
		dev_queue_xmit(skb);
		break;
	case 'e': 
				
#ifdef IFX_IGMP_PROXY
		if(mode == IS_PROXY)
		{
			ifx_setup_eth_hdr(skb, dev);
		}
	
#endif // IFX_IGMP_PROXY

		skb->dev = dev;
		/* 
   		 * In case of VLAN header, dev->hard_header_len is 18 (ETH header length + 4 bytes VLAN header ),
	 	 * whereas in normal cases (pure ETH interface) its 14 bytes. At this point we expect ETH hdr + IP packet + payload.   
		 */
		skb_push(skb, ETH_HLEN );
		dev_queue_xmit(skb);
		break;

	case 'u': // USB ports
#ifdef IFX_IGMP_PROXY
		if (mode == IS_PROXY)
		{
			ifx_setup_eth_hdr(skb, dev);
		}
#endif
		skb->dev = dev;
		skb_push(skb, ETH_HLEN );
		dev_queue_xmit(skb);
		break;

	case 's': // handle switch ports
#ifdef IFX_IGMP_PROXY
		if (mode == IS_PROXY)
		{
			ifx_setup_eth_hdr(skb, dev);
		}
#endif
		skb->dev = dev;
		skb_push(skb, dev->hard_header_len);
		dev_queue_xmit(skb);
		break;

	case 'r': 
#ifdef IFX_IGMP_PROXY
		ifx_setup_eth_hdr(skb, dev);
#endif
		skb->dev = dev;
		skb_push(skb, dev->hard_header_len);
		dev_queue_xmit(skb);
		break;

	default:
		goto DROP;
	}
	
	return 1;

DROP:
	if (debug) {
		printk ("[%s]:[%d] - packet dropped!!\n", __FUNCTION__, __LINE__);
	}
	kfree_skb(skb);
	return 0;
}


void mappingIpToEthMcast
	(
		__u32 ipvalue,
		unsigned char *pMac
	)
{
	struct	tempIpHdr{
		__u8	ip1;
		__u8	ip2;
		__u8	ip3;
		__u8	ip4;
	};
	__u32	tmpIp	= ntohl(ipvalue);
	
	struct tempIpHdr *pIpHdr	= (struct tempIpHdr *) &tmpIp;

	pMac[5]	= pIpHdr->ip4;
	pMac[4]	= pIpHdr->ip3;
	pMac[3]	= pIpHdr->ip2;
	pMac[3]	= pMac[3] & 0x7f;	
	pMac[2]	= 0x5e;
	pMac[1]	= 0x00;
	pMac[0]	= 0x01;
}

struct McastGroupTable *ifx_search_group_table
	(
		__u32 	groupAddr,
		int 	*count	
	)
{
	struct 	McastGroupTable *pGTemp;
	
	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next ){
		if( pGTemp->GroupAddr == groupAddr ){ /* Found */
			*count = pGTemp->count;
			return pGTemp;		
		}
	}
	return NULL;
}

int ifx_process_group_specific_query
	(
	 	__u32 groupAddr
	)
{
	int 	count				= 0;
	struct 	net_device 		*dev 	= NULL;
	struct 	McastGroupTable *pGTable	= NULL;
	struct 	McastDev		*pMDev	= NULL;	
	
	
	pGTable = ifx_search_group_table(groupAddr, &count);
	if ( count == 0 || pGTable == NULL )
	{
		return IFX_ERROR;
	}
			
	for( pMDev = pGTable->pHead; pMDev; pMDev = pMDev->next )
	{
		if (pMDev->dev == NULL)
			return 0;
	    
		dev = pMDev->dev;

	    if(dev == NULL)
			return IFX_ERROR;

	    if(ifx_is_dev_bridge(dev))
			return IFX_XMIT;
	}

	return IFX_ERROR;
    
}

int ifx_send_skb_to_group_device
	(
		struct 	sk_buff 	*skb,
		__u32 	groupAddr
	)
{
	int 	count						= 0;
	struct 	net_device 		*dev 		= NULL;
	struct 	sk_buff 		*clonedSkb	= NULL;
	struct 	McastGroupTable *pGTable	= NULL;
	struct 	McastDev		*pMDev 		= NULL;	
	
	if (debug) {
		printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
	}
	pGTable = ifx_search_group_table(groupAddr, &count);
	if ( count == 0 || pGTable == NULL ){
		goto DROP;
	}
			
	for( pMDev = pGTable->pHead; pMDev; pMDev = pMDev->next ){
		if( (dev = pMDev->dev) == NULL ){
			goto DROP;
		}
		
#ifdef IFX_IGMP_PROXY
		if(mode == IS_PROXY)
		{
			if( !ifx_is_downstream_dev(dev->name) ){ /* Not downstream dev */
				continue;
			}
		}
#endif	


#ifdef IFX_SNOOPING
		if(mode == IS_SNOOPING)
		{
			if(!strcmp(dev->name, "lo")) 
			{
				continue;			
			}
		}
#endif

		if(count > 1){
			clonedSkb = skb_clone (skb, GFP_ATOMIC);
			if(clonedSkb == NULL){
				if (debug) {
					printk ("[%s]:[%d] - failed to cloneSkb !!\n", __FUNCTION__, __LINE__);
				}
				goto DROP;
			}
			ifx_send_skb_by_dev(clonedSkb, dev);
		}
		else if (count == 1)
		{
			ifx_send_skb_by_dev(skb, dev);
		}
		else
			return 1;
		count--;
	}
	return 1;

DROP:
	if (debug) {
		printk ("[%s]:[%d] - packet dropped!!\n", __FUNCTION__, __LINE__);
	}
	kfree_skb(skb);
	return 0;
}

int ifx_send_igmpv2_report
	(
		struct sk_buff *skb
	)
{
	return 1;	
}

int ifx_mcast_group_table_delete
	(
		__u32 		groupAddr,
		char		*ifName, 
		struct net_device	*dev
	)
{
	struct 	McastGroupTable *pGTemp		= NULL;
	struct 	McastDev		*pMDev	= NULL;
	int flag = 0;
	
	ifx_process_mfcache (groupAddr);

	if (dev == NULL || ifName == NULL )
	{
		flag = 1;
	}

	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next ){
		if( pGTemp->GroupAddr == groupAddr ){
			for( pMDev = pGTemp->pHead; pMDev; pMDev = pMDev->next ){

				if (!flag)
				{
					if (pMDev->dev) {
						if( (!strcmp( pMDev->dev->name, ifName )) ) {
							flag = 1;
						}
					}
				}
				
				if (flag)
				{ /* Match */
					if( (pMDev == pGTemp->pHead) && (pMDev == pGTemp->pTail) ){ /* Only one entity */
						kfree(pMDev);
						if( (pGTemp == pGroupTableHead) && (pGTemp == pGroupTableTail) ){
							ifx_delete_member_timer(pGTemp);
							kfree(pGTemp);
							pGroupTableHead = NULL;
							pGroupTableTail = NULL;
						}
						else if( pGTemp == pGroupTableHead ){
							pGroupTableHead = pGTemp->next;
							pGroupTableHead->previous = NULL;
							ifx_delete_member_timer(pGTemp);
							kfree(pGTemp);
						}
						else if( pGTemp == pGroupTableTail ){
							pGroupTableTail = pGTemp->previous;
							pGroupTableTail->next = NULL;
							ifx_delete_member_timer(pGTemp);
							kfree(pGTemp);
						}
						else{
							pGTemp->previous->next = pGTemp->next;
							pGTemp->next->previous = pGTemp->previous;
							ifx_delete_member_timer(pGTemp);
							kfree(pGTemp);
						}
						/* printk("ifx_mcast_group_table_delete()==>The following IP was deleted from the table\n");
        					ifx_print_IpDot(groupAddr); */
						return 1;
					}
					else if( pMDev == pGTemp->pHead ){ /* The first entity */
						pGTemp->pHead = pMDev->next;
						pGTemp->pHead->previous = NULL;
						pGTemp->count--;
						kfree(pMDev);
					}
					else if( pMDev == pGTemp->pTail ){ /* The last entity */
						pGTemp->pTail = pMDev->previous;
						pGTemp->pTail->next = NULL;
						pGTemp->count--;
						kfree(pMDev);
					}
					else{
						pMDev->previous->next = pMDev->next;
						pMDev->next->previous = pMDev->previous;
						pGTemp->count--;
						kfree(pMDev);
					}
					return 1;
				}
			}
		}
	}
	return 0;
}

int ifx_group_device_event (struct net_device *dev)
{
	struct 	McastGroupTable *pGTemp;
	struct 	McastDev		*pMDev;
	
	if (dev == NULL) {
		return 0;	
	}

	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next )
	{
		for( pMDev = pGTemp->pHead; pMDev; pMDev = pMDev->next )
		{
			if (pMDev->dev == NULL)
				continue;

			if ( strcmp (pMDev->dev->name, dev->name) == 0 )
			{
				pMDev->dev = NULL;
				return 1;
			}

		}
	}
	return 0;
}


int ifx_mcast_group_table_add
	(
		__u32 		groupAddr,
		char		*ifName, 
		struct net_device	*dev
	)
{
	struct 	McastGroupTable 	*pGTemp;
	struct 	McastDev		*pMDev;
	struct  McastOriginTable *pMOrigin;
	struct DeleteInfo		*pDInfo;
	struct sk_buff *skb = NULL;
	unsigned long timeout, timespent;
	int match = 0, ret = 0;

	
#if 0
	if (dev == NULL) {
		return 0;	
	}
#endif
	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next ){
		if( pGTemp->GroupAddr == groupAddr ){
				
				/* membership timeout handling */
				if( timer_pending (&pGTemp->memberTimer))
				{
					if (debug) {
 						printk ("modified member timer [%s]:[%d]\n", __FUNCTION__, __LINE__);
					}
					mod_timer(&pGTemp->memberTimer,(jiffies + IGMP_GROUP_MEMBERSHIP_INTERVAL));
				}
#if 0
				if (dev == NULL)
					return 0;
#endif
			for( pMDev = pGTemp->pHead; pMDev; pMDev = pMDev->next ){
				if (pMDev->dev == NULL)
				{
					pMDev->dev = dev;
					pMDev->time = jiffies;
					
					/*: add mfc for every origin under this group */
			
					for (pMOrigin = pGTemp->pOriginHead; pMOrigin; pMOrigin = pMOrigin->next)
					{
						ifx_add_mfcctl (pMOrigin->origin, groupAddr, pMOrigin->vifIndex );
					}
					return 1;
				}
				if( !strcmp( pMDev->dev->name, ifName ) ) /* Match */
				{
					pMDev->time = jiffies;
					match = 1;

					if( pMDev->timerFlag == TRUE)
					{
						pGTemp->reportFlag		= FALSE;
						if( timer_pending (&pMDev->igmpTimer))
						{
							if (debug) {
 								printk ("deleting leave timer [%s]:[%d]\n", __FUNCTION__, __LINE__);
							}
							skb = (struct sk_buff *) (pMDev->igmpTimer.data);
							ret = del_timer ( &pMDev->igmpTimer );
							if (ret) 
							{ /* Timer was deleted from list and not fired */
								if (skb != NULL)
								{
									kfree_skb(skb);
								}
							}
							pMDev->timerFlag		= FALSE;
						}
					}
				}
				else
				{
					timeout = jiffies + IGMP_GROUP_MEMBERSHIP_INTERVAL;
					timespent = jiffies - pMDev->time;

					if (timespent  > timeout)
					{
						if (dev != NULL)
						{
							ifx_mcast_group_table_delete (groupAddr, pMDev->dev->name, pMDev->dev);
						}
						else {
							ifx_mcast_group_table_delete (groupAddr, NULL, NULL);
						}
					}
				}
			}

			if (match == 1)
				return 0;

			pMDev = (struct McastDev *)kmalloc( sizeof(struct McastDev), GFP_ATOMIC /* GFP_KERNEL */ ); /* Use GFP_ATOMIC instead */
			if(pMDev == NULL){
				printk ("[%s]:[%d] - failed to kmalloc !!\n", __FUNCTION__, __LINE__);
				return 0;
			}
			pMDev->dev 			= dev;
			pMDev->time			= jiffies;
			pMDev->timerFlag		= FALSE;
			pMDev->next			= NULL;
			pMDev->previous		= pGTemp->pTail;
			pGTemp->pTail->next = pMDev;
			pGTemp->pTail		= pMDev;
			pGTemp->count++;

			/* add mfc for every origin under this group */
			
			for (pMOrigin = pGTemp->pOriginHead; pMOrigin; pMOrigin = pMOrigin->next)
			{
				ifx_add_mfcctl (pMOrigin->origin, groupAddr, pMOrigin->vifIndex );
			}
				
			return 1;
		}
	}
	
	pGTemp 	= (struct McastGroupTable *)	kmalloc( sizeof(struct McastGroupTable), GFP_ATOMIC /* GFP_KERNEL */ ); /* Use GFP_ATOMIC instead */
	if(pGTemp == NULL){
		printk ("[%s]:[%d] - failed to kmalloc !!\n", __FUNCTION__, __LINE__);
		return 0;
	}
	pMDev 	= (struct McastDev *)kmalloc( sizeof(struct McastDev), GFP_ATOMIC /* GFP_KERNEL */ ); /* Use GFP_ATOMIC instead */
	if(pMDev == NULL){
		kfree(pGTemp);
		printk ("[%s]:[%d] - failed to kmalloc !!\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	pGTemp->pOriginHead		= NULL;
	pGTemp->pOriginTail		= NULL;

	pGTemp->pMacHead	= NULL;
	pGTemp->pMacTail	= NULL;
	

	pMDev->dev			= dev;
	pMDev->time			= jiffies;  
	pMDev->timerFlag		= FALSE;
		
	pMDev->next			= NULL;
	pMDev->previous			= NULL;
	pGTemp->GroupAddr		= groupAddr;

	pGTemp->reportFlag		= FALSE;

	pGTemp->count			= 1;
	pGTemp->queryCount		= 0;
	pGTemp->queryGroupCount	= 0;
	pGTemp->pHead			= pMDev;
	pGTemp->pTail			= pMDev;
	pGTemp->next			= NULL;
	pGTemp->previous		= pGroupTableTail;
	if( pGroupTableTail == NULL){ /* pGroupTableHead should be NULL as well!!*/
		pGroupTableHead = pGTemp;
		pGroupTableTail = pGTemp;
	}
	else{
		pGroupTableTail->next		= pGTemp;
		pGroupTableTail			= pGTemp;
	}

	pDInfo 	= (struct DeleteInfo *)kmalloc( sizeof(struct DeleteInfo), GFP_ATOMIC /* GFP_KERNEL */ ); /* Use GFP_ATOMIC instead */
	if(pDInfo == NULL){
		printk ("[%s]:[%d] - failed to kmalloc !!\n", __FUNCTION__, __LINE__);
		return 0;
	}

	pDInfo->groupAddr 	= groupAddr;
	if (dev != NULL)
		pDInfo->pNetDev		= dev;
	else	
		pDInfo->pNetDev		= NULL;


	init_timer (&pGTemp->memberTimer);

	pGTemp->memberTimer.expires  = jiffies + IGMP_GROUP_MEMBERSHIP_INTERVAL;
	pGTemp->memberTimer.data	 = (unsigned long)pDInfo;
	pGTemp->memberTimer.function = (void *) &ifx_membership_timer_expire_callback;

	add_timer(&pGTemp->memberTimer);

	return 1;
}


int ifx_process_kernel_cache_query 
	(
	 	__u32 origin,
		__u32 group,
		struct net_device *dev
	 )
{
	struct 	McastGroupTable *pGTemp;
	struct viftable *v;
	int count = 0;
	int incomingVif = -1;
	int vifi = 0;

	if (dev == NULL) {
		return 0;	
	}

	for(vifi = 0, v = gviftable; vifi < gNumVifs; vifi++, v++)
	{
		if(strcmp (v->vifname, dev->name) == 0)
		{
			// incomingVif = vifi; 
			// ifx_get_vif_index (dev, &incomingVif);
			ifx_search_vif_table (dev, &incomingVif);
			break;
		}
	}

	pGTemp = ifx_search_group_table (group, &count);
	if (pGTemp == NULL)
	{
		ifx_mcast_group_table_add (group, NULL, NULL);
	}

	if ((incomingVif == -1) || (incomingVif > gNumVifs))
	{
		return 0;
	}
	
	ifx_add_origin_to_group_table (origin, group, incomingVif);

	ifx_add_mfcctl (origin, group, incomingVif);
	
	return 1;
}


int ifx_add_origin_to_group_table
	(
	 	__u32 origin,
		__u32 group,
		int vifindex
	)
{
	struct 	McastGroupTable  *pGTemp;
	struct  McastOriginTable *pMOrigin;

	
	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next ){
		if( pGTemp->GroupAddr == group){

			for (pMOrigin = pGTemp->pOriginHead; pMOrigin; pMOrigin = pMOrigin->next){
				if (pMOrigin->origin == origin){
					return 0;
				}
			}

			pMOrigin = (struct McastOriginTable *) kmalloc (sizeof(struct McastOriginTable), GFP_ATOMIC);
			if (pMOrigin == NULL){
				printk ("[%s]:[%d] - failed to kmalloc !!\n", __FUNCTION__, __LINE__);
				return 0;
			}

			if ((pGTemp->pOriginHead == NULL) && (pGTemp->pOriginTail == NULL))
			{
				pMOrigin->origin 	= origin;
				pMOrigin->vifIndex  = vifindex;
				pMOrigin->next		= NULL;
				pMOrigin->previous  = NULL;

				pGTemp->pOriginHead		= pMOrigin;
				pGTemp->pOriginTail		= pMOrigin;
				return 1;
			}
			else
			{
				pMOrigin->origin 	= origin;
				pMOrigin->vifIndex  = vifindex;

				pMOrigin->next		= NULL;
				pMOrigin->previous	= pGTemp->pOriginTail;
				pGTemp->pOriginTail->next = pMOrigin;
				pGTemp->pOriginTail		= pMOrigin;
				return 1;
			}
		}
	}

	return 0;
}



int ifx_get_vif_index (struct net_device *dev, int *vifid)
{
	int index = 0;
	int ret = VIF_ACTIVE;


	if (dev == NULL)
		return -1;
	if(strcmp (dev->name, "lo") == 0 ||
	    strncmp (dev->name, "MEI_", 4) == 0)
	      return -1;
	
	for (index = 0 ; index < MAXVIFS ; index++) {
		if (gviftable[index].flag == VIF_EMPTY)
			break;
		if (!strcmp(gviftable[index].vifname, dev->name)) {
			if (gviftable[index].flag == VIF_INACTIVE) {
				gNumVifs++;
				ret = RET_INACTIVE;
				*vifid = index;
				return (ret);
			} 
		}
	}
	
	if (index < MAXVIFS) {
			gNumVifs++;
			*vifid = index;
			ret = RET_NEW;
	} else {
		ret = -1;
	}
	return (ret);
}

int ifx_vif_delete (int vifi)
{
	struct viftable *v;
	
	v = &gviftable [vifi];
	v->flag		 = VIF_INACTIVE;
	v->vifdev	 = NULL;
	// gNumVifs--;

	return 1;
}

int ifx_vif_device_event (struct net_device *dev)
{
	struct viftable *v;
	int ct = 0;

	v=&gviftable[0];

	for(ct=0;ct<MAXVIFS;ct++,v++) {
		if (v->vifdev == dev)
			ifx_vif_delete(ct);
	}
	return 1;
}

int ifx_search_vif_table (struct net_device *dev, int *vifi)
{
	struct viftable *v;
	int index = 0;
	__u32 addr;
		
	addr = ifx_find_dev_ip_addr (dev);
	if (addr == 0)
		return NO_DEV;

	for (index = 0 , v=gviftable; index <= gNumVifs ; index++, v++) 
	{
		if ( strcmp(v->vifname, dev->name) == 0 )
		{
		   if	(v->vifdev != NULL)
		   {
			   *vifi = v->vif.vifc_vifi;
			   if (v->vif.vifc_lcl_addr.s_addr == addr)
				  return ADDR_MATCH;	
			   else
			      return ADDR_NO_MATCH;
		   }
		}
	}
	return NO_DEV;
}

int ifx_vif_add (struct net_device *dev)
{
	struct viftable *v;
	struct vifctl vif;
	
	struct in_device 	*indev;
	struct in_ifaddr    *inf;
	__u32 addr;
	int index = 0;
	int vifi = 0;
	int ret = 0;
	

	if (dev == NULL)
		return 0;

	if(strcmp (dev->name, "lo") == 0 ||
	   	strncmp (dev->name, "MEI_", 4) == 0)
	    return 0;
	
	if (dev->ip_ptr == NULL)
	{
		//printk ("ifx_vif_add: dev->ptr is nULL for dev: %s\n",dev->name);
		return 0;
	}

	indev = (struct in_device *) dev->ip_ptr;
	inf   = indev->ifa_list;
		
	if( (indev == NULL) || (inf == NULL ))
	{
		//printk ("ifx_vif_add: dev has no ip addr\n");
		return 0;
	}

	ret = ifx_search_vif_table (dev, &vifi);
	if (ret == ADDR_MATCH)
	{
		return 0;
	}
	else if (ret == ADDR_NO_MATCH)
	{

		addr = ifx_find_dev_ip_addr (dev);
		if (addr == 0)
			return 0;
		v = &gviftable [vifi];
		v->vif.vifc_vifi  = vifi;
		v->vif.vifc_flags = 0;
		v->vif.vifc_threshold = 0;
		v->vif.vifc_rate_limit  = DEFAULT_PHY_RATE_LIMIT; 
		v->vif.vifc_lcl_addr.s_addr = addr;
		v->vif.vifc_rmt_addr.s_addr = htonl (0x00000000L); 
   		v->flag		 = VIF_ACTIVE;
		v->vifdev		 = dev;
		// strncpy (v->vifname, dev->name, IFNAMSIZ);
		strncpy (v->vifname, inf->ifa_label, IFNAMSIZ);
			
		v->vif.vifc_threshold = 1;

		memset (&vif, 0x0, sizeof (vif));

		vif.vifc_vifi			= vifi;
		vif.vifc_flags			= 0;
		vif.vifc_threshold 		= v->vif.vifc_threshold;
		vif.vifc_rate_limit     = DEFAULT_PHY_RATE_LIMIT;
		vif.vifc_lcl_addr.s_addr	= addr;
		vif.vifc_rmt_addr.s_addr	= htonl (0x00000000L);
			
		ret = ifx_add_vif (&vif, 0);
		return 1;

	}

	ret = ifx_get_vif_index (dev, &index);

	
	if (ret == -1)
		return 0;
	if (ret == RET_ACTIVE)
		return 0;
	

	
	
	if (ret == RET_INACTIVE)
	{
		addr = inf->ifa_address;
		if (addr == 0)
			return 0;
		v = &gviftable [index];
    	v->flag		 = VIF_ACTIVE;
		v->vifdev		 = dev;
		strncpy (v->vifname, inf->ifa_label, IFNAMSIZ);
		return 1;
	}

	if (ret == RET_NEW)
	{
		addr = ifx_find_dev_ip_addr (dev);
		if (addr == 0)
			return 0;
		v = &gviftable [index];
		v->vif.vifc_vifi  = index;
		v->vif.vifc_flags = 0;
		v->vif.vifc_threshold = 0;
		v->vif.vifc_rate_limit  = DEFAULT_PHY_RATE_LIMIT; 
		v->vif.vifc_lcl_addr.s_addr = addr;
		v->vif.vifc_rmt_addr.s_addr = htonl (0x00000000L); 
   		v->flag		 = VIF_ACTIVE;
		v->vifdev		 = dev;
		// strncpy (v->vifname, dev->name, IFNAMSIZ);
		strncpy (v->vifname, inf->ifa_label, IFNAMSIZ);
			
		v->vif.vifc_threshold = 1;

		memset (&vif, 0x0, sizeof (vif));

		vif.vifc_vifi			= index;
		vif.vifc_flags			= 0;
		vif.vifc_threshold 		= v->vif.vifc_threshold;
		vif.vifc_rate_limit     = DEFAULT_PHY_RATE_LIMIT;
		vif.vifc_lcl_addr.s_addr	= addr;
		vif.vifc_rmt_addr.s_addr	= htonl (0x00000000L);
			
		ret = ifx_add_vif (&vif, 0);
		return 1;
	}
	return 0;
}

int ifx_init_vif ()
{
	struct net_device *dev;

	memset (&gviftable, 0x0, sizeof (gviftable));
	
	for (dev = dev_base; dev ; dev = dev->next)
	{
		if(strcmp (dev->name, "lo") == 0 ||
	    	strncmp (dev->name, "MEI_", 4) == 0)
	      continue;

		ifx_vif_add (dev);
	}

	return 1;
}

int ifx_del_vif_in_kernel ()
{
	int vifcount = gNumVifs;

	while (vifcount != -1)
	{
		// ifx_del_vif (vifcount);
		vifcount--;
		gNumVifs--;
	}
	return 1;
}

struct net_device* ifx_get_netdevice (int vifi)
{
	struct viftable *v;
	struct net_device *dev;
	int index = 0;

	v = &gviftable[0];

	for (index = 0 ; index <= gNumVifs ; index++, v++) {

		if ((v->vif.vifc_vifi == vifi) && (v->vifdev != NULL))
		{
			dev = v->vifdev;
		   return dev;	
		}
	}
	return NULL;

}

int ifx_get_vif_ttl (int vifi)
{
	struct viftable *v;
	int index = 0;
	int ttl = 0;

	v = &gviftable[0];

	for (index = 0 ; index <= gNumVifs ; index++, v++) 
	{
		if (v->vif.vifc_vifi == vifi) 
		{
			ttl = v->vif.vifc_threshold;
			return ttl;
		}
	}
	return 0;
}

int ifx_get_outgoing_vifs 
	(
	 	__u32 group,
		int vifi
	)
{
	struct 	McastGroupTable 	*pGTemp		= NULL;
	struct 	McastDev		*pMDev;
	struct McastRouterTable *pRTemp;
	struct net_device *UpstreamDev;
	struct net_device *vifDev;
	int count = 0;
	int ttl = 0;

	vifDev = ifx_get_netdevice (vifi);
	
	
	/*  router ports should get mcast data anyways */ 
#ifdef IFX_IGMP_PROXY
	if (mode == IS_PROXY)
	{
		if (vifDev)
		{
			UpstreamDev = ifx_find_device_by_name (upstreamIfName);
			if (UpstreamDev) {
				if ( strcmp ( vifDev->name, UpstreamDev->name) == 0)
				{
					ttl = ifx_get_vif_ttl (vifi);
					if (debug) {
						printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
					}
					return ttl;
				}
			}
		}
	}
#endif

#ifdef IFX_SNOOPING
	if (mode == IS_SNOOPING)
	{
		for( pRTemp = pRouterTableHead; pRTemp != NULL ; pRTemp = pRTemp->next )
		{
			if (vifDev)
			{
				if ( strcmp ( vifDev->name, pRTemp->dev->name) == 0)
				{
					ttl = ifx_get_vif_ttl (vifi);
					return ttl;
				}
			}
		}
	}
#endif

	pGTemp = ifx_search_group_table (group, &count);
	if (pGTemp == NULL || count == 0)
	{
		if (debug) {
			printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
		}
		return 0;
	}

	for( pMDev = pGTemp->pHead; pMDev; pMDev = pMDev->next )	
	{
		if (pMDev->dev == NULL)
			continue;


		if (vifDev)
		{
			if ( strncmp (vifDev->name, "br",2) == 0)
			{
				ttl = ifx_get_vif_ttl (vifi);
				if (debug) {
					printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
				}
				return ttl;
			}

			if ( strcmp ( vifDev->name, pMDev->dev->name) == 0)
			{
				ttl = ifx_get_vif_ttl (vifi);
				if (debug) {
					printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
				}
				return ttl;
			}
		}


	}
	return 0;
}


int ifx_process_mfcache 
	(
	 	__u32 group
	 )
{
	struct 	McastGroupTable  *pGTemp;
	struct  McastOriginTable *pMOrigin;
	__u32 origin;
	int vifi = 0;

	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next )
	{
		if( pGTemp->GroupAddr == group)
		{

			for (pMOrigin = pGTemp->pOriginHead; pMOrigin; pMOrigin = pMOrigin->next)
			{
				origin = pMOrigin->origin;
				vifi   = pMOrigin->vifIndex;
				ifx_del_mfcctl (origin, group, vifi);
			}
		}
	}
	return 1;

}

int ifx_del_mfcctl 
	(
	 	__u32 origin,
		__u32 group,
		int vifi
	 )
{
	struct mfcctl 	mfc;
	int i = 0;

	memset(&mfc, 0x0, sizeof(mfc));
	mfc.mfcc_origin.s_addr		= origin;
	mfc.mfcc_mcastgrp.s_addr 	= group;
	mfc.mfcc_parent				= vifi;

	for(i = 0; i < gNumVifs; i++)
	{
		mfc.mfcc_ttls[i] = 0;
	}
	
	// ret = ifx_mfc_add (&mfc, 0);
	ifx_mfc_del (&mfc);
	if (debug) {
		printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
	}
	return 1;
}


int ifx_add_mfcctl 
	(
	 	__u32 origin, 
		__u32 group,
		int vifi
		
	)
{
	struct mfcctl 	mfc;
	int i = 0;

	memset(&mfc, 0x0, sizeof(mfc));
	mfc.mfcc_origin.s_addr		= origin;
	mfc.mfcc_mcastgrp.s_addr 	= group;
	mfc.mfcc_parent				= vifi;

	for(i = 0; i <= gNumVifs; i++)
	{
		if(i == vifi)
			mfc.mfcc_ttls[i] = 0;
		else
		{
			mfc.mfcc_ttls[i] = ifx_get_outgoing_vifs (group, i);
		}
	}

	ifx_mfc_add (&mfc, 0);	

	return 1;	
}

int ifx_process_igmp
	(
		struct sk_buff *skb
	)
{
	struct sk_buff *Copyskb		= NULL;
	struct 	McastGroupTable 	*pGTemp		= NULL;
	struct 	McastDev		*pMDev		= NULL;
	struct net_device *dev      = NULL;
	__u32   groupAddr;
	__u32   origin;
	__u32   group;
	__u32	srcIp;

	struct	iphdr	*pIpH		=	(struct iphdr *)(skb->data);
	struct	igmphdr	*pIgmpH;
	int n;
	int count = 0, dev_match = 0;
	int time;
	int ret = 0;
	
	n = (int)pIpH->ihl;
	n *= 4;
	pIgmpH = (struct igmphdr *) (skb->data + n );
	
	groupAddr = ntohl(pIgmpH->group);

	if (debug) {
		printk ("[%s]:[%d] \n", __FUNCTION__, __LINE__);
	}
	/* Version 1 and 2 Query (the same: 0x11) */
	if( pIgmpH->type == IGMP_HOST_MEMBERSHIP_QUERY ){ /* 0x11*/

#ifdef IFX_IGMP_PROXY
	if(mode == IS_PROXY) {
		if ( ifx_is_downstream_dev(skb->dev->name) ){ /* If not upstream interface, drop it */
			if (debug) {
				printk("ifx_process_igmp: (Query) Not from upstream dev!\n");
			}
			goto DROP;
		}
	}
#endif
	// printk("\nIFX_DEBUG - ifx_process_igmp - received Query from: %u.%u.%u.%u",IFX_PRINT_ADDR23(pIpH->saddr));
		if( pIgmpH->group == 0 ){ /* General Query */
			if( pIpH->daddr == IGMP_ALL_HOSTS ){
				ifx_update_general_query_count();

				/* refresh report flag, even in case of General query */
				ifx_refresh_report_flag();
				
#ifdef IFX_IGMP_PROXY
	if(mode == IS_PROXY) {
				/* TODO: check every downstream ports whether part of bridge / not */
				// ifx_send_skb_to_every_downstream_device(skb);
	
				ret = ifx_check_for_non_bridge_ports(skb);		

				if(ret == IFX_XMIT)
					return IFX_XMIT;
				else 
					return IFX_ERROR;
			
		}
#endif

#ifdef IFX_SNOOPING
			if(mode == IS_SNOOPING){
				if(pIpH->saddr != SRC_IPADDR)
				{				
					printk ("[%s]:[%d] Add router to list\n", __FUNCTION__, __LINE__);
					ifx_mcast_router_table_add(ntohl(pIpH->saddr), skb->dev );
				 }

				ret = ifx_check_for_non_bridge_ports(skb);

				if(ret == IFX_XMIT)
					return IFX_XMIT;
				else 
					return IFX_ERROR;
			}
#endif
				
			}
			else{
				//printk("DebugPrinting == ifx_process_igmp: (Query) group is 0 but not all_host Mcast Packet!\n");
				goto DROP;
			}
		}
		else{ /* Group Specific Query */
			if( IN_MULTICAST( ntohl(pIgmpH->group) ) ){
				if( pIpH->daddr != pIgmpH->group ){
					//printk("DebugPrinting == ifx_process_igmp: (Query) IP Dest is not equal to IGMP Group!\n");
					goto DROP;					
				}
				if (debug) {
					printk ("Got GroupSpecific query - [%s]:[%d] \n", __FUNCTION__, __LINE__);
				}

#ifdef IFX_SNOOPING
			if(mode == IS_SNOOPING){
				if(pIpH->saddr != SRC_IPADDR)
				{
					// printk ("IFX_DEBUG: ifx_process_igmp - Add router to List\n");
					ifx_mcast_router_table_add(ntohl(pIpH->saddr), skb->dev);
				}
			}
#endif
				
				ifx_update_group_specific_query_count( ntohl(pIgmpH->group), IFX_INCREASE_COUNT );

				pGTemp = ifx_search_group_table(ntohl(pIgmpH->group), &count);
			
				if( pGTemp == NULL )
				{
					if (debug) {
						printk ("[%s]:[%d] Rcvd GS-query, but entry not found !!\n", __FUNCTION__, __LINE__);
					}
					goto DROP;
				}

				pGTemp->reportFlag		= FALSE;
				
			    // ifx_send_skb_to_group_device(skb, ntohl(pIgmpH->group) );
				ret = ifx_send_skb_to_non_bridge_group_device(skb, ntohl(pIgmpH->group));
				
				if(ret == IFX_ERROR)
					return IFX_ERROR;
				else if(ret == IFX_XMIT)
				 	return IFX_XMIT;
				else
					return IFX_SENT_SKB;

			}
			else{ /* Group addr is not mcast packet, drop it */
				if (debug) {
					printk ("[%s]:[%d] GroupAddr is not multicast !!\n", __FUNCTION__, __LINE__);
				}
				goto DROP;
			}
		}
	}
	/* Version 1 and 2 Report, the Join report is also included per page 13 of RFC1112 */
	else if( (pIgmpH->type == IGMP_HOST_MEMBERSHIP_REPORT) || (pIgmpH->type == IGMPV2_HOST_MEMBERSHIP_REPORT) ){ /* 0x12 or 0x16 */

#ifdef IFX_IGMP_PROXY

		if(mode == IS_PROXY) {
			if ( !ifx_is_downstream_dev(skb->dev->name) ){ /* If not downstream interface, drop it */
				if (debug) {
					printk ("[%s]:[%d] (Report) Not from downstream dev!\n", __FUNCTION__, __LINE__);
				}
				goto DROP;
			}
		}
#endif
		if( IN_MULTICAST( ntohl(pIgmpH->group) ) ){
			/* Removed, fc: Maybe we don't need to check it?
			if( pIgmpH->group != pIpH->daddr ){
				printk("ifx_process_igmp: (Report) groupAddr is not equal to daddr!\n");
				goto DROP;				
			}*/
			if (debug) {
				printk ("[%s]:[%d] - adding group: %u.%u.%u.%u with dev: %s\n", __FUNCTION__, __LINE__, IFX_PRINT_ADDR23(pIgmpH->group),skb->dev->name);
			}

			ifx_mcast_group_table_add( ntohl(pIgmpH->group), skb->dev->name, skb->dev );

			origin = ntohl(pIpH->saddr);
			group  = ntohl(pIgmpH->group);


			if( ifx_check_report_flag( ntohl(pIgmpH->group)))
			{

				/* update vif with group list */
				
				// ifx_update_vif_group_table (origin, group );

				
#ifdef IFX_IGMP_PROXY
				
		   if(mode == IS_PROXY) {
		            /* Modify the src IP address before sending out */
		            dev = ifx_find_device_by_name (upstreamIfName);
                    srcIp = ifx_find_dev_ip_addr (dev);
                    if (srcIp == 0)
                    {
				   		ifx_change_src_ip(&skb);
                    }
                    else
                    {
		            	pIpH->saddr = htonl(srcIp);
		                ip_send_check(pIpH);
                    }

					ifx_send_skb_to_upstream_device (skb); 
					return IFX_SENT_SKB; 
				}
#endif

#ifdef IFX_SNOOPING
				if(mode == IS_SNOOPING)
				{
					ret = ifx_send_skb_to_mcast_router (skb);

					if(ret == IFX_ERROR)
						return IFX_ERROR;
					else if(ret == IFX_XMIT)
						return IFX_XMIT;
					else
						return IFX_SENT_SKB;
				
				}
#endif

			}
			else
			{
				return IFX_XMIT;
			}
			

		}
		else{ /* Group addr is not mcast packet, drop it */
			if (debug) {
				printk ("[%s]:[%d] GroupAddr is not multicast !!\n", __FUNCTION__, __LINE__);
			}
			goto DROP;
		}
	}
	/* Leave, Version 2 only */
	else if( pIgmpH->type == IGMP_HOST_LEAVE_MESSAGE ){ /* 0x17 */

#ifdef IFX_IGMP_PROXY
		if( mode == IS_PROXY){

			if ( !ifx_is_downstream_dev(skb->dev->name) ){ /* If not downstream interface, drop it */
				if (debug) {
					printk ("[%s]:[%d] - (Leave) Not from downstream dev!\n", __FUNCTION__, __LINE__);
				}
				goto DROP;
			}
		}
#endif
		if( IN_MULTICAST( ntohl(pIgmpH->group) ) ){
			/* Removed, fc: Maybe we don't need to check it?
			if( pIgmpH->group != pIpH->daddr ){
				printk("ifx_process_igmp: (Leave) groupAddr is not equal to daddr!\n");
				goto DROP;				
			}*/
			if (debug) {
				printk ("[%s]:[%d] - Leave for Group: %u.%u.%u.%u, from dev: %s\n",__FUNCTION__, __LINE__,                                                                               IFX_PRINT_ADDR23(pIgmpH->group), skb->dev->name);
			}

			/*  Send GS-query with Max. Resp. Time = 10 secs */
		
				pGTemp = ifx_search_group_table(ntohl(pIgmpH->group), &count);
			
				if( pGTemp == NULL )
				{
					if (debug) {
						printk ("[%s]:[%d] (Leave) - entry not found!\n", __FUNCTION__, __LINE__);
					}
					goto DROP;
				}
#ifdef IFX_SNOOPING
				if(mode == IS_SNOOPING)
				{
					/*  Fast leave processing - if dev is the only device part of group, send leave to upstream */
					if (ifx_is_dev_only_interface_with_group (groupAddr))
					{
						goto FAST_LEAVE;
					}
				}
#endif
				/* start leave timer for that interface */
				for( pMDev = pGTemp->pHead; pMDev != NULL ; pMDev  = pMDev->next )
				{
					if( pMDev->dev != NULL)
					{
						/* let switch device be handled by HW snooping */
						if (hw_snoop && ( !strncmp(skb->dev->name, "eth0", 4))) {
							dev_match = 0;
						}
						else if( !strcmp( pMDev->dev->name, skb->dev->name)) {
							dev_match = 1;
							break;	
						}
					}
				}	
				if (dev_match) {
					if(pMDev->timerFlag == TRUE)
					{
						goto DROP;
					}
					Copyskb = skb_copy(skb, GFP_ATOMIC);
					if(Copyskb == NULL)
					{
						printk ("[%s]:[%d] - failed to copySkb\n", __FUNCTION__, __LINE__);
						goto DROP;
					}	
					if (debug) {
						printk ("[%s]:[%d] - (Leave) - Init timer!\n", __FUNCTION__, __LINE__);
					}

					init_timer (&pMDev->igmpTimer);
					
					time = IFX_MAX_RESP_TIME;
					pMDev->igmpTimer.expires  = jiffies + (time * HZ);
					pMDev->igmpTimer.data		= (unsigned long)skb;
					pMDev->igmpTimer.function = (void *) &ifx_igmp_timer_expire_callback;
					
					pMDev->timerFlag			= TRUE; 

					ifx_send_query_to_group_device (Copyskb);
					
					add_timer (&pMDev->igmpTimer);
					return IFX_SENT_SKB;
				}
				else {
					goto DROP;
				}
		}
		else{ /* Group addr is not mcast packet, drop it */
			if (debug) {
				printk ("[%s]:[%d] GroupAddr is not multicast !!\n", __FUNCTION__, __LINE__);
			}
			goto DROP;
		}
	}

#ifdef IFX_SNOOPING
FAST_LEAVE:
	ifx_igmp_timer_expire_callback (skb);
	return 0;
#endif
	
DROP:
	if (debug) {
		printk ("[%s]:[%d] - packet dropped !!\n", __FUNCTION__, __LINE__);
	}
	kfree_skb(skb);
	return 0;	
}		

int ifx_send_skb_to_non_bridge_group_device
	(
	 	struct sk_buff *skb,
		__u32 groupAddr
	)
{
	int 	count						= 0;
	struct 	net_device 		*dev 		= NULL;
	struct 	sk_buff 		*clonedSkb	= NULL;
	struct 	McastGroupTable *pGTable	= NULL;
	struct 	McastDev		*pMDev 		= NULL;	
	
	pGTable = ifx_search_group_table(groupAddr, &count);
	if ( count == 0 || pGTable == NULL )
	{
			goto DROP;
	}
			
	for( pMDev = pGTable->pHead; pMDev; pMDev = pMDev->next )	
	{
		if (pMDev->dev == NULL)
			return 0;
		
		dev = pMDev->dev;

		/* TODO: exclude device properly */
		if(!strcmp(dev->name, "lo"))
			continue;
		if(!strcmp(dev->name, "MEI_PHY"))
			continue;
		if(!strcmp(dev->name, "br0"))
			continue;

		if(!strcmp(dev->name, skb->dev->name))
			continue;

		if(ifx_is_dev_bridge(dev))
			continue;
		else
		{
			clonedSkb = skb_clone (skb, GFP_ATOMIC);

			if(clonedSkb == NULL)
			{
				printk ("[%s]:[%d] - failed to cloneSkb\n", __FUNCTION__, __LINE__);
				goto DROP;
			}

			ifx_send_skb_by_dev(clonedSkb, dev);
		}

		
	}
	return IFX_XMIT;

DROP:
	kfree_skb(skb);
	if (debug) {
		printk ("[%s]:[%d] - packet dropped !!\n", __FUNCTION__, __LINE__);
	}
	return IFX_ERROR;
	
}
	

int ifx_check_for_non_bridge_ports
	(
	 	struct sk_buff *skb
	 )
{
	struct sk_buff *clonedSkb;
	struct net_device *dev;
	int bridgeCount 	= 0;
	int nonbridgeCount 	= 0;

	for(dev = dev_base; dev; dev = dev->next)
	{
		/* TODO: exclude device properly */
		if(!strcmp(dev->name, "lo"))
			continue;
		if(!strcmp(dev->name, "MEI_PHY"))
			continue;
		if(!strcmp(dev->name, skb->dev->name))
			continue;

		if(ifx_is_dev_bridge(dev))
			bridgeCount++;
		else
			nonbridgeCount++;
			
	}

	if(nonbridgeCount == 0)
		return IFX_XMIT;
	else
	{
		for(dev = dev_base; dev; dev = dev->next)
		{
			if(!strcmp(dev->name, "lo"))
				continue;
			if(!strcmp(dev->name, "MEI_PHY"))
				continue;
			
			if(!strcmp(dev->name, "br0"))
				continue;
			
			if(!strcmp(dev->name, skb->dev->name))
				continue;

			if(ifx_is_dev_bridge(dev))
				continue;
			
			clonedSkb = skb_clone(skb, GFP_ATOMIC);
				
			if(clonedSkb == NULL)
			{
				printk ("[%s]:[%d] - failed to cloneSkb\n", __FUNCTION__, __LINE__);
				goto DROP;
			}

			ifx_send_skb_by_dev(clonedSkb, dev);

		}

		return IFX_XMIT;
		
	}
		return IFX_XMIT;
	
DROP:
	kfree_skb(skb);
	if (debug) {
		printk ("[%s]:[%d] - packet dropped !!\n", __FUNCTION__, __LINE__);
	}
	return IFX_ERROR;

	
}

int ifx_process_mcast_routing
	(
		struct sk_buff *skb
	)
{
	struct	iphdr	*pIpH		=	(struct iphdr *)(skb->data);

	ifx_send_skb_to_group_device(skb, ntohl(pIpH->daddr) );
	return 1;	
}

int ifx_is_downstream_dev
	(
		char *ifName
	)
{
	if( !strcmp(ifName, upstreamIfName) || !strcmp(ifName, "lo") || !strncmp(ifName, "nas", 3)) { /* upstream dev or "lo" */
		return 0;
	}
	else /* Not upstream and not "lo" */
		return 1;
}

int ifx_convert_ipaddr_to_mcast_macaddr (__u32 ip, unsigned char *mac)
{
	int i;
	unsigned val;

	ip = (ip << 9 ) >> 9;
	for (i = 0; i < 3; i++) {
		val = ip & 0xFF;
		*(mac + (5-i)) = val;
		ip = ip >> 8;
	}
	*(mac + 2)=0x5E;
	*(mac + 1)=0x00;
	*(mac + 0)=0x01;

	return 1;
}

int ifx_is_dev_only_interface_with_group
	(
		__u32 groupAddr
	)
{
	
	struct 	McastGroupTable *pGTemp 	= NULL;
	struct 	McastDev		*pMDev	= NULL;
	int count = 0;

	pGTemp = ifx_search_group_table (groupAddr, &count);
	if (pGTemp == NULL)
	{
		return 0;
	}

	count = 0;

	for( pMDev = pGTemp->pHead; pMDev != NULL ; pMDev  = pMDev->next )
	{
		if(pMDev->dev == NULL)
			continue;
		else
			count++;
	}	
	if (count == 1)
	{
		return 1;	
	}
	else
	{
		return 0;	
	}

}

/* code for maintaining MAC table entries */

void ifx_print_ip_packet
	(
		struct sk_buff *skb
	)
{
	struct	iphdr	*pIpH		= (struct iphdr *)(skb->data);
	struct	ethhdr	*pEthH		= (struct ethhdr *)(skb->data - 14);
	long 	n;
	int 	i;

	/*	
	n = pIpH->tos; 			printk("ifx_print_ip_packet: tos = %ld\n", n);
	n = pIpH->tot_len; 		printk("ifx_print_ip_packet: tot_len = %ld\n", n);
	n = pIpH->id; 			printk("ifx_print_ip_packet: id = %ld\n", n);
	n = pIpH->frag_off;		printk("ifx_print_ip_packet: frag_off = %ld\n", n);
	n = pIpH->ttl; 			printk("ifx_print_ip_packet: ttl = %ld\n", n);
	n = pIpH->protocol;		printk("ifx_print_ip_packet: protocol = %ld\n", n);
	n = pIpH->check;		printk("ifx_print_ip_packet: check = %ld\n", n);
	*/

	n = ntohl(pIpH->saddr); 
	printk("Source IP");ifx_print_IpDot(n);
	n = ntohl(pIpH->daddr);	
	printk("Destination IP");ifx_print_IpDot(n);

	for( i=0; i<6; i++){
                n = pEthH->h_source[i];   printk("ifx_print_ip_packet: pEthH->h_source[%d] = %ld\n", i, n);
		n = pEthH->h_dest[i];	printk("ifx_print_ip_packet: pEthH->h_dest[%d] = %ld\n", i, n);
	}
}

void ifx_print_igmp_packet
	(
		struct sk_buff *skb
	)
{
	struct	iphdr	*pIpH		=	(struct iphdr *)(skb->data);
	struct	igmphdr	*pIgmpH;
	int 	nn;
	long 	n;
	
	nn = (int)pIpH->ihl;
	nn *= 4;
	pIgmpH = (struct igmphdr *) (skb->data + nn );
	
	n = pIgmpH->type; 			printk("ifx_print_igmp_packet: type = %ld\n", n);
	n = pIgmpH->code; 			printk("ifx_print_igmp_packet: code = %ld\n", n);
	n = pIgmpH->csum; 			printk("ifx_print_igmp_packet: csum = %ld\n", n);
	n = ntohl(pIgmpH->group);	printk("ifx_print_igmp_packet: group = %ld\n", n);
}

void ifx_update_general_query_count
	(
		void
	)
{
	struct 	McastGroupTable *pGTemp;
	
	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next )
		pGTemp->queryCount++;
}

int ifx_update_group_specific_query_count
	(
		__u32	groupAddr,
		int		IncOrDec
	)
{
	struct 	McastGroupTable *pGTemp;
	
	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next ){
		if( pGTemp->GroupAddr == groupAddr){
			if( IncOrDec == IFX_INCREASE_COUNT)
				pGTemp->queryGroupCount++;
			else{
				if( pGTemp->queryGroupCount == 0 )
					return 0;
				pGTemp->queryGroupCount--;			
			}
			break;
		}
	}
	return 1;
}

void ifx_print_McastGroupTable
	(
		void
	)
{
	struct 	McastGroupTable *pGTemp;
	struct 	McastDev		*pMDev;
	long 	n;
	//long	counter	=	0;	
	
	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next ){
		//counter++;
		n = ntohl(pGTemp->GroupAddr);
		printk("Group IP:");ifx_print_IpDot(n);
		//n = pGTemp->count; 		printk("ifx_print_McastGroupTable: count = %ld\n", n);
		//n = pGTemp->queryCount;	printk("ifx_print_McastGroupTable: queryCount = %ld\n", n);
		for( pMDev = pGTemp->pHead; pMDev; pMDev = pMDev->next )
			printk("Interface: %s\n", pMDev->dev->name );
	}
	//printk("ifx_print_McastGroupTable: Mcast group count in table = %ld\n", counter);
}

int ifx_skipped_protocols
	(
		struct sk_buff *skb
	)
{
	struct	iphdr	*pIpH		=	(struct iphdr *)(skb->data);
	
	/* Skip RIP2 multicast packets */
	/*
	3.6 Multicasting
	In order to reduce unnecessary load on those hosts which are not 
	listening to RIP-2 packets, an IP multicast address will be used for
	periodic broadcasts.  The IP multicast address is 224.0.0.9.  Note
	that IGMP is not needed since these are inter-router messages which
	are not forwarded.
	*/
	if( ntohl(pIpH->daddr) == 0xE0000009L /* 224.0.0.9 */ )
		return 1; /* Skip it */
	return 0;
}	

int ifx_print_IpDot
	(
		__u32 ipValue
	)
{
	struct  IpDotFormat {
                unsigned char a1;
                unsigned char a2;
                unsigned char a3;
                unsigned char a4;
        };
	struct IpDotFormat IdFmt;

	memcpy(&IdFmt, &ipValue, 4);
	printk("%d.%d.%d.%d\n", IdFmt.a1, IdFmt.a2, IdFmt.a3, IdFmt.a4);
	return 0;
}

int ifx_send_skb_to_other_ports 
	(
	 	struct sk_buff *skb
	)
{
	struct sk_buff 		*clonedSkb	= NULL;
	struct net_device	*dev;


	for(dev = dev_base; dev; dev = dev->next){

		if(dev == NULL)
		{
			goto DROP;
		}

		if(strcmp (dev->name, "lo") == 0 ||
		    strncmp (dev->name, "MEI_", 4) == 0)
			continue;			

		if(!strcmp(skb->dev->name,dev->name))
		{
			continue;
		}
		clonedSkb = skb_clone (skb, GFP_ATOMIC);
		
			if(clonedSkb == NULL){
				printk("ERROR == ifx_send_skb_to_other_ports: clonedSkb Failed \n");
				goto DROP;
			}

		ifx_send_skb_by_dev(clonedSkb, dev);

	}

	return 1;

	
DROP:
	if (debug) {
		printk ("***ERROR***: ifx_send_skb_to_other_ports - Packet dropped!!\n");
	}
	kfree_skb(skb);
	return 0;
}



