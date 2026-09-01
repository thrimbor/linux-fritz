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

#ifndef _CPMAC_FUSIV_IF_H_
#define _CPMAC_FUSIV_IF_H_

#if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/if.h>
#include <linux/avm_event.h>

typedef unsigned int (*fusiv_mdio_read_t)(void *port, unsigned short regadr, unsigned short phyadr);
typedef void (*fusiv_mdio_write_t)(void *port, unsigned short regadr, unsigned short phyadr, unsigned short data);
typedef void (*fusiv_set_port_speed_t)(unsigned char instance, unsigned char speed, unsigned char fullduplex);

extern struct semaphore mdio_semaphore;
void cpmac_fusiv_if_rx2(struct sk_buff *skb, struct mcfw_netdriver *mcfwdrv);
void cpmac_fusiv_rx_final(struct sk_buff *skb);
void cpmac_fusiv_open(unsigned int instance);
void cpmac_set_vx180_dev(struct net_device *dev);
void cpmac_set_vx180_port(void                   *pPort,
                          unsigned int            instance,
                          fusiv_mdio_read_t       mdio_read_ptr,
                          fusiv_mdio_write_t      mdio_write_ptr,
                          fusiv_set_port_speed_t  set_port_speed);

extern int cpmac_fusiv_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd) __attribute__ ((weak));


void cpmac_fusiv_set_port_speed(unsigned char port,
                                enum _avm_event_ethernet_speed speed,
                                unsigned char fullduplex);
char *fusiv_device_name_get(unsigned int device_number);
unsigned int fusiv_device_name_cmp(char *name, unsigned int device_number);
unsigned int fusiv_get_number_of_instances(void);

#endif /*--- #if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) ---*/

#endif /*--- #ifndef _CPMAC_FUSIV_IF_H_ ---*/

