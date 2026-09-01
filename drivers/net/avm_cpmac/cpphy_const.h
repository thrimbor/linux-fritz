/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,2007,2008,2009,2010 AVM GmbH <fritzbox_info@avm.de>
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

#ifndef _CPPHY_CONST_H_
#define _CPPHY_CONST_H_

#define CPPHY_MAX_CHAN 1
#define CPPHY_MAX_QUEUE 1
#define CPPHY_MAX_TX_BUFFERS 256
#define CPPHY_MAX_TX_PRIO_QUEUE_SIZE 32      /* Max size of biggest tx priority queue */
#define CPPHY_MAX_TX_DMA_PRIO_QUEUE_FACTOR 3 /* Factor, that the DMA queue is bigger than the corresponding priority queue */

/*--- #define CPPHY_MAX_RX_BUFFERS 256 ---*/ /* 256 needed for Multicast-Streams */
#define CPPHY_MAX_RX_BUFFERS    CONFIG_AVM_CPMAC_NUM_RX_BUFFERS
#define CPPHY_MAX_NEEDS CPPHY_MAX_RX_BUFFERS

/********************************************************************************\
 * ATTENTION!                                                                   *
 * For Puma this has to be defined in avalanche_cpgmac_f/cpgmac_f_NetLxCfg.h as *
 * CPMAC_DDA_DEFAULT_MAX_FRAME_SIZE as well!                                    *
\********************************************************************************/
#define CPPHY_MAX_RX_BUFFER_SIZE (2000 + (CPMAC_VLAN_TCI_LENGTH + CPMAC_VLAN_TPID_LENGTH))
#define CPPHY_MAX_RX_SERVICE (CPPHY_MAX_TX_BUFFERS / 3)
/* "2+" is needed to align the headers for 32 bit. performance++ */
/* 32 should be enough even for WLAN header */
/* IPSEC with PPPoE + VLAN needs: 20 (IPIP) + 16 (ESP front) + 6 (PPPoE) + 2 (PPP) + 4 (VLAN) = 48 */
#define CPPHY_SKB_HEADER_RESERVED 48
#define CPPHY_TOTAL_RX_RESERVED (2 + CPPHY_SKB_HEADER_RESERVED)
#define CPPHY_TOTAL_RX_BUFFER_SIZE (CPPHY_MAX_RX_BUFFER_SIZE + CPPHY_TOTAL_RX_RESERVED)
#define CPPHY_MAX_RX_FRAGMENTS 2
#define CPPHY_MAX_TX_SERVICE (CPPHY_MAX_TX_BUFFERS / 3)
/*--- #define CPPHY_MDIO_BUS_FREQ 10000000 ---*/
#define CPPHY_CLK_FREQ 2200000

#define CPPHY_TRAFFIC_THROTTLE_MIN 3000u
#define CPPHY_TRAFFIC_THROTTLE_MAX 5000u

#define CPPHY_MDIO_FLG_MDIX_ON      0x01

#define CPPHY_MAX_DIR 2
#define CPPHY_DIRECTION_TX 0
#define CPPHY_DIRECTION_RX 1

/* teardown modes */
#define CPPHY_TX_TEARDOWN 1
#define CPPHY_RX_TEARDOWN 2
#define CPPHY_FULL_TEARDOWN 4
#define CPPHY_BLOCKING_TEARDOWN 8
#define CPPHY_CALLBACK_TEARDOWN 0x10

/* mdio timeouts */
#define CPPHY_FIND_TO   2
#define CPPHY_RECK_TO 200
#define CPPHY_LINK_TO 500
#define CPPHY_NWST_TO 500
#define CPPHY_NWDN_TO 800
/* 2.74 Seconds <--Spec and empirical */
#define CPPHY_MDIX_TO 274

/* milli-seconds*/
#define CPPHY_AUTOMDIX_DELAY_MIN  80

/* additionally to cpmac NWAY defines */
#define NWAY_AUTOMDIX (1u << 15)

#define CPPHY_CPMAC_LOW_PORT_ID 0
#define CPPHY_CPMAC_HIGH_PORT_ID 1

/* address conversion handling */
#if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
#define CPPHY_VIRT_TO_PHYS(a) (((int)a)&~0xe0000000)
#define CPPHY_VIRT_TO_VIRT_NO_CACHE(a) ((void*)((CPPHY_VIRT_TO_PHYS(a))|0xa0000000))
#define CPPHY_VIRT_TO_VIRT_CACHE(a) ((void*)((CPPHY_VIRT_TO_PHYS(a))|0x80000000))
#define CPPHY_PHYS_TO_VIRT_NO_CACHE(a) ((void*)(((int)a)|0xa0000000))
#define CPPHY_PHYS_TO_VIRT_CACHE(a) ((void*)(((int)a)|0x80000000))
#elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
#define CPPHY_VIRT_TO_PHYS(addr) CPHYSADDR((addr))
#define CPPHY_PHYS_TO_VIRT_NO_CACHE(addr) KSEG1ADDR((addr))
#define CPPHY_PHYS_TO_VIRT_CACHE(addr) KSEG0ADDR((addr))
#define CPPHY_VIRT_TO_VIRT_NO_CACHE(addr) CPPHY_PHYS_TO_VIRT_NO_CACHE(CPPHY_VIRT_TO_PHYS((addr)))
#endif /*--- #elif defined(CONFIG_MIPS_UR8) ---*/

#endif

