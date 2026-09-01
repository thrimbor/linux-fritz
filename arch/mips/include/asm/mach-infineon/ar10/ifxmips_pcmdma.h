/*--------------------------------------------------------------------------------*\
 *   Copyright (C) 2012 AVM GmbH <fritzbox_info@avm.de>
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
\*--------------------------------------------------------------------------------*/
#ifndef __ifxmips_pcmdma_h__
#define __ifxmips_pcmdma_h__

#define IFX_DMA_FCC_CONTROL_BASE               (KSEG1 | 0x1F203000)

#define IFX_DMA_FCC_XXX_DW0_OWN                (0x1 << 31)
#define IFX_DMA_FCC_XXX_DW0_DATALENGTH(len)    ((len) & 0xFFFF)

#define IFX_DMA_FCC_RXI_DW0                    (IFX_DMA_FCC_CONTROL_BASE + 0x60)

#define IFX_DMA_FCC_RXI_DW1                    (IFX_DMA_FCC_CONTROL_BASE + 0x64)

#define IFX_DMA_FCC_RXI_SH_DW0                 (IFX_DMA_FCC_CONTROL_BASE + 0x68)
#define IFX_DMA_FCC_RXI_SH_DW1                 (IFX_DMA_FCC_CONTROL_BASE + 0x6C)

#define IFX_DMA_FCC_TXI_DW0                    (IFX_DMA_FCC_CONTROL_BASE + 0x70)

#define IFX_DMA_FCC_TXI_DW1                    (IFX_DMA_FCC_CONTROL_BASE + 0x74)

#define IFX_DMA_FCC_TXI_SH_DW0                 (IFX_DMA_FCC_CONTROL_BASE + 0x78)
#define IFX_DMA_FCC_TXI_SH_DW1                 (IFX_DMA_FCC_CONTROL_BASE + 0x7C)

#define IFX_DMA_FCC_RXE_DW0                    (IFX_DMA_FCC_CONTROL_BASE + 0x80)
#define IFX_DMA_FCC_RXE_DW1                    (IFX_DMA_FCC_CONTROL_BASE + 0x84)

#define IFX_DMA_FCC_RXE_SH_DW0                 (IFX_DMA_FCC_CONTROL_BASE + 0x88)
#define IFX_DMA_FCC_RXE_SH_DW1                 (IFX_DMA_FCC_CONTROL_BASE + 0x8C)

#define IFX_DMA_FCC_TXE_DW0                    (IFX_DMA_FCC_CONTROL_BASE + 0x90)

#define IFX_DMA_FCC_TXE_DW1                    (IFX_DMA_FCC_CONTROL_BASE + 0x94)

#define IFX_DMA_FCC_TXE_SH_DW0                 (IFX_DMA_FCC_CONTROL_BASE + 0x98)
#define IFX_DMA_FCC_TXE_SH_DW1                 (IFX_DMA_FCC_CONTROL_BASE + 0x9C)

#define IFX_DMA_ING_CH_SEL                     (IFX_DMA_FCC_CONTROL_BASE + 0xA0)
#define IFX_DMA_EG_CH_SEL                      (IFX_DMA_FCC_CONTROL_BASE + 0xA4)

/*--- FCC Interrupt Node Enable Register ---*/
#define IFX_DMA_FCC_IRNEN                      (IFX_DMA_FCC_CONTROL_BASE + 0xF4)
/* Ingress End of Packet Interrupt Request Enabled (Bit4) */
#define IFX_DMA_FCC_IRNEN_BIT_I_EOP             (0x1 << 4)
/* Egress End of Packet Interrupt Request Enabled (Bit3) */
#define IFX_DMA_FCC_IRNEN_BIT_E_EOP             (0x1 << 3)
/* Ingress Half End of Packet Interrupt Request Enabled (Bit2) */
#define IFX_DMA_FCC_IRNEN_BIT_PCMI              (0x1 << 2)
/* Egress Half End of Packet Interrupt Request Enabled (Bit1) */
#define IFX_DMA_FCC_IRNEN_BIT_PCME              (0x1 << 1)

/*--- FCC Interrupt Node Control Register ---*/
#define IFX_DMA_FCC_IRNCR                      (IFX_DMA_FCC_CONTROL_BASE + 0xF8)
/* Ingress Interrupt Clear (Bit2) */
#define IFX_DMA_FCC_IRNCR_BIT_PCMI              (0x1 << 2)
/* Egress Interrupt Clear (Bit1) */
#define IFX_DMA_FCC_IRNCR_BIT_PCME              (0x1 << 1)

/*--- FCC Interrupt Node Status Register ---*/
#define IFX_DMA_FCC_IRNSR                      (IFX_DMA_FCC_CONTROL_BASE + 0xFC)
/* Ingress Pending (Bit2) */
#define IFX_DMA_FCC_IRNSR_BIT_PCMI              (0x1 << 2)
/* Egress Pending (Bit1) */
#define IFX_DMA_FCC_IRNSR_BIT_PCME              (0x1 << 1)


#endif/*--- #ifndef __ifxmips_pcmdma_h__ ---*/
