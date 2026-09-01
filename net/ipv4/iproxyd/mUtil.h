#ifndef __MCAST_UTIL_H_
#define __MCAST_UTIL_H_

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/mroute.h>

int ifx_process_kernel_cache_query
	(
	 	__u32 origin,
		__u32 group,
		struct net_device *dev
	);

int ifx_add_origin_to_group_table
	(
	 	__u32 origin,
		__u32 group,
		int vifi
	);

int ifx_group_device_event (struct net_device *dev);

int ifx_get_vif_index 
	(
	 	struct net_device *dev,
		int *vifi
	);

int ifx_vif_delete (int vifi);

int ifx_vif_device_event (struct net_device *dev);

int ifx_search_vif_table
	(
	 	struct net_device *dev,
		int *vifi
	);

int ifx_vif_add (struct net_device *dev);

int ifx_init_vif (void);

int ifx_del_vif_in_kernel (void);

struct net_device* ifx_get_netdevice (int vifi);

int ifx_get_outgoing_vifs
	(
	 	__u32 group,
		int vifi
	);

int ifx_process_mfcache
	(
	 	__u32 group
	);

int ifx_del_mfcctl
	(
	 	__u32 origin,
		__u32 group,
		int vifi
	 );
	
int ifx_add_mfcctl
	(
	 	__u32 origin,
		__u32 group,
		int vifi
	);

int ifx_is_dev_part_of_switch (struct net_device *dev);

int ifx_convert_ipaddr_to_mcast_macaddr (__u32 ip, unsigned char *mac);

int ifx_get_switch_index (char *ifName);

int ifx_add_to_switch_portmap (char *ifName, unsigned long existingPortmap, unsigned long *portmap);

int ifx_del_from_switch_portmap (char *ifName, unsigned long existingPortmap, unsigned long *portmap);

int ifx_add_mac_entry (struct net_device *dev, __u32 groupAddr);

int ifx_del_mac_entry (struct net_device *dev, __u32 groupAddr);

struct McastMacInfo* ifx_search_mac_entry (__u32 groupaddr); 

void ifx_group_timer_cleanup (void);
	
void ifx_timer_cleanup (void);

int ifx_build_igmp_v2_report
    (
		  struct sk_buff **skb,
	      __u32 saddr,
	      __u32 group,
	      struct net_device *dev
	);

int ifx_add_report_table ( __u32 group );

int ifx_search_report_table ( __u32 group );

int ifx_delete_report_table ( __u32 group );

void ifx_report_timer_callback ( unsigned long data );

__u32 ifx_find_dev_ip_addr (struct net_device *dev);

int ifx_process_group_specific_query ( __u32 groupAddr);

int ifx_process_mcast_routing_in_proxy
	(
	 	struct sk_buff *skb,
		struct net_device *dev,
		__u32 groupAddr
	);

int ifx_check_dev_entry (struct net_device *dev, __u32 groupAddr);

int ifx_send_skb_to_non_bridge_group_device
	(
	 	struct sk_buff *skb,
		__u32 groupAddr
	);

int ifx_check_for_non_bridge_ports
	(
	 	struct sk_buff *skb
	 );

int ifx_mcast_router_table_search (struct net_device *Dev);

int ifx_mcast_router_table_add 
	(
		__u32 routerAddr,
		struct net_device *dev	
	);


void ifx_print_mcast_router_table (void);

int ifx_router_device_event (struct net_device *dev);

void ifx_router_timer_callback (unsigned long data);

int ifx_delete_router_table ( __u32 routerAddr );

int ifx_count_mcast_router_table (void);

int ifx_send_skb_to_mcast_router 
	(
	 	struct sk_buff *skb
	);

void ifx_refresh_report_flag (void);

void ifx_change_src_ip
	(
	  struct sk_buff **skb
	);

void ifx_igmp_timer_expire_callback 
	(
	 	unsigned long data
	);

int ifx_send_query_to_group_device 
	(
	 	struct sk_buff *skb
	);

int ifx_send_skb_to_other_ports 
	(
	 	struct sk_buff *skb
	);

int ifx_send_to_group_device_and_mcast_router 
	(
	 	struct sk_buff *skb,
		__u32  groupAddr
	);

int ifx_check_report_flag
	(
		__u32	groupAddr
	);

struct net_device *ifx_find_device_by_name
	(
		char *ifName
	);

int ifx_send_skb_to_every_downstream_device
	(
		struct sk_buff *skb
	);

int ifx_send_skb_to_group_device
	(
		struct sk_buff *skb,
		__u32 groupAddr
	);	
	
int ifx_send_skb_to_upstream_device
	(
		struct sk_buff *skb
	);	

int ifx_send_skb_by_ifname
	(
		struct sk_buff *skb,
		char *ifName	
	);

int ifx_send_skb_by_dev
	(
		struct sk_buff *skb,
		struct net_device *dev	
	);
	
int ifx_send_igmpv2_report
	(
		struct sk_buff *skb
	);

int ifx_count_downstream_dev
	(
		void
	);
	
int ifx_process_igmp
	(
		struct sk_buff *skb
	);

int ifx_process_mcast_routing
	(
		struct sk_buff *skb
	);		

struct McastGroupTable *ifx_search_group_table
	(
		__u32 	groupAddr,
		int 	*count	
	);

int ifx_is_downstream_dev
	(
		char *ifName
	);

void ifx_print_ip_packet
	(
		struct sk_buff *skb
	);		

void ifx_print_igmp_packet
	(
		struct sk_buff *skb
	);		

void ifx_update_general_query_count
	(
		void
	);

int ifx_update_group_specific_query_count
	(
		__u32	groupAddr,
		int		IncOrDec
	);

void ifx_print_McastGroupTable
	(
		void
	);

int ifx_mcast_group_table_add
	(
		__u32 		groupAddr,
		char		*ifName, 
		struct net_device	*dev
	);

int ifx_mcast_group_table_delete
	(
		__u32 		groupAddr,
		char		*ifName, 
		struct net_device	*dev
	);

int ifx_build_igmp_general_query (struct sk_buff **skb);

void ifx_send_general_query_callback (int queryInterval);

int ifx_is_dev_only_interface_with_group
	(
		__u32 groupAddr
	);

int ifx_skipped_protocols
	(
		struct sk_buff *skb
	);

int ifx_print_IpDot
	(
		__u32 ipValue
	);

void mappingIpToEthMcast
        (
                __u32 ipvalue,
                unsigned char *pMac
        );

#endif /* __MCAST_UTIL_H_ */
