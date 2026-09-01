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
#include "wyatt_earp.h"

#define ISDN_Common_Config              (FPGA_BASE + 0x64)
#define ISDN_REG_OUT                    (FPGA_BASE + 0x48)
#define ISDN_CODEC_RESET        0x1

#define ISDN_S0REG_TE_COMMAND_STATUS    (FPGA_BASE + 0x0C)
#define ISDN_S0REG_NT_COMMAND_STATUS    (FPGA_BASE + 0x1C)
#define S0_L1_DEACT_REQ        0x00000002  /* 1: Anforderung zum D-Kanalabbau */

/*-------------------------------------------------------------------------------------*\
    Achtung nicht 5010 etc! (32 Bit Zugriff)
\*-------------------------------------------------------------------------------------*/
int SuspendISDN(int state){
    FPGA_WRITE_DWORD(ISDN_Common_Config, ISDN_CODEC_RESET);

    FPGA_WRITE_DWORD(ISDN_REG_OUT, 0xF); /*--- Slics: alles 0 Pots: alles 1 ---*/

    FPGA_WRITE_DWORD(ISDN_S0REG_TE_COMMAND_STATUS, S0_L1_DEACT_REQ);
    FPGA_WRITE_DWORD(ISDN_S0REG_NT_COMMAND_STATUS, S0_L1_DEACT_REQ);

    DBG_WE_TRC(("Suspend ISDN\n"));
    state = state;
    return 0;
}
#endif/*--- #if !defined(CONFIG_MIPS_UR8) ---*/
