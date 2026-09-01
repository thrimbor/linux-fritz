#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
//#include <linux/config.h>
#include <linux/autoconf.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/igmp.h> 	/* struct igmphdr */

#include <linux/proc_fs.h>

#include <net/ip.h>
#include <net/checksum.h>

#include "mData.h"
#include "mUtil.h"

extern int (*mcast_hook_proxy)(struct sk_buff *);
extern int (*mcast_hook_proxy_fwd)(struct sk_buff *, struct net_device *);
extern int (*mcast_hook_mfc_cache) (struct sk_buff *);

extern int ifx_is_dev_bridge (struct net_device *dev);
extern int ifx_mrt_init (void);
extern int ifx_mrt_exit (void);


#ifdef CONFIG_ADM6996_SUPPORT_MODULE
extern int adm_process_protocol_filter_request (unsigned int cmd, PPROTOCOLFILTER uPROTOCOLFILTER);
extern int adm_enable_hw_snooping (int );
extern int adm_disable_hw_snooping ();
#endif // CONFIG_ADM6996_SUPPORT_MODULE

//static struct McastGroupTable *pGroupTableHead;
//static struct McastGroupTable *pGroupTableTail;
//static char upstreamIfName[IFNAMSIZ];
struct McastGroupTable *pGroupTableHead;
struct McastGroupTable *pGroupTableTail;

struct McastRouterTable *pRouterTableHead;
struct McastRouterTable *pRouterTableTail;

struct McastReportTable *pReportTableHead;
struct McastReportTable *pReportTableTail;

struct McastOriginTable *pOriginTableHead;
struct McastOriginTable *pOriginTableTail;

char upstreamIfName[IFNAMSIZ];
char switchIfName[IFNAMSIZ];
int  gLatency;

int 	mode = 1; /* 1= Proxy, 2 = snooping */
char	*uIf = "nas1";
char    *sIf; /* switch interface name */
int 	LeaveLatency = 10; /* Default = 1 sec*/
int 	hw_snoop = 0; /* 0 = disable hw snooping, 1 = enable hw snooping */
int     qint = IFX_IGMP_QUERY_INTERVAL;
int     debug = 0;

module_param( mode, int, S_IRUSR | S_IRGRP | S_IROTH );
MODULE_PARM_DESC(mode ,"Mode of Operation (1 = IGMP Proxy, 2 = IGMP Snooping)");
module_param( uIf, charp,  0000);
MODULE_PARM_DESC(uIf ,"upstream interface name");
module_param( hw_snoop, int, S_IRUSR | S_IRGRP | S_IROTH );
MODULE_PARM_DESC(hw_snoop ,"support for HW snooping");
module_param( LeaveLatency, int, S_IRUSR | S_IRGRP | S_IROTH  );
MODULE_PARM_DESC(LeaveLatency ,"Leavelatency period");
module_param( sIf, charp, 0000);
MODULE_PARM_DESC(sIf ,"switch interface name");
module_param( qint, int, S_IRUSR | S_IRGRP | S_IROTH );
MODULE_PARM_DESC(qint ,"query interval");
module_param( debug, int, S_IRUSR | S_IRGRP | S_IROTH );
MODULE_PARM_DESC(debug ,"debug option (0 = disable, 1 = enable)");



/*
return == 1, take this skb and process it
return == 0, let it go
*/
static int mcastProxy(struct sk_buff* skb)
{
	struct	iphdr	*pIpHeader	=	(struct iphdr *)(skb->data);

	__u32 saddr, group;
	int ret;

			
	if( pIpHeader && IN_MULTICAST( ntohl(pIpHeader->daddr) ) ){
		//printk("Multicasting Packet Got\n");
		//ifx_send_skb_to_interface(skb, "eth1");
		//ifx_send_skb_to_every_downstream_device(skb);		
		
		//ifx_print_ip_packet(skb);

		if( ifx_skipped_protocols(skb) == 1)
			return 0;
		
		if( pIpHeader->protocol == IPPROTO_IGMP ){
			// printk ("IFX_DEBUG - mcastProxy - Got an IGMP packet process it ! \n");	
			// ifx_print_igmp_packet(skb);

				ret = ifx_process_igmp(skb);
				if(ret == IFX_ERROR)
					return 1;
				else if (ret == IFX_XMIT)
					return 0;
				else
					return 1;
			
		}
		else
		{
			/* Data path */
			saddr = ntohl(pIpHeader->saddr);
			group = ntohl(pIpHeader->daddr);
			// printk("\n====================================================================\n");
			// ifx_print_McastGroupTable();
			// printk("\n====================================================================\n");

#ifdef IFX_SNOOPING	
			if(mode == IS_SNOOPING)
			{
				// printk("\n====================================================================\n");
				// ifx_print_mcast_router_table();
				// printk("\n====================================================================\n");
			}
#endif

#ifdef IFX_SNOOPING
			if(mode == IS_SNOOPING)
			{

				if(LOCAL_MCAST(ntohl(pIpHeader->daddr)))
				{
					/* Packet Inside 224.0.0.x range */
					ifx_send_skb_to_other_ports(skb);
					return 1;
				}
				else
					return 0;
			}
#endif


#ifdef IFX_IGMP_PROXY
			if(mode == IS_PROXY)
			{
				return 0;
			}
#endif


		}
		
	}
	
	/* Just let it go*/
	return 0;
}


