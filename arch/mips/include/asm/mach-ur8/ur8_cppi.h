/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2013 AVM GmbH <fritzbox_info@avm.de>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <asm/mach-ur8/ur8.h>
#include <asm/mach-ur8/hw_nwss.h>


/*------------------------------------------------------------------------------------------*\
 * Initialize teardown descriptors
 * Get Tx or Rx teardown descriptor for given channel
\*------------------------------------------------------------------------------------------*/
extern unsigned int ur8_teardown_init(void) __attribute__ ((weak));
extern void *ur8_get_tx_teardown_BD(unsigned char chNum) __attribute__ ((weak));
extern void *ur8_get_rx_teardown_BD(unsigned char chNum) __attribute__ ((weak));


