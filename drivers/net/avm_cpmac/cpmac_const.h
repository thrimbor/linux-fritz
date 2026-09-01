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

#ifndef _CPMAC_CONST_H_
#define _CPMAC_CONST_H_

#include <linux/version.h>

#define CPMAC_MAX_LED_HANDLES 4u

#define CPMAC_TICK_LENGTH       (4 * HZ) // (10 * HZ) TKL: Windows needs to see IPv6 router advertisements within 8s from link-up

#define CPMAC_TX_TIMEOUT        (10 * HZ)

#define CPMAC_MAX_WORK_ITEMS    4

#define CPMAC_LINK_OFF          0
#define CPMAC_LINK_ON           1
#define CPMAC_SPEED_100         2
#define CPMAC_SPEED_10          3
#define CPMAC_FULL_DPLX         4
#define CPMAC_HALF_DPLX         5
#define CPMAC_TX_ACTIVITY       6
#define CPMAC_RX_ACTIVITY       7

#define CPMAC_CFG_LONG_FRAMES                0
#define CPMAC_CFG_SHORT_FRAMES               1
#define CPMAC_CFG_PROMISCOUS                 1
#define CPMAC_CFG_BROADCAST                  1
#define CPMAC_CFG_MULTICAST                  1
#define CPMAC_CFG_ALL_MULTI                  (1* (CPMAC_CFG_MULTICAST))
#define CPMAC_CFG_AUTO_NEGOTIATION           1
/*--- #define CFG_START_LINK_SPEED          (_CPMDIO_10 | _CPMDIO_100 | _CPMDIO_HD | _CPMDIO_FD) ---*/ /* auto nego */
#define CPMAC_CFG_LOOP_BACK                  1
#define CPMAC_CFG_TX_FLOW_CNTL               0
#define CPMAC_CFG_RX_FLOW_CNTL               0
#define CPMAC_CFG_TX_PACING                  0
#define CPMAC_CFG_RX_PASS_CRC                0
#define CPMAC_CFG_QOS_802_1Q                 0
#define CPMAC_CFG_TX_NUM_CHAN                1

#define CPMAC_VLAN_TPID                       0x8100
#define CPMAC_VLAN_TPID_START_OFFSET          12
#define CPMAC_VLAN_TCI_START_OFFSET           14
#define CPMAC_VLAN_TCI_LENGTH                 2
#define CPMAC_VLAN_TPID_LENGTH                2
#define CPMAC_VLAN_TPID_END_OFFSET            (CPMAC_VLAN_TPID_START_OFFSET + CPMAC_VLAN_TPID_LENGTH)
#define CPMAC_VLAN_TCI_END_OFFSET             (CPMAC_VLAN_TCI_START_OFFSET  + CPMAC_VLAN_TCI_LENGTH)
#define CPMAC_VLAN_IS_802_1Q_FRAME(hdr_byte_ptr)       (ntohs(*(unsigned short*)((hdr_byte_ptr) + CPMAC_VLAN_TPID_START_OFFSET)) == CPMAC_VLAN_TPID)
#define CPMAC_VLAN_IS_0_LEN_FRAME(hdr_byte_ptr)        (   (ntohs(*(unsigned short*)((hdr_byte_ptr) + CPMAC_VLAN_TPID_START_OFFSET)) == 0) \
                                                        || (   CPMAC_VLAN_IS_802_1Q_FRAME(hdr_byte_ptr) \
                                                            && (ntohs(*(unsigned short*)((hdr_byte_ptr) + CPMAC_VLAN_TPID_START_OFFSET + 4)) == 0)))
#define CPMAC_VLAN_IS_VALID_VLAN_ID(hdr_byte_ptr)      (ntohs(*(unsigned short*)((hdr_byte_ptr) + CPMAC_VLAN_TCI_START_OFFSET)) & 0xfff)
#define CPMAC_VLAN_GET_CHANNEL(hdr_byte_ptr)           (ntohs(*(unsigned short*)((hdr_byte_ptr) + CPMAC_VLAN_TCI_START_OFFSET)))

#define CPMAC_VLAN_GET_VLAN_ID(hdr_byte_ptr)         (ntohs(*(unsigned short*)((hdr_byte_ptr) + CPMAC_VLAN_TCI_START_OFFSET)) & 0xfff)
#define CPMAC_VLAN_GET_VLAN_PRIO(hdr_byte_ptr)       (ntohs(*(unsigned short*)((hdr_byte_ptr) + CPMAC_VLAN_TCI_START_OFFSET)) & 0xf000)
#define CPMAC_VLAN_PUT_VLAN_ID(hdr_byte_ptr, vid)    *(unsigned short*)((hdr_byte_ptr) + CPMAC_VLAN_TCI_START_OFFSET) = htons(CPMAC_VLAN_GET_VLAN_PRIO(hdr_byte_ptr) | ((vid) & 0xfff))
#define CPMAC_VLAN_PUT_VLAN_PRIO(hdr_byte_ptr, prio) *(unsigned short*)((hdr_byte_ptr) + CPMAC_VLAN_TCI_START_OFFSET) = htons(CPMAC_VLAN_GET_VLAN_ID(hdr_byte_ptr) | ((prio) << 12))

#define CPMAC_ADM_PHY_PDN_TO       ((165 * HZ) / 10)
#define CPMAC_ADM_PHY_PUP_TO       (( 40 * HZ) / 10)
#define RODERICK
#define CPMAC_ADM_PHY_PUP_IDLE_TIME   ((10 * HZ) / 10)
#define CPMAC_ADM_PHY_PUP_TEST_100_TO ((45 * HZ) / 10)
#define CPMAC_ADM_PHY_PUP_TEST_10_TO  (( 1 * HZ) / 10)
#define CPMAC_ADM_PHY_PUP_LINKLOSS_TO ((40 * HZ) / 10)

#define CPMAC_ADM_PHY_PUP_NORMAL_MDIX ((45 * HZ) / 10)
#define CPMAC_ADM_PHY_PUP_IDLE_MDI    ((10 * HZ) / 10)

#define CPMAC_ADM_PHY_PUP_MUTE_TO  ((125 * HZ) / 10)
#define CPMAC_ADM_PHY_PUP_MDI_TO   (( 10 * HZ) / 10)
#define CPMAC_ADM_PHY_PUP_MDIX_TO  (( 10 * HZ) / 10)

#define CPMAC_ARL_UPDATE_INTERVAL  (10 * HZ)
#define CPMAC_AR_AGE_TIME           18        /* Time is given in number of 17 second increments */
#define CPMAC_ARL_REMOVE_TIMER     (60 * HZ)  /* Time, until AR entry is removed */
#define CPMAC_ARL_MAX_QUEUE_LENGTH 256        /* Maximum packets waiting in the AR queue */
#define CPMAC_ARL_MAX_AR_ENTRIES  5000        /* Maximum AR entries */

#define CPMAC_MAX_ASSIGNED_VIDS 4096
#define CPMAC_MAX_VLAN_GROUPS   4096
#define CPMAC_MAX_EXTERN_PORTS     4

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#   define kzalloc(size, flags) \
    ({ \
        void *__ret = kmalloc(size, flags); \
        if(__ret) \
            memset(__ret, 0, size); \
        __ret; \
    })
#endif /*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) ---*/

#endif /*--- #ifndef _CPMAC_CONST_H_ ---*/
