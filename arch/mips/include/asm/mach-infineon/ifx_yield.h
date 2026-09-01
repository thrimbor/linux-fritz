/*--------------------------------------------------------------------------------*\
 *   Copyright (C) 2013 AVM GmbH <fritzbox_info@avm.de>
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

#ifndef __ifx_yield_h__
#define __ifx_yield_h__

#include <ifx_regs.h>

#if defined(CONFIG_AR10) 
#define YIELD_SIGNAL_MPS_GI_VPE1_TO_VPE0            0x0
#define YIELD_SIGNAL_MPS_GI_VPE0_TO_VPE1            0x1
#define YIELD_SIGNAL_COP0_COMPARE                   0x2
#define YIELD_SIGNAL_PCM_RI                         0x3
#define YIELD_SIGNAL_PCM_TI                         0x4
#define YIELD_SIGNAL_DMA_8_11                       0x5                           
#define YIELD_SIGNAL_DEU                            0x6
#define YIELD_SIGNAL_EXIN                           0x7
#define YIELD_SIGNAL_SPI_TI                         0x8
#define YIELD_SIGNAL_USB0                           0x9
#define YIELD_SIGNAL_SPI_RI                         0xA
#define YIELD_SIGNAL_PERFORMANE_COUNTER             0xB
#define YIELD_SIGNAL_8KHZ_TI                        0xC
#define YIELD_SIGNAL_TIMER1A                        0xD
#define YIELD_SIGNAL_SI_PCINT                       0xE
#define YIELD_SIGNAL_SI_TIMERINT                    0xF

#elif defined(CONFIG_VR9) 
/*--- note: extra enable in IFX_YIELDEN for special irqline necessary (see below) ---*/
#define YIELD_SIGNAL_MPS                            0x0
#define YIELD_SIGNAL_EBU                            0x1
#define YIELD_SIGNAL_PCI                            0x1
#define YIELD_SIGNAL_DEU                            0x3
#define YIELD_SIGNAL_PCM_RI                         0x4
#define YIELD_SIGNAL_PCM_TI                         0x4
#define YIELD_SIGNAL_PPE                            0x5
#define YIELD_SIGNAL_EIU                            0x7
#define YIELD_SIGNAL_USB1_IR                        0x8
#define YIELD_SIGNAL_USB0_IR                        0x9
#define YIELD_SIGNAL_SPI                            0xA
#define YIELD_SIGNAL_SDIO                           0xB
#define YIELD_SIGNAL_PCIE                           0xB
#define YIELD_SIGNAL_DMA                            0xC                           
#define YIELD_SIGNAL_8KHZ_TI                        0xD
#define YIELD_SIGNAL_TIMER1                         0xD
#define YIELD_SIGNAL_USB0_OCIR                      0xE
#define YIELD_SIGNAL_USIF                           0xF
#define YIELD_SIGNAL_TIR                            0xF

#define LANTIQ_YIELD_TC2                            3
#define LANTIQ_YIELD_MASK2                          ( 0x1 << YIELD_SIGNAL_TIMER1 )
                                                    
                                                   
extern void ifx_yield_en(unsigned int reg, unsigned int bit, unsigned int on);

#define YIELDEN_USB0_IR(on)        ifx_yield_en(1,21,on)
#define YIELDEN_USB1_IR(on)        ifx_yield_en(1,22,on)
#define YIELDEN_USB_OCIR(on)       ifx_yield_en(2,3,on)
#define YIELDEN_PCM_TXIR(on)       ifx_yield_en(0,3,on)
#define YIELDEN_PCM_RXIR(on)       ifx_yield_en(0,4,on)
#define YIELDEN_8KHZ_TI(on)        ifx_yield_en(2,4,on)
#define YIELDEN_GPCT_1A(on)        ifx_yield_en(2,5,on)
#define YIELDEN_GPCT_1B(on)        ifx_yield_en(2,6,on)
#define YIELDEN_GPCT_2A(on)        ifx_yield_en(2,7,on)
#define YIELDEN_GPCT_2B(on)        ifx_yield_en(2,8,on)
#define YIELDEN_GPCT_3A(on)        ifx_yield_en(2,9,on)
#define YIELDEN_GPCT_3B(on)        ifx_yield_en(2,10,on)
#endif

#define LANTIQ_YIELD_TC                             2

#if defined (CONFIG_AVM_SIMPLE_PROFILING_YIELD_PCNT)
#define LANTIQ_YIELD_MASK                           ((0x1 << YIELD_SIGNAL_PCM_RI) | (0x1 << YIELD_SIGNAL_PERFORMANE_COUNTER))
#else
#define LANTIQ_YIELD_MASK                           (0x1 << YIELD_SIGNAL_PCM_RI) 
#endif


#endif/*--- #ifndef __ifx_yield_h__ ---*/

