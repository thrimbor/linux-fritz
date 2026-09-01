/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,...,2013 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*------------------------------------------------------------------------------------------*/

#include <stdarg.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/proc_fs.h>
#include <linux/env.h>
#include <asm/io.h>
#if !defined(CONFIG_NETCHIP_ADM69961)
#define CONFIG_NETCHIP_ADM69961
#endif
#include <linux/avm_event.h>
#ifdef CONFIG_AVM_RTP_TIMESTAMP
#include <linux/rtp_timestamp.h>
#endif
#include "cpmac_if.h"
#include "cpmac_const.h"
#include "cpmac_debug.h"
#include "cpmac_main.h"
#include "cpphy_const.h"
#include "cpphy_types.h"
#include "cpphy_mgmt.h"   /* for cpphy_mgmt_deinit */
#include "cpmac_eth.h"    /* for struct cpmac_devinfo */
#include "cpphy_main.h"   /* for cpphy_main_config_g_apply */
#include "cpphy_ar8216.h"
#if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0)
#if defined(CONFIG_MIPS_UR8)
#include <asm/mach-ur8/hw_nwss.h>
#include "cpphy_if.h"
#include "cpphy_if_g.h"
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/
#endif /*--- #if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0) ---*/

extern dev_desc_t g_dev_array[AVM_CPMAC_MAX_PHYS];
unsigned int cpmac_devices_installed = 0;

#if defined(CONFIG_IP_MULTICAST_FASTFORWARD)
extern struct mcfw_netdriver *cpmac_mcfw_netdrv;
extern int cpmac_mcfw_sourceid;
#endif /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_register_wan_receive(cpmac_fast_forward_rx_func_ptr wan_receive) {
    int irqs_are_enabled = !irqs_disabled();
    if(irqs_are_enabled) {
        local_bh_disable();
    }
    DEB_INFO("[%s] function %p\n", __FUNCTION__, wan_receive);
    cpmac_global.cpphy[0].mdio.switch_config.wan_receive_function = wan_receive;
    cpphy_switch_register_ffw_port(wan_receive, cpmac_global.cpphy[0].mdio.switch_config.wanport);
    if(irqs_are_enabled) {
        local_bh_enable();
    }
}
EXPORT_SYMBOL(cpmac_register_wan_receive);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_unregister_wan_receive(void) {
    int irqs_are_enabled = !irqs_disabled();
    if(irqs_are_enabled) {
        local_bh_disable();
    }
    DEB_INFO("[%s]\n", __FUNCTION__);
    cpmac_global.cpphy[0].mdio.switch_config.wan_receive_function = 0;
    cpphy_switch_unregister_ffw_port(cpmac_global.cpphy[0].mdio.switch_config.wanport);
    if(irqs_are_enabled) {
        local_bh_enable();
    }
}
EXPORT_SYMBOL(cpmac_unregister_wan_receive);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CPMAC_DMA_RX_PRIOQUEUE_DEBUG)
static char buf[60];
static int bufidx = 0;

static void log_from_dmaqueue(int wan) {
    if(bufidx == sizeof(buf) - 1) {
        buf[bufidx] = 0;
        DEB_INFO("cpmac: rx %s\n", buf);
        bufidx = 0;
    }
    buf[bufidx++] = '0' + (wan ? 1 : 0);
}
#endif /*--- #if defined(CPMAC_DMA_RX_PRIOQUEUE_DEBUG) ---*/


#if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline cpmac_err_t cpmac_if_check_mac_handle(cpmac_priv_t *cpmac_priv) {
    cpmac_err_t ret = CPMAC_ERR_NOERR;

    if(!cpmac_priv || !g_dev_array[cpmac_priv->inst].p_dev) {
        ret = CPMAC_ERR_ILL_HANDLE;
    }
    return ret;
}


