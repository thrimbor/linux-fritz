/*------------------------------------------------------------------------------------------*\
 * Copyright (C) 2006,...,2012 AVM GmbH <fritzbox_info@avm.de>                              *
 *                                                                                          *
 *   This program is free software; you can redistribute it and/or modify                   *
 *   it under the terms of the GNU General Public License as published by                   *
 *   the Free Software Foundation version 2 of the License.                                 *
 *                                                                                          *
 *   This program is distributed in the hope that it will be useful,                        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                           *
 *   GNU General Public License for more details.                                           *
 *                                                                                          *
 *   You should have received a copy of the GNU General Public License                      *
 *   along with this program; if not, write to the Free Software                            *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA              *
\*------------------------------------------------------------------------------------------*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <linux/if_vlan.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include "linux_avm_cpmac.h"


char *usage[] = {
    "Usage: ",
    /*--- "  cpmacconfig info", ---*/
    "  cpmacconfig <device> mode <cpmac mode> [optional VIDs]",
    "                       modes: normal, ata, split, splitata, allports",
    "  cpmacconfig <device> mode special (<dev name>,<port>[,<port>...] [optional VIDs]",
    "                       ports start at 0",
    "  cpmacconfig <device> addporttowan <port> [<optional decimal VID>]",
    "  cpmacconfig <device> settrunkports <portset> <name> <decimal VID range start> <decimal VID range end>",
    "  cpmacconfig <device> power <on interval> <off interval> <5 * (on|off|save)>",
    "  cpmacconfig <device> setreg <hex register> <hex value>",
    "  cpmacconfig <device> getreg <hex register>",
    "  cpmacconfig <device> setphy <hex phy> <hex register> <hex value>",
    "  cpmacconfig <device> getphy <hex phy> <hex register>",
    "  cpmacconfig <device> regdump",
    "  cpmacconfig <device> set_igmp_fwd <portmask>",
    "  cpmacconfig <device> support",
    "  cpmacconfig <device> test <value>",
    "  cpmacconfig <device> peek <location> [length [size]]",
    "  cpmacconfig <device> poke <location> <value> [size]",
    "  cpmacconfig <device> mirror <from_port> <to_port> <enable_ingress> <enable_egress>",
    "                       enable_* should be 0 or 1",
    "  cpmacconfig <device> vlanadd <decimal VID>",
    "                       <device> must be shorter than 16 characters",
    "  cpmacconfig <device> vlanrem",
    0
};

