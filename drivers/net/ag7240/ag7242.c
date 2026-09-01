/*------------------------------------------------------------------------------------------*\
 *
\*------------------------------------------------------------------------------------------*/
#include <linux/stddef.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/igmp.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/bitops.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <net/sch_generic.h>
#include <linux/if_pppox.h>
#include <linux/ip.h>
#include <net/checksum.h>

#define AG7240_DEBUG

#include "ag7240.h"
#include "ag7240_trc.h"
#include "ag7240_mac.h"
#include "athrs_ioctl.h"
#include <mach_avm.h>

#include <asm/prom.h>
#include <linux/workqueue.h>
#include <linux/avm_led_event.h>

#define PHY11G_CHIP_ID                      0x0302
#define PHY11G_SUB_ID                       0x60D1
#define ATH8030_CHIP_ID                     0x004D
#define ATH8030_SUB_ID                      0xD000

#define ATHR_PHY_MAX    2
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define MODULE_NAME "AG7240"
MODULE_LICENSE("Dual BSD/GPL");

module_param(tx_len_per_ds, int, 0);
MODULE_PARM_DESC(tx_len_per_ds, "Size of DMA chunk");

int fifo_3 = 0x1f00140;
module_param(fifo_3, int, 0);
MODULE_PARM_DESC(fifo_3, "fifo cfg 3 settings");

static void mii_gphy_work_handler(struct work_struct *work);
struct workqueue_struct *mii_gphy_workqueue;
DECLARE_WORK(mii_gphy_work, mii_gphy_work_handler);

