/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,...,2014 AVM GmbH <fritzbox_info@avm.de>
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

#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <net/pkt_sched.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/env.h>
#include <asm/mach_avm.h>
#include <asm/io.h>
#include <asm/atomic.h>
#if !defined(CONFIG_NETCHIP_ADM69961)
#define CONFIG_NETCHIP_ADM69961
#endif
#include <linux/avm_event.h>
#include <linux/avm_cpmac.h>
#include "cpmac_if.h"
#include "cpmac_const.h"
#include "cpmac_debug.h"
#include "cpmac_main.h"
#include "cpmac_reg.h"
#include "cpphy_const.h"
#include "cpphy_types.h"
#include "cpphy_mdio.h"
#include "cpphy_mgmt.h"
#include "cpphy_cppi.h"
#include "cpphy_main.h"
#include "cpphy_if.h"
#include "cpphy_if_g.h"
#include "cpphy_misc.h"
#include "adm6996.h"
#include "cpphy_adm6996.h"
#include "cpphy_ar8216.h"
#include "cpphy_switch.h"
#if defined(CONFIG_MIPS_UR8)
#include <asm/mach-ur8/ur8_cppi.h>
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/

#if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_if_free_tcb(cpphy_cppi_t *cppi, cpphy_tcb_t *tcb) {
    /* TODO: remove NULL checks; IRQ disabling not necessary? */
    if(unlikely(tcb == NULL)) {
        DEB_ERR("[%s] tcb == NULL\n", __FUNCTION__);
        return;
    }
    if(tcb->IsDynamicallyAllocated) {
        kfree((void *) tcb->KMallocPtr);
        cppi->support.tcbs_freed_dynamic++;
        return;
    }
    local_irq_disable();
    if(unlikely(cppi->TxLastFree == NULL)) {
        DEB_ERR("[%s] cppi->TxLastFree == NULL\n", __FUNCTION__);
        return;
    }
    tcb->Next = NULL;
    cppi->TxLastFree->Next = tcb;
    cppi->TxLastFree = tcb;
    cppi->support.tcbs_freed++;
    local_irq_enable();
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpphy_tcb_t *cpphy_if_alloc_tcb(cpphy_cppi_t *cppi) {
    cpphy_tcb_t *tcb;
    unsigned int tcbSize = sizeof (cpphy_tcb_t) + 0x3f; /* Enough size for aligning struct */

    local_irq_disable();
    if(cppi->TxFirstFree->Next == NULL) {
        cpphy_tcb_t *ptr;
        /* TODO Make freeing the allocated space on shutdown possible */
        ptr = kmalloc(tcbSize, GFP_ATOMIC);
        if(unlikely(ptr == NULL)) {
            local_irq_enable();
            DEB_ERR("[%s] Unable to allocate a new tcb!\n", __FUNCTION__);
            return NULL;
        }
        tcb = (cpphy_tcb_t *)(((unsigned int) ptr + 0x3f) & ~0x3f);
        dma_cache_wback_inv((unsigned long) tcb, sizeof(cpphy_tcb_t));
        tcb = (cpphy_tcb_t *) CPPHY_VIRT_TO_VIRT_NO_CACHE(tcb);
        tcb->KMallocPtr = (void *) ptr;
        tcb->IsDynamicallyAllocated = 1;
        cppi->support.tcbs_alloced_dynamic++;
        /*--- DEB_TEST("[cpphy_if_alloc_tcb] Allocated new tcb 0x%p\n", tcb); ---*/
        local_irq_enable();
        return tcb;
    }
    /*--- atomic_sub(1, &cppi->FreeTcbCount); ---*/
    tcb = (cpphy_tcb_t *) cppi->TxFirstFree;
    cppi->TxFirstFree = cppi->TxFirstFree->Next;
    cppi->support.tcbs_alloced++;
    local_irq_enable();
    return tcb;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_if_tx_stop_queue(cpphy_cppi_t *cppi) {
    t_cpphy_switch_config *switch_config = &cppi->mdio->switch_config; 
    unsigned char dev_num;

    local_irq_disable();
    for(dev_num = 0; dev_num < switch_config->devices; dev_num++) {
        if(!netif_queue_stopped(switch_config->device[dev_num].net_device)) {
            DEB_TRC("[%s] Stopping queue for device '%s'\n", __FUNCTION__,
                    switch_config->device[dev_num].net_device->name);
            netif_stop_queue(switch_config->device[dev_num].net_device);
        }
    }
    DEB_TRC("[%s] Stopping tx queue\n", __FUNCTION__);
    netif_stop_queue(cppi->cpmac_priv->owner);
    local_irq_enable();
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_if_tx_restart_queue(cpphy_cppi_t *cppi) {
    local_irq_disable();
    if(netif_carrier_ok(cppi->cpmac_priv->owner)) {
        if(netif_queue_stopped(cppi->cpmac_priv->owner)) {
            t_cpphy_switch_config *switch_config = &cppi->mdio->switch_config; 
            unsigned char dev_num;

            DEB_TRC("[%s] Starting tx queue\n", __FUNCTION__);
            netif_wake_queue(cppi->cpmac_priv->owner);
            for(dev_num = 0; dev_num < switch_config->devices; dev_num++) {
                if(   netif_carrier_ok(switch_config->device[dev_num].net_device)
                        && netif_queue_stopped(switch_config->device[dev_num].net_device)) {
                    DEB_TRC("[%s] Restarting queue for device '%s'\n", __FUNCTION__,
                            switch_config->device[dev_num].net_device->name);
                    netif_wake_queue(switch_config->device[dev_num].net_device);
                }
            }
        }
    }
    local_irq_enable();
    cpmac_mcfw_schedule_tasklet();
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_if_free_tx_skb(cpmac_priv_t   *cpmac_priv,
                          struct sk_buff *skb,
                          unsigned int    status) {
    if(likely(status == CPMAC_ERR_NOERR)) {
        /*--- DEB_DEBUG("[%s] %u bytes sent\n", __FUNCTION__, skb->len); ---*/
        cpmac_priv->cppi->TxLastCompleted = jiffies;
        cpmac_priv->net_dev_stats.tx_packets++;
        cpmac_priv->net_dev_stats.tx_bytes += skb->len;
    } else {
        DEB_INFO("[%s] %u bytes dropped (%u)\n", __FUNCTION__, skb->len, status);
        cpmac_priv->local_stats_tx_errors++;
    }

#   if defined(CONFIG_AVM_SIMPLE_PROFILING)
    skb_trace(skb, 20);
#   endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
    dev_kfree_skb_any(skb);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_if_tx_complete(cpphy_cppi_t *cppi,
                          struct sk_buff *skb,
                          unsigned int status) {
    cpmac_priv_t *cpmac_priv = cppi->cpmac_priv;
    unsigned int enough_free;
    unsigned int prio_queue;
    struct sk_buff *transit_skb;
    unsigned long flags;

    if(unlikely(skb == NULL)) {
        DEB_ERR("[%s] skb is NULL!\n", __FUNCTION__);
        return;
    }

    prio_queue = skb->uniq_id >> 24;

    enough_free = !atomic_read(&cppi->TxPrioQueues.q[prio_queue].DMAFree);
    atomic_inc(&cppi->TxPrioQueues.q[prio_queue].DMAFree);
    cppi->TxPrioQueues.q[prio_queue].BytesDequeued += skb->len;

    /* Make sure, that the transit list is parsed only for packets that should be in it */
    if(status == CPMAC_ERR_NOERR) {
        spin_lock_irqsave(&cppi->skb_transit_spinlock, flags);
        transit_skb = skb_dequeue(&cppi->skbs_in_transit);
        mb();
        spin_unlock_irqrestore(&cppi->skb_transit_spinlock, flags);
        if(unlikely((transit_skb != NULL) && (skb != transit_skb))) {
            unsigned int skbs_freed = 0;
            do {
                if(skbs_freed < 3) {
                    DEB_WARN("[%s] skb to complete: 0x%p; will drop expected skb 0x%p!\n", 
                             __func__, skb, transit_skb);
                }
                cpphy_if_free_tx_skb(cpmac_priv, transit_skb, CPMAC_ERR_DROPPED);
                spin_lock_irqsave(&cppi->skb_transit_spinlock, flags);
                transit_skb = skb_dequeue(&cppi->skbs_in_transit);
                mb();
                spin_unlock_irqrestore(&cppi->skb_transit_spinlock, flags);
                skbs_freed++;
            } while(unlikely((transit_skb != NULL) && (skb != transit_skb)));
            if(skbs_freed > 3) {
                DEB_WARN("[%s] skb to complete: 0x%p; dropped %u expected skbs overall!\n", 
                         __func__, skb, skbs_freed);
            }

        }
        if(likely(skb == transit_skb)) {
            cpphy_if_free_tx_skb(cpmac_priv, skb, status);
        } else {
            DEB_ERR("[%s] Could not find skb %p in tx list!\n", __func__, skb);
        }
    } else {
        cpphy_if_free_tx_skb(cpmac_priv, skb, status);
    }

    /* Wakeup of queue should be here, if there is more than one caller of this function */

    if(enough_free) {
        if(likely(!test_bit(0, &cpmac_priv->set_to_close))) {
            cpphy_if_tx_restart_queue(cppi);
            cpphy_if_data_from_queues(cppi);
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
inline void cpphy_if_tcb_enqueue(cpphy_cppi_t *cppi, unsigned char priority, cpphy_tcb_t *tcb) {
    cpphy_tcb_queue_t *queue = &cppi->TxPrioQueues.q[priority];
    unsigned long flags;

    spin_lock_irqsave(&queue->lock, flags);
    tcb->Next = NULL;
    if(queue->Last != NULL) {
        queue->Last->Next = tcb;
    }
    queue->Last = tcb;
    if(queue->First == NULL) {
        queue->First = tcb;
    }
    atomic_dec(&queue->Free);
    atomic_dec(&cppi->TxPrioQueues.SummedFree);
	spin_unlock_irqrestore(&queue->lock, flags);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline cpphy_tcb_t *cpphy_if_tcb_dequeue(cpphy_cppi_t *cppi, unsigned char priority) {
    cpphy_tcb_queue_t *queue = &cppi->TxPrioQueues.q[priority];
    cpphy_tcb_t *tcb;
    unsigned long flags;

    spin_lock_irqsave(&queue->lock, flags);
    tcb = (cpphy_tcb_t *) queue->First;
    if(queue->First != NULL) {
        queue->First = queue->First->Next;
        atomic_inc(&cppi->TxPrioQueues.SummedFree);
        atomic_inc(&queue->Free);
    }
    if(queue->First == NULL) {
        queue->Last = NULL;
    }
	spin_unlock_irqrestore(&queue->lock, flags);

    return tcb;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

#if defined(CPMAC_DMA_TX_PRIOQUEUE_DEBUG)
static char buf[60];
static int bufidx = 0;

static void log_to_dmaqueue(int priority) {
    if(bufidx == sizeof(buf)-1) {
        buf[bufidx] = 0;
        DEB_INFO("cpmac: tx %s\n", buf);
        bufidx = 0;
    }
    buf[bufidx++] = '0'+ (priority % CPPHY_PRIO_QUEUES);
}
#endif /*--- #if defined(CPMAC_DMA_TX_PRIOQUEUE_DEBUG) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_if_check_external_tagging(cpphy_cppi_t *cppi, cpphy_tcb_t *tcb) {
    cpphy_mdio_t *mdio = cppi->mdio;

    if((tcb != NULL) && (CPMAC_VLAN_IS_802_1Q_FRAME(tcb->skb->data))) { /* VLAN should be used */
        unsigned char is_tagged = mdio->switch_config.wanport_default_vid != CPMAC_VLAN_GET_VLAN_ID(tcb->skb->data);
        unsigned char keep_tagging;
        assert(mdio->switch_config.wanport < AVM_CPMAC_MAX_PORTS); /* To ease Klocwork checking */
        keep_tagging = mdio->switch_status.port[mdio->switch_config.wanport].keep_tag_outgoing;
        if(keep_tagging != is_tagged) {
            DEB_TRC("[%s] Need to switch tagging\n", __FUNCTION__);
            /* Need to change the keep_tagging setting */
            if(   (unsigned int) atomic_read(&cppi->TxPrioQueues.q[CPPHY_PRIO_QUEUE_WAN].DMAFree) 
               != cppi->TxPrioQueues.q[CPPHY_PRIO_QUEUE_WAN].MaxDMAFree) {
                /* Wait until there are no more WAN packets in *\
                 * the DMA queue before the tagging can be     *
                 \* switched.                                   */
                DEB_TRC("[%s] Waiting, until the WAN DMA queue is empty\n", __FUNCTION__);
                return 0;
            }
            if(mdio->wan_tagging_enable != is_tagged) {
                mdio->wan_tagging_enable = is_tagged;
                if(mdio->f->set_wan_keep_tagging) {
                    cpphy_mgmt_work_add(mdio, CPMAC_WORK_TOGGLE_VLAN, mdio->f->set_wan_keep_tagging, 0);
                }
            } 
            return 0;
        }
    }
    return 1;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_if_data_from_queues(cpphy_cppi_t *cppi) {
    cpphy_tcb_t *tcb;
    unsigned int count;
    unsigned char priority;

    if(unlikely(atomic_add_return(1, &cppi->dequeue_running) != 1)) {
        DEB_ERR("[%s] conflict\n", __FUNCTION__); /* TODO? */
        return;
    }

    /* Part 2: Find next skb from appropriate queue and send it */
    for( ;; ) {
        count = 0;

        for(priority = 0; priority < CPPHY_PRIO_QUEUES; priority++) {
            if(atomic_read(&cppi->TxPrioQueues.q[priority].Free) == (int) cppi->TxPrioQueues.q[priority].MaxSize) {
                continue; /* TxPrioQueue is empty */
            }
            if(atomic_read(&cppi->TxPrioQueues.q[priority].DMAFree) <= 0) {
                DEB_DEBUG("[%s] DMA queue %u full\n", __FUNCTION__, priority);
                /* Always stopping the whole cpmac0 queue when one priority
                 * queue is full is a problem, because that would prevent
                 * traffic for the other priority queue.
                 * As the main reason for the stop_queue is detecting the UR8
                 * LAN tx hangup, this is only done when the current priority
                 * queue is full and no packets have been completed by the UR8
                 * hardware. */
                if(time_after(jiffies, cppi->TxLastCompleted + CPMAC_TX_TIMEOUT)) {
                    DEB_TRC("[%s] Queue %u full, completion outstanding. Stopping tx queue.\n", __FUNCTION__, priority);
                    cpphy_if_tx_stop_queue(cppi);
                }
                continue;
            }
            count++;
            if(!cppi->TxPrioQueues.q[priority].Pause) {
                tcb = (cpphy_tcb_t *) cppi->TxPrioQueues.q[priority].First;
                /* Find out, if the tagging has to be changed */
                if(   cppi->mdio->f->check_external_tagging
                   && (priority == CPPHY_PRIO_QUEUE_WAN)) { /* Is this WAN traffic? */
                    if(!cppi->mdio->f->check_external_tagging(cppi, tcb)) {
                        count--;
                        continue;
                    }
                }

                tcb = cpphy_if_tcb_dequeue(cppi, priority);
                if(likely(tcb != NULL)) {
                    unsigned long flags;
                    atomic_dec(&cppi->TxPrioQueues.q[priority].DMAFree);
#                   ifdef CPMAC_DMA_TX_PRIOQUEUE_DEBUG
                    log_to_dmaqueue(priority);
#                   endif
                    spin_lock_irqsave(&cppi->skb_transit_spinlock, flags);
                    skb_queue_tail(&cppi->skbs_in_transit, tcb->skb);
                    mb();
                    spin_unlock_irqrestore(&cppi->skb_transit_spinlock, flags);
#                   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
                    cpphy_if_data_to_phy_dma(cppi, tcb);
#                   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
                    cpphy_if_g_data_to_phy_dma(cppi, tcb);
#                   else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#                   warning "Missing cpphy_if_g_data_to_phy_dma for this architecture"
#                   endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
                }
                cppi->TxPrioQueues.q[priority].Pause = cppi->TxPrioQueues.q[priority].PauseInit;
            } else {
                assert(cppi->TxPrioQueues.q[priority].Pause <= cppi->TxPrioQueues.q[priority].PauseInit);
                cppi->TxPrioQueues.q[priority].Pause--;
            }
        }
        if((count == 0) && (atomic_sub_return(1, &cppi->dequeue_running) == 0)) {
            break;
        }
    }

    /* Part 3 (optional): Reschedule queue priorities */
}


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
cpmac_err_t cpphy_if_control_req(cpmac_phy_handle_t phy_handle,
                                 cpmac_control_req_t control, ...) {
    va_list param;
    cpmac_err_t ret = CPMAC_ERR_NOERR;
    cpphy_mdio_t *mdio = &((cpphy_global_t *) phy_handle)->mdio;
    cpphy_cppi_t *cppi = &((cpphy_global_t *) phy_handle)->cppi;

    switch(control) {
        case CPMAC_CONTROL_REQ_IS_SWITCH:
            if(!mdio->switch_config.is_switch) {
                ret = CPMAC_ERR_NO_SWITCH;
            }
            break;
        case CPMAC_CONTROL_REQ_MULTI_SINGLE:
            cpphy_cppi_set_multi_promiscous (&((cpphy_global_t *)phy_handle)->cppi, 2, 0);
            break;
        case CPMAC_CONTROL_REQ_MULTI_ALL:
            cpphy_cppi_set_multi_promiscous (&((cpphy_global_t *)phy_handle)->cppi, 1, 0);
            break;
        case CPMAC_CONTROL_REQ_PROMISCOUS:
            cpphy_cppi_set_multi_promiscous (&((cpphy_global_t *)phy_handle)->cppi, 0, 1);
            break;
        case CPMAC_CONTROL_REQ_HW_STATUS:
            {
                struct net_device_stats *stats;

                va_start(param, control);
                stats = va_arg(param, void *);
                stats->collisions        = 0;
                stats->rx_crc_errors     = 0;
                stats->rx_dropped        = 0;
                stats->rx_errors         = 0;
                stats->rx_fifo_errors    = 0;
                stats->rx_frame_errors   = 0;
                stats->rx_length_errors  = 0;
                stats->rx_missed_errors  = 0;
                stats->rx_over_errors    = 0;
                stats->tx_carrier_errors = 0;
                stats->tx_errors         = 0;
                stats->tx_fifo_errors    = 0;
                if(mdio->f->update_hw_status) {
                    mdio->f->update_hw_status(mdio, stats);
                }
                cpphy_cppi_update_hw_status(cppi, stats);
                va_end(param);
            }
            break;
        case CPMAC_CONTROL_REQ_PORT_COUNT:
            /* to check: adjust port count in case of empty cpphy */
            /* design: abuse ret as port count */
            /*--- ret = (cpmac_err_t)((cpphy_global_t *) phy_handle)->cpmac_switch ? 4 : 1; ---*/
            /* TODO Should this depend on the device/PHY? */
            ret = (cpmac_err_t) cpmac_global.ports;
            break;
        case CPMAC_CONTROL_REQ_GENERIC_CONFIG:
            {
                void *ioctl_struct;
                struct avm_cpmac_ioctl_struct avm_ioctl;

                va_start(param, control);
                ioctl_struct = va_arg(param, void *);
                copy_from_user((void *) &avm_ioctl, ioctl_struct, sizeof(struct avm_cpmac_ioctl_struct));
                ret = cpphy_switch_ioctl(mdio, &avm_ioctl);
                copy_to_user(ioctl_struct, (void *) &avm_ioctl, sizeof(struct avm_cpmac_ioctl_struct));
                va_end(param);
            }
            break;
        default:
            DEB_INFO("[%s] unhandled control %u\n", __FUNCTION__, control);
            ret = CPMAC_ERR_ILL_CONTROL;
            break;
    }
    return ret;
}


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
void cpphy_if_isr_end(cpmac_phy_handle_t phy_handle) {
    CPMAC_MAC_EOI_VECTOR(((cpphy_global_t *)phy_handle)->cpmac_priv->owner->base_addr) = 0;
}


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
cpmac_err_t cpphy_if_init(cpmac_phy_handle_t phy_handle, cpmac_priv_t *cpmac_priv) {
    cpmac_err_t ret = CPMAC_ERR_NOERR;
    struct net_device *p_dev = cpmac_priv->owner;
    char *mac_name = NULL;
    char *mac_string = NULL;

    DEB_TRC("[%s]\n", __func__);

    ((cpphy_global_t *) phy_handle)->cpmac_priv = cpmac_priv;
    ((cpphy_global_t *) phy_handle)->cppi.cpmac_priv = cpmac_priv;

#   if defined(CONFIG_MIPS_UR8)
    cpmac_priv->UR8_QUEUE = (struct ur8_queue_manager *) UR8_NWSS_QUEUE;
    cpmac_priv->UR8_NWSS  = (struct ur8_nwss_register *)&(*(volatile unsigned int *)(UR8_NWSS_BASE));
    if(((cpphy_global_t *) phy_handle)->mdio.high_phy) {
        cpmac_priv->CPGMAC_F = (struct cpgmac_f_regs *) UR8_CPMAC1_BASE;
        mac_name = "macb";
    } else {
        cpmac_priv->CPGMAC_F = (struct cpgmac_f_regs *) UR8_CPMAC0_BASE;
        mac_name = "maca";
    }
    if(ur8_teardown_init() != 0) {
        DEB_ERR("[%s] Initialization of teardown register failed\n", __func__);
        return CPMAC_ERR_REGISTER_FAILED;
    }
#   else /*--- #if defined(CONFIG_MIPS_UR8) ---*/
#   warning "No reset routine for PHY"
    mac_name = "maca";
#   endif /*--- #else ---*/
    /*--- wait 100 ms ---*/
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(HZ / 10);

    mac_string = prom_getenv(mac_name);
    if(!mac_string) {
        mac_string="08.00.28.32.06.02";
        DEB_ERR("Error getting mac from Boot enviroment for %s\n", p_dev->name);
        DEB_ERR("Using default mac address: %s\n",mac_string);
        DEB_ERR("Use Bootloader command:\n");
        DEB_ERR("    setenv %s xx.xx.xx.xx.xx.xx\n",mac_name);
        DEB_ERR("to set mac address\n");
    }
    DEB_INFO("[%s] dev %s has mac addr: %s\n", __FUNCTION__, p_dev->name, mac_string);
    cpphy_misc_str2eaddr(p_dev->dev_addr, mac_string);

    cpphy_main_open((cpphy_global_t *) phy_handle, p_dev);
    return ret;
}


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
void cpphy_if_deinit(cpmac_phy_handle_t phy_handle) {
    cpmac_priv_t *cpmac_priv = ((cpphy_global_t *) phy_handle)->cpmac_priv;
    struct net_device *p_dev = (struct net_device *) (cpmac_priv->owner);
    cpphy_cppi_t *cppi = &((cpphy_global_t *) phy_handle)->cppi;

    DEB_TRC("[%s]\n", __FUNCTION__);

    cpphy_main_close(cppi);
    DEB_INFO("[%s] device %s closed\n", __FUNCTION__, p_dev->name);

    /* Buffer/descriptor resources may still need to be freed if a Close
       Mode 1 was performed prior to Shutdown - clean up here */
    if(cppi->RcbStart) {
        cpphy_cppi_free_rcb(cppi);
    }
    if(cppi->TcbStart) {
        cpphy_cppi_free_tcb(cppi);
    }

    release_mem_region(p_dev->base_addr, cpmac_priv->dev_size);
}

#endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/

