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

#ifndef _CPPHY_IF_G_H_
#define _CPPHY_IF_G_H_

#define UR8_TX_MAC0       16
#define UR8_TX_MAC1       17
#define UR8_TX_COMPLETE    0
#define UR8_RX_QUEUE       0
#define UR8_RX_FREE_QUEUE  0

cpmac_err_t cpphy_if_g_init_rcb(cpphy_cppi_t *cppi, int Num);
cpmac_err_t cpphy_if_g_data_to_phy(cpmac_phy_handle_t phy_handle, struct sk_buff *skb);
void cpphy_if_g_data_to_phy_dma(cpphy_cppi_t *cppi, cpphy_tcb_t *tcb);
void cpphy_if_g_isr_tasklet(unsigned long context);

#endif /*--- #ifndef _CPPHY_IF_G_H_ ---*/