/*------------------------------------------------------------------------------------------*\
 * Receive data from PHY
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpmac_if_data_from_phy(cpmac_priv_t   *cpmac_priv, 
                                   struct sk_buff *skb,
                                   unsigned int    length) {
    cpmac_err_t ret = CPMAC_ERR_NOERR;
    int is_wan = 0;
    unsigned int port = 1; /* Default for "silly" switches (e.g. 94.0.0.0 */
    cpphy_mdio_t *mdio = cpmac_priv->cppi->mdio;

    if(cpmac_if_check_mac_handle(cpmac_priv) != CPMAC_ERR_NOERR) {
        DEB_ERR("[%s] ill mac_handle %p\n", __FUNCTION__, cpmac_priv);
        ret = CPMAC_ERR_ILL_HANDLE;
    } else {
        struct net_device *p_dev = cpmac_priv->owner;

        /* invalidate the cache: data written by dma */
        dma_cache_inv((unsigned long) skb->data, skb->len);

        /* Strip potential padding */
        __skb_trim(skb, length);
        skb->ip_summed = CHECKSUM_NONE;
        DEB_DEBUG("[%s] (%u) %*pB ...\n", __FUNCTION__, skb->len, min(24u, skb->len), skb->data);

        /* Discard big packets and packets with length 0 */
        if(   (length > 1522)
           || (   (length > 1518)
               && !CPMAC_VLAN_IS_802_1Q_FRAME(skb->data)
              )
           || (CPMAC_VLAN_IS_0_LEN_FRAME(skb->data) && !mdio->mode_pppoa)
          ) {
            dev_kfree_skb_any(skb);
            if(length > 1518) {
                cpmac_priv->local_stats_rx_length_errors++;
            }
            cpmac_priv->local_stats_rx_errors++;
            DEB_WARN("[%s] drop pkt len %u\n", __FUNCTION__, length);
            ret = CPMAC_ERR_DROPPED;
        } else {
#           ifdef CONFIG_AVM_RTP_TIMESTAMP
            rtp_timestamp_insert_in_skb(skb); 
#           endif
#           ifdef CONFIG_AVM_PA
            if(avm_pa_dev_receive(AVM_PA_DEVINFO(cpmac_priv->owner), skb) == 0)
                return CPMAC_ERR_NOERR; /* skb accelerated */
#           endif /*--- #ifdef CONFIG_AVM_PA ---*/
            /* Port info in uniq_id per agreement with networking group */
            skb->uniq_id &= ~(0xFF << 24);
            if(CPMAC_VLAN_IS_802_1Q_FRAME(skb->data)) {
                /* Got VLAN packet. Relabel it. */
                unsigned short vid = CPMAC_VLAN_GET_VLAN_ID(skb->data);

                while(1) { /* Cleaner solution than several if levels */
                    unsigned int devoffset;

                    /*--- port = cpmac_priv->switch_config.map_vid_to_port[vid]; ---*/
                    port = mdio->switch_config.vidgroup[mdio->switch_config.map_vid_to_group[vid]].port_in;
                    devoffset = mdio->switch_config.map_port_to_dev[port];
                    skb->uniq_id |= (port + 1) << 24;
                    is_wan = (port == mdio->switch_config.wanport);

                    assert(devoffset < AVM_CPMAC_MAX_DEVS);
                    if(unlikely(mdio->switch_config.device[devoffset].net_device == 0)) {
                        break; /* Device disappeared */
                    }
                    p_dev = mdio->switch_config.device[devoffset].net_device;
                    /*--- DEB_TRC("[%s] Received data on port %u for device '%s'\n", __FUNCTION__, port, p_dev->name); ---*/

#                   if defined(CPMAC_DMA_RX_PRIOQUEUE_DEBUG)
                    log_from_dmaqueue(is_wan);
#                   endif/*--- #if defined(CPMAC_DMA_RX_PRIOQUEUE_DEBUG) ---*/

                    if(!p_dev) {
                        p_dev = cpmac_priv->owner;
                    } else {
                        struct cpmac_devinfo *d_priv = netdev_priv(p_dev);
                        d_priv->stats.rx_packets++;
                        d_priv->stats.rx_bytes += length;
                    }

                    skb->dev = p_dev;


                    if(   !is_wan
                       || mdio->switch_config.wanport_keeptags == 0
                       || mdio->switch_config.wanport_default_vid == vid) {
                        /* Remove VLAN header */
                        /*--- DEB_TRC("[%s] Removing VLAN header, because %s %s %s\n", ---*/
                                /*--- __FUNCTION__, ---*/
                                /*--- (!is_wan) ? "no wan  " : "", ---*/
                                /*--- (mdio->switch_config.wanport_keeptags == 0) ? "no keep tags  " : "", ---*/
                                /*--- (mdio->switch_config.wanport_default_vid == vid) ? "default vid" : ""); ---*/
                        memmove(skb->data + VLAN_HLEN, skb->data, 12);
                        skb_pull(skb, VLAN_HLEN);
                    }

#                   if defined(CONFIG_IP_MULTICAST_FASTFORWARD)
                    if(!(is_wan && mdio->mode_pppoa)) {
                        if(cpmac_mcfw_netdrv) {
                            if(is_wan) {
                                int rc;
                                rc = mcfw_multicast_forward_ethernet(cpmac_mcfw_sourceid, skb);
                                if(rc != 0) {
                                    cpmac_priv->net_dev_stats.rx_packets++;
                                    cpmac_priv->net_dev_stats.rx_bytes += length;
                                    return CPMAC_ERR_NOERR;
                                }
                            } else {
                                mcfw_snoop_recv(cpmac_mcfw_netdrv, port, skb);
                            }
                        }
                    }
#                   endif /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/

                    break; /* This while(1) should only run once! */
                }
            } else {
                /* If there is exactly one virtual device, it should be the receiver */
                if(mdio->switch_config.devices == 1) {
                    if(mdio->switch_config.device[0].net_device != NULL) {
                        p_dev = mdio->switch_config.device[0].net_device;
                        if(!p_dev) {
                            p_dev = cpmac_priv->owner;
                        }
                    }
                }
#               if defined(CONFIG_IP_MULTICAST_FASTFORWARD)
                if(mdio->f->get_phy_port) {
                    port = mdio->f->get_phy_port(mdio, skb);
                    if(port == 100) {
                        return CPMAC_ERR_NOERR; /* skb reception postponed */
                    }
                    assert(port < AVM_CPMAC_MAX_PORTS);
                    is_wan = (port == mdio->switch_config.wanport);
                    p_dev = mdio->switch_config.device[mdio->switch_config.map_port_to_dev[port]].net_device;
                    if(p_dev) {
                        /*--- DEB_TRC("[%s] Received data on port %u for device %u\n", __FUNCTION__, port, cpmac_priv->map_in_port_dev[port].dev); ---*/
                        if(p_dev != cpmac_priv->owner) {
                            struct cpmac_devinfo *d_priv = netdev_priv(p_dev);
                            d_priv->stats.rx_packets++;
                            d_priv->stats.rx_bytes += length;
                        }
                    } else {
                        p_dev = cpmac_priv->owner;
                    }
                    /* The port should be 1-4, therefor there is no adjustment needed for the Atheros switches */
                    /*--- skb->uniq_id |= (port) << 24; ---*/ /* Already done by get_phy_port */
                }

                skb->dev = p_dev;
                if(cpmac_mcfw_netdrv) {
                    if(mcfw_snoop_recv(cpmac_mcfw_netdrv, port, skb) == 0) {
                        int rc;
                        rc = mcfw_multicast_forward_ethernet(cpmac_mcfw_sourceid, skb);
                        if(rc != 0) {
                            cpmac_priv->net_dev_stats.rx_packets++;
                            cpmac_priv->net_dev_stats.rx_bytes += length;
                            return CPMAC_ERR_NOERR;
                        }
                    }
                }
#               else /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/
                if(p_dev != cpmac_priv->owner) {
                    ((struct cpmac_devinfo *) (p_dev->priv))->stats.rx_packets++;
                    ((struct cpmac_devinfo *) (p_dev->priv))->stats.rx_bytes += length;
                }
#               endif /*--- #else ---*/ /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/
            }
            skb->protocol = eth_type_trans(skb, p_dev);
            skb->dev = p_dev;
            /*--- DEB_TRC("[%s] last data was received for device %s\n", __FUNCTION__, skb->dev->name); ---*/
            if(is_wan && mdio->switch_config.wan_receive_function) {
                mdio->switch_config.wan_receive_function(skb);
            } else {
                netif_rx(skb);
            }
            cpmac_priv->net_dev_stats.rx_packets++;
            cpmac_priv->net_dev_stats.rx_bytes += length;
        }
    }
    return ret;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_if_reset_tx_queues(cpmac_priv_t *cpmac_priv) {
    cpmac_priv->UR8_QUEUE->tx_emac[0].prio[0] = 0;
    cpmac_priv->UR8_QUEUE->tx_emac[0].prio[1] = 0;
    cpmac_priv->UR8_QUEUE->tx_emac[1].prio[0] = 0;
    cpmac_priv->UR8_QUEUE->tx_emac[1].prio[1] = 0;
    cpmac_priv->UR8_QUEUE->tx_completion_queue[UR8_TX_COMPLETE] = 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0)
