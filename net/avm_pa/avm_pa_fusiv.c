#ifdef CONFIG_FUSIV_VX180

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <net/sock.h>
#include <linux/avm_pa.h>
#include <linux/spinlock.h>
#include <linux/avm_pa_intern.h>
#include <linux/avm_pa_hw.h>
#include <linux/avm_cpmac.h>
#include <net/pkt_sched.h>

#include <netpro/hostconfig.h>
#include <netpro/apflow.h>
#include <netpro/apflow_ipv6.h>
#include <netpro/userapi.h>
#include <netpro/hostapif.h>

#include <netpro/apstatistics.h>

#define AVM_PA_FUSIV_DEBUG

#if defined(AVM_PA_FUSIV_DEBUG)
#  define AVM_PA_FUSIV_DBG(a...) printk(KERN_ERR "[avm_pa_fusiv] " a);
#else
#  define AVM_PA_FUSIV_DBG(a...)
#endif
extern newapIfStruct_t apArray[];

enum fusivflowtype {
   fusiv_flow_v4,
   fusiv_flow_v6
};

/*------------------------------------------------------------------------------------------*\
 * AVM PA fusiv
\*------------------------------------------------------------------------------------------*/
struct avm_pa_fusiv_session {
    struct avm_pa_session *avm_session;
    unsigned char valid_session;
	unsigned char rxApId;
	unsigned char txApId;
	unsigned short flowhash;
	enum fusivflowtype flowtype;
	union 
	{
	   apFlowEntry_t     *v4;
	   apIpv6FlowEntry_t *v6;
	} flow;
#ifdef CONFIG_FUSIV_KERNEL_APSTATISTICS_PER_INTERFACE
	apStatistics_t prevStat;
#endif
};

static int avm_pa_fusiv_add_session(struct avm_pa_session *avm_session);
static int avm_pa_fusiv_remove_session(struct avm_pa_session *avm_session);

static DEFINE_SPINLOCK( session_list_lock );

static struct avm_hardware_pa avm_pa_fusiv = {
   .add_session               = avm_pa_fusiv_add_session,
   .remove_session            = avm_pa_fusiv_remove_session,
   .try_to_accelerate         = 0,
   .alloc_rx_channel          = 0,
   .alloc_tx_channel          = 0,
   .free_rx_channel           = 0,
   .free_tx_channel           = 0,
};


static struct avm_pa_fusiv_session fusiv_session_array[CONFIG_AVM_PA_MAX_SESSION];

#ifdef CONFIG_FUSIV_KERNEL_APSTATISTICS_PER_INTERFACE
#define AVM_PA_FUSIV_STAT_POLLING_TIME 1
static struct timer_list statistics_timer;
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

