/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2008,2009,...,2012 AVM GmbH <fritzbox_info@avm.de>
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

#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <asm/atomic.h>
#include <asm/mach_avm.h>
#if defined(CONFIG_AVM_POWER)
#include <linux/avm_power.h>
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/

#if !defined(CONFIG_NETCHIP_AR8216)
#define CONFIG_NETCHIP_AR8216
#endif
#if defined(CONFIG_MIPS_UR8)
#include <asm/mach-ur8/hw_mdio.h>
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/
#include <linux/avm_event.h>
#include <linux/avm_cpmac.h>
#include <linux/ar_reg.h>
#include <linux/adm_reg.h>

#include "cpmac_if.h"
#include "cpmac_const.h"
#include "cpmac_debug.h"
#include "cpphy_types.h"
#include "cpphy_mdio.h"
#include "cpphy_if.h"
#include "cpphy_if_g.h"
#include "cpphy_adm6996.h"
#include "cpphy_ar8216.h"
#include "cpphy_mgmt.h"
#include "cpmac_fusiv_if.h"
#include "cpmac_eth.h"


/*------------------------------------------------------------------------------------------*\
 * Switch configuration for different products                                              *
 *                                                                                          *
 * Ensure that device entries overlap only at the CPU port!                                 *
\*------------------------------------------------------------------------------------------*/
cpmac_switch_configuration_t switch_configuration[CPMAC_SWITCH_CONF_MAX_PRODUCTS][CPMAC_MODE_MAX_NO] = {
    { /* 7170 */
        /* No legal mode        */ { 0, 0xff,
                                        { {"", 0x0}
                                        }
                                   },
        /* CPMAC_MODE_NORMAL    */ { 1, 0xff,
                                        { {"eth0", 0x2f}
                                        }
                                   },
        /* CPMAC_MODE_ATA       */ { 2, 0,
                                        { {"wan",  0x21},
                                          {"eth0", 0x2e},
                                        }
                                   },
        /* CPMAC_MODE_SPLIT     */ { 4, 0xff,
                                        { {"eth0", 0x21},
                                          {"eth1", 0x22},
                                          {"eth2", 0x24},
                                          {"eth3", 0x28}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT_ATA */ { 4, 0,
                                        { {"wan",  0x21},
                                          {"eth1", 0x22},
                                          {"eth2", 0x24},
                                          {"eth3", 0x28},
                                        }
                                   },
        /* CPMAC_MODE_ALL_PORTS */ { 1, 0xff,
                                        { {"eth0", 0x3f}
                                        }
                                   }
    }, { /* VINAX5 */
        /* No legal mode        */ { 0, 0xff,
                                        { {"", 0x0}
                                        }
                                   },
        /* CPMAC_MODE_NORMAL    */ { 2, 4,
                                        { {"wan",  0x30},
                                          {"eth0", 0x2f}
                                        }
                                   },
        /* CPMAC_MODE_ATA       */ { 2, 0,
                                        { {"wan",  0x21},
                                          {"eth0", 0x2e}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT     */ { 5, 4,
                                        { {"wan",  0x30},
                                          {"eth0", 0x21},
                                          {"eth1", 0x22},
                                          {"eth2", 0x24},
                                          {"eth3", 0x28}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT_ATA */ { 4, 0,
                                        { {"wan",  0x21},
                                          {"eth1", 0x22},
                                          {"eth2", 0x24},
                                          {"eth3", 0x28}
                                        }
                                   },
        /* CPMAC_MODE_ALL_PORTS */ { 1, 0xff,
                                        { {"eth0", 0x3f}
                                        }
                                   }
    }, { /* VINAX7 */
        /* No legal mode        */ { 0, 0xff,
                                        { {"", 0x0}
                                        }
                                   },
        /* CPMAC_MODE_NORMAL    */ { 2, 6,
                                        { {"wan",  0x60},
                                          {"eth0", 0x2f}
                                        }
                                   },
        /* CPMAC_MODE_ATA       */ { 2, 0,
                                        { {"wan",  0x21},
                                          {"eth0", 0x2e}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT     */ { 5, 6,
                                        { {"wan",  0x60},
                                          {"eth0", 0x21},
                                          {"eth1", 0x22},
                                          {"eth2", 0x24},
                                          {"eth3", 0x28}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT_ATA */ { 4, 0,
                                        { {"wan",  0x21},
                                          {"eth1", 0x22},
                                          {"eth2", 0x24},
                                          {"eth3", 0x28}
                                        }
                                   },
        /* CPMAC_MODE_ALL_PORTS */ { 1, 0xff,
                                        { {"eth0", 0x6f}
                                        }
                                   }
    }, { /* ProfiVoIP 2 CPUs */
        /* No legal mode        */ { 0, 0xff,
                                        { {"", 0x0}
                                        }
                                   },
        /* CPMAC_MODE_NORMAL    */ { 2, 0xff,
                                        { {"eth0", 0x2f},
                                          {"cpu",  0x30}
                                        }
                                   },
        /* CPMAC_MODE_ATA       */ { 3, 0,
                                        { {"wan",  0x21},
                                          {"eth0", 0x2e},
                                          {"cpu",  0x30}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT     */ { 5, 0xff,
                                        { {"eth0", 0x21},
                                          {"eth1", 0x22},
                                          {"eth2", 0x24},
                                          {"eth3", 0x28},
                                          {"cpu",  0x30}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT_ATA */ { 5, 0,
                                        { {"wan",  0x21},
                                          {"eth1", 0x22},
                                          {"eth2", 0x24},
                                          {"eth3", 0x28},
                                          {"cpu",  0x30}
                                        }
                                   },
        /* CPMAC_MODE_ALL_PORTS */ { 1, 0xff,
                                        { {"eth0", 0x3f}
                                        }
                                   }
    }, { /* ProfiVoIP 1 CPU */
        /* No legal mode        */ { 0, 0xff,
                                        { {"", 0x0}
                                        }
                                   },
        /* CPMAC_MODE_NORMAL    */ { 1, 0xff,
                                        { {"eth0", 0x1f}
                                        }
                                   },
        /* CPMAC_MODE_ATA       */ { 2, 0,
                                        { {"wan",  0x11},
                                          {"eth0", 0x1e}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT     */ { 4, 0xff,
                                        { {"eth0", 0x11},
                                          {"eth1", 0x12},
                                          {"eth2", 0x14},
                                          {"eth3", 0x18}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT_ATA */ { 4, 0,
                                        { {"wan",  0x11},
                                          {"eth1", 0x12},
                                          {"eth2", 0x14},
                                          {"eth3", 0x18}
                                        }
                                   },
        /* CPMAC_MODE_ALL_PORTS */ { 1, 0xff,
                                        { {"eth0", 0x3f}
                                        }
                                   }
    }, { /* AR8216 */
        /* No legal mode        */ { 0, 0xff,
                                        { {"", 0x0}
                                        }
                                   },
        /* CPMAC_MODE_NORMAL    */ { 1, 0xff,
                                        { {"eth0", 0x1f}
                                        }
                                   },
        /* CPMAC_MODE_ATA       */ { 2, 1,
                                        { {"wan",  0x03},
                                          {"eth0", 0x1d},
                                        }
                                   },
        /* CPMAC_MODE_SPLIT     */ { 4, 0xff,
                                        { {"eth0", 0x03},
                                          {"eth1", 0x05},
                                          {"eth2", 0x09},
                                          {"eth3", 0x11}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT_ATA */ { 4, 1,
                                        { {"wan",  0x03},
                                          {"eth1", 0x05},
                                          {"eth2", 0x09},
                                          {"eth3", 0x11},
                                        }
                                   },
        /* CPMAC_MODE_ALL_PORTS */ { 1, 0xff,
                                        { {"eth0", 0x3f}
                                        }
                                   }
    }, { /* MAGPIE */
        /* No legal mode        */ { 0, 0xff,
                                        { {"", 0x0}
                                        }
                                   },
        /* CPMAC_MODE_NORMAL    */ { 2, 0xff,
                                        { {"magpie", 0x21},
                                          {"eth0",   0x1f}
                                        }
                                   },
        /* CPMAC_MODE_ATA       */ { 3, 1,
                                        { {"wan",    0x03},
                                          {"magpie", 0x21},
                                          {"eth0",   0x1d}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT     */ { 5, 0xff,
                                        { {"magpie", 0x21},
                                          {"eth0",   0x03},
                                          {"eth1",   0x05},
                                          {"eth2",   0x09},
                                          {"eth3",   0x11}
                                        }
                                   },
        /* CPMAC_MODE_SPLIT_ATA */ { 5, 1,
                                        { {"wan",    0x03},
                                          {"magpie", 0x21},
                                          {"eth1",   0x05},
                                          {"eth2",   0x09},
                                          {"eth3",   0x11}
                                        }
                                   },
        /* CPMAC_MODE_ALL_PORTS */ { 1, 0xff,
                                        { {"eth0", 0x3f}
                                        }
                                   }
    }
};

#if (CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_SUPPORT)
static char *switch_mode_names[CPMAC_MODE_MAX_NO] = { "none",
                                                      "NORMAL",
                                                      "ATA",
                                                      "SPLIT",
                                                      "SPLIT_ATA",
                                                      "ALL_PORTS",
                                                      "SPECIAL"
};

#endif /*--- #if (CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_SUPPORT) ---*/

#if (CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_INFOTRACE)
static char *switch_config_names[AVM_CPMAC_IOCTL_NUMBER_OF_IOCTLS] = {
                                      "",
                                      "CONFIG_GET_INFO",
                                      "CONFIG_SET_SWITCH_MODE",
                                      "CONFIG_GET_SWITCH_MODE",
                                      "CONFIG_SET_SWITCH_MODE_SPECIAL",
                                      "CONFIG_GET_SWITCH_MODE_CURRENT",
                                      "CONFIG_SET_SWITCH_REGISTER",
                                      "CONFIG_GET_SWITCH_REGISTER",
                                      "CONFIG_SET_PHY_POWER",
                                      "CONFIG_GET_PHY_POWER",
                                      "CONFIG_GET_SWITCH_REGISTER_DUMP",
                                      "CONFIG_TESTCMD",
                                      "CONFIG_GET_WAN_KEEPTAG",
                                      "CONFIG_GET_PORT_OUT_VID_MAP",
                                      "CONFIG_SET_PHY_REGISTER",
                                      "CONFIG_GET_PHY_REGISTER",
                                      "CONFIG_GET_BYTES_IN_WAN",
                                      "CONFIG_SET_PPPOA",
                                      "SUPPORT_DATA",
                                      "CONFIG_ADD_PORT_TO_WAN",
                                      "CONFIG_PEEK",
                                      "CONFIG_POKE",
                                      "CONFIG_SET_WAN_DUP",
                                      "CONFIG_MIRROR_PORT",
                                      "CONFIG_SET_IGMP_FWD",
                                      "CONFIG_ADD_TRUNK_PORT"
};
#endif /*--- #if (CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_INFOTRACE) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int cpphy_switch_assign_vid(cpphy_mdio_t *mdio, unsigned short vid) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned int i;

    if(switch_config->assigned_vids_count == CPMAC_MAX_ASSIGNED_VIDS) {
        DEB_ERR("[%s] CPMAC_MAX_ASSIGNED_VIDS reached!\n", __FUNCTION__);
        return 1;
    }
    for(i = 0; i < switch_config->assigned_vids_count; i++) {
        if(switch_config->assigned_vids[i] == vid) {
            DEB_ERR("[%s] VID %#x already assigned!\n", __FUNCTION__, vid);
            return 2;
        }
    }
    switch_config->assigned_vids[switch_config->assigned_vids_count] = vid;
    switch_config->assigned_vids_count++;

    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_switch_check_assigned_vid(cpphy_mdio_t *mdio, unsigned short vid) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned int i;

    for(i = 0; i < switch_config->assigned_vids_count; i++) {
        if(switch_config->assigned_vids[i] == vid) {
            /*--- DEB_TRC("[%s] VID %#x is assigned.\n", __FUNCTION__, vid); ---*/
            return 1;
        }
    }
    return 0;
}


/*------------------------------------------------------------------------------------------*\
 * Configure VLAN for one device
 *
 * Bitshift must be set before the first call to this function!
\*------------------------------------------------------------------------------------------*/
static void cpphy_switch_set_device(cpphy_mdio_t       *mdio,
                                    unsigned char       device_no,
                                    cpmac_device_new_t *device) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid, port, ps, external_ps;
    cpmac_vid_range_t *vid_range = NULL;

    if(device->name == NULL) {
        DEB_ERR("[%s] Error! Empty device name!\n", __FUNCTION__);
        return;
    }

    DEB_INFO("[%s] Configure cpmac device %u '%s' with target_mask 0x%x\n", __FUNCTION__, device_no, device->name, device->target_mask);

    strncpy(switch_config->device[device_no].name, device->name, AVM_CPMAC_MAX_DEVICE_NAME_LENGTH);
    switch_config->device[device_no].portset = device->target_mask;

    /* Define the FID for switches that can manage separate MAC tables, e.g. the Tantos */
    switch_config->device[device_no].FID = (strncmp(device->name, "wan", 3) == 0) ? 1 : 0;

    /* Map all ports of the port map to this device */
    for(port = 0; port < AVM_CPMAC_MAX_PORTS; port++) {
        if(port == switch_config->cpu_port) {
            /* There should be no incoming packets from the CPU port. *\
            \* Just use device 0 for that "case" as a sane default.   */
            switch_config->map_port_to_dev[port] = 0;
            continue;
        }
        if(device->target_mask & (1 << port)) {
            switch_config->map_port_to_dev[port] = device_no;
        }
    }

    switch_config->device[device_no].adm_used_bit_combinations = 0; /* Reset info */

    /* Do we have predefined VLAN IDs? */
    vid_range = device->vid_range;
    while(vid_range != NULL) {
        if(strncmp(device->name, "wan", 3) == 0) {
            switch_config->wanport_keeptags = 1;
        }
        for(vid = vid_range->start; vid <= vid_range->end; vid++) {
            /* Keep tags for the predefined VLANs */
            for(port = 0; port < AVM_CPMAC_MAX_PORTS; port++) {
                if(   (port != switch_config->cpu_port) 
                   && ((1 << port) & device->target_mask)) {
                    switch_config->port[port].keep_tag_outgoing = 1;
                }
            }
            port = 0;
            /* Find the first (perhaps only) non CPU port in the portset */
            while(   (port == switch_config->cpu_port) 
                  || (((1 << port) & device->target_mask) == 0)) {
                port++;
            }
            assert(port < AVM_CPMAC_MAX_PORTS);

            DEB_INFO("[%s] Using predefined VID %#x (%u) for device %s\n", __FUNCTION__, vid, vid, device->name);
            /* Behaviour of the ADM6996 family:                    *\
             *   - lower two bits are used for MAC table entries   *
            \*   - four bits of the VID are used for the VID group */
            switch_config->device[device_no].adm_used_bit_combinations |= 1 << (vid & 0x3);
            switch_config->adm_used_bit_combinations |= 1 << (vid & 0x3);
            if(mdio->f->config_vlan_group) {
                mdio->f->config_vlan_group(mdio, vid, device->target_mask, device->target_mask, port, switch_config->device[device_no].FID, 1);
            }
        }
        vid_range = vid_range->next;
    }

    /* Default VID for the device */
    switch_config->device[device_no].vid = 0;
    if(mdio->f->set_default_vid) {
        mdio->f->set_default_vid(mdio, device_no, device->target_mask);
    }

    /* Setup portset -> VID mappings for MC addressing */
    DEB_INFOTRC("[%s] Setup portset -> VID mappings for MC addressing\n", __FUNCTION__);
    external_ps = device->target_mask & (0xf << switch_config->external_port_offset);
    for(ps = 1; ps <= AVM_CPMAC_MAX_PORTSET; ps++) {
        if(ps != (ps & external_ps)) { /* ps is no subset of portset */
            continue;
        }
        /* Prevent making all possible portsets for multicast forwarding, because
         * there are not enough VLAN groups available in all cases! */
        if(switch_config->max_vid_groups < 32) {
            unsigned int okay = 0, ps_test = 1;
            while(ps_test & AVM_CPMAC_MAX_PORTSET) {
                if(ps_test == ps) {
                    okay = 1;
                }
                ps_test <<= 1;
            }
            if(okay == 0) {
                continue;
            }
        }

        /* If the default vid is set, it is set for this port set */
        if(   (ps == external_ps)
           && (switch_config->device[device_no].vid != 0)) {
            switch_config->map_portset_out[ps] = switch_config->device[device_no].vid;
            continue;
        }
        assert(mdio->f->get_new_vid);
        vid = mdio->f->get_new_vid(mdio, device_no);
        if(vid == 0) {
            DEB_ERR("[%s] Error! No unassigned VID (for MC out) available!\n", __FUNCTION__);
            return;
        }
        assert(mdio->f->config_vlan_group);
        mdio->f->config_vlan_group(mdio, 
                                   vid,
                                   ps,
                                   ps & (1 << switch_config->cpu_port),
                                   switch_config->vidgroup[switch_config->map_vid_to_group[switch_config->device[device_no].vid]].port_in,
                                   switch_config->device[device_no].FID,
                                   0);
        switch_config->map_portset_out[ps] = vid;
    }
    for(ps = 1; ps <= AVM_CPMAC_MAX_PORTSET; ps++) {
        if(switch_config->map_portset_out[ps] != 0) {
            DEB_INFOTRC("[%s] Portset %#x -> VID %#x\n", __FUNCTION__, ps, switch_config->map_portset_out[ps]);
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static cpmac_vid_range_t *new_vid_range(unsigned short start, unsigned short end) {
    cpmac_vid_range_t *vid_range = kmalloc(sizeof(cpmac_vid_range_t), GFP_KERNEL);
    if(vid_range != NULL) {
        vid_range->start = start;
        vid_range->end   = end;
        vid_range->next  = NULL;
    }
    return vid_range;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static cpmac_err_t cpphy_switch_real_configure(cpphy_mdio_t *mdio,
                                               struct avm_cpmac_config_struct *config);
static cpmac_err_t cpphy_switch_configure(cpphy_mdio_t *mdio, struct avm_cpmac_config_struct *config) {
    unsigned char device;
    unsigned int i;
    t_cpphy_switch_config *switch_config = &(mdio->switch_config);
    cpmac_device_configuration_t *device_config = &(switch_config->device_config);

    /* Assert some given things to ease Klocworks checking */
    assert(switch_config->cpu_port < AVM_CPMAC_MAX_PORTS);
    assert(mdio->switch_predefined_configs[config->cpmac_mode].number_of_devices);
    assert(mdio->switch_config.devices < AVM_CPMAC_MAX_DEVS);

    /* How to configure the VLAN */
    /* 1. Check the given configuration for obvious errors
     * 2. Reset the current configuration
     *    a) Map all VIDs to port 0
     *    b) Map all ports to device 0
     * 3. If a wan port exists, check the VIDs 
     */

    /* Is the switch in a normal power mode? */
    if(cpmac_global.global_power != CPPHY_POWER_GLOBAL_NONE) {
        DEB_WARN("[%s] Global power mode set. Aborting configuration.\n", __FUNCTION__);
        return CPMAC_ERR_CHAN_NOT_OPEN;
    }
    /* Is the switch configuration writable? */
    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE)) {
        DEB_ERR("[%s] Illegal configuration for this hardware!\n", __FUNCTION__);
        return CPMAC_ERR_NO_VLAN_POSSIBLE;
    }

    /* Check plausibility */
    if(config->cpmac_mode >= CPMAC_MODE_MAX_NO) {
        DEB_ERR("[%s] Unknown cpmac_configuration %u\n", __FUNCTION__, config->cpmac_mode);
        return CPMAC_ERR_EXCEEDS_LIMIT;
    }
    DEB_INFOTRC("[%s] Cpmac_configuration %u\n", __FUNCTION__, config->cpmac_mode);

    /* Is the number of devices small enough? */
    if(mdio->switch_predefined_configs[config->cpmac_mode].number_of_devices > AVM_CPMAC_MAX_DEVS) {
        DEB_ERR("[%s] Too many devices in configuration!\n", __FUNCTION__);
        return CPMAC_ERR_EXCEEDS_LIMIT;
    }

    /* Check, whether device ports overlap except for the CPU port */
    for(device = 0; device < mdio->switch_predefined_configs[config->cpmac_mode].number_of_devices; device++) {
        unsigned int device2;
        unsigned int mask = mdio->switch_predefined_configs[config->cpmac_mode].device[device].target_mask;

        if(!((1 << switch_config->cpu_port) & mask)) {
            DEB_WARN("[%s] Device '%s' (%u) does not include the CPU port! mask = %#x\n", __FUNCTION__,
                     mdio->switch_predefined_configs[config->cpmac_mode].device[device].name,
                     device,
                     mdio->switch_predefined_configs[config->cpmac_mode].device[device].target_mask);
        }
        mask &= ~(1 << switch_config->cpu_port); /* Ignore CPU port for overlapping test */
        for(device2 = device + 1; device2 < mdio->switch_predefined_configs[config->cpmac_mode].number_of_devices; device2++) {
            if(mdio->switch_predefined_configs[config->cpmac_mode].device[device2].target_mask & mask) {
                DEB_ERR("[%s] Attention! The masks for '%s' (%#x) and '%s' (%#x) overlap!\n", __FUNCTION__,
                        mdio->switch_predefined_configs[config->cpmac_mode].device[device].name,
                        mdio->switch_predefined_configs[config->cpmac_mode].device[device].target_mask,
                        mdio->switch_predefined_configs[config->cpmac_mode].device[device2].name,
                        mdio->switch_predefined_configs[config->cpmac_mode].device[device2].target_mask);
                return CPMAC_ERR_MISC;
            }
        }
    }

    /* Check the given VID, if any */
    for(i = 0; i < config->wan_vlan_number; i++) {
        if(config->wan_vlan_id[i] >= 4096) {
            DEB_ERR("[%s] Illegal VID %#x for WAN given!!\n", __FUNCTION__, config->wan_vlan_id[i]);
            return CPMAC_ERR_MISC;
        }
    }

    /* Remember the user given configuration */
    memcpy(&switch_config->user_given_config, config, sizeof(struct avm_cpmac_config_struct));

    DEB_INFO("[%s] Configure cpmac to mode %u with %u devices:\n", __FUNCTION__,
             config->cpmac_mode, mdio->switch_predefined_configs[config->cpmac_mode].number_of_devices);

    /* Convert old config to new configuration */
    device_config->number_of_devices = mdio->switch_predefined_configs[config->cpmac_mode].number_of_devices;
    device_config->wanport = mdio->switch_predefined_configs[config->cpmac_mode].wanport;
    for(device = 0; device < mdio->switch_predefined_configs[config->cpmac_mode].number_of_devices; device++) {
        cpmac_vid_range_t *vid_range;
        strncpy(device_config->device[device].name,
                mdio->switch_predefined_configs[config->cpmac_mode].device[device].name,
                AVM_CPMAC_MAX_DEVICE_NAME_LENGTH);
        device_config->device[device].target_mask = mdio->switch_predefined_configs[config->cpmac_mode].device[device].target_mask;

        /* Erase old VID range(s) */
        vid_range = device_config->device[device].vid_range;
        while(vid_range != NULL) {
            cpmac_vid_range_t *old_range = vid_range;
            vid_range = vid_range->next;
            kfree(old_range);
        }
        device_config->device[device].vid_range = NULL;

        if(   (device_config->wanport != 0xff)
           && (device_config->device[device].target_mask & (1 << device_config->wanport))) {
            for(i = 0; i < config->wan_vlan_number; i++) {
                vid_range = new_vid_range(config->wan_vlan_id[i], config->wan_vlan_id[i]);
                if(vid_range == NULL) {
                    DEB_ERR("[%s] No memory for vid_range!\n", __FUNCTION__);
                    return CPMAC_ERR_NOMEM;
                }
                vid_range->next = device_config->device[device].vid_range;
                device_config->device[device].vid_range = vid_range;
            }
        }
    }
    return cpphy_switch_real_configure(mdio, config);
}


static cpmac_err_t cpphy_switch_real_configure(cpphy_mdio_t *mdio,
                                               struct avm_cpmac_config_struct *config) {
    unsigned char device, port;
    unsigned short ps, vid_group, vid_group_count;
    unsigned int i;
    t_cpphy_switch_config *switch_config = &(mdio->switch_config);
    cpmac_device_configuration_t *dconfig = &(mdio->switch_config.device_config);

    /* Check all vid_ranges */
    for(device = 0; device < dconfig->number_of_devices; device++) {
        cpmac_vid_range_t *vid_range = dconfig->device[device].vid_range;
        while(vid_range != NULL) {
            if(   (vid_range->start > vid_range->end)
               || (vid_range->end >= 4096)) {
                DEB_ERR("[%s] Illegal vid_range: start %u (%#x), end %u (%#x)\n",
                        __func__,
                        vid_range->start, vid_range->start,
                        vid_range->end, vid_range->end);
                return CPMAC_ERR_MISC;
            }
            vid_range = vid_range->next;
        }
    }


    /* Make sure, that nothing works in parallel */
    cpphy_mgmt_work_stop(mdio);
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8)
    tasklet_disable(&mdio->cpmac_priv->tasklet);
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) ---*/
    if(mdio->f->cpmac_transfer_startstop) {
        mdio->f->cpmac_transfer_startstop(0);
    }

    /* Backup device entries */
    for(i = 0; i < AVM_CPMAC_MAX_DEVS; i++) {
        switch_config->olddevs[i] = switch_config->device[i].net_device;
        switch_config->device[i].net_device = 0;
    }

    /* RESET */ 
    if(mdio->f->vlan_clear) {
        mdio->f->vlan_clear(mdio);
    }

    /* Reset mappings */
    for(i = 0; i < 4096; i++) {
        /*--- switch_config->map_vid_to_port[i] = switch_config->cpu_port; ---*/
        switch_config->map_vid_to_group[i] = 4096;
        switch_config->vidgroup[i].used = 0;
    }
    switch_config->vidgroup[4096].vid = 1;
    switch_config->vidgroup[4096].portset = 0xf;
    switch_config->vidgroup[4096].port_in = switch_config->cpu_port;
    switch_config->vidgroup[4096].used = 1;

    for(port = 0; port < AVM_CPMAC_MAX_PORTS; port++) {
        switch_config->map_port_to_dev[port] = 0;
        switch_config->port[port].keep_tag_outgoing = 0;
        switch_config->port[port].use_tag_incoming = 0;
    }
    for(ps = 1; ps <= AVM_CPMAC_MAX_PORTSET; ps++) {
        switch_config->map_portset_out[ps] = 0;
    }
    switch_config->adm_used_bit_combinations = 0;
    switch_config->assigned_vids_count = 0;
    switch_config->wanport_keeptags = 0;

    /* Make sure to unregister wan fast forward, before the port info is gone */
    cpphy_switch_unregister_ffw_port(cpmac_global.cpphy[0].mdio.switch_config.wanport);

    /* Now start configuration */
    switch_config->enable_vlan = 1; /* TODO Consider old 7170 and other hardware! */
    if(mdio->f->set_good_vid_bitshift != NULL) {
        /* TODO Find a solution to get rid of config */
        mdio->f->set_good_vid_bitshift(mdio, config);
    }
    switch_config->devices = dconfig->number_of_devices;
    switch_config->wanport = dconfig->wanport;
    assert(switch_config->devices <= AVM_CPMAC_MAX_DEVS); /* To make Klocwork happy */

    /* Preallocate given vid_ranges */
    for(device = 0; device < dconfig->number_of_devices; device++) {
        cpmac_vid_range_t *vid_range = dconfig->device[device].vid_range;
        while(vid_range != NULL) {
            unsigned short vid;
            for(vid = vid_range->start; vid <= vid_range->end; vid++) {
                if(cpphy_switch_assign_vid(mdio, vid)) {
                    DEB_ERR("[%s] Error! VID %#x (%u) already assigned!\n", __FUNCTION__, vid, vid);
                    return CPMAC_ERR_MISC;
                }
            }
            vid_range = vid_range->next;
        }
    }

    for(device = 0; device < dconfig->number_of_devices; device++) {
        cpphy_switch_set_device(mdio, 
                                device,
                                &dconfig->device[device]);
    }

    for(device = 0; device < switch_config->devices; device++) {
        unsigned char *name = dconfig->device[device].name;
        unsigned short vid;
        unsigned int is_wan = (strcmp(name, "wan") == 0) ? 1 : 0;
        if(switch_config->device[device].vid == switch_config->port[switch_config->cpu_port].vid) {
            DEB_INFOTRC("[%s] No extra tagging needed for device '%s'\n", __FUNCTION__, name);
            vid = 0;
        } else {
            vid = switch_config->device[device].vid;
        }
        if(is_wan) {
            cpphy_switch_register_ffw_port(cpmac_global.cpphy[0].mdio.switch_config.wan_receive_function,
                                           cpmac_global.cpphy[0].mdio.switch_config.wanport);
        }
        switch_config->device[device].net_device = find_or_register_device(mdio, name, vid);
        assert(switch_config->device[device].net_device != NULL);
    }

    if(mdio->f->config_ports == NULL) {
        DEB_ERR("[%s] No config_ports function set! Aborting!\n", __FUNCTION__);
        return CPMAC_ERR_MISC;
    }
    mdio->f->config_ports(mdio);

    /* Now unregister the unused devices */
    for(i = 0; i < AVM_CPMAC_MAX_DEVS; i++) {
        struct net_device *dev = switch_config->olddevs[i];
        if(dev) {
            DEB_INFO("Unregister %s\n", dev->name);
            switch_config->olddevs[i] = 0;
            cpmac_unregister_device(dev);
        }
    }

    DEB_INFOTRC("[%s] Used VID groups:\n", __FUNCTION__);
    vid_group_count = 0;
    for(vid_group = 0; vid_group < switch_config->max_vid_groups; vid_group++) {
        if(switch_config->vidgroup[vid_group].used) {
            DEB_INFOTRC("[%s]   %#x: VID %#x (%u), portset %#x, port_in %u\n", 
                        __FUNCTION__, 
                        vid_group, 
                        switch_config->vidgroup[vid_group].vid,
                        switch_config->vidgroup[vid_group].vid,
                        switch_config->vidgroup[vid_group].portset,
                        switch_config->vidgroup[vid_group].port_in);
            vid_group_count++;
        }
    }
    DEB_INFOTRC("[%s] That leaves %u free VID groups.\n", __FUNCTION__, 
                switch_config->max_vid_groups - vid_group_count);
    DEB_INFOTRC("[%s] The assigned VID are:\n", __FUNCTION__);
    for(i = 0; i < switch_config->assigned_vids_count; i++) {
        DEB_INFOTRC("[%s]     %#x (%u)\n", __FUNCTION__, 
                    switch_config->assigned_vids[i],
                    switch_config->assigned_vids[i]);
    }
    DEB_TRC("\n[%s] done\n", __FUNCTION__);
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8)
    tasklet_enable(&mdio->cpmac_priv->tasklet);
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) ---*/
    if(mdio->f->cpmac_transfer_startstop) {
        mdio->f->cpmac_transfer_startstop(1);
    }
    cpphy_mgmt_work_start(mdio);

    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_switch_configure_special(cpphy_mdio_t *mdio, 
                                           cpmac_switch_configuration_t *config) {
    unsigned int i;

    DEB_INFOTRC("[%s] Setting special mode with %u devices and wanport %#x:\n", 
                __FUNCTION__, config->number_of_devices, config->wanport);
    for(i = 0; i < config->number_of_devices; i++) {
        DEB_INFOTRC("[%s]   '%s' on ports %#x\n",
                    __FUNCTION__, config->device[i].name, config->device[i].target_mask);
    }
    memcpy((unsigned char *) &mdio->switch_predefined_configs[CPMAC_MODE_SPECIAL], config, sizeof(cpmac_switch_configuration_t));
    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t adm_get_configured(cpphy_mdio_t *mdio, cpmac_switch_configuration_t *config) {
    memcpy(config,
           &mdio->switch_predefined_configs[mdio->switch_config.user_given_config.cpmac_mode],
           sizeof(cpmac_switch_configuration_t));
    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t adm_get_config_cpmac(cpphy_mdio_t *mdio, struct avm_cpmac_config_struct *config) {
    memcpy(config, &mdio->switch_config.user_given_config, sizeof(struct avm_cpmac_config_struct));
    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_switch_status(cpphy_mdio_t *mdio) {
    unsigned char device, port;
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;

    DEB_SUPPORT("General device configuration:\n");
    DEB_SUPPORT("    wan_port = %u\n", switch_config->device_config.wanport);
    for(device = 0; device < switch_config->device_config.number_of_devices; device++) {
        cpmac_vid_range_t *vid_range = switch_config->device_config.device[device].vid_range;
        DEB_SUPPORT("    %-*s: portmap %#x\n", 
                    AVM_CPMAC_MAX_DEVICE_NAME_LENGTH,
                    switch_config->device_config.device[device].name,
                    switch_config->device_config.device[device].target_mask);
        while(vid_range != NULL) {
            DEB_SUPPORT("        VID %04u - %04u (%#03x - %#03x)\n",
                        vid_range->start,
                        vid_range->end,
                        vid_range->start,
                        vid_range->end);
            vid_range = vid_range->next;
        }
    }
    DEB_SUPPORT("\n");

    DEB_SUPPORT("General switch status:\n");
    if(switch_config->user_given_config.cpmac_mode >= CPMAC_MODE_MAX_NO) {
        DEB_ERR("[%s] Unknown cpmac_configuration %u\n", __FUNCTION__, switch_config->user_given_config.cpmac_mode);
    } else {
        DEB_SUPPORT("    Switch config mode %s:\n", switch_mode_names[switch_config->user_given_config.cpmac_mode]);
        for(device = 0; device < mdio->switch_predefined_configs[switch_config->user_given_config.cpmac_mode].number_of_devices; device++) {
            DEB_SUPPORT("        %-*s: %#x\n", 
                         AVM_CPMAC_MAX_DEVICE_NAME_LENGTH,
                         mdio->switch_predefined_configs[switch_config->user_given_config.cpmac_mode].device[device].name,
                         mdio->switch_predefined_configs[switch_config->user_given_config.cpmac_mode].device[device].target_mask);
            if(  (1 << mdio->switch_predefined_configs[switch_config->user_given_config.cpmac_mode].wanport)
               & mdio->switch_predefined_configs[switch_config->user_given_config.cpmac_mode].device[device].target_mask) {
                unsigned char vid_no;
                for(vid_no = 0; vid_no < switch_config->user_given_config.wan_vlan_number; vid_no++) {
                    DEB_SUPPORT("            VID %#x\n", switch_config->user_given_config.wan_vlan_id[vid_no]);
                }
            }
        }
        DEB_SUPPORT("\n");
    }

    DEB_SUPPORT("    Devices:\n");
    for(device = 0; device < switch_config->devices; device++) {
        struct cpmac_devinfo *curr_priv_data = netdev_priv(switch_config->device[device].net_device);
        DEB_SUPPORT("        '%s': portset %#x, VID %#x (%u)\n",
                    switch_config->device[device].name,
                    switch_config->device[device].portset,
                    switch_config->device[device].vid,
                    switch_config->device[device].vid);
        if(switch_config->device[device].vid != curr_priv_data->VID) {
            DEB_ERR("[%s] Device VID %x and net_device VID (%x) differ!\n", __FUNCTION__,
                    switch_config->device[device].vid,
                    curr_priv_data->VID);
        }
    }
    DEB_SUPPORT("\n");
    DEB_SUPPORT("    Ports:\n");
    for(port = 0; port < AVM_CPMAC_MAX_PORTS; port++) {
        DEB_SUPPORT("        %u: portvlan %#x, vid %#x (%u), %s %s %s\n",
                    port,
                    switch_config->port[port].port_vlan_mask,
                    switch_config->port[port].vid,
                    switch_config->port[port].vid,
                    switch_config->port[port].keep_tag_outgoing ? "keep tags" : "keep no tags",
                    switch_config->port[port].use_tag_incoming ? "+ use port VID" : "",
                    (port == switch_config->cpu_port) ? ", is CPU port"
                                                      : (port == switch_config->wanport) ? ", is WAN port"
                                                                                         : "");
    }
    DEB_SUPPORT("\n");
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned long cpphy_switch_port_mirror(cpphy_mdio_t *mdio) {
    cpphy_mgmt_work_del(mdio, CPMAC_WORK_PORT_MIRROR);
    if(mdio->f->mirror_port != NULL) {
        mdio->f->mirror_port(mdio);
    } else {
        DEB_WARN("[%s] No function for port mirroring defined!\n", __FUNCTION__);
    }
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_cpmac_mirror_port(unsigned char enable_ingress,
                           unsigned char enable_egress,
                           unsigned char mirror_from_port,
                           unsigned char mirror_to_port) {
    cpphy_mdio_t *mdio = &cpmac_global.cpphy[0].mdio;
    mdio->switch_config.mirror_port.enable_ingress   = enable_ingress;
    mdio->switch_config.mirror_port.enable_egress    = enable_egress;
    mdio->switch_config.mirror_port.mirror_from_port = mirror_from_port;
    mdio->switch_config.mirror_port.mirror_to_port   = mirror_to_port;

    cpphy_mgmt_work_add(mdio, CPMAC_WORK_PORT_MIRROR, cpphy_switch_port_mirror, 0);
}
EXPORT_SYMBOL(avm_cpmac_mirror_port);

unsigned int avm_cpmac_get_wan_port_vlan(void) {
    cpphy_mdio_t *mdio = &cpmac_global.cpphy[0].mdio;
    return mdio->switch_config.map_portset_out[ 
	       1 << (0 /*Port 1*/ + mdio->switch_config.external_port_offset)];
}

EXPORT_SYMBOL(avm_cpmac_get_wan_port_vlan);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned long switch_dump(cpphy_mdio_t *mdio) {
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    unsigned int i;

    DEB_SUPPORT("Registerdump CPMAC SANGAM/OHIO\n");
    for(i = 0; i <= 0x28c; i += 4) {
        DEB_SUPPORT("CPMAC Register 0x%x = 0x%x\n", i, *((volatile unsigned int *) (mdio->cpmac_priv->owner->base_addr + i)));
    }
#   elif defined(CONFIG_MIPS_UR8) /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    DEB_SUPPORT("Registerdump CPMAC UR8\n");
    DEB_SUPPORT("    ID_VER             = %#x\n", mdio->cpmac_priv->CPGMAC_F->ID_VER);
    DEB_SUPPORT("    MAC_IN_VECTOR      = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_IN_VECTOR);
    DEB_SUPPORT("    MAC_EOI_VECTOR     = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_EOI_VECTOR);
    DEB_SUPPORT("    MAC_INT_STATRAW    = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_INT_STATRAW);
    DEB_SUPPORT("    MAC_INT_STATMASKED = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_INT_STATMASKED);
    DEB_SUPPORT("    MAC_INT_MASKSET    = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_INT_MASKSET);
    DEB_SUPPORT("    MAC_INT_MASKCLEAR  = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_INT_MASKCLEAR);
    DEB_SUPPORT("    RX_MBP_ENABLE      = %#x\n", mdio->cpmac_priv->CPGMAC_F->RX_MBP_ENABLE.Reg);
    DEB_SUPPORT("    RX_UNICAST_SET     = %#x\n", mdio->cpmac_priv->CPGMAC_F->RX_UNICAST_SET.Reg);
    DEB_SUPPORT("    RX_UNICAST_CLEAR   = %#x\n", mdio->cpmac_priv->CPGMAC_F->RX_UNICAST_CLEAR.Reg);
    DEB_SUPPORT("    RX_MAX_LEN         = %#x\n", mdio->cpmac_priv->CPGMAC_F->RX_MAX_LEN.Reg);
    DEB_SUPPORT("    MAC_CONTROL        = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_CONTROL.Reg);
    DEB_SUPPORT("    MAC_STATUS         = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_STATUS.Reg);
    DEB_SUPPORT("    FIFO_CONTROL       = %#x\n", mdio->cpmac_priv->CPGMAC_F->FIFO_CONTROL);
    DEB_SUPPORT("    SOFT_RESET         = %#x\n", mdio->cpmac_priv->CPGMAC_F->SOFT_RESET);
    DEB_SUPPORT("    MAC_SRC_ADDR_LOW   = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_SRC_ADDR_LOW.Reg);
    DEB_SUPPORT("    MAC_SRC_ADDR_HIGH  = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_SRC_ADDR_HIGH.Reg);
    DEB_SUPPORT("    MAC_HASH1          = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_HASH1);
    DEB_SUPPORT("    MAC_HASH2          = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_HASH2);
    DEB_SUPPORT("    BOFF_TEST          = %#x\n", mdio->cpmac_priv->CPGMAC_F->BOFF_TEST.Reg);
    DEB_SUPPORT("    TPACE_TEST         = %#x\n", mdio->cpmac_priv->CPGMAC_F->TPACE_TEST);
    DEB_SUPPORT("    RX_PAUSE           = %#x\n", mdio->cpmac_priv->CPGMAC_F->RX_PAUSE);
    DEB_SUPPORT("    TX_PAUSE           = %#x\n", mdio->cpmac_priv->CPGMAC_F->TX_PAUSE);
    DEB_SUPPORT("    PORT_VLAN          = %#x\n", mdio->cpmac_priv->CPGMAC_F->PORT_VLAN);
    DEB_SUPPORT("    RX_FLOW_THRESH     = %#x\n", mdio->cpmac_priv->CPGMAC_F->RX_FLOW_THRESH);
    DEB_SUPPORT("    MAC_ADDR_LOW       = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_ADDR_LOW.Reg);
    DEB_SUPPORT("    MAC_ADDR_HIGH      = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_ADDR_HIGH.Reg);
    DEB_SUPPORT("    MAC_INDEX          = %#x\n", mdio->cpmac_priv->CPGMAC_F->MAC_INDEX);
    DEB_SUPPORT("    STATISTICS:\n");
    DEB_SUPPORT("        RxGoodFrames          = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxGoodFrames);
    DEB_SUPPORT("        RxBroadcastFrames     = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxBroadcastFrames);
    DEB_SUPPORT("        RxMulticastFrames     = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxMulticastFrames);
    DEB_SUPPORT("        RxPauseFrames         = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxPauseFrames);
    DEB_SUPPORT("        RxCRCErrors           = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxCRCErrors);
    DEB_SUPPORT("        RxAlignCodeErrors     = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxAlignCodeErrors);
    DEB_SUPPORT("        RxOversizedFrames     = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxOversizedFrames);
    DEB_SUPPORT("        RxJabberFrames        = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxJabberFrames);
    DEB_SUPPORT("        RxUndersizedFrames    = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxUndersizedFrames);
    DEB_SUPPORT("        RxFragments           = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxFragments);
    DEB_SUPPORT("        RxFilteredFrames      = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxFilteredFrames);
    DEB_SUPPORT("        RxQOSFilteredFrames   = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxQOSFilteredFrames);
    DEB_SUPPORT("        RxOctets              = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxOctets);
    DEB_SUPPORT("        TxGoodFrames          = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxGoodFrames);
    DEB_SUPPORT("        TxBroadcastFrames     = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxBroadcastFrames);
    DEB_SUPPORT("        TxMulticastFrames     = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxMulticastFrames);
    DEB_SUPPORT("        TxPauseFrames         = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxPauseFrames);
    DEB_SUPPORT("        TxDeferredFrames      = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxDeferredFrames);
    DEB_SUPPORT("        TxCollisionFrames     = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxCollisionFrames);
    DEB_SUPPORT("        TxSingleCollFrames    = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxSingleCollFrames);
    DEB_SUPPORT("        TxMultiCollFrames     = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxMultiCollFrames);
    DEB_SUPPORT("        TxExcessiveCollisions = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxExcessiveCollisions);
    DEB_SUPPORT("        TxLateCollisions      = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxLateCollisions);
    DEB_SUPPORT("        TxUnderrun            = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxUnderrun);
    DEB_SUPPORT("        TxCarrierSenseErrors  = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxCarrierSenseErrors);
    DEB_SUPPORT("        TxOctets              = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.TxOctets);
    DEB_SUPPORT("        Tx64octetFrames       = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.Tx64octetFrames);
    DEB_SUPPORT("        Tx65t127octetFrames   = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.Tx65t127octetFrames);
    DEB_SUPPORT("        Tx128t255octetFrames  = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.Tx128t255octetFrames);
    DEB_SUPPORT("        Tx256t511octetFrames  = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.Tx256t511octetFrames);
    DEB_SUPPORT("        Tx512t1023octetFrames = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.Tx512t1023octetFrames);
    DEB_SUPPORT("        Tx1024tUoctetFrames   = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.Tx1024tUoctetFrames);
    DEB_SUPPORT("        NetOctets             = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.NetOctets);
    DEB_SUPPORT("        RxSofOverruns         = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxSofOverruns);
    DEB_SUPPORT("        RxMofOverruns         = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxMofOverruns);
    DEB_SUPPORT("        RxDmaOverruns         = %u\n", mdio->cpmac_priv->CPGMAC_F->STATISTIC.RxDmaOverruns);

    DEB_SUPPORT("NWSS registers\n");
    DEB_SUPPORT("    MAC0 Tx Channel tx_A = %#x, tx_B = %#x\n", 
                mdio->cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].tx_A.Register,
                mdio->cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].tx_B.Register);
    DEB_SUPPORT("    MAC0 Rx Channel rx_A = %#x, rx_B = %#x\n", 
                mdio->cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].rx_A.Register,
                mdio->cpmac_priv->UR8_NWSS->Channel_Cfg[UR8_TX_MAC0].rx_B.Register);
    DEB_SUPPORT("    Queue tx_int_enable       = %#x\n", mdio->cpmac_priv->UR8_QUEUE->tx_int_enable_set.Register);
    DEB_SUPPORT("    Queue tx_int_end          = %#x\n", mdio->cpmac_priv->UR8_QUEUE->tx_int_end.Register);
    DEB_SUPPORT("    Queue rx_int_enable       = %#x\n", mdio->cpmac_priv->UR8_QUEUE->rx_int_enable_set.Register);
    DEB_SUPPORT("    Queue rx_int_end          = %#x\n", mdio->cpmac_priv->UR8_QUEUE->rx_int_end.Register);
    DEB_SUPPORT("    Queue rx starvation count = %#x\n", mdio->cpmac_priv->UR8_QUEUE->queue_starvation_count.Register);
#   elif defined(CONFIG_ARCH_PUMA5) /*--- #elif defined(CONFIG_MIPS_UR8) ---*/
        /*--- printk("Registerdump CPMAC Puma\n"); ---*/
        /*--- printk("IdVer                  = %#x\n", hDDA->hDDC->regs->IdVer); ---*/
        /*--- printk("Mac_In_Vector          = %#x\n", hDDA->hDDC->regs->Mac_In_Vector); ---*/
        /*--- printk("Mac_EOI_Vector         = %#x\n", hDDA->hDDC->regs->Mac_EOI_Vector); ---*/
        /*--- printk("Mac_IntStat_Raw        = %#x\n", hDDA->hDDC->regs->Mac_IntStat_Raw); ---*/
        /*--- printk("Mac_IntStat_Masked     = %#x\n", hDDA->hDDC->regs->Mac_IntStat_Masked); ---*/
        /*--- printk("Mac_IntMask_Set        = %#x\n", hDDA->hDDC->regs->Mac_IntMask_Set); ---*/
        /*--- printk("Mac_IntMask_Clear      = %#x\n", hDDA->hDDC->regs->Mac_IntMask_Clear); ---*/
        /*--- printk("Rx_MBP_Enable          = %#x\n", hDDA->hDDC->regs->Rx_MBP_Enable); ---*/
        /*--- printk("Rx_Unicast_Set         = %#x\n", hDDA->hDDC->regs->Rx_Unicast_Set); ---*/
        /*--- printk("Rx_Unicast_Clear       = %#x\n", hDDA->hDDC->regs->Rx_Unicast_Clear); ---*/
        /*--- printk("Rx_Maxlen              = %#x\n", hDDA->hDDC->regs->Rx_Maxlen); ---*/
        /*--- printk("MacControl             = %#x\n", hDDA->hDDC->regs->MacControl); ---*/
        /*--- printk("MacStatus              = %#x\n", hDDA->hDDC->regs->MacStatus); ---*/
        /*--- printk("EMControl              = %#x\n", hDDA->hDDC->regs->EMControl); ---*/
        /*--- printk("FifoControl            = %#x\n", hDDA->hDDC->regs->FifoControl); ---*/
        /*--- printk("Mac_Cfig               = %#x\n", hDDA->hDDC->regs->Mac_Cfig); ---*/
        /*--- printk("Soft_Reset             = %#x\n", hDDA->hDDC->regs->Soft_Reset); ---*/
        /*--- printk("MacSrcAddr_Lo          = %#x\n", hDDA->hDDC->regs->MacSrcAddr_Lo); ---*/
        /*--- printk("MacSrcAddr_Hi          = %#x\n", hDDA->hDDC->regs->MacSrcAddr_Hi); ---*/
        /*--- printk("MacHash1               = %#x\n", hDDA->hDDC->regs->MacHash1); ---*/
        /*--- printk("MacHash2               = %#x\n", hDDA->hDDC->regs->MacHash2); ---*/
        /*--- printk("BoffTest               = %#x\n", hDDA->hDDC->regs->BoffTest); ---*/
        /*--- printk("Tpace_Test             = %#x\n", hDDA->hDDC->regs->Tpace_Test); ---*/
        /*--- printk("Rx_Pause               = %#x\n", hDDA->hDDC->regs->Rx_Pause); ---*/
        /*--- printk("Tx_Pause               = %#x\n", hDDA->hDDC->regs->Tx_Pause); ---*/
        /*--- printk("Port_VLAN              = %#x\n", hDDA->hDDC->regs->Port_VLAN); ---*/
        /*--- printk("Rx_Flowthresh          = %#x\n", hDDA->hDDC->regs->Rx_Flowthresh); ---*/
        /*--- printk("RxGoodFrames           = %#x\n", hDDA->hDDC->regs->RxGoodFrames); ---*/
        /*--- printk("RxBroadcastFrames      = %#x\n", hDDA->hDDC->regs->RxBroadcastFrames); ---*/
        /*--- printk("RxMulticastFrames      = %#x\n", hDDA->hDDC->regs->RxMulticastFrames); ---*/
        /*--- printk("RxPauseFrames          = %#x\n", hDDA->hDDC->regs->RxPauseFrames); ---*/
        /*--- printk("RxCRCErrors            = %#x\n", hDDA->hDDC->regs->RxCRCErrors); ---*/
        /*--- printk("RxAlignCodeErrors      = %#x\n", hDDA->hDDC->regs->RxAlignCodeErrors); ---*/
        /*--- printk("RxOversizedFrames      = %#x\n", hDDA->hDDC->regs->RxOversizedFrames); ---*/
        /*--- printk("RxJabberFrames         = %#x\n", hDDA->hDDC->regs->RxJabberFrames); ---*/
        /*--- printk("RxUndersizedFrames     = %#x\n", hDDA->hDDC->regs->RxUndersizedFrames); ---*/
        /*--- printk("RxFragments            = %#x\n", hDDA->hDDC->regs->RxFragments); ---*/
        /*--- printk("RxFilteredFrames       = %#x\n", hDDA->hDDC->regs->RxFilteredFrames); ---*/
        /*--- printk("RxOctets               = %#x\n", hDDA->hDDC->regs->RxOctets); ---*/
        /*--- printk("TxGoodFrames           = %#x\n", hDDA->hDDC->regs->TxGoodFrames); ---*/
        /*--- printk("TxBroadcastFrames      = %#x\n", hDDA->hDDC->regs->TxBroadcastFrames); ---*/
        /*--- printk("TxMulticastFrames      = %#x\n", hDDA->hDDC->regs->TxMulticastFrames); ---*/
        /*--- printk("TxPauseFrames          = %#x\n", hDDA->hDDC->regs->TxPauseFrames); ---*/
        /*--- printk("TxDeferredFrames       = %#x\n", hDDA->hDDC->regs->TxDeferredFrames); ---*/
        /*--- printk("TxCollisionFrames      = %#x\n", hDDA->hDDC->regs->TxCollisionFrames); ---*/
        /*--- printk("TxSingleCollFrames     = %#x\n", hDDA->hDDC->regs->TxSingleCollFrames); ---*/
        /*--- printk("TxMultCollFrames       = %#x\n", hDDA->hDDC->regs->TxMultCollFrames); ---*/
        /*--- printk("TxExcessiveCollisions  = %#x\n", hDDA->hDDC->regs->TxExcessiveCollisions); ---*/
        /*--- printk("TxLateCollisions       = %#x\n", hDDA->hDDC->regs->TxLateCollisions); ---*/
        /*--- printk("TxUnderrun             = %#x\n", hDDA->hDDC->regs->TxUnderrun); ---*/
        /*--- printk("TxCarrierSenseErrors   = %#x\n", hDDA->hDDC->regs->TxCarrierSenseErrors); ---*/
        /*--- printk("TxOctets               = %#x\n", hDDA->hDDC->regs->TxOctets); ---*/
        /*--- printk("Reg64octetFrames       = %#x\n", hDDA->hDDC->regs->Reg64octetFrames); ---*/
        /*--- printk("Reg65t127octetFrames   = %#x\n", hDDA->hDDC->regs->Reg65t127octetFrames); ---*/
        /*--- printk("Reg128t255octetFrames  = %#x\n", hDDA->hDDC->regs->Reg128t255octetFrames); ---*/
        /*--- printk("Reg256t511octetFrames  = %#x\n", hDDA->hDDC->regs->Reg256t511octetFrames); ---*/
        /*--- printk("Reg512t1023octetFrames = %#x\n", hDDA->hDDC->regs->Reg512t1023octetFrames); ---*/
        /*--- printk("Reg1024tUPoctetFrames  = %#x\n", hDDA->hDDC->regs->Reg1024tUPoctetFrames); ---*/
        /*--- printk("NetOctets              = %#x\n", hDDA->hDDC->regs->NetOctets); ---*/
        /*--- printk("RxSofOverruns          = %#x\n", hDDA->hDDC->regs->RxSofOverruns); ---*/
        /*--- printk("RxMofOverruns          = %#x\n", hDDA->hDDC->regs->RxMofOverruns); ---*/
        /*--- printk("RxDmaOverruns          = %#x\n", hDDA->hDDC->regs->RxDmaOverruns); ---*/
        /*--- printk("MacAddr_Lo             = %#x\n", hDDA->hDDC->regs->MacAddr_Lo); ---*/
        /*--- printk("MacAddr_Hi             = %#x\n", hDDA->hDDC->regs->MacAddr_Hi); ---*/
        /*--- printk("QueueInfo              = %#x\n", hDDA->hDDC->regs->QueueInfo); ---*/
        /*--- printk("MacIndex               = %#x\n", hDDA->hDDC->regs->MacIndex); ---*/
#   endif /*--- #elif defined(CONFIG_ARCH_PUMA5) ---*/
    DEB_SUPPORT("\n");

    if(cpmac_global.global_power == CPPHY_POWER_GLOBAL_NONE) {
        cpphy_switch_status(mdio);

        if(mdio->f->switch_dump != NULL) {
            mdio->f->switch_dump(mdio);
        } else {
            DEB_SUPPORT("Switch dump requested, but no known switch is connected.\n");
        }
        if(mdio->f->print_mac_table != NULL) {
            mdio->f->print_mac_table(mdio);
        }
    } else {
        DEB_SUPPORT("Global power setting used. Doing no switch dump.\n");
    }
    mdio->switch_dump_value = 0;
    cpphy_mgmt_work_del(mdio, CPMAC_WORK_SWITCH_DUMP);
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_switch_ioctl(cpphy_mdio_t                  *mdio, 
                               struct avm_cpmac_ioctl_struct *ioctl_struct) {
    cpmac_err_t ret;

    ret = CPMAC_ERR_NOERR;
    DEB_INFOTRC("[%s] Generic switch config '%s' (%u) called\n", 
                 __FUNCTION__,
                 (ioctl_struct->type < AVM_CPMAC_IOCTL_NUMBER_OF_IOCTLS) ? switch_config_names[ioctl_struct->type] : "unknown",
                 ioctl_struct->type);
    switch(ioctl_struct->type) {
        case AVM_CPMAC_IOCTL_CONFIG_SET_PHY_REGISTER:
            mdio_write(mdio, 
                       ioctl_struct->u.phy_register.phy, 
                       ioctl_struct->u.phy_register.reg, 
                       ioctl_struct->u.phy_register.value);
            break;
        case AVM_CPMAC_IOCTL_CONFIG_GET_PHY_REGISTER:
            ioctl_struct->u.phy_register.value = mdio_read(mdio, 
                                                           ioctl_struct->u.phy_register.phy,
                                                           ioctl_struct->u.phy_register.reg);
            break;
        case AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_REGISTER:
            if(mdio->f->get_switch_register) {
                ioctl_struct->u.switch_register.value = mdio->f->get_switch_register(mdio, ioctl_struct->u.switch_register.reg);
            } else {
                ret = CPMAC_ERR_ILL_CONTROL;
            }
            break;
        case AVM_CPMAC_IOCTL_CONFIG_SET_SWITCH_REGISTER:
            if(mdio->f->set_switch_register) {
                mdio->f->set_switch_register(mdio, ioctl_struct->u.switch_register.reg, ioctl_struct->u.switch_register.value);
            } else {
                ret = CPMAC_ERR_ILL_CONTROL;
            }
            break;
        case AVM_CPMAC_IOCTL_CONFIG_GET_INFO:
            /*--- ioctl_struct->u.info.internal_ports = xxx ? 1 : 2; ---*/
            /*--- ioctl_struct->u.info.external_ports = (cpmac_err_t)((cpphy_global_t *) phy_handle)->cpmac_switch ? 4 : ioctl_struct->u.info.internal_ports; ---*/
            /*--- ret = CPMAC_ERR_NOERR; ---*/
            /* FIXME Must be corrected */
            ret = CPMAC_ERR_INTERNAL;
            break;
        case AVM_CPMAC_IOCTL_SUPPORT_DATA:
            cpphy_mgmt_support_data(mdio);
            break;
        case AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_REGISTER_DUMP:
            mdio->switch_dump_value = ioctl_struct->u.value;
            cpphy_mgmt_work_add(mdio, CPMAC_WORK_SWITCH_DUMP, switch_dump, 0);
            break;
#       if defined(CPMAC_EXTERNAL_COMMAND_DEBUG)
        case AVM_CPMAC_IOCTL_CONFIG_TESTCMD:
            cpphy_mgmt_debug(mdio, ioctl_struct->u.value);
            break;
        case AVM_CPMAC_IOCTL_CONFIG_PEEK:
            ret = cpphy_mgmt_debug_peek(ioctl_struct->u.peek.ptr,
                                        ioctl_struct->u.peek.size,
                                        ioctl_struct->u.peek.value);
            break;
        case AVM_CPMAC_IOCTL_CONFIG_POKE:
            ret = cpphy_mgmt_debug_poke(ioctl_struct->u.peek.ptr,
                                        ioctl_struct->u.peek.size,
                                        ioctl_struct->u.peek.value);
            break;
#       endif /*--- #if defined(CPMAC_EXTERNAL_COMMAND_DEBUG) ---*/
        default:
#           if defined(CONFIG_AVM_CPMAC_SWITCH)
            if(!mdio->switch_config.is_switch) {
                ret = CPMAC_ERR_ILL_CONTROL;
                break;
            }
            switch(ioctl_struct->type) {
                case AVM_CPMAC_IOCTL_CONFIG_GET_BYTES_IN_WAN:
                    ioctl_struct->u.result =   mdio->cpmac_priv->cppi->TxPrioQueues.q[CPPHY_PRIO_QUEUE_WAN].BytesEnqueued 
                                             - mdio->cpmac_priv->cppi->TxPrioQueues.q[CPPHY_PRIO_QUEUE_WAN].BytesDequeued;
                    break;
                case AVM_CPMAC_IOCTL_CONFIG_SET_SWITCH_MODE:
                    /* Make sure, that nothing works in parallel */
                    ret = cpphy_switch_configure(mdio, &ioctl_struct->u.switch_mode);
                    break;
                case AVM_CPMAC_IOCTL_CONFIG_ADD_PORT_TO_WAN:
                    ret = CPMAC_ERR_ILL_CONTROL;
                    if(mdio->f->add_port_to_wan != NULL) {
                        ret = mdio->f->add_port_to_wan(mdio, 
                                                       ioctl_struct->u.config_port_vlan.port,
                                                       ioctl_struct->u.config_port_vlan.vid);
                    }
                    break;
                case AVM_CPMAC_IOCTL_CONFIG_SET_TRUNK_PORTS:
                    ret = CPMAC_ERR_ILL_CONTROL;
                    if(mdio->f->add_trunk_port != NULL) {
                        ret = mdio->f->add_trunk_port(mdio, ioctl_struct->u.trunk);
                    }
                    break;
#               if defined(CONFIG_MIPS_UR8)
                case AVM_CPMAC_IOCTL_CONFIG_SET_PPPOA:
                    ret = adm_set_mode_pppoa(mdio, ioctl_struct->u.value);
                    break;
#               endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/
                case AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_MODE:
                    /* This is generic enough to work with the Atheros as well */
                    ret = adm_get_config_cpmac(mdio, &ioctl_struct->u.switch_mode);
                    break;
                case AVM_CPMAC_IOCTL_CONFIG_SET_SWITCH_MODE_SPECIAL:
                    if(mdio->f->configure_special) {
                        ret = mdio->f->configure_special(mdio, &ioctl_struct->u.switch_config);
                    } else {
                        ret = CPMAC_ERR_ILL_CONTROL;
                    }
                    break;
                case AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_MODE_CURRENT:
                    /* This is generic enough to work with the Atheros as well */
                    ret = adm_get_configured(mdio, &ioctl_struct->u.switch_config);
                    break;
                case AVM_CPMAC_IOCTL_CONFIG_SET_PHY_POWER:
                    ret = adm_switch_port_power_config(mdio, &ioctl_struct->u.phy_power_setup, 1);
                    break;
                case AVM_CPMAC_IOCTL_CONFIG_GET_PHY_POWER:
                    ret = adm_switch_port_power_config(mdio, &ioctl_struct->u.phy_power_setup, 0);
                    break;
                case AVM_CPMAC_IOCTL_CONFIG_GET_WAN_KEEPTAG:
                    ioctl_struct->u.result = mdio->switch_config.wanport_keeptags;
                    break;
                case AVM_CPMAC_IOCTL_CONFIG_GET_PORT_OUT_VID_MAP:
                    {
                        unsigned int port;
                        for(port = 0; port < 4; port++) {
                            assert(port + mdio->switch_config.external_port_offset < AVM_CPMAC_MAX_PORTS);
                            ioctl_struct->u.port_out_vid_map[port] = mdio->switch_config.map_portset_out[1 << (port + mdio->switch_config.external_port_offset)];
                            /*--- ioctl_struct->u.port_out_vid_map[port] = mdio->switch_config.map_port_out[port + mdio->switch_config.external_port_offset]; ---*/
                            DEB_INFOTRC("[%s] map_port_out output: port %u = %#x\n", __FUNCTION__, port, ioctl_struct->u.port_out_vid_map[port]);
                        }
                    }
                    break;
                case AVM_CPMAC_IOCTL_CONFIG_MIRROR_PORT:
                    avm_cpmac_mirror_port(ioctl_struct->u.mirror_port.enable_ingress,
                                          ioctl_struct->u.mirror_port.enable_egress,
                                          ioctl_struct->u.mirror_port.mirror_from_port,
                                          ioctl_struct->u.mirror_port.mirror_to_port);
                    break;
                case AVM_CPMAC_IOCTL_CONFIG_SET_IGMP_FWD:
                    if(mdio->f->set_igmp_fwd) {
                        mdio->f->set_igmp_fwd(mdio, ioctl_struct->u.igmp_fwd_portmap);
                    } else {
                        ret = CPMAC_ERR_ILL_CONTROL;
                    }
                    break;
                default:
                    ret = CPMAC_ERR_ILL_CONTROL;
                    break;
            }
#           else /*--- #if defined(CONFIG_AVM_CPMAC_SWITCH) ---*/
            ret = CPMAC_ERR_ILL_CONTROL;
#           endif /*--- #else ---*/ /*--- #if defined(CONFIG_AVM_CPMAC_SWITCH) ---*/
            break;
    }
    if(ret == CPMAC_ERR_ILL_CONTROL) {
        DEB_INFO("[%s] Unknown type for CPMAC_CONTROL_REQ_GENERIC_CONFIG: %u\n", __FUNCTION__, ioctl_struct->type);
    }

    return ret;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_switch_device_vlan_change_notify(char *devicename, unsigned short vid) {
    cpmac_device_vlan_change_cb_item_t *item;

	for(item = cpmac_global.vlan_change_cb; item; item = item->next) {
        if(   (strlen(devicename) == strlen(item->devicename))
           && (strncmp(devicename, item->devicename, AVM_CPMAC_MAX_DEVICE_NAME_LENGTH) == 0)) {
            item->cb(devicename, vid, item->context);
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpmac_register_device_vlan_change_cb(char                        *devicename, 
                                                  cpmac_device_vlan_change_cb  cb,
                                                  void                        *context) {
    cpmac_device_vlan_change_cb_item_t *item;
    unsigned char device, phy;

    if(strlen(devicename) > AVM_CPMAC_MAX_DEVICE_NAME_LENGTH) {
        DEB_ERR("[%s] Someone tried to register a too long device name!\n", __FUNCTION__);
        return 1;
    }
    if(cb == NULL) {
        DEB_ERR("[%s] Call back function is a NULL pointer!\n", __FUNCTION__);
        return 1;
    }
    item = kmalloc(sizeof(cpmac_device_vlan_change_cb_item_t), GFP_KERNEL);
    if(item == NULL) {
        DEB_ERR("[%s] Out of memory!\n", __FUNCTION__);
        return 1;
    }
    strncpy(item->devicename, devicename, AVM_CPMAC_MAX_DEVICE_NAME_LENGTH);
    item->cb = cb;
    item->context = context;
    item->next = NULL;
    if(cpmac_global.vlan_change_cb == NULL) {
        cpmac_global.vlan_change_cb = item;
    } else {
        cpmac_device_vlan_change_cb_item_t *last_item = cpmac_global.vlan_change_cb;
        while(last_item->next != NULL) {
            last_item = last_item->next;
        }
        last_item->next = item;
    }

    /* Initial call back! */
    for(phy = 0; phy < cpmac_global.phys; phy++) {
        for(device = 0; device < cpmac_global.cpphy[phy].mdio.switch_config.devices; device++) {
            t_cpmac_device_config *dev = &cpmac_global.cpphy[phy].mdio.switch_config.device[device];
            if(   (strlen(devicename) == strlen(dev->name))
               && (strncmp(devicename, dev->name, AVM_CPMAC_MAX_DEVICE_NAME_LENGTH) == 0)) {
                cb(devicename, dev->vid, context);
            }
        }
    }
    DEB_INFOTRC("[%s] '%s' (cb = %p) registered successful\n", __FUNCTION__, devicename, cb);

    return 0;
}
EXPORT_SYMBOL(cpmac_register_device_vlan_change_cb);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpmac_unregister_device_vlan_change_cb(char                        *devicename, 
                                                    cpmac_device_vlan_change_cb  cb,
                                                    void                        *context __attribute__ ((unused))) {
    cpmac_device_vlan_change_cb_item_t *item, *last_item = NULL;

    if(cb == NULL) {
        DEB_ERR("[%s] Call back function is a NULL pointer!\n", __FUNCTION__);
        return 1;
    }
    item = cpmac_global.vlan_change_cb;
    while(item != NULL) {
        if(   (strlen(devicename) == strlen(item->devicename))
           && (strncmp(devicename, item->devicename, AVM_CPMAC_MAX_DEVICE_NAME_LENGTH) == 0)
           && (cb == item->cb)) {
            DEB_TRC("[%s] for '%s' (cb = %p) successful\n", __FUNCTION__, devicename, cb);
            if(last_item == NULL) {
                cpmac_global.vlan_change_cb = item->next;
            } else {
                last_item->next = item->next;
            }
            kfree(item);
            return 0;
        }
        last_item = item;
        item = item->next;
    }
    DEB_WARN("[%s] for '%s' (cb = %p) failed!\n", __FUNCTION__, devicename, cb);
    return 1;
}
EXPORT_SYMBOL(cpmac_unregister_device_vlan_change_cb);


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpphy_switch_register_ffw_port(cpmac_fast_forward_rx_func_ptr f, 
                                            unsigned char                  port) {
    int irqs_are_enabled = !irqs_disabled();
    if(irqs_are_enabled) {
        local_bh_disable();
    }
    if((port >= AVM_CPMAC_MAX_PORTS) || (cpmac_global.ffw_rx_ptr[port] != NULL)) {
        if(irqs_are_enabled) {
            local_bh_enable();
        }
        DEB_WARN("[%s] Illegal port (%u) or fast forward already set!\n", __FUNCTION__, port);
        return 1;
    }
    DEB_INFO("[%s] Register fast forward function %p for port %u\n", __FUNCTION__, f, port);
    cpmac_global.ffw_rx_ptr[port] = f;
    if(irqs_are_enabled) {
        local_bh_enable();
    }
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_switch_unregister_ffw_port(unsigned char port) {
    int irqs_are_enabled = !irqs_disabled();
    if(port >= AVM_CPMAC_MAX_PORTS) {
        DEB_WARN("[%s] Illegal port %u!\n", __FUNCTION__, port);
        return;
    }
    if(irqs_are_enabled) {
        local_bh_disable();
    }
    DEB_INFO("[%s] Unregister fast forward function for port %u\n", __FUNCTION__, port);
    cpmac_global.ffw_rx_ptr[port] = NULL;
    if(irqs_are_enabled) {
        local_bh_enable();
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_switch_add_trunk_port(cpphy_mdio_t *mdio,
                                        cpmac_trunk_port_configuration_t trunk_config) {
    cpmac_device_configuration_t *device_config = &(mdio->switch_config.device_config);
    unsigned int device, device_exists = 0, trunk = 0;
    cpmac_vid_range_t *vid_range;

    DEB_INFO("[%s] '%s' on portset %#x in VID range %u - %u (%#x - %#x)\n", __func__,
             trunk_config.name, trunk_config.portset,
             trunk_config.vlan_range_start, trunk_config.vlan_range_end,
             trunk_config.vlan_range_start, trunk_config.vlan_range_end);
    if(trunk_config.vlan_range_start > trunk_config.vlan_range_end) {
        DEB_WARN("[%s] Illegal VLAN range: start %u (%#x) > end %u (%#x)\n", __func__,
                 trunk_config.vlan_range_start, trunk_config.vlan_range_start,
                 trunk_config.vlan_range_end, trunk_config.vlan_range_end);
        return CPMAC_ERR_MISC;
    }

    for(device = 0; device < device_config->number_of_devices; device++) {
        if(strncmp(trunk_config.name, device_config->device[device].name, AVM_CPMAC_MAX_DEVICE_NAME_LENGTH) == 0) {
            device_exists = 1;
            trunk = device;
            break;
        }
    }
    if(!device_exists) {
        if(trunk_config.portset == 0) {
            DEB_WARN("[%s] No port mask given and device '%s' does not exist!\n", __func__, trunk_config.name);
            return CPMAC_ERR_MISC;
        }
        if(device_config->number_of_devices >= AVM_CPMAC_MAX_DEVS) {
            DEB_WARN("[%s] Can not add another device. Limit reached.\n", __func__);
            return CPMAC_ERR_MISC;
        }
        for(device = device_config->number_of_devices; device > 0; device--) {
            device_config->device[device] = device_config->device[device - 1];
        }
        memset(&device_config->device[0], 0, sizeof(cpmac_device_new_t));
        strncpy(device_config->device[0].name, trunk_config.name, AVM_CPMAC_MAX_DEVICE_NAME_LENGTH);
        device_config->number_of_devices++;
        trunk = 0;
    } else {
        if(trunk_config.portset != 0) {
            DEB_WARN("[%s] Device '%s' exists and will be modified to external port set %#x!\n",
                     __func__, trunk_config.name, trunk_config.portset);
        }
    }

    if(trunk_config.portset != 0) {
        unsigned short portset;

        portset = trunk_config.portset << mdio->switch_config.external_port_offset;

        for(device = 0; device < device_config->number_of_devices; device++) {
            if(device == trunk) {
                /* This will be changed anyway */
                continue;
            }
            device_config->device[device].target_mask &= ~portset;
            if(device_config->device[device].target_mask == (1 << mdio->switch_config.cpu_port)) {
                DEB_WARN("[%s] device '%s' has only the CPU port left!\n", __func__,
                         device_config->device[device].name);
                return CPMAC_ERR_MISC;
            }
        }
        device_config->device[trunk].target_mask = portset | (1 << mdio->switch_config.cpu_port);
    }

    /* Erase old VID range(s) */
    vid_range = device_config->device[trunk].vid_range;
    while(vid_range != NULL) {
        cpmac_vid_range_t *old_range = vid_range;
        vid_range = vid_range->next;
        kfree(old_range);
    }
    device_config->device[trunk].vid_range = NULL;

    /* Check for overlapping VLAN ranges */
    for(device = 0; device < device_config->number_of_devices; device++) {
        vid_range = device_config->device[device].vid_range;
        while(vid_range != NULL) {
            if(   (   (trunk_config.vlan_range_start >= vid_range->start) 
                   && (trunk_config.vlan_range_start <= vid_range->end))
               || (   (trunk_config.vlan_range_end   >= vid_range->start)
                   && (trunk_config.vlan_range_end   <= vid_range->end))
               || (   (trunk_config.vlan_range_start <= vid_range->start)
                   && (trunk_config.vlan_range_end   >= vid_range->end))) {
                DEB_WARN("[%s] VLAN range %#x - %#x clashes with device '%s' (%#x - %#x)\n", __func__,
                         trunk_config.vlan_range_start, trunk_config.vlan_range_end,
                         device_config->device[device].name,
                         vid_range->start, vid_range->end);
                return CPMAC_ERR_MISC;
            }
            vid_range = vid_range->next;
        }
    }

    /* Now enter the wished for VID ranges */
    vid_range = new_vid_range(trunk_config.vlan_range_start, trunk_config.vlan_range_end);
    if(vid_range == NULL) {
        DEB_ERR("[%s] No memory for vid_range!\n", __FUNCTION__);
        return CPMAC_ERR_NOMEM;
    }
    device_config->device[trunk].vid_range = vid_range;

    return cpphy_switch_real_configure(mdio, NULL);
}

