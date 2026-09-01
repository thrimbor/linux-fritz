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

#ifndef _CPMAC_PRODUCT_CONF_H_
#define _CPMAC_PRODUCT_CONF_H_

/* Attention! The format of this file has to match the init_cpmac.pl script! */

/* init_cpmac defines */
#define AVM_CPMAC_MAX_HWREV_LENGTH 20u
#define AVM_CPMAC_MAX_PHYS 2u

/* init_cpmac PHY_TYPES */
typedef enum {
    CPMAC_PHY_TYPE_NONE,
    CPMAC_PHY_TYPE_INTERNAL,
    CPMAC_PHY_TYPE_DUMMY,
    CPMAC_PHY_TYPE_ADM6996FC,
    CPMAC_PHY_TYPE_TANTOS,
    CPMAC_PHY_TYPE_AR8216,
    CPMAC_PHY_TYPE_AR8226,
    CPMAC_PHY_TYPE_AR8316,
    CPMAC_PHY_TYPE_AR8327,
    CPMAC_PHY_TYPE_ADM7001,
    CPMAC_PHY_TYPE_VITESSE,
    CPMAC_PHY_TYPE_11G
} cpmac_phy_types;

/* init_cpmac PHY_MODES */
typedef enum {
    CPMAC_PHY_MODE_DEFAULT,
    CPMAC_PHY_MODE_PASSIVE,
    CPMAC_PHY_MODE_7170,
    CPMAC_PHY_MODE_VINAX5,
    CPMAC_PHY_MODE_VINAX7,
    CPMAC_PHY_MODE_PROFI1,
    CPMAC_PHY_MODE_PROFI2,
    CPMAC_PHY_MODE_AR8216,
    CPMAC_PHY_MODE_MAGPIE
} cpmac_phy_configs;

typedef struct {
    cpmac_phy_types   type;
    cpmac_phy_configs config;
    unsigned short    ports;
    unsigned short    reset_pin;
    unsigned short    mii_portmask; /* Switch ports connected via MII that report no link status */
} cpmac_product_config_phy_struct;

typedef struct {
    char                            hwrev[AVM_CPMAC_MAX_HWREV_LENGTH];
    unsigned char                   phys;
    cpmac_product_config_phy_struct phy[AVM_CPMAC_MAX_PHYS];
} cpmac_product_config_struct;

typedef struct {
    unsigned int number_of_products;
    cpmac_product_config_struct product[];
} cpmac_product_struct;


#endif /*--- #ifndef _CPMAC_PRODUCT_CONF_H_ ---*/

