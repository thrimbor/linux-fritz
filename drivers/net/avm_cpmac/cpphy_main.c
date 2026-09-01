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
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/reboot.h>
/*--- #include <linux/ioport.h> ---*/ /* FIXME Is this needed? */
#if !defined(CONFIG_NETCHIP_ADM69961)
#define CONFIG_NETCHIP_ADM69961
#endif
#include <linux/avm_event.h>
#if defined(CONFIG_AVM_LED)
#include <linux/avm_led.h>
#endif /*--- #if defined(CONFIG_AVM_LED) ---*/
#if defined(CONFIG_MIPS_OHIO)
#include <asm/mips-boards/ohio.h>
#endif /*--- #if defined(CONFIG_MIPS_OHIO) ---*/
#if defined(CONFIG_MIPS_UR8)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
#include <asm/mach-ur8/ur8.h>
#else /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
#include <asm/mips-boards/ur8.h>
#endif
#include <asm/mach-ur8/hw_nwss.h>
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/
#include <asm/mach_avm.h>

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
#if defined(CONFIG_MIPS_UR8)
#include "cpphy_if_g.h"
#include "cpgmac_f.h"
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/
#include "cpphy_adm6996.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
static struct notifier_block reboot_notifier_block = {NULL, NULL, 0};

#if defined(CONFIG_MIPS_UR8)
static struct resource cpmac_resource_nwss_tx_queue = {
    .name = "CPMAC",
    .start = UR8_TX_MAC0,
    .end   = UR8_TX_MAC1,
	.flags	= IORESOURCE_DMA
};
static struct resource cpmac_resource_nwss_tx_completion_queue = {
    .name = "CPMAC",
    .start = UR8_TX_COMPLETE,
    .end   = UR8_TX_COMPLETE,
	.flags	= IORESOURCE_DMA
};
static struct resource cpmac_resource_nwss_rx_queue = {
    .name = "CPMAC",
    .start = UR8_RX_QUEUE,
    .end   = UR8_RX_QUEUE,
	.flags	= IORESOURCE_DMA
};
static struct resource cpmac_resource_nwss_free_buffer_queue = {
    .name = "CPMAC",
    .start = UR8_RX_FREE_QUEUE,
    .end   = UR8_RX_FREE_QUEUE,
	.flags	= IORESOURCE_DMA
};
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_main_mac_address_set(cpphy_global_t *cpphy_global __attribute__ ((unused)),
                                       struct net_device *p_dev) {
#   if defined(CONFIG_MIPS_UR8)
    unsigned int i;
    struct cpgmac_f_regs *CPGMAC_F = ((cpmac_priv_t *) netdev_priv (p_dev))->CPGMAC_F;
#   endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/

#   if defined(CONFIG_MIPS_OHIO)
    /* high */
    *((volatile unsigned int *)(p_dev->base_addr + 0x508)) = 0;
    /* med */
    *((volatile unsigned int *)(p_dev->base_addr + 0x504)) =   (p_dev->dev_addr[0] <<  0)
                                                             | (p_dev->dev_addr[1] <<  8)
                                                             | (p_dev->dev_addr[2] << 16)
                                                             | (p_dev->dev_addr[3] << 24);
    /* low */
    *((volatile unsigned int *)(p_dev->base_addr + 0x500)) =   (1 << 20)  /*--- funktion unbekannt ---*/
                                                             | (1 << 19)  /*--- funktion unbekannt ---*/
                                                             | (0 << 16)  /*--- funktion unbekannt ---*/
                                                             | (p_dev->dev_addr[4] <<  0)
                                                             | (p_dev->dev_addr[5] <<  8);
#   elif defined(CONFIG_MIPS_AR7) /*--- #if defined(CONFIG_MIPS_OHIO) ---*/
    CPMAC_MACADDRLO_0(p_dev->base_addr) = p_dev->dev_addr[5];
    CPMAC_MACADDRMID(p_dev->base_addr)  = p_dev->dev_addr[4];
    CPMAC_MACADDRHI(p_dev->base_addr)   =    p_dev->dev_addr[0]
                                          | (p_dev->dev_addr[1] <<  8)
                                          | (p_dev->dev_addr[2] << 16)
                                          | (p_dev->dev_addr[3] << 24);
#   elif defined(CONFIG_MIPS_UR8) /*--- #elif defined(CONFIG_MIPS_AR7) ---*/
    /* Set the receive address */
    /*--- set receive-address ---*/

    CPGMAC_F->MAC_SRC_ADDR_LOW.Reg =   (p_dev->dev_addr[5] << 8)
                                     | (p_dev->dev_addr[4]);
    CPGMAC_F->MAC_SRC_ADDR_HIGH.Reg =   (p_dev->dev_addr[3] << 24)
                                      | (p_dev->dev_addr[2] << 16)
                                      | (p_dev->dev_addr[1] << 8)
                                      | (p_dev->dev_addr[0]);
    CPGMAC_F->MAC_HASH1 = 0;
    CPGMAC_F->MAC_HASH2 = 0;

    /*--- Set source MAC address - REQUIRED ---*/
    /* Ensure correct MAC by setting MAC for all channels */
    for(i = 0; i < 8; i++) {
        CPGMAC_F->MAC_INDEX = i;
        CPGMAC_F->MAC_ADDR_HIGH.Reg =   (p_dev->dev_addr[3] << 24)
                                      | (p_dev->dev_addr[2] << 16)
                                      | (p_dev->dev_addr[1] << 8)
                                      | (p_dev->dev_addr[0]);
        CPGMAC_F->MAC_ADDR_LOW.Reg =   (p_dev->dev_addr[5] << 8)
                                     | (p_dev->dev_addr[4]);
    }
#   else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#   warning "No code for setting the MAC address!";
#   endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
}


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
#if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
static void cpphy_main_config_apply(cpphy_global_t *cpphy_global, struct net_device *p_dev) {
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(p_dev);
    cpmac_capabilities_t *capabilities = &cpmac_priv->capabilities;
    unsigned int RxMbpEnable;

    CPMAC_RX_MAXLEN(p_dev->base_addr) = CPPHY_MAX_RX_BUFFER_SIZE;

    cpphy_main_mac_address_set(cpphy_global, p_dev);

    /* Init RX Multicast/Broadcast/Promiscous */
    RxMbpEnable = 0;
    if(capabilities->promiscous) {
        RxMbpEnable |= RX_CAF_EN;
    }
    if(capabilities->broadcast) {
        RxMbpEnable |= RX_BROAD_EN;
    }
    if(capabilities->multicast) {
        RxMbpEnable |= RX_MULT_EN;
    }
    if(capabilities->short_frames) {
        RxMbpEnable |= RX_CSF_EN;
    }
    if(!capabilities->long_frames) {
        RxMbpEnable |= RX_NO_CHAIN;
    }
    CPMAC_RX_MBP_ENABLE(p_dev->base_addr) = RxMbpEnable;
    DEB_TRC("[%s] MBP_ENABLE 0x%08X\n", __func__, CPMAC_RX_MBP_ENABLE(p_dev->base_addr));

    CPMAC_MACHASH1(p_dev->base_addr) = capabilities->multicast ? 0xffffffff : 0;
    CPMAC_MACHASH2(p_dev->base_addr) = capabilities->multicast ? 0xffffffff : 0;

    CPMAC_RX_UNICAST_CLEAR(p_dev->base_addr) = 0xff;
    /* channel 0 for unicast */
    CPMAC_RX_UNICAST_SET(p_dev->base_addr) = 1;

    /* Enable MII and flow control in both directions */
    CPMAC_MACCONTROL(p_dev->base_addr) |= MII_EN | TX_FLOW_EN | RX_FLOW_EN;
}
#endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
#if defined(CONFIG_MIPS_UR8)
void cpphy_main_config_g_apply(cpphy_global_t *cpphy_global, struct net_device *p_dev) {
    cpmac_priv_t *cpmac_priv = (cpmac_priv_t *) netdev_priv(p_dev);
    cpmac_capabilities_t *capabilities = &cpmac_priv->capabilities;
    struct __nwss_channel_cfg channel_cfg_tmp;

    union _cpgmacf_MAC_Ctrl MAC_control_init = {
        Bits: {
            fullduplex      : 1,
            loopback        : 0,
            mtest           : 0,
            rx_flow_en      : 1,
            tx_flow_en      : 1,
            gmii_en         : 1,
            tx_pace         : 0,
            gig             : 0,
            memtest         : 0,
            tx_short_gap_en : 0,
            cmd_idle        : 0,
            ifctl_a         : 0,
            ifctl_b         : 0,
            gig_force       : 0,
            ext_en          : 0,
            rx_vlan_en      : 0
        }
    };
    union _cpgmacf_Rx_MBP_Enable Rx_MBP_Enable_init = {
        Bits: {
            rx_mult_ch         : 0,
            rx_mult_en         : 1,
            rx_broad_ch        : 0,
            rx_broad_en        : 1,
            rx_prom_ch         : 0,
            rx_caf_en          : 1,
            rx_cef_en          : 0,
            rx_csf_en          : 0,
            rx_cmf_en          : 0,
            high_pri_thresh    : 0,
        }
    };
    union _cpmac_Rx_MaxLen cpmac_rx_MaxLen_init = {
        Bits: {
            RxMaxLen   : CPPHY_MAX_RX_BUFFER_SIZE
        }
    };
    union _cpmac_Rx_UNI cpmac_unicast_set_init = {
        Bits: {
            ch0  :1,
            ch1  :0,
            ch2  :0,
            ch3  :0,
            ch4  :0,
            ch5  :0,
            ch6  :0,
            ch7  :0
        }
    };
    union _cpmac_Rx_UNI cpmac_unicast_clear_init = {
        Bits: {
            ch0  :1,
            ch1  :1,
            ch2  :1,
            ch3  :1,
            ch4  :1,
            ch5  :1,
            ch6  :1,
            ch7  :1
        }
    };

    /* Empty tx and rx queues */
    /* Reset queues */
#if 0
    avm_put_device_into_reset(9);
    msleep(500);
    avm_take_device_out_of_reset(9);
    msleep(500);
#endif /*--- #if 0 ---*/
    cpmac_priv->UR8_QUEUE->soft_reset = 1;
    cpmac_priv->CPGMAC_F->SOFT_RESET = 1;
    msleep(500);

    /* Init RX Multicast/Broadcast/Promiscous */
    Rx_MBP_Enable_init.Bits.rx_mult_en  = (capabilities->multicast)    ? 1 : 0;
    Rx_MBP_Enable_init.Bits.rx_broad_en = (capabilities->broadcast)    ? 1 : 0;
    Rx_MBP_Enable_init.Bits.rx_caf_en   = (capabilities->promiscous)   ? 1 : 0;
    Rx_MBP_Enable_init.Bits.rx_csf_en   = (capabilities->short_frames) ? 1 : 0;

    cpmac_priv->CPGMAC_F->RX_MAX_LEN = cpmac_rx_MaxLen_init;
    cpmac_priv->CPGMAC_F->RX_UNICAST_CLEAR = cpmac_unicast_clear_init;
    cpmac_priv->CPGMAC_F->RX_UNICAST_SET   = cpmac_unicast_set_init;
    cpmac_priv->CPGMAC_F->RX_MBP_ENABLE = Rx_MBP_Enable_init;
    cpmac_priv->CPGMAC_F->MAC_CONTROL = MAC_control_init;
    cpphy_main_mac_address_set(cpphy_global, p_dev);

    if(request_resource(&nwss_tx_queue_resource, &cpmac_resource_nwss_tx_queue)) {
        DEB_INFO("could not get tx queue resource! conflicts will exist!\n");
    }
    if(request_resource(&nwss_tx_completion_queue_resource, &cpmac_resource_nwss_tx_completion_queue)) {
        DEB_INFO("Could not get tx complete queue resource! Conflicts will exist!\n");
    }
    if(request_resource(&nwss_rx_queue_resource, &cpmac_resource_nwss_rx_queue)) {
        DEB_INFO("Could not get rx queue resource! Conflicts will exist!\n");
    }
    if(request_resource(&nwss_free_buffer_queue_resource, &cpmac_resource_nwss_free_buffer_queue)) {
        DEB_INFO("Could not get rx free queue resource! Conflicts will exist!\n");
    }
    DEB_TRC("[%s] Disable tx and rx channels\n", __func__);

    cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].tx_B.Bits.enable = 0;
    cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC1].tx_B.Bits.enable = 0;
    cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].rx_B.Bits.enable = 0;
    cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC1].rx_B.Bits.enable = 0;
    DEB_TRC("[%s] Empty tx, rx and free buffer queues\n", __func__);
    cpmac_priv->UR8_QUEUE->tx_emac[0].prio[0] = 0;
    cpmac_priv->UR8_QUEUE->tx_emac[0].prio[1] = 0;
    cpmac_priv->UR8_QUEUE->tx_emac[1].prio[0] = 0;
    cpmac_priv->UR8_QUEUE->tx_emac[1].prio[1] = 0;
    cpmac_priv->UR8_QUEUE->tx_completion_queue[UR8_TX_COMPLETE] = 0;
    cpmac_priv->UR8_QUEUE->rx_queue[UR8_RX_QUEUE].prio[0] = 0;
    cpmac_priv->UR8_QUEUE->rx_queue[UR8_RX_QUEUE].prio[1] = 0;
    cpmac_priv->UR8_QUEUE->rx_queue[UR8_RX_QUEUE].prio[2] = 0;
    cpmac_priv->UR8_QUEUE->rx_queue[UR8_RX_QUEUE].prio[3] = 0;
    cpmac_priv->UR8_QUEUE->free_db_queue[UR8_RX_FREE_QUEUE].pointer = 0;

    /* Now setup the channels */
    DEB_TRC("[%s] Setup tx and rx channels\n", __func__);
    channel_cfg_tmp.tx_A.Register = 0;
    channel_cfg_tmp.tx_A.Bits.desc_copy_bytecnt = 0;
    cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].tx_A.Register = channel_cfg_tmp.tx_A.Register;
    channel_cfg_tmp.tx_B.Register = 0;
    channel_cfg_tmp.tx_B.Bits.channel_mode = 0;
    channel_cfg_tmp.tx_B.Bits.packet_type = PACKET_TYPE_ETHERNET;
    channel_cfg_tmp.tx_B.Bits.cq_index = UR8_TX_COMPLETE;
    channel_cfg_tmp.tx_B.Bits.enable = 1;
    cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].tx_B.Register = channel_cfg_tmp.tx_B.Register;
    cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].rx_A.Register = 0;
    channel_cfg_tmp.rx_B.Register = 0;
    channel_cfg_tmp.rx_B.Bits.channel_mode = 0;
    channel_cfg_tmp.rx_B.Bits.rq_index = 0xE00 | UR8_RX_QUEUE;
    channel_cfg_tmp.rx_B.Bits.enable = 1;
    cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].rx_B.Register = channel_cfg_tmp.rx_B.Register;

    cpphy_cppi_set_multi_promiscous(&cpphy_global->cppi,
                                    (capabilities->multicast) ? 1 : 0,
                                    (capabilities->promiscous) ? 1 : 0);
}
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
static void cpphy_main_close_cpmac (struct net_device *p_dev) {
    DEB_INFOTRC("[%s] %s\n", __func__, p_dev->name);
    /* perform normal close duties */
    CPMAC_MACCONTROL(p_dev->base_addr) &= ~MII_EN;
    CPMAC_TX_CONTROL(p_dev->base_addr) &= ~TX_EN;
    CPMAC_RX_CONTROL(p_dev->base_addr) &= ~RX_EN;

    /* disable interrupt masks */
    CPMAC_TX_INTMASK_CLEAR(p_dev->base_addr) = 0xFF;
    CPMAC_RX_INTMASK_CLEAR(p_dev->base_addr) = 0xFF;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_main_init_queues(cpphy_global_t *cpphy_global) {
    unsigned int i;

    cpphy_global->cppi.TxPrioQueues.NumberOfPrioQueues = CPPHY_PRIO_QUEUES;
    atomic_set(&cpphy_global->cppi.TxPrioQueues.SummedFree, 0);

    cpphy_global->cppi.NeededDMAtcbs = 0;
    for(i = 0; i < CPPHY_PRIO_QUEUES; i++) {
        cpphy_global->cppi.TxPrioQueues.q[i].MaxSize = CPPHY_MAX_TX_PRIO_QUEUE_SIZE / (i + 1);
        atomic_set(&cpphy_global->cppi.TxPrioQueues.q[i].Free, cpphy_global->cppi.TxPrioQueues.q[i].MaxSize);
        atomic_add(cpphy_global->cppi.TxPrioQueues.q[i].MaxSize, &cpphy_global->cppi.TxPrioQueues.SummedFree);
        cpphy_global->cppi.TxPrioQueues.q[i].MaxDMAFree = CPPHY_MAX_TX_DMA_PRIO_QUEUE_FACTOR * cpphy_global->cppi.TxPrioQueues.q[i].MaxSize;
        atomic_set(&cpphy_global->cppi.TxPrioQueues.q[i].DMAFree, cpphy_global->cppi.TxPrioQueues.q[i].MaxDMAFree);
        cpphy_global->cppi.TxPrioQueues.q[i].Pause = 0;
        cpphy_global->cppi.TxPrioQueues.q[i].PauseInit = i + 1;
        cpphy_global->cppi.TxPrioQueues.q[i].First = NULL;
        cpphy_global->cppi.TxPrioQueues.q[i].Last = NULL;
        spin_lock_init(&cpphy_global->cppi.TxPrioQueues.q[i].lock);
        DEB_INFO("Priority queue %u is size %u\n", i, cpphy_global->cppi.TxPrioQueues.q[i].MaxSize);
        cpphy_global->cppi.NeededDMAtcbs += atomic_read(&cpphy_global->cppi.TxPrioQueues.q[i].DMAFree);
    }
    cpphy_global->cppi.skb_transit_spinlock = SPIN_LOCK_UNLOCKED;
    skb_queue_head_init(&cpphy_global->cppi.skbs_in_transit);

    cpphy_global->cpmac_priv->TxPrioQueues = &cpphy_global->cppi.TxPrioQueues;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_main_open(cpphy_global_t *cpphy_global, struct net_device *p_dev __attribute__ ((unused))) {
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    unsigned int i;
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    cpmac_err_t ret = CPMAC_ERR_NOERR;

    DEB_TRC("[%s]\n", __func__);

    /* Decide, which TxChannel to use for transfer */
    cpphy_global->cppi.TxChannel = 0;

#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    /* After Reset clear the Transmit and Receive DMA Head Descriptor Pointers */
    for(i = 0; i < 8; i++) {
        CPMAC_TX_HDP(p_dev->base_addr, i) = 0;
    }
    for(i = 0; i < 8; i++) {
        CPMAC_RX_HDP(p_dev->base_addr, i) = 0;
    }

    CPMAC_RX_BUFFER_OFFSET(p_dev->base_addr) = 0;
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
#   else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#   warning "Cleanup of queues needed for unknown architecture?"
#   endif /*--- #else ---*/

    cpphy_main_init_queues(cpphy_global);

    cpphy_global->cppi.cpmac_priv = cpphy_global->cpmac_priv;
    cpphy_global->cpmac_priv->cppi = &cpphy_global->cppi;
    cpphy_global->cppi.mdio = &cpphy_global->mdio;
    cpphy_global->cppi.hw_state = CPPHY_HW_ST_OPENED;

    /* Init Tx and Rx DMA */

#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    CPMAC_TX_CONTROL(p_dev->base_addr) |= TX_EN;
    CPMAC_RX_CONTROL(p_dev->base_addr) |= RX_EN;

    /* Enable HostErr Ints (only for debugging) */
    CPMAC_MAC_INTMASK_SET(p_dev->base_addr) |= 2;
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
#   else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#   endif /*--- #else ---*/

    cpphy_mdio_init(cpphy_global);

    if(reboot_notifier_block.notifier_call == NULL) {
        reboot_notifier_block.notifier_call = cpphy_entry_prepare_reboot;
        reboot_notifier_block.next = NULL;
        reboot_notifier_block.priority = 0;
        register_reboot_notifier(&reboot_notifier_block);
    }

    return ret;
}


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
cpmac_err_t cpphy_main_close(cpphy_cppi_t *cppi) {
    cpmac_err_t ret = CPMAC_ERR_NOERR;
    struct net_device *p_dev = (struct net_device *)cppi->cpmac_priv->owner;

    DEB_INFO("[%s] %s\n", __func__, p_dev->name);

    /* Verify proper device state */
    if(cppi->hw_state == CPPHY_HW_ST_OPENED) {
        if(reboot_notifier_block.notifier_call) {
            unregister_reboot_notifier(&reboot_notifier_block);
            reboot_notifier_block.notifier_call = NULL;
        }
        cpphy_main_close_cpmac(p_dev);

        if(cppi->mdio->switch_config.is_switch) {
            /* FIXME */
#       if defined(CONFIG_AVM_LED)
        } else {
            if(cppi->cpmac_priv->led[cppi->cpmac_priv->inst].handle != 0) {
                avm_led_free_handle(cppi->cpmac_priv->led[cppi->cpmac_priv->inst].handle);
            }
#       endif /*--- #if defined(CONFIG_AVM_LED) ---*/
        }
#       if defined(CONFIG_MIPS_UR8)
        release_resource(&cpmac_resource_nwss_tx_queue);
        release_resource(&cpmac_resource_nwss_tx_completion_queue);
        release_resource(&cpmac_resource_nwss_rx_queue);
        release_resource(&cpmac_resource_nwss_free_buffer_queue);
#       endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/

        /* put device back into reset */
        avm_put_device_into_reset(cppi->cpmac_priv->mac_reset_bit); /* FIXME Is this needed for UR8? */
        msleep(2);

        cppi->hw_state = CPPHY_HW_ST_INITIALIZED;
    }
    return ret;
}

/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
cpmac_err_t cpphy_main_register(cpphy_global_t *cpphy_global) {
    cpmac_service_funcs_t service_funcs;

    DEB_TRC("[%s]\n", __func__);

    service_funcs.init = &cpphy_if_init;
    service_funcs.deinit = &cpphy_if_deinit;
    service_funcs.control_req = &cpphy_if_control_req;
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    service_funcs.data_to_phy = &cpphy_if_data_to_phy;
    service_funcs.isr_tasklet = &cpphy_if_isr_tasklet;
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    service_funcs.data_to_phy = &cpphy_if_g_data_to_phy;
    service_funcs.isr_tasklet = &cpphy_if_g_isr_tasklet;
#   else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#   warning "No data_to_phy function for this architecture!";
#   endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
    service_funcs.isr_end = &cpphy_if_isr_end;

    /* register at mac */
    return cpmac_if_register(cpphy_global, &service_funcs);
}
EXPORT_SYMBOL(cpphy_main_register);

#endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/

