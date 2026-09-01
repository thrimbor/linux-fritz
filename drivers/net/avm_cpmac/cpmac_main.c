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
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/avm_debug.h>
#include <net/pkt_sched.h>

#if !defined(CONFIG_NETCHIP_ADM69961)
#define CONFIG_NETCHIP_ADM69961
#endif
#include <linux/avm_event.h>
#include <linux/avm_cpmac.h>
#if defined(CONFIG_AVM_LED)
#include <linux/avm_led.h>
#endif /*--- #if defined(CONFIG_AVM_LED) ---*/
#ifdef CONFIG_AVM_RTP_TIMESTAMP
#include <linux/rtp_timestamp.h>
#endif

#include "cpgmac_f.h"
#include "cpmac_if.h"
#include "cpmac_const.h"
#include "cpmac_debug.h"
#include "cpmac_main.h"
#include "cpphy_const.h"
#include "cpphy_types.h"        /* for cpphy_global_t */
#include "cpphy_if.h"           /* for cpphy_if_tx_complete */
#include "cpphy_main.h"         /* for cpphy_main_init_queues and cpphy_main_config_g_apply */
#include "cpphy_adm6996.h"      /* for ADM_GET_TAG_GROUP */
#include "cpphy_ar8216.h"       /* for ATH_GET_TAG_GROUP */
#include "cpphy_mgmt.h"         /* for cpphy_mdio_event_dataupdate */
#include "cpphy_mdio.h"         /* for cpphy_mdio_event_update */
#include "cpphy_cppi.h"         /* for cpphy_cppi_teardown */
#include "cpmac_puma_if.h"
#include <asm/mach_avm.h>

#include <linux/if_vlan.h>
#include <linux/wireless.h>     /* For preventing the error on receiving the
                                   wireless extension ioctl */

#ifdef CONFIG_IP_MULTICAST_FASTFORWARD
struct mcfw_netdriver *cpmac_mcfw_netdrv = 0;
int cpmac_mcfw_sourceid = 0;
#endif

void *cpmac_event_handle;
dev_desc_t g_dev_array[AVM_CPMAC_MAX_PHYS];

MODULE_AUTHOR("Maintainer: AVM GmbH");
MODULE_DESCRIPTION("Ethernet driver for FRITZ!Box");

