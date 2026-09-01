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

#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <net/pkt_sched.h>
#include <linux/delay.h>
#include <asm/mach_avm.h>
/*--- #include <asm/mach-ohio/led_hal.h> ---*/ /* FIXME Is this still needed? */

#if defined(CONFIG_MIPS_UR8)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
#include <asm/mach-ur8/ur8.h>
#else /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
#include <asm/mips-boards/ur8.h>
#endif /*--- #else ---*/ /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/

#include <asm/mach-ur8/hw_nwss.h>
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/

#include <asm/io.h>

#if !defined(CONFIG_NETCHIP_ADM69961)
#define CONFIG_NETCHIP_ADM69961
#endif
#include <linux/avm_event.h>

#include "cpmac_if.h"
#include "cpmac_const.h"
#include "cpmac_debug.h"
#include "cpphy_const.h"
#include "cpphy_types.h"
#include "cpphy_cppi.h"
#include "cpphy_if.h"
#include "cpphy_if_g.h"
#include "cpmac_reg.h"


/* swap mac addr with [5 - ()] */
#define DA(a, bit) ((unsigned int)(((a)[5-((bit)>>3)] >> ((bit) & 7)) & 1))

#if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
/*------------------------------------------------------------------------------------------*\
 * according to cpmac manual
\*------------------------------------------------------------------------------------------*/
static unsigned int cpphy_cppi_hash_fun(unsigned char *a) {
    unsigned int hash;

    hash  =  DA(a,0) ^ DA(a, 6) ^ DA(a,12) ^ DA(a,18) ^ DA(a,24) ^ DA(a,30) ^ DA(a,36) ^ DA(a,42);
    hash |= (DA(a,1) ^ DA(a, 7) ^ DA(a,13) ^ DA(a,19) ^ DA(a,25) ^ DA(a,31) ^ DA(a,37) ^ DA(a,43)) << 1;
    hash |= (DA(a,2) ^ DA(a, 8) ^ DA(a,14) ^ DA(a,20) ^ DA(a,26) ^ DA(a,32) ^ DA(a,38) ^ DA(a,44)) << 2;
    hash |= (DA(a,3) ^ DA(a, 9) ^ DA(a,15) ^ DA(a,21) ^ DA(a,27) ^ DA(a,33) ^ DA(a,39) ^ DA(a,45)) << 3;
    hash |= (DA(a,4) ^ DA(a,10) ^ DA(a,16) ^ DA(a,22) ^ DA(a,28) ^ DA(a,34) ^ DA(a,40) ^ DA(a,46)) << 4;
    hash |= (DA(a,5) ^ DA(a,11) ^ DA(a,17) ^ DA(a,23) ^ DA(a,29) ^ DA(a,35) ^ DA(a,41) ^ DA(a,47)) << 5;
    return hash;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_cppi_add_hash(cpphy_cppi_t *cppi, unsigned char *MacAddress) {
    unsigned int HashValue;
    unsigned int HashBit;

    HashValue = cpphy_cppi_hash_fun (MacAddress);
    if(HashValue < 32) {
        HashBit = (1 << HashValue);
        cppi->hash1 |= HashBit;
    } else {
        HashBit = (1 << (HashValue - 32));
        cppi->hash2 |= HashBit;
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_cppi_set_multi_promiscous(cpphy_cppi_t *cppi __attribute__ ((unused)),
                                     unsigned int  multi __attribute__ ((unused)),
                                     unsigned int  promiscous __attribute__ ((unused))) {
    struct net_device *p_dev = cppi->cpmac_priv->owner;

    if(cppi->hw_state != CPPHY_HW_ST_OPENED) {
        DEB_WARN("[%s] illegal state %u\n", __FUNCTION__, cppi->hw_state);
    } else {
        int i;
#       if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
        unsigned int RxMbpEnable;
#       endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/

        cppi->hash1 = 0;
        cppi->hash2 = 0;
        if(multi) {
            if(multi == 1) {
                cppi->hash1 = 0xffffffff;
                cppi->hash2 = 0xffffffff;
            } else {  /* list of multicast addresses */
                struct dev_mc_list *p_dmi = p_dev->mc_list;

                for(i = 0; i < p_dev->mc_count; i++, p_dmi = p_dmi->next) {
                    DEB_INFO("[%s] add %02x:%02x:%02x:%02x:%02x:%02x\n",
                             __FUNCTION__,
                             p_dmi->dmi_addr[0],
                             p_dmi->dmi_addr[1],
                             p_dmi->dmi_addr[2],
                             p_dmi->dmi_addr[3],
                             p_dmi->dmi_addr[4],
                             p_dmi->dmi_addr[5]);
                    cpphy_cppi_add_hash(cppi, p_dmi->dmi_addr);
                }
            }
        }
#       if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
        RxMbpEnable = CPMAC_RX_MBP_ENABLE(p_dev->base_addr);
        RxMbpEnable &= ~(RX_CAF_EN | RX_MULT_EN);
        if(multi)
            RxMbpEnable |= RX_MULT_EN;
        if(promiscous)
            RxMbpEnable |= RX_CAF_EN;
        CPMAC_RX_MBP_ENABLE(p_dev->base_addr) = RxMbpEnable;
        CPMAC_MACHASH1(p_dev->base_addr) = cppi->hash1;
        CPMAC_MACHASH2(p_dev->base_addr) = cppi->hash2;
        DEB_TRC("[%s] MBP_ENABLE 0x%08X\n", __FUNCTION__, CPMAC_RX_MBP_ENABLE(p_dev->base_addr));
#       elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
        /* Promiscous mode for channel 0 */
        cppi->cpmac_priv->CPGMAC_F->RX_MBP_ENABLE.Bits.rx_caf_en = promiscous ? 1 : 0;
        cppi->cpmac_priv->CPGMAC_F->RX_MBP_ENABLE.Bits.rx_prom_ch = 0;
        /* Enable multicast rx for channel 0 */
        cppi->cpmac_priv->CPGMAC_F->RX_MBP_ENABLE.Bits.rx_mult_en = multi ? 1 : 0;
        cppi->cpmac_priv->CPGMAC_F->RX_MBP_ENABLE.Bits.rx_mult_ch = 0;
        cppi->cpmac_priv->CPGMAC_F->MAC_HASH1 = cppi->hash1;
        cppi->cpmac_priv->CPGMAC_F->MAC_HASH2 = cppi->hash2;
        DEB_TRC("[%s] MBP_ENABLE 0x%08X (%s, %s)\n", __FUNCTION__,
                cppi->cpmac_priv->CPGMAC_F->RX_MBP_ENABLE.Reg,
                promiscous ? "promiscous" : "not promiscous",
                multi ? "multicast" : "no multicast");
#       else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#       warning  "Multicast and promiscous mode needed for this architecture"
#       endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
    }
}


/*----------------------------------------------------------------------------------*\
  add counter to stats (may be preinitialized by cpphy_mdio_update_hw_status())
\*----------------------------------------------------------------------------------*/
void cpphy_cppi_update_hw_status(cpphy_cppi_t *cppi __attribute__ ((unused)),
                                 struct net_device_stats *stats __attribute__ ((unused))) {
    unsigned int rx_crc_errors, rx_frame_errors, rx_length_errors, rx_over_errors,
                 rx_fifo_errors, rx_missed_errors, tx_carrier_errors, tx_fifo_errors,
                 collisions;

#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    struct net_device *p_dev = cppi->cpmac_priv->owner;

    stats->multicast += CPMAC_RXMULTICASTFRAMES(p_dev->base_addr);

    rx_crc_errors = CPMAC_RXCRCERRORS(p_dev->base_addr);
    /* received frame alignment error */
    rx_frame_errors = CPMAC_RXALIGNCODEERRORS(p_dev->base_addr);
    rx_length_errors = CPMAC_RXJABBERFRAMES(p_dev->base_addr);
    rx_length_errors += CPMAC_RXUNDERSIZEDFRAMES(p_dev->base_addr);
    rx_length_errors += CPMAC_RXFRAGMENTS(p_dev->base_addr);
    /* receiver ring buffer overflow */
    rx_over_errors = CPMAC_RXDMAOVERRUNS(p_dev->base_addr);
    /* receiver fifo overrun */
    rx_fifo_errors =   CPMAC_RXSOFOVERRUNS(p_dev->base_addr)
                     + CPMAC_RXMOFOVERRUNS(p_dev->base_addr)
                     - rx_over_errors;
    /* receiver missed packet: to check */
    rx_missed_errors = CPMAC_RXFILTEREDFRAMES(p_dev->base_addr);

    DEB_TRC("cpphy_cppi_update_hw_status, %u, %u, %u, %u, %u, %u, %u, %u, %u\n",
            CPMAC_RXCRCERRORS(p_dev->base_addr),
            CPMAC_RXALIGNCODEERRORS(p_dev->base_addr),
            CPMAC_RXJABBERFRAMES(p_dev->base_addr),
            CPMAC_RXUNDERSIZEDFRAMES(p_dev->base_addr),
            CPMAC_RXFRAGMENTS(p_dev->base_addr),
            CPMAC_RXDMAOVERRUNS(p_dev->base_addr),
            CPMAC_RXSOFOVERRUNS(p_dev->base_addr),
            CPMAC_RXMOFOVERRUNS(p_dev->base_addr),
            CPMAC_RXFILTEREDFRAMES(p_dev->base_addr));

    /* detailed tx_errors */
    tx_carrier_errors = CPMAC_TXCARRIERSENSEERRORS(p_dev->base_addr);
    tx_fifo_errors = CPMAC_TXUNDERRUN(p_dev->base_addr);

    collisions =   CPMAC_TXEXCESSIVECOLLISIONS(p_dev->base_addr)
                 + CPMAC_TXLATECOLLISIONS(p_dev->base_addr);
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    stats->multicast += cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxMulticastFrames;

    rx_crc_errors = cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxCRCErrors;
    /* received frame alignment error */
    rx_frame_errors = cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxAlignCodeErrors;
    rx_length_errors = cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxJabberFrames;
    rx_length_errors += cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxUndersizedFrames;
    rx_length_errors += cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxOversizedFrames;
    rx_length_errors += cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxFragments;
    /* receiver ring buff overflow */
    rx_over_errors = cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxDmaOverruns;
    /* receiver fifo overrun */
    rx_fifo_errors =   cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxSofOverruns
                     + cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxMofOverruns
                     - rx_over_errors;
    /* receiver missed packet: to check */
    rx_missed_errors = cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxFilteredFrames;

    DEB_TRC("cpphy_cppi_update_hw_status, %u, %u, %u, %u, %u, %u, %u, %u, %u\n",
        cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxCRCErrors,
        cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxAlignCodeErrors,
        cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxJabberFrames,
        cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxUndersizedFrames,
        cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxFragments,
        cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxDmaOverruns,
        cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxSofOverruns,
        cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxMofOverruns,
        cppi->cpmac_priv->CPGMAC_F->STATISTIC.RxFilteredFrames);

    /* detailed tx_errors */
    tx_carrier_errors = cppi->cpmac_priv->CPGMAC_F->STATISTIC.TxCarrierSenseErrors;
    tx_fifo_errors = cppi->cpmac_priv->CPGMAC_F->STATISTIC.TxUnderrun;

    collisions =   cppi->cpmac_priv->CPGMAC_F->STATISTIC.TxExcessiveCollisions
                 + cppi->cpmac_priv->CPGMAC_F->STATISTIC.TxLateCollisions;
#   else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#   warning "No update_hw_status defined for this architecture"
#   endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
    stats->rx_crc_errors += rx_crc_errors;
    stats->rx_frame_errors += rx_frame_errors;
    stats->rx_length_errors += rx_length_errors;
    stats->rx_over_errors += rx_over_errors;
    stats->rx_fifo_errors += rx_fifo_errors;
    stats->rx_missed_errors += rx_missed_errors;
    stats->rx_errors +=   rx_length_errors
                        + rx_crc_errors
                        + rx_frame_errors
                        + rx_missed_errors;
    stats->rx_dropped += rx_fifo_errors + rx_over_errors;

    /* detailed tx_errors */
    stats->tx_carrier_errors += tx_carrier_errors;
    stats->tx_fifo_errors += tx_fifo_errors;

    stats->collisions += collisions;
    stats->tx_errors += tx_carrier_errors + tx_fifo_errors + collisions;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void *cpphy_cppi_malloc_buffer(unsigned int size,
                               unsigned int tot_buf_size,
                               unsigned int tot_reserve_bytes,
                               struct net_device *p_dev,
                               struct sk_buff **skb) {
    if((*skb = dev_alloc_skb(tot_buf_size))) {
        (*skb)->dev = p_dev;
        skb_reserve(*skb, tot_reserve_bytes);
        return skb_put(*skb, size);
    }
    return NULL;
}
#endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
static void *cpphy_cppi_malloc_buffer_startup(unsigned int size,
                                              unsigned int tot_buf_size,
                                              unsigned int tot_reserve_bytes,
                                              struct net_device *p_dev,
                                              struct sk_buff **skb) {
    if((*skb = alloc_skb(tot_buf_size, GFP_KERNEL))) {
        (*skb)->dev = p_dev;
        skb_reserve(*skb, tot_reserve_bytes);
        return skb_put(*skb, size);
    }
    return NULL;
}
#endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
#if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
void cpphy_cppi_free_rcb(cpphy_cppi_t *cppi) {
    cpphy_rcb_t *rcb;

    /* Free Rx data buffers attached to descriptors, if necessary */
    rcb = cppi->RxPrevEnqueue;
    do {
        if(rcb->skb) {
            dev_kfree_skb_any((struct sk_buff *) rcb->skb);
            rcb->skb = 0;
        }
        rcb = (cpphy_rcb_t *) rcb->Next;
    } while(rcb && (rcb != cppi->RxPrevEnqueue));

    /* free up all desciptors at once */
    kfree(cppi->RcbStart);
    cppi->RcbStart = 0;
}


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
void cpphy_cppi_free_tcb(cpphy_cppi_t *cppi) {
    /* free all descriptors at once */
    kfree(cppi->TcbStart);
    cppi->TcbStart = 0;
}
#endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/


/*----------------------------------------------------------------------------------*\
  Transmit buffer descriptor allocation
\*----------------------------------------------------------------------------------*/
#if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
static cpmac_err_t cpphy_cppi_init_tcb(cpphy_cppi_t *cppi) {
    cpmac_err_t ret = CPMAC_ERR_NOERR;
    unsigned int i;
    unsigned int Num = CPPHY_MAX_TX_BUFFERS;
    cpphy_tcb_t *pTcb = NULL;
    char *AllTcb;
    int  tcbSize, size_malloc;

    DEB_TRC("[%s]\n", __func__);

    tcbSize = (sizeof (cpphy_tcb_t) + 0x3f) & ~0x3f;
    size_malloc = (tcbSize * Num) + 0x3f;

    /* if the memory has already been allocated, simply reuse it! */
    if(!(AllTcb = cppi->TcbStart)) {
        DEB_TRC("[%s] Allocating block for tcbs\n", __func__);
        /* malloc all TCBs at once */
        if(!(AllTcb = (char *) kmalloc (size_malloc, GFP_KERNEL))) {
            ret = CPMAC_ERR_NOMEM;
        } else {
            dma_cache_wback_inv((unsigned long) AllTcb, size_malloc);
            /* keep this address for freeing later */
            cppi->TcbStart = AllTcb;
            AllTcb = (char *) CPPHY_VIRT_TO_VIRT_NO_CACHE(AllTcb);
            memset(AllTcb, 0, size_malloc);
        }
    }
    if(AllTcb) {
        DEB_TRC("[%s] Generating tcbs in allocated area\n", __func__);

        /* align to cache line */
        AllTcb = (char *) (((unsigned int) AllTcb + 0x3f) & ~0x3f);

        if(Num <= cppi->NeededDMAtcbs) {
            DEB_WARN("[%s] Need more DMA tcbs (%u) than are available (%u)!\n", __FUNCTION__,
                     cppi->NeededDMAtcbs, Num);
            return CPMAC_ERR_NOMEM;
        }
        cppi->TxFirst = (cpphy_tcb_t *) AllTcb; /* FIXME Is this correct? This is not the malloced address! */
        cppi->TxDmaActive = 0;
        atomic_set(&cppi->dma_send_running, 0);
        atomic_set(&cppi->dequeue_running, 0);

#       if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
        /* design: descriptors as logical ring buffer (with HW next ptr NULL terminated) */
        /* TxPrevEnqueue: previously enqueued tx skb (dma active) */
        /* TxCurrDequeue: next tx skb to be send completed */
        cppi->TxCurrDequeue = (cpphy_tcb_t *) cppi->TxFirst;
        cppi->TxPrevEnqueue = (cpphy_tcb_t *) (AllTcb + ((Num - 1) * tcbSize));
        cppi->TxLast = cppi->TxPrevEnqueue;

        /* First build the ring buffer for DMA */
        pTcb = (cpphy_tcb_t *) cppi->TxFirst;
        for(i = 0; i < cppi->NeededDMAtcbs; i++) {
            pTcb->mode = CB_EOQ_BIT;   /* design: start DMA initially */
            pTcb->Next = (cpphy_tcb_t *) (((unsigned char *) pTcb) + tcbSize);
            pTcb = (cpphy_tcb_t *) pTcb->Next;
        }
        cppi->TxPrevEnqueue = pTcb;
        cppi->TxPrevEnqueue->Next = cppi->TxFirst;

        /* Now enqueue the preallocated tcbs for normal usage */
        pTcb = (cpphy_tcb_t *) (((unsigned char *) pTcb) + tcbSize);
        cppi->TxFirstFree = pTcb;
        cppi->TxLastFree = cppi->TxFirstFree;
        for(i = 0; i < Num - cppi->NeededDMAtcbs - 1; i++) {
            pTcb = (cpphy_tcb_t *)(((unsigned char *) pTcb) + tcbSize);
            cpphy_if_free_tcb(cppi, pTcb);
        }
#       elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
        cppi->TxFirstFree = (volatile cpphy_tcb_t *) AllTcb;
        cppi->TxLastFree = cppi->TxFirstFree;

        pTcb = (cpphy_tcb_t *) cppi->TxFirstFree;
        for(i = 0; i < Num - 1; i++) {
            pTcb = (cpphy_tcb_t *)(((unsigned char *) pTcb) + tcbSize);
            cpphy_if_free_tcb(cppi, pTcb);
        }
#       else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#       warning "Missing init_tcb for this architecture!"
#       endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
    }
    cppi->support.tcbs_freed = 0;
    return ret;
}
#endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
#if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) /* Not needed for this architecture */
cpmac_err_t cpphy_cppi_start_dma(cpphy_cppi_t *cppi __attribute__ ((unused))) {
    cpmac_err_t ret = CPMAC_ERR_NOERR;
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    struct net_device *p_dev = (struct net_device *) cppi->cpmac_priv->owner;
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/

    DEB_TRC("[%s] init\n", __func__);
    if(cppi->hw_state < CPPHY_HW_ST_OPENED) {
        /* hardware has never been opened, leave immediately */
        DEB_WARN("[%s] hw not initialized (state %u)\n", __FUNCTION__, cppi->hw_state);
        ret = CPMAC_ERR_HW_NOT_INITIALIZED;
    } else {
        if(cppi->TxOpen) {
            DEB_WARN("[%s] tx, already open\n", __FUNCTION__);
        } else {
            /* Initialize buffer memory for the tx handling */
            if((ret = cpphy_cppi_init_tcb(cppi)) == CPMAC_ERR_NOERR) {
#               if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
                CPMAC_TX_INTMASK_SET(p_dev->base_addr) = (1 << cppi->TxChannel);
#               elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
                cppi->cpmac_priv->UR8_QUEUE->tx_int_enable_set.Bits.txcq0_int_enable = 1;
#               else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#               warning "Tx-IRQ-Setup for unknown architecture missing"
#               endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
                cppi->TxOpen = 1;
            } else {
                DEB_ERR("[%s] failed!\n", __FUNCTION__);
            }
        }
        if(ret == CPMAC_ERR_NOERR) {
            if(cppi->RxOpen) {
                DEB_WARN("[%s] rx, already open\n", __FUNCTION__);
            } else {
                /* Initialize buffer memory for the rx handling */
#               if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
                if((ret = cpphy_cppi_init_rcb(cppi, CPPHY_MAX_RX_BUFFERS, CPPHY_MAX_RX_BUFFER_SIZE)) == CPMAC_ERR_NOERR) {
                    CPMAC_RX_INTMASK_SET(p_dev->base_addr) = (1 << 0);
#               elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
                if((ret = cpphy_if_g_init_rcb(cppi, CPPHY_MAX_RX_BUFFERS)) == CPMAC_ERR_NOERR) {
                    cppi->cpmac_priv->UR8_QUEUE->rx_int_enable_set.Register = (1 << UR8_RX_QUEUE);
#               else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#               warning "Rx-IRQ-Setup for unknown architecture missing"
                if(0) {
#               endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
                    cppi->RxOpen = 1;
                } else {
                    DEB_ERR("[%s] cpphy_cppi_init_rcb failed!\n", __func__);
                    if(cppi->TxOpen) {
                        cppi->TxOpen = 0;
                        cpphy_cppi_free_tcb(cppi);
                    }
                }
            }
        }
    }
    return ret;
}
#endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/ /* Not needed for this architecture */


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_cppi_rx_dma_pause(cpphy_cppi_t *cppi __attribute__ ((unused))) {
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    struct net_device *p_dev = cppi->cpmac_priv->owner;
    CPMAC_RX_INTMASK_CLEAR(p_dev->base_addr) = (1 << 0);
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    cppi->cpmac_priv->UR8_QUEUE->rx_int_enable_clear.Register = (1 << UR8_RX_QUEUE);
#   endif /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_cppi_rx_dma_restart(cpphy_cppi_t *cppi __attribute__ ((unused))) {
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    struct net_device *p_dev = cppi->cpmac_priv->owner;
    CPMAC_RX_INTMASK_SET(p_dev->base_addr) = (1 << 0);
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    cppi->cpmac_priv->UR8_QUEUE->rx_int_enable_set.Register = (1 << UR8_RX_QUEUE);
#   endif /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
}


/*----------------------------------------------------------------------------------*\
 * Start of Teardown.
\*----------------------------------------------------------------------------------*/
#if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
cpmac_err_t cpphy_cppi_teardown(cpphy_cppi_t *cppi, unsigned int Mode) {
    int timeout = 0;
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    struct net_device *p_dev = cppi->cpmac_priv->owner;
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/

    if(Mode & CPPHY_TX_TEARDOWN) {
        if(cppi->TxTeardownPending) {
            DEB_WARN("[%s] tx already pending\n", __func__);
        } else {
            if(cppi->hw_state < CPPHY_HW_ST_OPENED) {
                /* hardware has never been opened, leave immediately */
                DEB_INFO("[%s] tx, hw in state %u\n", __FUNCTION__, cppi->hw_state);
            } else if(!cppi->TxOpen) {
                DEB_WARN("[%s] tx, already torn down\n", __FUNCTION__);
            } else {
                /* set teardown flag for int handler */
                cppi->TxTeardownPending = Mode;

                /* request TX channel teardown */
#               if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
                (CPMAC_TX_TEARDOWN(p_dev->base_addr)) = cppi->TxChannel;
#               elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
                cpmac_if_reset_tx_queues(cppi->cpmac_priv);
                cppi->cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].tx_B.Bits.teardown = 1;
#               endif /*--- #elif defined(CONFIG_MIPS_UR8) ---*/

                /* if mode is blocking: wait until teardown has completed */
                if(Mode & CPPHY_BLOCKING_TEARDOWN) {
                    timeout = 0;
                    while(cppi->TxTeardownPending & CPPHY_TX_TEARDOWN) {
                        msleep_interruptible(2);
                        timeout++;
                        if(timeout > 100) {
                            DEB_WARN("[%s] break tx wait\n", __func__);
                            break;
                        }
                    }
                }
#               if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0)
                if(cppi->TxTeardownPending & CPPHY_CALLBACK_TEARDOWN) {
                    cppi->TxTeardownPending = 0;
                    cpmac_if_teardown_complete(cppi->cpmac_priv);
                }
#               endif /*--- #if defined(CPMAC_TX_TIMEOUT) && (CPMAC_TX_TIMEOUT > 0) ---*/
                cppi->TxTeardownPending = 0;
            }
        }
    }

    if(Mode & CPPHY_RX_TEARDOWN) {
        if(cppi->RxTeardownPending) {
            DEB_WARN("[%s] rx already pending\n", __func__);
        } else {
            if(cppi->hw_state < CPPHY_HW_ST_OPENED) {
                DEB_INFO("[%s] rx, hw in state %u\n", __FUNCTION__, cppi->hw_state);
            } else if(!cppi->RxOpen) {
                /* hardware has never been opened, leave immediately */
                DEB_WARN("[%s] rx, already torn down\n", __func__);
            } else {
                /* set teardown flag for int handler */
                cppi->RxTeardownPending = Mode;

                /* request RX channel teardown */
#               if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
                CPMAC_RX_TEARDOWN(p_dev->base_addr) = 0;
#               elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
                cppi->cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].rx_B.Bits.teardown = 1;
#               endif /*--- #elif defined(CONFIG_MIPS_UR8) ---*/

                /* if mode is blocking: wait until teardown has completed */
                if(Mode & CPPHY_BLOCKING_TEARDOWN) {
                    timeout = 0;
                    while(cppi->RxTeardownPending & CPPHY_RX_TEARDOWN) {
                        msleep_interruptible(2);
                        timeout++;
                        if(timeout > 100) {
                            if(cppi->cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].rx_B.Bits.enable == 0) {
                                /* There might be no teardown descriptor */
                                DEB_WARN("[%s] no rx teardown descriptor, but channel is disabled anyway.\n", __func__);
                                break;
                            }
                            DEB_WARN("[%s] break rx wait\n", __func__);
                            break;
                        }
                    }
                }
                cppi->RxTeardownPending = 0;
            }
        }
    }
    return CPMAC_ERR_NOERR;
}
#endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/

