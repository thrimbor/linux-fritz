/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2006 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
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

#include <linux/autoconf.h>
#if !defined(CONFIG_MIPS_UR8)
#include <linux/kernel.h>
#include <asm/addrspace.h>
#include "wyatt_earp.h"

#if !defined(CONFIG_MIPS_OHIO)
#define CONFIG_MIPS_OHIO 0
#endif /*--- #if !defined(CONFIG_MIPS_OHIO) ---*/

/*--- #define KSEG1                   0xa0000000 ---*/
#define MDIO_BASE               (KSEG1ADDR(0x08611E00))

#define MDIO_USER_ACCESS(inst)       (*(volatile unsigned *)(MDIO_BASE + 0x80 + ((inst) ? 8 : 0)))

#define MDIO_USER_ACCESS_DATA(data)       ((data) << 0)
#define MDIO_USER_ACCESS_PHYADDR(addr)    ((addr) << 16)
#define MDIO_USER_ACCESS_DATA(data)       ((data) << 0)
#define MDIO_USER_ACCESS_REGADDR(addr)    ((addr) << 21)

#define MDIO_USER_ACCESS_WRITE            (1 << 30)
#define MDIO_USER_ACCESS_GO               (1U << 31)

#define REG_ADM_LC_PHY_CONTROL_PORT0          0x200
#define REG_ADM_LC_PHY_CONTROL_PORT1          0x220
#define REG_ADM_LC_PHY_CONTROL_PORT2          0x240
#define REG_ADM_LC_PHY_CONTROL_PORT3          0x260
#define REG_ADM_LC_PHY_CONTROL_PORT4          0x280

#define PHY_PDOWN                      (1 << 11)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void cpphy_wait_for_access_complete (unsigned int inst) {
  while (MDIO_USER_ACCESS(inst) & MDIO_USER_ACCESS_GO);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void cpphy_user_access(unsigned int inst, unsigned int method, unsigned int regadr, unsigned int phyadr, unsigned int data) {
  unsigned int  control;

    control = MDIO_USER_ACCESS_GO | (method) |
    MDIO_USER_ACCESS_REGADDR(regadr & 0x1f) |
    MDIO_USER_ACCESS_PHYADDR(phyadr & 0x1f) |
    MDIO_USER_ACCESS_DATA(data & 0xffff);
    MDIO_USER_ACCESS(inst) = control;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_user_access_write (unsigned int inst, unsigned int regadr, unsigned int phyadr, unsigned int data) {
  cpphy_wait_for_access_complete (inst);  /* Wait until UserAccess ready */
  cpphy_user_access (inst, MDIO_USER_ACCESS_WRITE, regadr, phyadr, data);
}

/*-------------------------------------------------------------------------------------*\
 * erst mal nur für Switch! - 16 Bit -Modus
\*-------------------------------------------------------------------------------------*/
int SuspendEthernet(int state) {

    /* 3070, 7170 mit OHIO, also nur einem PHY */

    cpphy_user_access_write (0, (REG_ADM_LC_PHY_CONTROL_PORT0 & 0x1F), REG_ADM_LC_PHY_CONTROL_PORT0 >> 5, PHY_PDOWN);
    cpphy_user_access_write (0, (REG_ADM_LC_PHY_CONTROL_PORT1 & 0x1F), REG_ADM_LC_PHY_CONTROL_PORT1 >> 5, PHY_PDOWN);
    cpphy_user_access_write (0, (REG_ADM_LC_PHY_CONTROL_PORT2 & 0x1F), REG_ADM_LC_PHY_CONTROL_PORT2 >> 5, PHY_PDOWN);
    cpphy_user_access_write (0, (REG_ADM_LC_PHY_CONTROL_PORT3 & 0x1F), REG_ADM_LC_PHY_CONTROL_PORT3 >> 5, PHY_PDOWN);
    cpphy_user_access_write (0, (REG_ADM_LC_PHY_CONTROL_PORT4 & 0x1F), REG_ADM_LC_PHY_CONTROL_PORT4 >> 5, PHY_PDOWN);
#if CONFIG_MIPS_OHIO != 1
    cpphy_user_access_write (1, (REG_ADM_LC_PHY_CONTROL_PORT0 & 0x1F), REG_ADM_LC_PHY_CONTROL_PORT0 >> 5, PHY_PDOWN);
    cpphy_user_access_write (1, (REG_ADM_LC_PHY_CONTROL_PORT1 & 0x1F), REG_ADM_LC_PHY_CONTROL_PORT1 >> 5, PHY_PDOWN);
    cpphy_user_access_write (1, (REG_ADM_LC_PHY_CONTROL_PORT2 & 0x1F), REG_ADM_LC_PHY_CONTROL_PORT2 >> 5, PHY_PDOWN);
    cpphy_user_access_write (1, (REG_ADM_LC_PHY_CONTROL_PORT3 & 0x1F), REG_ADM_LC_PHY_CONTROL_PORT3 >> 5, PHY_PDOWN);
    cpphy_user_access_write (1, (REG_ADM_LC_PHY_CONTROL_PORT4 & 0x1F), REG_ADM_LC_PHY_CONTROL_PORT4 >> 5, PHY_PDOWN);
#endif/*--- #if CONFIG_MIPS_OHIO != 1 ---*/
    state = state;
    DBG_WE_TRC(("SuspendEthernet\n"));
    return 0;
}
#endif/*--- #if !defined(CONFIG_MIPS_UR8) ---*/