char *modes[] = {
    "illegal",
    "normal", 
    "ata", 
    "split", 
    "split_ata", 
    "all_ports", 
    "special"
};
const unsigned char max_mode = 7;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void show_usage() {
    unsigned int i = 0;
    printf("\ncpmacconfig built on %s at %s\n\n", __DATE__, __TIME__);
    while(usage[i] != 0) {
        puts(usage[i++]);
    }
    printf("SIOCDEVPRIVATE (0x89F0) = %#x, AVM_CPMAC_IOCTL_GENERIC (SIOCDEVPRIVATE + 15) = %#x\n", SIOCDEVPRIVATE, AVM_CPMAC_IOCTL_GENERIC);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int main(int argc, char** argv) {
    unsigned int i, value;
    int fd = 0;
    struct avm_cpmac_ioctl_struct config;
    struct ifreq ifr;
    char *cmd = NULL;

    if(argc < 3) {
        show_usage();
        exit(1);
    }

    if(argc > 1) {
        if(strlen(argv[1]) > 15) {
            fprintf(stderr,"Error: device name must be 15 characters or less.\n");
            goto usage_error;
        }
    }
    if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr,"cpmac_ioctl: socket(2) failed\n");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, argv[1], sizeof(ifr.ifr_name) - 1);
    ifr.ifr_data = (void *) &config;

    cmd = argv[2];

    if(strcasecmp(cmd, "info") == 0) {
        printf("Information about cpmac: \n");

        /*--- config.type = AVM_CPMAC_IOCTL_CONFIG_GET_INFO; ---*/
        /*--- if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0) ---*/
            /*--- goto ioctl_failed; ---*/
        /*--- printf("  Ports: %u internal, %u external\n", config.u.info.internal_ports, ---*/
                                                    /*--- config.u.info.external_ports); ---*/
        config.type = AVM_CPMAC_IOCTL_CONFIG_GET_PHY_POWER;
        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;
        printf("  PHY power: %u0ms on, %u0ms off, status = %s %s %s %s %s\n",
                config.u.phy_power_setup.time_on,
                config.u.phy_power_setup.time_off,
                (config.u.phy_power_setup.mode[0] == ADM_PHY_POWER_ON) ? "on" : ((config.u.phy_power_setup.mode[0] == ADM_PHY_POWER_OFF) ? "off" : "save"),
                (config.u.phy_power_setup.mode[1] == ADM_PHY_POWER_ON) ? "on" : ((config.u.phy_power_setup.mode[1] == ADM_PHY_POWER_OFF) ? "off" : "save"),
                (config.u.phy_power_setup.mode[2] == ADM_PHY_POWER_ON) ? "on" : ((config.u.phy_power_setup.mode[2] == ADM_PHY_POWER_OFF) ? "off" : "save"),
                (config.u.phy_power_setup.mode[3] == ADM_PHY_POWER_ON) ? "on" : ((config.u.phy_power_setup.mode[3] == ADM_PHY_POWER_OFF) ? "off" : "save"),
                (config.u.phy_power_setup.mode[4] == ADM_PHY_POWER_ON) ? "on" : ((config.u.phy_power_setup.mode[4] == ADM_PHY_POWER_OFF) ? "off" : "save"));

        config.type = AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_MODE;
        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;
        if(max_mode <= config.u.switch_mode.cpmac_mode) {
            printf("  Switch mode: unknown (%u)", config.u.switch_mode.cpmac_mode);
        } else {
            printf("  Switch mode: %s (%u) ", modes[config.u.switch_mode.cpmac_mode], config.u.switch_mode.cpmac_mode);
        }
        if(config.u.switch_mode.wan_vlan_number > 0) {
            printf("with VLANs ");
            for(i = 0; i < config.u.switch_mode.wan_vlan_number; i++) {
                printf("%u ", config.u.switch_mode.wan_vlan_id[i]);
            }
        }
        printf("\n");
        config.type = AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_MODE_CURRENT;
        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;
        if(config.u.switch_config.wanport != 0xff) {
            printf("  WAN port %u\n", config.u.switch_config.wanport);
        }
        for(i = 0; i < config.u.switch_config.number_of_devices; i++) {
            printf("  \"%s\": %#2x\n", config.u.switch_config.device[i].name,
                                       config.u.switch_config.device[i].target_mask);
        }

    } else if(strcasecmp(cmd, "mode") == 0) {
        char *mode = argv[3];

        /* Configure switch mode settings */
        if((argc < 4) || (argc > 12))
            goto usage_error;
        if(strcmp(mode, "normal") == 0) {
            config.u.switch_mode.cpmac_mode = CPMAC_MODE_NORMAL;
        } else if (strcmp(mode, "ata") == 0) {
            config.u.switch_mode.cpmac_mode = CPMAC_MODE_ATA;
        } else if (strcmp(mode, "split") == 0) {
            config.u.switch_mode.cpmac_mode = CPMAC_MODE_SPLIT;
        } else if (strcmp(mode, "splitata") == 0) {
            config.u.switch_mode.cpmac_mode = CPMAC_MODE_SPLIT_ATA;
        } else if (strcmp(mode, "allports") == 0) {
            config.u.switch_mode.cpmac_mode = CPMAC_MODE_ALL_PORTS;
        } else if (strcmp(mode, "special") == 0) {
            config.u.switch_mode.cpmac_mode = CPMAC_MODE_SPECIAL;
        }

        if(config.u.switch_mode.cpmac_mode == CPMAC_MODE_SPECIAL) {
            char *ptr = NULL;

            config.type = AVM_CPMAC_IOCTL_CONFIG_SET_SWITCH_MODE_SPECIAL;

            config.u.switch_config.number_of_devices = 0;
            config.u.switch_config.wanport = 0xff;

            for(i = 4; 
                   (i < (unsigned int) argc)
                && (config.u.switch_config.number_of_devices < AVM_CPMAC_MAX_DEVS);
                i++) {
                unsigned char device = config.u.switch_config.number_of_devices;

                ptr = strchr(argv[i], ',');
                if(ptr == NULL)
                    break;
                strncpy(config.u.switch_config.device[device].name, argv[i], AVM_CPMAC_MAX_DEVICE_NAME_LENGTH - 1);
                config.u.switch_config.device[device].target_mask = 0;
                while(*(++ptr) != 0) {
                    char port = atoi(ptr);
                    if((port < 0) || (port >= AVM_CPMAC_MAX_PORTS)) {
                        fprintf(stderr, "Illegal port (%i) given\n", port);
                        goto usage_error;
                    }
                    config.u.switch_config.device[device].target_mask |= 1 << port;
                    ptr = strchr(ptr, ',');
                    if(ptr == NULL)
                        break;
                }
                config.u.switch_config.number_of_devices++;
            }
            if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
                goto ioctl_failed;
            config.u.switch_mode.cpmac_mode = CPMAC_MODE_SPECIAL;
        } else {
            i = 4;
        }
        config.u.switch_mode.wan_vlan_number = 0;
        for( /* keep i */ ; (i < (unsigned int) argc) && (config.u.switch_mode.wan_vlan_number < 4); i++) {
            if(!isdigit(*argv[i])) {
                fprintf(stderr, "\"%s\" is no number", argv[i]);
                goto usage_error;
            }
            config.u.switch_mode.wan_vlan_id[config.u.switch_mode.wan_vlan_number++] = (unsigned short) atoi(argv[i]);
        }

        config.type = AVM_CPMAC_IOCTL_CONFIG_SET_SWITCH_MODE;
        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;

    } else if(strcasecmp(cmd, "addporttowan") == 0) {
        /* Add port to WAN */
        if((argc < 4) || (argc > 5))
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_ADD_PORT_TO_WAN;
        config.u.config_port_vlan.vid = 0; /* Default */
        sscanf(argv[3], "%u", &value);
        config.u.config_port_vlan.port = (unsigned char) value;
        if(argc > 4) {
            sscanf(argv[4], "%x", &value);
            config.u.config_port_vlan.vid = (unsigned short) value;
        }
        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;

    } else if(strcasecmp(cmd, "settrunkports") == 0) {
        /* Set trunk ports */
        if(argc != 7)
            goto usage_error;

        config.type = AVM_CPMAC_IOCTL_CONFIG_SET_TRUNK_PORTS;
        sscanf(argv[3], "%x", &value);
        config.u.trunk.portset = (unsigned char) value;
        strncpy(config.u.trunk.name, argv[4], AVM_CPMAC_MAX_DEVICE_NAME_LENGTH - 1);
        sscanf(argv[5], "%u", &value);
        config.u.trunk.vlan_range_start = (unsigned short) value;
        sscanf(argv[6], "%u", &value);
        config.u.trunk.vlan_range_end = (unsigned short) value;
        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;

    } else if(strcasecmp(cmd, "power") == 0) {
        /* Configure power settings */
        if(argc != 9)
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_SET_PHY_POWER;
        for(i = 3; i <= 4; i++) {
            if(!isdigit(*argv[i])) {
                fprintf(stderr, "\"%s\" is no number", argv[i]);
                goto usage_error;
            }
        }
        config.u.phy_power_setup.time_on  = atoi(argv[3]) / 10;
        config.u.phy_power_setup.time_off = atoi(argv[4]) / 10;
        for(i = 0; i < 5; i++) {
            if(strcasecmp(argv[i + 5], "on") == 0) {
                config.u.phy_power_setup.mode[i] = ADM_PHY_POWER_ON;
            } else if(strcasecmp(argv[i + 5], "off") == 0) {
                config.u.phy_power_setup.mode[i] = ADM_PHY_POWER_OFF;
            } else if(strcasecmp(argv[i + 5], "save") == 0) {
                config.u.phy_power_setup.mode[i] = ADM_PHY_POWER_SAVE;
            } else {
                goto usage_error;
            }
        }
        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;

    } else if(strcasecmp(cmd, "setreg") == 0) {
        /* Set the given switch register */
        if(argc != 5)
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_SET_SWITCH_REGISTER;
        sscanf(argv[3], "%x", &config.u.switch_register.reg);
        sscanf(argv[4], "%x", &config.u.switch_register.value);
        printf("Setting register %#x to %#x\n", config.u.switch_register.reg, config.u.switch_register.value);

        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;

    } else if(strcasecmp(cmd, "getreg") == 0) {
        /* Get the contents of the given switch register */
        if(argc != 4)
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_REGISTER;
        sscanf(argv[3], "%x", &config.u.switch_register.reg);

        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;

        printf("Register %#x = %#x\n", config.u.switch_register.reg, config.u.switch_register.value);
    } else if(strcasecmp(cmd, "setphy") == 0) {
        /* Set the given PHY register */
        if(argc != 6)
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_SET_PHY_REGISTER;
        sscanf(argv[3], "%x", &value);
        config.u.phy_register.phy = (unsigned char) value;
        sscanf(argv[4], "%x", &value);
        config.u.phy_register.reg = (unsigned short) value;
        sscanf(argv[5], "%x", &value);
        config.u.phy_register.value = (unsigned short) value;
        printf("Setting phy %#x register %#x to %#x\n", 
               config.u.phy_register.phy,
               config.u.phy_register.reg, 
               config.u.phy_register.value);

        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;

    } else if(strcasecmp(cmd, "getphy") == 0) {
        /* Get the contents of the given PHY register */
        if(argc != 5)
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_GET_PHY_REGISTER;
        sscanf(argv[3], "%x", &value);
        config.u.phy_register.phy = (unsigned short) value;
        sscanf(argv[4], "%x", &value);
        config.u.phy_register.reg = (unsigned short) value;

        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;

        printf("PHY %#x register %#x = %#x\n", 
               config.u.phy_register.phy, 
               config.u.phy_register.reg, 
               config.u.phy_register.value);
    } else if(strcasecmp(cmd, "regdump") == 0) {
        /* Initiate complete switch register dump */
        if(argc != 3)
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_GET_SWITCH_REGISTER_DUMP;

        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;
    } else if(strcasecmp(cmd, "set_igmp_fwd") == 0) {
        /* Execute set_igmp_fwd function in driver */
        if(argc != 4)
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_SET_IGMP_FWD;
        sscanf(argv[3], "%x", &config.u.igmp_fwd_portmap);

        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;
    } else if(strcasecmp(cmd, "support") == 0) {
        /* Print support data of the driver */
        if(argc != 3)
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_SUPPORT_DATA;

        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;
    } else if(strcasecmp(cmd, "test") == 0) {
        /* Execute test function in driver */
        if(argc != 4)
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_TESTCMD;
        sscanf(argv[3], "%x", &config.u.value);

        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;
    } else if(strcasecmp(cmd, "peek") == 0) {
        /* Print memory location(s) */
        if((argc < 4) || (argc > 6))
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_PEEK;
        sscanf(argv[3], "%x", &value);
        config.u.peek.ptr = (void *) value;
        config.u.peek.size = 4;
        config.u.peek.value = 1;
        if(argc > 4) {
            sscanf(argv[4], "%x", &config.u.peek.value);
            if(argc > 5) {
                sscanf(argv[5], "%x", &config.u.peek.size);
            }
        }
        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;
    } else if(strcasecmp(cmd, "poke") == 0) {
        /* Poke memory location */
        if((argc < 5) || (argc > 6))
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_POKE;
        sscanf(argv[3], "%x", &value);
        config.u.peek.ptr = (void *) value;
        sscanf(argv[4], "%x", &config.u.peek.value);
        config.u.peek.size = 4;
        if(argc > 5) {
            sscanf(argv[5], "%x", &config.u.peek.size);
        }
        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;
    } else if(strcasecmp(cmd, "mirror") == 0) {
        /* Configure port mirror */
        if(argc != 7)
            goto usage_error;
        config.type = AVM_CPMAC_IOCTL_CONFIG_MIRROR_PORT;
        sscanf(argv[3], "%x", &value);
        config.u.mirror_port.mirror_from_port = (unsigned char) value;
        sscanf(argv[4], "%x", &value);
        config.u.mirror_port.mirror_to_port = (unsigned char) value;
        sscanf(argv[5], "%x", &value);
        config.u.mirror_port.enable_ingress = (unsigned char) value;
        sscanf(argv[6], "%x", &value);
        config.u.mirror_port.enable_egress = (unsigned char) value;
        if(ioctl(fd, AVM_CPMAC_IOCTL_GENERIC, (caddr_t) &ifr) < 0)
            goto ioctl_failed;
    } else if(strcasecmp(cmd, "vlanadd") == 0) {
        /* Add VLAN tagged device */
        struct vlan_ioctl_args if_request;

        if(argc != 4)
            goto usage_error;

        memset(&if_request, 0, sizeof(struct vlan_ioctl_args));
        if_request.cmd = ADD_VLAN_CMD;
        if_request.u.name_type = VLAN_NAME_TYPE_PLUS_VID;
        if_request.u.VID = atoi(argv[3]);
        strncpy(if_request.device1, argv[1], 15);
        if(ioctl(fd, SIOCSIFVLAN, &if_request) < 0)
            goto ioctl_failed;
    } else if(strcasecmp(cmd, "vlanrem") == 0) {
        /* Remove VLAN tagged device */
        struct vlan_ioctl_args if_request;

        if(argc != 3)
            goto usage_error;

        memset(&if_request, 0, sizeof(struct vlan_ioctl_args));
        if_request.cmd = DEL_VLAN_CMD;
        strncpy(if_request.device1, argv[1], 15);
        if(ioctl(fd, SIOCSIFVLAN, &if_request) < 0)
            goto ioctl_failed;
    } else {
        goto usage_error;
    }

    close(fd);
    return 0;

ioctl_failed:
    fprintf(stderr, "cpmac_ioctl failed\n");
    goto error_exit;

usage_error:
    fprintf(stderr, "argc = %u\n", argc);
    show_usage();

error_exit:
    if(fd >= 0)
        close(fd);
    return -1;
} /* main */