static int mcastProxyFwd(struct sk_buff* skb, struct net_device *Dev)
{

	struct  iphdr   *iph	= skb->nh.iph;
	// struct  igmphdr   *igmph  = skb->h.igmph;
	struct  igmphdr   *igmph;
	__u32 groupAddr;
	int ret,n;
	
	groupAddr 	= ntohl(iph->daddr);
	
	if (! IN_MULTICAST(ntohl(iph->daddr)))
		return 1;

	if(iph && IN_MULTICAST(ntohl(iph->daddr)))
	{
		if( ifx_skipped_protocols(skb) == 1)
			return 1;
		if(iph->protocol == IPPROTO_IGMP)
		{
			// Neeraj: using same as in mUtil.c we are getting type as 0x46
			//igmph  = skb->h.igmph;
			n = (int)iph->ihl;
			n *= 4;
			igmph = (struct igmphdr *) (skb->data + n );
			//printk("type:%x, mode:%d\n",igmph->type,mode);
		    	if(igmph->type == IGMP_HOST_MEMBERSHIP_QUERY )
		    	{
				if(igmph->group == 0) /* General query */
			  	{
			   		if(iph->daddr == IGMP_ALL_HOSTS)
			   		{
#ifdef IFX_IGMP_PROXY
						if(mode == IS_PROXY)
						{
							if(!strcmp(Dev->name,upstreamIfName))
								return 0;
						}
#endif
			       			if(!strcmp(Dev->name,"lo"))
				   			return 0;
			       			if(!strcmp(Dev->name,"MEI_PHY"))
				   			return 0;
			       			if(!strcmp(Dev->name,skb->dev->name))
				   			return 0;
			       			else
				   			return 1;

					} 
				}
				else
				{
					if(IN_MULTICAST(ntohl(igmph->group)))
			    		{
						/* Group-specific query */
						ret = ifx_process_group_specific_query(ntohl(igmph->group));

						if(ret == IFX_ERROR)
				    			return IFX_ERROR;
						else
				    			return IFX_XMIT;
				
			    		}
				}
		   	}

			else if( (igmph->type == IGMP_HOST_MEMBERSHIP_REPORT) || 
			       (igmph->type == IGMPV2_HOST_MEMBERSHIP_REPORT) ||
					(igmph->type ==  IGMP_HOST_LEAVE_MESSAGE))
		    	{
			
#ifdef IFX_IGMP_PROXY
				if(mode == IS_PROXY)
				{
			    		if(!strcmp(Dev->name, upstreamIfName))
			    		{
						if(ifx_is_dev_bridge(Dev))
				    			return 1;
						else
				    			return 0;
			    		}
				}
#endif

#ifdef IFX_SNOOPING
				if(mode == IS_SNOOPING)
				{
			    		ret = ifx_mcast_router_table_search(Dev);

			    		if(ret == IFX_XMIT)
			    		{
						if(ifx_is_dev_bridge(Dev))
				    			return 1;
						else
				    			return 0;
			    		}
			    		else
						return 0;
				}
#endif
			
		  	}
			
			
		}

		else
		{

#ifdef IFX_SNOOPING
			if(mode == IS_SNOOPING)
			{	   
				if(LOCAL_MCAST(ntohl(iph->daddr)))	
				{
					if(!strcmp(Dev->name, "lo"))
						return 0;
					if(!strcmp(Dev->name, "MEI_PHY"))
						return 0;
					else
						return 1;					
				}
				else
				{
					/* process non-local multicast data, send only to those in Group table */
				
					if (ifx_check_dev_entry (Dev, groupAddr) == 1)
						return 1;
					else if(ifx_mcast_router_table_search (Dev) == 1)
					{
						/* add mac entry for router ports */
						/* ifx_add_mac_entry (Dev, groupAddr); */
						return 1;
					}
					else
						return 0;
				}
			}
#endif

#ifdef IFX_IGMP_PROXY
		   
			if(mode == IS_PROXY)
			{
				if(!strcmp(skb->dev->name, upstreamIfName))
				{
					if (ifx_check_dev_entry (Dev, groupAddr) == 1)
						return 1;
					else
						return 0; 
				}
				else
				{
					/* pkt from one of the downstream */
					/* send one copy to upstream & remaining to joined downstream ports */
					ret = ifx_process_mcast_routing_in_proxy(skb, Dev, groupAddr);
					if(ret)
						return 1;
					else
						return 0;
				
				}

			}
		
#endif
		}

	}

	return 1;

}

