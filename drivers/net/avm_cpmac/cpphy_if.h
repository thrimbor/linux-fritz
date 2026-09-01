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

#ifndef _CPPHY_IF_H_
#define _CPPHY_IF_H_

cpmac_err_t cpphy_if_control_req (cpmac_phy_handle_t phy_handle, cpmac_control_req_t control, ...);
cpmac_err_t cpphy_if_data_to_phy (cpmac_phy_handle_t phy_handle, struct sk_buff *skb);
cpmac_err_t cpphy_if_init (cpmac_phy_handle_t phy_handle, cpmac_priv_t *cpmac_priv);
unsigned long switch_dump(cpphy_mdio_t *mdio);
cpmac_err_t cpphy_switch_ioctl(cpphy_mdio_t *mdio, struct avm_cpmac_ioctl_struct *ioctl_struct);
cpphy_tcb_t *cpphy_if_alloc_tcb(cpphy_cppi_t *cppi);
unsigned int cpphy_if_check_external_tagging(cpphy_cppi_t *cppi, cpphy_tcb_t *tcb);
void cpphy_if_data_from_queues(cpphy_cppi_t *cppi);
void cpphy_if_deinit (cpmac_phy_handle_t phy_handle);
void cpphy_if_free_tcb(cpphy_cppi_t *cppi, cpphy_tcb_t *tcb);
void cpphy_if_isr_end(cpmac_phy_handle_t phy_handle);
void cpphy_if_isr_tasklet (unsigned long context);
void cpphy_if_tcb_enqueue(cpphy_cppi_t *cppi, unsigned char priority, cpphy_tcb_t *tcb);
void cpphy_if_tick (cpmac_phy_handle_t phy_handle);
void cpphy_if_free_tx_skb(cpmac_priv_t   *cpmac_priv,
                          struct sk_buff *skb,
                          unsigned int    status);
void cpphy_if_tx_complete(cpphy_cppi_t *cppi,
                          struct sk_buff *skb,
                          unsigned int status);
void cpphy_if_tx_stop_queue(cpphy_cppi_t *cppi);
void cpphy_if_tx_restart_queue(cpphy_cppi_t *cppi);

#endif /*--- #ifndef _CPPHY_IF_H_ ---*/

