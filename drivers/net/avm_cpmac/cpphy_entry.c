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

#include <stdarg.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#if !defined(CONFIG_NETCHIP_ADM69961)
#define CONFIG_NETCHIP_ADM69961
#endif
#include <linux/avm_event.h>
#if defined(CONFIG_MIPS_OHIO)
#include <asm/mips-boards/ohio.h>
#endif /*--- #if defined(CONFIG_MIPS_OHIO) ---*/
#include <linux/version.h>
#if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE)
#include <linux/env.h>
#else /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
#include <asm/mips-boards/prom.h>
#endif /*--- #else ---*/ /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
#if defined(CONFIG_ARCH_PUMA5)
#include <mach/hw_gpio.h>
#endif /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/
#if defined(CONFIG_MIPS_UR8)

#if (KERNEL_VERSION(2,6,32) <= LINUX_VERSION_CODE)
#include <ur8.h>
#include <asm/prom.h>
#else
#include <asm/mips-boards/ur8.h>
#endif

#include <asm/mach-ur8/hw_nwss.h>
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/
#include <asm/mach_avm.h>

#include "cpmac_if.h"
#include "cpmac_const.h"
#include "cpmac_debug.h"
#include "cpphy_const.h"
#include "cpphy_types.h"
#include "cpphy_main.h"
#include "cpmac_main.h"
#include "cpphy_adm6996.h"
#include "cpphy_ar8216.h"
#include "cpphy_mdio.h"
#include "cpgmac_f.h"
#include "cpmac_product_conf.h"
#include "cpphy_mgmt.h"
#include "cpphy_if.h"
#include "cpmac_fusiv_if.h"
#if defined(CONFIG_AVM_POWER)
#include <linux/avm_power.h>
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/


MODULE_AUTHOR("Maintainer: AVM GmbH");
MODULE_DESCRIPTION("Ethernet driver for FRITZ!Box");
MODULE_LICENSE("GPL");

/*----------------------------------------------------------------------------------*\
 * Zugriff auf cpmac_cpphy_global aus anderen Modulen ueber phy_handle              *
\*----------------------------------------------------------------------------------*/
cpmac_global_t cpmac_global;
extern cpmac_product_struct cpmac_products;

/*------------------------------------------------------------------------------------------*\
 * Global resources                                                                         *
\*------------------------------------------------------------------------------------------*/
static struct resource cpmac_resource_reset_gpio_phy0 = {
    .name = "CPMAC reset phy 0",
    .flags = IORESOURCE_IO		
};

static struct resource cpmac_resource_reset_gpio_phy1 = {
    .name = "CPMAC reset phy 1",
    .flags = IORESOURCE_IO		
};


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_entry_invalid_product_warning(void) {
    DEB_ERR("*****************************************************************************\n");
    DEB_ERR("*****************************************************************************\n");
    DEB_ERR("*****************************************************************************\n");
    DEB_ERR("***                                                                       ***\n");
    DEB_ERR("***                                                                       ***\n");
    DEB_ERR("***                                                                       ***\n");
    DEB_ERR("*** ATTENTION! There is no valid ethernet configuration for this product! ***\n");
    DEB_ERR("***                                                                       ***\n");
    DEB_ERR("***                                                                       ***\n");
    DEB_ERR("***                                                                       ***\n");
    DEB_ERR("*****************************************************************************\n");
    DEB_ERR("*****************************************************************************\n");
    DEB_ERR("*****************************************************************************\n");
}


