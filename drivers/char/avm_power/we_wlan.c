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

#define WLAN_ADAPTOR_VERSION WLAN_ADAPTOR_VERSION_1130  /*--- Fixme ---*/

#include <linux/autoconf.h>
#if !defined(CONFIG_MIPS_UR8)
#include <linux/version.h>
#include <asm/addrspace.h>
#include <asm/uaccess.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <asm/mips-boards/ohio.h>
#include <asm/mips-boards/ohio_clk.h>
#include <asm/mach-ohio/hw_uart.h>
#include <asm/mach-ohio/hw_reset.h>
#include <asm/mach_avm.h>

#define PHYSADDR(a) CPHYSADDR(a)

#else/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/
#include <asm/avalanche/avalanche_map.h>
#endif/*--- #else ---*//*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/

#include "wyatt_earp.h"

#define DMA_GLB_CFG                    (0x0740)
#define ECPU_CTRL                      (0x0804)

#define K1BASE                         0xA0000000
/*--- wlan/src/h/pform_types.h to be continued... ---*/
#define PHYS_TO_K1(X) (PHYSADDR(X) | K1BASE)


#define VLYNQ_BOARD_ACX_PORTAL_BASE_ADDR  0x04000000  /* PHYS_ADDR of VLYNQ Portal  */

#if (WLAN_ADAPTOR_VERSION == WLAN_ADAPTOR_VERSION_1130)
/*
 * TNETW1130 definitions
 * ----------------------
 */
#define VLYNQ_ACX111_MEM_OFFSET     0xC0000000  /* Physical address of 1130 memory */
#define VLYNQ_ACX111_MEM_SIZE       0x00040000  /* Total size of the 1130 memory   */
#define VLYNQ_ACX111_SRAM_SIZE      0x0001E000  /* 1130 SRAM size memory 40kB ZWS + 80kB */
#define VLYNQ_ACX111_REG_OFFSET     0xF0000000  /* PHYS_ADDR of 1130 control registers   */
#define VLYNQ_ACX111_REG_SIZE       0x00022000  /* Size of 1130 registers area, MAC+PHY  */
#define ACX111_VL1_REMOTE_BASE      0xD0000000  /* Host Window for 1130 (Master Mode) */

#elif (WLAN_ADAPTOR_VERSION == WLAN_ADAPTOR_VERSION_1150)
/*
 * TNETW1150 definitions
 * ----------------------
 */
#define VLYNQ_ACX111_MEM_OFFSET     0x00000000  /* Physical address of 1150 memory */
#define VLYNQ_ACX111_MEM_SIZE       0x00080000  /* Total size of the 1150 memory   */
//#define VLYNQ_ACX111_SRAM_SIZE      ((60ul+80ul)*1024)  /* 1150 SRAM size memory 60kB ZWS + 80kB */
#define VLYNQ_ACX111_REG_OFFSET     0x00300000  /* PHYS_ADDR of 1150 control registers   */
#define VLYNQ_ACX111_REG_SIZE       0x00100000  /* Size of 1150 registers area, MAC+PHY  */
#define ACX111_VL1_REMOTE_BASE      0x60000000  /* Host Window for 1150 (Master Mode) */

#else
#error Missing definitions for this adaptor.
#endif

#define VLYNQ_BOARD_ACX111_REG_ADDRESS		PHYS_TO_K1(VLYNQ_BOARD_ACX_PORTAL_BASE_ADDR + VLYNQ_ACX111_MEM_SIZE)
//#define VLYNQ_BOARD_ACX111_MEM_ADDRESS		PHYS_TO_K1(VLYNQ_BOARD_ACX_PORTAL_BASE_ADDR)

#define ACX100_REG_ADDRESS		VLYNQ_BOARD_ACX111_REG_ADDRESS

#define	PAL_LE2UINT32 le32_to_cpu
#define ENDIAN_HANDLE_LONG	PAL_LE2UINT32

#define whal_acxMemGetU32(Addr) ( ENDIAN_HANDLE_LONG(*(volatile unsigned int *)(Addr)) ) 
#define whal_acxMemSetU32(Addr, Val) ( *(volatile unsigned int *)(Addr) = ENDIAN_HANDLE_LONG((unsigned int)(Val)) )

#define whal_acxRegRead( ofst)    \
   whal_acxMemGetU32( ACX100_REG_ADDRESS+(unsigned int)(ofst))
#define whal_acxRegWrite( ofst, v)   \
   whal_acxMemSetU32( ACX100_REG_ADDRESS+(unsigned int)(ofst), (v))


#define ACX_REG_ECPU_CONTROL				( ECPU_CTRL )

#if( WLAN_ADAPTOR_VERSION == WLAN_ADAPTOR_VERSION_1130)
#define ECPU_CONTROL_HALT	0x00000001
#elif( WLAN_ADAPTOR_VERSION == WLAN_ADAPTOR_VERSION_1150)
#define ECPU_CONTROL_HALT	0x00000101				
#endif /* WLAN_ADAPTOR_VERSION */

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int SuspendWLAN(int state){
	int dma_glb_cfg;
	/* halt */
	whal_acxRegWrite( ACX_REG_ECPU_CONTROL, ECPU_CONTROL_HALT);
	/* reset DMA */
	dma_glb_cfg = whal_acxRegRead(DMA_GLB_CFG);
	dma_glb_cfg |= 0x4;
	whal_acxRegWrite( DMA_GLB_CFG, dma_glb_cfg);
    DBG_WE_TRC(("SuspendWLAN\n"));
    state = state;
    return 0;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int SuspendVLYNC(int state){
#if defined(CONFIG_MIPS_OHIO)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    unsigned int _reset_handle = OHIO_RESET_START_GPIO + 17;
    avm_put_device_into_reset(_reset_handle);
#else /*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/
    avalanche_reset_ctrl(64, IN_RESET);
#endif /*--- #else ---*/ /*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) ---*/
    state = state;
    DBG_WE_TRC(("SuspendVLYNC\n"));
#endif/*--- #if defined(CONFIG_MIPS_OHIO) ---*/
    return 0;
}
#endif/*--- #if !defined(CONFIG_MIPS_UR8) ---*/