cpmac_err_t cpmac_if_teardown_complete(cpmac_priv_t *cpmac_priv) {
#   if defined(CONFIG_MIPS_UR8)
    struct __nwss_channel_cfg channel_cfg_tmp;
    struct sk_buff *transit_skb;
    cpphy_cppi_t *cppi = cpmac_priv->cppi;
    unsigned long flags;
#   endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/

    DEB_WARN("[%s]\n", __func__);

#   if defined(CONFIG_MIPS_UR8)
    cpmac_if_reset_tx_queues(cpmac_priv);
    atomic_set(&cppi->TxPrioQueues.q[0].DMAFree, cppi->TxPrioQueues.q[0].MaxDMAFree);
    atomic_set(&cppi->TxPrioQueues.q[1].DMAFree, cppi->TxPrioQueues.q[1].MaxDMAFree);
    spin_lock_irqsave(&cppi->skb_transit_spinlock, flags);
    while((transit_skb = skb_dequeue(&cppi->skbs_in_transit)) != NULL) {
        cpphy_if_free_tx_skb(cpmac_priv, transit_skb, CPMAC_ERR_DROPPED);
    }
    mb();
    spin_unlock_irqrestore(&cppi->skb_transit_spinlock, flags);
    channel_cfg_tmp.tx_A.Register = 0;
    channel_cfg_tmp.tx_A.Bits.desc_copy_bytecnt = 0;
    cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].tx_A.Register = channel_cfg_tmp.tx_A.Register;
    channel_cfg_tmp.tx_B.Register = 0;
    channel_cfg_tmp.tx_B.Bits.channel_mode = 0;
    channel_cfg_tmp.tx_B.Bits.packet_type = PACKET_TYPE_ETHERNET;
    channel_cfg_tmp.tx_B.Bits.cq_index = UR8_TX_COMPLETE;
    channel_cfg_tmp.tx_B.Bits.enable = 1;
    cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].tx_B.Register = channel_cfg_tmp.tx_B.Register;
