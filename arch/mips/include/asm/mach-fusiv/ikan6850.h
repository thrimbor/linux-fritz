/*
 * Copyright (C) 2006  Ikanos Communications. All rights reserved.
 * The information and source code contained herein is the property
 * of Ikanos Communications.
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
*/

#ifndef _IKAN6850_H_
#define _IKAN6850_H_
#ifdef CONFIG_FUSIV_VX180
/* Interrupt controller registers */

typedef struct {
	unsigned long l;
	unsigned long h;
} int_stat_t;
typedef struct {
        unsigned long   ipc_id;         /* Interrupt controller ID */
        unsigned long   pad1[0x3f];     /* Reserved */
        unsigned long   cpe_prio[10];   /* CPE Interrupt Priority Registers */
        int_stat_t      cpe_stat[6];   /* CPE Interrupt Status Registers*/
        int_stat_t      emac_stat[3];   /* EMAC AP Interrupt Status Registers */
        int_stat_t      bmu_stat;       /* BMU AP Interrupt Status Registers */
        int_stat_t      wap_stat;      /* WAP AP Interrupt Register */
        int_stat_t      spa_stat;      /* SPA AP Interrupt Register */
        int_stat_t      pci_stat;       /* PCI Interrupt Registers */
        unsigned long   pad3[0x5a];     /* Reserved */
        unsigned long   cpe_per;        /* CPE Peripheral Interrupt */
} ipc_t;
/* Define Interrupt numbers. */
#define DMA_INT         0
#define ARB_INT         1
#define SCU_INT         2
#define MIPS_TIMER_INT       3
#define GPIO2_INT       4
#define DBUSA_ARB_INT        5
#define UART1_INT       6
#define SPI_INT         7
#define GPIO_INT        8
#define GPT1_INT        9
#define GPT2_INT        10
#define GPT3_INT        11
#define DBUSB_INT        12
#define GIGE2_INT       13
#define GIGE1_INT       14
#define VDSP_IDMA2_INT       15
#define VDSP_IDMA1_INT       16
#define DSP0_FL0_INT    17
#define DSP0_FL1_INT    18
#define DSP0_PF0_INT    19
#define DSP0_PF1_INT    20
#define DSP1_FL0_INT    21
#define DSP1_FL1_INT    22
#define DSP1_PF0_INT    23
#define DSP1_PF1_INT    24
#define PCI_INT         25
#define VDSL_PHY_INT         26
#define DBUSC_INT        27
#define SPA_INT       28
#define UART2_INT       29
#define WAP_GIGE_INT        30
#define BMU_GIGE_INT   31
#define AP_DMA_INT      32
#define MIPS_SW0_INT        33
#define MIPS_SW1_INT       34
#define USBH_INT         35
#define EMAC3_INT        36
#define VDSL_AP_INT       36
#define MIPS_PC_INT       37
#define ADI_6843_MAX_INTS       38

#define ADI_6843_UART0_ADDR	KSEG1ADDR(0xb9020000)
#define ADI_6843_UART1_ADDR	KSEG1ADDR(0xb90A0000)

#if defined(CONFIG_FUSIV_AT300) && CONFIG_FUSIV_AT300 
typedef struct {
        unsigned long   ipc_id;         /* Interrupt controller ID */
        unsigned long   pad1[0x3f];     /* Reserved */
        unsigned long   cpe_prio[10];   /* CPE Interrupt Priority Registers */
        int_stat_t      cpe_stat[10];   /* CPE Interrupt Status Registers*/
        int_stat_t      emac_stat[3];   /* EMAC AP Interrupt Status Registers */
        int_stat_t      bmu_stat;       /* BMU AP Interrupt Status Registers */
        int_stat_t      peri_stat;      /* Peripheral AP Interrupt Register */
        int_stat_t      pci_stat;       /* PCI Interrupt Registers */
        unsigned long   pad3[0x54];     /* Reserved */
        unsigned long   cpe_per;        /* CPE Peripheral Interrupt */
} ipc_t;
/* Define Interrupt numbers. */
#define DMA_INT         0
#define ARB_INT         1
#define SCU_INT         2
#define USB_INT         3
#define WIU2_INT	4
#define WIU1_INT	5
#define UART1_INT       6
#define SPI_INT         7
#define GPIO_INT        8
#define GPT1_INT        9
#define GPT2_INT        10
#define GPT3_INT        11
#define ARB2_INT        12
#define EMAC2_INT       13
#define EMAC1_INT       14
#define IDMA2_INT       15
#define IDMA1_INT       16
#define DSP0_FL0_INT    17
#define DSP0_FL1_INT    18
#define DSP0_PF0_INT    19
#define DSP0_PF1_INT    20
#define DSP1_FL0_INT    21
#define DSP1_FL1_INT    22
#define DSP1_PF0_INT    23
#define DSP1_PF1_INT    24
#define PCI_INT         25
#define EMAC3_INT       26
#define ARB3_INT        27
#define SPORT2_RX_INT   28
#define UART2_INT       29
#define SPA_INT		30
#define BMU_INT         31
#define AP_DMA_INT      32
#define SPORT2_TX_INT   34

#define ADI_6843_MAX_INTS       36
#endif // AT300
#endif
#endif /* _IKAN_6850_H_ */
