/*
 *     Definitions for AVM RTP Timestamp
 *
 * Copyright (c) 2012-2013 AVM GmbH <info@avm.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed and/or modified under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
 
#ifndef __RTP_TIMESTAMP_H
#define __RTP_TIMESTAMP_H

#ifdef __KERNEL__
#include <linux/skbuff.h>
#endif

enum _rtp_timestamp_state {
   ts_stopped = 0,    
   ts_running = 1,    
   ts_running_with_stamp_remove = 2    
};
 
struct _rtp_timestamp_stat {
    enum _rtp_timestamp_state state; 
    unsigned short session; /* RTP Port */
    signed long    incoming_cnt; 
    signed long    incoming_sum;
    signed long    incoming_quadsum;    
    signed long    incoming_min;
    signed long    incoming_max;
    signed long    outgoing_cnt; 
    signed long    outgoing_sum;
    signed long    outgoing_quadsum;    
    signed long    outgoing_min;
    signed long    outgoing_max;    
};
#ifdef __KERNEL__
void rtp_timestamp_start_stat(unsigned short rtp_port, int remove_stamp_on_egress);
void rtp_timestamp_stop_stat(unsigned short rtp_port);
void rtp_timestamp_clear_stat(unsigned short rtp_port);

void rtp_timestamp_insert_in_skb(struct sk_buff *skb);
int rtp_timestamp_insert_in_rtp(unsigned short rtp_port, unsigned char *data, unsigned int datalen);

void rtp_timestamp_trace_session_from_skb_data(struct sk_buff *skb, unsigned short is_outgoing);
void rtp_timestamp_trace_session_from_skb_network(struct sk_buff *skb, unsigned short with_ethernet_encap, unsigned short is_outgoing); /* for kdsldmod */
void rtp_timestamp_trace_session_from_rtp(unsigned short rtp_port, unsigned char *data, unsigned int *datalen, unsigned short is_outgoing);

void rtp_timestamp_read_stat(unsigned short rtp_port, struct _rtp_timestamp_stat *rtp_ts_stat);

int get_padding_len(void);
#endif
#endif /* __RTP_TIMESTAMP_H */