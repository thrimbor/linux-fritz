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

#ifndef _CPMAC_PUMA_IF_H_
#define _CPMAC_PUMA_IF_H_

#if defined(CONFIG_ARCH_PUMA5)
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/if.h>
/*--- #include <linux/etherdevice.h> ---*/

extern struct semaphore mdio_semaphore;
void cpmac_puma_init(struct net_device *dev, void *phy_dev);
void cpmac_puma_set_dev(struct net_device *dev);
struct net_device *cpmac_puma_get_dev(void);
void cpmac_puma_open(void);
void cpmac_puma_close(void);
void cpmac_puma_if_rx(struct sk_buff *skb);
void cpmac_puma_rx_final(struct sk_buff *skb);
void cpmac_puma_transfer_startstop(unsigned int start);

extern int cpmac_puma_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd) __attribute__ ((weak));

void cpmac_puma_printk(const char *text, unsigned int line);

/* In avalanche_cpgmac_f/switch.c */
void puma_mdio_write(void *phy_dev, 
                     unsigned short regadr,
                     unsigned short phyadr,
                     unsigned short value);
unsigned int puma_mdio_read(void *phy_dev, unsigned short address, unsigned short phy);
void cpmac_puma_set_port_speed(unsigned char port,
                               unsigned char speed,
                               unsigned char fullduplex);
#endif /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/

#endif /*--- #ifndef _CPMAC_PUMA_IF_H_ ---*/