ag7240_mac_t *ag7240_macs[2];
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ag7240_get_macaddr(ag7240_mac_t *mac, u8 *mac_addr) {
    
    int i;
    char *ep, *p = prom_getenv("maca");
    if (p) {
        memset(mac_addr, 0, 6);
        for (i = 0; i < 6; i++)	{
            mac_addr[i] = p ? simple_strtoul(p, &ep, 16) : 0;
            if (p)
                p = (*ep) ? ep+1 : ep;
        }
        printk("[%s] address %2x:%2x:%2x:%2x:%2x:%2x \n" , __FUNCTION__ \
            ,mac_addr[0] ,mac_addr[1] ,mac_addr[2] ,mac_addr[3] ,mac_addr[4] ,mac_addr[5]);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ag7240_vet_tx_len_per_pkt(unsigned int *len) {
    unsigned int l;

    /* make it into words */
    l = *len & ~3;

    /* Not too small */
    if (l < AG7240_TX_MIN_DS_LEN) {
        l = AG7240_TX_MIN_DS_LEN;
    } else {
        /* Avoid len where we know we will deadlock, that is the range between fif_len/2 and the MTU size */
        if (l > AG7240_TX_FIFO_LEN/2) {
            if (l < AG7240_TX_MTU_LEN) {
                l = AG7240_TX_MTU_LEN;
            } else if (l > AG7240_TX_MAX_DS_LEN) {
                l = AG7240_TX_MAX_DS_LEN;
            }
            *len = l;
        }
    }
}

/*------------------------------------------------------------------------------------------*\
 * es gibt nur einen MDIO-MAC
\*------------------------------------------------------------------------------------------*/
uint16_t ag7240_mdio_read(ag7240_mac_t *mac, uint8_t reg) {
    uint16_t      addr  = (mac->phy_addr << AG7240_ADDR_SHIFT) | reg, val;
    volatile int  rddata;
    uint16_t      ii = 0x1000;
    uint32_t      mac_base = ag7240_mac_base(0);

    ar7240_reg_wr(mac_base + AG7240_MII_MGMT_CMD, 0x0);
    ar7240_reg_wr(mac_base + AG7240_MII_MGMT_ADDRESS, addr);
    ar7240_reg_wr(mac_base + AG7240_MII_MGMT_CMD, AG7240_MGMT_CMD_READ);

    do {
        rddata = ar7240_reg_rd(mac_base + AG7240_MII_MGMT_IND) & 0x1;
        schedule();
    } while(rddata && --ii);

    val = ar7240_reg_rd(mac_base + AG7240_MII_MGMT_STATUS);
    ar7240_reg_wr(mac_base + AG7240_MII_MGMT_CMD, 0x0);

    return val;
}

/*------------------------------------------------------------------------------------------*\
 * es gibt nur einen MDIO-MAC
\*------------------------------------------------------------------------------------------*/
void ag7240_mdio_write(ag7240_mac_t *mac, uint8_t reg, uint16_t data) {
    uint16_t      addr  = (mac->phy_addr << AG7240_ADDR_SHIFT) | reg;
    volatile int  rddata;
    uint16_t      ii = 0x1000;
    uint32_t      mac_base = ag7240_mac_base(0);

    ar7240_reg_wr(mac_base + AG7240_MII_MGMT_ADDRESS, addr);
    ar7240_reg_wr(mac_base + AG7240_MII_MGMT_CTRL, data);

    do {
        rddata = ar7240_reg_rd(mac_base + AG7240_MII_MGMT_IND) & 0x1;
        schedule();
    } while(rddata && --ii);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct net_device_stats *ag7240_get_stats(struct net_device *dev) {

    ag7240_mac_t *mac = (ag7240_mac_t *) netdev_priv(dev);
    struct Qdisc *sch;
    int carrier = netif_carrier_ok(dev);

    if (mac->mac_link == AG7240_NOLINK) {
        sch = rcu_dereference(dev->qdisc);
        mac->mac_net_stats.tx_dropped = sch->qstats.drops;
        return &mac->mac_net_stats;
    }

    /* MIB registers reads will fail if link is lost while resetting PHY. */
    if (carrier && !mac->phy_in_reset) {

        mac->mac_net_stats.rx_packets = ag7240_reg_rd(mac,AG7240_RX_PKT_CNTR);
        mac->mac_net_stats.tx_packets = ag7240_reg_rd(mac,AG7240_TX_PKT_CNTR);
        mac->mac_net_stats.rx_bytes   = ag7240_reg_rd(mac,AG7240_RX_BYTES_CNTR);
        mac->mac_net_stats.tx_bytes   = ag7240_reg_rd(mac,AG7240_TX_BYTES_CNTR);
        mac->mac_net_stats.tx_errors  = ag7240_reg_rd(mac,AG7240_TX_CRC_ERR_CNTR);
        mac->mac_net_stats.rx_dropped = ag7240_reg_rd(mac,AG7240_RX_DROP_CNTR);
        mac->mac_net_stats.tx_dropped = ag7240_reg_rd(mac,AG7240_TX_DROP_CNTR);
        mac->mac_net_stats.collisions = ag7240_reg_rd(mac,AG7240_TOTAL_COL_CNTR);
    
        /* detailed rx_errors: */
        mac->mac_net_stats.rx_length_errors = ag7240_reg_rd(mac,AG7240_RX_LEN_ERR_CNTR);
        mac->mac_net_stats.rx_over_errors   = ag7240_reg_rd(mac,AG7240_RX_OVL_ERR_CNTR);
        mac->mac_net_stats.rx_crc_errors    = ag7240_reg_rd(mac,AG7240_RX_CRC_ERR_CNTR);
        mac->mac_net_stats.rx_frame_errors  = ag7240_reg_rd(mac,AG7240_RX_FRM_ERR_CNTR);
    
        mac->mac_net_stats.rx_errors  = ag7240_reg_rd(mac,AG7240_RX_CODE_ERR_CNTR) + 
                                        ag7240_reg_rd(mac,AG7240_RX_CRS_ERR_CNTR) +      
                                        mac->mac_net_stats.rx_length_errors +
                                        mac->mac_net_stats.rx_over_errors +
                                        mac->mac_net_stats.rx_crc_errors +
                                        mac->mac_net_stats.rx_frame_errors; 
    
        mac->mac_net_stats.multicast  = ag7240_reg_rd(mac,AG7240_RX_MULT_CNTR) +
                                        ag7240_reg_rd(mac,AG7240_TX_MULT_CNTR);

    }
    return &mac->mac_net_stats;
}

/*------------------------------------------------------------------------------------------*\
 * Code under test - do not use
\*------------------------------------------------------------------------------------------*/
static ag7240_desc_t * ag7240_get_tx_ds(ag7240_mac_t *mac, int *len, unsigned char **start,int ac) {
    ag7240_desc_t      *ds;
    int                len_this_ds;
    ag7240_ring_t      *r   = &mac->mac_txring[ac];
    ag7240_buffer_t    *bp;

    /* force extra pkt if remainder less than 4 bytes */
    if (*len > tx_len_per_ds)
        if (*len < (tx_len_per_ds + 4))
            len_this_ds = tx_len_per_ds - 4;
        else
            len_this_ds = tx_len_per_ds;
    else
        len_this_ds    = *len;

    ds = &r->ring_desc[r->ring_head];

    ag7240_trc_new(ds,"ds addr");
    ag7240_trc_new(ds,"ds len");
    assert(!ag7240_tx_owned_by_dma(ds));

    ds->pkt_size       = len_this_ds;
    ds->pkt_start_addr = virt_to_phys(*start);
    ds->more           = 1;

    *len   -= len_this_ds;
    *start += len_this_ds;

    /* Time stamp each packet */
    if (mac_has_flag(mac,CHECK_DMA_STATUS)) {
        bp = &r->ring_buffer[r->ring_head];
        /*--- bp->trans_start = jiffies; ---*/ 
    }

    ag7240_ring_incr(r->ring_head);

    return ds;
}

/*
 * Tx operation:
 * We do lazy reaping - only when the ring is "thresh" full. If the ring is 
 * full and the hardware is not even done with the first pkt we q'd, we turn
 * on the tx interrupt, stop all q's and wait for h/w to
 * tell us when its done with a "few" pkts, and then turn the Qs on again.
 *
 * Locking:
 * The interrupt only touches the ring when Q's stopped  => Tx is lockless, 
 * except when handling ring full.
 *
 * Desc Flushing: Flushing needs to be handled at various levels, broadly:
 * - The DDr FIFOs for desc reads.
 * - WB's for desc writes.
 */
static void ag7240_handle_tx_full(ag7240_mac_t *mac) {
    unsigned long flags;

    assert(!netif_queue_stopped(mac->mac_dev));

    mac->mac_net_stats.tx_fifo_errors ++;

    netif_stop_queue(mac->mac_dev);

    spin_lock_irqsave(&mac->mac_lock, flags);
    ag7240_intr_enable_tx(mac);
    spin_unlock_irqrestore(&mac->mac_lock, flags);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ag7240_hard_start(struct sk_buff *skb, struct net_device *dev) {
    struct ethhdr      *eh;
    ag7240_mac_t *mac = (ag7240_mac_t *) netdev_priv(dev);
    ag7240_ring_t      *r;
    ag7240_buffer_t    *bp;
    ag7240_desc_t      *ds, *fds;
    unsigned char      *start;
    int                len;
    int                nds_this_pkt;
    int                ac = 0;
#ifdef CONFIG_AR7240_S26_VLAN_IGMP
    if(unlikely((skb->len <= 0) || (skb->len > (dev->mtu + ETH_VLAN_HLEN +6 ))))
#else
    if(unlikely((skb->len <= 0) || (skb->len > (dev->mtu + ETH_HLEN))))
#endif
    {
        printk(MODULE_NAME ": bad skb, len %d\n", skb->len);
        goto dropit;
    }

    for (ac = 0;ac < mac->mac_noacs; ac++) {
        if (ag7240_tx_reap_thresh(mac,ac)) 
            ag7240_tx_reap(mac,ac);
    }

    /* Select the TX based on access category */
    ac = ENET_AC_BE;
    if ( (mac_has_flag(mac,WAN_QOS_SOFT_CLASS)) || (mac_has_flag(mac,ATHR_S26_HEADER))
        || (mac_has_flag(mac,ATHR_S16_HEADER)))  { 
        /* Default priority */
        eh = (struct ethhdr *) skb->data;

        if (eh->h_proto  == __constant_htons(ETHERTYPE_IP)) {
            const struct iphdr *ip = (struct iphdr *)
                        (skb->data + sizeof (struct ethhdr));
            /*
             * IP frame: exclude ECN bits 0-1 and map DSCP bits 2-7
             * from TOS byte.
             */
            ac = TOS_TO_ENET_AC ((ip->tos >> TOS_ECN_SHIFT) & 0x3F);
        }
    }
    skb->priority=ac;
    /* add header to normal frames sent to LAN*/
    if (mac_has_flag(mac,ATHR_S26_HEADER)) {
        skb_push(skb, ATHR_HEADER_LEN);
        skb->data[0] = 0x30; /* broadcast = 0; from_cpu = 0; reserved = 1; port_num = 0 */
        skb->data[1] = (0x40 | (ac << HDR_PRIORITY_SHIFT)); /* reserved = 0b10; priority = 0; type = 0 (normal) */
        skb->priority=ENET_AC_BE;
    }

    if (mac_has_flag(mac,ATHR_S16_HEADER)) {
        skb_push(skb, ATHR_HEADER_LEN);
        memcpy(skb->data,skb->data + ATHR_HEADER_LEN, 12);

        skb->data[12] = 0x30; /* broadcast = 0; from_cpu = 0; reserved = 1; port_num = 0 */
        skb->data[13] = 0x40 | ((ac << HDR_PRIORITY_SHIFT)); /* reserved = 0b10; priority = 0; type = 0 (normal) */
        skb->priority=ENET_AC_BE;
    }
    /*hdr_dump("Tx",mac->mac_unit,skb->data,ac,0);*/
    r = &mac->mac_txring[skb->priority];

    assert(r);

    ag7240_trc_new(r->ring_head,"hard-stop hd");
    ag7240_trc_new(r->ring_tail,"hard-stop tl");

    ag7240_trc_new(skb->len,    "len this pkt");
    ag7240_trc_new(skb->data,   "ptr 2 pkt");

    dma_cache_wback((unsigned long)skb->data, skb->len);

    bp          = &r->ring_buffer[r->ring_head];

    assert(bp);

    bp->buf_pkt = skb;
    len         = skb->len;
    start       = skb->data;

    assert(len > 4);

    nds_this_pkt = 1;

    fds = ds = ag7240_get_tx_ds(mac, &len, &start, skb->priority);

    while (len > 0) {
        ds = ag7240_get_tx_ds(mac, &len, &start, skb->priority);
        nds_this_pkt++;
        ag7240_tx_give_to_dma(ds);
    }

    ds->res1            = 0;
    ds->res2            = 0;
    ds->ftpp_override   = 0;
    ds->res3            = 0;

    ds->more            = 0;
    ag7240_tx_give_to_dma(fds);

    bp->buf_lastds      = ds;
    bp->buf_nds         = nds_this_pkt;

    ag7240_trc_new(ds,"last ds");
    ag7240_trc_new(nds_this_pkt,"nmbr ds for this pkt");

    wmb();

    mac->net_tx_packets ++;
    mac->net_tx_bytes += skb->len;

    ag7240_trc(ag7240_reg_rd(mac, AG7240_DMA_TX_CTRL),"dma idle");
    
    if (mac_has_flag(mac,WAN_QOS_SOFT_CLASS)) {
        ag7240_tx_start_qos(mac,skb->priority);
    } else {
        ag7240_tx_start(mac);
    }

    for ( ac = 0;ac < mac->mac_noacs; ac++) {
        if (unlikely(ag7240_tx_ring_full(mac,ac))) 
            ag7240_handle_tx_full(mac);
    }

    return NETDEV_TX_OK;

dropit:
    printk(MODULE_NAME ": dropping skb %08x\n", (unsigned int)skb);
    kfree_skb(skb);
    return NETDEV_TX_OK;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ag7240_do_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd) {

    struct eth_cfg_params *ethcfg;
    ag7240_mac_t *mac = (ag7240_mac_t *) netdev_priv(dev);

#ifdef CONFIG_AR7240_S26_VLAN_IGMP
    struct arl_struct * arl = (struct arl_struct *) (&ifr->ifr_ifru.ifru_mtu);
    unsigned int vlan_value = ifr->ifr_ifru.ifru_ivalue;
    unsigned short vlan_id = vlan_value >> 16;
    unsigned short mode = vlan_value >> 16;
    unsigned short vlan_port = vlan_value & 0x1f;
    unsigned int flag = 0;
    uint32_t ret = 0;
#endif

    ethcfg = (struct eth_cfg_params *)ifr->ifr_data;

    switch(cmd){
#if 0 /*--- #ifdef CONFIG_AR7240_S26_VLAN_IGMP ---*/
        case S26_PACKET_FLAG:
            printk("ag7240::S26_PACKET_FLAG %d \n",vlan_value);
            set_packet_inspection_flag(vlan_value);
            break;

        case S26_VLAN_ADDPORTS:
            if(vlan_id>4095) return -EINVAL;
            printk("ag7240::S26_ADD_PORT vid = %d ports=%x.\n",vlan_id,vlan_port);
            ret = python_ioctl_vlan_addports(vlan_id,vlan_port);
            break;

        case S26_VLAN_DELPORTS:
            if(vlan_id>4095) return -EINVAL;
            printk("ag7240::S26_DEL_PORT vid = %d ports=%x.\n",vlan_id,vlan_port);
            ret = python_ioctl_vlan_delports(vlan_id,vlan_port);
            break;

        case S26_VLAN_SETTAGMODE:
            printk("ag7240::S26_VLAN_SETTAGMODE mode=%d portno=%d .\n",mode,vlan_port);
            ret = python_port_egvlanmode_set(vlan_port,mode);
            break;

        case S26_VLAN_SETDEFAULTID:
            if(vlan_id>4095) return -EINVAL;
            printk("ag7240::S26_VLAN_SETDEFAULTID vid = %d portno=%d.\n",vlan_id,vlan_port);
            ret = python_port_default_vid_set(vlan_port,vlan_id);
            break;

        case  S26_IGMP_ON_OFF:
        {
            int tmp = 0;
            tmp = vlan_value & (0x1 << 7);
            vlan_port &= ~(0x1 << 7);
            if(vlan_port>4) return -EINVAL;
            if(tmp != 0){
                printk("ag7240::Enable IGMP snooping in port no %x.\n",vlan_port);
                ret= python_port_igmps_status_set(vlan_port,1);
            }else{
                printk("ag7240::Disable IGMP snooping in port no %x.\n",vlan_port);
                ret= python_port_igmps_status_set(vlan_port,0);
            }
        }
            break;

        case S26_LINK_GETSTAT:
            if(vlan_port>4){/* if port=WAN */
                int fdx, phy_up;
                ag7240_phy_speed_t  speed;
                ag7240_get_link_status(0, &phy_up, &fdx, &speed, 4);
                ifr->ifr_ifru.ifru_ivalue = (speed<<16|fdx<<8|phy_up);
                printk("ag7240::S26_LINK_GETSTAT portno WAN is %x.\n",ifr->ifr_ifru.ifru_ivalue);
            }else if(vlan_port > 0){
                flag = athrs26_phy_is_link_alive(vlan_port-1);
                ifr->ifr_ifru.ifru_ivalue = flag;
                printk("ag7240::S26_LINK_GETSTAT portno %d is %s.\n",vlan_port,flag?"up":"down");
            }else{
                ifr->ifr_ifru.ifru_ivalue = 1;
            }
            /* PHY 0-4 <---> port 1-5 in user space. */
            break;

        case S26_VLAN_ENABLE:
            python_ioctl_enable_vlan();
            printk("ag7240::S26_VLAN_ENABLE.\n");
            break;

        case S26_VLAN_DISABLE:
            python_ioctl_disable_vlan();
            printk("ag7240::S26_VLAN_DISABLE.\n");
            break;

        case S26_ARL_ADD:
            ret = python_fdb_add(arl->mac_addr,arl->port_map,arl->sa_drop);
            printk("ag7240::S26_ARL_ADD,mac:[%x.%x.%x.%x.%x.%x] port[%x] drop %d\n",
                arl->mac_addr.uc[0],arl->mac_addr.uc[1],arl->mac_addr.uc[2],arl->mac_addr.uc[3],
                arl->mac_addr.uc[4],arl->mac_addr.uc[5],arl->port_map,arl->sa_drop);
            break;

        case S26_ARL_DEL:
            ret = python_fdb_del(arl->mac_addr);
            printk("ag7240::S26_ARL_DEL mac:[%x.%x.%x.%x.%x.%x].\n",arl->mac_addr.uc[0],arl->mac_addr.uc[1],
            arl->mac_addr.uc[2],arl->mac_addr.uc[3],arl->mac_addr.uc[4],arl->mac_addr.uc[5]);

        case S26_MCAST_CLR:
            /* 0: switch off the unkown multicast packets over vlan. 1: allow the unknown multicaset packets over vlans. */
            if((vlan_port%2)==0)
                python_clear_multi();
            else
                python_set_multi();
                printk("ag7240::S26_MCAST_CLR --- %s.\n", (vlan_port%2)?"enable Multicast":"disable Multicast");
            break;
#endif
        case S26_RD_PHY: 
            ethcfg->val = ag7240_mdio_read(mac, ethcfg->phy_reg);
            break;
        
        case S26_WR_PHY:
            ag7240_mdio_write(mac, ethcfg->phy_reg, ethcfg->val);
            break;

        case S26_FORCE_PHY:
            DPRINTF("Duplex %d Phy_Reg %d\n",ethcfg->duplex, ethcfg->phy_reg);
            if(ethcfg->phy_reg < ATHR_PHY_MAX) {
                if(ethcfg->val == 10) {
                    DPRINTF("Forcing 10Mbps %d on port:%d \n", ethcfg->duplex,(ethcfg->phy_reg));
                    /*--- athrs26_force_10M(ethcfg->phy_reg,ethcfg->duplex); ---*/
                }else if(ethcfg->val == 100) {
                    DPRINTF("Forcing 100Mbps %d on port:%d \n", ethcfg->duplex,(ethcfg->phy_reg));
                    /*--- athrs26_force_100M(ethcfg->phy_reg,ethcfg->duplex); ---*/
                }else if(ethcfg->val == 1000) {
                    DPRINTF("Forcing 1Gbps %d on port:%d \n", ethcfg->duplex,(ethcfg->phy_reg));
                    /*--- athrs26_force_100M(ethcfg->phy_reg,ethcfg->duplex); ---*/
                    break;
                }else if(ethcfg->val == 0) {
                    DPRINTF("Enabling Auto Neg on port:%d \n",(ethcfg->phy_reg));
                    /*--- s26_wr_phy(ethcfg->phy_reg,ATHR_PHY_CONTROL,0x9000); ---*/
               }else
                   return -EINVAL;
               mac->config_phy(mac, ethcfg->val, ethcfg->duplex);
#if 0
               if(ATHR_ETHUNIT(ethcfg->phy_reg) == ENET_UNIT_WAN) {
                   if(mac_has_flag(ag7240_macs[ENET_UNIT_LAN],ETH_SWONLY_MODE))
                       ag7240_check_link(ag7240_macs[ENET_UNIT_LAN],ethcfg->phy_reg);
                   else
                       ag7240_check_link(ag7240_macs[0],ethcfg->phy_reg);
               }else{ 
                   ag7240_check_link(ag7240_macs[1],ethcfg->phy_reg);
               }
#endif
            break;
            } 
        default: 
            return -EINVAL;
        }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ag7240_tx_timeout(struct net_device *dev) {

    printk(KERN_ERR "[%s]\n", __FUNCTION__);
}

/*
 * Several fields need to be programmed based on what the PHY negotiated
 * Ideally we should quiesce everything before touching the pll, but:
 * 1. If its a linkup/linkdown, we dont care about quiescing the traffic.
 * 2. If its a single gigE PHY, this can only happen on lup/ldown.
 * 3. If its a 100Mpbs switch, the link will always remain at 100 (or nothing)
 * 4. If its a gigE switch then the speed should always be set at 1000Mpbs, 
 *    and the switch should provide buffering for slower devices.
 *
 * XXX Only gigE PLL can be changed as a parameter for now. 100/10 is hardcoded.
 * XXX Need defines for them -
 * XXX FIFO settings based on the mode
 */
static void ag7240_set_mac_from_link(ag7240_mac_t *mac) {

    ag7240_set_mac_duplex(mac, mac->mac_fdx);
    ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_3, fifo_3);

    switch (mac->mac_speed) {
        case AG7240_PHY_SPEED_1000T:
            ag7240_set_mac_if(mac, 1);
            ag7240_reg_rmw_set(mac, AG7240_MAC_FIFO_CFG_5, (1 << 19));
            if ( mac->mac_unit == 0 )
                ar7240_reg_wr(AR7242_ETH_XMII_CONFIG, (1<<25));
                /*--- ar7240_reg_wr(AR7242_ETH_XMII_CONFIG, 0x16000000 | (1<<25)); ---*/
            break;

        case AG7240_PHY_SPEED_100TX:
            ag7240_set_mac_if(mac, 0);
            ag7240_set_mac_speed(mac, 1);
            ag7240_reg_rmw_clear(mac, AG7240_MAC_FIFO_CFG_5, (1 << 19));
            if ( mac->mac_unit == 0 )
                ar7240_reg_wr(AR7242_ETH_XMII_CONFIG,0x0101);
            break;

        case AG7240_PHY_SPEED_10T:
            ag7240_set_mac_if(mac, 0);
            ag7240_set_mac_speed(mac, 0);
            ag7240_reg_rmw_clear(mac, AG7240_MAC_FIFO_CFG_5, (1 << 19));
            if ( mac->mac_unit == 0 ) {
                ar7240_reg_wr(AR7242_ETH_XMII_CONFIG,0x1313);
            }
            break;

        default:
            assert(0);
    }
    DPRINTF(": cfg_1: %#x\n", ag7240_reg_rd(mac, AG7240_MAC_FIFO_CFG_1));
    DPRINTF(": cfg_2: %#x\n", ag7240_reg_rd(mac, AG7240_MAC_FIFO_CFG_2));
    DPRINTF(": cfg_3: %#x\n", ag7240_reg_rd(mac, AG7240_MAC_FIFO_CFG_3));
    DPRINTF(": cfg_4: %#x\n", ag7240_reg_rd(mac, AG7240_MAC_FIFO_CFG_4));
    DPRINTF(": cfg_5: %#x\n", ag7240_reg_rd(mac, AG7240_MAC_FIFO_CFG_5));
}

/*------------------------------------------------------------------------------------------*\
 * Used for Interrupt Handler
\*------------------------------------------------------------------------------------------*/
struct _hw_gpio_irqhandle   *irq_handle;
static int mii_gphy_int_handler (unsigned int irq) {

    queue_work(mii_gphy_workqueue, &mii_gphy_work);
    avm_gpio_disable_irq(irq_handle);
    return IRQ_HANDLED;
}

union phy_irq_status {
    struct _phy_irq_status {
        unsigned int reserved1 : 16;
        unsigned int wake_on_lan : 1;
        unsigned int msre : 1;
        unsigned int nprx : 1;
        unsigned int nptx : 1;
        unsigned int ane : 1;
        unsigned int anc : 1;
        unsigned int reserved2 : 4;
        unsigned int adsc : 1;
        unsigned int polariy : 1;
        unsigned int mdix : 1;
        unsigned int duplex : 1;
        unsigned int linkspeed : 1;
        unsigned int linkstate : 1;
    } Bits;
    unsigned int reg;
}; 

static uint16_t phy11g_read_irq_status(ag7240_mac_t *mac);
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void mii_gphy_work_handler(struct work_struct *work) {

    union phy_irq_status  status;
    uint32_t i, old_status;
    uint16_t    linkReg, speedReg;
    ag7240_mac_t    *mac;

    DPRINTF("[%s\n", __FUNCTION__);
    
    for (i = 0; i < AG7240_NMACS; i++) {
        mac = ag7240_macs[i];
        if (mac->phy_addr != (-1)) { 
            old_status = mac->mac_link;
            status.reg = phy11g_read_irq_status(mac);

            DPRINTF("[%s] PHY %d status 0x%x\n", __FUNCTION__, mac->phy_addr, status.reg);

            /*--- interrupt status bit ---*/
            if(status.Bits.linkstate || status.Bits.linkspeed || status.Bits.duplex || status.Bits.mdix) { 

                /*cause the interrupt due to link status, speed & FDX */
                linkReg = ag7240_mdio_read(mac, 0x01);     /*--- default-Phy-register ---*/

                /*------------------------------------------------------------------------------*\
                 * Link Status prüfen
                \*------------------------------------------------------------------------------*/
                if((linkReg & (1 << 2)) == 0) {     /*--- link Status ---*/
                    mac->mac_link = 0x0;   /*--- LINK DOWN ---*/
                } else {
                    mac->mac_link = 0x1; /*--- LINK UP ---*/
                    /*--------------------------------------------------------------------------*\
                     * Speed Prüfen wenn link UP
                    \*--------------------------------------------------------------------------*/
                    speedReg = mac->read_phy_link_status(mac);

                    if(speedReg & (1 << 3)) {
                        mac->mac_fdx = AG7240_DUPLEX_FULL;
                    }else {
                        mac->mac_fdx = AG7240_DUPLEX_HALF;
                    }
                    switch (speedReg & (3 << 0)) {
                        case 0 :  /*--- speed 10BASE-T ---*/
                            mac->mac_speed = AG7240_PHY_SPEED_10T;
                            DPRINTF("[%s] speed = %d duplex = %s\n", __FUNCTION__, 10, mac->mac_fdx == DUPLEX_HALF ? "half" : "full");
                            break;
                        case 1:	  /*--- speed 100BASE-TX ---*/
                            mac->mac_speed = AG7240_PHY_SPEED_100TX;
                            DPRINTF("[%s] speed = %d duplex = %s\n", __FUNCTION__, 100, mac->mac_fdx == DUPLEX_HALF ? "half" : "full");
                            break;
                        case 2:	  /*--- speed 1000BASE-T ---*/
                            mac->mac_speed = AG7240_PHY_SPEED_1000T;
                            DPRINTF("[%s] speed = %d duplex = %s\n", __FUNCTION__, 1000, mac->mac_fdx == DUPLEX_HALF ? "half" : "full");
                            break;
                    }
                }
            }
            if(old_status != mac->mac_link) {
                if(mac->mac_dev) {
                    if(mac->mac_link) {
                        ag7240_set_mac_from_link(mac);

                        netif_carrier_on(mac->mac_dev);
                        netif_start_queue(mac->mac_dev);

                        if (led_event_action)
                            (*led_event_action)(LEDCTL_VERSION, event_lan1_active, 1);
                        DPRINTF("[%s] '%s' link is up\n",   __FUNCTION__, mac->mac_dev->name);
                    } else {
                        ag7240_intr_disable_tx(mac);

                        netif_carrier_off(mac->mac_dev);
                        netif_stop_queue(mac->mac_dev);
                        if (led_event_action)
                            (*led_event_action)(LEDCTL_VERSION, event_lan1_inactive, 1);
                        DPRINTF("[%s] '%s' link is down\n", __FUNCTION__, mac->mac_dev->name);
                    }
                }
            }
        }
    }

    avm_gpio_enable_irq(irq_handle);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static uint16_t phy11g_read_irq_status(ag7240_mac_t *mac) {

    return ag7240_mdio_read(mac, 0x1a);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static uint16_t phy11g_read_link_status(ag7240_mac_t *mac) {

    return  ag7240_mdio_read(mac, 0x18);
}

/*------------------------------------------------------------------------------------------*\
    (1 << 6)     force speed selection (MSB)
    (1 << 7)     collition test duplex
    (1 << 8)     full duplex
    (1 << 9)     restart autoneg
    (1 << 10)    isolate
    (1 << 11)    power down
    (1 << 12)    autoneg enable
    (1 << 13)    force speed selection (LSB)
    (1 << 14)    loop back
    (1 << 15)    reset
\*------------------------------------------------------------------------------------------*/
static uint32_t phy11g_config(ag7240_mac_t *mac, uint32_t speed, uint32_t duplex) {

    uint16_t value = 0;

    switch (speed) {
        case 0:
            value = (1 << 9) | (1 << 12);   /*--- restart autoneg | autoneg enable ---*/
            break;
        case 10:
            if (duplex)
                value = (1 << 8);           /*--- enable fullduplex ---*/
            break;
        case 100:
            value = (1 << 13);
            if (duplex)
                value |= (1 << 8);
            break;
        case 1000:
            value = (1 << 6);
            if (duplex)
                value |= (1 << 8);
            break;
    }

    ag7240_mdio_write(mac, 0x0, value);

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void phy11g_setup_int(ag7240_mac_t *mac, uint32_t on_off) {

    DPRINTF("[%s] %sable Interrupt\n", __FUNCTION__, on_off ? "en":"dis");
    if (on_off) {
        ag7240_mdio_write(mac, 0x19, (uint16_t)
                (1 << 0) |   /*--- Link State change mask (enable) ---*/
                (1 << 1) |   /*--- Link Speed change mask (enable) ---*/
                (1 << 2) |   /*--- Link Duplex change mask (enable) ---*/
                (1 << 3) |   /*--- Link MDIX change mask (enable) ---*/
                (1 << 4) |   /*--- Link MDI Polarity change mask (enable) ---*/
                /*--- (1 << 5) | ---*/   /*--- Link Speed auto down shift detect mask (enable) ---*/
                /*--- (1 << 10) | ---*/   /*--- auto neg complete mask (enable) ---*/
                /*--- (1 << 11) | ---*/   /*--- auto neg error mask (enable) ---*/
                /*--- (1 << 12) | ---*/   /*--- next page transmit mask (enable) ---*/
                /*--- (1 << 13) | ---*/   /*--- next page receive mask (enable) ---*/
                /*--- (1 << 14) | ---*/   /*--- master slave resolution error mask (enable) ---*/
                /*--- (1 << 15) | ---*/   /*--- wake on LAN Event mask (enable) ---*/
                0);
        avm_gpio_enable_irq(irq_handle);
    } else {
        ag7240_mdio_write(mac, 0x19, (uint16_t)0);
        avm_gpio_disable_irq(irq_handle);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static uint32_t phy11g_init_phy(ag7240_mac_t *mac) {

    uint16_t    phy_id1;

    /*--- release external PHY ---*/
    avm_gpio_out_bit(CONFIG_AR7242_GPIO_RESET_PIN, 1);
    mac->phy_in_reset = 0;

    {
        int i;
        for (i=0; i<10000; i++) {
            phy_id1 = ag7240_mdio_read(mac, 2);
            mdelay(10);
            if (phy_id1 != 0xFFFF) {
                DPRINTF("{%s} phy_id1 0x%x %d\n",__FUNCTION__, phy_id1, i);
                break;
            }
        }
    }

    phy_id1 = ag7240_mdio_read(mac, 2);
    printk("[%s] found PHY 0x%x 0x%x\n", __FUNCTION__, phy_id1, ag7240_mdio_read(mac, 3));

    if (phy_id1 == PHY11G_CHIP_ID) {
        ag7240_mdio_write(mac , 0x0, (uint16_t)
                /*--- (1 << 6) | ---*/   /*--- force speed selection (MSB) ---*/
                /*--- (1 << 7) | ---*/   /*--- collition test duplex ---*/
                (1 << 8) |   /*--- full duplex ---*/
                /*--- (1 << 9) | ---*/   /*--- restart autoneg ---*/
                /*--- (1 << 10) | ---*/   /*--- isolate ---*/
                /*--- (1 << 11) | ---*/   /*--- power down ---*/
                (1 << 12) |   /*--- autoneg enable ---*/
                /*--- (1 << 13) | ---*/   /*--- force speed selection (LSB) ---*/
                /*--- (1 << 14) | ---*/   /*--- loop back ---*/
                (1 << 15) |   /*--- reset ---*/
                0);
        while (ag7240_mdio_read(mac, 0) & (1<<15));    /*--- wait for reset ---*/ 

        ag7240_mdio_write(mac, 0x1d, (uint16_t)0x100);      /*--- spezial Lantiq - TPGDATA = 0x100 ---*/
        return 0;
    }

    return 1;
}

/*------------------------------------------------------------------------------------------*\
 * Head is the first slot with a valid buffer. Tail is the last slot 
 * replenished. Tries to refill buffers from tail to head.
\*------------------------------------------------------------------------------------------*/
static int ag7240_rx_replenish(ag7240_mac_t *mac) {
    ag7240_ring_t   *r     = &mac->mac_rxring;
    int              head  = r->ring_head, tail = r->ring_tail, refilled = 0;
    ag7240_desc_t   *ds;
    ag7240_buffer_t *bf;

    ag7240_trc(head,"hd");
    ag7240_trc(tail,"tl");

    while ( tail != head ) {
        bf = &r->ring_buffer[tail];
        ds = &r->ring_desc[tail];

        ag7240_trc(ds,"ds");

        assert(!ag7240_rx_owned_by_dma(ds));

        assert(!bf->buf_pkt);

        bf->buf_pkt         = ag7240_buffer_alloc();
        if (!bf->buf_pkt) {
            printk(MODULE_NAME ": outta skbs!\n");
            break;
        }
        dma_cache_inv((unsigned long)bf->buf_pkt->data, AG7240_RX_BUF_SIZE);
        ds->pkt_start_addr  = virt_to_phys(bf->buf_pkt->data);

        ag7240_rx_give_to_dma(ds);
        refilled ++;

        ag7240_ring_incr(tail);

    }

    /* Flush descriptors */
    wmb();
#if defined(ATH_USE_AR7240)
    if (is_ar7240()) {
        ag7240_reg_wr(mac,AG7240_MAC_CFG1,(ag7240_reg_rd(mac,AG7240_MAC_CFG1)|0xc));
        ag7240_rx_start(mac);
    }
#endif

    r->ring_tail = tail;
    ag7240_trc(refilled,"refilled");

    return refilled;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ag7240_recv_packets(struct net_device *dev, ag7240_mac_t *mac, int quota, int *work_done) {
    ag7240_ring_t       *r     = &mac->mac_rxring;
    ag7240_desc_t       *ds;
    ag7240_buffer_t     *bp;
    struct sk_buff      *skb;
    ag7240_rx_status_t   ret   = AG7240_RX_STATUS_DONE;
    int head = r->ring_head, len, status, iquota = quota, more_pkts, rep;

    ag7240_trc(iquota,"iquota");
    status = ag7240_reg_rd(mac, AG7240_DMA_RX_STATUS);

process_pkts:
    ag7240_trc(status,"status");

#if 0
    /* Dont assert these bits if the DMA check has passed. The DMA
     * check will clear these bits if a hang condition is detected
     * on resetting the MAC.
     */ 
    if(mac->dma_check == 0) {
        assert((status & AG7240_RX_STATUS_PKT_RCVD));
        assert((status >> 16));
    } else
        mac->dma_check = 0;
#endif

    /* Flush the DDR FIFOs for our gmac */
    ar7240_flush_ge(mac->mac_unit);

    assert(quota > 0); /* WCL */

    while(quota) {
        ds    = &r->ring_desc[head];

        ag7240_trc(head,"hd");
        ag7240_trc(ds,  "ds");

        if (ag7240_rx_owned_by_dma(ds)) {
            /*--- assert(quota != iquota); ---*/ /* WCL */
            if(quota == iquota)
                goto done;
            break;
        }
        ag7240_intr_ack_rx(mac);

        bp                  = &r->ring_buffer[head];
        len                 = ds->pkt_size;
        skb                 = bp->buf_pkt;

        assert(skb);
        skb_put(skb, len - ETHERNET_FCS_SIZE);
        /* 01:00:5e:X:X:X is a multicast MAC address.*/
        if(mac_has_flag(mac,ATHR_S26_HEADER) && (mac->mac_unit == 1)) {
#if 0
            int offset = 14;
            if(ignore_packet_inspection){
                if((skb->data[2]==0x01)&&(skb->data[3]==0x00)&&(skb->data[4]==0x5e)){
        
                    /*If vlan, calculate the offset of the vlan header. */
                    if((skb->data[14]==0x81)&&(skb->data[15]==0x00)){
                        offset +=4;             
                    }

                    /* If pppoe exists, calculate the offset of the pppoe header. */
                    if((skb->data[offset]==0x88)&&((skb->data[offset+1]==0x63)|(skb->data[offset+1]==0x64))){
                        offset += sizeof(struct pppoe_hdr);             
                    }

                    /* Parse IP layer.  */
                    if((skb->data[offset]==0x08)&&(skb->data[offset+1]==0x00)){
                        struct iphdr * iph;
                        struct igmphdr *igh;

                        offset +=2;
                        iph =(struct iphdr *)(skb->data+offset);
                        offset += (iph->ihl<<2);
                        igh =(struct igmphdr *)(skb->data + offset); 

                        /* Sanity Check and insert port information into igmp checksum. */
                        if((iph->ihl > 4 )&&(iph->version==4)&&(iph->protocol==2)){
                            igh->csum = 0x7d50 |(skb->data[0]&0xf);
                            printk("[eth1] Type %x port %x.\n",igh->type,skb->data[0]&0xf);
                        }
            
                    }                               
                }
            }
#endif
            if(mac_has_flag(mac,ATHR_S26_HEADER)) {
                int tos;
                struct ethhdr       *eh;
                unsigned short diffs[2];
     
                if ((skb->data[1] & HDR_PACKET_TYPE_MASK) == NORMAL_PACKET) {
                    /* Default priority */
                    //printk("[R] head ac: %d\n", ((skb->data[1] >> HDR_PRIORITY_SHIFT) & HDR_PRIORITY_MASK));
                    tos = AC_TO_ENET_TOS (((skb->data[1] >> HDR_PRIORITY_SHIFT) & HDR_PRIORITY_MASK));
                    skb_pull(skb, 2);
                    eh = (struct ethhdr *)skb->data;
                    if (eh->h_proto  == __constant_htons(ETHERTYPE_IP)) {
                        struct iphdr *ip = (struct iphdr *) (skb->data + sizeof (struct ethhdr));

                        /* IP frame: exclude ECN bits 0-1 and map DSCP bits 2-7 from TOS byte. */
                        tos = (tos << TOS_ECN_SHIFT ) & TOS_ECN_MASK;
                        if ( tos != ip->tos ) {
                            diffs[0] = htons(ip->tos) ^ 0xFFFF;
                            ip->tos = ((ip->tos &  ~(TOS_ECN_MASK)) | tos);
                            diffs[1] = htons(ip->tos);
                            ip->check = csum_fold(csum_partial((char *)diffs,
                                      sizeof(diffs),ip->check ^ 0xFFFF));
                        }  
                    }
                    skb->priority = tos;
                } else {
                    skb_pull(skb,2);
                }
            }   
        }
#ifdef CONFIG_AR7240_S26_VLAN_IGMP  
        /* For swith S26, if vlan_id == 0, we will remove the tag header. */
        if(mac->mac_unit==1) {
            if((skb->data[12]==0x81) && (skb->data[13]==0x00) && ((skb->data[14] & 0xf)==0x00) && (skb->data[15]==0x00)) {
                memmove(&skb->data[4], &skb->data[0], 12);
                skb_pull(skb,4);
            }
        }  
#endif

        mac->net_rx_packets ++;
        mac->net_rx_bytes += skb->len;

        /* also pulls the ether header */
        skb->protocol       = eth_type_trans(skb, dev);
        skb->dev            = dev;
        bp->buf_pkt         = NULL;
        dev->last_rx        = jiffies;

        quota--;
        netif_receive_skb(skb);
        ag7240_ring_incr(head);
    }

    assert(iquota != quota);
    r->ring_head   =  head;

    rep = ag7240_rx_replenish(mac);

    /*
    * let's see what changed while we were slogging.
    * ack Rx in the loop above is no flush version. It will get flushed now.
    */
    status       =  ag7240_reg_rd(mac, AG7240_DMA_RX_STATUS);
    more_pkts    =  (status & AG7240_RX_STATUS_PKT_RCVD);

    ag7240_trc(more_pkts,"more_pkts");

    if (!more_pkts) goto done;

    /* more pkts arrived; if we have quota left, get rolling again */
    if (quota)      goto process_pkts;

    /* out of quota */
    ret = AG7240_RX_STATUS_NOT_DONE;

done:
    *work_done   = (iquota - quota);

    if (unlikely(ag7240_rx_ring_full(mac))) 
        return AG7240_RX_STATUS_OOM;

    /* !oom; if stopped, restart h/w */
    if (unlikely(status & AG7240_RX_STATUS_OVF)) {
        mac->net_rx_over_errors ++;
        ag7240_intr_ack_rxovf(mac);
        ag7240_rx_start(mac);
    }

    return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ag7240_poll(struct napi_struct *napi, int budget) {

    ag7240_mac_t *mac = container_of(napi, ag7240_mac_t, mac_napi);
    struct net_device *dev = mac->mac_dev;
    int work_done,      max_work  = budget;
    ag7240_rx_status_t  ret;

    /*--- DPRINTF("{%s}\n", __FUNCTION__); ---*/

    ret = ag7240_recv_packets(dev, mac, max_work, &work_done);

    /*--- DPRINTF("{%s} ret 0x%x\n", __FUNCTION__, ret); ---*/
    if (work_done < budget) {
        napi_complete(napi); /* vermutlich frueher netif_rx_complete */
        ag7240_intr_enable_recv(mac);
    }

    if (likely(ret == AG7240_RX_STATUS_NOT_DONE)) {
        printk("[%s] work left\n", __FUNCTION__);   /* We have work left */

    } else if (ret == AG7240_RX_STATUS_OOM) {
        printk(MODULE_NAME ": oom..?\n");

        /* Start timer, stop polling, but do not enable rx interrupts. */
        /*--- mod_timer(&mac->mac_oom_timer, jiffies+1); ---*/
    }

    return work_done;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct sk_buff * ag7240_buffer_alloc(void) {
    struct sk_buff *skb;

    skb = dev_alloc_skb(AG7240_RX_BUF_SIZE + AG7242_RX_HW_WORKAROUND_PAD);
    if (unlikely(!skb))
        return NULL;
    skb_reserve(skb, AG7240_RX_RESERVE);

    return skb;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ag7240_buffer_free(struct sk_buff *skb) {
    if (in_irq())
        dev_kfree_skb_irq(skb);
    else
        dev_kfree_skb(skb);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ag7240_ring_free(ag7240_ring_t *r) {
    dma_free_coherent(NULL, sizeof(ag7240_desc_t)*r->ring_nelem, r->ring_desc,
        r->ring_desc_dma);
    kfree(r->ring_buffer);
    DPRINTF("%s Freeing at 0x%lx\n",__func__,(unsigned long) r->ring_buffer);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ag7240_ring_release(ag7240_mac_t *mac, ag7240_ring_t  *r) {
    int i;

    for(i = 0; i < r->ring_nelem; i++) {
        if (r->ring_buffer[i].buf_pkt) {
            ag7240_buffer_free(r->ring_buffer[i].buf_pkt);
        }
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ag7240_tx_free(ag7240_mac_t *mac) {
    int ac;
    
    for(ac = 0;ac < mac->mac_noacs; ac++) {
        ag7240_ring_release(mac, &mac->mac_txring[ac]);
        ag7240_ring_free(&mac->mac_txring[ac]);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ag7240_rx_free(ag7240_mac_t *mac) {
    ag7240_ring_release(mac, &mac->mac_rxring);
    ag7240_ring_free(&mac->mac_rxring);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ag7240_ring_alloc(ag7240_ring_t *r, int count) {
    int desc_alloc_size, buf_alloc_size;

    desc_alloc_size = sizeof(ag7240_desc_t)   * count;
    buf_alloc_size  = sizeof(ag7240_buffer_t) * count;

    memset(r, 0, sizeof(ag7240_ring_t));

    r->ring_buffer = (ag7240_buffer_t *)kmalloc(buf_alloc_size, GFP_KERNEL);
    DPRINTF("%s Allocated %d at 0x%lx\n",__func__,buf_alloc_size,(unsigned long) r->ring_buffer);
    if (!r->ring_buffer) {
        printk(MODULE_NAME ": unable to allocate buffers\n");
        return 1;
    }

    r->ring_desc  =  (ag7240_desc_t *)dma_alloc_coherent(NULL, 
        desc_alloc_size,
        &r->ring_desc_dma, 
        GFP_DMA);
    if (! r->ring_desc) {
        printk(MODULE_NAME ": unable to allocate coherent descs\n");
        kfree(r->ring_buffer);
        DPRINTF("%s Freeing at 0x%lx\n",__func__,(unsigned long) r->ring_buffer);
        return 1;
    }

    memset(r->ring_buffer, 0, buf_alloc_size);
    memset(r->ring_desc,   0, desc_alloc_size);
    r->ring_nelem   = count;

    return 0;
}

/*------------------------------------------------------------------------------------------*\
 * allocate and init rings, descriptors etc.
\*------------------------------------------------------------------------------------------*/
static int ag7240_tx_alloc(ag7240_mac_t *mac) {
    ag7240_ring_t *r ;
    ag7240_desc_t *ds;
    int ac, i, next;

    for(ac = 0;ac < mac->mac_noacs; ac++) {

       r  = &mac->mac_txring[ac];
       if (ag7240_ring_alloc(r, AG7240_TX_DESC_CNT))
           return 1;

       ag7240_trc(r->ring_desc,"ring_desc");

       ds = r->ring_desc;
       for(i = 0; i < r->ring_nelem; i++ ) {
            ag7240_trc_new(ds,"tx alloc ds");
            next                =   (i == (r->ring_nelem - 1)) ? 0 : (i + 1);
            ds[i].next_desc     =   ag7240_desc_dma_addr(r, &ds[next]);
            ag7240_tx_own(&ds[i]);
       }
   }
   return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ag7240_rx_alloc(ag7240_mac_t *mac) {
    ag7240_ring_t *r  = &mac->mac_rxring;
    ag7240_desc_t *ds;
    int i, next, tail = r->ring_tail;
    ag7240_buffer_t *bf;

    if (ag7240_ring_alloc(r, AG7240_RX_DESC_CNT))
        return 1;

    ag7240_trc(r->ring_desc,"ring_desc");

    ds = r->ring_desc;
    for(i = 0; i < r->ring_nelem; i++ ) {
        next                =   (i == (r->ring_nelem - 1)) ? 0 : (i + 1);
        ds[i].next_desc     =   ag7240_desc_dma_addr(r, &ds[next]);
    }

    for (i = 0; i < AG7240_RX_DESC_CNT; i++) {
        bf                  = &r->ring_buffer[tail];
        ds                  = &r->ring_desc[tail];

        bf->buf_pkt         = ag7240_buffer_alloc();
        if (!bf->buf_pkt) 
            goto error;

        dma_cache_inv((unsigned long)bf->buf_pkt->data, AG7240_RX_BUF_SIZE);
        ds->pkt_start_addr  = virt_to_phys(bf->buf_pkt->data);

        ds->res1           = 0;
        ds->res2           = 0;
        ds->ftpp_override  = 0;
        ds->res3           = 0;
        ds->more    = 0;
        ag7240_rx_give_to_dma(ds);
        ag7240_ring_incr(tail);
    }

    return 0;
error:
    printk(MODULE_NAME ": unable to allocate rx\n");
    ag7240_rx_free(mac);
    return 1;
}

/*------------------------------------------------------------------------------------------*\
 * Reap from tail till the head or whenever we encounter an unxmited packet.
\*------------------------------------------------------------------------------------------*/
static int ag7240_tx_reap(ag7240_mac_t *mac,int ac) {
    ag7240_ring_t   *r;
    ag7240_desc_t   *ds;
    ag7240_buffer_t *bf;
    int head, tail, reaped = 0, i;
    unsigned long flags;

    r = &mac->mac_txring[ac];
    head = r->ring_head;
    tail = r->ring_tail;

    ag7240_trc_new(head,"hd");
    ag7240_trc_new(tail,"tl");

    ar7240_flush_ge(mac->mac_unit);

    while(tail != head) {
        ds   = &r->ring_desc[tail];

        ag7240_trc_new(ds,"ds");

        if(ag7240_tx_owned_by_dma(ds))
            break;

        bf      = &r->ring_buffer[tail];
        assert(bf->buf_pkt);

        ag7240_trc_new(bf->buf_lastds,"lastds");

        if(ag7240_tx_owned_by_dma(bf->buf_lastds))
            break;

        for(i = 0; i < bf->buf_nds; i++) {
            ag7240_intr_ack_tx(mac);
            ag7240_ring_incr(tail);
        }

        ag7240_buffer_free(bf->buf_pkt);
        bf->buf_pkt = NULL;

        reaped ++;
    }

    r->ring_tail = tail;

    if (netif_queue_stopped(mac->mac_dev) &&
        (ag7240_ndesc_unused(mac, r) >= AG7240_TX_QSTART_THRESH) &&
        netif_carrier_ok(mac->mac_dev))
    {
        if (ag7240_reg_rd(mac, AG7240_DMA_INTR_MASK) & AG7240_INTR_TX) {
            spin_lock_irqsave(&mac->mac_lock, flags);
            ag7240_intr_disable_tx(mac);
            spin_unlock_irqrestore(&mac->mac_lock, flags);
        }
        netif_wake_queue(mac->mac_dev);
    }

    return reaped;
}

/*
 * Interrupt handling:
 * - Recv NAPI style (refer to Documentation/networking/NAPI)
 *
 *   2 Rx interrupts: RX and Overflow (OVF).
 *   - If we get RX and/or OVF, schedule a poll. Turn off _both_ interurpts. 
 *
 *   - When our poll's called, we
 *     a) Have one or more packets to process and replenish
 *     b) The hardware may have stopped because of an OVF.
 *
 *   - We process and replenish as much as we can. For every rcvd pkt 
 *     indicated up the stack, the head moves. For every such slot that we
 *     replenish with an skb, the tail moves. If head catches up with the tail
 *     we're OOM. When all's done, we consider where we're at:
 *
 *      if no OOM:
 *      - if we're out of quota, let the ints be disabled and poll scheduled.
 *      - If we've processed everything, enable ints and cancel poll.
 *
 *      If OOM:
 *      - Start a timer. Cancel poll. Ints still disabled. 
 *        If the hardware's stopped, no point in restarting yet. 
 *
 *      Note that in general, whether we're OOM or not, we still try to
 *      indicate everything recvd, up.
 *
 * Locking: 
 * The interrupt doesnt touch the ring => Rx is lockless
 *
 */
static irqreturn_t ag7240_intr(int cpl, void *dev_id) {

    struct net_device *dev  = (struct net_device *)dev_id;
    ag7240_mac_t *mac = (ag7240_mac_t *) netdev_priv(dev);
    int   isr, imr, ac, handled = 0;

    isr   = ag7240_get_isr(mac);
    imr   = ag7240_reg_rd(mac, AG7240_DMA_INTR_MASK);

    ag7240_trc(isr,"isr");
    ag7240_trc(imr,"imr");

    /*--- DPRINTF("{%s} isr 0x%x imr 0x%x\n", __FUNCTION__, isr, imr); ---*/

    assert(isr == (isr & imr));
    if (isr & (AG7240_INTR_RX_OVF)) {
        handled = 1;

#if defined(ATH_USE_AR7240)
        if (is_ar7240()) 
            ag7240_reg_wr(mac,AG7240_MAC_CFG1,(ag7240_reg_rd(mac,AG7240_MAC_CFG1) & 0xfffffff3));
#endif

        ag7240_intr_ack_rxovf(mac);
    }
    if (likely(isr & AG7240_INTR_RX)) {
        handled = 1;

        if (napi_schedule_prep( &mac->mac_napi )) {
            /*--- DPRINTF("{%s} disable Ints\n", __FUNCTION__); ---*/
            ag7240_intr_disable_recv(mac);
            __napi_schedule( &mac->mac_napi );
        } else {
            printk(MODULE_NAME ": driver bug! interrupt while in poll\n");
            assert(0);
            ag7240_intr_disable_recv(mac);
        }
    }
    if (likely(isr & AG7240_INTR_TX)) {
        handled = 1;
        ag7240_intr_ack_tx(mac);
        /* Which queue to reap ??????????? */
        for(ac = 0; ac < mac->mac_noacs; ac++)
            ag7240_tx_reap(mac,ac);
    }
    if (unlikely(isr & AG7240_INTR_RX_BUS_ERROR)) {
        assert(0);
        handled = 1;
        ag7240_intr_ack_rxbe(mac);
    }
    if (unlikely(isr & AG7240_INTR_TX_BUS_ERROR)) {
        assert(0);
        handled = 1;
        ag7240_intr_ack_txbe(mac);
    }

    if (!handled) {
        assert(0);
        printk(MODULE_NAME ": unhandled intr isr %#x\n", isr);
    }

    /*--- DPRINTF("[%s] return IRQ_HANDLED\n", __FUNCTION__); ---*/
    return IRQ_HANDLED;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ag7240_hw_stop(ag7240_mac_t *mac) {
    ag7240_rx_stop(mac);
    ag7240_tx_stop(mac);
    ag7240_int_disable(mac);

    /*
     * put everything into reset.
     * Dont Reset WAN MAC as we are using eth0 MDIO to access S26 Registers.
     */
    if(mac->mac_unit == 1 || is_ar7241() || is_ar7242())
        ag7240_reg_rmw_set(mac, AG7240_MAC_CFG1, AG7240_MAC_CFG1_SOFT_RST);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ag7240_hw_setup(ag7240_mac_t *mac) {
    ag7240_ring_t *tx, *rx = &mac->mac_rxring;
    ag7240_desc_t *r0, *t0;
    uint32_t w1 = 0, w2 = 0;
    uint32_t ac;
    struct net_device      *dev = mac->mac_dev;

    DPRINTF("[%s] mac->mac_unit %d\n", __FUNCTION__, mac->mac_unit); 
    if(mac->mac_unit) {
        ag7240_reg_wr(mac, AG7240_MAC_CFG1, (AG7240_MAC_CFG1_RX_EN | AG7240_MAC_CFG1_TX_EN | AG7240_MAC_CFG1_RX_FCTL | AG7240_MAC_CFG1_TX_FCTL));
        ag7240_reg_rmw_set(mac, AG7240_MAC_CFG2, (AG7240_MAC_CFG2_PAD_CRC_EN | AG7240_MAC_CFG2_LEN_CHECK | AG7240_MAC_CFG2_IF_1000));
    } else {
        ag7240_reg_wr(mac, AG7240_MAC_CFG1, (AG7240_MAC_CFG1_RX_EN | AG7240_MAC_CFG1_TX_EN));
        ag7240_reg_rmw_set(mac, AG7240_MAC_CFG2, (AG7240_MAC_CFG2_PAD_CRC_EN | AG7240_MAC_CFG2_LEN_CHECK));
    }
    ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_0, 0x1f00);

    /* Disable AG7240_MAC_CFG2_LEN_CHECK to fix the bug that 
     * the mac address is mistaken as length when enabling Atheros header 
     */
    if (mac_has_flag(mac,ATHR_S26_HEADER) || mac_has_flag(mac,ATHR_S16_HEADER))
        ag7240_reg_rmw_clear(mac, AG7240_MAC_CFG2, AG7240_MAC_CFG2_LEN_CHECK)

    if ((mac->mac_unit == 0) && is_ar7242()) {        /* External MII mode */
        ar7240_reg_rmw_set(AG7240_ETH_CFG, AG7240_ETH_CFG_RGMII_GE0); 
    }

    /*--- set the mac addr ---*/
    addr_to_words(dev->dev_addr, w1, w2);
    ag7240_reg_wr(mac, AG7240_GE_MAC_ADDR1, w1);
    ag7240_reg_wr(mac, AG7240_GE_MAC_ADDR2, w2);

    ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_1, 0x10ffff);
    ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_2, 0x015500aa);

    /*------------------------------------------------------------------------------------------*\
     * Weed out junk frames (CRC errored, short collision'ed frames etc.)
    \*------------------------------------------------------------------------------------------*/
    ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_4, 0x3ffff);

    /*------------------------------------------------------------------------------------------*\
     * Drop CRC Errors, Pause Frames ,Length Error frames, Truncated Frames dribble nibble and rxdv error frames.
    \*------------------------------------------------------------------------------------------*/
    /*--- DPRINTF("Setting Drop CRC Errors, Pause Frames and Length Error frames \n"); ---*/
    if (mac_has_flag(mac,ATHR_S26_HEADER)) {
        ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_5, 0xe6bc0);
    } else {
        ag7240_reg_wr(mac, AG7240_MAC_FIFO_CFG_5, 0x66b82);
    }

    if (mac_has_flag(mac,WAN_QOS_SOFT_CLASS)) {
        /* Enable Fixed priority */
        ag7240_reg_wr(mac,AG7240_DMA_TX_ARB_CFG,AG7240_TX_QOS_MODE_WEIGHTED
                | AG7240_TX_QOS_WGT_0(0x7)
                | AG7240_TX_QOS_WGT_1(0x5) 
                | AG7240_TX_QOS_WGT_2(0x3)
                | AG7240_TX_QOS_WGT_3(0x1));

        for(ac = 0;ac < mac->mac_noacs; ac++) {
            tx = &mac->mac_txring[ac];
            t0  =  &tx->ring_desc[0];
            switch(ac) {
                case ENET_AC_VO:
                    ag7240_reg_wr(mac, AG7240_DMA_TX_DESC_Q0, ag7240_desc_dma_addr(tx, t0));
                    break;
                case ENET_AC_VI:
                    ag7240_reg_wr(mac, AG7240_DMA_TX_DESC_Q1, ag7240_desc_dma_addr(tx, t0));
                    break;
                case ENET_AC_BK:
                    ag7240_reg_wr(mac, AG7240_DMA_TX_DESC_Q2, ag7240_desc_dma_addr(tx, t0));
                    break;
                case ENET_AC_BE:
                    ag7240_reg_wr(mac, AG7240_DMA_TX_DESC_Q3, ag7240_desc_dma_addr(tx, t0));
                    break;
            }
        }
    } else {
        tx = &mac->mac_txring[0];
        t0  =  &tx->ring_desc[0];
        ag7240_reg_wr(mac, AG7240_DMA_TX_DESC_Q0, ag7240_desc_dma_addr(tx, t0));
    }

    r0  =  &rx->ring_desc[0];
    ag7240_reg_wr(mac, AG7240_DMA_RX_DESC, ag7240_desc_dma_addr(rx, r0));

    DPRINTF(": cfg1 %#x cfg2 %#x\n", ag7240_reg_rd(mac, AG7240_MAC_CFG1), ag7240_reg_rd(mac, AG7240_MAC_CFG2));
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ag7240_open(struct net_device *dev) {

    ag7240_mac_t *mac = netdev_priv(dev);
    int status;

    DPRINTF("[%s] irq %d %s\n", __FUNCTION__, mac->mac_irq, dev->name);

    assert(mac);
    if (mac_has_flag(mac,WAN_QOS_SOFT_CLASS))
        mac->mac_noacs = 4;
    else
        mac->mac_noacs = 1;

    status = request_irq(mac->mac_irq, ag7240_intr, IRQF_DISABLED, dev->name, dev);
    if (status < 0) {
        printk(MODULE_NAME ": request irq %d failed %d\n", mac->mac_irq, status);
        return 1;
    }
    if (ag7240_tx_alloc(mac)) goto tx_failed;
    if (ag7240_rx_alloc(mac)) goto rx_failed;

    mac->mac_speed = -1;

    ag7240_hw_setup(mac);

    if (mac->init_phy(mac)) {
        printk(KERN_ERR "[%s] ERROR no PHY found\n", __FUNCTION__);
        avm_gpio_out_bit(CONFIG_AR7242_GPIO_RESET_PIN, 0);
        free_irq(mac->mac_irq, dev);
        return -EFAULT;
    }

    mac->config_phy(mac, 0, 0);     /*--- config autoneg ---*/
    mac->setup_int(mac, 1);

    /*--- dev->trans_start = jiffies; ---*/

    /* Keep carrier off while initialization and switch it once the link is up.  */
    netif_carrier_off(dev);
    netif_stop_queue(dev);
 
    mac->mac_ifup = 1;
    ag7240_int_enable(mac);

#if defined(ATH_USE_AR7240)
    if (is_ar7240()) {
        ag7240_intr_enable_rxovf(mac);
    }
#endif

    ag7240_rx_start(mac);

    return 0;

rx_failed:
    ag7240_tx_free(mac);
tx_failed:
    free_irq(mac->mac_irq, dev);
    return 1;

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ag7240_stop(struct net_device *dev) {

    ag7240_mac_t *mac = netdev_priv(dev);
    unsigned long flags;

    spin_lock_irqsave(&mac->mac_lock, flags);
    mac->mac_ifup = 0;
    netif_stop_queue(dev);
    netif_carrier_off(dev);

    ag7240_hw_stop(mac);
    free_irq(mac->mac_irq, dev);

    spin_unlock_irqrestore(&mac->mac_lock, flags);

    ag7240_tx_free(mac);
    ag7240_rx_free(mac);

    avm_gpio_disable_irq(irq_handle);
    avm_gpio_out_bit(CONFIG_AR7242_GPIO_RESET_PIN, 0);
    mac->phy_in_reset = 1;

#if 0
    /* During interface up on PHY reset the link status will be updated.
    * Clearing the software link status while bringing the interface down
    * will avoid the race condition during PHY RESET.
    */
    if (mac_has_flag(mac,ETH_SOFT_LED)) {
        if (mac->mac_unit == ENET_UNIT_LAN) {
            for(i = 0;i < 4; i++)
                PLedCtrl.ledlink[i] = 0;
        }
        else {
            PLedCtrl.ledlink[4] = 0;
        }
    }
    if (is_ar7242())
        del_timer(&mac->mac_phy_timer);
#endif

    /*ag7240_trc_dump();*/

    return 0;

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static const struct net_device_ops netdev_ops = {
    .ndo_open            =  ag7240_open,
    .ndo_get_stats       =  ag7240_get_stats,
    .ndo_stop            =  ag7240_stop,
    .ndo_start_xmit      =  ag7240_hard_start,
    .ndo_do_ioctl        =  ag7240_do_ioctl,
    .ndo_tx_timeout      =  ag7240_tx_timeout,
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init ag7240_init(void) {

    int i;
    struct net_device *netdev;
    ag7240_mac_t      *mac;
    uint32_t mask;

    /* 
    * tx_len_per_ds is the number of bytes per data transfer in word increments.
    * 
    * If the value is 0 than we default the value to a known good value based
    * on benchmarks. Otherwise we use the value specified - within some 
    * cosntraints of course.
    *
    * Tested working values are 256, 512, 768, 1024 & 1536.
    *
    * A value of 256 worked best in all benchmarks. That is the default.
    *
    */

    DPRINTF("[%s]\n", __FUNCTION__);

    /* Tested 256, 512, 768, 1024, 1536 OK, 1152 and 1280 failed*/
    if (0 == tx_len_per_ds)
        tx_len_per_ds = CONFIG_AG7240_LEN_PER_TX_DS;

    ag7240_vet_tx_len_per_pkt( &tx_len_per_ds);
    printk(MODULE_NAME ": Length per segment %d\n", tx_len_per_ds);

    /*------------------------------------------------------------------------------------------*\
     * Compute the number of descriptors for an MTU 
    \*------------------------------------------------------------------------------------------*/
    tx_max_desc_per_ds_pkt =1;

    printk(MODULE_NAME ": Max segments per packet %d\n", tx_max_desc_per_ds_pkt);
    printk(MODULE_NAME ": Max tx descriptor count    %d\n", AG7240_TX_DESC_CNT);
    printk(MODULE_NAME ": Max rx descriptor count    %d\n", AG7240_RX_DESC_CNT);

    /*------------------------------------------------------------------------------------------*\
     * Let hydra know how much to put into the fifo in words (for tx) 
    \*------------------------------------------------------------------------------------------*/
    if (0 == fifo_3)
        fifo_3 = 0x000001ff | ((AG7240_TX_FIFO_LEN-tx_len_per_ds)/4)<<16;
    DPRINTF(MODULE_NAME ": fifo cfg 3 %08x\n", fifo_3);

    /*----------------------------------------------------------------------*\
     * Do the rest of the initializations 
    \*----------------------------------------------------------------------*/
    for(i = 0; i < AG7240_NMACS; i++) {

        netdev = alloc_etherdev(sizeof(ag7240_mac_t));
        if (!netdev) {
            printk(MODULE_NAME ": unable to allocate etherdev\n");
            return 1;
        }

        mac = netdev_priv(netdev); 

        memset(mac, 0, sizeof(ag7240_mac_t));

        mac->mac_unit               =  i;
        mac->mac_base               =  ag7240_mac_base(i);
        mac->mac_irq                =  ag7240_mac_irq(i);
        mac->mac_noacs              =  1;
        ag7240_macs[i]              =  mac;

        mac->read_phy_int_status    = phy11g_read_irq_status;
        mac->read_phy_link_status   = phy11g_read_link_status;
        mac->setup_int              = phy11g_setup_int;
        mac->config_phy             = phy11g_config;
        mac->init_phy               = phy11g_init_phy;

        if (i == 0) {
            mac->phy_addr           = 0;
            mac->phy_in_reset       = 1;
        } else {
            mac->phy_addr           = (-1);
        }

#ifdef CONFIG_ATHEROS_HEADER_EN
        if (mac->mac_unit == ENET_UNIT_LAN)    
    	    mac_set_flag(mac,ATHR_S26_HEADER);
#endif 
#ifdef CONFIG_ETH_SOFT_LED
#if defined(ATH_USE_AR7240)
        if (is_ar7240())
            mac_set_flag(mac,ETH_SOFT_LED);
#endif
#endif
#ifdef CONFIG_CHECK_DMA_STATUS 
        mac_set_flag(mac,CHECK_DMA_STATUS);
#endif

#ifdef CONFIG_S26_SWITCH_ONLY_MODE
        if (is_ar7241()) {
            if(i) {
                mac_set_flag(mac,ETH_SWONLY_MODE);
            } else {
                continue;
            }
        }
#endif
        spin_lock_init(&mac->mac_lock);

#if 0       /*--- TODO ---*/
        /* out of memory timer */
        init_timer(&mac->mac_oom_timer);
        mac->mac_oom_timer.data     = (unsigned long)mac;
        mac->mac_oom_timer.function = (void *)ag7240_oom_timer;
#endif

#if 0   /*--- Haiko ---*/
        /* watchdog task */
        INIT_WORK(&mac->mac_tx_timeout, ag7240_tx_timeout_task);
#endif

        mask = ag7240_reset_mask(mac->mac_unit);    /*--- reset GEx ---*/ 
        ar7240_reg_rmw_set(AR7240_RESET, mask);
        mdelay(100);
        ar7240_reg_rmw_clear(AR7240_RESET, mask);
        mdelay(100);

        mac->mac_dev            =  netdev;
        netdev->netdev_ops      = &netdev_ops;

        netif_napi_add(netdev, &mac->mac_napi, ag7240_poll, AG7240_NAPI_WEIGHT);
        napi_enable(&mac->mac_napi);

        ag7240_get_macaddr(mac, netdev->dev_addr);

        if (register_netdev(netdev)) {
            printk(MODULE_NAME ": register netdev failed\n");
            goto failed;
        }

#ifdef CONFIG_ATHRS_QOS
        athr_register_qos(mac);
#endif

        netif_carrier_off(netdev);
        ag7240_reg_rmw_set(mac, AG7240_MAC_CFG1, AG7240_MAC_CFG1_SOFT_RST | AG7240_MAC_CFG1_RX_RST | AG7240_MAC_CFG1_TX_RST);

        udelay(20);
        mask = ag7240_reset_mask(mac->mac_unit);

    }

    ag7240_reg_wr(mac, AG7240_MAC_MII_MGMT_CFG, 6 | (1 << 31));     /*--- enable MDIO ---*/
    ag7240_reg_wr(mac, AG7240_MAC_MII_MGMT_CFG, 6);

    avm_gpio_ctrl(CONFIG_AR7242_MDIOINT_PIN, GPIO_PIN, GPIO_INPUT_PIN);      /*--- configure MDIO-INT Pin ---*/
    avm_gpio_ctrl(CONFIG_AR7242_GPIO_RESET_PIN, GPIO_PIN, GPIO_OUTPUT_PIN); 
    avm_gpio_out_bit(CONFIG_AR7242_GPIO_RESET_PIN, 0);                       /*--- reset external PHY ---*/
    ag7240_macs[0]->phy_in_reset = 1;

    mii_gphy_workqueue = create_singlethread_workqueue("mii_gphy_workqueue");
    if(mii_gphy_workqueue == NULL) {
        printk(KERN_ERR "[%s] create_singlethread_workqueue(\"mii_gphy_workqueue\" failed\n", __FUNCTION__);
        return -EFAULT;
    }

    irq_handle = avm_gpio_request_irq( 1 << CONFIG_AR7242_MDIOINT_PIN, GPIO_ACTIVE_LOW, GPIO_EDGE_SENSITIVE, mii_gphy_int_handler);
    if (!irq_handle) {
        printk(KERN_ERR "[%s] ERROR cannot get ext irq!\n", __FUNCTION__);
        return -EFAULT;
    }

    avm_gpio_disable_irq(irq_handle);
    ag7240_trc_init();

#if 0
    athrs_reg_dev(ag7240_macs);

    if (mac_has_flag(mac,CHECK_DMA_STATUS))
        prev_dma_chk_ts = jiffies;

    if (mac_has_flag(mac,ETH_SOFT_LED)) {
        init_timer(&PLedCtrl.led_timer);
        PLedCtrl.led_timer.data     = (unsigned long)(&PLedCtrl);
        PLedCtrl.led_timer.function = (void *)led_control_func;
        mod_timer(&PLedCtrl.led_timer,(jiffies + AG7240_LED_POLL_SECONDS));
    }
#endif

    return 0;

failed:

    for(i = 0; i < AG7240_NMACS; i++) {
#ifdef CONFIG_S26_SWITCH_ONLY_MODE
        if (is_ar7241() && i == 0)
            continue;
#endif
        if (!ag7240_macs[i]) 
            continue;
        if (ag7240_macs[i]->mac_dev) 
            free_netdev(ag7240_macs[i]->mac_dev);
        kfree(ag7240_macs[i]);
        DPRINTF("%s Freeing at 0x%lx\n",__func__,(unsigned long) ag7240_macs[i]);
    }
    return 1;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void __exit ag7240_cleanup(void) {
    int i;

    /*--- free_irq(AR7240_MISC_IRQ_ENET_LINK, ag7240_macs[0]->mac_dev); ---*/
    destroy_workqueue(mii_gphy_workqueue);
    avm_gpio_disable_irq(irq_handle);
    for(i = 0; i < AG7240_NMACS; i++) {
        if (!ag7240_macs[i])
            continue;
        if (mac_has_flag(ag7240_macs[i],ETH_SWONLY_MODE) && i == 0) {
            kfree(ag7240_macs[i]);
            continue;
        }
        unregister_netdev(ag7240_macs[i]->mac_dev);
        free_netdev(ag7240_macs[i]->mac_dev);
        kfree(ag7240_macs[i]);
        DPRINTF("%s Freeing at 0x%lx\n", __FUNCTION__ ,(unsigned long) ag7240_macs[i]);
    }
#if 0
    if (mac_has_flag(ag7240_macs[0],ETH_SOFT_LED)) 
        del_timer(&PLedCtrl.led_timer);
#endif
    printk(MODULE_NAME ": cleanup done\n");
}

module_init(ag7240_init);
module_exit(ag7240_cleanup);
