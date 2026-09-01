/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,...,2012 AVM GmbH <fritzbox_info@avm.de>
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

/*------------------------------------------------------------------------------------------*\
 * cpmac port management                                                                    *
 *                                                                                          *
 * The idea is to provide a generalized interface to the cpmac driver. Each device can      *
 * return information about the existing hardware and can be used to configure any          *
 * existing switches for VLAN use.                                                          *
 *                                                                                          *
 * To request information about the hardware, send an ioctl AVM_CPMAC_IOCTL_INFO to a       *
 * cpmac device and provide a pointer to a memory region for the port information. The      *
 * answer consists of the number of available devices (eth0...), called internal_ports,     *
 * and the number of external ports of the queried device.                                  *
 *                                                                                          *
 * Configuring the switch hardware is done via sending an AVM_CPMAC_IOCTL_CONFIG to the     *
 * corresponding device. The parts of the configuration structure are:                      *
 *                                                                                          *
 *   groups - Number of VLAN groups, that should be configured. 0 means none.               *
 *   vlan   - Structure to configure the given VLAN groups. The entries consist of          *
 *            id          - VLAN ID, for which information is given                         *
 *            source_mask - mask for ports that should tag packets with the given ID        *
 *            target_mask - mask for ports that should receive packets tagged with the      *
 *                          given ID                                                        *
 *   keep_tag_mask - mask for ports that keep tags on outgoing packets                      *
 *                                                                                          *
 * The same syntax is to be used for getting the current configuration. Provide an empty    *
 * structure to the GET_CONFIG ioctl and it will be filled with the current configuration.  *
 *                                                                                          *
 * Comments: - Avoid assigning more than one source tag per port. Only the last one counts! *
 *           - Ports that should keep their tag should appear in at least one source_mask   *
 *           - On ADM6996FC port 5 is the second MII (e.g. VDSL)                            *
\*------------------------------------------------------------------------------------------*/

#if !defined(AVM_CPMAC_H)
#define AVM_CPMAC_H

#if defined(__KERNEL__)
#include <linux/skbuff.h>
#endif /*--- #if defined(__KERNEL__) ---*/

#define AVM_CPMAC_MAX_MAPPINGS     16u
#define AVM_CPMAC_MAX_PORTS        7u
#define AVM_CPMAC_MAX_DEVS         5u
#define AVM_CPMAC_MAX_DEVICE_NAME_LENGTH 10u

/*
 * !!! Achtung !!!
 *
 * AVM_CPMAC_IOCTL_GENERIC muss zwingend SIOCDEVPRIVATE + 15 bleiben:
 * - Auf IKANOS sind die Werte davor schon belegt. AVM_CPMAC_IOCTL_GENERIC
 *   wird dort in fusiv_src/kernel/drivers/inc/stackios.h direkt definiert.
 *   (Wahrscheinlich kann wegen der 7340 linux/avm_cpmac.h nicht includiert
 *    werden).
 * - Auf PUMA ist AVM_CPMAC_IOCTL_GENERIC bisher direkt in
 *   drivers/net/avalanche_cpgmac_f/cpgmac_f_NetLx.c definiert ...
 */
#define AVM_CPMAC_IOCTL_GENERIC    (SIOCDEVPRIVATE + 15)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/* data format for AVM_CPMAC_IOCTL_INFO */
struct avm_switch_info {
    unsigned int internal_ports; /* Number of overall internal ports */
    unsigned int external_ports; /* Number of external ports for the queried device */
};

