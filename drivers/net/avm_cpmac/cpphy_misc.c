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

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#if !defined(CONFIG_NETCHIP_ADM69961)
#define CONFIG_NETCHIP_ADM69961
#endif
#include <linux/avm_event.h>

#include "cpmac_if.h"
#include "cpmac_const.h"
#include "cpmac_debug.h"
#include "cpmac_main.h"
#include "cpphy_const.h"
#include "cpphy_types.h"
#include "cpphy_misc.h"

/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
static unsigned char cpphy_misc_str2hexnum(unsigned char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return 0;
}


/*----------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------------------*/
void cpphy_misc_str2eaddr(unsigned char *ea, unsigned char *str)
{
  int i;
  unsigned char num;

  for(i = 0; i < 6; i++) {
    if ((*str == '.') || (*str == ':'))
        str++;
    num = cpphy_misc_str2hexnum(*str++) << 4;
    num |= cpphy_misc_str2hexnum(*str++);
    ea[i] = num;
  }
}

