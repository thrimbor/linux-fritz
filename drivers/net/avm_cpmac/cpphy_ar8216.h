/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,2007,...,2012 AVM GmbH <fritzbox_info@avm.de>
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

#ifndef _CPPHY_AR8216_H_
#define _CPPHY_AR8216_H_

#include <linux/avm_cpmac.h>

void ar_ar8216_init(cpphy_mdio_t *mdio);
void ar_ar8226_init(cpphy_mdio_t *mdio);
void ar_ar8316_init(cpphy_mdio_t *mdio);
void ar_ar8327_init(cpphy_mdio_t *mdio);
void ar_ar8316_mac5_enable(cpphy_mdio_t *mdio, unsigned char enable);
void ar_ar8216_deinit(cpphy_mdio_t *mdio);
void ar_ar8226_deinit(cpphy_mdio_t *mdio);
void ar_ar8316_deinit(cpphy_mdio_t *mdio);
void ar_ar8327_deinit(cpphy_mdio_t *mdio);
void ar8216_ar_init(void);
void ar8216_ar_deinit(void);
void ar8216_ar_del_port(cpphy_mdio_t *mdio, unsigned char port);
void ar8216_add_vlan_vid(cpphy_mdio_t *mdio, unsigned int vid, unsigned int mem_mask);
void ar8327_add_vlan_vid(cpphy_mdio_t *mdio, unsigned int vid, unsigned int mem_mask);
void ar_vlan_fbox_reset (cpphy_mdio_t *mdio);
unsigned int ar_check_link_orig(cpphy_mdio_t *mdio);
unsigned int ar_check_link_emv(cpphy_mdio_t *mdio);
unsigned int ar_switch_test_link(cpphy_mdio_t *mdio, unsigned char port);
void ar8316_switch_port_power(cpphy_mdio_t *mdio, unsigned char port, unsigned char power_on);
void ar8316_switch_port_speed_throttle(cpphy_mdio_t *mdio, unsigned char port, unsigned char do_throttle);
void ar_switch_port_power_orig(cpphy_mdio_t *mdio, unsigned char port, unsigned char power_on);
void ar_switch_port_power_emv(cpphy_mdio_t *mdio, unsigned char port, cpphy_switch_powermodes_t mode);
void ar_boot_power_on_emv(cpphy_mdio_t *mdio);
void ar_switch_port_power_on_boot_orig(cpphy_mdio_t *mdio, unsigned short portset);
void ar8216_mdio_write16(cpphy_mdio_t *mdio, unsigned short regadr, unsigned short data);
int ar8216_mdio_write32(cpphy_mdio_t *mdio, unsigned int address, unsigned int data);
unsigned short ar8216_mdio_read16(cpphy_mdio_t *mdio, unsigned short regadr);
unsigned int ar8216_mdio_read32(cpphy_mdio_t *mdio, unsigned int address);
enum ath_display_regs {ATH_PORT_REGISTERS, ATH_CONTROL, ATH_MII};
void display_ath_regs(cpphy_mdio_t *mdio, enum ath_display_regs control);
void ar8316_print_table(cpphy_mdio_t *mdio);
void display_mdio_regs(cpphy_mdio_t *mdio);
void switch_dump_atheros(cpphy_mdio_t *mdio);
void ar8216_dump_counters(cpphy_mdio_t *mdio);
void ar8316_dump_counters(cpphy_mdio_t *mdio);
void ar8327_dump_counters(cpphy_mdio_t *mdio);

unsigned long ar8216_ar_work_item(cpphy_mdio_t *mdio);
unsigned char ar8216_get_phy_port(cpphy_mdio_t *mdio, struct sk_buff *skb);

void ar_config_ports(cpphy_mdio_t *mdio);
void ar8327_config_ports(cpphy_mdio_t *mdio);
void ar_vlan_clear(cpphy_mdio_t *mdio);
void ar8327_vlan_clear(cpphy_mdio_t *mdio);
cpmac_err_t ar_configure_cpmac(cpphy_mdio_t *mdio, struct avm_cpmac_config_struct *config);
unsigned short cpphy_ar_get_new_vid(cpphy_mdio_t  *mdio,
                                    unsigned char  device);
unsigned short cpphy_ar_get_vid_group(cpphy_mdio_t *mdio);
void cpphy_ar_set_default_vid(cpphy_mdio_t   *mdio, 
                              unsigned char   device, 
                              unsigned short  portset);
void ar_config_vlan_group(cpphy_mdio_t   *mdio,
                          unsigned short  vid,
                          unsigned short  portset,
                          unsigned short  tagged_portset,
                          unsigned char   port,
                          unsigned char   FID,
                          unsigned char   use_as_default);
void ar8327_config_vlan_group(cpphy_mdio_t   *mdio,
                              unsigned short  vid,
                              unsigned short  portset,
                              unsigned short  tagged_portset,
                              unsigned char   in_port __attribute__ ((unused)),
                              unsigned char   FID __attribute__ ((unused)),
                              unsigned char   use_as_default __attribute__ ((unused)));
void adm_switch_wan_keep_tagging_ar8x16(cpphy_mdio_t *mdio, unsigned char port);
void cpphy_ar8316_mirror_port(cpphy_mdio_t *mdio);
void cpphy_ar8216_set_igmp_fwd(cpphy_mdio_t *mdio, unsigned int fwd_portmap);
void cpphy_ar8327_set_igmp_fwd(cpphy_mdio_t *mdio, unsigned int fwd_portmap);
cpmac_err_t ar8327_add_port_to_wan(cpphy_mdio_t *mdio, unsigned char port, unsigned short vid);

#define ATH_GET_TAG_GROUP(x) (((x) - 0x10) & 0xf)

#endif /*--- #ifndef _CPPHY_AR8216_H_ ---*/