/*------------------------------------------------------------------------------------------*\
 * CPMAC-Configuration                                                                      *
 *                                                                                          *
 * The following table lists the existing modi and their consequences for existing device   *
 * nodes on different FBoxes: (examples)                                                    *
 *                                                                                          *
 * Mode        7170             VDSL             ProfiVoIP       6360                       *
 *                                                                                          *
 * NORMAL      eth0             eth0, wan        eth0            eth0,magpie                *
 * ATA         eth0, wan        eth0, wan        eth0, wan       eth0,wan,magpie            *
 * SPLIT       eth0-eth3        eth0-eth3, wan   eth0-eth3       eth0-eth3,magpie           *
 * SPLIT_ATA   eth0-eth2, wan   eth0-eth2, wan   eth0-eth2, wan  eth0-eth2,wan,magpie       *
 * ALL_PORTS   eth0             eth0             eth0            eth0                       *
 * SPECIAL     ???              ???              ???             ???                        *
 *                                                                                          *
 * The ATA modi map the WAN port to an ethernet port. Therefor the connected target of the  *
 * wan device changes in the VDSL case in ATA mode.                                         *
 *                                                                                          *
 * In the ALL_PORTS mode all ports can communicate with each other.                         *
 *                                                                                          *
 * The SPECIAL mode is for special configuration and not preset. To use it call the ioctl   *
 * with type AVM_CPMAC_IOCTL_CONFIG_SET_SWITCH_MODE_SPECIAL to configure the mode, then     *
 * call AVM_CPMAC_IOCTL_CONFIG_SET_SWITCH_MODE with the SPECIAL mode.                       *
 *                                                                                          *
 * In configurations that provide a WAN port, VLANs can be routed through them.             *
\*------------------------------------------------------------------------------------------*/

enum avm_cpmac_modes {
    CPMAC_MODE_NORMAL    = 1,
    CPMAC_MODE_ATA       = 2,
    CPMAC_MODE_SPLIT     = 3,
    CPMAC_MODE_SPLIT_ATA = 4,
    CPMAC_MODE_ALL_PORTS = 5,
    CPMAC_MODE_SPECIAL   = 6,
    CPMAC_MODE_MAX_NO    = 7
};

enum avm_cpmac_switch_type_configurations {
    CPMAC_SWITCH_CONF_7170         = 0,
    CPMAC_SWITCH_CONF_VINAX5       = 1,
    CPMAC_SWITCH_CONF_VINAX7       = 2,
    CPMAC_SWITCH_CONF_PROFIVOIP2   = 3,
    CPMAC_SWITCH_CONF_PROFIVOIP1   = 4,
    CPMAC_SWITCH_CONF_AR8216       = 5,
    CPMAC_SWITCH_CONF_MAGPIE       = 6,
    CPMAC_SWITCH_CONF_MAX_PRODUCTS = 7
};

struct avm_cpmac_config_struct {
    enum avm_cpmac_modes cpmac_mode;       /* Mode of the cpmac as described above */
    unsigned short       wan_vlan_number;  /* Number of VLAN IDs to be passed through WAN */
    unsigned short       wan_vlan_id[4];   /* VLAN IDs to be passed through WAN */
};


/*------------------------------------------------------------------------------------------*\
 * Structure to define switch configuration                                                 *
 *                                                                                          *
 *  number_of_devices - Number of devices defined in this structure                         *
 *  wanport           - Port that is used for WAN; 0xff means that no ethernet port is WAN  *
 *  device            - List of devices with name for the netdevice and target mask for the *
 *                      switch ports to be used                                             *
\*------------------------------------------------------------------------------------------*/
typedef struct {
  char               name[AVM_CPMAC_MAX_DEVICE_NAME_LENGTH]; /* Name of device */
  unsigned char      target_mask; /* Mask for the ports that should receive tagged packets */
} cpmac_device_struct;

typedef struct {
  unsigned char number_of_devices;
  unsigned char wanport;
  cpmac_device_struct device[AVM_CPMAC_MAX_DEVS];
} cpmac_switch_configuration_t;


/* If no portset is given, only the VID range of the given device is replaced. */
typedef struct {
    unsigned char  portset;                                /* Portset for trunk (bits 0-3) */
    char           name[AVM_CPMAC_MAX_DEVICE_NAME_LENGTH]; /* Name of trunk port device */
    unsigned short vlan_range_start;                       /* First VID in range for trunk port */
    unsigned short vlan_range_end;                         /* Last VID in range for trunk port */
} cpmac_trunk_port_configuration_t;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum cpmac_adm_phy_power_mode {
    ADM_PHY_POWER_OFF = 0,
    ADM_PHY_POWER_SAVE = 1,
    ADM_PHY_POWER_ON = 2,
    ADM_PHY_POWER_NUMBER_OF_MODES = 3
};

typedef struct {
    unsigned int                  time_on;
    unsigned int                  time_off;
    enum cpmac_adm_phy_power_mode mode[AVM_CPMAC_MAX_PORTS];
} adm_phy_power_setup_t;

