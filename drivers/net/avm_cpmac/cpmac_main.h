/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,2007,2008,2009,2010 AVM GmbH <fritzbox_info@avm.de>
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

#ifndef _CPMAC_MAIN_H_
#define _CPMAC_MAIN_H_
#include <linux/avm_event.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AVM_CPMAC_SWITCH_NONE     0
#define AVM_CPMAC_SWITCH_ADM6996L 1
#define AVM_CPMAC_SWITCH_ADM6996  2
#define AVM_CPMAC_SWITCH_TANTOS   3
#define AVM_CPMAC_SWITCH_AR8216   4
#define AVM_CPMAC_SWITCH_AR8316   5
#define AVM_CPMAC_SWITCH_ANY      6

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpmac_main_start_timer(void);
void cpmac_main_stop_timer(void);
int cpmac_main_timer_init (void);
void cpmac_main_handle_mdio_status_ind (cpmac_priv_t *cpmac_priv, unsigned int linked);
void cpmac_main_event_notify(void *context, enum _avm_event_id id);
void cpmac_main_event_update(void);
void cpmac_main_handle_event_update (cpmac_priv_t *cpmac_priv);
void cpmac_main_dev_init (struct net_device *dev);
void cpmac_main_probe(void);
void cpmac_mcfw_schedule_tasklet(void);

#endif
