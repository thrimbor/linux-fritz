/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2009,2010,2011 AVM GmbH <fritzbox_info@avm.de>
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

#ifndef _CPPHY_SWITCH_H_
#define _CPPHY_SWITCH_H_

#include <linux/ioctl.h>
#include <linux/avm_cpmac.h>

typedef struct cpmac_device_config_struct {
    char               name[AVM_CPMAC_MAX_DEVICE_NAME_LENGTH + 1];
    unsigned short     portset;
    unsigned char      FID; /* Tantos-FID / default bit combination for ADM6996 */
    unsigned char      adm_used_bit_combinations;
    unsigned short     vid;
    struct net_device *net_device;
} t_cpmac_device_config;

#define AVM_CPMAC_DEFAULT_PORT AVM_CPMAC_MAX_PORTS
#define AVM_CPMAC_MAX_PORTSET 0x1f

enum avm_cpmac_mdio_targets {
    CPMAC_MDIO_TARGET_SWITCH = 0,
    CPMAC_MDIO_TARGET_MAGPIE = 1,
    CPMAC_MDIO_TARGET_NONE
};

typedef struct cpmac_vid_range_struct {
    struct cpmac_vid_range_struct *next;  /* Ptr to next VID range */
    unsigned short                 start; /* Start of VID range */
    unsigned short                 end;   /* End of VID range */
} cpmac_vid_range_t;

typedef struct {
  char               name[AVM_CPMAC_MAX_DEVICE_NAME_LENGTH]; /* Name of device */
  unsigned char      target_mask; /* Mask for the ports that should receive tagged packets */
  cpmac_vid_range_t *vid_range;
} cpmac_device_new_t;

typedef struct {
  unsigned char       number_of_devices;
  unsigned char       wanport;
  cpmac_device_new_t  device[AVM_CPMAC_MAX_DEVS];
} cpmac_device_configuration_t;

typedef struct {
    /* New switch config */
    unsigned char  is_switch;
    unsigned char  ports;
    unsigned char  cpu_port;
    unsigned char  external_port_offset;    /* Which port is the first external port? */
    volatile unsigned short is_being_configured;
    unsigned short used_portset;
    unsigned short mii_portmask;
    unsigned short max_vid_groups;
    unsigned char  mdio_upperhalf_first;
    unsigned char  enable_vlan;
    unsigned char  wanport_keeptags;
    struct {
        unsigned char  keep_tag_outgoing; /* Tag outgoing packets from this port */
        unsigned char  use_tag_incoming;  /* Tag incoming packets into this port if untagged */
        unsigned short vid;               /* Tag untagged incoming packets with this VID */
        unsigned short port_vlan_mask;    /* Configured ports for port VLAN */
    } port[AVM_CPMAC_MAX_PORTS];
    volatile cpmac_fast_forward_rx_func_ptr wan_receive_function;
    unsigned short assigned_vids[CPMAC_MAX_ASSIGNED_VIDS];
    unsigned short assigned_vids_count;
    unsigned short map_portset_out[AVM_CPMAC_MAX_PORTSET + 1]; /* assume max. four ports, possibly shifted by one */
    struct {
        unsigned int   vid       : 12;
        unsigned int   reserved  :  4; 
        unsigned int   portset   :  8;
        unsigned int   port_in   :  4;
        unsigned int   FID       :  2;
        unsigned int   reserved2 :  1; 
        unsigned int   used      :  1;
    } vidgroup[4097];
    unsigned short map_vid_to_group[4096];
    unsigned char map_port_to_dev[AVM_CPMAC_DEFAULT_PORT + 1];
    unsigned char adm_bitshift;
    unsigned char adm_used_bit_combinations; /* Bit 0 = 00, Bit 1 = 01, Bit 2 = 10, Bit 3 = 11 */
    unsigned char          use_EMV;
    unsigned char          devices;
    t_cpmac_device_config  device[AVM_CPMAC_MAX_DEVS + 1];
    struct net_device     *olddevs[AVM_CPMAC_MAX_DEVS];
    volatile enum avm_cpmac_mdio_targets mdio_target;
    volatile enum avm_cpmac_magpie_reset magpie_reset;
    struct {
        unsigned char enable_ingress;
        unsigned char enable_egress;
        unsigned char mirror_from_port;
        unsigned char mirror_to_port;
    } mirror_port;
    struct avm_cpmac_config_struct user_given_config;
    cpmac_device_configuration_t   device_config;

    /* Old switch config */
    /*--- unsigned short map_portmap_out[AVM_CPMAC_MAX_DEVS][1 << CPMAC_MAX_EXTERN_PORTS]; ---*/
    struct {
        unsigned char dev;  /* Port to offset into devs (incoming) */
        unsigned char used; /* Marker, if the field is used */
    } map_in_port_dev[CPMAC_MAX_EXTERN_PORTS];
    /*--- unsigned int      dev_ports[AVM_CPMAC_MAX_DEVS]; ---*/
    unsigned char wanport;
    unsigned short wanport_default_vid;
#   ifdef CONFIG_IP_MULTICAST_FASTFORWARD
    mcfw_portset map_vid_portset[32]; /* Easy fix for Atheros multicasting */
#   endif /*--- #ifdef CONFIG_IP_MULTICAST_FASTFORWARD ---*/
} t_cpphy_switch_config;

#endif /*--- #ifndef _CPPHY_SWITCH_H_ ---*/

