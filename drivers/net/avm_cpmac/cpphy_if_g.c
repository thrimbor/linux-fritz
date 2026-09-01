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

#if defined(CONFIG_MIPS_UR8)
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <net/pkt_sched.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
#include <asm/mach-ur8/ur8.h>
#else
#include <asm/mips-boards/ur8.h>
#endif
#include <asm/mach-ur8/hw_nwss.h>
/*--- #include <asm/mach-ohio/led_hal.h> ---*/
#include <asm/mach_avm.h>
#include <asm/io.h>
#include <asm/atomic.h>
#if defined(CONFIG_AVM_SIMPLE_PROFILING)
#include <linux/avm_profile.h>
#endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/

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
#include "cpphy_cppi.h"
#include "cpphy_main.h"
#include "cpphy_if.h"
#include "cpphy_if_g.h"
#include "cpphy_misc.h"
#include "adm6996.h"
#include "cpphy_adm6996.h"
#include "cpgmac_f.h"


static cpphy_rcb_t *unused_rcbs = NULL;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_if_g_data_to_phy_dma (cpphy_cppi_t *cppi, cpphy_tcb_t *tcb) {
    cpmac_priv_t *cpmac_priv = cppi->cpmac_priv;

    /* TODO Support for both EMACs and priorities */
    cpmac_priv->UR8_QUEUE->tx_emac[0].prio[0] = CPPHY_VIRT_TO_PHYS(tcb);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_if_g_data_to_phy(cpmac_phy_handle_t phy_handle, struct sk_buff *skb) {
    cpphy_cppi_t *cppi = &((cpphy_global_t *)phy_handle)->cppi;
    cpphy_mdio_t *mdio = &((cpphy_global_t *)phy_handle)->mdio;
    unsigned char priority;
    unsigned int frame_length;
    cpphy_tcb_t *tcb;

    if(unlikely(!cppi->TxOpen || cppi->TxTeardownPending || (mdio->state != CPPHY_MDIO_ST_LINKED))) {
        DEB_INFO("[%s] chan closing or not opened (%u:%u)\n", __FUNCTION__,
            cppi->hw_state, cppi->TxTeardownPending);
        return CPMAC_ERR_CHAN_NOT_OPEN;
    }

    /******************************************\
     * Part 1: Add skb(s) to correct queue(s) *
    \******************************************/

    /* Check which queue to use */
    priority = (skb->uniq_id >> 24) & 0xff;
    assert(priority < CPPHY_PRIO_QUEUES);

    if(atomic_read(&cppi->TxPrioQueues.q[priority].Free) == 0) {
        DEB_INFOTRC("[%s] No priority queue (%u) entry free!\n", __FUNCTION__, priority);
        return CPMAC_ERR_NO_BUFFER;
    }

    /*--- if(cppi->TxPrioQueues.q[priority].Free != 0) { ---*/
    /* prepare tx data to be available for dma */
    dma_cache_wback_inv((unsigned long) skb->data, skb->len);

    DEB_DEBUG("[%s] (%u) %*pB ...\n", __FUNCTION__, skb->len, min(24u, skb->len), skb->data);

    /* supply min ether frame size */
    frame_length = skb->len;
    if (frame_length < 60) {
        frame_length = 60;  /* + 4 byte hardware added fcs -> min frame length of 64 bytes */
    }

    /* Allocate tcb, set it up with the skb data */
    tcb = cpphy_if_alloc_tcb(cppi);
    if(unlikely(tcb == NULL)) {
        DEB_ERR("[%s] Could not allocate tcb!\n", __FUNCTION__);
        return CPMAC_ERR_NO_BUFFER;
    }

    tcb->Tags.Dword         = 0;
    tcb->Packet.Dword       = 0;
    tcb->Packet.Bits.packet_type = PACKET_TYPE_ETHERNET;
    tcb->Packet.Bits.buffer_length = frame_length;
    tcb->pData              = CPPHY_VIRT_TO_PHYS((unsigned int *)skb->data);
    tcb->Buffer.Bits.length = frame_length;
    tcb->Buffer.Bits.offset = 0;
    tcb->NextPacket         = NULL;
    tcb->NextDescr          = NULL;
    tcb->skb                = skb;
    barrier();
    /* Cache write back happens directly before enqueueing the tcb to the DMA */

    /* Enqueue tcb to the corresponding priority queue */
    cpphy_if_tcb_enqueue(cppi, priority, tcb);
    cppi->TxPrioQueues.q[priority].BytesEnqueued += skb->len;
    cpphy_if_data_from_queues(cppi);

    return CPMAC_ERR_NOERR;
}


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
static cpmac_err_t cpphy_if_g_enqueue_rcb(cpphy_cppi_t *cppi, cpphy_rcb_t *rcb) {
    char *pBuf;

    do {
        if(unlikely(rcb == NULL)) {
            rcb = unused_rcbs;
            unused_rcbs = unused_rcbs->Next;
        }

        memset(rcb, 0, sizeof(cpphy_rcb_t));

        /* Allocate buffer and enter data into packet descriptor */
        pBuf = (char *) cpphy_cppi_malloc_buffer(CPPHY_MAX_RX_BUFFER_SIZE,
                                                 CPPHY_TOTAL_RX_BUFFER_SIZE,
                                                 CPPHY_TOTAL_RX_RESERVED,
                                                 cppi->cpmac_priv->owner,
                                                 (struct sk_buff **) &rcb->skb);
        if(unlikely(!pBuf)) {
            DEB_ERR("[%s] Not enough memory for receive buffers!\n", __FUNCTION__);
            rcb->Next = unused_rcbs;
            unused_rcbs = rcb;
            return CPMAC_ERR_NO_BUFFER;
        }

        /* Null/0 need not be written because of the memset above */
        /*--- rcb->Packet.Bits.packet_type = PACKET_TYPE_ETHERNET; ---*/
        /*--- rcb->Packet.Bits.buffer_length = CPPHY_MAX_RX_BUFFER_SIZE; ---*/
        rcb->Buffer.Bits.length = CPPHY_MAX_RX_BUFFER_SIZE;
        rcb->pData = CPPHY_VIRT_TO_PHYS(pBuf);
        dma_cache_wback_inv((unsigned long) rcb, sizeof(cpphy_rcb_t));
        barrier();

        /* Enqueue packet descriptors */
        cppi->cpmac_priv->UR8_QUEUE->free_db_queue[UR8_RX_FREE_QUEUE].pointer = CPPHY_VIRT_TO_PHYS((unsigned int) rcb);

        rcb = NULL;
    } while(unlikely(unused_rcbs != NULL));

    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
 * main handler of receive interrupts
\*------------------------------------------------------------------------------------------*/
static unsigned int cpphy_if_g_rx_int(cpphy_cppi_t *cppi) {
    cpphy_rcb_t *rcb;
    cpphy_rcb_t *last_rcb;

    rcb = (cpphy_rcb_t *) CPPHY_PHYS_TO_VIRT_NO_CACHE(cppi->cpmac_priv->UR8_QUEUE->rx_queue[UR8_RX_QUEUE].prio[0]);
#   if defined(CONFIG_AVM_SIMPLE_PROFILING)
    avm_simple_profiling_log(avm_profile_data_type_cpphyrx_begin, (unsigned int)cpphy_if_g_rx_int, (unsigned int)rcb);
#   endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
    if(rcb != (cpphy_rcb_t *) CPPHY_PHYS_TO_VIRT_NO_CACHE(0)) {
        while(rcb != (cpphy_rcb_t *) CPPHY_PHYS_TO_VIRT_NO_CACHE(0)) {
            if(unlikely(((unsigned int) rcb) & 0x1)) {
                /* LSB is set to signal teardown complete */
#               if (CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_INFOTRACE)
                struct __nwss_td_desc *tdd = (struct __nwss_td_desc *) ((unsigned int) rcb ^ 0x1);
#               endif /*--- #if (CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_INFOTRACE) ---*/
                DEB_INFOTRC("[%s] rx channel %u teardown complete!\n", __FUNCTION__, tdd->channel_no);
                cppi->RxTeardownPending &= ~CPPHY_RX_TEARDOWN;
                break;
            }
            if(unlikely(rcb->Buffer.Bits.offset != 0)) { /* TODO */
                DEB_ERR("[%s] did not expect offset in rx buffer: 0x%x\n", __FUNCTION__, rcb->Buffer.Bits.offset);
            }
            if(unlikely(rcb->Buffer.Bits.length != rcb->Packet.Bits.buffer_length)) {
                DEB_ERR("[%s] found different buffer lengths: buffer 0x%x, packet 0x%x\n", __FUNCTION__,
                        rcb->Buffer.Bits.length, rcb->Packet.Bits.buffer_length);
            }
            if(unlikely(rcb->NextDescr != NULL)) { /* TODO */
                DEB_ERR("[%s] did not expect further descriptors in rx buffer 0x%p\n", __FUNCTION__, rcb->NextDescr);
            }
            /* TODO Check, why there were four bytes too many UR8? Tantos? */
#           if defined(CONFIG_AVM_SIMPLE_PROFILING)
            skb_trace(rcb->skb, 22);
#           endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/
            if(likely(rcb->skb)) {
                cpmac_if_data_from_phy(cppi->cpmac_priv, (struct sk_buff *) rcb->skb, rcb->Buffer.Bits.length - 4);
            } else {
                DEB_ERR("[%s] Received rcb %p with skb == NULL !\n", __FUNCTION__, rcb);
            }
            last_rcb = rcb;
            rcb = (cpphy_rcb_t *) CPPHY_PHYS_TO_VIRT_NO_CACHE((unsigned int) rcb->NextPacket);
            cpphy_if_g_enqueue_rcb(cppi, last_rcb);
        }
        /* Acknowledge interrupt */
        cppi->cpmac_priv->UR8_QUEUE->rx_int_end.Bits.cq_num = UR8_RX_QUEUE;
    }
#   if defined(CONFIG_AVM_SIMPLE_PROFILING)
    avm_simple_profiling_log(avm_profile_data_type_cpphyrx_end, (unsigned int)cpphy_if_g_rx_int, (unsigned int)rcb);
#   endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/

    return 0;
}

/*------------------------------------------------------------------------------------------*\
 * Handler for transmit complete interrupts
\*------------------------------------------------------------------------------------------*/
static unsigned int cpphy_if_g_tx_int(cpphy_cppi_t *cppi) {
    cpphy_tcb_t *tcb, *next_tcb;

#   if defined(CONFIG_AVM_SIMPLE_PROFILING)
    avm_simple_profiling_log(avm_profile_data_type_cpphytx_begin, (unsigned int)cpphy_if_g_tx_int, (unsigned int)0);
#   endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/

    /* Walk the list of completed packets */
    while(   (tcb = (cpphy_tcb_t *) CPPHY_PHYS_TO_VIRT_NO_CACHE(cppi->cpmac_priv->UR8_QUEUE->tx_completion_queue[UR8_TX_COMPLETE]))
          != (cpphy_tcb_t *) CPPHY_PHYS_TO_VIRT_NO_CACHE(0)) {
        while(tcb != (cpphy_tcb_t *) CPPHY_PHYS_TO_VIRT_NO_CACHE(0)) {
            if(unlikely((unsigned int) tcb & 0x1)) {
                /* LSB is set to signal teardown complete */
#               if (CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_INFOTRACE)
                struct __nwss_td_desc *tdd = (struct __nwss_td_desc *) ((unsigned int) tcb ^ 0x1);
#               endif /*--- #if (CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_INFOTRACE) ---*/
                DEB_INFOTRC("[%s] tx channel %u teardown complete!\n", __FUNCTION__, tdd->channel_no);
                cppi->TxTeardownPending &= ~CPPHY_TX_TEARDOWN;
                break;
            }

            if(unlikely(tcb->NextDescr != NULL)) {
                /* We use single buffers to transmit packets */
                DEB_ERR("[%s] Did not expect tcb->NextDescr != NULL!\n", __FUNCTION__);
            }
            cpphy_if_tx_complete(cppi, (struct sk_buff *) tcb->skb, CPMAC_ERR_NOERR);
            tcb->skb = NULL; /* tcb may be reused immediately */
            next_tcb = (cpphy_tcb_t *) CPPHY_PHYS_TO_VIRT_NO_CACHE((unsigned int) tcb->NextPacket);
            cpphy_if_free_tcb(cppi, tcb);
            tcb = next_tcb;
        }

        {
            struct net_device *p_dev = cppi->cpmac_priv->owner;
            struct Qdisc *q = p_dev->qdisc;

            if(netif_queue_stopped(p_dev)) {
                if(likely(!test_bit(0, &cppi->cpmac_priv->set_to_close))) {
                    cpphy_if_tx_restart_queue(cppi);
                }
            } else if(q->q.qlen) {
#               if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
                /*--- napi_schedule(&cppi->cpmac_priv->napi); ---*/
                __netif_schedule(q);
#               else /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
                netif_schedule(p_dev);
#               endif /*--- #else ---*/ /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
            }
        }
        /* Acknowledge interrupt */
        cppi->cpmac_priv->UR8_QUEUE->tx_int_end.Bits.cq_num = UR8_TX_COMPLETE;
    }
#   if defined(CONFIG_AVM_SIMPLE_PROFILING)
    avm_simple_profiling_log(avm_profile_data_type_cpphytx_end, (unsigned int)cpphy_if_g_tx_int, (unsigned int)0);
#   endif /*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING) ---*/

    return 0;
}


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
void cpphy_if_g_isr_tasklet(unsigned long context) {
    cpmac_phy_handle_t phy_handle = (cpmac_phy_handle_t)context;
    cpphy_cppi_t *cppi = &((cpphy_global_t *) phy_handle)->cppi;

    /* Verify proper device state - important because a call prior to Open would *\
    \* result in a lockup                                                        */
    if(unlikely(cppi->hw_state != CPPHY_HW_ST_OPENED)) {
        DEB_TRC("[%s] Tasklet called, but state is not open!\n", __FUNCTION__);
        return;
    }

    /* First receive packets */
    cpphy_if_g_rx_int(cppi);

    /* Now clean up completed tx packets */
    cpphy_if_g_tx_int(cppi);

    /* Make it possible to replenish rcbs with skbs even without receiving a packet */
    if(unlikely(unused_rcbs != NULL)) {
        cpphy_if_g_enqueue_rcb(cppi, NULL);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_if_g_init_rcb(cpphy_cppi_t *cppi, int Num) {
    int i;
    cpphy_rcb_t *rcb;
    char *AllRcb;
    int rcbSize;
    int size_malloc;
    cpmac_err_t ret = CPMAC_ERR_NOERR;

    /* Align on 64 byte boundary for UR8 */
    rcbSize = (sizeof(cpphy_rcb_t) + 0x3f) &~ 0x3f;
    size_malloc = (rcbSize * Num) + 0x3f;

    if(!(AllRcb = (char *) kmalloc(size_malloc, GFP_KERNEL))) {
        DEB_ERR("[%s] Out of memory!\n", __func__);
        ret = CPMAC_ERR_NOMEM;
    } else {
        dma_cache_wback_inv((unsigned long) AllRcb, size_malloc);
        cppi->MaxNeedCount = Num - 1;   /* need one complete buff to still get rx int */

        /* keep this address for freeing later */
        cppi->RcbStart = AllRcb;

        AllRcb = (char *) CPPHY_VIRT_TO_VIRT_NO_CACHE(AllRcb);
        memset(AllRcb, 0, size_malloc);

        /* align to cache line */
        AllRcb = (char *) (((unsigned int) AllRcb + 0x3f) & ~0x3f);
        rcb = (cpphy_rcb_t *) AllRcb;
        DEB_TRC("[%s] Initialize rx free buffer/descriptor queue\n", __FUNCTION__);
        cppi->cpmac_priv->UR8_QUEUE->free_db_queue[UR8_RX_FREE_QUEUE].size = CPPHY_MAX_RX_BUFFER_SIZE;
        for(i = 0; i < Num; i++) {
            if(CPMAC_ERR_NOERR != cpphy_if_g_enqueue_rcb(cppi, rcb)) {
                DEB_ERR("[%s] Out of memory on enqueue!\n", __func__);
                return CPMAC_ERR_NOMEM;
            }
            rcb = (cpphy_rcb_t *) (((unsigned char *) rcb) + rcbSize);
        }
    }
    return ret;
}

#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/

