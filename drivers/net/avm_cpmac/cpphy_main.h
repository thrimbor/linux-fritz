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

#ifndef _CPPHY_MAIN_H_
#define _CPPHY_MAIN_H_

cpmac_err_t cpphy_main_open (cpphy_global_t *cpphy_global, struct net_device *p_dev);
cpmac_err_t cpphy_main_close (cpphy_cppi_t *cppi);
cpmac_err_t cpphy_main_register (cpphy_global_t *cpphy_global);
void cpphy_main_init_queues(cpphy_global_t *cpphy_global);
void cpphy_main_config_g_apply(cpphy_global_t *cpphy_global, struct net_device *p_dev);


int cpphy_entry_prepare_reboot(struct notifier_block *self, unsigned long kind, void *cmd);

#endif