#   endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/

    netif_wake_queue(cpmac_priv->owner);
    cpphy_if_data_from_queues(cpmac_priv->cppi);

    return CPMAC_ERR_NOERR;
}
#endif /*--- #if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpmac_if_register(cpmac_phy_handle_t phy_handle, cpmac_service_funcs_t *service_funcs) {
    cpphy_global_t *phy = (cpphy_global_t *) phy_handle;
    struct net_device *p_dev;
    cpmac_priv_t *cpmac_priv;
    int failed;
    cpmac_err_t ret = CPMAC_ERR_NOERR;
    char DevName[16];

    DEB_TRC("[%s]\n", __func__);

    if(phy->instance >= AVM_CPMAC_MAX_PHYS) {
        DEB_ERR("[%s] phy_id %u exceeds limit\n", __FUNCTION__, phy->instance);
        ret = CPMAC_ERR_EXCEEDS_LIMIT;
    } else if(g_dev_array[phy->instance].p_dev) {
        DEB_ERR("[%s] phy_id %u already registered\n", __FUNCTION__, phy->instance);
        ret = CPMAC_ERR_ALREADY_REGISTERED;
    } else {
        if(phy->mdio.switch_config.is_switch) {
            sprintf(DevName, "cpmac%u", phy->instance);
        } else {
            sprintf(DevName, "eth%u", phy->instance);
        }
        if(!(p_dev = alloc_netdev(sizeof(cpmac_priv_t), DevName, cpmac_main_dev_init))) {
            DEB_ERR("[%s] no mem for device.\n", __FUNCTION__);
            ret = CPMAC_ERR_NOMEM;
        } else {
            /* eth0 needs a queue, cpmac not. alloc_netdev set the device name
             * after calling the dev_init routine. Therefore querying the name
             * has to happen here. */
            if(strncmp(p_dev->name, "cpmac", 5) == 0) {
                p_dev->tx_queue_len = 0;
            }
            cpmac_priv = (cpmac_priv_t *) netdev_priv(p_dev);
            cpmac_priv->owner = p_dev;
            cpmac_priv->inst = phy->instance;
            cpmac_priv->pseudo_device = 0;
            /*--- cpmac_priv->cpu_freq = avalanche_clkc_get_freq(CLKC_MIPS); ---*/
            g_dev_array[cpmac_priv->inst].p_dev = p_dev;
            memcpy(&g_dev_array[cpmac_priv->inst].service_funcs,
                   service_funcs,
                   sizeof(cpmac_service_funcs_t));
            g_dev_array[cpmac_priv->inst].phy_handle = phy_handle;

            clear_bit(0, &cpmac_priv->set_to_close);

            cpmac_devices_installed++;

            /* Initialize procfs entries */
#           if defined(CONFIG_PROC_FS)
            cpmac_priv->procfs_dir = proc_mkdir("driver/avm_cpmac", NULL);
            cpmac_priv->procfs_dir = proc_mkdir(p_dev->name, cpmac_priv->procfs_dir);
#           endif /*--- #if defined(CONFIG_PROC_FS) ---*/

            /* init PHY */
            g_dev_array[cpmac_priv->inst].service_funcs.init(g_dev_array[cpmac_priv->inst].phy_handle,
                                                             (void *)cpmac_priv);
            cpphy_main_config_g_apply((cpphy_global_t *) g_dev_array[cpmac_priv->inst].phy_handle, p_dev);
            tasklet_init(&cpmac_priv->tasklet, g_dev_array[cpmac_priv->inst].service_funcs.isr_tasklet,
                         (unsigned long)phy_handle);
#           ifdef CONFIG_AVM_PA
            {
                struct avm_pa_pid_cfg cfg;
                snprintf(cfg.name, sizeof(cfg.name), "%s", p_dev->name);
                cfg.framing = avm_pa_framing_ether;
                cfg.default_mtu = 1500;
                cfg.tx_func = cpmac_pa_dev_transmit;
                cfg.tx_arg = p_dev;
                if(avm_pa_dev_pid_register(AVM_PA_DEVINFO(p_dev), &cfg) < 0)
                    printk(KERN_ERR "%s: failed to register PA PID\n", cfg.name);
            }
#           endif /*--- #ifdef CONFIG_AVM_PA ---*/

            failed = register_netdev(p_dev);
            if(failed) {
                DEB_ERR("[%s] Could not register device for inst %d because of reason code %d.\n",
                        __FUNCTION__, phy->instance, failed);
                free_netdev(p_dev);
                ret = CPMAC_ERR_REGISTER_FAILED;
            } else {
                DEB_ERR("[%s] dev %s (phy_id %u) registered\n", __FUNCTION__, DevName, phy->instance);
            }
        }
    }
    return ret;
}

#endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpmac_if_release(cpmac_priv_t *cpmac_priv __attribute__ ((unused))) {
    cpmac_err_t ret = CPMAC_ERR_NOERR;

#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8)
    if((ret = cpmac_if_check_mac_handle(cpmac_priv)) != CPMAC_ERR_NOERR) {
        DEB_ERR("[%s] ill mac_handle, %p\n", __FUNCTION__, cpmac_priv);
    } else {
        cpphy_func_deinit_t deinit;
        cpmac_phy_handle_t *phy_handle;
        struct net_device *p_dev = cpmac_priv->owner;

        DEB_INFO("[%s] %s\n", __FUNCTION__, p_dev->name);

        cpphy_mgmt_deinit(cpmac_priv->cppi->mdio);

        phy_handle = g_dev_array[cpmac_priv->inst].phy_handle;
        deinit = g_dev_array[cpmac_priv->inst].service_funcs.deinit;

        netif_carrier_off(p_dev);
        unregister_netdev(p_dev);   /* this will invoke cpmac_main_dev_close(): teardown */

        /* leave int working until teardown has completed */
        set_bit(0, &cpmac_priv->set_to_close);
        tasklet_kill(&cpmac_priv->tasklet);

        g_dev_array[cpmac_priv->inst].p_dev = NULL;
        deinit(phy_handle);

        kfree(p_dev);

        if(cpmac_devices_installed) {
            cpmac_devices_installed--;
        }
    }
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) ---*/
    return ret;
}


