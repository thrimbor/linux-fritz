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

#ifndef __wyatt_earp_h__
#define __wyatt_earp_h__

int SuspendWLAN(int state);
int SuspendVLYNC(int state);
int SuspendLEDs(int state);
int SuspendUSB(int state);
int SuspendISDN(int state);
int SuspendEthernet(int state);
int SuspendXILINX(int state);

#define FPGA_BASE           0xBE000000
#define FPGA_WRITE_DWORD(addr, value)  (*(( volatile unsigned int *)(addr)) = value)
#define FPGA_READ_DWORD(addr)           *(( volatile unsigned int *)(addr))
#define DBG_TRC(a)          printk a
#define DBG_ERR(a)          printk a

/*--- Output-Fkt getriggert vom TIATM ---*/
extern void Wyatt_Earp(int Mask);

/*--- #define WE_DEBUG ---*/

#if defined(WE_DEBUG)
extern void sio_puts(char *chr);

#define DBG_WE_TRC(a) sio_puts(a)
#define DBG_WE_ERR(a) sio_puts(a)
#else
#define DBG_WE_TRC(a) 
#define DBG_WE_ERR(a)
#endif/*--- #else ---*/

#endif/*--- #ifndef __wyatt_earp_h__ ---*/
