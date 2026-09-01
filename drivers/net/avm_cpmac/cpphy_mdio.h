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

#ifndef _CPPHY_MDIO_H_
#define _CPPHY_MDIO_H_

unsigned int cpphy_mdio_user_access_read(cpphy_mdio_t *mdio, 
                                         unsigned short regadr,
                                         unsigned short phyadr);
void cpphy_mdio_user_access_write(cpphy_mdio_t *mdio, 
                                  unsigned short regadr, 
                                  unsigned short phyadr, 
                                  unsigned short data);
unsigned short mdio_write(cpphy_mdio_t  *mdio, 
                          unsigned short phy, 
                          unsigned short address,
                          unsigned short value);
unsigned short mdio_read(cpphy_mdio_t *mdio, unsigned short phy, unsigned short address);
void cpphy_mdio_update (cpphy_mdio_t *mdio, struct cpmac_event_struct *event_struct);
void cpphy_mdio_phy_check_link(cpphy_mdio_t *mdio);
unsigned long cpphy_mdio_tick(cpphy_mdio_t *mdio);
void cpphy_mdio_init_phy(cpphy_mdio_t *mdio);
void cpphy_mdio_init_vitesse(cpphy_mdio_t *mdio);
unsigned int cpphy_mdio_check_link_vitesse(cpphy_mdio_t *mdio);
void cpphy_mdio_init_11G(cpphy_mdio_t *mdio);
unsigned int cpphy_mdio_check_link_11G(cpphy_mdio_t *mdio);
void cpphy_mdio_init_adm7001(cpphy_mdio_t *mdio);
unsigned int cpphy_mdio_check_link_adm7001(cpphy_mdio_t *mdio);
void cpphy_mdio_init(cpphy_global_t *cpphy_global);
void cpphy_mdio_work_start(cpphy_mdio_t *mdio, unsigned int timeout);
void cpphy_mdio_work_stop(cpphy_mdio_t *mdio);
void cpphy_mdio_deinit(cpphy_mdio_t *mdio);

#endif