/*------------------------------------------------------------------------------------------*\
 * Configuration structure for all cpmac related ioctl configuration                        *
 *                                                                                          *
 *   type             - Type of configuration included in this structure                    *
 *                                                                                          *
 *   switch_mode      - Mode of the switch with information about used VIDs                 *
 *                                                                                          *
 *   switch_config    - Used for special mode, that is freely configurable                  *
 *                                                                                          *
 *   switch_register  - Direct access to switch registers                                   *
 *                                                                                          *
 *   phy_power_setup  - Configuration of the power policy for the switch PHYs               *
 *                                                                                          *
 *   config_port_vlan - Add LAN port to the VLAN of the WAN                                 *
 *                                                                                          *
\*------------------------------------------------------------------------------------------*/

#define AVM_CPMAC_IOCTL_CONFIG_GET_INFO                 0x01u
#define AVM_CPMAC_IOCTL_CONFIG_SET_SWITCH_MODE          0x02u
#define AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_MODE          0x03u
#define AVM_CPMAC_IOCTL_CONFIG_SET_SWITCH_MODE_SPECIAL  0x04u
#define AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_MODE_CURRENT  0x05u
#define AVM_CPMAC_IOCTL_CONFIG_SET_SWITCH_REGISTER      0x06u
#define AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_REGISTER      0x07u
#define AVM_CPMAC_IOCTL_CONFIG_SET_PHY_POWER            0x08u
#define AVM_CPMAC_IOCTL_CONFIG_GET_PHY_POWER            0x09u
#define AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_REGISTER_DUMP 0x0au
#define AVM_CPMAC_IOCTL_CONFIG_TESTCMD                  0x0bu
#define AVM_CPMAC_IOCTL_CONFIG_GET_WAN_KEEPTAG          0x0cu
#define AVM_CPMAC_IOCTL_CONFIG_GET_PORT_OUT_VID_MAP     0x0du
#define AVM_CPMAC_IOCTL_CONFIG_SET_PHY_REGISTER         0x0eu
#define AVM_CPMAC_IOCTL_CONFIG_GET_PHY_REGISTER         0x0fu
#define AVM_CPMAC_IOCTL_CONFIG_GET_BYTES_IN_WAN         0x10u
#define AVM_CPMAC_IOCTL_CONFIG_SET_PPPOA                0x11u
#define AVM_CPMAC_IOCTL_SUPPORT_DATA                    0x12u
#define AVM_CPMAC_IOCTL_CONFIG_ADD_PORT_TO_WAN          0x13u
#define AVM_CPMAC_IOCTL_CONFIG_PEEK                     0x14u
#define AVM_CPMAC_IOCTL_CONFIG_POKE                     0x15u
#define AVM_CPMAC_IOCTL_CONFIG_SET_WAN_DUP              0x16u
#define AVM_CPMAC_IOCTL_CONFIG_MIRROR_PORT              0x17u
#define AVM_CPMAC_IOCTL_CONFIG_SET_IGMP_FWD             0x18u
#define AVM_CPMAC_IOCTL_CONFIG_SET_TRUNK_PORTS          0x19u
#define AVM_CPMAC_IOCTL_NUMBER_OF_IOCTLS                0x20u


struct avm_cpmac_ioctl_struct {
    unsigned int type; /* Type of configuration */
    union {
        struct avm_switch_info info;                /* GET_INFO */
        struct avm_cpmac_config_struct switch_mode; /* SET_SWITCH_MODE, GET_SWITCH_MODE */
        cpmac_switch_configuration_t switch_config; /* SET_SWITCH_MODE_SPECIAL, GET_SWITCH_MODE_CURRENT */
        struct {
            unsigned int reg, value;
        } switch_register;                          /* SET_SWITCH_REGISTER, GET_SWITCH_REGISTER */
        struct {
            unsigned short phy, reg, value;
        } phy_register;                             /* SET_PHY_REGISTER, GET_PHY_REGISTER */
        adm_phy_power_setup_t phy_power_setup;      /* SET_PHY_POWER, GET_PHY_POWER */
        unsigned int result;                        /* GET_WAN_KEEPTAG, GET_BYTES_IN_WAN */
        unsigned short port_out_vid_map[4];         /* GET_PORT_OUT_VID_MAP */
        unsigned int value;                         /* GET_SWITCH_REGISTER_DUMP, SET_PPPOA */
        struct {
            unsigned char port;
            unsigned short vid;
        } config_port_vlan;                         /* ADD_PORT_TO_WAN */
        struct {
            void         *ptr;
            unsigned int  size;
            unsigned int  value;
        } peek;                                     /* PEEK, POKE */
        struct {
            unsigned char enable_ingress;
            unsigned char enable_egress;
            unsigned char mirror_from_port;
            unsigned char mirror_to_port;
        } mirror_port;                              /* MIRROR_PORT */
        unsigned int igmp_fwd_portmap;              /* SET_IGMP_FWD */
        cpmac_trunk_port_configuration_t trunk;     /* ADD_TRUNK_PORT */
    } u;
};



