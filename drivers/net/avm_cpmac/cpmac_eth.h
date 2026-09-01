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

struct cpmac_devinfo {
    unsigned int             pseudo_device;
    int                      is_wan;
    unsigned short           VID;
    struct net_device       *real_dev;
    cpphy_mdio_t            *mdio;
    struct net_device_stats  stats;
    struct dev_mc_list      *old_mc_list;
    int                      old_allmulti;
    int                      old_promiscuity;
};

void cpmac_unregister_device(struct net_device *dev);

void cpmac_update_device(struct net_device *dev, unsigned short VID);

struct net_device *find_or_register_device(cpphy_mdio_t   *mdio,
                                           char           *name,
                                           unsigned short  VID);

/* cpphy_main.c */
void cpmac_set_wan_keep_tagging(struct net_device *dev, unsigned char enable);