static int mcastCache (struct sk_buff *skb)
{
	struct iphdr *iph;
	struct net_device *dev;
	__u32 origin, group;

	dev = skb->dev;
	iph = skb->nh.iph;
	
	origin = iph->saddr;
	group  = iph->daddr;

	ifx_process_kernel_cache_query (origin, group, dev);
	return 1;
	
}


static int ifx_mcast_device_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = (struct net_device *)ptr;
	

	if (event == NETDEV_UNREGISTER)
	{
		ifx_group_device_event (dev);
#ifdef IFX_SNOOPING
		if (mode == IS_SNOOPING)
		{
			ifx_router_device_event (dev);
		}
#endif
		ifx_vif_device_event (dev);
		return NOTIFY_DONE;
	}
		return NOTIFY_DONE;
}

static int ifx_mcast_inetaddr_event (struct notifier_block *this, unsigned long event, void *ptr)
{
	struct in_ifaddr *ifa = (struct in_ifaddr*)ptr;
	struct in_device *in_dev = ifa->ifa_dev;
	struct net_device *dev = in_dev->dev;

	if (event == NETDEV_UP)
	{
		ifx_vif_add (dev);
		return NOTIFY_DONE;
	}
	return NOTIFY_DONE;
	
}

#if 0
#ifdef CONFIG_PROC_FS
int ifx_igmp_info (char *buffer, char **start, off_t offset, int length)
{
	struct 	McastGroupTable *pGTemp = NULL;
	struct 	McastDev		*pMDev = NULL;
	struct McastMacInfo	*pMacTemp = NULL;
	struct net_device *dev = NULL;
	int len = 0, pos = 0, begin = 0;
	int size = 0;

	len += sprintf(buffer,
		 "Group		Mac	        Dev	  	Portmap\n");
	pos=len;
	for( pGTemp = pGroupTableHead; pGTemp; pGTemp = pGTemp->next ){
		for (pMacTemp = pGTemp->pMacHead, pMDev = pGTemp->pHead; pMacTemp; 
			pMacTemp = pMacTemp->next, pMDev = pMDev->next)	
		{
			if (pMDev == NULL)
				dev = NULL;
			else
				dev = pMDev->dev;

			size = sprintf(buffer+len, "%08lX   %02X:%02X:%02X:%02X:%02X:%02X   %s     %8ld",
				(unsigned long)(pGTemp->GroupAddr), 
				(unsigned char)(pMacTemp->mcastMac[0]),
				(unsigned char)(pMacTemp->mcastMac[1]),
				(unsigned char)(pMacTemp->mcastMac[2]),
				(unsigned char)(pMacTemp->mcastMac[3]),
				(unsigned char)(pMacTemp->mcastMac[4]),
				(unsigned char)(pMacTemp->mcastMac[5]),
				dev ? dev->name : " ", pMacTemp->portMap);
		}
			size += sprintf(buffer+len+size, "\n");
			len+=size;
			pos+=size;
			if(pos<offset)
			{
				len=0;
				begin=pos;
			}
			if(pos>offset+length)
				goto done;
	}
done:
  	*start=buffer+(offset-begin);
  	len-=(offset-begin);
  	if(len>length)
  		len=length;
	if (len < 0) {
		len = 0;
	}
  	return len;
}
#endif
#endif



static struct notifier_block ifx_mcast_device_notifier={
	ifx_mcast_device_event,
	NULL,
	0
};


struct notifier_block ifx_mcast_inetaddr_notifier = {
	ifx_mcast_inetaddr_event,
	NULL,
	0
};


#define UNUSED __attribute__((unused))