#if defined(__KERNEL__)
/*------------------------------------------------------------------------------------------*\
 * Functions and types available to other entities in the kernel
\*------------------------------------------------------------------------------------------*/

/* Fast forwarding functions for WAN and magpie */
typedef void (*cpmac_fast_forward_rx_func_ptr)(struct sk_buff *skb);
void cpmac_register_wan_receive(cpmac_fast_forward_rx_func_ptr wan_receive);
void cpmac_unregister_wan_receive(void);

unsigned int cpmac_register_magpie_receive(cpmac_fast_forward_rx_func_ptr magpie_receive);
void cpmac_unregister_magpie_receive(void);


/* Traffic class management to make it possible to prevent dropping of essential packets */
#define CPMAC_TRAFFIC_CLASSES_AVAILABLE
enum cpmac_traffic_classes {
    CPMAC_TRAFFIC_ESSENTIAL         = 0,
    CPMAC_TRAFFIC_IMPORTANT         = 1,
    CPMAC_TRAFFIC_NORMAL            = 2,
    CPMAC_TRAFFIC_OTHER             = 3,
    CPMAC_TRAFFIC_NUMBER_OF_CLASSES = 4
};

typedef enum cpmac_traffic_classes (*cpmac_wlan_classify_func_ptr)(struct sk_buff *skb);
unsigned int cpmac_register_wlan_classify(cpmac_wlan_classify_func_ptr wlan_classify);
void cpmac_unregister_wlan_classify(void);


#define CPMACWAN_SET_WAN_KEEP_TAGGING_AVAILABLE
void cpmacwan_set_wan_keep_tagging(struct net_device *dev, int enable);

/* Callbacks for default VID changes of devices with the given name */
/* A given VID of 0 means that there is no device with this name */
typedef void (*cpmac_device_vlan_change_cb)(char *devicename, 
                                            unsigned short VID,
                                            void *context);
unsigned int cpmac_register_device_vlan_change_cb(char *devicename, 
                                                  cpmac_device_vlan_change_cb cb,
                                                  void *context);
unsigned int cpmac_unregister_device_vlan_change_cb(char *devicename, 
                                                    cpmac_device_vlan_change_cb cb,
                                                    void *context);


/* Controlling function of and access to magpie */
enum avm_cpmac_magpie_reset {
    CPMAC_MAGPIE_RESET_ON = 0,
    CPMAC_MAGPIE_RESET_OFF = 1,
    CPMAC_MAGPIE_RESET_PULSE = 2
};

void cpmac_magpie_reset(enum avm_cpmac_magpie_reset value);
void cpmac_magpie_mdio_write(unsigned short regadr,
                             unsigned short phyadr,
                             unsigned short data);
unsigned int cpmac_magpie_mdio_read(unsigned short regadr,
                                    unsigned short phyadr);


void avm_cpmac_mirror_port(unsigned char enable_ingress,
                           unsigned char enable_egress,
                           unsigned char mirror_from_port,
                           unsigned char mirror_to_port);

unsigned int avm_cpmac_get_wan_port_vlan(void);

char *cpmac_device_name_get(unsigned int device_number);
unsigned int cpmac_device_name_cmp(char *name, unsigned int device_number);
unsigned int cpmac_get_number_of_instances(void);

#endif /*--- #if defined(__KERNEL__) ---*/

#endif /*--- #if !defined(AVM_CPMAC_H) ---*/

