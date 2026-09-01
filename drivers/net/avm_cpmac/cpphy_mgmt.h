/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2008,2009,2010 AVM GmbH <fritzbox_info@avm.de>
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

#if !defined(_CPPHY_MGMT_H_)
#define _CPPHY_MGMT_H_

void cpphy_mgmt_power_init(cpphy_mdio_t *mdio);
void cpphy_mgmt_device_power_up(unsigned int reset_bit, unsigned char power_on);
void cpphy_mgmt_global_power(cpphy_mdio_t *mdio, unsigned char power_on);
void cpphy_mgmt_global_power_set(cpmac_power_mode_t mode);
unsigned int cpphy_mgmt_process_global_power(cpphy_mdio_t *mdio); /* TODO Should be here only temporarily */
unsigned int cpphy_mgmt_check_link_phy(cpphy_mdio_t *mdio);
unsigned int cpphy_mgmt_check_link_dummy(cpphy_mdio_t *mdio);
unsigned int cpphy_mgmt_check_link_emv(cpphy_mdio_t *mdio);
unsigned int cpphy_mgmt_check_link_orig(cpphy_mdio_t *mdio);
unsigned int cpphy_mgmt_check_link(cpphy_mdio_t *mdio);
void cpphy_mgmt_event_dataupdate(cpphy_mdio_t *mdio);
void cpphy_mgmt_check_powersave_settings(void);
void cpphy_mgmt_power(cpphy_mdio_t *mdio);
cpmac_err_t cpphy_mgmt_work_add(cpphy_mdio_t        *mdio, 
                                cpmac_work_ident_t   ident,
                                cpmac_work_function  function,
                                unsigned long        initial_timeout);
cpmac_err_t cpphy_mgmt_work_del(cpphy_mdio_t *mdio, cpmac_work_ident_t ident);
void cpphy_mgmt_work_start(cpphy_mdio_t *mdio);
void cpphy_mgmt_work_schedule(cpphy_mdio_t *mdio, 
                              cpmac_work_ident_t ident, 
                              unsigned long timeout);
void cpphy_mgmt_work_stop(cpphy_mdio_t *mdio);
cpmac_err_t cpphy_mgmt_init(cpphy_mdio_t *mdio);
void cpphy_mgmt_deinit(cpphy_mdio_t *mdio);
void cpphy_mgmt_support_data(cpphy_mdio_t *mdio);
void cpphy_mgmt_debug(cpphy_mdio_t *mdio, unsigned int value);
cpmac_err_t cpphy_mgmt_debug_peek(void *ptr, unsigned int size, unsigned int length);
cpmac_err_t cpphy_mgmt_debug_poke(void *ptr, unsigned int size, unsigned int value);
unsigned long cpphy_mgmt_debug_work(cpphy_mdio_t *mdio);


unsigned short cpphy_switch_get_vid_group(cpphy_mdio_t *mdio,
                                          unsigned char bitcombination);
unsigned int cpphy_switch_check_assigned_vid(cpphy_mdio_t *mdio,
                                             unsigned short vid);
void cpphy_switch_status(cpphy_mdio_t *mdio);
cpmac_err_t cpphy_switch_configure_special(cpphy_mdio_t *mdio, 
                                           cpmac_switch_configuration_t *config);
cpmac_err_t adm_get_configured(cpphy_mdio_t *mdio,
                               cpmac_switch_configuration_t *config);
cpmac_err_t adm_get_config_cpmac(cpphy_mdio_t *mdio,
                                 struct avm_cpmac_config_struct *config);
unsigned int cpphy_switch_register_ffw_port(cpmac_fast_forward_rx_func_ptr f, 
                                            unsigned char                  port);
void cpphy_switch_unregister_ffw_port(unsigned char port);
void cpphy_switch_device_vlan_change_notify(char *devicename, unsigned short vid);
cpmac_err_t cpphy_switch_add_trunk_port(cpphy_mdio_t *mdio, 
                                        cpmac_trunk_port_configuration_t trunk_config);


#endif /*--- #if !defined(_CPPHY_MGMT_H_) ---*/

