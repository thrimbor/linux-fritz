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

#ifndef _CPPHY_ADM6996_H_
#define _CPPHY_ADM6996_H_

#include <linux/avm_cpmac.h>
#include "cpmac_main.h"

/* Interprete four bits of the VID as VID group (FID) starting from bit given here */
#define ADM_6996_TAGSHIFT 8
#define ADM_GET_TAG_GROUP(x) (((x) >> ADM_6996_TAGSHIFT) & 0xf)

#define ADM_DEVICE_ADDRESS 0x100
#define ADM_SERIAL_OFFSET 0x200
#define ADM_GET_SERIAL_REG(mdio, addr) adm_read_32Bit_cached((mdio), (addr))
#define ADM_GET_EEPROM_REG adm_read
/*--- #define ADM_GET_EEPROM_REG adm_safe_read ---*/
#define ADM_PUT_EEPROM_REG(mdio, addr, dat) adm_write((mdio), (addr), (dat))

cpmac_err_t adm_configure_cpmac(cpphy_mdio_t *mdio, struct avm_cpmac_config_struct *config);
cpmac_err_t adm_set_mode_pppoa(cpphy_mdio_t *mdio, unsigned int enable_pppoa);
cpmac_err_t adm_switch_port_power_config(cpphy_mdio_t          *mdio,
                                         adm_phy_power_setup_t *setup,
                                         unsigned char          set);
int adm_write(cpphy_mdio_t *mdio, unsigned short addr, unsigned short dat);
int adm_write_32(cpphy_mdio_t *mdio, unsigned int addr, unsigned int dat);
unsigned int adm_6996l_check_link(cpphy_mdio_t *mdio);
unsigned int adm_6996xc_check_link(cpphy_mdio_t *mdio);
unsigned int adm_check_link(cpphy_mdio_t *mdio);
unsigned int adm_read_32Bit_cached(cpphy_mdio_t *mdio, unsigned int addr);
unsigned short adm_read(cpphy_mdio_t *mdio, unsigned short addr);
unsigned int adm_read_32(cpphy_mdio_t *mdio, unsigned int addr);
unsigned int adm_tantos_check_link(cpphy_mdio_t *mdio);
unsigned long adm_set_wan_keep_tagging(cpphy_mdio_t *mdio);
unsigned char cpphy_adm_get_bitcombination(cpphy_mdio_t *mdio, unsigned char device);
unsigned short cpphy_adm_get_new_vid(cpphy_mdio_t  *mdio,
                                     unsigned char  device);
unsigned short cpphy_tantos_get_new_vid(cpphy_mdio_t  *mdio,
                                        unsigned char  device);

unsigned short tantos_read(cpphy_mdio_t *mdio, unsigned short *ptr);
cpmac_err_t adm_add_port_to_wan(cpphy_mdio_t *mdio, unsigned char port, unsigned short vid);
void adm_adm6996_config_vlan_group(cpphy_mdio_t   *mdio,
                                   unsigned short  vid,
                                   unsigned short  portset,
                                   unsigned short  tagged_portset,
                                   unsigned char   port,
                                   unsigned char   FID,
                                   unsigned char   use_as_default);
void adm_adm6996l_init(cpphy_mdio_t *mdio);
void adm_adm6996xc_init(cpphy_mdio_t *mdio);
void adm_config_ports(cpphy_mdio_t *mdio);
void adm_tantos_config_ports(cpphy_mdio_t *mdio);
void adm_prepare_reboot(cpphy_mdio_t *mdio);
void adm_set_good_vid_bitshift(cpphy_mdio_t *mdio, struct avm_cpmac_config_struct *config);
void adm_switch_port_power(cpphy_mdio_t *mdio, unsigned char port, unsigned char power_on);
void adm_switch_port_power_6996(cpphy_mdio_t *mdio, 
                                unsigned char port, 
                                unsigned char power_on);
void adm_switch_port_power_tantos(cpphy_mdio_t *mdio, 
                                  unsigned char port, 
                                  unsigned char power_on);
void adm_switch_wan_keep_tagging_adm6996(cpphy_mdio_t *mdio, unsigned char port);
void adm_switch_wan_keep_tagging_ar8x16(cpphy_mdio_t *mdio, unsigned char port);
void adm_tantos_config_vlan_group(cpphy_mdio_t   *mdio,
                                  unsigned short  vid,
                                  unsigned short  portset,
                                  unsigned short  tagged_portset,
                                  unsigned char   port,
                                  unsigned char   FID,
                                  unsigned char   use_as_default);
void adm_tantos_init(cpphy_mdio_t *mdio);
void adm_update_cache(cpphy_mdio_t *mdio);
void adm_update_error_status(cpphy_mdio_t *mdio, unsigned char port);
void adm_vlan_clear(cpphy_mdio_t *mdio);
void adm_vlan_fbox_reset(cpphy_mdio_t *mdio);
void adm_vlan_reset(cpphy_mdio_t *mdio);
void cpphy_adm_set_default_vid(cpphy_mdio_t   *mdio, 
                               unsigned char   device, 
                               unsigned short  portset);
cpmac_err_t cpphy_adm_set_port_vid_mapping(cpphy_mdio_t *mdio, 
                                           unsigned char device,
                                           unsigned char port);
void cpphy_adm_update_hw_status(cpphy_mdio_t *mdio, struct net_device_stats *stats);
void cpphy_tantos_set_default_vid(cpphy_mdio_t   *mdio, 
                                  unsigned char   device, 
                                  unsigned short  portset);
void switch_dump_adm6996(cpphy_mdio_t *mdio);
void switch_dump_tantos(cpphy_mdio_t *mdio);

#if defined(CONFIG_AVM_POWER)
int adm_power_config(int state);
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/

#endif /*--- #ifndef _CPPHY_ADM6996_H_ ---*/

