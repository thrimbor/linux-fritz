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

#ifndef _CPPHY_CPPI_H_
#define _CPPHY_CPPI_H_

void *cpphy_cppi_malloc_buffer(unsigned int size, unsigned int tot_buf_size,
                               unsigned int tot_reserve_bytes, struct net_device *p_dev,
                               struct sk_buff **skb);
void cpphy_cppi_update_hw_status (cpphy_cppi_t *cppi, struct net_device_stats *stats);
cpmac_err_t cpphy_cppi_rx_int (cpphy_cppi_t *cppi);
int cpphy_cppi_tx_int (cpphy_cppi_t *cppi);
void cpphy_cppi_free_rcb (cpphy_cppi_t *cppi);
void cpphy_cppi_free_tcb (cpphy_cppi_t *cppi);
void cpphy_cppi_set_multi_promiscous (cpphy_cppi_t *cppi, unsigned int multi, unsigned int promiscous);
cpmac_err_t cpphy_cppi_teardown (cpphy_cppi_t *cppi, unsigned int Mode);
cpmac_err_t cpphy_cppi_start_dma(cpphy_cppi_t *cppi);
void cpphy_cppi_rx_dma_pause(cpphy_cppi_t *cppi);
void cpphy_cppi_rx_dma_restart(cpphy_cppi_t *cppi);

#endif