/*------------------------------------------------------------------------------------------*\
 * Functions for working with different device names                                        *
\*------------------------------------------------------------------------------------------*/
static void cpmac_device_name_init(char *name) {
    unsigned char i;
    static unsigned int function_has_been_run = 0;
    
    if(function_has_been_run) {
        return;
    }
    function_has_been_run = 1;

#   if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)
    /* For 7340 switch the port/instance assignments */
    if(cpmac_global.phys > 1) {
        cpmac_global.device_name_length[0] = sprintf(cpmac_global.device_name[0], "%s%u", name, 1);
        cpmac_global.device_name_length[1] = sprintf(cpmac_global.device_name[1], "%s%u", name, 0);
    } else
#   endif /*--- #if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) ---*/
    {
        for(i = 0; i < min((unsigned int) cpmac_global.phys, AVM_CPMAC_MAX_PHYS); i++) {
            cpmac_global.device_name_length[i] = sprintf(cpmac_global.device_name[i], "%s%u", name, i);
        }
    }

    DEB_TRC("[%s] result:\n", __FUNCTION__);
    for(i = 0; i < min((unsigned int) cpmac_global.phys, AVM_CPMAC_MAX_PHYS); i++) {
        DEB_TRC("[%s]     %s with length %u defined\n", 
                __FUNCTION__, 
                cpmac_global.device_name[i],
                cpmac_global.device_name_length[i]);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *cpmac_device_name_get(unsigned int device_number) {
    if(device_number >= cpmac_global.phys) {
        DEB_ERR("[%s] Illegal device number %u >= %u!\n", 
                __FUNCTION__, device_number, cpmac_global.phys);
        return "";
    }
    DEB_TRC("[%s] for device %u: '%s'\n", __FUNCTION__, 
            device_number, cpmac_global.device_name[device_number]);
    return cpmac_global.device_name[device_number];
}
EXPORT_SYMBOL(cpmac_device_name_get);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpmac_device_name_cmp(char *name, unsigned int device_number) {
    if(device_number >= cpmac_global.phys) {
        DEB_TRC("[%s] Unexpected device '%s' number %u >= %u!\n", 
                __FUNCTION__, name, device_number, cpmac_global.phys);
        return 0;
    }
    return (strncmp(name, cpmac_global.device_name[device_number], cpmac_global.device_name_length[device_number]) == 0);
}
EXPORT_SYMBOL(cpmac_device_name_cmp);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpmac_get_number_of_instances(void) {
    return cpmac_global.phys;
}
EXPORT_SYMBOL(cpmac_get_number_of_instances);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int cpphy_entry_check_for_emv(void) {
    unsigned long long m0, m1, m2, m3, m4, m5, mac;
    unsigned char *macstring = prom_getenv("maca");
    if(macstring == NULL)
        return 0;

    sscanf(macstring, "%llx:%llx:%llx:%llx:%llx:%llx", &m0, &m1, &m2, &m3, &m4, &m5);
    mac =   ((m0 & 0xff) << 40)
          | ((m1 & 0xff) << 32)
          | ((m2 & 0xff) << 24)
          | ((m3 & 0xff) << 16)
          | ((m4 & 0xff) <<  8)
          | ((m5 & 0xff) <<  0);

    return (   0
            || ((mac >= 0x001C4A000000ull) && (mac <= 0x001C4AFFFFFFull))
            || ((mac >= 0x001F3F000000ull) && (mac <= 0x001F3F345DC9ull))
            || ((mac >= 0x001F3F3AE817ull) && (mac <= 0x001F3F6DE11Cull))
            || ((mac >= 0x0024FE000000ull) && (mac <= 0x0024FE1EE1F9ull)) 
           );
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void cpphy_entry_set_config(cpphy_global_t              *cpphy, 
                                   unsigned char                phy) {
    cpphy_mdio_t *mdio = &cpphy->mdio;
    unsigned char mode, device;

    mdio->Mode = 0 | CPPHY_SWITCH_MODE_WRITE | CPPHY_SWITCH_MODE_READ; /* 32 Bit, read, write */
    mdio->switch_config.cpu_port = 5;
    mdio->switch_config.used_portset = cpphy->config->ports;
    mdio->switch_config.mii_portmask = cpphy->config->mii_portmask;
    mdio->mode_pppoa = 0;
    if(cpphy->config->reset_pin != 0xffff) {
        struct resource *resource_ptr;
        if(phy == 0) {
            resource_ptr = &cpmac_resource_reset_gpio_phy0;
        } else {
            resource_ptr = &cpmac_resource_reset_gpio_phy1;
        }
        resource_ptr->start = resource_ptr->end = cpphy->config->reset_pin;
        if(request_resource(&gpio_resource, resource_ptr)) {
            DEB_ERR("[%s] Error! Could not reserve GPIO %u for reset!\n", __FUNCTION__, cpphy->config->reset_pin);
            return;
        }
#       if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)
        mdio->reset_bit = cpphy->config->reset_pin;
        avm_gpio_ctrl(mdio->reset_bit, GPIO_PIN, GPIO_OUTPUT_PIN);
        avm_gpio_out_bit(mdio->reset_bit, 0);
#       else /*--- #if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) ---*/
        mdio->reset_bit = (unsigned int) avm_register_reset_gpio(cpphy->config->reset_pin, LOW_ACTIVE);
        if(mdio->reset_bit == (unsigned int) -1) {
            DEB_ERR("[%s] Error! Could not register GPIO %u for reset!\n", __FUNCTION__, cpphy->config->reset_pin);
            return;
        }
#       endif /*--- #else ---*/ /*--- #if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) ---*/
        cpphy_mgmt_global_power(mdio, 0);
        cpphy_mgmt_global_power_set(CPPHY_POWER_GLOBAL_OFF);
    }

    switch(cpphy->config->type) {
#       if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
        case CPMAC_PHY_TYPE_ADM6996FC:
            DEB_INFOTRC("[%s] Configure functions for ADM6996xC\n", __FUNCTION__);
            cpphy->f->check_external_tagging  = cpphy_if_check_external_tagging;
            cpphy->f->check_link              = adm_6996xc_check_link;
            cpphy->f->config_ports            = adm_config_ports;
            cpphy->f->configure_special       = cpphy_switch_configure_special;
            cpphy->f->config_vlan_group       = adm_adm6996_config_vlan_group;
            cpphy->f->get_new_vid             = cpphy_adm_get_new_vid;
            cpphy->f->get_switch_register     = adm_read_32;
            cpphy->f->init_phy                = adm_adm6996xc_init;
            cpphy->f->deinit_phy              = 0;
            cpphy->f->mgmt_check_link         = cpphy_mgmt_check_link_orig;
            cpphy->f->prepare_reboot          = adm_prepare_reboot;
            cpphy->f->set_default_vid         = cpphy_adm_set_default_vid;
            cpphy->f->set_good_vid_bitshift   = adm_set_good_vid_bitshift;
            cpphy->f->set_port_vid_mapping    = cpphy_adm_set_port_vid_mapping;
            cpphy->f->set_switch_register     = adm_write_32;
            cpphy->f->set_wan_keep_tagging    = adm_set_wan_keep_tagging;
            cpphy->f->switch_dump             = switch_dump_adm6996;
            cpphy->f->switch_port_power       = adm_switch_port_power_6996;
            cpphy->f->switch_wan_keep_tagging = adm_switch_wan_keep_tagging_adm6996;
            cpphy->f->update_hw_status        = cpphy_adm_update_hw_status;
            cpphy->f->vlan_clear              = adm_vlan_clear;
            cpphy->mdio.switch_config.cpu_port             = 5;
            cpphy->mdio.switch_config.is_switch            = 1;
            cpphy->mdio.switch_config.ports                = 6;
            cpphy->mdio.switch_config.external_port_offset = 0;
            cpphy->mdio.switch_config.max_vid_groups       = 16;
            break;
        case CPMAC_PHY_TYPE_TANTOS:
            DEB_INFOTRC("[%s] Configure functions for Tantos\n", __FUNCTION__);
            cpphy->f->add_port_to_wan        = adm_add_port_to_wan;
            cpphy->f->check_link             = adm_tantos_check_link;
            cpphy->f->config_ports           = adm_tantos_config_ports;
            cpphy->f->configure_special      = cpphy_switch_configure_special;
            cpphy->f->config_vlan_group      = adm_tantos_config_vlan_group;
            cpphy->f->get_new_vid            = cpphy_tantos_get_new_vid;
            cpphy->f->get_switch_register    = adm_read_32;
            cpphy->f->init_phy               = adm_tantos_init;
            cpphy->f->deinit_phy             = 0;
            cpphy->f->mgmt_check_link        = cpphy_mgmt_check_link_orig;
            cpphy->f->set_default_vid        = cpphy_tantos_set_default_vid;
            cpphy->f->set_switch_register    = adm_write_32;
            cpphy->f->switch_dump            = switch_dump_tantos;
            cpphy->f->switch_port_power      = adm_switch_port_power_tantos;
            cpphy->mdio.switch_config.cpu_port             = 5;
            cpphy->mdio.switch_config.is_switch            = 1;
            cpphy->mdio.switch_config.ports                = 7;
            cpphy->mdio.switch_config.external_port_offset = 0;
            cpphy->mdio.switch_config.max_vid_groups       = 16;
            break;
#       endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/
        case CPMAC_PHY_TYPE_AR8216:
            DEB_INFOTRC("[%s] Configure functions for AR8216\n", __FUNCTION__);
#           if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8)
            cpphy->f->set_wan_keep_tagging    = adm_set_wan_keep_tagging;
            cpphy->f->switch_wan_keep_tagging = adm_switch_wan_keep_tagging_ar8x16;
#           endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) ---*/
            /*--- cpphy->f->switch_port_power       = ar_switch_port_power_orig; ---*/
            /*--- cpphy->f->power_on_boot           = ar_switch_port_power_on_boot_orig; ---*/
            cpphy->f->check_link              = cpphy->mdio.switch_config.use_EMV ? ar_check_link_emv
                                                                                  : ar_check_link_orig;
            cpphy->f->config_ports            = ar_config_ports;
            cpphy->f->config_vlan_group       = ar_config_vlan_group;
            cpphy->f->dump_counters           = ar8216_dump_counters;
            cpphy->f->init_phy                = ar_ar8216_init;
            cpphy->f->deinit_phy              = ar_ar8216_deinit;
            cpphy->f->mgmt_check_link         = cpphy->mdio.switch_config.use_EMV ? cpphy_mgmt_check_link_emv
                                                                                  : cpphy_mgmt_check_link_orig;
            cpphy->f->set_igmp_fwd            = cpphy_ar8216_set_igmp_fwd;
            cpphy->f->vlan_add                = ar8216_add_vlan_vid;
            cpphy->f->vlan_clear              = ar_vlan_clear;
            cpphy->mdio.switch_config.max_vid_groups       = 16;
            cpphy->mdio.switch_config.mdio_upperhalf_first =  1;
            goto generic_atheros;
        case CPMAC_PHY_TYPE_AR8226:
            /* Attention! Copy changes to ar_ar8216_init, too! */
            DEB_INFOTRC("[%s] Configure functions for AR8226\n", __FUNCTION__);
#           if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8)
            cpphy->f->set_wan_keep_tagging    = adm_set_wan_keep_tagging;
            cpphy->f->switch_wan_keep_tagging = adm_switch_wan_keep_tagging_ar8x16;
#           endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) ---*/
            cpphy->f->add_port_to_wan         = adm_add_port_to_wan;
            cpphy->f->check_link              = ar_check_link_orig;
            cpphy->f->config_ports            = ar_config_ports;
            cpphy->f->config_vlan_group       = ar_config_vlan_group;
            cpphy->f->dump_counters           = ar8316_dump_counters;
            cpphy->f->init_phy                = ar_ar8226_init;
            cpphy->f->deinit_phy              = ar_ar8226_deinit;
            cpphy->f->mgmt_check_link         = cpphy_mgmt_check_link_orig;
            cpphy->f->set_igmp_fwd            = cpphy_ar8216_set_igmp_fwd;
            cpphy->f->switch_port_power       = ar8316_switch_port_power;
            cpphy->f->vlan_add                = ar8216_add_vlan_vid;
            cpphy->f->vlan_clear              = ar_vlan_clear;
            cpphy->mdio.switch_config.max_vid_groups       = 16;
            cpphy->mdio.switch_config.mdio_upperhalf_first =  1;
            goto generic_atheros;
        case CPMAC_PHY_TYPE_AR8327:
            DEB_INFOTRC("[%s] Configure functions for AR8327\n", __FUNCTION__);
            cpphy->f->add_port_to_wan        = ar8327_add_port_to_wan;
            cpphy->f->check_link             = ar_check_link_orig;
            cpphy->f->config_ports           = ar8327_config_ports;
            cpphy->f->config_vlan_group      = ar8327_config_vlan_group;
            cpphy->f->dump_counters          = ar8327_dump_counters;
            cpphy->f->init_phy               = ar_ar8327_init;
            cpphy->f->deinit_phy             = ar_ar8327_deinit;
            cpphy->f->mgmt_check_link        = cpphy_mgmt_check_link_orig;
            cpphy->f->mirror_port            = cpphy_ar8316_mirror_port;
            cpphy->f->print_mac_table        = ar8316_print_table;
            cpphy->f->set_igmp_fwd           = cpphy_ar8327_set_igmp_fwd;
            cpphy->f->switch_port_power      = ar8316_switch_port_power;
#           if defined(POWERMANAGEMENT_THROTTLE_ETH)
            cpphy->f->switch_port_speed_throttle = ar8316_switch_port_speed_throttle;
#           endif /*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/
            cpphy->f->vlan_add               = ar8327_add_vlan_vid;
            cpphy->f->vlan_clear             = ar8327_vlan_clear;
            cpphy->mdio.switch_config.max_vid_groups       = 4096;
            cpphy->mdio.switch_config.mdio_upperhalf_first =  0;
            goto generic_atheros;
            break;
        case CPMAC_PHY_TYPE_AR8316:
            DEB_INFOTRC("[%s] Configure functions for AR8316\n", __FUNCTION__);
            cpphy->f->add_port_to_wan         = adm_add_port_to_wan;
            cpphy->f->add_trunk_port          = cpphy_switch_add_trunk_port;
            cpphy->f->check_link              = ar_check_link_orig;
            cpphy->f->config_ports            = ar_config_ports;
            cpphy->f->config_vlan_group       = ar_config_vlan_group;
            cpphy->f->dump_counters           = ar8316_dump_counters;
            cpphy->f->init_phy                = ar_ar8316_init;
            cpphy->f->deinit_phy              = ar_ar8316_deinit;
            cpphy->f->mgmt_check_link         = cpphy_mgmt_check_link_orig;
            cpphy->f->mirror_port             = cpphy_ar8316_mirror_port;
            cpphy->f->print_mac_table         = ar8316_print_table;
            cpphy->f->set_igmp_fwd            = cpphy_ar8216_set_igmp_fwd;
            cpphy->f->switch_port_power       = ar8316_switch_port_power;
#           if defined(POWERMANAGEMENT_THROTTLE_ETH)
            cpphy->f->switch_port_speed_throttle = ar8316_switch_port_speed_throttle;
#           endif /*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/
            cpphy->f->vlan_add                = ar8216_add_vlan_vid;
            cpphy->f->vlan_clear              = ar_vlan_clear;
            cpphy->mdio.switch_config.max_vid_groups       = 4096;
            cpphy->mdio.switch_config.mdio_upperhalf_first =  1;
            goto generic_atheros;
            break;
generic_atheros:
#           if defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_UR8)
            cpphy->f->check_external_tagging = cpphy_if_check_external_tagging;
#           endif /*--- #if defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_UR8) ---*/
            cpphy->f->ar_work_item            = ar8216_ar_work_item;
            cpphy->f->configure_special       = cpphy_switch_configure_special;
            cpphy->f->get_new_vid             = cpphy_ar_get_new_vid;
            cpphy->f->get_phy_port            = ar8216_get_phy_port;
            cpphy->f->get_switch_register     = ar8216_mdio_read32;
            cpphy->f->set_default_vid         = cpphy_ar_set_default_vid;
            cpphy->f->set_switch_register     = ar8216_mdio_write32;
            cpphy->f->switch_dump             = switch_dump_atheros;
            cpphy->mdio.switch_config.cpu_port             = 0;
            cpphy->mdio.switch_config.external_port_offset = 1;
            cpphy->mdio.switch_config.is_switch            = 1;
            cpphy->mdio.switch_config.ports                = 6;
            break;
        case CPMAC_PHY_TYPE_VITESSE:
            DEB_INFOTRC("[%s] Configure functions for VITESSE\n", __FUNCTION__);
#           if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)
            cpmac_device_name_init("eth");
#           endif /*--- #if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) ---*/
            cpphy->f->check_link             = cpphy_mdio_check_link_vitesse;
            cpphy->f->init_phy               = cpphy_mdio_init_vitesse;
            cpphy->f->deinit_phy             = 0;
            cpphy->f->mgmt_check_link        = cpphy_mgmt_check_link_orig;
            cpphy->f->switch_dump            = display_mdio_regs;
#           if defined(POWERMANAGEMENT_THROTTLE_ETH)
            cpphy->f->switch_port_speed_throttle = ar8316_switch_port_speed_throttle;
#           endif /*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/
            cpphy->mdio.switch_config.ports  = 1;
            cpphy->mdio.switch_config.external_port_offset = cpphy->mdio.inst;
            break;
        case CPMAC_PHY_TYPE_11G:
#           if defined(CONFIG_ARCH_PUMA5)
            cpmac_device_name_init("eth");
            cpphy->f->check_link             = cpphy_mdio_check_link_11G;
#           endif /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/
            cpphy->f->init_phy               = cpphy_mdio_init_11G;
            cpphy->f->deinit_phy             = 0;
            cpphy->f->switch_port_power      = ar8316_switch_port_power;
            cpphy->f->switch_dump            = display_mdio_regs;
            cpphy->f->mgmt_check_link        = cpphy_mgmt_check_link_orig;
#           if defined(POWERMANAGEMENT_THROTTLE_ETH)
            cpphy->f->switch_port_speed_throttle = ar8316_switch_port_speed_throttle;
#           endif /*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/
#           if defined(CONFIG_AR9)
            cpmac_global.instance[0].mii_mode[phy_number - 1] = AR9_MODE_RGMII;
#           endif /*--- #if defined(CONFIG_AR9) ---*/
            cpphy->mdio.switch_config.ports   = 1;
            break;
        case CPMAC_PHY_TYPE_ADM7001:
            DEB_INFOTRC("[%s] Configure functions for PHY\n", __FUNCTION__);
            cpphy->f->check_link      = cpphy_mdio_check_link_adm7001;
            cpphy->f->init_phy        = cpphy_mdio_init_adm7001;
            cpphy->f->deinit_phy      = 0;
            cpphy->f->mgmt_check_link = cpphy_mgmt_check_link_orig;
            cpphy->f->switch_dump     = display_mdio_regs;
            cpphy->mdio.switch_config.ports                = 1;
            cpphy->mdio.switch_config.external_port_offset = 0;
            break;
        case CPMAC_PHY_TYPE_INTERNAL:
            DEB_INFOTRC("[%s] Configure functions for PHY\n", __FUNCTION__);
            cpphy->f->init_phy        = cpphy_mdio_init_phy;
            cpphy->f->deinit_phy      = 0;
            cpphy->f->mgmt_check_link = cpphy_mgmt_check_link_phy;
            cpphy->f->switch_dump     = display_mdio_regs;
            cpphy->mdio.switch_config.ports                = 1;
            cpphy->mdio.switch_config.external_port_offset = 0;
            break;
        case CPMAC_PHY_TYPE_DUMMY:
            cpphy->f->mgmt_check_link = cpphy_mgmt_check_link_dummy;
            cpphy->mdio.switch_config.external_port_offset = 0;
            break;
        default:
            cpphy->f->mgmt_check_link = cpphy_mgmt_check_link_dummy;
            cpphy->mdio.switch_config.external_port_offset = 0;
            break;
    }
    switch(cpphy->config->config) {
        case CPMAC_PHY_MODE_7170:
            mdio->switch_predefined_configs = &(switch_configuration[CPMAC_SWITCH_CONF_7170][0]);
            break;
        case CPMAC_PHY_MODE_VINAX5:
            mdio->switch_predefined_configs = &(switch_configuration[CPMAC_SWITCH_CONF_VINAX5][0]);
            break;
        case CPMAC_PHY_MODE_VINAX7:
            mdio->switch_predefined_configs = &(switch_configuration[CPMAC_SWITCH_CONF_VINAX7][0]);
#           if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
            cpphy->f->set_mode_pppoa = adm_set_mode_pppoa;
#           endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/
            break;
        case CPMAC_PHY_MODE_PROFI2:
#           if defined(CONFIG_MIPS_UR8)
            {
                char *cpu_nr, *cmdline = prom_getcmdline();

                mdio->switch_predefined_configs = &(switch_configuration[CPMAC_SWITCH_CONF_PROFIVOIP2][0]);
                cpu_nr = strstr(cmdline, "CPU_NR=");
                if(cpu_nr == NULL) {
                    DEB_INFO("[%s] ProfiVoIP: running on first CPU\n", __FUNCTION__);
                    mdio->switch_predefined_configs = switch_configuration[CPMAC_SWITCH_CONF_PROFIVOIP2];
                } else {
                    DEB_INFO("[%s] ProfiVoIP: running on second CPU\n", __FUNCTION__);
                    mdio->Mode = 0; /* No access at all! */
                    mdio->linked = 1;
                }
            }
#           else /*--- #if defined(CONFIG_MIPS_UR8) ---*/
            DEB_ERR("[%s] No Profi mode possible!\n", __FUNCTION__);
#           endif /*--- #else ---*/ /*--- #if defined(CONFIG_MIPS_UR8) ---*/
            break;
        case CPMAC_PHY_MODE_AR8216:
            mdio->switch_predefined_configs = &(switch_configuration[CPMAC_SWITCH_CONF_AR8216][0]);
            break;
        case CPMAC_PHY_MODE_MAGPIE:
            mdio->switch_predefined_configs = &(switch_configuration[CPMAC_SWITCH_CONF_MAGPIE][0]);
            break;
        case CPMAC_PHY_MODE_PROFI1:
        case CPMAC_PHY_MODE_DEFAULT:
        case CPMAC_PHY_MODE_PASSIVE:
            return;
    }
    DEB_INFOTRC("[%s] Adjusting portsets to product specific portset\n", __FUNCTION__);
    for(mode = 0; mode < CPMAC_MODE_MAX_NO; mode++) {
        for(device = 0; device < mdio->switch_predefined_configs[mode].number_of_devices; device++) {
            unsigned short oldset = mdio->switch_predefined_configs[mode].device[device].target_mask;

            mdio->switch_predefined_configs[mode].device[device].target_mask &= mdio->switch_config.used_portset;
            if(oldset != mdio->switch_predefined_configs[mode].device[device].target_mask) {
                DEB_TRC("[%s] Adjusted mode %#x, device %u: portset %#x -> %#x\n", 
                        __FUNCTION__, 
                        mode, 
                        device, 
                        oldset,
                        mdio->switch_predefined_configs[mode].device[device].target_mask);
            }
        }
    }

}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpphy_entry_probe(void) {
    cpmac_product_config_struct *config = NULL;
    char *hwrev, buff[AVM_CPMAC_MAX_HWREV_LENGTH + 1], *p;
    unsigned char instance = 0;
    unsigned char phy;

    /* Print driver version */
    DEB_ERR("Version: %s\n", AVM_CPMAC_VERSION);

#   if (defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7))
    /* Changes for UR8 might have damaged the OHIO/AR7 code */
#   error "OHIO/AR7 not supported anymore!"
#   endif /*--- #if (defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7)) ---*/

    /* Initialize administrative array */
    memset(&cpmac_global, 0, sizeof(cpmac_global_t));
    for(instance = 0; instance < AVM_CPMAC_MAX_PHYS; instance++) {
        cpmac_global.cpphy[instance].f = kzalloc(sizeof(cpmac_funcs_t), GFP_KERNEL);
        if(cpmac_global.cpphy[instance].f == NULL) {
            DEB_ERR("[%s] Out of memory!\n", __FUNCTION__);
            return -ENOMEM;
        }
        cpmac_global.cpphy[instance].mdio.f = cpmac_global.cpphy[instance].f;
        cpmac_global.cpphy[instance].cppi.f = cpmac_global.cpphy[instance].f;
    }

    cpmac_global.products = &cpmac_products;

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    hwrev = prom_getenv("HWRevision");
    /*--- DEB_TRC("HWRevision=\"%s\"\n", hwrev); ---*/
    if(!hwrev) {
        return -ENODEV;
    }

    strncpy(buff, hwrev, 17);
    buff[AVM_CPMAC_MAX_HWREV_LENGTH - 1] = 0;
    assert(strlen(buff) < AVM_CPMAC_MAX_HWREV_LENGTH - 2); /* To ease Klocwork checking */
    strncat(buff, " ", 1);
    assert(strlen(buff) < AVM_CPMAC_MAX_HWREV_LENGTH); /* To ease Klocwork checking */

    /* These are not supported anymore! */
    if(   (!(strncmp( "94", hwrev, 2)) && (strlen(hwrev) < 4)) /* First 7170 revision */
       || (!(strncmp( "95", hwrev, 2)) && (strlen(hwrev) < 4)) /* First 7140 revision */
       || (!(strncmp("107", hwrev, 3)) && (strlen(hwrev) < 5)) /* First 7140 Annex A revision */
      ) {
        cpphy_entry_invalid_product_warning();
        return -ENODEV;
    }
    /*----------------------------------------------------------------------------------*\
     * aus "94.0.0.1 "  -->   "94 " erzeugen
    \*----------------------------------------------------------------------------------*/
    p = strchr(buff, '.');
    if(p) {
        *p++ = ' ';
        *p   = '\0';
    }

    cpmac_global.ports = 0;
    cpmac_global.phys = 0;
    for(cpmac_global.product_id = 0; cpmac_global.product_id < cpmac_global.products->number_of_products; cpmac_global.product_id++) {
        if(!strncmp(cpmac_global.products->product[cpmac_global.product_id].hwrev, buff, AVM_CPMAC_MAX_HWREV_LENGTH)) {
            config = &cpmac_global.products->product[cpmac_global.product_id];

            for(phy = 0; phy < config->phys; phy++) {
                if(config->phy[phy].type == CPMAC_PHY_TYPE_NONE) {
                    DEB_INFO("[%s] PHY %u: none\n", __FUNCTION__, phy);
                } else {
                    unsigned char port, ports = 0;
                    unsigned char port_modes_possible = 0;
                    enum _avm_event_ethernet_speed max_port_speed = avm_event_ethernet_speed_no_link;

                    cpmac_global.phys++;
                    assert(cpmac_global.phys <= AVM_CPMAC_MAX_PHYS);
                    switch(config->phy[phy].type) {
                        case CPMAC_PHY_TYPE_ADM6996FC:
                        case CPMAC_PHY_TYPE_TANTOS:
                            port_modes_possible =   (1 << ADM_PHY_POWER_OFF)
                                                  | (1 << ADM_PHY_POWER_ON)
                                                  | (1 << ADM_PHY_POWER_SAVE);
                            ports = 4;
                            max_port_speed = avm_event_ethernet_speed_100M;
                            break;
                        case CPMAC_PHY_TYPE_AR8226:
                            port_modes_possible = (1 << ADM_PHY_POWER_ON);
                            ports = 4;
                            max_port_speed = avm_event_ethernet_speed_100M;
                            break;
                        case CPMAC_PHY_TYPE_AR8316:
                        case CPMAC_PHY_TYPE_AR8327:
                            port_modes_possible = (1 << ADM_PHY_POWER_ON);
                            ports = 4;
                            max_port_speed = avm_event_ethernet_speed_1G;
                            break;
                        case CPMAC_PHY_TYPE_AR8216:
                            if(   (!(strncmp( "144", hwrev, 3)) && (strlen(hwrev) < 4))    /* 7240 with 8226 */
                               || (!(strncmp( "145", hwrev, 3)) && (strlen(hwrev) < 4))) { /* 7270v3 with 8226 */
                                char *subrevstr = prom_getenv("HWSubRevision");
                                unsigned int subrev;
                                if(subrevstr != NULL) {
                                    sscanf(subrevstr, "%u", &subrev);
                                    if(subrev > 1) {
                                        DEB_INFO("[%s] AR8226 version\n", __FUNCTION__);
                                        config->phy[phy].type = CPMAC_PHY_TYPE_AR8226;
                                    }
                                }
                            }
                            port_modes_possible = 1 << ADM_PHY_POWER_ON;
                            ports = 4;
                            max_port_speed = avm_event_ethernet_speed_100M;
                            break;
                        case CPMAC_PHY_TYPE_ADM7001:
                            ports = 1;
                            port_modes_possible =   (1 << ADM_PHY_POWER_OFF)
                                                  | (1 << ADM_PHY_POWER_ON);
                            max_port_speed = avm_event_ethernet_speed_100M;
                            break;
                        case CPMAC_PHY_TYPE_VITESSE:
                            ports = 1;
                            port_modes_possible =   (1 << ADM_PHY_POWER_OFF)
                                                  | (1 << ADM_PHY_POWER_ON);
                            max_port_speed = avm_event_ethernet_speed_1G;
                            break;
                        case CPMAC_PHY_TYPE_11G:
                            ports = 1;
                            port_modes_possible = (1 << ADM_PHY_POWER_ON);
                            max_port_speed = avm_event_ethernet_speed_1G;
                            break;
                        case CPMAC_PHY_TYPE_NONE:
                            port_modes_possible = 1 << ADM_PHY_POWER_OFF;
                        default:
                            break;
                    }
                    for(port = 0; port < ports; port++) {
                        cpmac_global.map_port_to_instance[cpmac_global.ports] = phy;
                        cpmac_global.event_data.port[cpmac_global.ports].maxspeed = max_port_speed;
                        /* Check, whether the port should be used */
                        if((1 << port) & config->phy[phy].ports) {
                            cpmac_global.power.port_modes_possible[cpmac_global.ports] = port_modes_possible;
                        } else {
                            cpmac_global.power.port_modes_possible[cpmac_global.ports] = (1 << ADM_PHY_POWER_OFF);
                        }
                        DEB_INFOTRC("[%s] Mapping port %u to instance %u\n", __FUNCTION__, cpmac_global.ports, phy);
                        cpmac_global.ports++;
                    }
                    DEB_INFO("[%s] PHY %u: %s, reset %u, mode %s, ports %#x, mii_port %#x, port modes %s%s%s\n", __FUNCTION__,
                             phy,
                             (config->phy[phy].type == CPMAC_PHY_TYPE_ADM6996FC) ? "ADM6996LC/FC switch" :
                             (config->phy[phy].type == CPMAC_PHY_TYPE_TANTOS)    ? "Tantos switch" :
                             (config->phy[phy].type == CPMAC_PHY_TYPE_AR8216)    ? "AR8216 switch" :
                             (config->phy[phy].type == CPMAC_PHY_TYPE_AR8226)    ? "AR8226 switch" :
                             (config->phy[phy].type == CPMAC_PHY_TYPE_AR8316)    ? "AR8316 switch" :
                             (config->phy[phy].type == CPMAC_PHY_TYPE_AR8327)    ? "AR8327 switch" :
                             (config->phy[phy].type == CPMAC_PHY_TYPE_ADM7001)   ? "ADM7001 PHY" :
                             (config->phy[phy].type == CPMAC_PHY_TYPE_VITESSE)   ? "VITESSE PHY" :
                             (config->phy[phy].type == CPMAC_PHY_TYPE_11G)       ? "Lantiq PHY 11G" :
                             (config->phy[phy].type == CPMAC_PHY_TYPE_DUMMY)     ? "dummy" 
                                                                                 : "unknown",
                             (config->phy[phy].reset_pin == 0xffff) ? 9999 : config->phy[phy].reset_pin,
                             (config->phy[phy].config == CPMAC_PHY_MODE_DEFAULT) ? "default" :
                             (config->phy[phy].config == CPMAC_PHY_MODE_PASSIVE) ? "passive" :
                             (config->phy[phy].config == CPMAC_PHY_MODE_7170)    ? "7170" :
                             (config->phy[phy].config == CPMAC_PHY_MODE_VINAX5)  ? "VINAX5" :
                             (config->phy[phy].config == CPMAC_PHY_MODE_VINAX7)  ? "VINAX7" :
                             (config->phy[phy].config == CPMAC_PHY_MODE_PROFI1)  ? "PROFI1" :
                             (config->phy[phy].config == CPMAC_PHY_MODE_PROFI2)  ? "PROFI2" :
                             (config->phy[phy].config == CPMAC_PHY_MODE_AR8216)  ? "AR8216" :
                             (config->phy[phy].config == CPMAC_PHY_MODE_MAGPIE)  ? "MAGPIE" : "unknown",
                             config->phy[phy].ports,
                             config->phy[phy].mii_portmask,
                             (port_modes_possible & (1 << ADM_PHY_POWER_ON)) ? "ON " : "",
                             (port_modes_possible & (1 << ADM_PHY_POWER_SAVE)) ? "SAVE " : "",
                             (port_modes_possible & (1 << ADM_PHY_POWER_OFF)) ? "OFF " : ""
                            );
                }
            }
            DEB_INFO("[%s] HWRev %s: This hardware has %u PHY%s (with %u external ports):\n", 
                     __FUNCTION__, hwrev, config->phys, (config->phys > 1) ? "s" : "", cpmac_global.ports);
            break; /* Product found */
        }
    }
    if(config == NULL) {
        cpphy_entry_invalid_product_warning();
        return -ENODEV;
    }
#   if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)
    /* For 7340 switch the port/instance assignments */
    if(!(strncmp( "171 ", hwrev, 3))) {
        cpmac_global.map_port_to_instance[0] = 1;
        cpmac_global.map_port_to_instance[1] = 0;
    }
#   endif /*--- #if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) ---*/

    instance = 0;
    for(phy = 0; phy < config->phys; phy++) {
        if(config->phy[phy].type == CPMAC_PHY_TYPE_NONE) {
            continue;
        }
        cpmac_global.cpphy[instance].is_used = 1;
        cpmac_global.cpphy[instance].instance = instance;
        /* Check, whether we need EMV handling for the W503V */
        if(!(strncmp( "136 ", hwrev, 3))) {
            cpmac_global.cpphy[instance].mdio.switch_config.use_EMV = cpphy_entry_check_for_emv() ? 1 : 0;
        }
        cpmac_global.cpphy[instance].config = &config->phy[phy];
        cpphy_entry_set_config(&cpmac_global.cpphy[instance], phy);
        cpmac_global.cpphy[instance].mdio.high_phy = (phy > 0) ? 1 : 0;
        cpmac_global.cpphy[instance].mdio.inst = instance;
        instance++;
    }
    cpmac_device_name_init("cpmac");

    cpmac_global.workqueue = create_singlethread_workqueue("CPMAC workqueue");

    return 0;
}
arch_initcall(cpphy_entry_probe);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int cpphy_entry_device_init(void) {
#   if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
    unsigned char instance;
    /* Register all instances */
    for(instance = 0; instance < AVM_CPMAC_MAX_PHYS; instance++) {
        if(cpmac_global.cpphy[instance].is_used) {
            if(CPMAC_ERR_NOERR != cpphy_main_register(&cpmac_global.cpphy[instance])) {
                DEB_ERR("[%s] Could not register instance %u!\n", __FUNCTION__, instance);
                return -ENODEV;
            }
        }
    }
    /* Continuation via callback in cpphy_if_init() */
#   endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/
    cpmac_main_probe();
    return 0;
}
device_initcall(cpphy_entry_device_init);


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
static void cpphy_entry_exit(void) {
    unsigned char instance;

    DEB_INFO("cpphy_entry_exit, init\n");
    for(instance = 0; instance < AVM_CPMAC_MAX_PHYS; instance++) {
        cpmac_if_release(cpmac_global.cpphy[instance].cpmac_priv);
    }
    flush_workqueue(cpmac_global.workqueue);
    destroy_workqueue(cpmac_global.workqueue);
}
module_exit(cpphy_entry_exit);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int cpphy_entry_prepare_reboot(struct notifier_block *self __attribute__ ((unused)),
                               unsigned long          kind __attribute__ ((unused)),
                               void                  *cmd __attribute__ ((unused))) {
    unsigned char instance;

    /* We are neither interested in the kind of powerdown nor the command */

    for(instance = 0; instance < AVM_CPMAC_MAX_PHYS; instance++) {
        if(cpmac_global.cpphy[instance].f->prepare_reboot) {
            DEB_INFO("System is going down. Resetting VLAN.\n");
            cpmac_global.cpphy[instance].f->prepare_reboot(&cpmac_global.cpphy[instance].mdio);
        }
    }

    return NOTIFY_DONE;
}

