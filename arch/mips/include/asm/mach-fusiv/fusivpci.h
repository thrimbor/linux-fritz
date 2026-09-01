/*
 * Copyright (C) 2006  Ikanos Communications. All rights reserved.
 * The information and source code contained herein is the property
 * of Ikanos Communications. 
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

//======================================================================
// File Name	:	$RCSfile: fusivpci.h,v $
// Description	:	Provide a simple interface to the PCI bus on Fusiv CPU
// CVS ID	:	$Id: fusivpci.h,v 1.2 2009/06/30 08:25:45 kcsk Exp $
//======================================================================

#ifndef _FUSIVPCI_H_
#define _FUSIVPCI_H_

#define  UINT32 unsigned int
// Addresses for the AD6489 PCI hardware. Note that this list may not
// be complete. See the AD6489 PRM for full details
#define PCI_CONTROL      *(volatile UINT32*)0xB9170038   //PCI Bridge Control Register
#define PCI_RST         (1 << 1)                // Reset PCI
#define PCI_RST_EN      (1 << 2)                // Enable driving reset
#define PCI_EN          0x00000001              //Bit#0. Enable PCI core 
#define PCI_EN_ARB      (1 << 4)                // Enable AD6489 arbiter

#define PCI_INT_CTL     *(volatile UINT32 *)0xb900004c   // PCI interrupt enable (host)
#define PCI_INT_A       (1 << 0)                // Enable INTA detection
#define PCI_INT_B       (1 << 1)                // Enable INTB detection
#define PCI_INT_BDG     (1 << 4)                // Enable bridge int detect
#define PCI_INT_SERR    (1 << 5)                // Enable SERR detection int

#define PCI_INT_STAT    *(volatile UINT32 *)0xb9000050   // PCI interrupt status (host)
// Use values from PCI_INT_CTL to detect int present

#define PCI_INT_GEN     *(volatile UINT32 *)0xb9000054   // PCI interrupt gen (device)
#define PCI_INT_SAT     (1 << 0)                // Generate INTA

#define PCI_MBAP        *(volatile UINT32 *)0xb9000058   // Upper 8 bits for PCI mem acc
#define PCI_MBAP_MASK   0xff000000              // Bits to MBAP

#define PCI_ID          *(volatile UINT32*)0xB9170068    //PCI Device Id & Vendor Id
#define PCI_REV         *(volatile UINT32*)0xB917006C    //PCI Class code & Rev Id
#define PCI_SVID        *(volatile UINT32*)0xB9170070    //PCI SubsystemId & Vendow Id
#define PCI_CRP_CTL     *(volatile UINT32 *)0xb9170000   // PCI control for access to
                                                // our config space. Lower
                                                // 10 bits are addr.
#define PCI_TAR_CTL     *(volatile UINT32 *)0xb91700BC

#define PCI_CRP_CMD_SHFT        16
#define PCI_CRP_BE_SHFT         20

// Use this macro to form value to write into PCI_CRP_CTL
#define PCI_CRP_VAL(addr, cmd, bes) \
        ((((UINT32)addr) & 0x000007ff) \
        | ((cmd) << PCI_CRP_CMD_SHFT) | \
        ((bes) << PCI_CRP_BE_SHFT))

#define PCI_CMD_IO_RD   0x2     // I/O read command
#define PCI_CMD_IO_WR   0x3     // I/O write command
#define PCI_CMD_CFG_RD  0xa     // Configuration read command
#define PCI_CMD_CFG_WR  0xb     // Configuration write command

#define PCI_CMD_BE_0    (1 << 0)        // Enable byte 0
#define PCI_CMD_BE_1    (1 << 1)        // Enable byte 1
#define PCI_CMD_BE_2    (1 << 2)        // Enable byte 2
#define PCI_CMD_BE_3    (1 << 3)        // Enable byte 3

#define PCI_CRP_WDAT    	*(volatile UINT32 *)0xb9170004   // Write data after setting CTL
#define PCI_CRP_RDAT    	*(volatile UINT32 *)0xb9170008   // Read data after setting CTL

#define PCI_NPC_CTL     	*(volatile UINT32 *)0xb9170010   // PCI control for access to
#define PCI_ERR_REG		*(volatile UINT32 *)0xb917001c
                                                // I/O and config spaces of
                                                // other devices
#define PCI_NPC_CMD_SHFT        0
#define PCI_NPC_BE_SHFT         4
    
// Use PCI commands and byte enables from PCI_NPC_CTL
// Use this macro to form value to write into PCI_NPC_CTL
#define PCI_NPC_VAL(cmd, bes) \
        (((cmd) << PCI_NPC_CMD_SHFT) | ((bes) << PCI_NPC_BE_SHFT))

#define PCI_NPC_ADDR    *(volatile UINT32 *)0xb917000c   // Address for NPC
#define PCI_NPC_WDAT    *(volatile UINT32 *)0xb9170014   // Write data after setting CTL
#define PCI_NPC_RDAT    *(volatile UINT32 *)0xb9170018   // Read data after setting CTL

#define PCI_ERR_REG     *(volatile UINT32 *)0xb917001c   // PCI access errors
#define PCI_ERR_MASK    0x00000003              // Bits representing PCI err
#define PCI_AHB_MASK    0x00000100              // Bit representing AHB err

#define PCI_ERR_ADDR    *(volatile UINT32 *)0xb9170020   // Address of last PCI error
#define PCI_AHB_ADDR    *(volatile UINT32 *)0xb9170024   // Address of last AHB error

// Definitions related to the hardware environment, mainly board layout.

#define PCI_IS_HOST     // We are expected to configure devices on bus
#define PCI_FIRST_CFG   20      // First address bit used as IDSEL for devices
#define PCI_LAST_CFG    27      // Last address bit used as IDSEL
#define PCI_MEM_BASE    0x1a000000      // Base address for mem config
#define PCI_MEM_END     0x1affffff      // Last address for mem config
#define PCI_PHYMEM_BASE     0x1a000000      // Added by durai
#define PCI_MEM_DIFF	0x1FFFFFFF // This is difference between PCI_PHYMEM_BASE and PCI_MEM_BASE
				  // which is used to convert the virtual address to physical address

// Details about how to set up our configuration space registers.
// Some of these details (i.e. the IDs) are not really important if
// we are setup as a host but some details will be useful if someone
// uses a PCI analyser

#define PCI_OUR_DEV_ID          0x5555
#define PCI_OUR_VENDOR_ID       0x5555
#define PCI_OUR_SS_DEV_ID       0x5555
#define PCI_OUR_SS_VENDOR_ID    0x5555
#define PCI_OUR_CLASS           0x02
#define PCI_OUR_REV             0x01

#define PCI_RST_CONTROL  *(volatile UINT32*)0xB9050700
#define PCI_STAT         *(volatile UINT32*)0xB917003C
#define PCI_MBAR_CONF    *(volatile UINT32*)0xB9170044
#define PCI_IMBAR1_MASK  *(volatile UINT32*)0xB9170048
#define PCI_IMEM1_TRANS  *(volatile UINT32*)0xB917004C
#define PCI_IMBAR2_MASK  *(volatile UINT32*)0xB9170050
#define PCI_IMEM2_TRANS  *(volatile UINT32*)0xB9170054
#define PCI_IMBAR3_MASK  *(volatile UINT32*)0xB9170058
#define PCI_IMEM3_TRANS  *(volatile UINT32*)0xB917005c

#define PCI_MBAP1        *(volatile UINT32*)0xB917007C
#define PCI_MBAP2        *(volatile UINT32*)0xB9170080
#define PCI_MBAP3        *(volatile UINT32*)0xB9170084
#define PCI_MBAP4        *(volatile UINT32*)0xB9170088

#ifdef CONFIG_MACH_FUSIV_MIPS32
#define PCI_MBAP5        *(volatile UINT32*)0xB917008C
#define PCI_MBAP6        *(volatile UINT32*)0xB9170090
#define PCI_MBAP7        *(volatile UINT32*)0xB9170094
#define PCI_MBAP8        *(volatile UINT32*)0xB9170098
#define PCI_MBAP9        *(volatile UINT32*)0xB917009C
#define PCI_MBAP10        *(volatile UINT32*)0xB91700a0
#define PCI_MBAP11        *(volatile UINT32*)0xB91700a4
#define PCI_MBAP12        *(volatile UINT32*)0xB91700a8
#define PCI_MBAP13        *(volatile UINT32*)0xB91700aC
#define PCI_MBAP14        *(volatile UINT32*)0xB91700b0
#define PCI_MBAP15        *(volatile UINT32*)0xB91700b4
#define PCI_MBAP16        *(volatile UINT32*)0xB91700b8
#endif

/* Bits in PCI Control Register */
#define INB_ADDR_TR_EN  0x00000002   /* bit#1 */
#define OUTB_END_EN     0x00000008   /* bit#3 */
#define INTA_EN         0x00000010   /* bit#4 */
#define INT_SERR        0x00000100   /* bit#4 */
#define BDG_IRQ_EN      0x00000200   /* bit#7 */


#endif /* _FUSIVIPCH_H_ */
