/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2008,2009,2010 AVM GmbH <fritzbox_info@avm.de>
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

/*---------------------------------------------------------------------------*\
 * Address Resolution
\*---------------------------------------------------------------------------*/

#include <linux/skbuff.h>
#if defined(CONFIG_AVM_POWER)
#include <linux/avm_power.h>
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/

#if !defined(CONFIG_NETCHIP_AR8216)
#define CONFIG_NETCHIP_AR8216
#endif
#include <linux/avm_event.h>
#include <linux/ar_reg.h>
#include <linux/etherdevice.h>
#if defined(CONFIG_IP_MULTICAST_FASTFORWARD)
#   include <linux/mcfastforward.h>
#endif /*--- #if defined(CONFIG_IP_MULTICAST_FASTFORWARD) ---*/

#include "cpmac_if.h"
#include "cpmac_const.h"
#include "cpmac_debug.h"
#include "cpmac_main.h"
#include "cpphy_types.h"
#include "cpphy_ar8216.h"
#include "cpphy_mdio.h"
#include "cpphy_mgmt.h"
#include "cpphy_cppi.h"
#include "cpmac_fusiv_if.h"
#include "cpmac_puma_if.h"


/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void macport_del(cpmac_macportmap_t *p);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static inline unsigned long ar8216_lock(void) {
    unsigned long flags;
    spin_lock_irqsave(&cpmac_global.ar8216_spinlock, flags);
    return flags;
}

static inline void ar8216_unlock(unsigned long flags) {
    spin_unlock_irqrestore(&cpmac_global.ar8216_spinlock, flags);
}


/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
static kmem_cache_t       *kmem_cache_macportmap = 0;
#else
static struct kmem_cache  *kmem_cache_macportmap = 0;
#endif
static atomic_t init_count;