static const char *mac2str(const void *cp, char *buf, size_t size)
{
    const unsigned char *mac = (const unsigned char *)cp;
    snprintf(buf, size, "%02X:%02X:%02X:%02X:%02X:%02X",
                        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

static int avm_pa_fusiv_add_session_v4(struct avm_pa_session *avm_session) {
    unsigned long slock_flags;
    int i, rc;
    struct avm_pa_fusiv_session new_session;
    int res = AVM_PA_TX_ERROR_SESSION;
	apFlowEntry_t flow;
	apFlowEntry_t *newflow;
	unsigned int proto, priority, mtu;
	struct avm_pa_pid_hwinfo *ingress_hw, *egress_hw;
	unsigned short flowhash;
	char srcmac[32], dstmac[32], insrc[32];

	AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session_v4: start\n");
	
	ingress_hw = avm_pa_pid_get_hwinfo( avm_session->ingress_pid_handle );
	egress_hw  = avm_pa_pid_get_hwinfo( avm_session->egress[0].pid_handle );

	proto = avm_session->ingress.pkttype & AVM_PA_PKTTYPE_PROTO_MASK;

	memset(&flow, 0, sizeof(flow));
	flow.entryType = AP_TCP_UDP_ENTRY;

	for (i = 0; i < avm_session->ingress.nmatch; i++) {
	   struct avm_pa_match_info *p = &avm_session->ingress.match[i];
	   hdrunion_t *hdr = (hdrunion_t *)&avm_session->ingress.hdrcopy[p->offset + avm_session->ingress.hdroff];
	   
	   switch (p->type) {
	   case AVM_PA_VLAN:
		  break;
	   case AVM_PA_ETH:
		  memcpy(flow.inSrcMacAddr, &hdr->ethh.h_source, ETH_ALEN);
		  break;
	   case AVM_PA_PPP:
		  break;
	   case AVM_PA_PPPOE:
		  {
			 flow.inSessionId = hdr->pppoeh.sid;
		     flow.operations |= (1 << AP_CHECK_PPPOE_BIT);
		  }
		  break;
	   case AVM_PA_IPV4:
		  flow.srcIPAddr = hdr->iph.saddr;
		  flow.dstIPAddr = hdr->iph.daddr;
		  flow.pktInfo.l3Proto.proto = proto;
		  break;
	   case AVM_PA_IPV6:
		  break;
	   case AVM_PA_PORTS:
		  flow.otherInfo.tcpUdpInfo.srcPort = ntohs(hdr->ports[0]);
		  flow.otherInfo.tcpUdpInfo.dstPort = ntohs(hdr->ports[1]);
		  break;
	   default:
		  AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session_v4: can not accelerate, unsupported ingress match type %d \n", p->type);
		  return res;
	   }
	}

	for (i = 0; i < avm_session->egress[0].match.nmatch; i++) {
	   struct avm_pa_match_info *p = avm_session->egress[0].match.match+i;
	   hdrunion_t *hdr = (hdrunion_t *)&avm_session->egress[0].match.hdrcopy[p->offset + avm_session->egress[0].match.hdroff ];
		
	   switch (p->type) {
	   case AVM_PA_VLAN:
	      flow.vlanId = hdr->vlanh.vlan_tci;
	      flow.operations |= (1 << AP_ADD_VLAN_HDR_BIT);
		  break;
	   case AVM_PA_ETH:
		  memcpy(flow.srcMacAddr, &hdr->ethh.h_source, ETH_ALEN);
		  memcpy(flow.dstMacAddr, &hdr->ethh.h_dest, ETH_ALEN);
		  flow.operations |= (1 << AP_DO_ETH_HDR_BIT);
		  break;
	   case AVM_PA_PPP:
		  break;
	   case AVM_PA_PPPOE:
		  {
			 struct pppoehdr *pppoe_hdr = (struct pppoehdr *)(avm_session->egress[0].match.hdrcopy 
									   + avm_session->egress[0].match.hdroff + avm_session->egress[0].pppoe_offset);
			 flow.outSessionId = pppoe_hdr->sid;
			 flow.operations |= (1 << AP_ADD_PPPOE_HDR_BIT);
			 break;
		  }
	   case AVM_PA_IPV4:
		  break;
	   case AVM_PA_IPV6:
		  memcpy(flow.srcIpv6Addr, hdr->ipv6h.saddr.s6_addr32, 16);
		  memcpy(flow.dstIpv6Addr, hdr->ipv6h.daddr.s6_addr32, 16);
		  flow.operations |= (1 << AP_IPV4_TO_IPV6_TUNNEL_BIT);
		  break;
	   case AVM_PA_PORTS:
		  break;
	   default:
		  AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session_v4: can not accelerate, unsupported egress match type %d \n", p->type);
		  break;
	   }
	}

	/* ATA -> LAN: add port VLAN for WAN Port */
	if ((egress_hw->apId == MAC1_ID) && (ingress_hw->apId == MAC1_ID)
		  && (avm_session->egress[0].pid_handle != avm_session->ingress_pid_handle)) {
	   flow.vlanId = avm_cpmac_get_wan_port_vlan();
	   flow.operations |= (1 << AP_ADD_VLAN_HDR_BIT);
	}

#if CONFIG_FUSIV_KERNEL_APSTATISTICS_PER_INTERFACE
	memset(&flow.apStatistics, 0, sizeof(apStatistics_t));
    memset(&new_session.prevStat, 0, sizeof(apStatistics_t));
#endif

	if (avm_session->mod.v4_mod.flags & AVM_PA_V4_MOD_DADDR) {
	   flow.natDstIPAddr = avm_session->mod.v4_mod.daddr;
	   flow.natDstPort = flow.otherInfo.tcpUdpInfo.dstPort;
	   flow.operations |= (1 << AP_DO_DST_NAT_BIT);
	}
	if (avm_session->mod.v4_mod.flags & AVM_PA_V4_MOD_DPORT) {
	   flow.natDstPort = avm_session->mod.v4_mod.dport;
	   flow.operations |= (1 << AP_DO_DST_NAT_BIT);
	}
	if (avm_session->mod.v4_mod.flags & AVM_PA_V4_MOD_SADDR) {
	   flow.natIPAddr = avm_session->mod.v4_mod.saddr;
	   flow.natPort = flow.otherInfo.tcpUdpInfo.srcPort;
	   flow.operations |= (1 << AP_DO_SRC_NAT_BIT);
	}
	if (avm_session->mod.v4_mod.flags & AVM_PA_V4_MOD_SPORT) {
	   flow.natPort = avm_session->mod.v4_mod.sport;
	   flow.operations |= (1 << AP_DO_SRC_NAT_BIT);
	}

	priority = avm_session->egress[0].output.priority;
	priority = (priority & TC_H_MIN_MASK);
	if (priority > 7) priority = 7;
	flow.egressList[0].pFlowID = (void *) priority;

	mtu = avm_session->egress[0].mtu;
	flow.egressList[0].pFlowID = (void *)((UINT32)flow.egressList[0].pFlowID |
		              (UINT32)(mtu) << MTU_SIZE_OFFSET_INSIDE_FLOW_ID);

	flow.egressList[0].pEgress = (void *)(K1_TO_PHYS(apArray[egress_hw->apId].apTxFifo));

	flow.userHandle = (unsigned int)&new_session;
	flow.operations |= (0x1 << AP_ROUTE_VALID_BIT);

	flowhash = apCalculateHash(ingress_hw->apId, &flow);

	rc = apAddFlowEntry(ingress_hw->apId, flowhash, &flow, &newflow);

	if (rc != 0) {
	   AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session_v4: can not accelerate, apAddFlowEntry returned %d \n", rc);
	   return res;
	}

	new_session.avm_session = avm_session;
	new_session.valid_session = 1;
	new_session.rxApId = ingress_hw->apId;
	new_session.txApId = egress_hw->apId;
	new_session.flowhash = flowhash;
	new_session.flow.v4 = newflow;
	new_session.flowtype = fusiv_flow_v4;

    mac2str(flow.srcMacAddr, srcmac, sizeof(srcmac));
    mac2str(flow.dstMacAddr, dstmac, sizeof(dstmac));
    mac2str(flow.inSrcMacAddr, insrc, sizeof(insrc));
	AVM_PA_FUSIV_DBG("apAddFlowEntry: AP#%d->AP#%d srcmac %s dstmac %s insrc %s\n",
		   ingress_hw->apId, egress_hw->apId, srcmac, dstmac, insrc);
	
	// take_session_lock
	spin_lock_irqsave( &session_list_lock, slock_flags);
	if ( !fusiv_session_array[ avm_session->session_handle ].valid_session ){
		fusiv_session_array[ avm_session->session_handle ] = new_session;
		res = AVM_PA_TX_SESSION_ADDED;
	} else {
        AVM_PA_FUSIV_DBG("session add failed - double call for add by avm_pa?!");
        dump_stack();
	}
	// release_session_lock
	spin_unlock_irqrestore( &session_list_lock, slock_flags);

	AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session_v4: done\n");

    return res;
}

static int avm_pa_fusiv_add_session_v6(struct avm_pa_session *avm_session) {
    unsigned long slock_flags;
    int i, rc;
    struct avm_pa_fusiv_session new_session;
    int res = AVM_PA_TX_ERROR_SESSION;
	apIpv6FlowEntry_t flow;
	apIpv6FlowEntry_t *newflow;
	unsigned int proto, priority;
	struct avm_pa_pid_hwinfo *ingress_hw, *egress_hw;
	unsigned short flowhash;
	char srcmac[32], dstmac[32], insrc[32];

	AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session_v6: start\n");
	
	ingress_hw = avm_pa_pid_get_hwinfo( avm_session->ingress_pid_handle );
	egress_hw  = avm_pa_pid_get_hwinfo( avm_session->egress[0].pid_handle );

	proto = avm_session->ingress.pkttype & AVM_PA_PKTTYPE_PROTO_MASK;

	memset(&flow, 0, sizeof(flow));

	if (proto == IPPROTO_UDP)
	   flow.entryType = AP_UDP_ENTRY;
	else if (proto == IPPROTO_TCP)
	   flow.entryType = AP_TCP_ENTRY;
	else {
	   AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session_v6: unsupported protocol %u\n", proto);
	   return res;
	}

	for (i = 0; i < avm_session->ingress.nmatch; i++) {
	   struct avm_pa_match_info *p = &avm_session->ingress.match[i];
	   hdrunion_t *hdr = (hdrunion_t *)&avm_session->ingress.hdrcopy[p->offset + avm_session->ingress.hdroff];
	   
	   switch (p->type) {
	   case AVM_PA_VLAN:
		  break;
	   case AVM_PA_ETH:
		  memcpy(&flow.inSrcMacAddr, &hdr->ethh.h_source, ETH_ALEN);
		  break;
	   case AVM_PA_PPP:
		  break;
	   case AVM_PA_PPPOE:
		  {
			 flow.inSessionId = hdr->pppoeh.sid;
		     flow.operations |= (1 << AP_CHECK_PPPOE_BIT);
		  }
		  break;
	   case AVM_PA_IPV4:
		  break;
	   case AVM_PA_IPV6:
		  memcpy(flow.srcIpv6Addr, hdr->ipv6h.saddr.s6_addr32, 16);
		  memcpy(flow.dstIpv6Addr, hdr->ipv6h.daddr.s6_addr32, 16);
		  break;
	   case AVM_PA_PORTS:
		  flow.otherInfo.tcpUdpInfo.srcPort = ntohs(hdr->ports[0]);
		  flow.otherInfo.tcpUdpInfo.dstPort = ntohs(hdr->ports[1]);
		  break;
	   default:
		  AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session_v6: can not accelerate, unsupported ingress match type %d \n", p->type);
		  return res;
	   }
	}

	for (i = 0; i < avm_session->egress[0].match.nmatch; i++) {
	   struct avm_pa_match_info *p = avm_session->egress[0].match.match+i;
	   hdrunion_t *hdr = (hdrunion_t *)&avm_session->egress[0].match.hdrcopy[p->offset + avm_session->egress[0].match.hdroff ];
		
	   switch (p->type) {
	   case AVM_PA_VLAN:
	      flow.vlanId = hdr->vlanh.vlan_tci;
	      flow.operations |= (1 << AP_ADD_VLAN_HDR_BIT);
		  break;
	   case AVM_PA_ETH:
		  memcpy(flow.srcMacAddr, &hdr->ethh.h_source, ETH_ALEN);
		  memcpy(flow.dstMacAddr, &hdr->ethh.h_dest, ETH_ALEN);
		  flow.operations |= (1 << AP_DO_ETH_HDR_BIT);
		  break;
	   case AVM_PA_PPP:
		  break;
	   case AVM_PA_PPPOE:
		  {
			 struct pppoehdr *pppoe_hdr = (struct pppoehdr *)(avm_session->egress[0].match.hdrcopy 
									   + avm_session->egress[0].match.hdroff + avm_session->egress[0].pppoe_offset);
			 flow.outSessionId = pppoe_hdr->sid;
			 flow.operations |= (1 << AP_ADD_PPPOE_HDR_BIT);
			 break;
		  }
	   case AVM_PA_IPV4:
		  flow.srcIPAddr = hdr->iph.saddr;
		  flow.dstIPAddr = hdr->iph.daddr;
		  flow.operations |= (0x1 << AP_IPV6_TO_IPV4_TUNNEL_BIT);
		  break;
	   case AVM_PA_IPV6:
		  break;
	   case AVM_PA_PORTS:
		  break;
	   default:
		  AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session_v6: can not accelerate, unsupported egress match type %d \n", p->type);
		  break;
	   }
	}

	/* ATA -> LAN: add port VLAN for WAN Port */
	if ((egress_hw->apId == MAC1_ID) && (ingress_hw->apId == MAC1_ID)
		  && (avm_session->egress[0].pid_handle != avm_session->ingress_pid_handle)) {
	   flow.vlanId = avm_cpmac_get_wan_port_vlan();
	   flow.operations |= (1 << AP_ADD_VLAN_HDR_BIT);
	}

#if CONFIG_FUSIV_KERNEL_APSTATISTICS_PER_INTERFACE
	memset(&flow.apStatistics, 0, sizeof(apStatistics_t));
    memset(&new_session.prevStat, 0, sizeof(apStatistics_t));
#endif

	priority = avm_session->egress[0].output.priority;
	priority = (priority & TC_H_MIN_MASK);
	if (priority > 7) priority = 7;

	flow.userHandle = (unsigned int)&new_session;
	flow.egressList[0].pEgress = (void *)(K1_TO_PHYS(apArray[egress_hw->apId].apTxFifo));
	flow.egressList[0].pFlowID = (void *) priority;
	flow.operations |= (0x1 << AP_ROUTE_VALID_BIT);

	flowhash = apIpv6CalculateHash(ingress_hw->apId, &flow);

	rc = apIpv6AddFlowEntry(ingress_hw->apId, flowhash, &flow, &newflow);

	if (rc != 0) {
	   AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session_v6: can not accelerate, apAddFlowEntry returned %d \n", rc);
	   return res;
	}

	new_session.avm_session = avm_session;
	new_session.valid_session = 1;
	new_session.rxApId = ingress_hw->apId;
	new_session.txApId = egress_hw->apId;
	new_session.flowhash = flowhash;
	new_session.flow.v6 = newflow;
	new_session.flowtype = fusiv_flow_v6;

    mac2str(flow.srcMacAddr, srcmac, sizeof(srcmac));
    mac2str(flow.dstMacAddr, dstmac, sizeof(dstmac));
    mac2str(flow.inSrcMacAddr, insrc, sizeof(insrc));
	AVM_PA_FUSIV_DBG("apAddFlowEntry: AP#%d->AP#%d srcmac %s dstmac %s insrc %s\n",
		   ingress_hw->apId, egress_hw->apId, srcmac, dstmac, insrc);
	
	// take_session_lock
	spin_lock_irqsave( &session_list_lock, slock_flags);
	if ( !fusiv_session_array[ avm_session->session_handle ].valid_session ){
		fusiv_session_array[ avm_session->session_handle ] = new_session;
		res = AVM_PA_TX_SESSION_ADDED;
	} else {
        AVM_PA_FUSIV_DBG("session add failed - double call for add by avm_pa?!");
        dump_stack();
	}
	// release_session_lock
	spin_unlock_irqrestore( &session_list_lock, slock_flags);

	AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session_v6: done\n");

    return res;
}

static int avm_pa_fusiv_add_session(struct avm_pa_session *avm_session) {
	struct avm_pa_pkt_match *ingress;
	struct avm_pa_egress *egress;
	struct avm_pa_pid_hwinfo *ingress_hw, *egress_hw;
	unsigned int proto;

    BUG_ON( avm_session->session_handle >= CONFIG_AVM_PA_MAX_SESSION);

	if (avm_session->negress != 1) {
	   AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session: can not accelerate, egress = %d\n", avm_session->negress);
	   return AVM_PA_TX_ERROR_SESSION;
	}
	ingress = &avm_session->ingress;
	egress = &avm_session->egress[0];
	ingress_hw = avm_pa_pid_get_hwinfo( avm_session->ingress_pid_handle );
	egress_hw  = avm_pa_pid_get_hwinfo( avm_session->egress[0].pid_handle );

	if (!ingress_hw || !egress_hw) {
	   AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session: can not accelerate, hw pointer not valid\n");
	   return AVM_PA_TX_ERROR_SESSION;
	}

	if ((egress_hw->apId == 0) || (ingress_hw->apId == 0)) {
	   AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session: can not accelerate, %s AP-ID is 0\n",
			 egress_hw->apId ? "ingress" : "egress");
	   return AVM_PA_TX_ERROR_SESSION;
	}
	if (egress_hw->apId == BMU_ID) {
	   AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session: can not accelerate, AP-ID %d not supported\n",
			 egress_hw->apId);
	   return AVM_PA_TX_ERROR_SESSION;
	}

	proto = avm_session->ingress.pkttype & AVM_PA_PKTTYPE_PROTO_MASK;
	if (proto != IPPROTO_TCP && proto != IPPROTO_UDP) {
	   AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session: can not accelerate, protocol %u not supported \n", proto);
	   return AVM_PA_TX_ERROR_SESSION;
	}

	if (((ingress->pkttype & AVM_PA_PKTTYPE_IP_MASK) == AVM_PA_PKTTYPE_IPV4)) {
	   /* IPV4 -> IPV4 */
	   if (((ingress->pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_NONE)
		   && (egress->match.pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_NONE)
	      return avm_pa_fusiv_add_session_v4(avm_session);
	   /* IPV4 -> IPV4 in IPV6 (DsLite) */
	   if (((ingress->pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_NONE)
		   && (egress->match.pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_IPV6ENCAP)
	      return avm_pa_fusiv_add_session_v4(avm_session); /* currently buggy */
	      //return AVM_PA_TX_ERROR_SESSION;
	   /* IPV4 in IPV6 -> IPV4 (DsLite) */
	   if (((ingress->pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_IPV6ENCAP)
		   && (egress->match.pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_NONE)
	      return avm_pa_fusiv_add_session_v4(avm_session);
	} else if ((ingress->pkttype & AVM_PA_PKTTYPE_IP_MASK) == AVM_PA_PKTTYPE_IPV6) {
	   /* IPV6 -> IPV6 */
	   if (((ingress->pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_NONE)
		   && (egress->match.pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_NONE)
	      return avm_pa_fusiv_add_session_v6(avm_session);
	   /* IPV6 -> IPV6 in IPV4 (6to4) */
	   if (((ingress->pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_NONE)
		   && (egress->match.pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_IPV4ENCAP)
	      return avm_pa_fusiv_add_session_v6(avm_session);
	   /* IPV6 in IPV4 -> IPV6 (6to4) */
	   if (((ingress->pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_IPV4ENCAP)
		   && (egress->match.pkttype & AVM_PA_PKTTYPE_IPENCAP_MASK) == AVM_PA_PKTTYPE_NONE)
	      return avm_pa_fusiv_add_session_v6(avm_session);
	}
	AVM_PA_FUSIV_DBG("avm_pa_fusiv_add_session: can not accelerate, unsupported protocol\n");
	return AVM_PA_TX_ERROR_SESSION;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int avm_pa_fusiv_remove_session( struct avm_pa_session *avm_session ){
	int status = AVM_PA_TX_ERROR_SESSION;
    unsigned long slock_flags;
    struct avm_pa_fusiv_session session_to_remove;
	void *moved;
	int rc = 0;

    BUG_ON( avm_session->session_handle >= CONFIG_AVM_PA_MAX_SESSION);

    // take_session_lock
	spin_lock_irqsave( &session_list_lock, slock_flags);
    if ( ( avm_session == fusiv_session_array[ avm_session->session_handle ].avm_session)
            && fusiv_session_array[ avm_session->session_handle ].valid_session ){
        session_to_remove = fusiv_session_array[ avm_session->session_handle ];
    } else {
        // no valid session found
        // release_session_lock and return
	    spin_unlock_irqrestore( &session_list_lock, slock_flags);
	    return status;
    }
    // release_session_lock
	spin_unlock_irqrestore( &session_list_lock, slock_flags);

	if (session_to_remove.flowtype == fusiv_flow_v4 && session_to_remove.flow.v4) {
	   if ((rc = apDeleteFlowEntry(session_to_remove.rxApId, session_to_remove.flowhash, 
				                   session_to_remove.flow.v4, (void *)&moved)) == 0) {
		  struct avm_pa_fusiv_session *fusiv_session;
		  apFlowEntry_t *flowp = session_to_remove.flow.v4;

#if CONFIG_FUSIV_KERNEL_APSTATISTICS_PER_INTERFACE
		  AVM_PA_FUSIV_DBG("ap2apFlowDelete Stats %lu rxPkt %lu rxByte %lu txPkt %lu txByte \n",
				flowp->apStatistics.rxPkt, flowp->apStatistics.rxByte, flowp->apStatistics.txPkt, flowp->apStatistics.txByte);
#endif
		  if (moved) {
			 fusiv_session = (struct avm_pa_fusiv_session *)flowp->userHandle;
			 fusiv_session->flow.v4 = flowp;
			 AVM_PA_FUSIV_DBG("avm_pa_fusiv_remove_session: v4 flow entry moved \n");
		  }
	   } else {
		  AVM_PA_FUSIV_DBG("avm_pa_fusiv_remove_session: apDeleteFlowEntry failed (rc = %d) \n", rc);
	   }
	} else if (session_to_remove.flowtype == fusiv_flow_v6 && session_to_remove.flow.v6) {
	   if ((rc = apIpv6DeleteFlowEntry(session_to_remove.rxApId, session_to_remove.flowhash, 
				                   session_to_remove.flow.v6, (void *)&moved)) == 0) {
		  struct avm_pa_fusiv_session *fusiv_session;
		  apIpv6FlowEntry_t *flowp = session_to_remove.flow.v6;

#if CONFIG_FUSIV_KERNEL_APSTATISTICS_PER_INTERFACE
		  AVM_PA_FUSIV_DBG("ap2apFlowDelete Stats %lu rxPkt %lu rxByte %lu txPkt %lu txByte \n",
				flowp->apStatistics.rxPkt, flowp->apStatistics.rxByte, flowp->apStatistics.txPkt, flowp->apStatistics.txByte);
#endif
		  if (moved) {
			 fusiv_session = (struct avm_pa_fusiv_session *)flowp->userHandle;
			 fusiv_session->flow.v6 = flowp;
			 AVM_PA_FUSIV_DBG("avm_pa_fusiv_remove_session: v6 flow entry moved \n");
		  }
	   } else {
		  AVM_PA_FUSIV_DBG("avm_pa_fusiv_remove_session: apIpv6DeleteFlowEntry failed (rc = %d) \n", rc);
	   }
	}
	AVM_PA_FUSIV_DBG("avm_pa_fusiv_remove_session: apDeleteFlowEntry successful \n");

    // take_session_lock
	spin_lock_irqsave( &session_list_lock, slock_flags);
    if ( ( avm_session == fusiv_session_array[ avm_session->session_handle ].avm_session)
            && fusiv_session_array[ avm_session->session_handle ].valid_session ){
        memset(&fusiv_session_array[ avm_session->session_handle ],0 , sizeof(struct avm_pa_fusiv_session) );
    	status = AVM_PA_TX_OK;
    } else {
        AVM_PA_FUSIV_DBG("session has been removed already - double call for remove by avm_pa?!");
        dump_stack();
    }
    // release_session_lock
	spin_unlock_irqrestore( &session_list_lock, slock_flags);

    return status;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/


#ifdef CONFIG_FUSIV_KERNEL_APSTATISTICS_PER_INTERFACE
static void avm_pa_check_stat(unsigned long dummy) {
   unsigned int i;
   struct avm_pa_fusiv_session *session;
   unsigned long bytes_since_last_report;
   unsigned long packets_since_last_report;
   apFlowEntry_t *flowpv4;
   apIpv6FlowEntry_t *flowpv6;
    unsigned long slock_flags;

   spin_lock_irqsave( &session_list_lock, slock_flags);

   for (i = 0; i < CONFIG_AVM_PA_MAX_SESSION; i++) {
      session =  &fusiv_session_array[i];
	  if (!session->valid_session) continue;

	  if (session->flowtype == fusiv_flow_v4) {
	     flowpv4 = session->flow.v4;
		 if (flowpv4->apStatistics.txByte >= session->prevStat.txByte) {
			bytes_since_last_report = flowpv4->apStatistics.txByte - session->prevStat.txByte;
		 } else {
			bytes_since_last_report = 
			   ((unsigned long) -1) - session->prevStat.txByte + flowpv4->apStatistics.txByte;
		 }
		 if (flowpv4->apStatistics.txPkt >= session->prevStat.txPkt) {
			packets_since_last_report = flowpv4->apStatistics.txPkt - session->prevStat.txPkt;
		 } else {
			packets_since_last_report = 
			   ((unsigned long) -1) - session->prevStat.txPkt + flowpv4->apStatistics.txPkt;
		 }
		 session->prevStat.txByte = flowpv4->apStatistics.txByte;
		 session->prevStat.rxByte = flowpv4->apStatistics.rxByte;
		 session->prevStat.txPkt = flowpv4->apStatistics.txPkt;
		 session->prevStat.rxPkt = flowpv4->apStatistics.rxPkt;
	  } else {
	     flowpv6 = session->flow.v6;
		 if (flowpv6->apStatistics.txByte >= session->prevStat.txByte) {
			bytes_since_last_report = flowpv6->apStatistics.txByte - session->prevStat.txByte;
		 } else {
			bytes_since_last_report = 
			   ((unsigned long) -1) - session->prevStat.txByte + flowpv6->apStatistics.txByte;
		 }
		 if (flowpv6->apStatistics.txPkt >= session->prevStat.txPkt) {
			packets_since_last_report = flowpv6->apStatistics.txPkt - session->prevStat.txPkt;
		 } else {
			packets_since_last_report = 
			   ((unsigned long) -1) - session->prevStat.txPkt + flowpv6->apStatistics.txPkt;
		 }
		 session->prevStat.txByte = flowpv6->apStatistics.txByte;
		 session->prevStat.rxByte = flowpv6->apStatistics.rxByte;
		 session->prevStat.txPkt = flowpv6->apStatistics.txPkt;
		 session->prevStat.rxPkt = flowpv6->apStatistics.rxPkt;
	  }

	  if (session->avm_session && session->avm_session->session_handle)
	     avm_pa_hardware_session_report(session->avm_session->session_handle,
			                            packets_since_last_report, bytes_since_last_report);
	  else 
	     AVM_PA_FUSIV_DBG("avm_pa_check_stat: no session handle\n");
   }
   spin_unlock_irqrestore( &session_list_lock, slock_flags);

   mod_timer(&statistics_timer,  jiffies + HZ * AVM_PA_FUSIV_STAT_POLLING_TIME - 1);

   //TODO: statistics bei loeschen
}
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

static int __init avm_pa_fusiv_init(void) {
	AVM_PA_FUSIV_DBG("[%s] start \n", __func__);

    memset( &fusiv_session_array[0], 0 , sizeof( struct avm_pa_fusiv_session ) * CONFIG_AVM_PA_MAX_SESSION );

    avm_pa_register_hardware_pa( &avm_pa_fusiv );

#ifdef CONFIG_FUSIV_KERNEL_APSTATISTICS_PER_INTERFACE
	setup_timer(&statistics_timer, avm_pa_check_stat, 0 );
    mod_timer(&statistics_timer, jiffies + HZ * AVM_PA_FUSIV_STAT_POLLING_TIME - 1);
#endif

	AVM_PA_FUSIV_DBG("[%s] init complete \n", __func__);

	return 0;
}

static void __exit avm_pa_fusiv_exit(void) {
	AVM_PA_FUSIV_DBG("[%s] start \n", __func__);

#ifdef CONFIG_FUSIV_KERNEL_APSTATISTICS_PER_INTERFACE
    del_timer(&statistics_timer);
#endif

    avm_pa_register_hardware_pa( 0 );

	AVM_PA_FUSIV_DBG("[%s] exit complete \n", __func__);
}

module_init(avm_pa_fusiv_init);
module_exit(avm_pa_fusiv_exit);

MODULE_DESCRIPTION("Ikanos HW acceleration");

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

#undef unix
struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
    .name = "avm_pa_fusiv",
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
};

#endif
