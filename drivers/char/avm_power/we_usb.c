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

#include <linux/kernel.h>
#include "wyatt_earp.h"

#define AHCI_GCR_globalReset_FLAG	0x1
#define AHCI_GlobalControlReg       (FPGA_BASE + 0x8000 + 0x4000 + 0x4)
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int SuspendUSB(int state){
    FPGA_WRITE_DWORD(AHCI_GlobalControlReg, AHCI_GCR_globalReset_FLAG);
    state = state;
    DBG_WE_TRC(("Suspend USB\n"));
    return 0;
}
