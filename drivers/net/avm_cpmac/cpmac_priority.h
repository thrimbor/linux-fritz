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

#ifndef _CPMAC_PRIORITY_H
#define _CPMAC_PRIORITY_H

#include "cpmac_const.h"
#include <linux/skbuff.h>

enum cpmac_enum_prio_queues {
    CPPHY_PRIO_QUEUE_LAN = 0,
    CPPHY_PRIO_QUEUE_WAN,
    CPPHY_PRIO_QUEUES
};

typedef union BUFFER {
    unsigned int Dword;
    struct _BUFF_PARAM {
        unsigned int length :16;
        unsigned int offset :16;
    } Bits;
} BUFFER;

typedef union PACKET {
    unsigned int Dword;
    struct _PACKET_CTRL {
        unsigned int  buffer_length :16;
        unsigned int  protocol_spec :4;
        unsigned int  region_offset :3;
        unsigned int  packet_error  :1;
        unsigned int  add_buff_cnt  :3;
        unsigned int  descr_type    :1;
        unsigned int  packet_type   :4;
    } Bits;
} PACKET;

typedef union PACKET_TAGS {
    unsigned int Dword;
    struct _BUFF_TAGS {
        unsigned int dest        :16;
        unsigned int sub_channel :4;
        unsigned int channel     :6;
        unsigned int port        :6;
    } Bits;
} PACKET_TAGS;

/* TODO Unite rcb and tcb? */
typedef struct {
#   if defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7)
    unsigned int   HNext;
    unsigned int   BufPtr;
    unsigned int   Off_BLen;
    unsigned int   mode;
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7) ---*/
    void          *NextPacket;
    PACKET_TAGS    Tags;
    PACKET         Packet;
    unsigned int   pData;
    BUFFER         Buffer;
    void          *NextDescr;
#   elif (defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
    /* Nothing needed yet */
#   else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#   error "No rcb definition available!"
#   endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
    void           *Next;
    struct sk_buff *skb;
    void           *KMallocPtr;
} cpphy_rcb_t;

typedef struct {
#   if defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7)
    unsigned int HNext;
    unsigned int BufPtr;
    unsigned int Off_BLen;
    unsigned int mode;
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7) ---*/
    void          *NextPacket;
    PACKET_TAGS    Tags;
    PACKET         Packet;
    unsigned int   pData;
	BUFFER         Buffer;
    void          *NextDescr;
#   elif (defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
    /* Nothing needed yet */
#   else /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
#   error "No tcb definition available!"
#   endif /*--- #else ---*/ /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
    struct sk_buff *skb;
    void           *Next;
    void           *KMallocPtr;
    unsigned int    IsDynamicallyAllocated;
} cpphy_tcb_t;

typedef struct {
    atomic_t               Free;
    atomic_t               DMAFree;
    spinlock_t             lock;
             unsigned int  MaxSize;
             unsigned int  MaxDMAFree;
             unsigned int  BytesEnqueued;
             unsigned int  BytesDequeued;
             unsigned int  Pause;
             unsigned int  PauseInit;
    volatile cpphy_tcb_t  *First;
    volatile cpphy_tcb_t  *Last;
} cpphy_tcb_queue_t;

typedef struct {
    unsigned int      NumberOfPrioQueues;
    atomic_t          SummedFree;
    cpphy_tcb_queue_t q[CPPHY_PRIO_QUEUES];
} cpphy_tcb_prio_queues_t;

#endif /*--- #ifndef _CPMAC_PRIORITY_H ---*/

