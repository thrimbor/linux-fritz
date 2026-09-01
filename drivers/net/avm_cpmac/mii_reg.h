/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006, ..., 2011 AVM GmbH <fritzbox_info@avm.de>
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


/****************************************************************************
**      AR8216 support
*****************************************************************************/
#ifndef _MII_REG_H_
#define _MII_REG_H_

/* PHY control */
#define MII_REG_CONTROL                     0x00u
#  define MII_REG_CONTROL_RESET             (1u << 15)  
#  define MII_REG_CONTROL_LOOPBACK          (1u << 14)  
#  define MII_REG_CONTROL_SPEEDSEL_LSB      (1u << 13) 
#  define MII_REG_CONTROL_AUTONEG_ENABLE    (1u << 12)
#  define MII_REG_CONTROL_POWERDOWN         (1u << 11)
#  define MII_REG_CONTROL_ISOLATE           (1u << 10)
#  define MII_REG_CONTROL_AUTONEG_RESTART   (1u <<  9)
#  define MII_REG_CONTROL_FULLDUPLEX        (1u <<  8)
#  define MII_REG_CONTROL_COLLISION_TEST    (1u <<  7)
#  define MII_REG_CONTROL_SPEEDSEL_MSB      (1u <<  6)

/* PHY status */
#define MII_REG_STATUS                      0x01u
#  define MII_REG_STATUS_100_T4             (1u << 15)
#  define MII_REG_STATUS_100_X_FD           (1u << 14)
#  define MII_REG_STATUS_100_X_HD           (1u << 13)
#  define MII_REG_STATUS_10_FD              (1u << 12)
#  define MII_REG_STATUS_10_HD              (1u << 11)
#  define MII_REG_STATUS_100_T2_FD          (1u << 10)
#  define MII_REG_STATUS_100_T2_HD          (1u <<  9)
#  define MII_REG_STATUS_EXTENDED_STATUS    (1u <<  8)
#  define MII_REG_STATUS_MF_PREAM_SUPPR     (1u <<  6)
#  define MII_REG_STATUS_AUTONEG_COMPLETE   (1u <<  5)
#  define MII_REG_STATUS_REMOTE_FAULT       (1u <<  4)
#  define MII_REG_STATUS_AUTONEG_ABLE       (1u <<  3)
#  define MII_REG_STATUS_LINK               (1u <<  2)
#  define MII_REG_STATUS_JABBER             (1u <<  1)
#  define MII_REG_STATUS_EXT_CAP            (1u <<  0)

/* PHY identifier */
#define MII_REG_PHY_ID1                     0x02u
#define MII_REG_PHY_ID2                     0x03u

/* PHY autonegotiation advertisement */
#define MII_REG_AUTONEG_ADV                 0x04u
#define MII_REG_AUTONEG_PARTNER             0x05u
#define MII_REG_AUTONEG_EXP                 0x06u
#define MII_REG_AUTONEG_NEXT_PAGE           0x07u
#define MII_REG_AUTONEG_PARTNER_NEXT_PAGE   0x08u
#define MII_REG_1000BASET_CTRL              0x09u
#  define MII_REG_1000BASET_CTRL_ADV_HD     (1u <<  8)
#  define MII_REG_1000BASET_CTRL_ADV_FD     (1u <<  9)
#  define MII_REG_1000BASET_CTRL_MSPT       (1u << 10)
#  define MII_REG_1000BASET_CTRL_MS         (1u << 11)
#  define MII_REG_1000BASET_CTRL_MSEN       (1u << 12)

#define MII_REG_MASTER_SLAVE_CONTROL        0x10u
#define MII_REG_EXTENDED_STATUS             0x15u

#endif /*--- #define _MII_REG_H_ ---*/

