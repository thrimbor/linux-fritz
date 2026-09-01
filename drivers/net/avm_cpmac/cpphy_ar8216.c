/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2008,...,2013 AVM GmbH <fritzbox_info@avm.de>
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
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>
#include <asm/atomic.h>
#include <linux/version.h>
#if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE)
#include <linux/env.h>
#else /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
#include <asm/mips-boards/prom.h>
#endif /*--- #else ---*/ /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
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
#include "cpphy_const.h"
#include "cpphy_types.h"
#include "cpphy_mdio.h"
#include "cpphy_adm6996.h"
#include "cpphy_ar8216.h"
#include "cpphy_mgmt.h"
#include "cpmac_fusiv_if.h"
#include "cpmac_eth.h"
#include "mii_reg.h"

/*------------------------------------------------------------------------------------------*\
 * The configuration is defined in cpphy_adm6996.c                                          *
\*------------------------------------------------------------------------------------------*/
extern cpmac_switch_configuration_t switch_configuration[CPMAC_SWITCH_CONF_MAX_PRODUCTS][CPMAC_MODE_MAX_NO];

/*------------------------------------------------------------------------------------------*\
 * die MDIO-Register des AR8216 sind 32bit gross, MDIO kann nur 16bit Zugriffe, d.h.
 * es muessen 2 Zugriffe stattfinden
 * zuerst wird die Registerpage ausgewaehlt, da die Registeradressen des AR8216 groesser als
 * der MDIO-Adressraum ist
\*------------------------------------------------------------------------------------------*/
unsigned int ar8216_mdio_read32(cpphy_mdio_t *mdio, unsigned int address) {
    unsigned int tmp, tmp1;
    unsigned int reg_addr;

    reg_addr = (address & 0xfffffffc);
    mdio_write(mdio, 0x18, 0, (reg_addr >> 9) & 0x3ff);

    /* Attention! Reading 32 bit self clearing registers has to take into
     * account whether the upper or lower word triggers this clearing action.
     * Upto now it looks like the upper word triggers the clearing, both on
     * AR8316 and similar and on AR8327. */
    tmp = mdio_read(mdio, 0x10 | ((reg_addr >> 6) & 0x7), (reg_addr >> 1) & 0x1F);
    /*--- DEB_TRC("[%s]: low16bit 0x%x\n", __FUNCTION__, tmp); ---*/
    tmp1 = mdio_read(mdio, 0x10 | ((reg_addr >> 6) & 0x7), ((reg_addr >> 1) & 0x1F) + 1);
    /*--- DEB_TRC("[%s]: high16bit 0x%x\n", __FUNCTION__, tmp1); ---*/

    return (tmp | (tmp1 << 16));
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned short ar8216_mdio_read16(cpphy_mdio_t *mdio, unsigned short regadr) {
    unsigned short register_address, phy_address;

    register_address = (regadr & 0xFFFFFFFC) >> 1;
    phy_address = 0x10 | ((register_address >> 5) & 0x7);

    return mdio_read(mdio, phy_address, register_address & 0x1F);
}


/*------------------------------------------------------------------------------------------*\
 * zuerst die oberen 16bit schreiben
\*------------------------------------------------------------------------------------------*/
int ar8216_mdio_write32(cpphy_mdio_t *mdio, unsigned int address, unsigned int data) {
    unsigned int reg_addr;

    if(unlikely(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE))) {
        return -1;
    }

    reg_addr = (address & 0xfffffffc);
    mdio_write(mdio, 0x18, 0, (reg_addr >> 9) & 0x3ff);

    if(mdio->switch_config.mdio_upperhalf_first) {
        mdio_write(mdio, 0x10 | ((reg_addr >> 6) & 0x7), ((reg_addr >> 1) & 0x1F) + 1, (unsigned short) (data >> 16));
        mdio_write(mdio, 0x10 | ((reg_addr >> 6) & 0x7), (reg_addr >> 1) & 0x1F, data & 0xFFFF);
    } else {
        mdio_write(mdio, 0x10 | ((reg_addr >> 6) & 0x7), (reg_addr >> 1) & 0x1F, data & 0xFFFF);
        mdio_write(mdio, 0x10 | ((reg_addr >> 6) & 0x7), ((reg_addr >> 1) & 0x1F) + 1, (unsigned short) (data >> 16));
    }

    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar8216_mdio_write16(cpphy_mdio_t *mdio, unsigned short regadr, unsigned short data) {
    unsigned short register_address, phy_address;

    register_address = (regadr & 0xFFFFFFFC) >> 1;
    phy_address = 0x10 | ((register_address >> 5) & 0x7);

    mdio_write(mdio, phy_address, register_address & 0x1F, data & 0xFFFF);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static char *cnt8216names[] = {
                               "RxBroad",
                               "RxPause",
                               "RxMulti",
                               "RxFcsErr",
                               "RxAlignErr",
                               "RxRunt",
                               "RxFragment",
                               "Rx64Byte",
                               "Rx128Byte",
                               "Rx256Byte",
                               "Rx512Byte",
                               "Rx1024Byte",
                               "RxMaxByte",
                               "RxTooLong",
                               "RxGoodByteLow",
                               "RxGoodByteHigh",
                               "RxBadByteLow",
                               "RxBadByteHigh",
                               "RxOverflow",
                               "Filtered",
                               "TxBroad",
                               "TxPause",
                               "TxMulti",
                               "TxUnderRun",
                               "Tx64Byte",
                               "Tx128Byte",
                               "Tx256Byte",
                               "Tx512Byte",
                               "Tx1024Byte",
                               "TxMaxByte",
                               "TxOverSize",
                               "TxByteLow",
                               "TxByteHigh",
                               "TxCollision",
                               "TxAbortCol",
                               "TxMultiCol",
                               "TxSingleCol",
                               "TxExcDefer",
                               "TxDefer",
                               "TxLateCol"
                              };
void ar8216_dump_counters(cpphy_mdio_t *mdio) {
    unsigned int i;

    DEB_SUPPORT("Counters\n");
    DEB_SUPPORT("                    CPU port      Port 1      Port 2      Port 3      Port 4      Port 5\n");
    for(i = 0; i <= 0x9c; i += 4) {
        DEB_SUPPORT("  %14s  %10.u  %10.u  %10.u  %10.u  %10.u  %10.u\n", 
                    cnt8216names[i >> 2],
                    ar8216_mdio_read32(mdio, 0x19000 + i),
                    ar8216_mdio_read32(mdio, 0x190A0 + i),
                    ar8216_mdio_read32(mdio, 0x19140 + i),
                    ar8216_mdio_read32(mdio, 0x191E0 + i),
                    ar8216_mdio_read32(mdio, 0x19280 + i),
                    ar8216_mdio_read32(mdio, 0x19320 + i));
    }
    DEB_SUPPORT("\n");
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static char *cnt8316names[] = {
                               /* 0x00 */    "RxBroad",
                               /* 0x04 */    "RxPause",
                               /* 0x08 */    "RxMulti",
                               /* 0x0c */    "RxFcsErr",
                               /* 0x10 */    "RxAlignErr",
                               /* 0x14 */    "RxRunt",
                               /* 0x18 */    "RxFragment",
                               /* 0x1c */    "Rx64Byte",
                               /* 0x20 */    "Rx128Byte",
                               /* 0x24 */    "Rx256Byte",
                               /* 0x28 */    "Rx512Byte",
                               /* 0x2c */    "Rx1024Byte",
                               /* 0x30 */    "Rx1518Byte",
                               /* 0x34 */    "RxMaxByte",
                               /* 0x38 */    "RxTooLong",
                               /* 0x3c */    "RxGoodByteLow",
                               /* 0x40 */    "RxGoodByteHigh",
                               /* 0x44 */    "Reserved",
                               /* 0x48 */    "Reserved",
                               /* 0x4c */    "RxOverflow",
                               /* 0x50 */    "Filtered",
                               /* 0x54 */    "TxBroad",
                               /* 0x58 */    "TxPause",
                               /* 0x5c */    "TxMulti",
                               /* 0x60 */    "TxUnderRun",
                               /* 0x64 */    "Tx64Byte",
                               /* 0x68 */    "Tx128Byte",
                               /* 0x6c */    "Tx256Byte",
                               /* 0x70 */    "Tx512Byte",
                               /* 0x74 */    "Tx1024Byte",
                               /* 0x78 */    "Tx1518Byte",
                               /* 0x7c */    "TxMaxByte",
                               /* 0x80 */    "TxOverSize",
                               /* 0x84 */    "TxByteLow",
                               /* 0x88 */    "TxByteHigh",
                               /* 0x8c */    "TxCollision",
                               /* 0x90 */    "TxAbortCol",
                               /* 0x94 */    "TxMultiCol",
                               /* 0x98 */    "TxSingleCol",
                               /* 0x9c */    "TxExcDefer",
                               /* 0xa0 */    "TxDefer",
                               /* 0xa4 */    "TxLateCol"
                              };
void ar8316_dump_counters(cpphy_mdio_t *mdio) {
    unsigned int i;

    DEB_SUPPORT("Counters\n");
    DEB_SUPPORT("                    CPU port      Port 1      Port 2      Port 3      Port 4      Port 5\n");
    for(i = 0; i <= 0xa4; i += 4) {
        DEB_SUPPORT("  %14s  %10.u  %10.u  %10.u  %10.u  %10.u  %10.u\n", 
                    cnt8316names[i >> 2],
                    ar8216_mdio_read32(mdio, 0x20000 + i),
                    ar8216_mdio_read32(mdio, 0x20100 + i),
                    ar8216_mdio_read32(mdio, 0x20200 + i),
                    ar8216_mdio_read32(mdio, 0x20300 + i),
                    ar8216_mdio_read32(mdio, 0x20400 + i),
                    ar8216_mdio_read32(mdio, 0x20500 + i));
    }
    DEB_SUPPORT("\n");
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar8327_dump_counters(cpphy_mdio_t *mdio) {
    unsigned int i;

    DEB_SUPPORT("Counters\n");
    DEB_SUPPORT("                    CPU port      Port 1      Port 2      Port 3      Port 4      Port 5\n");
    for(i = 0; i <= 0xa4; i += 4) {
        DEB_SUPPORT("  %14s  %10.u  %10.u  %10.u  %10.u  %10.u  %10.u\n", 
                    cnt8316names[i >> 2],
                    ar8216_mdio_read32(mdio, 0x01000 + i),
                    ar8216_mdio_read32(mdio, 0x01100 + i),
                    ar8216_mdio_read32(mdio, 0x01200 + i),
                    ar8216_mdio_read32(mdio, 0x01300 + i),
                    ar8216_mdio_read32(mdio, 0x01400 + i),
                    ar8216_mdio_read32(mdio, 0x01500 + i));
    }
    DEB_SUPPORT("\n");
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void display_mdio_regs(cpphy_mdio_t *mdio) {
    unsigned short reg;

    DEB_SUPPORT("MII registers (all values in hex)\n");
    DEB_SUPPORT("                      Port 1    Port 2    Port 3    Port 4    Port 5\n");
    for(reg = 0; reg <= 0x1E; reg++) {
        DEB_SUPPORT("  (0x%02x)            %8.x  %8.x  %8.x  %8.x  %8.x\n", 
                    reg,
                    mdio_read(mdio, 0, reg),
                    mdio_read(mdio, 1, reg),
                    mdio_read(mdio, 2, reg),
                    mdio_read(mdio, 3, reg),
                    mdio_read(mdio, 4, reg)
                   );
    }
    DEB_SUPPORT("\n");
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define LENGTH 256
static inline void print_reg_line(cpphy_mdio_t *mdio,
                                  char *string, 
                                  char *name,
                                  unsigned short base_reg,
                                  unsigned short step) {
    unsigned int length = 0;
    unsigned short port;

    length += snprintf(string, LENGTH, "%12s ", name);
    length += snprintf(string + length, LENGTH - length, "(%#.03x + %#.02x)  ", base_reg, step);
    for(port = 0; port < 7; port++) {
        length += snprintf(string + length, LENGTH - length, "%8.x  ", ar8216_mdio_read32(mdio, base_reg + step * port));
    }
    DEB_SUPPORT("%s\n", string);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int regs_8216[] = {0, 0x4, 0x8, 0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x2c,
                   0x30, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x50, 0x54, 0x58,
                   0x5C, 0x60, 0x64, 0x68, 0x6C, 0x70, 0x74, 0x78, 0x80,
                   0x94, 0x98, 0xb0, 0xb4, 0xb8, 0xbc, -1};
static int regs_8327[] = {0, 0x4, 0x8, 0xc, 0x10, 0x20, 0x24, 0x28, 0x2c, 0x30,
                   0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x50, 0x54, 0x58,
                   0x5C, 0x60, 0x64, 0x78, 0x98, 0xe0, 0xe4,
                   0x618, 0x620, 0x624,
                   -1};

void display_ath_regs(cpphy_mdio_t *mdio, enum ath_display_regs control) {
    char *string;
    int *regs;
    int i = 0;
    unsigned short port, reg, control_reg;

    string = kmalloc(LENGTH, GFP_KERNEL);
    if(string == NULL) {
        DEB_WARN("[%s] Out of memory!\n", __FUNCTION__);
        return;
    }
    switch(control) {
        case ATH_PORT_REGISTERS:
            switch(cpmac_global.cpphy[mdio->phy_num].config->type) {
                case CPMAC_PHY_TYPE_AR8327:
                    DEB_SUPPORT("Port registers (all values in hex)\n");
                    DEB_SUPPORT("                             CPU port    Port 1    Port 2    Port 3    Port 4    Port 5    Port 6\n");
                    print_reg_line(mdio, string, "status", 0x7c, 4);
                    print_reg_line(mdio, string, "hdr_ctl", 0x9c, 4);
                    print_reg_line(mdio, string, "vlan_ctl0", 0x420, 8);
                    print_reg_line(mdio, string, "vlan_ctl1", 0x424, 8);
                    print_reg_line(mdio, string, "lookup_ctl", 0x660, 12);
                    print_reg_line(mdio, string, "pri_ctl", 0x664, 12);
                    print_reg_line(mdio, string, "learn_limit", 0x668, 12);
                    DEB_SUPPORT("\n");
                    break;
                case CPMAC_PHY_TYPE_AR8216:
                case CPMAC_PHY_TYPE_AR8316:
                case CPMAC_PHY_TYPE_AR8226:
                default:
                    DEB_SUPPORT("Port control registers (all values in hex)\n");
                    DEB_SUPPORT("          CPU port    Port 1    Port 2    Port 3    Port 4    Port 5\n");
                    DEB_SUPPORT("             0x100     0x200     0x300     0x400     0x500     0x600\n");
                    for(control_reg = 0; control_reg < 0x20; control_reg += 4) {
                        unsigned int length = 0;

                        length += snprintf(string, LENGTH, "  (0x%02x)  ", control_reg);

                        for(port = 0x100; port < 0x700; port += 0x100) {
                            length += snprintf(string + length, LENGTH - length, "%8.x  ", ar8216_mdio_read32(mdio, port + control_reg));
                        }
                        DEB_SUPPORT("%s\n", string);
                    }
                    DEB_SUPPORT("\n");
                    break;
            }
            break;
        case ATH_CONTROL:
            DEB_SUPPORT("Global control registers (all values in hex)\n");
            switch(cpmac_global.cpphy[mdio->phy_num].config->type) {
                case CPMAC_PHY_TYPE_AR8327:
                    regs = regs_8327;
                    break;
                case CPMAC_PHY_TYPE_AR8216:
                case CPMAC_PHY_TYPE_AR8316:
                case CPMAC_PHY_TYPE_AR8226:
                default:
                    regs = regs_8216;
                    break;
            }
            while (regs[i] != -1) {
                DEB_SUPPORT("  0x%3.x  %8.x\n", regs[i], ar8216_mdio_read32(mdio, regs[i]));
                i++;
            }
            DEB_SUPPORT("\n");
            break;
        case ATH_MII:
            display_mdio_regs(mdio);
            DEB_SUPPORT("MDIO Debug registers\n");
            DEB_SUPPORT("                      Port 1    Port 2    Port 3    Port 4    Port 5\n");
            for(reg = 0; reg < 0x15; reg++) {
                if(reg == 0x11)
                    continue;
                for(port = 0; port < 5; port++) {
                    mdio_write(mdio, port, 0x1d, reg);
                }
                DEB_SUPPORT("  (0x%02x)            %8.x  %8.x  %8.x  %8.x  %8.x\n", 
                        reg,
                        mdio_read(mdio, 0, 0x1e),
                        mdio_read(mdio, 1, 0x1e),
                        mdio_read(mdio, 2, 0x1e),
                        mdio_read(mdio, 3, 0x1e),
                        mdio_read(mdio, 4, 0x1e)
                        );
                if(reg == 3)
                    reg = 0x10 - 1;
            }
            DEB_SUPPORT("\n");
            break;
    }
    kfree(string);
#   undef LENGTH
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ar8216_wait_VLAN_table(cpphy_mdio_t *mdio) {
    DEB_DEBUG("[%s]\n", __FUNCTION__);
    do {
        schedule();
    } while(VLAN_TABLE_1_VT_BUSY & ar8216_mdio_read32(mdio, AR8216_GLOBAL_VLAN_TABLE_1));
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ar8327_wait_VLAN_table(cpphy_mdio_t *mdio) {
    DEB_DEBUG("[%s]\n", __FUNCTION__);
    do {
        schedule();
    } while(AR8327_VTU_FUNC1_BUSY & ar8216_mdio_read32(mdio, AR8327_VTU_FUNC1));
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ar8316_read_VLAN_table(cpphy_mdio_t *mdio) {
    unsigned short vid = 0;
    unsigned int RegData;

    ar8216_wait_VLAN_table(mdio);
    DEB_SUPPORT("VLAN table\n");
    DEB_SUPPORT("  VID           port map\n");
    for( ;; ) {
        unsigned char valid;
        unsigned short members;

        ar8216_mdio_write32(mdio, 
                            AR8216_GLOBAL_VLAN_TABLE_1,
                              VLAN_TABLE_1_VT_FUNC_VALUE(VLAN_TABLE_FUNC_READ_NEXT_ENTRY)
                            | VLAN_TABLE_1_VT_BUSY
                            | VLAN_TABLE_1_VID_VALUE(vid)
                           );
        ar8216_wait_VLAN_table(mdio);
        RegData = ar8216_mdio_read32(mdio, AR8216_GLOBAL_VLAN_TABLE_1);
        vid = ((RegData & VLAN_TABLE_1_VID_MASK) >> VLAN_TABLE_1_VID_SHIFT) & 0xfff;
        if(vid == 0) {
            break; /* Done */
        }
        RegData = ar8216_mdio_read32(mdio, AR8216_GLOBAL_VLAN_TABLE_2);
        valid = (RegData & VLAN_TABLE_2_VT_VALID) ? 1 : 0;
        members = (RegData & VLAN_TABLE_2_VID_MEM_MASK) >> VLAN_TABLE_2_VID_MEM_SHIFT;
        DEB_SUPPORT("  %04u (%#03x)  ports %#02x %s\n", vid, vid, members, valid ? "" : "not valid!");
    }
    DEB_SUPPORT("\n");
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ar8327_read_VLAN_table(cpphy_mdio_t *mdio) {
    unsigned short vid = 0;
    unsigned int RegData0, RegData1;
    char modes[4] = {'O', 'U', 'T', '-'};

    ar8327_wait_VLAN_table(mdio);
    DEB_SUPPORT("VLAN table\n");
    DEB_SUPPORT("  (port: %c = unmodified, %c = untagged, %c = tagged, %c = not member)\n",
                modes[0], modes[1], modes[2], modes[3]);
    DEB_SUPPORT("  VID           port map\n");
               
    for( ;; ) {
        ar8216_mdio_write32(mdio, 
                            AR8327_VTU_FUNC1,
                            0
                            | AR8327_VTU_FUNC1_BUSY
                            | AR8327_VTU_FUNC1_FUNC_VALUE(AR8327_VTU_FUNC1_FUNC_VALUE_GET_NEXT)
                            | AR8327_VTU_FUNC1_VID_VALUE(vid));

        RegData0 = ar8216_mdio_read32(mdio, AR8327_VTU_FUNC0);
        /*--- DEB_TEST("[%s] RegData0 = %#x\n", __FUNCTION__, RegData0); ---*/
        RegData1 = ar8216_mdio_read32(mdio, AR8327_VTU_FUNC1);
        vid = ((RegData1 & AR8327_VTU_FUNC1_VID_MASK) >> AR8327_VTU_FUNC1_VID_SHIFT) & 0xfff;
        /*--- DEB_TEST("[%s] RegData1 = %#x\n", __FUNCTION__, RegData1); ---*/
        if((RegData1 & (AR8327_VTU_FUNC1_BUSY | AR8327_VTU_FUNC1_VID_MASK)) == 0) {
            break; /* Done */
        }
        /*--- DEB_SUPPORT("  %04u (%#03x)  ports %#02x %s\n", vid, vid, members, valid ? "" : "not valid!"); ---*/
        DEB_SUPPORT("  %04u (%#03x)  %c%c%c%c%c%c%c\n",
                    vid,
                    vid,
                    modes[(RegData0 & AR8327_VTU_FUNC0_EG_VLAN_MODE_MASK(6)) >> AR8327_VTU_FUNC0_EG_VLAN_MODE_SHIFT(6)],
                    modes[(RegData0 & AR8327_VTU_FUNC0_EG_VLAN_MODE_MASK(5)) >> AR8327_VTU_FUNC0_EG_VLAN_MODE_SHIFT(5)],
                    modes[(RegData0 & AR8327_VTU_FUNC0_EG_VLAN_MODE_MASK(4)) >> AR8327_VTU_FUNC0_EG_VLAN_MODE_SHIFT(4)],
                    modes[(RegData0 & AR8327_VTU_FUNC0_EG_VLAN_MODE_MASK(3)) >> AR8327_VTU_FUNC0_EG_VLAN_MODE_SHIFT(3)],
                    modes[(RegData0 & AR8327_VTU_FUNC0_EG_VLAN_MODE_MASK(2)) >> AR8327_VTU_FUNC0_EG_VLAN_MODE_SHIFT(2)],
                    modes[(RegData0 & AR8327_VTU_FUNC0_EG_VLAN_MODE_MASK(1)) >> AR8327_VTU_FUNC0_EG_VLAN_MODE_SHIFT(1)],
                    modes[(RegData0 & AR8327_VTU_FUNC0_EG_VLAN_MODE_MASK(0)) >> AR8327_VTU_FUNC0_EG_VLAN_MODE_SHIFT(0)]);
    }
    DEB_SUPPORT("\n");
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void switch_dump_atheros(cpphy_mdio_t *mdio) {
    display_ath_regs(mdio, ATH_PORT_REGISTERS);
    display_ath_regs(mdio, ATH_CONTROL);
    display_ath_regs(mdio, ATH_MII);
    if(mdio->f->dump_counters) {
        mdio->f->dump_counters(mdio);
    }
    if(cpmac_global.cpphy[mdio->phy_num].config->type == CPMAC_PHY_TYPE_AR8327) {
        ar8327_read_VLAN_table(mdio);
    } else {
        ar8316_read_VLAN_table(mdio);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ar8216_softreset(cpphy_mdio_t *mdio) {
    unsigned int tmp = (1<<31);

    DEB_TRC("[%s] softreset switch\n", __FUNCTION__);
    ar8216_mdio_write32(mdio, AR8216_GLOBAL_DEVICE_ID, tmp);
    mdelay(200);

    while (tmp & (1 << 31)) {
        schedule();
        tmp = ar8216_mdio_read32(mdio, AR8216_GLOBAL_DEVICE_ID);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ar8216_del_vlan_vid(cpphy_mdio_t *mdio, unsigned int vid) {
    ar8216_wait_VLAN_table(mdio);

    ar8216_mdio_write32(mdio,
                        AR8216_GLOBAL_VLAN_TABLE_1, 
                        0
                        | VLAN_TABLE_1_VT_FUNC_VALUE(VLAN_TABLE_FUNC_PURGE_ENTRY)
                        | VLAN_TABLE_1_VT_BUSY
                        | VLAN_TABLE_1_VID_VALUE(vid)
                       );
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ar8327_del_vlan_vid(cpphy_mdio_t *mdio, unsigned int vid) {
    unsigned int RegData;

    DEB_TRC("[%s] vid %u (%#x)\n", __FUNCTION__, vid, vid);

    ar8327_wait_VLAN_table(mdio);

    RegData =   0
              | AR8327_VTU_FUNC1_FUNC_VALUE(AR8327_VTU_FUNC1_FUNC_VALUE_PURGE_ENTRY)
              | AR8327_VTU_FUNC1_VID_VALUE(vid);
    ar8216_mdio_write32(mdio, AR8327_VTU_FUNC1, RegData);
    RegData |= AR8327_VTU_FUNC1_BUSY;
    ar8216_mdio_write32(mdio, AR8327_VTU_FUNC1, RegData);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar8216_add_vlan_vid(cpphy_mdio_t *mdio, unsigned int vid, unsigned int mem_mask) {
    ar8216_del_vlan_vid(mdio, vid);

    ar8216_wait_VLAN_table(mdio);

    ar8216_mdio_write32(mdio,
                        AR8216_GLOBAL_VLAN_TABLE_2,
                        0
                        | VLAN_TABLE_2_VID_MEM_VALUE(mem_mask)
                        | VLAN_TABLE_2_VT_VALID           
                       );

    ar8216_mdio_write32(mdio,
                        AR8216_GLOBAL_VLAN_TABLE_1, 
                        0
                        | VLAN_TABLE_1_VT_FUNC_VALUE(VLAN_TABLE_FUNC_LOAD_ENTRY)
                        | VLAN_TABLE_1_VT_BUSY
                        | VLAN_TABLE_1_VID_VALUE(vid)
                       );
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar8327_add_vlan_vid(cpphy_mdio_t *mdio, unsigned int vid, unsigned int mem_mask) {
    unsigned int RegData0, RegData1;
    unsigned short port;

    ar8327_del_vlan_vid(mdio, vid);

    ar8327_wait_VLAN_table(mdio);

    RegData0 = AR8327_VTU_FUNC0_VALID;
    for(port = 0; port < 7; port++) {
        if(mem_mask & (1 << port)) {
            RegData0 |= AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE(port, AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE_UNMODIFIED);
            /*--- RegData0 |= AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE(port, AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE_TAGGED); ---*/
        } else {
            RegData0 |= AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE(port, AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE_NOT_MEMBER);
        }
    }
    ar8216_mdio_write32(mdio, AR8327_VTU_FUNC0, RegData0);

    RegData1 =   0
               | AR8327_VTU_FUNC1_FUNC_VALUE(AR8327_VTU_FUNC1_FUNC_VALUE_LOAD_ENTRY)
               | AR8327_VTU_FUNC1_VID_VALUE(vid)
               | AR8327_VTU_FUNC1_BUSY;
    ar8216_mdio_write32(mdio, AR8327_VTU_FUNC1, RegData1);

    DEB_TRC("[%s] vid %u (%#x) mem_mask %#x (VTU_FUNC0 = %#x, VTU_FUNC1 = %#x)\n", 
            __FUNCTION__, vid, vid, mem_mask, RegData0, RegData1);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ar_generic_init(cpphy_mdio_t *mdio) {
    unsigned char port;
    unsigned int tmp;

    DEB_INFOTRC("[%s]\n", __FUNCTION__);

	ar8216_ar_init();

    mdio->run_ar_queue_without_removal_counter = 0;

    for(port = 0; port < 6; port++) {
        DEB_TEST("[%s] Checking PHY %u reg 0: %#x\n", 
                 __func__, port, 
                 mdio_read(mdio, 
                           port, 
                           AR8216_MII_CONTROL));
        mdio_write(mdio, 
                   port, 
                   AR8216_MII_AUTO_NEG_ADV, 
                   ATHR_ADVERTISE_ALL);
        mdio_write(mdio, 
                   port, 
                   AR8216_MII_CONTROL, 
                   AR8216_PHYCTRL_SW_RESET | AR8216_PHYCTRL_AUTONEG_EN);
    }

    mdelay(200);
    for(port = 1; port < 6; port++) {
        ar8216_mdio_write32(mdio, 
                            (port + 1) * 0x100 + AR8216_PORT_STATUS_OFFSET, 
                            ATH_PORT_STATUS_LINKEN);
        ar8216_mdio_write32(mdio, 
                            (port + 1) * 0x100 + AR8216_PORT_CONTROL_OFFSET,
                            ATH_PORT_CTRL_LEARN_EN
                            | ATH_PORT_CTRL_EG_VLAN_MODE(ATH_PORT_CTRL_EG_VLAN_MODE_UNTAGGED)
                            | ATH_PORT_CTRL_PORTSTATE_FORWARD
                            );
        /* Default VID 1, port member only CPU port */
        if(cpmac_global.cpphy[mdio->phy_num].config->type == CPMAC_PHY_TYPE_AR8226) {
            tmp = ar8216_mdio_read32(mdio, (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET);
            tmp &= ~(ATH_PORT_VLAN_VID_MASK | ATH_PORT_VLAN_CVID_MASK);
            tmp |= ATH_PORT_VLAN_VID_VALUE(1) | ATH_PORT_VLAN_CVID_VALUE(1);
            ar8216_mdio_write32(mdio, (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET, tmp);

            tmp = ar8216_mdio_read32(mdio, (port + 1) * 0x100 + AR8226_PORT_VLAN2_OFFSET);
            tmp &= ~(ATH_PORT_VLAN2_VID_MEM_MASK | ATH_PORT_VLAN2_8021_QMODE_MASK);
            tmp |=   ATH_PORT_VLAN2_VID_MEM_VALUE(0x3f & ~(1 << port))
                   | ATH_PORT_VLAN2_8021_QMODE_VALUE(ATH_PORT_VLAN2_8021_QMODE_DISABLE);
            ar8216_mdio_write32(mdio, (port + 1) * 0x100 + AR8226_PORT_VLAN2_OFFSET, tmp);
        } else {
            tmp = ar8216_mdio_read32(mdio, (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET);
            tmp &= ~(ATH_PORT_VLAN_VID_MASK | ATH_PORT_VLAN_VID_MEM_MASK | ATH_PORT_VLAN_8021_QMODE_MASK);
            tmp |=   ATH_PORT_VLAN_VID_VALUE(1)
                   | ATH_PORT_VLAN_VID_MEM_VALUE(0x3f & ~(1 << port))
                   | ATH_PORT_VLAN_8021_QMODE_VALUE(ATH_PORT_VLAN_8021_QMODE_DISABLE);
            ar8216_mdio_write32(mdio, (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET, tmp);
        }
    }

    ar8216_mdio_write32(mdio, 
                        0x100 + AR8216_PORT_CONTROL_OFFSET,
                          ATH_PORT_CTRL_LEARN_EN
                        | ATH_PORT_CTRL_EG_VLAN_MODE(ATH_PORT_CTRL_EG_VLAN_MODE_UNTAGGED)
                        | ATH_PORT_CTRL_SINGLE_VLAN_EN
                        | ATH_PORT_CTRL_PORTSTATE_FORWARD);

    /* Disable unused ports as far as possible */
    for(port = 0; port < mdio->switch_config.ports; port++) {
        if(mdio->switch_config.used_portset & (1 << port)) {
            continue;
        }
        DEB_INFOTRC("[%s] Disable unused port %u\n", __FUNCTION__, port);
        ar8216_mdio_write32(mdio, 
                            (port + 1) * 0x100 + AR8216_PORT_STATUS_OFFSET, 
                            0);
        ar8216_mdio_write32(mdio, 
                            (port + 1) * 0x100 + AR8216_PORT_CONTROL_OFFSET, 
                            ATH_PORT_CTRL_PORTSTATE_DISABLE);
        /* TODO The following command seems quite wrong! */
        ar8216_mdio_write32(mdio, 
                            (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET, 
                            ATH_PORT_CTRL_PORTSTATE_DISABLE);
        mdio_write(mdio, port, 0x0, 0xc00);
    }

    /* Allow bigger packet size: CPPHY_MAX_RX_BUFFER_SIZE + 4 bytes for double tagged jumbo packets */
    tmp = ar8216_mdio_read32(mdio, AR8216_GLOBAL_CONTROL);
    tmp &= 0xfffffc00;
    tmp |= (CPPHY_MAX_RX_BUFFER_SIZE + 4);
    ar8216_mdio_write32(mdio, AR8216_GLOBAL_CONTROL, tmp);

    /* Enable MIB counters */
    ar8216_mdio_write32(mdio, AR8216_GLOBAL_MIB, 0x40000000);

    mdio->state = CPPHY_MDIO_ST_LINKED;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar_ar8216_init(cpphy_mdio_t *mdio) {
    unsigned int tmp;

    ar8216_softreset(mdio);

    tmp = ar8216_mdio_read32(mdio, AR8216_GLOBAL_DEVICE_ID);
    if((tmp & 0xffff) == 0x0201) {
        unsigned char port;

        DEB_WARN("[%s] Recognized AR8226 without HWSubRevision!\n", __FUNCTION__);
        cpmac_global.cpphy[mdio->phy_num].config->type = CPMAC_PHY_TYPE_AR8226;
#       if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5))
        mdio->f->set_wan_keep_tagging    = adm_set_wan_keep_tagging;
        mdio->f->switch_wan_keep_tagging = adm_switch_wan_keep_tagging_ar8x16;
#       endif /*--- #if !(defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) || defined(CONFIG_ARCH_PUMA5)) ---*/
        mdio->f->check_link             = ar_check_link_orig;
        mdio->f->init_phy               = ar_ar8226_init;
        mdio->f->dump_counters          = ar8316_dump_counters;
        mdio->f->deinit_phy             = ar_ar8226_deinit;
        mdio->f->set_igmp_fwd           = cpphy_ar8216_set_igmp_fwd;
        mdio->f->mgmt_check_link        = cpphy_mgmt_check_link_orig;
        mdio->f->switch_port_power      = ar8316_switch_port_power;
        mdio->switch_config.max_vid_groups = 16;
        for(port = 0; port < cpmac_global.ports; port++) {
            /* Check, whether the port should be used */
            if(cpmac_global.power.port_modes_possible[port] | (1 << ADM_PHY_POWER_ON)) {
                cpmac_global.power.port_modes_possible[port] =   (1 << ADM_PHY_POWER_OFF)
                                                               | (1 << ADM_PHY_POWER_ON)
                                                               | (1 << ADM_PHY_POWER_SAVE);
            }
        }
        cpmac_global.power.setup = cpmac_global.power.setup_asked_for;
        cpphy_mgmt_check_powersave_settings();
        ar_ar8226_init(mdio);
        return;
    }
    tmp |= 0x500000;
    ar8216_mdio_write32(mdio, AR8216_GLOBAL_DEVICE_ID, (1 << 24) | tmp);  /*--- change MII-Clockphase by 180 ---*/

    ar_generic_init(mdio);

    ar8216_mdio_write32(mdio, 0x38, 0xc000050e);      /*--- undefined lt. Datenblatt ---*/

    /*--- ar8216_mdio_write32(mdio, 0x60, 0xffffffff); ---*/
    ar8216_mdio_write32(mdio, 0x60, 0x0);
    ar8216_mdio_write32(mdio, 0x64, 0xaaaaaaaa);
    ar8216_mdio_write32(mdio, 0x68, 0x55555555);    
    /*--- ar8216_mdio_write32(mdio, 0x6c, 0x0); ---*/    
    ar8216_mdio_write32(mdio, 0x6c, 0xffffffff);    
    /*--- ar8216_mdio_write32(mdio, 0x70, 0x41af); ---*/
    ar8216_mdio_write32(mdio, 0x70, 0xfa41);

    /*--- ar8216_mdio_write32(mdio, 0x78, (1 << 8)); ---*/
    
    /* Enable address table entry aging (CPMAC_AR_AGE_TIME * 17 seconds) */
    ar8216_mdio_write32(mdio, AR8216_GLOBAL_AR_CONTROL, AR_CONTROL_AGE_EN | AR_CONTROL_AGE_TIME(CPMAC_AR_AGE_TIME));

    /* Enable CPU port */
    ar8216_mdio_write32(mdio, 
                        AR8216_PORT0_CONTROL_BASIS,
                          ATH_PORT_STATUS_SPEED_100M
                        | ATH_PORT_STATUS_TXMACEN
                        | ATH_PORT_STATUS_RXMACEN
                        | ATH_PORT_STATUS_TXFLOWEN
                        | ATH_PORT_STATUS_RXFLOWEN
                        | ATH_PORT_STATUS_FULLDUPLEX
                        | ATH_PORT_STATUS_PHY_LINKUP
                        | ATH_PORT_STATUS_LINK_PAUSE
                        | ATH_PORT_STATUS_ASYNC_PAUSE
                       );
    ar8216_mdio_write32(mdio, AR8216_GLOBAL_CPUPORT, 0x1f0);

    if(mdio->f->ar_work_item) {
        cpphy_mgmt_work_add(mdio, CPMAC_WORK_UPDATE_MAC_TABLE, mdio->f->ar_work_item, CPMAC_ARL_UPDATE_INTERVAL);
    } else {
        DEB_ERR("[%s] No ar_work_item found!\n", __FUNCTION__);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar_ar8226_init(cpphy_mdio_t *mdio) {
    unsigned int tmp;

    ar8216_softreset(mdio);

    ar_generic_init(mdio);

    /* Enable CPU port */
    ar8216_mdio_write32(mdio, 
                        AR8216_PORT0_CONTROL_BASIS,
                        ATH_PORT_STATUS_SPEED_100M
                        | ATH_PORT_STATUS_TXMACEN
                        | ATH_PORT_STATUS_RXMACEN
                        | ATH_PORT_STATUS_TXFLOWEN
                        | ATH_PORT_STATUS_RXFLOWEN
                        | ATH_PORT_STATUS_FULLDUPLEX
                       );
    ar8216_mdio_write32(mdio, AR8216_GLOBAL_CPUPORT, 0x1f0);

    ar8216_mdio_write32(mdio, 0x0,  0x0201);
    ar8216_mdio_write32(mdio, 0x04, 0x80000400);
    ar8216_mdio_write32(mdio, 0x10, 0x80061B20);
    ar8216_mdio_write32(mdio, 0x2c, 0xfe7f7f7f);

    /* Configure to forward IGMP packets to the CPU instead of copying, IF
     * forwarding gets enabled in port control */
    tmp = ar8216_mdio_read32(mdio, AR8316_QM_CONTROL);
    tmp &= ~(AR8316_QM_CONTROL_IGMP_COPY_EN);
    ar8216_mdio_write32(mdio, AR8316_QM_CONTROL, tmp);

    if(mdio->f->ar_work_item) {
        cpphy_mgmt_work_add(mdio, CPMAC_WORK_UPDATE_MAC_TABLE, mdio->f->ar_work_item, CPMAC_ARL_UPDATE_INTERVAL);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar_ar8327_init(cpphy_mdio_t *mdio) {
    unsigned int tmp;
    union ar8327_port_status port_status;

    ar8216_softreset(mdio);

	ar8216_ar_init();

    /* Enable CPU port */
    port_status.reg = 0;
    port_status.bits.speed               = link_speed_1000M;
    port_status.bits.txmac_enable        = 1;
    port_status.bits.rxmac_enable        = 1;
    port_status.bits.tx_flow_enable      = 1;
    port_status.bits.rx_flow_enable      = 1;
    port_status.bits.full_duplex         = 1;
    port_status.bits.tx_half_flow_enable = 1;
    port_status.bits.link                = 1;
    port_status.bits.flow_link_enable    = 1;
    ar8216_mdio_write32(mdio, AR8327_PORT_STATUS(0), port_status.reg);
                        
    ar8216_mdio_write32(mdio,
                        AR8327_PORT0_PAD_MODE_CTRL,
                          AR8327_PORT0_MAC_RGMII_RXCLK_SEL
                        | AR8327_PORT0_MAC_RGMII_TXCLK_SEL
                        | AR8327_PORT0_MAC_RGMII_EN);
    ar8216_mdio_write32(mdio,
                        AR8327_PORT6_PAD_MODE_CTRL,
                          AR8327_PORT6_PHY4_RGMII_EN
                        | AR8327_PORT6_MAC_RGMII_RXCLK_SEL);
    ar8216_mdio_write32(mdio, AR8327_PWS, AR8327_PWS_PACKAGE148_EN);

    /*--- ar8216_mdio_write32(mdio, AR8216_GLOBAL_CPUPORT, 0x1f0); ---*/

    /*--- ar8216_mdio_write32(mdio, 0x0,  0x0201); ---*/
    ar8216_mdio_write32(mdio, 
                        AR8327_GLOBAL_FW_CTRL1,
                          AR8327_GLOBAL_FW_CTRL1_UNI_FLOOD_DP_VALUE(0x7f)
                        | AR8327_GLOBAL_FW_CTRL1_MULTI_FLOOD_DP_VALUE(0x7f)
                        | AR8327_GLOBAL_FW_CTRL1_BROAD_DP_VALUE(0x7f)
                        | AR8327_GLOBAL_FW_CTRL1_IGMP_JOIN_LEAVE_DP_VALUE(0x7f));

    /* Enable MIB counter */
    tmp = ar8216_mdio_read32(mdio, AR8327_MODULE_EN);
    tmp |= AR8327_MODULE_EN_MIB_EN;
    ar8216_mdio_write32(mdio, AR8327_MODULE_EN, tmp);

    /* Configure to forward IGMP packets to the CPU instead of copying, IF
     * forwarding gets enabled in port control */
    tmp = ar8216_mdio_read32(mdio, AR8327_GLOBAL_FW_CTRL0);
    tmp |= AR8327_GLOBAL_FW_CTRL0_CPU_PORT_EN;
    ar8216_mdio_write32(mdio, AR8327_GLOBAL_FW_CTRL0, tmp);

    if(mdio->f->ar_work_item) {
        cpphy_mgmt_work_add(mdio, CPMAC_WORK_UPDATE_MAC_TABLE, mdio->f->ar_work_item, CPMAC_ARL_UPDATE_INTERVAL);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar_ar8316_mac5_enable(cpphy_mdio_t *mdio, unsigned char enable) {
    unsigned int strap_on = ar8216_mdio_read32(mdio, AR8316_GLOBAL_PWR_ON_STRAP);
    unsigned int port_status = ar8216_mdio_read32(mdio, AR8216_PORT5_CONTROL_BASIS + AR8216_PORT_STATUS_OFFSET);
    if(enable) {
        strap_on |= AR8316_GLOBAL_PWR_ON_STRAP_PHY4_RGMII_EN;
        port_status |=   ATH_PORT_STATUS_SPEED_1000M
                       | ATH_PORT_STATUS_TX_HALF_FLOW_EN
                       | ATH_PORT_STATUS_PHY_LINKUP;
    } else {
        strap_on &= ~AR8316_GLOBAL_PWR_ON_STRAP_PHY4_RGMII_EN;
        port_status &= ~(  ATH_PORT_STATUS_SPEED_1000M
                         | ATH_PORT_STATUS_TX_HALF_FLOW_EN
                         | ATH_PORT_STATUS_PHY_LINKUP);
    }
    ar8216_mdio_write32(mdio, AR8316_GLOBAL_PWR_ON_STRAP, strap_on);
    ar8216_mdio_write32(mdio, AR8216_PORT5_CONTROL_BASIS + AR8216_PORT_STATUS_OFFSET, port_status);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar_ar8316_init(cpphy_mdio_t *mdio) {
    unsigned int tmp;
    char *hwrev = prom_getenv("HWRevision");

    ar8216_softreset(mdio);

    ar_generic_init(mdio);

    tmp = ar8216_mdio_read32(mdio, 0x108);
    tmp |= ((1<<24)|(1<<2));
    ar8216_mdio_write32(mdio, 0x108, tmp);

    /* Enable CPU port */
    ar8216_mdio_write32(mdio, 
                        AR8216_PORT0_CONTROL_BASIS,
                        ATH_PORT_STATUS_SPEED_1000M
                        | ATH_PORT_STATUS_TXMACEN
                        | ATH_PORT_STATUS_RXMACEN
                        | ATH_PORT_STATUS_TXFLOWEN
                        | ATH_PORT_STATUS_RXFLOWEN
                        | ATH_PORT_STATUS_FULLDUPLEX
                       );
    ar8216_mdio_write32(mdio, AR8216_GLOBAL_CPUPORT, 0x1f0);

    /*--- ar8216_mdio_write32(mdio, 0x2c, 0x003f003f); ---*/
    /* Define portsets for - broadcasts adresses              *
     *                     - unknown unicast adresses         *
     *                     - IGMP snooping of fast join/leave */
    ar8216_mdio_write32(mdio, 0x2c, 0x43f3f3f);
    ar8216_mdio_write32(mdio,
                        AR8316_GLOBAL_PWR_ON_STRAP,
                          AR8316_GLOBAL_PWR_ON_STRAP_RESERVED
                        | AR8316_GLOBAL_PWR_ON_STRAP_MAC0_GMII_EN
                        | AR8316_GLOBAL_PWR_ON_STRAP_MAC0_MAC_MODE
                        | AR8316_GLOBAL_PWR_ON_STRAP_MAC5_PHY_MODE
                        | AR8316_GLOBAL_PWR_ON_STRAP_LED_OPEN_EN);
    ar8216_mdio_write32(mdio, 
                        AR8216_PORT5_CONTROL_BASIS + AR8216_PORT_STATUS_OFFSET,  
                          ATH_PORT_STATUS_SPEED_10M
                        | ATH_PORT_STATUS_TXMACEN
                        | ATH_PORT_STATUS_RXMACEN
                        | ATH_PORT_STATUS_TXFLOWEN
                        | ATH_PORT_STATUS_RXFLOWEN
                        | ATH_PORT_STATUS_FULLDUPLEX
                       );

    /* Queue priorization and queue length limits */
    if(mdio->switch_predefined_configs == &(switch_configuration[CPMAC_SWITCH_CONF_MAGPIE][0])) {
#       if 0
        unsigned char port;

        for(port = 1; port <= 4; port++) {
            /* Limit all external port queue sizes to 128 */
            ar8216_mdio_write32(mdio,
                                (port + 1) * 0x100 + 0x18,
                                  0x03000000 
                                | ((160 / 4) << 16) /* max 160 buffers for the port */
                                | (( 48 / 4) << 12) /* max 60 buffers for priority queue 3 */
                                | (( 48 / 4) <<  8) /* max 60 buffers for priority queue 2 */
                                | (( 48 / 4) <<  4) /* max 60 buffers for priority queue 1 */
                                | (( 48 / 4) <<  0) /* max 60 buffers for priority queue 0 */
                               );
            /* Reduce the priority of the external ports */
            tmp = ar8216_mdio_read32(mdio, (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET);
            tmp &= ~(7 << 27);
            tmp |= 0 << 27;
            ar8216_mdio_write32(mdio, (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET, tmp);
        }
        /* Limit CPU port queue size to 160 */
        port = 0;
        ar8216_mdio_write32(mdio,
                            (port + 1) * 0x100 + 0x18,
                              0x03000000 
                            | (( 40 / 4) << 16) /* max 160 buffers for the port */
                            | ((  8 / 4) << 12) /* max 60 buffers for priority queue 3 */
                            | ((  8 / 4) <<  8) /* max 60 buffers for priority queue 2 */
                            | ((  8 / 4) <<  4) /* max 60 buffers for priority queue 1 */
                            | ((  8 / 4) <<  0) /* max 60 buffers for priority queue 0 */
                           );
#       endif /*--- #if 0 ---*/

        /* Mark packets from MAC port as highest priority */
        tmp = ar8216_mdio_read32(mdio, (5 + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET);
        tmp &= ~(7 << 27);
        tmp |= 7 << 27;
        ar8216_mdio_write32(mdio, (5 + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET, tmp);
        ar8216_mdio_write32(mdio, (5 + 1) * 0x100 + AR8216_PORT_PRIORITY_OFFSET, 0x0008002d);
        /* Configure CPU port egress queues as SPQ */
        tmp = ar8216_mdio_read32(mdio, AR8216_GLOBAL_CONTROL);
        tmp &= ~(1 << 31);
        ar8216_mdio_write32(mdio, AR8216_GLOBAL_CONTROL, tmp);
    }

    /* Configure to forward IGMP packets to the CPU instead of copying, IF
     * forwarding gets enabled in port control */
    tmp = ar8216_mdio_read32(mdio, AR8316_QM_CONTROL);
    tmp &= ~(AR8316_QM_CONTROL_IGMP_COPY_EN);
    ar8216_mdio_write32(mdio, AR8316_QM_CONTROL, tmp);

    /* Enable address table entry aging (CPMAC_AR_AGE_TIME * 17 seconds) */
    tmp = ar8216_mdio_read32(mdio, AR8216_GLOBAL_AR_CONTROL);
    tmp &= ~(AR_CONTROL_AGE_EN | AR_CONTROL_AGE_TIME_MASK);
    tmp |= AR_CONTROL_AGE_EN | AR_CONTROL_AGE_TIME(CPMAC_AR_AGE_TIME);
    ar8216_mdio_write32(mdio, AR8216_GLOBAL_AR_CONTROL, tmp);

    /* Make the MAC aa:aa:aa:aa:aa:aa a static entry in the MAC table linking
     * it to the port where the Magpie is connected to */
    if(hwrev && (   (!(strncmp("157", hwrev, 3)) && (strlen(hwrev) < 5)) /* 6360 */
                 || (!(strncmp("187", hwrev, 3)) && (strlen(hwrev) < 5)) /* 6340 */
      )) {
        ar8216_mdio_write32(mdio, AR8316_GLOBAL_AR_1, 0
                            | AR8316_GLOBAL_AR_1_ADDR_BYTE0_VALUE(0xaa)
                            | AR8316_GLOBAL_AR_1_ADDR_BYTE1_VALUE(0xaa)
                            | AR8316_GLOBAL_AR_1_ADDR_BYTE2_VALUE(0xaa)
                            | AR8316_GLOBAL_AR_1_ADDR_BYTE3_VALUE(0xaa)
                           );
        ar8216_mdio_write32(mdio, AR8316_GLOBAL_AR_2, 0
                            | AR8316_GLOBAL_AR_2_DES_PORT_VALUE(0x20)
                            | AR8316_GLOBAL_AR_2_AT_STATUS_VALUE(AR8316_GLOBAL_AR_2_AT_STATUS_VALUE_STATIC)
                           );
        ar8216_mdio_write32(mdio, AR8316_GLOBAL_AR_0, 0
                            | AR8316_GLOBAL_AR_0_AT_BUSY 
                            | AR8316_GLOBAL_AR_0_AT_FUNC_VALUE(AR8316_GLOBAL_AR_0_AT_FUNC_VALUE_LOAD)
                            | AR8316_GLOBAL_AR_0_ADDR_BYTE4_VALUE(0xaa)
                            | AR8316_GLOBAL_AR_0_ADDR_BYTE5_VALUE(0xaa)
                           );
    }

    if(mdio->f->ar_work_item) {
        cpphy_mgmt_work_add(mdio, CPMAC_WORK_UPDATE_MAC_TABLE, mdio->f->ar_work_item, CPMAC_ARL_UPDATE_INTERVAL);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

void ar_ar8216_deinit(cpphy_mdio_t *mdio __attribute__ ((unused)))
{
   ar8216_ar_deinit();
}

void ar_ar8226_deinit(cpphy_mdio_t *mdio __attribute__ ((unused)))
{
   ar8216_ar_deinit();
}

void ar_ar8316_deinit(cpphy_mdio_t *mdio __attribute__ ((unused)))
{
   ar8216_ar_deinit();
}

void ar_ar8327_deinit(cpphy_mdio_t *mdio __attribute__ ((unused)))
{
   ar8216_ar_deinit();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ar_switch_test_link(cpphy_mdio_t *mdio, unsigned char port) {
    unsigned int old_fullduplex, speed = 0;
    enum _avm_event_ethernet_speed old_speed;
    enum _link_speed old_link;

    /* Save old state */
    old_link = cpmac_global.event_data.port[port].link;
    old_speed = cpmac_global.event_data.port[port].speed;
    old_fullduplex = cpmac_global.event_data.port[port].fullduplex;

    if(!cpmac_global.power.port_on[port]) {
        /* Without power, the link information would be wrong */
        cpmac_global.event_data.port[port].link = 0;
    } else {
        /* Check real time link information */
        if(cpmac_global.cpphy[mdio->phy_num].config->type == CPMAC_PHY_TYPE_AR8327) {
            union ar8327_port_status port_status;
            port_status.reg = ar8216_mdio_read32(mdio, AR8327_PORT_STATUS(port + mdio->switch_config.external_port_offset));
            cpmac_global.event_data.port[port].link = port_status.bits.link;
            cpmac_global.event_data.port[port].fullduplex = port_status.bits.full_duplex;
            speed = port_status.bits.speed;
        } else {
            unsigned short status = mdio_read(mdio, port, AR8216_MII_PHY_STATUS);
            cpmac_global.event_data.port[port].link = (status & ATHR_PHY_STATUS_LINK) ? 1 : 0;
            cpmac_global.event_data.port[port].fullduplex = (status & ATHR_PHY_STATUS_DUPLEX) ? 1 : 0;
            speed = (status & ATHR_PHY_STATUS_SPEED_MASK) >> ATHR_PHY_STATUS_SPEED_SHIFT;
        }
    }

    /* Set new state */
    cpmac_global.event_data.port[port].speed100 = (speed == 1) ? 1 : 0;
    cpmac_global.event_data.port[port].speed = avm_event_ethernet_speed_no_link;
    if(cpmac_global.event_data.port[port].link) {
        switch(speed) { 
            case 0: cpmac_global.event_data.port[port].speed = avm_event_ethernet_speed_10M; break;
            case 1: cpmac_global.event_data.port[port].speed = avm_event_ethernet_speed_100M; break;
            case 2: cpmac_global.event_data.port[port].speed = avm_event_ethernet_speed_1G; break;
            default: /* Nothing to change */ break;
        }
    } else {
        ar8216_ar_del_port(mdio, port + mdio->switch_config.external_port_offset);
    }

    if(   (cpmac_global.event_data.port[port].link != old_link)
       || (cpmac_global.event_data.port[port].speed != old_speed)
       || (cpmac_global.event_data.port[port].fullduplex != old_fullduplex)) {
        /* Link changed. Update link status */
        mdio->event_update_needed++;
    }

    return cpmac_global.event_data.port[port].link;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ar_check_link_orig(cpphy_mdio_t *mdio) {
    unsigned int link_status = 0;
    unsigned char phy;

    for(phy = 0; phy < AR8216_MAX_PHYS; phy++) {
        link_status |= ar_switch_test_link(mdio, phy) << (phy + 1);
    }

    mdio->linked = link_status;

    return mdio->linked;
}

 
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ar_check_link_emv(cpphy_mdio_t *mdio) {
    return mdio->linked;
}

 
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar_switch_port_power_on_boot_orig(cpphy_mdio_t *mdio, unsigned short portset) {
    unsigned char port;

    DEB_WARN("[%s] This will take approx. 12 seconds!\n", __FUNCTION__);
    for(port = 0; port < 5; port++) {
        if((1 << port) & portset) {
            mdio_write(mdio, port, 0x0,  0x3100);
            mdio_write(mdio, port, 0x1d, 0x3);
            mdio_write(mdio, port, 0x1e, 0x11);
        }
    }
    /*--- ssleep(4); ---*/
    for(port = 0; port < 5; port++) {
        if((1 << port) & portset) {
            mdio_write(mdio, port, 0x1d, 0x2);
            mdio_write(mdio, port, 0x1e, 0x3200);
        }
    }
    /*--- ssleep(4); ---*/
    for(port = 0; port < 5; port++) {
        if((1 << port) & portset) {
            mdio_write(mdio, port, 0x1d, 0x1);
            mdio_write(mdio, port, 0x1e, 0x1d0);
        }
    }
    /*--- ssleep(4); ---*/
    for(port = 0; port < 5; port++) {
        if((1 << port) & portset) {
            mdio_write(mdio, port, 0x1d, 0x0);
            mdio_write(mdio, port, 0x1e, 0x024e);
        }
    }
    ssleep(4);
    for(port = 0; port < 5; port++) {
        if((1 << port) & portset) {
            mdio_write(mdio, port, 0x0, 0xffff);
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if 0
static unsigned int ar_switch_test_duplex(cpphy_mdio_t *mdio, unsigned char port) {
    unsigned short temp;

    /* Check real time duplex resolved information */
    temp = mdio_read(mdio, port, 0x11);
    return (temp & ATHR_PHY_STATUS_SPEED_DUPLEX);
}
#endif /*--- #if 0 ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar8316_switch_port_power(cpphy_mdio_t *mdio, 
                              unsigned char port, 
                              unsigned char power_on) {
    unsigned short RegData;

    DEB_DEBUG("[%s] %lu Port %u %s\n", __FUNCTION__, jiffies, port, power_on ? "on" : "off"); 

    RegData = mdio_read(mdio, port, AR8216_MII_CONTROL);
    if(power_on) {
        RegData &= ~AR8216_PHYCTRL_POWERDOWN;
        RegData |= AR8216_PHYCTRL_AUTONEG_RESTART;
    } else {
        RegData |= AR8216_PHYCTRL_POWERDOWN;
    }
    mdio_write(mdio, port, AR8216_MII_CONTROL, RegData);
}


#if defined(POWERMANAGEMENT_THROTTLE_ETH)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar8316_switch_port_speed_throttle(cpphy_mdio_t *mdio,
                                       unsigned char port,
                                       unsigned char do_throttle) {
    unsigned short RegData;

    /* Special handling for 7340 */
    if(cpmac_global.cpphy[0].config->type == CPMAC_PHY_TYPE_VITESSE) {
        port = (port == 0) ? 1 : 0;
    }
    do_throttle = do_throttle ? 1 : 0;

    cpphy_mgmt_work_schedule(mdio, CPMAC_WORK_TICK, 5 * HZ);
    RegData = mdio_read(mdio, port, MII_REG_1000BASET_CTRL);
    if(do_throttle) {
        /* Disable 1000 Base T negotiation capability */
        DEB_INFO("[%s] Throttling port %u speeds to 100 MBit/s\n", __FUNCTION__, port);
        RegData &= ~(MII_REG_1000BASET_CTRL_ADV_HD | MII_REG_1000BASET_CTRL_ADV_FD);
    } else {
        /* Enable 1000 Base T negotiation capability */
        DEB_INFO("[%s] Unthrottling port %u speeds\n", __FUNCTION__, port);
        RegData |= MII_REG_1000BASET_CTRL_ADV_HD | MII_REG_1000BASET_CTRL_ADV_FD;
    }
    mdio_write(mdio, port, MII_REG_1000BASET_CTRL, RegData);

    /* Now reset the port to let the change take effect */
    RegData = mdio_read(mdio, port, MII_REG_CONTROL);
    mdio_write(mdio, port, MII_REG_CONTROL, RegData | MII_REG_CONTROL_AUTONEG_RESTART);
}
#endif /*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar_switch_port_power_orig(cpphy_mdio_t *mdio, 
                               unsigned char port, 
                               unsigned char power_on) {
    /*--- DEB_TEST("[%s] %lu Port %u %s\n", __FUNCTION__, jiffies, port, power_on ? "on" : "off"); ---*/ 
    if(power_on) {
        mdio_write(mdio, port, 0x1d, 0x0);
        mdio_write(mdio, port, 0x1e, 0x124e);
        mdio_write(mdio, port, 0x1d, 0x3);
        mdio_write(mdio, port, 0x1e, 0x11);
        mdio_write(mdio, port, 0x1d, 0x2);
        mdio_write(mdio, port, 0x1e, 0x3220);
        mdio_write(mdio, port, 0x1d, 0x1);
        mdio_write(mdio, port, 0x1e, 0x1d0);
        mdio_write(mdio, port, 0x1d, 0x0);
        mdio_write(mdio, port, 0x1e, 0x024e);
        mdio_write(mdio, port, 0x0,  0xffff);
    } else {
        mdio_write(mdio, port, 0x0,  0x1200);
        mdio_write(mdio, port, 0x1d, 0x0);
        mdio_write(mdio, port, 0x1e, 0x124e);
        mdio_write(mdio, port, 0x1d, 0x1);
        mdio_write(mdio, port, 0x1e, 0x10);
        mdio_write(mdio, port, 0x1d, 0x2);
        mdio_write(mdio, port, 0x1e, 0x3000);
        mdio_write(mdio, port, 0x1d, 0x3);
        mdio_write(mdio, port, 0x1e, 0x0000);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar_boot_power_on_emv(cpphy_mdio_t *mdio) {
    unsigned char port;

    for(port = 0; port < 4; port++) {
        /* Switch on port again */
        DEB_INFOTRC("[%s] port %u: restart after CPPHY_POWER_MODE_OFF\n", __FUNCTION__, port);
        mdio_write(mdio, port, 0x1d, 0x0);
        mdio_write(mdio, port, 0x1e, 0x124e);
        mdio_write(mdio, port, 0x1d, 0x3);
        mdio_write(mdio, port, 0x1e, 0x11);
        mdio_write(mdio, port, 0x1d, 0x2);
        mdio_write(mdio, port, 0x1e, 0x3220);
        mdio_write(mdio, port, 0x1d, 0x1);
        mdio_write(mdio, port, 0x1e, 0x1d0);
        mdio_write(mdio, port, 0x1d, 0x0);
        mdio_write(mdio, port, 0x1e, 0x024e);
        mdio_write(mdio, port, 0x0,  0x3100);
    }
    ssleep(4);

    for(port = 0; port < 4; port++) {
        /* Force port into CPPHY_POWER_MODE_ON_MDI_MLT3 to prevent initial 10 MBit/s links */
        DEB_INFOTRC("[%s] port %u: reset as MDI/MLT3\n", __FUNCTION__, port);
        mdio_write(mdio, port, 0x14, 0x2c);
        mdio_write(mdio, port, 0x10, 0x800);
        mdio_write(mdio, port, 0x0,  0xa100);
        cpmac_global.power.port_mode[port] = CPPHY_POWER_MODE_ON_MDI_MLT3;
    }
    ssleep(2);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar_switch_port_power_emv(cpphy_mdio_t *mdio, 
                              unsigned char port, 
                              cpphy_switch_powermodes_t mode) {
    cpmac_global.power.time_to_wait_to[port] = jiffies;

    if(mode == cpmac_global.power.port_mode[port]) {
        DEB_TEST("[%s] Mode %u already set for port %u\n", __FUNCTION__, mode, port);
        return;
    }

    switch(mode) {
        case CPPHY_POWER_MODE_OFF:
            /* TODO Check, whether this sequence works in all contexts (even in mute mode?) */
            DEB_INFOTRC("[%s] port %u: CPPHY_POWER_MODE_OFF\n", __FUNCTION__, port);
            mdio_write(mdio, port, 0x0,  0x1200);
            mdio_write(mdio, port, 0x1d, 0x0);
            mdio_write(mdio, port, 0x1e, 0x124e);
            mdio_write(mdio, port, 0x1d, 0x1);
            mdio_write(mdio, port, 0x1e, 0x10);
            mdio_write(mdio, port, 0x1d, 0x2);
            mdio_write(mdio, port, 0x1e, 0x3000);
            mdio_write(mdio, port, 0x1d, 0x3);
            mdio_write(mdio, port, 0x1e, 0x0000);
            break;

        case CPPHY_POWER_MODE_ON_IDLE_MDI:
            DEB_INFOTRC("[%s] port %u: CPPHY_POWER_MODE_ON_IDLE_MDI\n", __FUNCTION__, port);
            mdio_write(mdio, port, 0x10, 0x800);
            mdio_write(mdio, port, 0x00, 0xa100);
            break;

        case CPPHY_POWER_MODE_ON:
#           if CPMAC_USE_DEBUG_LEVEL_INFO
            {
                unsigned short temp = mdio_read(mdio, port, 0x11);
                DEB_INFO("[%s] port %u: CPPHY_POWER_MODE_ON %u MBit/s, %s duplex, MDI%s\n", __FUNCTION__, 
                         port,
                         (temp & ATHR_PHY_STATUS_SPEED_MASK) ? 100 : 10,
                         (temp & ATHR_PHY_STATUS_DUPLEX) ? "full" : "half",
                         (temp & ATHR_PHY_STATUS_MDI_CROSSOVER) ? "X" : ""
                        );
            }
#           endif /*--- #if CPMAC_USE_DEBUG_LEVEL_INFO ---*/
            /* If this is the first linked port, lock port 0 to MDIX */
            if(mdio->linked == 0) {
                if(port != 0) {
                    ar_switch_port_power_emv(mdio, 0, CPPHY_POWER_MODE_ON_MDIX);
                }
            }
            mdio->linked |= (1 << port);
            DEB_TEST("[%s] ports linked: %#x\n", __FUNCTION__, mdio->linked);
            if((port != 0) || (mdio->linked != (1 << 0))) {
                if(cpmac_global.power.port_mode[port] == CPPHY_POWER_MODE_ON_MDI_MLT3) {
                    mdio_write(mdio, port, 0x10, 0x860);
                    mdio_write(mdio, port, 0x0,  0xffff);
                }
            }
            break;

        case CPPHY_POWER_MODE_ON_MDI_MLT3:
            if(ar_switch_test_link(mdio, port)) {
                ar_switch_port_power_emv(mdio, port, CPPHY_POWER_MODE_ON);
                return;
            }
            DEB_INFOTRC("[%s] port %u: CPPHY_POWER_MODE_ON_MDI_MLT3\n", __FUNCTION__, port);
            mdio_write(mdio, port, 0x14, 0x2c);
            mdio_write(mdio, port, 0x10, 0x800);
            mdio_write(mdio, port, 0x0,  0xa100);
            break;

        case CPPHY_POWER_MODE_ON_MDIX:
            if(ar_switch_test_link(mdio, port)) {
                ar_switch_port_power_emv(mdio, port, CPPHY_POWER_MODE_ON);
                return;
            }
            DEB_INFOTRC("[%s] port %u: CPPHY_POWER_MODE_ON_MDIX\n", __FUNCTION__, port);
            mdio_write(mdio, port, 0x14, 0x2c);
            mdio_write(mdio, port, 0x10, 0x820);
            mdio_write(mdio, port, 0x0,  0xffff);
            break;

        case CPPHY_POWER_MODE_LINK_LOST:
            /* Do nothing */
            break;

        default:
            DEB_ERR("[%s] port %u: Illegal mode (%u)\n", __FUNCTION__, port, mode);
            return;
    }
    cpmac_global.power.port_mode[port] = mode;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar_vlan_clear(cpphy_mdio_t *mdio) {
    if(unlikely(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE)))
        return; /*--- CPMAC_ERR_NO_VLAN_POSSIBLE; ---*/

    ar8216_wait_VLAN_table(mdio);
    ar8216_mdio_write32(mdio, 
                        AR8216_GLOBAL_VLAN_TABLE_1, 
                          VLAN_TABLE_1_VT_FUNC_VALUE(VLAN_TABLE_FUNC_FLUSH_ALL)
                        | VLAN_TABLE_1_VT_BUSY
                       );
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar8327_vlan_clear(cpphy_mdio_t *mdio) {
    if(unlikely(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE)))
        return; /*--- CPMAC_ERR_NO_VLAN_POSSIBLE; ---*/

    ar8327_wait_VLAN_table(mdio);
    ar8216_mdio_write32(mdio, 
                        AR8327_VTU_FUNC1, 
                          AR8327_VTU_FUNC1_FUNC_VALUE(AR8327_VTU_FUNC1_FUNC_VALUE_FLUSH_ALL)
                        | AR8327_VTU_FUNC1_BUSY
                       );
}


/*------------------------------------------------------------------------------------------*\
 * Configure VLAN on the switch                                                             *
\*------------------------------------------------------------------------------------------*/
void ar_config_vlan_group(cpphy_mdio_t   *mdio,
                          unsigned short  vid,
                          unsigned short  portset,
                          unsigned short  tagged_portset __attribute__ ((unused)),
                          unsigned char   port,
                          unsigned char   FID __attribute__ ((unused)),
                          unsigned char   use_as_default __attribute__ ((unused))) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid_group;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE))
        return; /*--- CPMAC_ERR_NO_VLAN_POSSIBLE; ---*/

    vid_group = cpphy_ar_get_vid_group(mdio);

    switch_config->map_vid_to_group[vid] = vid_group;
    assert(vid_group < 4097);
    switch_config->vidgroup[vid_group].port_in = port;
    switch_config->vidgroup[vid_group].vid = vid;
    switch_config->vidgroup[vid_group].portset = portset;
    assert(mdio->f->vlan_add);
    mdio->f->vlan_add(mdio, vid, portset);
    switch_config->map_portset_out[portset] = vid;

    DEB_INFOTRC("[%s] Setting VID %#x with portmask %#x\n", __FUNCTION__, vid, portset);
}


/*------------------------------------------------------------------------------------------*\
 * Configure VLAN on the switch                                                             *
\*------------------------------------------------------------------------------------------*/
void ar8327_config_vlan_group(cpphy_mdio_t   *mdio,
                              unsigned short  vid,
                              unsigned short  portset,
                              unsigned short  tagged_portset,
                              unsigned char   in_port __attribute__ ((unused)),
                              unsigned char   FID __attribute__ ((unused)),
                              unsigned char   use_as_default __attribute__ ((unused))) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned int RegData0, RegData1;
    unsigned char port;
    unsigned short vid_group;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE))
        return; /*--- CPMAC_ERR_NO_VLAN_POSSIBLE; ---*/

    ar8327_del_vlan_vid(mdio, vid);
    ar8327_wait_VLAN_table(mdio);

    vid_group = cpphy_ar_get_vid_group(mdio);

    switch_config->map_vid_to_group[vid] = vid_group;
    assert(vid_group < 4097);
    switch_config->vidgroup[vid_group].port_in = in_port;
    switch_config->vidgroup[vid_group].vid = vid;
    switch_config->vidgroup[vid_group].portset = portset;

    RegData0 = AR8327_VTU_FUNC0_VALID;
    for(port = 0; port < 7; port++) {
        if(portset & (1 << port)) {
            if(tagged_portset & (1 << port)) {
                /*--- RegData0 |= AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE(port, AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE_UNMODIFIED); ---*/
                RegData0 |= AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE(port, AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE_TAGGED);
            } else {
                RegData0 |= AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE(port, AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE_UNTAGGED);
            }
        } else {
            RegData0 |= AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE(port, AR8327_VTU_FUNC0_EG_VLAN_MODE_VALUE_NOT_MEMBER);
        }
    }
    ar8216_mdio_write32(mdio, AR8327_VTU_FUNC0, RegData0);

    RegData1 =   0
               | AR8327_VTU_FUNC1_FUNC_VALUE(AR8327_VTU_FUNC1_FUNC_VALUE_LOAD_ENTRY)
               | AR8327_VTU_FUNC1_VID_VALUE(vid)
               | AR8327_VTU_FUNC1_BUSY;
    ar8216_mdio_write32(mdio, AR8327_VTU_FUNC1, RegData1);

    switch_config->map_portset_out[portset] = vid;

    DEB_INFOTRC("[%s] Setting VID %#x with portmask %#x\n", __FUNCTION__, vid, portset);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned short cpphy_ar_get_vid_group(cpphy_mdio_t *mdio) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid_group;

    for(vid_group = 0; vid_group < switch_config->max_vid_groups; vid_group++) {
        if(!switch_config->vidgroup[vid_group].used) {
            break;
        }
    }
    if(vid_group >= switch_config->max_vid_groups) {
        DEB_ERR("[%s] Error! No free VLAN groups! Using 0 anyway!\n", __FUNCTION__);
        vid_group = 0;
    }
    switch_config->vidgroup[vid_group].used = 1;
    return vid_group;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned short cpphy_ar_get_new_vid(cpphy_mdio_t  *mdio,
                                    unsigned char  device __attribute__ ((unused))) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid;

    for(vid = 4; vid < 4096; vid++) {
        if(!cpphy_switch_check_assigned_vid(mdio, vid)) {
            switch_config->assigned_vids[switch_config->assigned_vids_count++] = vid;
            return vid;
        }
    }
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_ar_set_default_vid(cpphy_mdio_t   *mdio, 
                              unsigned char   device, 
                              unsigned short  portset) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid = 0, port, first_port;

    first_port = 1;
    for(port = 0; port < AVM_CPMAC_MAX_PORTS; port++) {
        if((portset & (1 << port)) == 0) {
            continue; /* Port is not in portset */
        }
        switch_config->port[port].port_vlan_mask = portset;
        if(port == switch_config->cpu_port)  {
            continue; /* No tag for the CPU port needed */
        }
        if(first_port) {
            vid = cpphy_ar_get_new_vid(mdio, device);
            if(vid == 0) {
                DEB_ERR("[%s] Error! No unassigned VID (for default) available!\n", __FUNCTION__);
                return;
            }
            assert(mdio->f->config_vlan_group);
            DEB_INFOTRC("[%s] Setting default VID %#x for '%s' (portset %#x)\n",
                        __FUNCTION__, vid, mdio->switch_config.device[device].name, portset);
            mdio->f->config_vlan_group(mdio, vid, portset, 0, port, 0, first_port);
            switch_config->port[switch_config->cpu_port].vid = vid;
            switch_config->device[device].vid = vid;
        }
        switch_config->port[port].vid = vid;
        first_port = 0;
    }
    /*--- switch_config->port[switch_config->cpu_port].keep_tag_outgoing = 1; ---*/
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar_config_ports(cpphy_mdio_t *mdio) {
    t_cpphy_switch_config *switch_config = &mdio->switch_config;
    unsigned int port, RegData;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE))
        return; /*--- CPMAC_ERR_NO_VLAN_POSSIBLE; ---*/

    assert(switch_config->ports < 7);

    for(port = 0; port < switch_config->ports; port++) {
        if(!(switch_config->used_portset & (1 << port)))
            continue;
        RegData = ar8216_mdio_read32(mdio,
                                     (port + 1) * 0x100 + AR8216_PORT_CONTROL_OFFSET);
        RegData &= ~(ATH_PORT_CTRL_EG_VLAN_MODE_MASK | ATH_PORT_CTRL_PORTSTATE_MASK);
        if(port == switch_config->cpu_port) {
            /* Keep original tagging from input port on the CPU port        *\
             * Port identification is done via MAC table, so incoming       *
             * untagged packets can remain untagged. This should circumvent *
            \* the tagging problem of the AR8216                            */
            RegData |=   ATH_PORT_CTRL_EG_VLAN_MODE(ATH_PORT_CTRL_EG_VLAN_MODE_UNMODIFIED)
                       | ATH_PORT_CTRL_PORTSTATE_FORWARD;
        } else {
            RegData |=   ATH_PORT_CTRL_EG_VLAN_MODE(  (switch_config->port[port].keep_tag_outgoing) 
                                                    ? ATH_PORT_CTRL_EG_VLAN_MODE_TAGGED
                                                    : ATH_PORT_CTRL_EG_VLAN_MODE_UNTAGGED)
                       | ATH_PORT_CTRL_PORTSTATE_FORWARD;
        }
        ar8216_mdio_write32(mdio, 
                            (port + 1) * 0x100 + AR8216_PORT_CONTROL_OFFSET,
                            RegData);
        /* Current port should not be in port VLAN mask for Atheros switches */
        /* Stay clear of changing reserved bits! */
        switch_config->port[port].port_vlan_mask &= ~(1 << port);
        DEB_TRC("[%s] Set port %u: %s tagging, VID %#x (%u), portset %#x\n",
                __FUNCTION__,
                port,
                switch_config->port[port].keep_tag_outgoing ? "keep" : "remove",
                switch_config->port[port].vid,
                switch_config->port[port].vid,
                switch_config->port[port].port_vlan_mask);
        if(cpmac_global.cpphy[mdio->phy_num].config->type == CPMAC_PHY_TYPE_AR8226) {
            RegData = ar8216_mdio_read32(mdio, (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET);
            RegData &= ~(ATH_PORT_VLAN_VID_MASK | ATH_PORT_VLAN_CVID_MASK);
            RegData |=   ATH_PORT_VLAN_VID_VALUE(switch_config->port[port].vid)
                       | ATH_PORT_VLAN_CVID_VALUE(switch_config->port[port].vid);
            ar8216_mdio_write32(mdio, (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET, RegData);

            RegData = ar8216_mdio_read32(mdio, (port + 1) * 0x100 + AR8226_PORT_VLAN2_OFFSET);
            RegData &= ~(ATH_PORT_VLAN2_VID_MEM_MASK | ATH_PORT_VLAN2_8021_QMODE_MASK);
            RegData |= ATH_PORT_VLAN2_VID_MEM_VALUE(switch_config->port[port].port_vlan_mask);
            
            if(   (port == switch_config->cpu_port) /* Default tagging for faster traffic */
               || (switch_config->port[port].use_tag_incoming) /* Added port to WAN with tagging */
              ) {
                RegData |= ATH_PORT_VLAN2_8021_QMODE_VALUE(ATH_PORT_VLAN2_8021_QMODE_FALLBACK);
            } else {
                RegData |= ATH_PORT_VLAN2_8021_QMODE_VALUE(ATH_PORT_VLAN2_8021_QMODE_DISABLE);
            }
            ar8216_mdio_write32(mdio, (port + 1) * 0x100 + AR8226_PORT_VLAN2_OFFSET, RegData);
            DEB_TRC("[%s] Port VLAN register port %u: %#x %#x\n",
                    __FUNCTION__,
                    port,
                    ar8216_mdio_read32(mdio, 
                                       (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET),
                    ar8216_mdio_read32(mdio, 
                                       (port + 1) * 0x100 + AR8226_PORT_VLAN2_OFFSET));
        } else {
            RegData = ar8216_mdio_read32(mdio, (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET);
            RegData &= ~(ATH_PORT_VLAN_VID_MASK | ATH_PORT_VLAN_VID_MEM_MASK | ATH_PORT_VLAN_8021_QMODE_MASK);
            RegData |=   ATH_PORT_VLAN_VID_VALUE(switch_config->port[port].vid)
                       | ATH_PORT_VLAN_VID_MEM_VALUE(switch_config->port[port].port_vlan_mask)
                       | ATH_PORT_VLAN_8021_QMODE_VALUE(ATH_PORT_VLAN_8021_QMODE_FALLBACK);
            ar8216_mdio_write32(mdio, (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET, RegData);
            DEB_TRC("[%s] Port VLAN register port %u: %#x\n",
                    __FUNCTION__,
                    port,
                    ar8216_mdio_read32(mdio, 
                                       (port + 1) * 0x100 + AR8216_PORT_VLAN_OFFSET));
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar8327_config_ports(cpphy_mdio_t *mdio) {
    t_cpphy_switch_config *switch_config = &mdio->switch_config;
    unsigned int port, RegData;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE))
        return; /*--- CPMAC_ERR_NO_VLAN_POSSIBLE; ---*/

    assert(switch_config->ports < 7);

    /* Set the default VIDs for the ports */
    for(port = 0; port < switch_config->ports; port++) {
        union ar8327_port_lookup_ctrl port_lookup;

        if(!(switch_config->used_portset & (1 << port)))
            continue;
        RegData = ar8216_mdio_read32(mdio, AR8327_PORT_VLAN_CTRL0(port));
        RegData &= ~(  AR8327_PORT_VLAN_CTRL0_DEFAULT_SVID_MASK
                     | AR8327_PORT_VLAN_CTRL0_ING_PORT_SPRI_MASK
                     | AR8327_PORT_VLAN_CTRL0_DEFAULT_CVID_MASK
                     | AR8327_PORT_VLAN_CTRL0_ING_PORT_CPRI_MASK);
        RegData |=   AR8327_PORT_VLAN_CTRL0_DEFAULT_SVID_VALUE(switch_config->port[port].vid)
                   | AR8327_PORT_VLAN_CTRL0_DEFAULT_CVID_VALUE(switch_config->port[port].vid);
        ar8216_mdio_write32(mdio, AR8327_PORT_VLAN_CTRL0(port), RegData);

        /* Current port should not be in port VLAN mask for Atheros switches */
        switch_config->port[port].port_vlan_mask &= ~(1 << port);

        /* Stay clear of changing reserved bits! */
        port_lookup.reg = ar8216_mdio_read32(mdio, AR8327_PORT_LOOKUP_CTRL(port));
        port_lookup.bits.port_vid_mem = switch_config->port[port].port_vlan_mask & 0x7f;
        if(   (port == switch_config->cpu_port) /* Default tagging for faster traffic */
           || (switch_config->port[port].use_tag_incoming) /* Added port to WAN with tagging */
          ) {
            port_lookup.bits.vlan_mode = ar8327_vlan_mode_fallback;
        } else {
            port_lookup.bits.vlan_mode = ar8327_vlan_mode_disable;
        }
        ar8216_mdio_write32(mdio, AR8327_PORT_LOOKUP_CTRL(port), port_lookup.reg);

        RegData = ar8216_mdio_read32(mdio, AR8327_PORT_VLAN_CTRL1(port));
        RegData &= ~AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_MASK;
        if(port == switch_config->cpu_port) {
            RegData |= AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_VALUE(AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_VALUE_UNTOUCHED);
        } else {
            if(switch_config->port[port].keep_tag_outgoing) {
                RegData |= AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_VALUE(AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_VALUE_TAGGED);
            } else {
                RegData |= AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_VALUE(AR8327_PORT_VLAN_CTRL1_EG_VLAN_MODE_VALUE_UNTAGGED);
            }
        }
        ar8216_mdio_write32(mdio, AR8327_PORT_VLAN_CTRL1(port), RegData);
    }

    for(port = 0; port < switch_config->ports; port++) {
        if(!(switch_config->used_portset & (1 << port)))
            continue;
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void adm_switch_wan_keep_tagging_ar8x16(cpphy_mdio_t *mdio, unsigned char port) {
    unsigned int RegData;

    RegData = ar8216_mdio_read32(mdio, (port + 1) * AR8216_PORT0_CONTROL_BASIS + AR8216_PORT_CONTROL_OFFSET);
    RegData &= ~(3u << 8); /* Delete VLAN status */
    RegData |= ATH_PORT_CTRL_EG_VLAN_MODE(  (mdio->switch_status.port[port].keep_tag_outgoing) 
                                          ? ATH_PORT_CTRL_EG_VLAN_MODE_TAGGED
                                          : ATH_PORT_CTRL_EG_VLAN_MODE_UNTAGGED);
    ar8216_mdio_write32(mdio, (port + 1) * AR8216_PORT0_CONTROL_BASIS + AR8216_PORT_CONTROL_OFFSET, RegData);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_ar8316_mirror_port(cpphy_mdio_t *mdio) {
    unsigned int RegData;
    unsigned char port;
    unsigned char enable_ingress   = mdio->switch_config.mirror_port.enable_ingress;
    unsigned char enable_egress    = mdio->switch_config.mirror_port.enable_egress;
    unsigned char mirror_from_port = mdio->switch_config.mirror_port.mirror_from_port;
    unsigned char mirror_to_port   = mdio->switch_config.mirror_port.mirror_to_port;

    DEB_TEST("[%s] Mirror from %u to %u - %s %s\n", __FUNCTION__,
             mirror_from_port, mirror_to_port,
             enable_ingress ? "ingress" : "",
             enable_egress ? "egress" : "");
    if((mirror_to_port > 5) || (mirror_from_port > 5)) {
        DEB_WARN("[%s] Error! There is no port %u to mirror from/to.\n", __FUNCTION__, mirror_from_port);
        return;
    }
    if(mirror_from_port == mirror_to_port) {
        DEB_WARN("[%s] Error! Mirroring to the same port is not allowed!\n", __FUNCTION__);
        return;
    }
    if(cpmac_global.cpphy[mdio->phy_num].config->type == CPMAC_PHY_TYPE_AR8327) {
        union ar8327_port_lookup_ctrl port_lookup;
        union ar8327_port_hol_ctrl1 port_hol_ctrl1;

        for(port = 0; port <= 5; port++) {
            port_lookup.reg = ar8216_mdio_read32(mdio, AR8327_PORT_LOOKUP_CTRL(port));
            port_hol_ctrl1.reg = ar8216_mdio_read32(mdio, AR8327_PORT_HOL_CTRL1(port));

            port_lookup.bits.ing_mirror_en = 0;
            port_hol_ctrl1.bits.eg_mirror_en = 0;
            if(port == mirror_from_port) {
                port_lookup.bits.ing_mirror_en = enable_ingress;
                port_hol_ctrl1.bits.eg_mirror_en = enable_egress;
            }
            ar8216_mdio_write32(mdio, AR8327_PORT_LOOKUP_CTRL(port), port_lookup.reg);
            ar8216_mdio_write32(mdio, AR8327_PORT_HOL_CTRL1(port), port_hol_ctrl1.reg);
        }

        RegData = ar8216_mdio_read32(mdio, AR8327_GLOBAL_FW_CTRL0);
        RegData &= ~AR8327_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM_MASK;
        if(enable_ingress || enable_egress) {
            RegData |= AR8327_GLOBAL_FW_CTRL0_MIRROR_PORT_NUM_VALUE(mirror_to_port);
        }
        ar8216_mdio_write32(mdio, AR8327_GLOBAL_FW_CTRL0, RegData);
    } else {
        for(port = 0; port <= 5; port++) {
            RegData = ar8216_mdio_read32(mdio, (port + 1) * AR8216_PORT0_CONTROL_BASIS + AR8216_PORT_CONTROL_OFFSET);
            if(port == mirror_from_port) {
                RegData &= ~(ATH_PORT_CTRL_ING_MIRROR_EN | ATH_PORT_CTRL_EG_MIRROR_EN);
                RegData |= enable_ingress ? ATH_PORT_CTRL_ING_MIRROR_EN : 0;
                RegData |= enable_egress ? ATH_PORT_CTRL_EG_MIRROR_EN : 0;
                ar8216_mdio_write32(mdio, (port + 1) * AR8216_PORT0_CONTROL_BASIS + AR8216_PORT_CONTROL_OFFSET, RegData);
            }
        }

        RegData = ar8216_mdio_read32(mdio, AR8216_GLOBAL_CPUPORT);
        RegData &= ~AR8216_GLOBAL_CPUPORT_MIRROR_PORT_NUM_MASK;
        if(enable_ingress || enable_egress) {
            RegData |= AR8216_GLOBAL_CPUPORT_MIRROR_PORT_NUM(mirror_to_port);
        }
        ar8216_mdio_write32(mdio, AR8216_GLOBAL_CPUPORT, RegData);
    }

    if(enable_ingress || enable_egress) {
        DEB_INFO("[%s] Enabling mirroring from port %u to port %u: %s %s\n",
                 __FUNCTION__, mirror_from_port, mirror_to_port, 
                 enable_ingress ? "ingress" : "", 
                 enable_egress ? "egress" : "");
    } else {
        DEB_INFO("[%s] Disabling port mirroring\n", __FUNCTION__);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_ar8216_set_igmp_fwd(cpphy_mdio_t *mdio, unsigned int fwd_portmap) {
    unsigned char port;

    DEB_INFO("[%s] Setting IGMP forwarding for this portmap: %#x\n", __FUNCTION__, fwd_portmap);

    for(port = 1; port < 6; port++) {
        unsigned char enable = fwd_portmap & (1 << (port -1));
        unsigned int RegData;

        RegData = ar8216_mdio_read32(mdio, (port + 1) * 0x100 + AR8216_PORT_CONTROL_OFFSET);
        if(enable) {
            RegData |= ATH_PORT_CTRL_IGMP_MLD_EN;
        } else {
            RegData &= ~ATH_PORT_CTRL_IGMP_MLD_EN;
        }
        ar8216_mdio_write32(mdio, (port + 1) * 0x100 + AR8216_PORT_CONTROL_OFFSET, RegData);
        DEB_INFO("[%s] %sabling IGMP forwarding for ethernet port %u\n", __FUNCTION__, enable ? "En" : "Dis", port - 1);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_ar8327_set_igmp_fwd(cpphy_mdio_t *mdio, unsigned int fwd_portmap) {
    unsigned char port;

    DEB_INFO("[%s] Setting IGMP forwarding for this portmap: %#x\n", __FUNCTION__, fwd_portmap);

    for(port = 1; port < 6; port++) {
        unsigned char enable = fwd_portmap & (1 << (port - 1));

        if(port < 4) {
            unsigned int RegData = ar8216_mdio_read32(mdio, AR8327_FRAM_ACK_CTRL0);
            if(enable) {
                RegData |= AR8327_FRAM_ACK_CTRL0_IGMP_MLD_EN(port);
            } else {
                RegData &= ~AR8327_FRAM_ACK_CTRL0_IGMP_MLD_EN(port);
            }
            ar8216_mdio_write32(mdio, AR8327_FRAM_ACK_CTRL0, RegData);
        } else {
            unsigned int RegData = ar8216_mdio_read32(mdio, AR8327_FRAM_ACK_CTRL1);
            if(enable) {
                RegData |= AR8327_FRAM_ACK_CTRL1_IGMP_MLD_EN(port);
            } else {
                RegData &= ~AR8327_FRAM_ACK_CTRL1_IGMP_MLD_EN(port);
            }
            ar8216_mdio_write32(mdio, AR8327_FRAM_ACK_CTRL1, RegData);
        }
        DEB_INFOTRC("[%s] %sabling IGMP forwarding for ethernet port %u\n", __FUNCTION__, enable ? "En" : "Dis", port - 1);
    }

}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t ar8327_add_port_to_wan(cpphy_mdio_t *mdio, unsigned char port, unsigned short vid) {
    unsigned int group, found = 12345;
    t_cpphy_switch_config *switch_config = &mdio->switch_config;
    unsigned char wanport = switch_config->wanport;
    unsigned short tagged_portset;
    unsigned char use_tag_incoming = (vid == 0) ? 0 : 1;

    DEB_INFOTRC("[%s] Asked to add port %u to WAN for VLAN ID %u (%#x)\n",
                __FUNCTION__, port, vid, vid);

    /* Check port */
    if(port > 3) {
        DEB_ERR("[%s] Illegal port %u given\n", __FUNCTION__, port);
        return CPMAC_ERR_EXCEEDS_LIMIT;
    }
    port += switch_config->external_port_offset;

    if(port >= AVM_CPMAC_MAX_PORTS) {
        DEB_ERR("[%s] Port + port offset exceeds limit! (%u >= %u)\n", 
                __FUNCTION__, port, AVM_CPMAC_MAX_PORTS);
        return CPMAC_ERR_EXCEEDS_LIMIT;
    }
    
    /* This function is only intended for a kind of ATA mode */
    if(wanport == 0xff) {
        DEB_ERR("[%s] No WAN port defined!\n", __FUNCTION__);
        return CPMAC_ERR_ILL_CONTROL;
    }
    /* Find group for this VID */
    for(group = 0; group < switch_config->max_vid_groups; group++) {
        if(!switch_config->vidgroup[group].used) 
            continue;
        /* VID == 0 => Use first VLAN group that includes the WAN port */
        if((vid == 0) && (switch_config->vidgroup[group].portset & (1 << wanport))) {
            vid = switch_config->vidgroup[group].vid & 0xfff; /* & 0xfff for Klocwork */
        }
        if(switch_config->vidgroup[group].vid != vid) 
            continue;
        found = group;
        DEB_TRC("[%s] Found group %u for VID %u\n", __FUNCTION__, group, vid);
        break;
    }
    if(found == 12345) {
        DEB_ERR("[%s] Unknown VID %u given!\n", __FUNCTION__, vid);
        return CPMAC_ERR_EXCEEDS_LIMIT;
    }

    assert(mdio->f->config_vlan_group);
    /* Remove port from generic ethernet VLAN */
    for(group = 0; group < switch_config->max_vid_groups; group++) {
        unsigned int bit, count = 0;

        /* Only valid groups are interesting */
        if(!switch_config->vidgroup[group].used) 
            continue;

        /* Group should include port and exclude wanport */
        if(   !(switch_config->vidgroup[group].portset & (1 << port))
           ||  (switch_config->vidgroup[group].portset & (1 << wanport))) {
            continue;
        }

        /* Group should include more than two ports */
        for(bit = 0; bit < 16; bit++) {
            if(switch_config->vidgroup[group].portset & (1 << bit))
                count++;
        }
        if(count < 3) {
            DEB_INFOTRC("[%s] Not removing port from portset %#x, because only one port would be left!\n",
                        __FUNCTION__, switch_config->vidgroup[group].portset);
            continue;
        }

        /* Remove port from portmasks */
        switch_config->vidgroup[group].portset  &= ~(1 << port);
        tagged_portset =   switch_config->vidgroup[group].portset 
                         & (  (1 << switch_config->cpu_port)
                            | (switch_config->wanport_keeptags ? (1 << switch_config->wanport) : 0));
        mdio->f->config_vlan_group(mdio, switch_config->vidgroup[group].vid,
                                   switch_config->vidgroup[group].portset, 
                                   tagged_portset,
                                   switch_config->vidgroup[group].port_in, 
                                   switch_config->vidgroup[group].FID, 0);
    }

    /* Add port to given VID */
    switch_config->vidgroup[found].portset |= 1 << port;
    tagged_portset =   switch_config->vidgroup[found].portset 
                     & (  (1 << switch_config->cpu_port)
                        | (switch_config->wanport_keeptags ? (1 << switch_config->wanport) : 0));
    mdio->f->config_vlan_group(mdio, switch_config->vidgroup[found].vid,
                               switch_config->vidgroup[found].portset, 
                               tagged_portset,
                               switch_config->vidgroup[found].port_in, 
                               switch_config->vidgroup[found].FID, 0);


    /* Change default VID for the given port */
    DEB_INFOTRC("[%s] Setting default VID %u for port %u\n", __FUNCTION__, vid, port);
    switch_config->port[port].vid = vid;
    switch_config->port[port].keep_tag_outgoing = 0;
    switch_config->port[port].use_tag_incoming = use_tag_incoming;
    switch_config->port[wanport].use_tag_incoming = use_tag_incoming;

    for(port = 0; port < switch_config->ports; port++) {
        unsigned short portset;
        vid     = switch_config->port[port].vid;
        group   = switch_config->map_vid_to_group[vid];
        portset = switch_config->vidgroup[group].portset & 0xffff;
        switch_config->port[port].port_vlan_mask = portset;
    }
    mdio->f->config_ports(mdio);

    return CPMAC_ERR_NOERR;
}