#if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_main_handle_mdio_status_ind(cpmac_priv_t *cpmac_priv, unsigned int linked) {
    struct net_device *p_dev = cpmac_priv->owner;

    if(linked) {
        if(!netif_carrier_ok(p_dev)) {
            netif_carrier_on(cpmac_priv->owner);
            DEB_INFO("[%s] carrier on for %s\n", __FUNCTION__, p_dev->name);
        }
        if(netif_running(p_dev) && netif_queue_stopped(p_dev)) {
            cpphy_if_tx_restart_queue(cpmac_priv->cppi);
            DEB_INFOTRC("[%s] wake queue for %s\n", __FUNCTION__, p_dev->name);
        }
    } else {
        if(netif_carrier_ok(p_dev)) {
            netif_carrier_off(p_dev);
            DEB_INFO("[%s] carrier off for %s\n", __FUNCTION__, p_dev->name);
        }
        if(!netif_queue_stopped(p_dev)) {
            cpphy_if_tx_stop_queue(cpmac_priv->cppi); /* do not let kernel send packets anymore */
            DEB_INFOTRC("[%s] stop queue for %s\n", __FUNCTION__, p_dev->name);
        }
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t cpmac_main_isr(int irq __attribute__ ((unused)),
                                  void *p_param,
                                  struct pt_regs *regs __attribute__ ((unused))) {
# else /*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) ---*/
static irqreturn_t cpmac_main_isr(int irq __attribute__ ((unused)), void *p_param) {
#endif /*--- # else ---*/
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) p_param;

    /*--- DEB_DEBUG("[%s] received irq\n", __FUNCTION__); ---*/

    /* performance-impact */
    /*--- disable_irq_nosync(cpmac_priv->intr); ---*/
#   if defined(CPU_PROFILE_LOG)
    ohio_simple_profiling_text("cpmac_main_isr");
#   endif
    tasklet_schedule(&cpmac_priv->tasklet);

    return IRQ_HANDLED;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int cpmac_main_setup_configuration(cpmac_capabilities_t * capabilities) {
    unsigned int net_flags = 0;

    capabilities->promiscous = CPMAC_CFG_PROMISCOUS;
    capabilities->broadcast = CPMAC_CFG_BROADCAST;
    capabilities->multicast = CPMAC_CFG_MULTICAST;
    capabilities->short_frames = CPMAC_CFG_SHORT_FRAMES;
    capabilities->long_frames = CPMAC_CFG_LONG_FRAMES;
    capabilities->auto_negotiation = CPMAC_CFG_AUTO_NEGOTIATION;        /* ??? */
    if(capabilities->broadcast) {
        net_flags |= IFF_BROADCAST;
    }
    if(capabilities->multicast) {
        net_flags |= IFF_MULTICAST;
    }
    return net_flags;
}

#if defined(CONFIG_AVM_LED)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpmac_led_remove(int handle __attribute__ ((unused))) {
    /* TODO Is it really necessary to do anything? */
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpmac_main_alloc_led_handle(cpmac_priv_t *cpmac_priv, unsigned int number) {
    cpmac_priv->led[number].handle = avm_led_alloc_handle("cpmac", number, cpmac_led_remove);
    if(IS_ERR((void *) cpmac_priv->led[number].handle)) {
        cpmac_priv->led[number].handle = 0;
    }
    if(cpmac_priv->led[number].handle == 0) {
        DEB_ERR("[%s] Error! Could not allocate LED handle %u!\n", __FUNCTION__, number);
    }
}
#endif /*--- #if defined(CONFIG_AVM_LED) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpmac_main_dev_open(struct net_device *p_dev) {
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(p_dev);
    cpmac_err_t ret;
    cpphy_mdio_t *mdio = &((cpphy_global_t *) g_dev_array[cpmac_priv->inst].phy_handle)->mdio;
    int result;

    DEB_INFO("[%s] %s\n", __FUNCTION__, p_dev->name);
    cpphy_main_init_queues((cpphy_global_t *) g_dev_array[cpmac_priv->inst].phy_handle);
    /* Apply Configuration Values to Device */
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    cpphy_main_config_apply(cpphy_global, p_dev);
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    cpphy_main_config_g_apply((cpphy_global_t *) g_dev_array[cpmac_priv->inst].phy_handle, p_dev);
#   else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#   warning "Might need configuration initialization for this architecture"
#   endif /*--- #else ---*/

    DEB_INFO("[%s] continued %s\n", __FUNCTION__, p_dev->name);

#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
#   if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
    result = request_irq(cpmac_priv->intr, cpmac_main_isr, IRQF_DISABLED, "Cpmac Driver", cpmac_priv);
#   else /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
    result = request_irq(cpmac_priv->intr, cpmac_main_isr, SA_INTERRUPT, "Cpmac Driver", cpmac_priv);
#   endif /*--- #else ---*/ /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
    if(result != 0) {
        DEB_ERR("[%s] failed to register the irq %u for %s\n", __FUNCTION__,
                cpmac_priv->intr, p_dev->name);
        return -EAGAIN;
    }
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    ur8int_set_type(UR8INT_NWSS_Tx0, ur8_interrupt_type_edge, ur8_interrupt_type_active_high);
    ur8int_set_type(UR8INT_NWSS_Rx0, ur8_interrupt_type_edge, ur8_interrupt_type_active_high);
#   if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
    result = request_irq(UR8INT_NWSS_Tx0, cpmac_main_isr, IRQF_DISABLED, "Cpmac Driver Tx", cpmac_priv);
#   else /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
    result = request_irq(UR8INT_NWSS_Tx0, cpmac_main_isr, SA_INTERRUPT, "Cpmac Driver Tx", cpmac_priv);
#   endif /*--- #else ---*/ /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
    if(result != 0) {
        DEB_ERR("[%s] failed to register the irq %u for %s\n",
                __FUNCTION__, UR8INT_NWSS_Tx0, p_dev->name);
        return (-1);
    }
#   if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
    result = request_irq(UR8INT_NWSS_Rx0, cpmac_main_isr, IRQF_DISABLED, "Cpmac Driver Rx", cpmac_priv);
#   else /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
    result = request_irq(UR8INT_NWSS_Rx0, cpmac_main_isr, SA_INTERRUPT, "Cpmac Driver Rx", cpmac_priv);
#   endif /*--- #else ---*/ /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
    if(result != 0) {
        DEB_ERR("[%s] failed to register the irq %u for %s\n", __FUNCTION__,
                UR8INT_NWSS_Rx0, p_dev->name);
        free_irq(UR8INT_NWSS_Tx0, cpmac_priv);
        return (-1);
    }
#   else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#   warning "No IRQs requested for this architecture!"
#   endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/

#   if defined(CONFIG_AVM_LED)
    if(mdio->switch_config.is_switch) {
        /* Setup led handles for a switch, if it is accessible */
        if(mdio->Mode & CPPHY_SWITCH_MODE_READ) {
            unsigned int i;
            for(i = 0; i < CPMAC_MAX_LED_HANDLES; i++) {
                cpmac_main_alloc_led_handle(cpmac_priv, i);
            }
        }
    } else {
        /* Setup led handle for this cpmac instance */
        cpmac_main_alloc_led_handle(cpmac_priv, cpmac_priv->inst);
    }
    mdio->event_update_needed = 1;
#   endif /*--- #if defined(CONFIG_AVM_LED) ---*/

    cpmac_global.cpphy[mdio->inst].is_open = 1;
    cpphy_mgmt_global_power_set(CPPHY_POWER_GLOBAL_ON);
    cpphy_mgmt_work_schedule(mdio, CPMAC_WORK_TICK, 0);
    ssleep(1);

    /* start dma */
    if((ret = cpphy_cppi_start_dma(cpmac_priv->cppi)) != CPMAC_ERR_NOERR) {
        DEB_ERR("[%s] failed to start dma, %u\n", __FUNCTION__, ret);
        /* FIXME Free correct IRQ for UR8! */
        free_irq(cpmac_priv->intr, cpmac_priv);
        return -EAGAIN;
    }

#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    cpmac_priv->irq_pace_handle = avm_int_ctrl_irq_pacing_register(cpmac_priv->intr);
    cpmac_priv->irq_pace_value = 1;
    avm_int_ctrl_irq_pacing_set(cpmac_priv->irq_pace_handle, cpmac_priv->irq_pace_value);
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/

    cpphy_if_tx_restart_queue(cpmac_priv->cppi);
    DEB_INFOTRC("[%s] start queue for %s\n", __FUNCTION__, p_dev->name);
#   if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
    /*--- netif_napi_add(p_dev, &cpmac_priv->napi, cpmac_poll, 64); ---*/
#   endif /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern dev_desc_t g_dev_array[AVM_CPMAC_MAX_PHYS];
static int cpmac_main_dev_close(struct net_device *p_dev) {
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(p_dev);
    cpphy_mdio_t *mdio = &((cpphy_global_t *) g_dev_array[cpmac_priv->inst].phy_handle)->mdio;

    DEB_INFO("[%s] %s\n", __FUNCTION__, p_dev->name);

    set_bit(0, &cpmac_priv->set_to_close);

    /* inform the upper layers. */
    if(!netif_queue_stopped(p_dev)) {
        DEB_INFOTRC("[%s] stop queue for %s\n", __FUNCTION__, p_dev->name);
        cpphy_if_tx_stop_queue(cpmac_priv->cppi);
    }

    /* full teardown tx/rx before free int */
    cpphy_cppi_teardown(cpmac_priv->cppi, CPPHY_RX_TEARDOWN | CPPHY_TX_TEARDOWN | CPPHY_FULL_TEARDOWN | CPPHY_BLOCKING_TEARDOWN);

    cpphy_mgmt_global_power_set(CPPHY_POWER_GLOBAL_SET_OFF);

#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    free_irq(cpmac_priv->intr, cpmac_priv);
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    free_irq(UR8INT_NWSS_Tx0, cpmac_priv);
    free_irq(UR8INT_NWSS_Rx0, cpmac_priv);
#   endif /*--- #elif defined(CONFIG_MIPS_UR8) ---*/

    cpmac_priv->cppi->TxOpen = 0;
    cpmac_priv->cppi->RxOpen = 0;
    cpmac_global.cpphy[mdio->inst].is_open = 0;
    clear_bit(0, &cpmac_priv->set_to_close);

    return (0);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpmac_main_dev_send(struct sk_buff *skb, struct net_device *p_dev) {
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(p_dev);
    unsigned int drop = 0;
    cpmac_err_t ret = CPMAC_ERR_NOERR;

    DEB_DEBUG("[%s] (%u) %*pB ...\n", __FUNCTION__, skb->len, min(24u, skb->len), skb->data);

#   ifdef CONFIG_AVM_RTP_TIMESTAMP
    rtp_timestamp_trace_session_from_skb_data(skb, 1);
#   endif

    /* if prio is out of range, use 0 */
    if(cpmac_priv->cppi->mdio->switch_config.devices == 1) {
        skb->uniq_id &= 0xffffff;
    }
    if((skb->uniq_id >> 24) >= CPPHY_PRIO_QUEUES) {
        DEB_TRC("[%s] Packet with illegal prio queue (%#lx) in uniq_id received. Forcing prio queue 0.\n",
                 __FUNCTION__, skb->uniq_id >> 24);
        skb->uniq_id &= 0xffffff;
    }
#   if defined(CONFIG_AVM_SIMPLE_PROFILING)
    skb_trace(skb, 19);
#   endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/

    if(skb->len < CPMAC_VLAN_TCI_START_OFFSET) {
        /* drop frame shorter than 14 bytes */
        drop = 1;
        DEB_WARN("[%s] drop short pkt (%u bytes)\n", __FUNCTION__, skb->len);
    }

    p_dev->trans_start = jiffies;

    if(!drop) {
        ret = g_dev_array[cpmac_priv->inst].service_funcs.data_to_phy(g_dev_array[cpmac_priv->inst].phy_handle, skb);
#       ifdef CONFIG_IP_MULTICAST_FASTFORWARD
        if(ret == CPMAC_ERR_NOERR && cpmac_mcfw_netdrv) {
            if(CPMAC_VLAN_IS_802_1Q_FRAME(skb->data)) {
                cpphy_mdio_t *mdio = cpmac_priv->cppi->mdio;
                unsigned short vid = CPMAC_VLAN_GET_VLAN_ID(skb->data);
                /*--- if(mdio->cpmac_switch == AVM_CPMAC_SWITCH_AR8216) { ---*/
                    /*--- (void) mcfw_snoop_send(cpmac_mcfw_netdrv, ---*/
                                           /* TODO Is this still correctly used? */
                                           /*--- mdio->switch_config.map_vid_portset[ATH_GET_TAG_GROUP(vid)], ---*/
                                           /*--- mdio->switch_config.vidgroup[mdio->switch_config.map_vid_to_group].portset, ---*/
                                           /*--- skb); ---*/
                /*--- } else { ---*/
                (void) mcfw_snoop_send(cpmac_mcfw_netdrv,
                                       /*--- mdio->switch_config.map_vid_portset[ADM_GET_TAG_GROUP(vid)], ---*/
                                       mdio->switch_config.vidgroup[mdio->switch_config.map_vid_to_group[vid]].portset,
                                       skb);
                /*--- } ---*/
            } else {
                /*
                 * HW 94.0.0.0
                 */
                mcfw_portset set;
                mcfw_portset_reset(&set);
                mcfw_portset_port_add(&set, 0);
                (void) mcfw_snoop_send(cpmac_mcfw_netdrv, set, skb);
            }
        }
#       endif
    }
    if(ret == CPMAC_ERR_NO_BUFFER) {
        DEB_TRC("[%s] CPMAC_ERR_NO_BUFFER stop queue\n", __FUNCTION__);
        cpphy_if_tx_stop_queue(cpmac_priv->cppi);
        drop = 1; /* Changed to fix TL6440 */
    }
    if(drop || (ret != CPMAC_ERR_NOERR)) {
        DEB_INFO("[%s] drop, %u, %u\n", __FUNCTION__, drop, ret);
        cpphy_if_tx_complete(cpmac_priv->cppi, skb, CPMAC_ERR_DROPPED);
    }
    return NETDEV_TX_OK;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0)
static void cpmac_main_tx_timeout(struct net_device *p_dev) {
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(p_dev);
    static unsigned long last_timeout = 0;
    static unsigned int timeout_count = 0;

    DEB_WARN("[%s] %s, latency: %lu\n", __FUNCTION__, p_dev->name, jiffies - p_dev->trans_start);

    if(time_after(jiffies, last_timeout + (300 * HZ))) {
        timeout_count = 0;
    }
    timeout_count++;
    last_timeout = jiffies;
    if(timeout_count >= 3) {
        panic("LAN tx problem");
    } else {
        cpphy_cppi_teardown(cpmac_priv->cppi, CPPHY_TX_TEARDOWN | CPPHY_CALLBACK_TEARDOWN);
    }
}
#endif /*--- #if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct net_device_stats *cpmac_main_get_net_stats(struct net_device *p_dev) {
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(p_dev);

    /* do not access the hardware if it is in the reset state. */
    if(likely(!test_bit(0, &cpmac_priv->set_to_close))) {
        g_dev_array[cpmac_priv->inst].service_funcs.control_req(g_dev_array[cpmac_priv->inst].phy_handle,
                                                                CPMAC_CONTROL_REQ_HW_STATUS,
                                                                &cpmac_priv->net_dev_stats);
        cpmac_priv->net_dev_stats.rx_errors += cpmac_priv->local_stats_rx_errors;
        cpmac_priv->net_dev_stats.rx_length_errors += cpmac_priv->local_stats_rx_length_errors;
        cpmac_priv->net_dev_stats.tx_errors += cpmac_priv->local_stats_tx_errors;
        cpmac_priv->net_dev_stats.tx_dropped = p_dev->qdisc->qstats.drops;
    }

    return &cpmac_priv->net_dev_stats;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpmac_main_multicast_set(struct net_device *p_dev) {
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(p_dev);
    cpmac_capabilities_t *capabilities = &cpmac_priv->capabilities;

    if(p_dev->flags & IFF_PROMISC) {
        if(capabilities->promiscous) {
            g_dev_array[cpmac_priv->inst].service_funcs.control_req(g_dev_array[cpmac_priv->inst].phy_handle,
                                                                    CPMAC_CONTROL_REQ_PROMISCOUS);
            DEB_INFO("[%s] %s set to promiscous mode\n", __FUNCTION__, p_dev->name);
        } else {
            DEB_WARN("[%s] %s not set to promiscous mode\n", __FUNCTION__, p_dev->name);
        }
    } else if(p_dev->flags & IFF_ALLMULTI) {
        if(capabilities->multicast) {
            g_dev_array[cpmac_priv->inst].service_funcs.control_req(g_dev_array[cpmac_priv->inst].phy_handle,
                                                                    CPMAC_CONTROL_REQ_MULTI_ALL);
            DEB_INFO("[%s] %s has been set to the ALL_MULTI mode\n", __FUNCTION__, p_dev->name);
        } else {
            DEB_WARN("[%s] %s not configured for ALL MULTI mode\n", __FUNCTION__, p_dev->name);
        }
    } else if(capabilities->multicast) {
        g_dev_array[cpmac_priv->inst].service_funcs.control_req(g_dev_array[cpmac_priv->inst].phy_handle,
                                                                CPMAC_CONTROL_REQ_MULTI_SINGLE);
        DEB_INFO("[%s] %s configured for %d multicast addresses\n", __FUNCTION__, p_dev->name, p_dev->mc_count);
    } else {
        DEB_WARN("[%s] %s has not been configured for multicast handling\n", __FUNCTION__, p_dev->name);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpmac_main_ioctl(struct net_device *p_dev, struct ifreq *ifr, int cmd) {
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(p_dev);
    int ret = 0;

    DEB_TRC("[%s] at jiffies %lu: %u (%s priv: %u)\n", 
             __FUNCTION__, 
             jiffies,
             cmd, 
             (cmd >= SIOCDEVPRIVATE) ? "" : "not ",
             (cmd >= SIOCDEVPRIVATE) ? cmd - SIOCDEVPRIVATE : 0);
    switch(cmd) {
        case (SIOCDEVPRIVATE + 0):
            DEB_WARN("[%s] AVM_CPMAC_IOCTL_INFO is obsolete!\n", __FUNCTION__); /* TODO */
            break;
        case (SIOCDEVPRIVATE + 5):
            DEB_WARN("[%s] AVM_CPMAC_IOCTL_SET_CONFIG_N is obsolete!\n", __FUNCTION__); /* TODO */
            break;
        case (SIOCDEVPRIVATE + 6):
            DEB_WARN("[%s] AVM_CPMAC_IOCTL_GET_CONFIG_N is obsolete!\n", __FUNCTION__); /* TODO */
            break;
        case AVM_CPMAC_IOCTL_GENERIC:
            if(g_dev_array[cpmac_priv->inst].p_dev) {
                cpmac_err_t err;

                err = g_dev_array[cpmac_priv->inst].service_funcs.control_req(g_dev_array[cpmac_priv->inst].phy_handle,
                                                                              CPMAC_CONTROL_REQ_GENERIC_CONFIG,
                                                                              ifr->ifr_data);
                if(err != CPMAC_ERR_NOERR) {
                    DEB_WARN("[%s] generic config, dev %u, err: %u\n", __FUNCTION__, cpmac_priv->inst, err);
                    switch(err) {
                        case CPMAC_ERR_NO_VLAN_POSSIBLE:
                        case CPMAC_ERR_CHAN_NOT_OPEN:
                            ret = -EIO; break;
                        default:
                            ret = -EINVAL; break;
                    }
                }
            } else {
                DEB_WARN("[%s] generic config, dev %u not registered\n", __FUNCTION__, cpmac_priv->inst);
                ret = -ENXIO;
            }
            break;
        case SIOCETHTOOL:
            break;
#           if 0
#           if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE)
            ret = dev_ethtool(dev_net(p_dev), ifr);
#           else /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
            DEB_WARN("[%s] Warning! Ioctl for ETHTOOL called\n", __FUNCTION__);
            ret = -EINVAL;
            /* TODO Find out, why ethtool lets the box crash */
            /*--- ret = dev_ethtool(ifr); ---*/
#           endif /*--- #else ---*/ /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
            break;
#           endif /*--- #if 0 ---*/
        case SIOCGIWNAME:
            /* We have no wireless extension, but do not want an error message */
            ret = -EINVAL;
            break;
        default:
            DEB_ERR("[%s] Error! Unknown ioctl %u (%s priv: %u)\n", 
                    __FUNCTION__,
                    cmd, 
                    (cmd >= SIOCDEVPRIVATE) ? "" : "not ",
                    (cmd >= SIOCDEVPRIVATE) ? cmd - SIOCDEVPRIVATE : 0);
            ret = -EINVAL;
            break;
    }

    return ret;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
static const struct net_device_ops netdev_ops = {
    .ndo_open = &cpmac_main_dev_open,
    .ndo_stop = &cpmac_main_dev_close,
    .ndo_start_xmit = &cpmac_main_dev_send,
    .ndo_get_stats = &cpmac_main_get_net_stats,
    .ndo_set_multicast_list = &cpmac_main_multicast_set,
    .ndo_do_ioctl = &cpmac_main_ioctl,
#   if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0)
    .ndo_tx_timeout = &cpmac_main_tx_timeout,
#   endif /*--- #if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0) ---*/
#   if defined(CONFIG_NET_POLL_CONTROLLER)
    .ndo_poll_controller = cpmac_main_netpoll,
#   endif /*--- #if defined(CONFIG_NET_POLL_CONTROLLER) ---*/
};
#endif /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_main_dev_init(struct net_device *p_dev) {
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(p_dev);

    memset(cpmac_priv, 0, sizeof(cpmac_priv_t));
    ether_setup(p_dev);

#   if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0)
    p_dev->watchdog_timeo = CPMAC_TX_TIMEOUT;
#   endif /*--- #if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0) ---*/

#   if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
    p_dev->netdev_ops = &netdev_ops;
#   else /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)) ---*/
    p_dev->open = &cpmac_main_dev_open;
    p_dev->stop = &cpmac_main_dev_close;
    p_dev->hard_start_xmit = &cpmac_main_dev_send;
    p_dev->get_stats = &cpmac_main_get_net_stats;
    p_dev->set_multicast_list = &cpmac_main_multicast_set;
    p_dev->do_ioctl = &cpmac_main_ioctl;
#   if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0)
    p_dev->tx_timeout = &cpmac_main_tx_timeout;
#   endif /*--- #if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0) ---*/
#   endif /*--- #else ---*/ /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)) ---*/

    p_dev->hard_header_len += CPPHY_TOTAL_RX_RESERVED;
    p_dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
    p_dev->flags |= cpmac_main_setup_configuration(&cpmac_priv->capabilities);
    netif_carrier_off(p_dev);
}
#endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_main_event_update(void) {
    struct cpmac_event_struct *event_struct;

    if(avm_event_source_check_id(cpmac_event_handle, avm_event_id_ethernet_connect_status)) {
        if(!(event_struct = (struct cpmac_event_struct *) kmalloc(sizeof(struct cpmac_event_struct), GFP_ATOMIC))) {
            DEB_ERR("[%s] nomem for status update\n", __FUNCTION__);
        } else {
            cpmac_global.event_data.ports = cpmac_global.ports;
            memcpy((unsigned char *) event_struct,
                   (unsigned char *) &cpmac_global.event_data,
                   sizeof (struct cpmac_event_struct));
            event_struct->event_header.id = avm_event_id_ethernet_connect_status;
            avm_event_source_trigger(cpmac_event_handle,
                                     avm_event_id_ethernet_connect_status,
                                     sizeof(struct cpmac_event_struct),
                                     (void *) event_struct);
        }
    } else {
        DEB_TRC("[%s] no one interested in event\n", __FUNCTION__);
    }
}


/*------------------------------------------------------------------------------------------*\
  to check: lock anything (own entry point)
  update request from application
\*------------------------------------------------------------------------------------------*/
void cpmac_main_event_notify(void *context __attribute__ ((unused)), enum _avm_event_id id) {
    DEB_TEST("[%s] id %u\n", __FUNCTION__, id); /* FIXME */
    switch(id) {
        case avm_event_id_ethernet_connect_status:
            cpmac_main_event_update();
            break;
        default:
            DEB_WARN("[%s] unhandled id %u!\n", __FUNCTION__, id);
            break;
    }
}


#if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) || defined(CONFIG_ARCH_PUMA5)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_IP_MULTICAST_FASTFORWARD)

static struct tasklet_struct cpmac_mcfw_tasklet;
static struct sk_buff_head cpmac_mcfw_queue;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_mcfw_schedule_tasklet(void) {
    if(skb_queue_len(&cpmac_mcfw_queue))
        tasklet_schedule(&cpmac_mcfw_tasklet);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_MIPS_UR8)
static void cpmac_mcfw_handle_tasklet(unsigned long data __attribute__ ((unused))) {
    struct net_device *dev = g_dev_array[0].p_dev;
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(dev);
    struct sk_buff *skb;
    int i = 32;

    while(   (i-- > 0)
          && skb_queue_len(&cpmac_mcfw_queue)
          && (atomic_read(&cpmac_priv->cppi->TxPrioQueues.q[0].Free) != 0)) {
        skb = skb_dequeue(&cpmac_mcfw_queue);
        assert(skb != NULL); /* Make Klocwork happy */
        cpmac_main_dev_send(skb, dev);
    }
    if(   skb_queue_len(&cpmac_mcfw_queue)
       && (atomic_read(&cpmac_priv->cppi->TxPrioQueues.q[0].Free) != 0)) {
        tasklet_schedule(&cpmac_mcfw_tasklet);
    }
}
#else /*--- defined(CONFIG_MIPS_UR8) ---*/
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpmac_mcfw_handle_tasklet(unsigned long data __attribute__ ((unused))) {
    struct sk_buff *skb;
    int i = 32;

    while(i-- > 0 && (skb = skb_dequeue(&cpmac_mcfw_queue)) != 0) {
        dev_queue_xmit(skb);
    }
    if(skb_queue_len(&cpmac_mcfw_queue))
        tasklet_schedule(&cpmac_mcfw_tasklet);
}
#endif /*--- #else ---*/ /*--- defined(CONFIG_MIPS_UR8) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpmac_dev_queue_xmit(struct net_device *dev, struct sk_buff *skb) {
    struct ethhdr *eth = (struct ethhdr *) skb->data;

    memcpy(eth->h_source, dev->dev_addr, ETH_ALEN);
    skb->dev = dev;

    if(irqs_disabled()) {       /* tiatm is source */
        skb_queue_tail(&cpmac_mcfw_queue, skb);
        tasklet_schedule(&cpmac_mcfw_tasklet);
    } else {
#       if defined(CONFIG_MIPS_UR8)
        cpmac_main_dev_send(skb, dev);
#       else /*--- #if defined(CONFIG_MIPS_UR8) ---*/
        dev_queue_xmit(skb);
#       endif /*--- #else ---*/ /*--- #if defined(CONFIG_MIPS_UR8) ---*/
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpmac_mc_transmit(void           *privatedata __attribute__ ((unused)),
                              int             sourceid __attribute__ ((unused)),
                              mcfw_portset    portset,
                              struct sk_buff *skb) {
    struct net_device *p_dev;
    t_cpphy_switch_config *switch_config;
    struct sk_buff *tskb;
    unsigned short vid = 0;

    /* TODO Right now global variables are used. This should change in the future */

#   if defined(CONFIG_ARCH_PUMA5)
    p_dev = cpmac_puma_get_dev();
#   else /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/
    p_dev = g_dev_array[0].p_dev;
#   endif /*--- #else ---*/ /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/

    if(p_dev == 0) {
        dev_kfree_skb_any(skb);
        return;
    }

    /* MC streams go to the LAN */
    skb->uniq_id &= 0xffffff;
    skb->uniq_id |= (CPPHY_PRIO_QUEUE_LAN << 24);

#   if defined(CONFIG_ARCH_PUMA5)
    switch_config = &(cpmac_global.cpphy[0].mdio.switch_config);
#   else /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/
    switch_config = &(((cpmac_priv_t *) (netdev_priv(p_dev)))->cppi->mdio->switch_config);
#   endif /*--- #else ---*/ /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/

    assert(portset <= AVM_CPMAC_MAX_PORTSET);
    vid = switch_config->map_portset_out[portset];

    if(vid != 0) {
        if((tskb = __vlan_put_tag(skb, vid)) != 0) {
            cpmac_dev_queue_xmit(p_dev, tskb);
        } else {
            dev_kfree_skb_any(skb);
        }
    } else {
        if(switch_config->enable_vlan) {
            unsigned short tag;
            int lastport = 0xff;
            int port;
            for(port = 0; port <= 5; port++) {
                if(mcfw_portset_port_is_in_set(&portset, port)) {
                    if(lastport != 0xff) {
                        struct sk_buff *nskb = skb_copy(skb, GFP_ATOMIC);
                        /*--- tag = switch_config->map_portset_out[1 << (lastport + switch_config->external_port_offset)]; ---*/
                        tag = switch_config->map_portset_out[1 << (lastport)];
                        if(nskb && (nskb = __vlan_put_tag(nskb, tag)) != 0) {
                            cpmac_dev_queue_xmit(p_dev, nskb);
                        }
                    }
                    lastport = port;
                }
            }
            if(lastport != 0xff) {
                /*--- tag = switch_config->map_portset_out[1 << (lastport + switch_config->external_port_offset)]; ---*/
                tag = switch_config->map_portset_out[1 << (lastport)];
                if((skb = __vlan_put_tag(skb, tag)) != 0) {
                    cpmac_dev_queue_xmit(p_dev, skb);
                }
            } else {
                DEB_ERR("cpmac_mc_transmit: no port in portset\n");
                dev_kfree_skb_any(skb);
            }
        } else {
            cpmac_dev_queue_xmit(p_dev, skb);
        }
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct mcfw_netdriver _cpmac_mcfw_netdrv = {
    name:                  "cpmac",
    netdriver_mc_transmit: cpmac_mc_transmit,
    privatedata:           (void *) 0
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpmac_mcfw_init(void) {
    int err;

    skb_queue_head_init(&cpmac_mcfw_queue);
    tasklet_init(&cpmac_mcfw_tasklet, cpmac_mcfw_handle_tasklet, 0);

    err = mcfw_netdriver_register(&_cpmac_mcfw_netdrv);
    if(err)
        return err;
    cpmac_mcfw_netdrv = &_cpmac_mcfw_netdrv;
    cpmac_mcfw_sourceid =
            mcfw_multicast_forward_alloc_id(cpmac_mcfw_netdrv->name);
    DEB_INFO("[%s] done\n", __FUNCTION__);
    return err;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if 0
static void cpmac_mcfw_exit(void) {
    if(cpmac_mcfw_netdrv) {
        mcfw_netdriver_unregister(cpmac_mcfw_netdrv);
        cpmac_mcfw_netdrv = 0;
        mcfw_multicast_forward_free_id(cpmac_mcfw_sourceid);
        tasklet_kill(&cpmac_mcfw_tasklet);
        DEB_INFO("[%s] done\n", __FUNCTION__);
    }
}
#endif /*--- #if 0 ---*/
#endif /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/

#endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) || defined(CONFIG_ARCH_PUMA5) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_main_probe(void) {
    DEB_INFO("[%s] init\n", __FUNCTION__);

    cpmac_event_handle = avm_event_source_register("Ethernet status",
                                                     /*--- (1ull << avm_event_id_ethernet_status) ---*/
                                                   (1ull << avm_event_id_ethernet_connect_status),
                                                   cpmac_main_event_notify, NULL);
    if(!cpmac_event_handle) {
        DEB_ERR("[%s] Could not register avm_event_source!\n", __FUNCTION__);
    }
#   if defined(CONFIG_IP_MULTICAST_FASTFORWARD) && !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185))
    if(cpmac_mcfw_init() != 0) {
        DEB_ERR("[%s] cpmac_mcfw_init: failed!\n", __FUNCTION__);
    }
#   endif /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) && !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185))---*/
}


/* FIXME */
#if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) || defined(CONFIG_ARCH_PUMA5)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if 0
void cpmac_main_exit(void) {
    struct net_device *dev;
    cpmac_priv_t *cpmac_priv;
    unsigned int i;

    DEB_INFO("[%s] init\n", __FUNCTION__);
#   if defined(CONFIG_IP_MULTICAST_FASTFORWARD)
    cpmac_mcfw_exit();
#   endif /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/
#   if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE)
    /* FIXME Activate the code! */
#   else /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
    avm_event_source_release(cpmac_event_handle);
#   endif /*--- #else ---*/ /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/

    /* FIXME Code might be duplicated in cpphy_entry_exit! */
    for(i = 0; i < AVM_CPMAC_MAX_PHYS; i++) {
        if((dev = g_dev_array[i].p_dev)) {
            cpmac_priv = (cpmac_priv_t *) netdev_priv(dev);
            cpmac_if_release(cpmac_priv);
        }
    }
}
#endif /*--- #if 0 ---*/

#endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_AVM_PA
void cpmac_pa_dev_transmit(void *arg, struct sk_buff *skb) {
    skb->dev = (struct net_device *) arg;
#   if defined(CONFIG_MIPS_UR8)
    /* Target is cpmac0 */
    if(skb->dev->tx_queue_len == 0) {
        cpmac_main_dev_send(skb, skb->dev);
    } else {
        dev_queue_xmit(skb);
    }
#   else /*--- #if defined(CONFIG_MIPS_UR8) ---*/
    dev_queue_xmit(skb);
#   endif /*--- #else ---*/ /*--- #if defined(CONFIG_MIPS_UR8) ---*/
}
#endif /*--- #ifdef CONFIG_AVM_PA ---*/