void ar8216_ar_init(void) {
    atomic_inc(&init_count);
    if(kmem_cache_macportmap) 
        return;
#   if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
    kmem_cache_macportmap = kmem_cache_create("ar8216macport",
            sizeof(cpmac_macportmap_t),
            0, 0, 0, 0);
#   else /*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28) ---*/
    kmem_cache_macportmap = kmem_cache_create("ar8216macport",
            sizeof(cpmac_macportmap_t),
            0, 0, 0);
#   endif /*--- #else ---*/ /*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28) ---*/
    if(kmem_cache_macportmap == 0)
        panic("ar8216_ar_init: kmem_cache_create failed\n");
    cpmac_global.ar8216_spinlock = SPIN_LOCK_UNLOCKED;
    DEB_INFO("[%s] Initialisation done.\n", __FUNCTION__);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar8216_ar_deinit(void) {
    if(atomic_dec_and_test(&init_count) && kmem_cache_macportmap) {
        unsigned long flags = ar8216_lock();
        cpmac_macportmap_t *p;
        while((p = cpmac_global.macportmap.lru_head) != 0) {
            macport_del(p);
            if(p == cpmac_global.macportmap.lru_head) {
                DEB_ERR("[%s] macport_del didn't delete\n", __FUNCTION__);
                break;
            }
        }
        kmem_cache_destroy(kmem_cache_macportmap);
        kmem_cache_macportmap = 0;
        ar8216_unlock(flags);
    }
}


/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static unsigned machash(unsigned char mac[ETH_ALEN]) {
    return mac[0] ^ mac[1] ^ mac[2] ^ mac[3] ^ mac[4] ^ mac[5];
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline cpmac_macportmap_t *macport_find(unsigned char mac[ETH_ALEN], unsigned *hashp) {
    cpmac_macportmap_t *p;

    *hashp = machash(mac);

    for(p = cpmac_global.macportmap.hashtab[*hashp]; p; p = p->link) {
        if(memcmp(p->mac, mac, ETH_ALEN) == 0)
            break;
    }
    return p;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline cpmac_macportmap_t **macport_find_pp(unsigned char mac[ETH_ALEN]) {
    cpmac_macportmap_t **pp = &cpmac_global.macportmap.hashtab[machash(mac)];

    while(*pp) { 
        if(memcmp((*pp)->mac, mac, ETH_ALEN) == 0)
            return pp;
        pp = &(*pp)->link;
    }
    return pp;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void macport_hash_del(cpmac_macportmap_t *p) {
    cpmac_macportmap_t **pp;
    if(p->isinhash) {
        pp = macport_find_pp(p->mac);
        if(p == *pp) {
            *pp = p->link;
            p->link = 0;
            p->isinhash = 0;
        } else {
            DEB_ERR("[%s] %p not in hash\n", __FUNCTION__, p);
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void macport_hash_add_head(cpmac_macportmap_t *p, unsigned hash) {
    if(p->isinhash)
        macport_hash_del(p);
    p->link = cpmac_global.macportmap.hashtab[hash];
    cpmac_global.macportmap.hashtab[hash] = p;
    p->isinhash = 1;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void macport_hash_add_tail(cpmac_macportmap_t *p) {
    cpmac_macportmap_t **pp;

    if(p->isinhash)
        macport_hash_del(p);

    pp = macport_find_pp(p->mac);
    p->link = *pp;
    *pp = p;
    p->isinhash = 1;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void macport_lru_del(cpmac_macportmap_t *p) {
    if(p->isonlru) {
        if(p->lru_prev) 
            p->lru_prev->lru_next = p->lru_next;
        else 
            cpmac_global.macportmap.lru_head = p->lru_next;
        if(p->lru_next) 
            p->lru_next->lru_prev = p->lru_prev;
        else
            cpmac_global.macportmap.lru_tail = p->lru_prev;
        p->lru_next = p->lru_prev = 0;
        p->isonlru = 0;
    } else {
        DEB_ERR("[%s] %p not on lru\n", __FUNCTION__, p);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void macport_lru_add(cpmac_macportmap_t *p) {
    if(p->isonlru)
        macport_lru_del(p);
    if(cpmac_global.macportmap.lru_tail) {
        p->lru_next = 0;
        p->lru_prev = cpmac_global.macportmap.lru_tail;
        cpmac_global.macportmap.lru_tail->lru_next = p;
        cpmac_global.macportmap.lru_tail = p;
    } else {
        p->lru_next = 0;
        p->lru_prev = 0;
        cpmac_global.macportmap.lru_head = cpmac_global.macportmap.lru_tail = p;
    }
    p->isonlru = 1;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void macport_free(cpmac_macportmap_t *p) {
    atomic_dec(&cpmac_global.macportmap.mapentries);
    kmem_cache_free(kmem_cache_macportmap, p);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void macport_del(cpmac_macportmap_t *p) {
    struct sk_buff *skb;

    if(p->isinhash)
        macport_hash_del(p);
    macport_lru_del(p);
    DEB_TRC("[%s] remove %02x:%02x:%02x:%02x:%02x:%02x port %u (%d skbs)\n",
            __FUNCTION__, 
            p->mac[0], p->mac[1], p->mac[2],
            p->mac[3], p->mac[4], p->mac[5], 
            p->port, skb_queue_len(&p->list));
    while((skb = skb_dequeue(&p->list)) != NULL) {
        cpmac_global.macportmap.skb_dropped++;
        atomic_dec(&cpmac_global.macportmap.nskbs);
        dev_kfree_skb_any(skb);
    }
    macport_free(p);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static cpmac_macportmap_t *macport_alloc(unsigned char mac[ETH_ALEN]) {
    cpmac_macportmap_t *p = 0;
    if(unlikely(kmem_cache_macportmap == 0)) {
        if(net_ratelimit())
            DEB_WARN("[%s] got packet before initialisation\n", __FUNCTION__);
        return 0;
    }
    if(unlikely(atomic_read(&cpmac_global.macportmap.mapentries) >= CPMAC_ARL_MAX_AR_ENTRIES))
        return 0;
    p = (cpmac_macportmap_t *)kmem_cache_alloc(kmem_cache_macportmap, GFP_KERNEL);
    if(likely(p)) {
        memset(p, 0, sizeof(cpmac_macportmap_t));
        skb_queue_head_init(&p->list);
        memcpy(p->mac, mac, ETH_ALEN);
        atomic_inc(&cpmac_global.macportmap.mapentries);
    }
    return p;
}


/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static inline unsigned char *get_srcmac(struct sk_buff *skb) {
#  if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE)
   if(skb_mac_header_was_set(skb))
      return skb_mac_header(skb) + ETH_ALEN;
#  else /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
   if(skb->mac.raw)
       return skb->mac.raw + ETH_ALEN;
#  endif /*--- #else ---*/ /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
   return skb->data + ETH_ALEN;
}


/*---------------------------------------------------------------------------*\
 * Check the port associated with the MAC address of the skb.                *
 * Return the port number on success.                                        *
 * Return 100 on failure and enqueue the packet for later delivery,          *
 * when the MAC address is known.                                            *
\*---------------------------------------------------------------------------*/
unsigned char ar8216_get_phy_port(cpphy_mdio_t *mdio, struct sk_buff *skb) {
    cpmac_macportmap_t *p;
    unsigned char *mac;
    unsigned long flags;
    unsigned hash;

    mac = get_srcmac(skb);
    flags = ar8216_lock();
    if(likely((p = macport_find(mac, &hash)) != 0)) {
        p->last_access = jiffies;
        macport_lru_add(p);
        if(likely(p->is_resolved)) {
            unsigned char port = p->port;
            ar8216_unlock(flags);
            skb->uniq_id &= ~(0xFF << 24);
            skb->uniq_id |= (port) << 24;
            return port;
        }
        if(atomic_read(&cpmac_global.macportmap.nskbs) < CPMAC_ARL_MAX_QUEUE_LENGTH) {
            skb_queue_tail(&p->list, skb);
            atomic_inc(&cpmac_global.macportmap.nskbs);
            ar8216_unlock(flags);
        } else {
            ar8216_unlock(flags);
            cpmac_global.macportmap.skb_dropped++;
            dev_kfree_skb_any(skb);
        }
        return 100; /* not resolved */
    }
    if(   atomic_read(&cpmac_global.macportmap.nskbs) >= CPMAC_ARL_MAX_QUEUE_LENGTH
       || (p = macport_alloc(mac)) == 0) {
       ar8216_unlock(flags);
       cpmac_global.macportmap.skb_dropped++;
       dev_kfree_skb_any(skb);
       return 100; /* not resolved */
    }
    /*
     * Because we assume it will be used soon again,
     * we add it in front of hash table link list.
     */
    macport_hash_add_head(p, hash);
    p->last_access = jiffies;
    macport_lru_add(p);
    skb_queue_tail(&p->list, skb);
    atomic_inc(&cpmac_global.macportmap.nskbs);
    ar8216_unlock(flags);
    DEB_DEBUG("[%s] need port for %02x:%02x:%02x:%02x:%02x:%02x\n",
              __FUNCTION__,
              p->mac[0], p->mac[1], p->mac[2],
              p->mac[3], p->mac[4], p->mac[5]);
    cpphy_mgmt_work_schedule(mdio, CPMAC_WORK_UPDATE_MAC_TABLE, 0);
    return 100; /* not resolved */
}

#if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)
#define macport_rx(mdio, skb) cpmac_fusiv_if_rx2(skb, NULL)
#define MDIO_UNUSED    __attribute__ ((unused))
#elif defined(CONFIG_ARCH_PUMA5)
#define macport_rx(mdio, skb) cpmac_puma_if_rx(skb)
#define MDIO_UNUSED    __attribute__ ((unused))
#else
#define macport_rx(mdio, skb) cpmac_if_data_from_phy(mdio->cpmac_priv, skb, skb->len)
#define MDIO_UNUSED
#endif

static int macport_set_port(cpphy_mdio_t *mdio MDIO_UNUSED,
                            unsigned char mac[ETH_ALEN], unsigned char port) {
#undef MDIO_UNUSED
    cpmac_macportmap_t *p;
    unsigned long flags;
    unsigned hash;
    int count = 0;

    flags = ar8216_lock();
    if((p = macport_find(mac, &hash)) != 0) {
        struct sk_buff *skb;
        if(skb_queue_len(&p->list))
            p->last_access = jiffies;
        macport_lru_add(p);
#       if CPMAC_USE_DEBUG_LEVEL_TRACE
        if(p->is_resolved && (p->port != port)) {
            DEB_TRC("[%s] updating %02x:%02x:%02x:%02x:%02x:%02x port %u->%u (%d skbs)\n",
                     __FUNCTION__,
                     p->mac[0], p->mac[1], p->mac[2],
                     p->mac[3], p->mac[4], p->mac[5], 
                     p->port, port, skb_queue_len(&p->list));
        }
#       endif /*--- #if CPMAC_USE_DEBUG_LEVEL_TRACE ---*/
        p->port = port;
        p->isinmactable = 1;
        if(p->is_resolved) {
            ar8216_unlock(flags);
            return 0;
        }
        p->is_resolved = 1;
        ar8216_unlock(flags);
        DEB_TRC("[%s] new %02x:%02x:%02x:%02x:%02x:%02x port %u (%d skbs)\n",
                 __FUNCTION__,
                 p->mac[0], p->mac[1], p->mac[2],
                 p->mac[3], p->mac[4], p->mac[5], 
                 p->port, skb_queue_len(&p->list));
        while((skb = skb_dequeue(&p->list)) != 0) {
            atomic_dec(&cpmac_global.macportmap.nskbs);
            skb->uniq_id &= ~(0xFF << 24);
            skb->uniq_id |= (port) << 24;
            macport_rx(mdio, skb);
            count++;
        }
        return count;
    }
    if((p = macport_alloc(mac)) == 0) {
        ar8216_unlock(flags);
        return 0;
    }
    p->is_resolved = 1;
    p->port = port;
    /*
     * mac address coming from switch address table,
     * we assume it is only talking to some attached to switch
     * not to us, so we add it at the tail of hash table link list.
     */
    macport_hash_add_tail(p);
    p->last_access = jiffies;
    macport_lru_add(p);
    ar8216_unlock(flags);
    DEB_TRC("[%s] got %02x:%02x:%02x:%02x:%02x:%02x port %u\n", 
             __FUNCTION__,
             p->mac[0], p->mac[1], p->mac[2],
             p->mac[3], p->mac[4], p->mac[5], 
             p->port);
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ar8216_print_table(cpphy_mdio_t *mdio __attribute__ ((unused))) {
#   if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_SUPPORT
    cpmac_macportmap_t *p;
    DEB_SUPPORT("MAC table: %d entries, %d skbs (jiffies now: %lu)\n",
                 atomic_read(&cpmac_global.macportmap.mapentries),
                 atomic_read(&cpmac_global.macportmap.nskbs),
                 jiffies);
    for(p = cpmac_global.macportmap.lru_tail; p; p = p->lru_prev) {
        DEB_SUPPORT("    %02x:%02x:%02x:%02x:%02x:%02x port %u (%d skbs) last %lu\n",
                    p->mac[0], p->mac[1], p->mac[2],
                    p->mac[3], p->mac[4], p->mac[5], 
                    p->port, skb_queue_len(&p->list), p->last_access);
    }
#   endif /*--- #if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_SUPPORT ---*/
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar8316_print_table(cpphy_mdio_t *mdio) {
   ar8216_print_table(mdio);
}


/*------------------------------------------------------------------------------------------*\
 * Remove MAC entries for the given port from switch MAC table and our MAC list
\*------------------------------------------------------------------------------------------*/
void ar8216_ar_del_port(cpphy_mdio_t *mdio, unsigned char port) {
    cpmac_macportmap_t *p, *next;
    unsigned long flags;

    if(cpmac_global.cpphy[mdio->phy_num].config->type == CPMAC_PHY_TYPE_AR8327) {
        ar8216_mdio_write32(mdio, 
                            AR8327_ATU_FUNC,
                            0
                            | AR8327_ATU_FUNC_BUSY 
                            | AR8327_ATU_FUNC_FUNC_VALUE(AR8327_ATU_FUNC_FUNC_VALUE_FLUSH_PORT)
                            | AR8327_ATU_FUNC_AT_PORT_NUM_VALUE(port)
                           );
    } else {
        ar8216_mdio_write32(mdio, 
                            AR8216_GLOBAL_AR_1,
                            0
                            | AR_1_AT_BUSY 
                            | AR_1_AT_FUNC_VALUE(AR_1_AT_FUNC_VALUE_FLUSH_PORT)
                            | AR_1_AT_PORT_NUM_VALUE(port)
                           );
    }

    flags = ar8216_lock();
    for(p = cpmac_global.macportmap.lru_head; p; p = next) {
        next = p->lru_next;
        if(p->port == port) {
            DEB_TRC("[%s] remove %02x:%02x:%02x:%02x:%02x:%02x port %u\n", 
                     __FUNCTION__,
                     p->mac[0], p->mac[1], p->mac[2],
                     p->mac[3], p->mac[4], p->mac[5], 
                     p->port);
            macport_del(p);
        }
    }
    ar8216_unlock(flags);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ar8216_read_mac_table(cpphy_mdio_t *mdio) {
    unsigned char mac[ETH_ALEN];
    unsigned int AR1 = 0, AR2, AR3;
    unsigned char port;
    int n, maxskbs = 0;

    for(;;) {
        do {
            AR1 = ar8216_mdio_read32(mdio, AR8216_GLOBAL_AR_1) & AR_1_AT_BUSY;
        } while(AR1);

        ar8216_mdio_write16(mdio, AR8216_GLOBAL_AR_1, 0
            | AR_1_AT_BUSY 
            | AR_1_AT_FUNC_VALUE(AR_1_AT_FUNC_VALUE_NEXT)
            | AR_1_AT_PORT_NUM_VALUE(0)
            );

        AR1 = ar8216_mdio_read32(mdio, AR8216_GLOBAL_AR_1);
        AR2 = ar8216_mdio_read32(mdio, AR8216_GLOBAL_AR_2);
        AR3 = ar8216_mdio_read32(mdio, AR8216_GLOBAL_AR_3);
        
        /* No more entries? */
        if((AR2 == 0) && (((AR1 >> AR_1_AT_ADDR_BYTE5_SHIFT) & 0xffff) == 0) && (AR3 == 0)) {
            break;
        }

        mac[0] = (unsigned char) ((AR2 & AR_2_AT_ADDR_BYTE0_MASK) >> AR_2_AT_ADDR_BYTE0_SHIFT);
        mac[1] = (unsigned char) ((AR2 & AR_2_AT_ADDR_BYTE1_MASK) >> AR_2_AT_ADDR_BYTE1_SHIFT);
        mac[2] = (unsigned char) ((AR2 & AR_2_AT_ADDR_BYTE2_MASK) >> AR_2_AT_ADDR_BYTE2_SHIFT);
        mac[3] = (unsigned char) ((AR2 & AR_2_AT_ADDR_BYTE3_MASK) >> AR_2_AT_ADDR_BYTE3_SHIFT);
        mac[4] = (unsigned char) ((AR1 & AR_1_AT_ADDR_BYTE4_MASK) >> AR_1_AT_ADDR_BYTE4_SHIFT);
        mac[5] = (unsigned char) ((AR1 & AR_1_AT_ADDR_BYTE5_MASK) >> AR_1_AT_ADDR_BYTE5_SHIFT);
        switch((AR3 & AR_3_DES_PORT_MASK) >> AR_3_DES_PORT_SHIFT) {
            case 0x01: port = 0; break;
            case 0x02: port = 1; break;
            case 0x04: port = 2; break;
            case 0x08: port = 3; break;
            case 0x10: port = 4; break;
            case 0x20: port = 5; break;
            default:
                DEB_ERR("[%s] Error! Unknown port %#x, ignore entry\n",
                        __FUNCTION__,
                        (AR3 & AR_3_DES_PORT_MASK) >> AR_3_DES_PORT_SHIFT);
                continue;
        }
        n = macport_set_port(mdio, mac, port);
        if(n > maxskbs) 
            maxskbs = n;
    }
    return maxskbs;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ar8327_read_mac_table(cpphy_mdio_t *mdio) {
    unsigned char mac[ETH_ALEN];
    unsigned int data0, data1, data2;
    unsigned char port;
    int n, maxskbs = 0;

    ar8216_mdio_write32(mdio, AR8327_ATU_DATA0, 0);
    ar8216_mdio_write32(mdio, AR8327_ATU_DATA1, 0);
    ar8216_mdio_write32(mdio, AR8327_ATU_DATA2, 0);
    for(;;) {
        do {
            data0 = ar8216_mdio_read32(mdio, AR8327_ATU_FUNC) & AR8327_ATU_FUNC_BUSY;
        } while(data0);

        ar8216_mdio_write32(mdio, 
                            AR8327_ATU_FUNC,
                            0
                            | AR8327_ATU_FUNC_BUSY 
                            | AR8327_ATU_FUNC_FUNC_VALUE(AR8327_ATU_FUNC_FUNC_VALUE_NEXT)
                           );

        data0 = ar8216_mdio_read32(mdio, AR8327_ATU_DATA0);
        data1 = ar8216_mdio_read32(mdio, AR8327_ATU_DATA1);
        data2 = ar8216_mdio_read32(mdio, AR8327_ATU_DATA2);
        
        /* No more entries? */
        if(   (data0 == 0) 
           && ((data1 & AR8327_ATU_DATA1_ATU_MAC_ADDR1_MASK) == 0)
           && ((data2 & (AR8327_ATU_DATA2_ATU_VID_MASK | AR8327_ATU_DATA2_ATU_STATUS_MASK)) == 0)) {
            break;
        }

        mac[5] = (data0 >>  0) & 0xff;
        mac[4] = (data0 >>  8) & 0xff;
        mac[3] = (data0 >> 16) & 0xff;
        mac[2] = (data0 >> 24) & 0xff;
        mac[1] = ((data1 & AR8327_ATU_DATA1_ATU_MAC_ADDR1_MASK) >> 0) & 0xff;
        mac[0] = ((data1 & AR8327_ATU_DATA1_ATU_MAC_ADDR1_MASK) >> 8) & 0xff;
        switch((data1 & AR8327_ATU_DATA1_ATU_DES_PORT_MASK) >> AR8327_ATU_DATA1_ATU_DES_PORT_SHIFT) {
            case 0x01: port = 0; break;
            case 0x02: port = 1; break;
            case 0x04: port = 2; break;
            case 0x08: port = 3; break;
            case 0x10: port = 4; break;
            case 0x20: port = 5; break;
            default:
                DEB_ERR("[%s] Error! Unknown port %#x, ignore entry\n",
                        __FUNCTION__,
                        ((data1 & AR8327_ATU_DATA1_ATU_DES_PORT_MASK) >> AR8327_ATU_DATA1_ATU_DES_PORT_SHIFT));
                continue;
        }
        n = macport_set_port(mdio, mac, port);
        if(n > maxskbs) 
            maxskbs = n;
    }
    return maxskbs;
}


/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
unsigned long ar8216_ar_work_item(cpphy_mdio_t *mdio) {
    unsigned long flags;
    int maxskbs __attribute__ ((unused));
#   if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_DEBUG
    unsigned long old_skb_dropped;
#   endif /*--- #if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_DEBUG ---*/

    if(cpmac_global.global_power != CPPHY_POWER_GLOBAL_NONE) {
        DEB_INFOTRC("[%s] Waiting for global power settings to settle.\n", __FUNCTION__);
        return CPMAC_ARL_UPDATE_INTERVAL;
    }

    /*--- DEB_DEBUG("[%s] %d map entries and %d skbs (at start)\n", ---*/
              /*--- __FUNCTION__, ---*/
              /*--- atomic_read(&cpmac_global.macportmap.mapentries), ---*/
              /*--- atomic_read(&cpmac_global.macportmap.nskbs)); ---*/

    if(cpmac_global.cpphy[mdio->phy_num].config->type == CPMAC_PHY_TYPE_AR8327) {
        maxskbs = ar8327_read_mac_table(mdio);
    } else {
        maxskbs = ar8216_read_mac_table(mdio);
    }

    /*
     * remove old entries
     */
#   if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_DEBUG
    old_skb_dropped = cpmac_global.macportmap.skb_dropped;
#   endif /*--- #if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_DEBUG ---*/
    flags = ar8216_lock();
    {
        cpmac_macportmap_t *p, *next;
        for(p = cpmac_global.macportmap.lru_head; p; p = next) {
            if(time_after(p->last_access + CPMAC_ARL_REMOVE_TIMER, jiffies)) {
                break;
            }
            next = p->lru_next;
            if(p->isinmactable) {
                p->isinmactable = 0;
            } else {
                macport_del(p);
            }
        }
    }
    ar8216_unlock(flags);

    DEB_DEBUG("[%s] %d map entries and %d skbs %lu dropped, max queue len %d (at end)\n",
              __FUNCTION__,
              atomic_read(&cpmac_global.macportmap.mapentries),
              atomic_read(&cpmac_global.macportmap.nskbs),
              cpmac_global.macportmap.skb_dropped - old_skb_dropped,
              maxskbs);
    return CPMAC_ARL_UPDATE_INTERVAL;
}