static int __init UNUSED iproxyd_init(void)
{
	int  queryInterval = 0;
#ifdef CONFIG_ADM6996_SUPPORT_MODULE
	int  pos = -1;
#endif
	struct net_device *WanDev;

#ifdef IFX_IGMP_PROXY			
	if(mode == IS_PROXY){
		strcpy( upstreamIfName, uIf);
#if !defined (IFX_ETH_AS_WAN)
		if( (upstreamIfName[0] != 'n') && (upstreamIfName[0] != 'p') ){
			printk("ERROR, only ppp or nas interface is allowed to be the upstream interface, module exit!\n");
			return -EINVAL;
		}
#endif
		WanDev = ifx_find_device_by_name (uIf);
		if (WanDev == NULL){
			printk ("iproxyd_init: Wan interface: %s not found !!\n",upstreamIfName);
			return -EINVAL;
		}
		if (hw_snoop && sIf)
		{
			strcpy (switchIfName, sIf);
		}
		else
		{
			switchIfName[0] = '\0';
		}
	}
#endif // IFX_IGMP_PROXY

	register_netdevice_notifier(&ifx_mcast_device_notifier);
	register_inetaddr_notifier(&ifx_mcast_inetaddr_notifier);
	
	/* Init values setting */
	pGroupTableHead = NULL;
	pGroupTableTail = NULL;

	pReportTableHead = NULL;
	pReportTableTail = NULL;

	pOriginTableHead = NULL;
	pOriginTableTail = NULL;


#ifdef IFX_SNOOPING
	if(mode == IS_SNOOPING) {
		pRouterTableHead = NULL;
		pRouterTableTail = NULL;
	}
#endif
	
	/* Use Leave latency value given by user, default is 1 sec */
	if (LeaveLatency <= 0)
		gLatency = 10; // default = 1 sec 
	else
		gLatency = LeaveLatency;

#ifdef CONFIG_ADM6996_SUPPORT_MODULE
	/* HW snooping */
	if (hw_snoop)
	{
		if (!strcmp(upstreamIfName, "swport0")) {
			pos = 0;
		} else if (!strcmp(upstreamIfName, "swport1")) {
			pos = 1;
		} else if (!strcmp(upstreamIfName, "swport2")) {
			pos = 2;
		} else if (!strcmp(upstreamIfName, "swport3")) {
			pos = 3;
		} else if (!strcmp(upstreamIfName, "swport4")) {
			pos = 4;
		}
		else {
			pos = -1;
		}
		adm_enable_hw_snooping(pos);
	}
#endif // CONFIG_ADM6996_SUPPORT_MODULE

	ifx_mrt_init ();
	ifx_init_vif ();

	mcast_hook_proxy = mcastProxy;

	/* Santosh : Hook for multicast forwarding in Bridge mode */
	mcast_hook_proxy_fwd 	= mcastProxyFwd;
	/* Santosh : Hook for multicast cache to add entries in Linux Multicast Forwarding Cache */
	mcast_hook_mfc_cache	= mcastCache;

#if 0
#ifdef CONFIG_PROC_FS	
	if(mode == IS_SNOOPING) {
		proc_net_create("iproxyd",0,ifx_igmp_info);
	}
#endif // IFX_SNOOPING
#endif	

	if (qint != 0)		
	{
		queryInterval = (qint * HZ);
		ifx_send_general_query_callback (queryInterval);
	}
	
	return 0;
}

static void __exit UNUSED iproxyd_cleanup(void)
{
	unregister_netdevice_notifier(&ifx_mcast_device_notifier);
	unregister_inetaddr_notifier(&ifx_mcast_inetaddr_notifier);

	/* cleanup running timers */
	ifx_group_timer_cleanup ();

	ifx_mrt_exit ();
	ifx_del_vif_in_kernel ();

#ifdef CONFIG_ADM6996_SUPPORT_MODULE
	/* HW snooping, reset to default values */
	if (hw_snoop)
	{
		adm_disable_hw_snooping();
	}
#endif // CONFIG_ADM6996_SUPPORT_MODULE
	
#ifdef CONFIG_PROC_FS	
#ifdef IFX_SNOOPING
	if(mode == IS_SNOOPING) {
		proc_net_remove("iproxyd");
	}
#endif // IFX_SNOOPING
#endif	
	mcast_hook_proxy     = NULL;
	mcast_hook_proxy_fwd = NULL;
	mcast_hook_mfc_cache = NULL;
}

module_init(iproxyd_init);
module_exit(iproxyd_cleanup);

#if 0
EXPORT_SYMBOL(mcast_hook_mfc_cache);
EXPORT_SYMBOL(mcast_hook_proxy_fwd);
EXPORT_SYMBOL(mcast_hook_proxy);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("fchang@infineon");
MODULE_DESCRIPTION("IGMP Proxy/Snooping ver 1.3");
