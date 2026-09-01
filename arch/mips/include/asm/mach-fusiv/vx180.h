/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifndef _asm_mips_mach_ikan_mips_vx180_h_
#define _asm_mips_mach_ikan_mips_vx180_h_

#include <asm/addrspace.h>

/*------------------------------------------------------------------------------------------*\
 * System Memory Map
\*------------------------------------------------------------------------------------------*/
#define VX180_SYSTEM_RAM_START              0x00000000
#define VX180_PCI_MEM_START                 0x1A000000
#define VX180_ASYNC_BANK3_START             0x1C000000
#define VX180_ASYNC_BANK2_START             0x1D000000
#define VX180_ASYNC_BANK1_START             0x1E000000
#define VX180_FLASH_START                   0x1F000000

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
#define VX180_UART1_BASE    KSEG1ADDR(0x19020000)
#define VX180_UART2_BASE    KSEG1ADDR(0x190A0000)

/*------------------------------------------------------------------------------------------*\
 * System Configuration
\*------------------------------------------------------------------------------------------*/
#define VX180_SYSTEM_CONFIG_BASE             (0x19000000)

#define VX180_GENERAL_STATUS_BASE            KSEG1ADDR(0x1C + VX180_SYSTEM_CONFIG_BASE)
union __vx180_general_status_register {
    struct _vx180_general_status_register {
        volatile unsigned int Reserved1 : 4;      /*--- 31:28             R Reserved ---*/
        volatile unsigned int VOX_REV_ID : 4;     /*--- 27:24             R           Fusiv Vx180 Revision ID. ---*/
        volatile unsigned int Reserved0 : 7;      /*--- 23:17             R           Reserved ---*/
        volatile unsigned int SI_IPL : 6;         /*--- 16:11 R MIPS ---*/
        volatile unsigned int SI_IACK : 1;        /*--- 10    R MIPS ---*/
        volatile unsigned int SI_DBS: 2;          /*--- 9:8   R MIPS ---*/
        volatile unsigned int SI_IBS : 4;         /*--- 7:4   R MIPS ---*/
        volatile unsigned int BYPASS_PLL_BUF : 1; /*--- 3     R PLL Buffer is bypassed. ---*/
        volatile unsigned int SPARE1 : 1;         /*--- 2     R Spare 1 ---*/
        volatile unsigned int SPARE2 : 1;         /*--- 1     R Spare 2 ---*/
        volatile unsigned int SPARE3 : 1;         /*--- 0     R Spare 3 ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_USB2_DELAY_BASE                KSEG1ADDR(0x20 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_usb2_delay_register {
    struct _vx180_usb2_delay_register {
        /*--- 31:0 R/W The delay required between USB 2.0 Host Controlle ---*/
         /*--- “txdata” and “txsezero” singals to USB-PHYs ---*/
         /*--- (UTMI) ---*/
         /*--- NOTE: Value must be 2n -1, ---*/
         /*--- Where 0 <= ‘n’ <= 32 ---*/
        volatile unsigned int USB2_DLY_SEL;
    } Bits;
    volatile unsigned int Reg;
};


#define VX180_PCI_CTRL_STAT_BASE             KSEG1ADDR(0x40 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_pci_ctrl_stat_register {
    struct _vx180_pci_ctrl_stat_register {
        volatile unsigned int Reserved : 12;               /*--- 31:20        R             Reserved ---*/
        volatile unsigned int PCI_CSTS_CHG : 1;            /*--- 19           RO            PCI CardBus status change indication signal from PCI CardBus.  (Not used in PCI Mode). ---*/
        volatile unsigned int PCI_CLK_CHG_REQ : 1;         /*--- 18           RO            PCI clock change request.  Indicates to the application that the system clock is about to be changed, that is, slowed down or stopped. ---*/
        volatile unsigned int PCI_CLK_CHG : 1;             /*--- 17           RO            PCI clock change detected.  Indicates that the system clock has bee slowed down or stopped. ---*/
        volatile unsigned int PCI_CINT : 1;                /*--- 16           RO            PCI CardBus interrupt.  From PCI CardBus System.  (Applicable only in CardBus mode). ---*/
        volatile unsigned int Reserved0 : 12;              /*--- 15:4         R             Reserved ---*/
        volatile unsigned int PCI_MODE_SELECT : 2;         /*--- 3:2          R/W              ‘00’ : PCI Mode ‘01’ : CardBus Mode ‘1x’ : MiniPCI Mode ---*/
        volatile unsigned int PCI_APP_REQ_NORM_CLK : 1;    /*--- 1            R/W           PCI Application request normal clock. ---*/
        volatile unsigned int PCI_CARDBUS_CLKRUN_SEL : 1;  /*--- 0            R/W              ‘0’ : Selects internal clockrun.  ‘1’ : Selects external clockrun. ---*/
    } Bits;
    volatile unsigned int Reg;
};


#define VX180_SPORT_MUX_CTRL_BASE            (volatile unsigned int *)KSEG1ADDR(0x80 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_sport_mux_ctrl_register {
    struct _vx180_sport_mux_ctrl_register {
        volatile unsigned int Reserved2 : 23;               /*--- 31:9               R          Reserved ---*/
        volatile unsigned int SPORT1_LOOPBACK : 1;          /*--- 8                  R/W        Enables SPORT1 loopback. ---*/
        volatile unsigned int Reserved1 : 3;                /*--- 7:5                R          Reserved ---*/
        volatile unsigned int DSP2_SPORT1_EN : 1;           /*--- 4                  R/W        ‘1’ : Enables DSP2 SPORT1.  ‘0’ : Disables DSP2 SPORT1. ---*/
        volatile unsigned int INV_TFS0 : 1;                 /*--- 3                  R/W        Invert SPORT0 TX frame sync. ---*/
        volatile unsigned int Reserved0: 2;                 /*--- 2:1                R          Reserved ---*/
        volatile unsigned int DSP2_SPORT_EN : 1;            /*--- 0                  R/W        ‘1’ : Enables DSP2 SPORT .  ‘0’ : Disables DSP2 SPORT. ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_SYSTEM_CONTROL_BASE            (volatile unsigned int *)KSEG1ADDR(0x88 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_system_control_register {
    struct _vx180_system_control_register {
        volatile unsigned int USB2_CSR_ENDIAN_CONV : 1;              /*--- 31        R/W         ‘1’ : Enables USB2 CSR Endian conversion, else disables. ---*/
        volatile unsigned int USB2_DATA_ENDIAN_CONV : 1;             /*--- 30        R/W         ‘1’ : Enables USB2 DATA Endian conversion, else disables. ---*/
        volatile unsigned int USB2_DESC_ENDIAN_CONV : 1;             /*--- 29        R/W         ‘1’ : Enables USB2 Descriptor Endian conversion, else disabled. ---*/
        volatile unsigned int SPI_CS4_EN : 1;                        /*--- 28        R/W         ‘1’ : SPI Chip Select 4 is driven from GPIO [12]. ---*/
        volatile unsigned int SPI_CS3_EN : 1;                        /*--- 27        R/W         ‘1’ : SPI Chip Select 3 is driven from GPIO [11]. ---*/
        volatile unsigned int SPI_CS2_EN : 1;                        /*--- 26        R/W         ‘1’ : SPI Chip Select 2 is driven from GPIO [2]. ---*/
        volatile unsigned int SPI_CS1_EN : 1;                        /*--- 25        R/W         ‘1’ : SPI Chip Select 1 is driven from GPIO [1].  NOTE: During SPI booting SPI Chip Select 1 is driven through GPIO [1]– even if this bit is not set. ---*/
        volatile unsigned int SPI_CS0_EN : 1;                        /*--- 24        R/W         ‘1’ : SPI Chip Select 0 is driven from GPIO [1]. ---*/
        volatile unsigned int Reserved4 : 6;                         /*--- 23:18     R           Reserved ---*/
        volatile unsigned int RELEASE_CPE_RESET_AFTER_SPI_BOOT : 1;  /*--- 17        R/W            ‘1’ : After SPI Boot, CPE reset is released.  ‘0’ : Even after SPI Boot is complete, CPE remains in reset state.  NOTE: Valid only if SPI BOOT is enabled. ---*/
        volatile unsigned int Reserved3 : 2;                         /*--- 16:15     R           Reserved ---*/
        volatile unsigned int DISABLE_SPI : 1;                       /*--- 14        R/W            ‘1’ : Disables SPI Module.  ‘0’ : Enables SPI Module. ---*/
        volatile unsigned int Reserved2 : 6;                         /*--- 13:8      R           Reserved ---*/
        volatile unsigned int DSP1_PF3_ENABLE : 1;                   /*--- 7         R/W         ‘1’ : DSP1 PF3 pin connected externally through the GPIO [10] pin. ---*/
        volatile unsigned int DSP1_PF2_ENABLE : 1;                   /*--- 6         R/W         ‘1’ : DSP1 PF2 pin connected externally through the GPIO [9] pin. ---*/
        volatile unsigned int DSP0_PF3_ENABLE : 1;                   /*--- 5         R/W         ‘1’ : DSP0 PF3 pin connected externally through the GPIO [8] pin. ---*/
        volatile unsigned int DSP0_PF2_ENABLE : 1;                   /*--- 4         R/W         ‘1’ : DSP0 PF2 pin connected externally through the GPIO [7] pin. ---*/
        volatile unsigned int Reserved1 : 2;                         /*--- 3:2       R D           Reserved ---*/
        volatile unsigned int ON_DEMAND_LOCK : 1;                    /*--- 1         R/W            ‘0’ : Disables ON Demand Lock.  ‘1’ : Enables ON Demand Lock.  Required as part of boot code; settings updated only after Internal configuration reset. ---*/
        volatile unsigned int Reserved0 : 1;                         /*--- 0         R/W         Reserved ---*/
    } Bits;
    volatile unsigned int Reg;
};


#define VX180_MODE_CONTROL_BASE              (volatile unsigned int *)KSEG1ADDR(0xB0 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_mode_control_register {
    struct _vx180_mode_control_register {
        volatile unsigned int Reserved : 12;               /*--- 31:20         R            Reserved ---*/
        volatile unsigned int USB2_CLKCKTRST_N : 1;        /*--- 19            R/W          Initial reset signal for USB Host DPLL.  Active LOW ---*/
        volatile unsigned int USB2_SIM_MODE : 1;           /*--- 18            R/W            ‘1’ : USB Host count full 1 ms and USB DPLL block Reset.  ‘0’ : USB Host counter use simulation time. ---*/
        volatile unsigned int Reserved2 : 4;               /*--- 17:14         R            Reserved ---*/
        volatile unsigned int INT_BOOTROM_SEL : 1;         /*--- 13            R/W            ‘1’ : Enables access to the internal boot-ROM used for SPI boot, as well as simultaneously disables access to external flash.  ‘0’ : Disables access to internal boot ROM used for SPI boot, default access to external flash. ---*/
        volatile unsigned int PCI_66MHZ_MODE : 1;          /*--- 12            R/W            ‘1’ : PCI operates at 66 MHz.  ‘0’ : PCI operates at 33 MHz. ---*/
        volatile unsigned int PCI_INT_ARB_ENB : 1;         /*--- 11            R/W            ‘1’ : Enables PCI internal arbiter.  ‘0’ : Disables PCI internal arbiter. ---*/
        volatile unsigned int PCI_HOST_DEV_SWITCH_ENB : 1; /*--- 10                   R/W            ‘1’ : PCI is configured as a Host (Central Resource).  A      ‘0’ : PCI is configured as a PCI device. ---*/
        volatile unsigned int Reserved1 : 4;               /*--- 9:6                        Reserved ---*/
        volatile unsigned int UART2_EN : 1;                /*--- 5             R/W          ‘1’ : UART2 transmit and receive pins are connected through GPIO [3] and GPIO [4] respectively. ---*/
        volatile unsigned int NTR_CLK_ENABLE : 1;          /*--- 4             R/W          ‘1’ : NTR CLK pin connected through GPIO [7]. ---*/
        volatile unsigned int DSP1_EMU_EN : 1;             /*--- 3 R/W ‘1’ : DSP1 emulation pins connected externally through PCI_AD [17:9] pins. ---*/
        volatile unsigned int DSP0_EMU_EN : 1;             /*--- 2 R/W ‘1’ : DSP0 emulation pins connected externally through the PCI_AD [8:0] pins. ---*/
        volatile unsigned int Reserved0 : 2;               /*--- 1:0    R ---*/
    } Bits;
    volatile unsigned int Reg;
};
/*--------------------------------------------------------------------------------*\
 * PLL Register
\*--------------------------------------------------------------------------------*/
#define VX180_PROC_PLL_CONTROL              KSEG1ADDR(0xB4 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_proc_pll_control {
    struct _vx180_proc_pll_control {
        volatile unsigned int Reserved0 : 18;               /*--- 31:14 ---*/
        volatile unsigned int PROC_PLL_MVAL: 6;             /*--- 13:8  R/W     PROC PLL multiplication value  ---*/
        volatile unsigned int Reserved1 : 3;                /*--- 7:5 ---*/
        volatile unsigned int PROC_PLL_PD :1;               /*--- 4:4   R/W     PROC PLL power down ---*/
        volatile unsigned int PROC_PLL_RST :1;              /*--- 3:3   R/W     PROC PLL reset ---*/
        volatile unsigned int PROC_PLL_CNTRST :1;           /*--- 2:2   R/W     PROC PLL counter reset ---*/
        volatile unsigned int Reserved2 :2;                 /*--- 1:0 ---*/
    } Bits;
    volatile unsigned int Reg;
};
#define VX180_INTERN_CONF_RESET_CONTROL              KSEG1ADDR(0xB8 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_intern_conf_reset_control {
    struct _vx180_intern_conf_reset_control {
        volatile unsigned int Reserved0 : 15;              /*--- 31:17 ---*/
        volatile unsigned int CONFIG_RESET: 1;             /*--- 16:16  R/W     ?1? : Resets PLL, in addition to the whole
                                                                    Fusiv Vx180 System.
                                                                    The reset is active low and remains for 8191 input
                                                                    clock (25 MHz) cycles.---*/
        volatile unsigned int Reserved1 : 16;              /*--- 15:0 ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DSP_PLL_CONTROL              (volatile unsigned int *)KSEG1ADDR(0xBC + VX180_SYSTEM_CONFIG_BASE)
union __vx180_dsp_pll_control {
    struct _vx180_dsp_pll_control {
        volatile unsigned int Reserved0 : 18;              /*--- 31:14 ---*/
        volatile unsigned int DSP_PLL_MVAL: 6;             /*--- 13:8   R/W     DSP PLL multiplication value  ---*/
        volatile unsigned int Reserved1 : 3;               /*--- 7:5 ---*/
        volatile unsigned int DSP_PLL_PD :1;               /*--- 4:4    R/W     DSP PLL power down ---*/
        volatile unsigned int DSP_PLL_RST :1;              /*--- 3:3    R/W     DSP PLL reset ---*/
        volatile unsigned int DSP_PLL_CNTRST :1;           /*--- 2:2    R/W     DSP PLL counter reset ---*/
        volatile unsigned int Reserved2 :2;                /*--- 1:0 ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_SYSTEM_PLL_CONTROL              KSEG1ADDR(0xAC + VX180_SYSTEM_CONFIG_BASE)
union __vx180_system_pll_control {
    struct _vx180_system_pll_control {
        volatile unsigned int Reserved0 : 18;               /*--- 31:14 ---*/
        volatile unsigned int SYSTEM_PLL_MVAL: 6;           /*--- 13:8  R/W     SYSTEM PLL multiplication value  ---*/
        volatile unsigned int Reserved1 : 3;                /*--- 7:5 ---*/
        volatile unsigned int SYSTEM_PLL_PD :1;             /*--- 4:4   R/W     SYSTEM PLL power down ---*/
        volatile unsigned int SYSTEM_PLL_RST :1;            /*--- 3:3   R/W     SYSTEM PLL reset ---*/
        volatile unsigned int SYSTEM_PLL_CNTRST :1;         /*--- 2:2   R/W     SYSTEM PLL counter reset ---*/
        volatile unsigned int Reserved2 :2;                 /*--- 1:0 ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_USB_PCI_PLL_CONTROL              KSEG1ADDR(0xC8 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_usb_pci_pll_control {
    struct _vx180_usb_pci_pll_control {
        volatile unsigned int Reserved0 : 11;               /*--- 31:21 ---*/
        volatile unsigned int PCI_PLL_PD :1;                /*--- 20:20     R/W     PCI PLL power down ---*/
        volatile unsigned int PCI_PLL_RST :1;               /*--- 19:19     R/W     PCI PLL reset ---*/
        volatile unsigned int PCI_PLL_CNTRST :1;            /*--- 18:18     R/W     PCI PLL counter reset ---*/
        volatile unsigned int Reserved1 : 11;               /*--- 17:7 ---*/
        volatile unsigned int USB_CLKOUT_SEL : 2;           /*--- 6:5       R/W     USB PLL output clock select.
                                                                    ? 2?b00 : 12 MHz clock
                                                                    ? 2?b01 : 96 MHz clock
                                                                    ? 2?b10 : 480 MHz clock
                                                                    ? 2?b11 : Disable ---*/
        volatile unsigned int USB_PLL_PD :1;                /*--- 4:4       R/W     USB PLL power down ---*/
        volatile unsigned int USB_PLL_RST :1;               /*--- 3:3       R/W     USB PLL reset ---*/
        volatile unsigned int USB_PLL_CNTRST : 1;           /*--- 2:2       R/W     USB PLL counter reset ---*/
        volatile unsigned int USB_12MHZ_SEL : 2;            /*--- 1:0       R/W     USB PLL 12 MHz clock select.  ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_CLK_GATE_CONTROL               (volatile unsigned int *)KSEG1ADDR(0xCC + VX180_SYSTEM_CONFIG_BASE)
union __vx180_clk_gate_control {
    struct _vx180_clk_gate_control {
        volatile unsigned int Reserved : 16;        /*--- 31:16 ---*/
        volatile unsigned int SAP_SYS_CLK_EN:1;     /*--- 15 R/W ?? 1: Enables SAP System clock . ---*/
                                                    /*---        ?? 0: Disables SAP System clock. ---*/
        volatile unsigned int SAP_APU_CLK_EN:1;     /*--- 14 R/W ?? 1: Enables SAP APU clock. ---*/
                                                    /*---        ?? 0: Disables SAP APU clock. ---*/
        volatile unsigned int USBH_CLK_EN:1;        /*--- 13 R/W ?? 1: Enables USB Host System clock. ---*/
                                                    /*---        ?? 0: Disabled USB Host System clock. ---*/
        volatile unsigned int WAP_SYS_CLK_EN:1;     /*--- 12 R/W ?? 1: Enables WAP System clock. ---*/
                                                    /*---        ?? 0: Disables WAP System clock. ---*/
        volatile unsigned int WAP_APU_CLK_EN:1;     /*--- 11 R/W ?? 1: Enables WAP APU clock. ---*/
                                                    /*---        ?? 0: Disables WAP APU clock. ---*/
        volatile unsigned int BMU_SYS_CLK_EN:1;     /*--- 10 R/W ?? 1: Enables BMU System clock. ---*/
                                                    /*---        ?? 0: Disables BMU System clock. ---*/
        volatile unsigned int BMU_APU_CLK_EN:1;     /*--- 9 R/W  ?? 1: Enables BMU APU clock. ---*/
                                                    /*---        ?? 0: disables BMU APU clock ---*/
        volatile unsigned int VAP_APU_CLK_EN:1;     /*--- 8 R/W  ?? 1: Enables VAP APU clock. ---*/
                                                    /*---        ?? 0: Disables VAP APU clock. ---*/
        volatile unsigned int VAP_SYS_CLK_EN:1;     /*--- 7 R/W  ?? 1: Enables VAP System clock. ---*/
                                                    /*---        ?? 0: Disables VAP System clock. ---*/
        volatile unsigned int GIGE2_APU_CLK_EN:1;   /*--- 6 R/W  ?? 1: Enables GIGE2 APU clock. ---*/
                                                    /*---        ?? 0: Disables GIGE2 APU clock. ---*/
        volatile unsigned int GIGE2_CLK_EN:1;       /*--- 5 R/W  ?? 1: Enables GIGE2 System clock. ---*/
                                                    /*---        ?? 0: Disabled GIGE2 System clock. ---*/
        volatile unsigned int GIGE1_APU_CLK_EN:1;   /*--- 4 R/W  ?? 1: Enables GIGE1 APU clock. ---*/
                                                    /*---        ?? 0: Disables GIGE1 APU clock. ---*/
        volatile unsigned int GIGE1_CLK_EN:1;       /*--- 3 R/W  ?? 1: Enables EMAC1 System clock. ---*/
                                                    /*---        ?? 0: Disables EMAC1 System clock. ---*/
        volatile unsigned int EBI_CLK_EN:1;         /*--- 2 R/W  ?? 1: Enables AMC System clock. ---*/
                                                    /*---        ?? 0: Disables AMC System clock. ---*/
        volatile unsigned int VPHY_CLK_EN:1;        /*--- 1 R/W  ?? 1: Enables VPHY System clock. ---*/
                                                    /*---        ?? 0: Disabled VPHY System clock. ---*/
        volatile unsigned int PCI_CLK_EN:1;         /*--- 0 R/W  ?? 1: Enables PCI System clock. ---*/
                                                    /*---        ?? 0: Disables PCI System clock. ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_MODE_STATUS_BASE               KSEG1ADDR(0xD0 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_mode_status_register {
    struct _vx180_mode_status_register {
        volatile unsigned int INT_BOOT_ROM_SEL : 1;       /*--- 31       R          ’1’ : Selects Internal boot-ROM. ---*/
        volatile unsigned int Reserved6 : 1;              /*--- 30       R          Reserved ---*/
        volatile unsigned int UART2_SEL : 1;              /*--- 29       R          ‘1’ : Selects UART2. ---*/
        volatile unsigned int NTR_CLK_ENB : 1;            /*--- 28       R          ‘1’ : Enables NTR clock. ---*/
        volatile unsigned int Reserved5 : 1;              /*--- 27       R          Reserved ---*/
        volatile unsigned int CPE_MODE : 1;               /*--- 26       R          CPE5 stand-alone mode. ---*/
        volatile unsigned int Reserved4 : 1;              /*--- 25       R          Returns ‘1’ on read. ---*/
        volatile unsigned int PLL_DEBUG_MODE : 1;         /*--- 24       R          ‘1’ : Enables PLL debug mode. ---*/
        volatile unsigned int SPI_BOOT_EN : 1;            /*--- 23       R          ‘1’ : Enables SPI boot mode. ---*/
        volatile unsigned int Reserved3 : 1;              /*--- 22       R          Returns ‘1’ on read. ---*/
        volatile unsigned int CONFIG_5 : 1;               /*--- 21       R          UTMI product test mode. ---*/
        volatile unsigned int CONFIG_4 : 1;               /*--- 20       R          Functional debug mode. ---*/
        volatile unsigned int CONFIG_3 : 1;               /*--- 19       R          MII2 configuration for MII/RMII/GMII. ---*/
        volatile unsigned int CONFIG_2 : 1;               /*--- 18       R          MII2 configuration for MII/RMII/GMII. ---*/
        volatile unsigned int CONFIG_1 : 1;               /*--- 17       R          MII1 configuration for MII/RMII/GMII. ---*/
        volatile unsigned int CONFIG_0 : 1;               /*--- 16       R          MII1 configuration for MII/RMII/GMII. ---*/
        volatile unsigned int Reserved2 : 2;              /*--- 15:14    R          Reserved ---*/
        volatile unsigned int NEW_BOOT_MODE : 1;          /*--- 13       R              ‘1’ : Enables New boot mode.  ‘0’ : Normal mode. ---*/
        volatile unsigned int UTMI_PHY2_EN : 1;           /*--- 12 A R              ‘1’ : Selects PHY-2 of USB PHY.  ‘0’ : Selects PHY-1 of USB PHY.  NOTE: Applicable only if “UTMI_PROD_TESTMODE” is active. ---*/
        volatile unsigned int UTMI_PROD_TESTMODE : 1;     /*--- 11       R              ‘1’ : USB PHY in Product test mode.  ‘0’ : USB PHY in Functional mode. ---*/
        volatile unsigned int Reserved : 1;               /*--- 10       R to  Return ‘0’ on read ---*/
        volatile unsigned int TESTMODE : 1;               /*--- 9        R              ‘1’ : Enables Test mode.  ‘0’ : Functional mode ---*/
        volatile unsigned int Reserved1 : 6;              /*--- 8:3      R Return ‘0’ on read ---*/
        volatile unsigned int PCI_HOST_DEVICE_SWITCH : 1; /*--- 2 R    ‘1’ : PCI in Host mode.  ‘0’ : PCI in Device mode. ---*/
        volatile unsigned int PCI_INT_ARB_ENB : 1;        /*--- 1 R ‘1’ : Enables PCI internal arbiter. ---*/
        volatile unsigned int Reserved0 : 1;              /*--- 0 R Returns ‘0’ on read ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_PAD_PIO_MISC_CTRL_BASE         (volatile unsigned int *)KSEG1ADDR(0xD4 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_pad_pio_misc_ctrl_register {
    struct _vx180_pad_pio_misc_ctrl_register {
        volatile unsigned int Reserved1 : 24;                        /*--- 31:8       R            Reserved ---*/
        volatile unsigned int VDSL_PHY_GPIO3_EN : 1;                 /*--- 7          R/W          Enables GPIO 19, which is shared between MIPS processor and xDSL PHY processor. This Enable Register allows xDSL PHY to control the GPIO. ---*/
        volatile unsigned int VDSL_PHY_GPIO2_EN : 1;                 /*--- 6          R/W          Enables GPIO 18, which is shared between MIPS processor and xDSL PHY processor. This Enable Register allows xDSL PHY to control the GPIO. ---*/
        volatile unsigned int VDSL_PHY_GPIO1_EN : 1;                 /*--- 5          R/W          Enables GPIO 17, which is shared between MIPS processor and xDSL PHY processor. This Enable Register allows xDSL PHY to control the GPIO. ---*/
        volatile unsigned int VDSL_PHY_GPIO0_EN : 1;                 /*--- 4          R/W          Enables GPIO 16, which is shared between MIPS processor and xDSL PHY processor. This Enable Register allows xDSL PHY to control the GPIO. ---*/
        volatile unsigned int Reserved0 : 2;                         /*--- 3:2        R   A        Reserved ---*/
        volatile unsigned int VAP_ADDR_MAP_EN : 1;                   /*--- 1          R/W          Enables VAP address map. ---*/
        volatile unsigned int PAD_PIO_RELCPU : 1;                    /*--- 0          R/W          Release CPU of VPHY to‘0’ : Asserts request ready.  ‘1’ : Negates request ready. ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_USB_STAT_BASE                  KSEG1ADDR(0xD8 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_usb_stat_register {
    struct _vx180_usb_stat_register {
        volatile unsigned int Reserved : 1;             /*--- 31                R           Reserved ---*/
        volatile unsigned int LOCKPLL : 1;              /*--- 30                R           UTMI PLL Lock ---*/
        volatile unsigned int PORT2_OK : 1;             /*--- 29                R           UTMI Port - 2 ‘1’ : PHY is ready to operate.  ‘0’ : PHY is not ready or in suspend mode. ---*/
        volatile unsigned int PORT1_OK : 1;             /*--- 28                R           UTMI Port -1 ‘1’ : PHY is ready to operate.  un                                                        ‘0’ : PHY is not ready or in suspend mode. ---*/
        volatile unsigned int EHCI_LPSMC_STATE : 4;     /*--- 27:24             R           Indicates the state of the LPSMC.  Used only for debugging. ---*/
        volatile unsigned int EHCI_XFER_CNT : 11;       /*--- 23:13             R           Transfer byte count from EHCI Master. ---*/
        volatile unsigned int EHCI_XFER_PRDC : 1;       /*--- 12                R           Current transfer is periodic. ---*/
        volatile unsigned int OHCI_0_GLOBALSUSPEND : 1; /*--- 11                R           OHCI-0 is in Global suspend state. ---*/
        volatile unsigned int OHCI_0_RWE : 1;           /*--- 10                R           Enables Remote wake-up. ---*/
        volatile unsigned int OHCI_0_RMTWKP : 1;        /*--- 9                 R           Enables Remote wake-up status. ---*/
        volatile unsigned int OHCI_0_DRWE : 1;          /*--- 8                 R           Enables Device remote wake-up. ---*/
        volatile unsigned int OHCI_0_CCS : 2;           /*--- 7:6               R OHCI-0 current connect status of each port. ---*/
        volatile unsigned int OHCI_0_SOF : 1;           /*--- 5                 R           OHCI-0 start of frame. ---*/
        volatile unsigned int OHCI_0_BUFACC : 1;        /*--- 4                 R OHCI-0 Buffer access. ---*/
        volatile unsigned int OHCI_0_SMI : 1;           /*--- 3 R OHCI System Management interrupt. ---*/
        volatile unsigned int OHCI_0_IRQ : 1;           /*--- 2 R OHCI-0 Bus General interrupt. ---*/
        volatile unsigned int EHCI_INTERRUPT : 1;       /*--- 1 R USB Host Controller interrupt. ---*/
        volatile unsigned int EHCI_BUFACC : 1;          /*--- 0 R EHCI Buffer Access ‘1’ : Data read/write ‘0’ : Descriptor read/write NOTE: Supports big endian operation. ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_USB2_MISC_CTRL_BASE            KSEG1ADDR(0xDC + VX180_SYSTEM_CONFIG_BASE)
union __vx180_usb2_misc_ctrl_register {
    struct _vx180_usb2_misc_ctrl_register {
        volatile unsigned int Reserved : 26;               /*--- 31:6         R           Reserved ---*/
        volatile unsigned int USB2_PHY2_RST : 1;           /*--- 5            R/W ‘1’ : Asserts USB2 UTMI PHY2 reset.  ‘0’ : De-asserts USB2 UTMI PHY2 reset. ---*/
        volatile unsigned int USB2_PHY1_RST : 1;           /*--- 4 R/W ‘1’ : Asserts USB2 UTMI PHY1 reset.  ‘0’ : De-asserts USB2 UTMI PHY1 reset. ---*/
        volatile unsigned int USB2_PHY_CBRST : 1;          /*--- 3 R/W ‘1’ : Asserts USB2 UTMI PHY common blocks reset.  ‘0’ : De-asserts USB2 UTMI PHY common blocks reset. ---*/
        volatile unsigned int USB2_UTMI_2_SUSPEND_EN : 1;  /*--- 2 R/W ‘1’ : Enables USB2 UTMI-2 suspend.  ‘0’ : Disables USB2 UTMI-2 suspend. ---*/
        volatile unsigned int USB2_UTMI_1_SUSPEND_EN : 1;  /*--- 1 R/W ‘1’ : Enables USB2 UTMI-1 suspend.  ‘0’ : Disables USB2 UTMI-1 suspend. ---*/
        volatile unsigned int USB_PHY_SYSCLK_EN : 1;       /*--- 0 R/W ‘1’ : Enables UTMI PHY System clock.  ‘0’ : Disables UTMI PHY System clock. ---*/
    } Bits;
    volatile unsigned int Reg;
};


#define VX180_GENERAL_MISC_CTRL_BASE         KSEG1ADDR(0xE0 + VX180_SYSTEM_CONFIG_BASE)
union __vx180_general_misc_ctrl_register {
    struct _vx180_general_misc_ctrl_register {
        volatile unsigned int Reserved : 16;                /*--- 31:16              R          Reserved ---*/
        volatile unsigned int GSCANRAMWR : 1;               /*--- 15                 R/W        ATPG Coverage for MIPS ---*/
        volatile unsigned int Reserved0 : 13;               /*--- 14:2               R          Reserved ---*/
        volatile unsigned int CLK_OUT_SEL : 2;              /*--- 1:0                R/W          ‘00’ : LOW ‘01’ : MIPS Clock/4 ‘10’ : System Clock ‘11’ : USB Clock ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_PSB_CTRL_BASE                  KSEG1ADDR(0xEC + VX180_SYSTEM_CONFIG_BASE)
union __vx180_psb_ctrl_register {
    struct _vx180_psb_ctrl_register {
        volatile unsigned short PSB_MAP;    /*--- 31:16 R/W VDSP PIO bridge memory address map. ---*/
        volatile unsigned short PSB_MASK;   /*--- 15:0  R/W VDSP PIO bridge memory address mask. ---*/
    } Bits;
    volatile unsigned int Reg;
};
/*--------------------------------------------------------------------------------*\
 * Timer 
\*--------------------------------------------------------------------------------*/
#define VX180_TIM_CPE_GLB_STS_BASE(a)             KSEG1ADDR(0x19070000 + ((a) * 0x20))
union __vx180_tim_cpe_glb_sts_register {
    struct _vx180_tim_cpe_glb_sts_register {
        volatile unsigned short Reserved0:2;    /*--- 15:14 R/W Reserved ---*/
        volatile unsigned short TIMDIS2:1;      /*--- 13 R/W Timer 2 : ?Write one to Disable? ---*/
        volatile unsigned short TIMEN2:1;       /*--- 12 R/W Timer 2 : ?Write one to Enable? ---*/
        volatile unsigned short TIMDIS1:1;      /*--- 11 R/W Timer 1 : ?Write one to Disable? ---*/
        volatile unsigned short TIMEN1:1;       /*--- 10 R/W Timer 1 : ?Write one to Enable? ---*/
        volatile unsigned short TIMDIS0:1;      /*--- 9 R/W Timer 0 : ?Write one to Disable? ---*/
        volatile unsigned short TIMEN0 :1;      /*--- 8 R/W Timer 0 : ?Write one to Enable? ---*/
        volatile unsigned short Reserved1:1;    /*--- 7 R/W Reserved ---*/
        volatile unsigned short OVF_ERR2:1;     /*--- 6 R/W Same as for Timer2 ---*/
        volatile unsigned short OVF_ERR1:1;     /*--- 5 R/W Same as for Timer1 ---*/
        volatile unsigned short OVF_ERR0:1;     /*--- 4 R/W Set if initialized with the following: ---*/
                                                /*---       Period < Width or Period = Width, or ---*/
                                                /*---       Period = 0 in PWM_OUT Mode (for Timer0) ---*/
                                                /*---       Set if the Counter wraps (Error Condition) in ---*/
                                                /*---       WIDTH_CAP Mode. ---*/
                                                /*---       Write? one? to clear this bit. ---*/
        volatile unsigned short Reserved2:1;    /*--- 3     Reserved ---*/
        volatile unsigned short IRQ2:1;         /*--- 2 R/W Timer 2 IRQ : ?Write one to clear? ---*/
        volatile unsigned short IRQ1:1;         /*--- 1 R/W Timer 1 IRQ : ?Write one to clear? ---*/
        volatile unsigned short IRQ0:1;         /*--- 0 R/W Timer 1 IRQ : ?Write one to clear? ---*/
        unsigned short Reserved;
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_TIM_CPE_CFG_BASE(a)             KSEG1ADDR(0x19070004 + ((a) * 0x20))
union __vx180_tim_cpe_cfg_register {
    struct _vx180_tim_cpe_cfg_register {
        volatile unsigned short Reserved0:10;   /*--- 15:6  R/W Reserved ---*/
        volatile unsigned short AUX_IN_SEL:1;   /*--- 5     R/W ? 1 : AUX_IN select ---*/
                                                /*---           ? 0 : Reserved ---*/
        volatile unsigned short IRQ_ENA:1;      /*--- 4     R/W ? 1 : Interrupt request enable ---*/
                                                /*---           ? 0: Interrupt request disable ---*/
        volatile unsigned short PERIOD_CNT:1;   /*--- 3     R/W ? 1 : Count to end of period ---*/
                                                /*---           ? 0 : Count to end of width ---*/
        volatile unsigned short PULSE_HI:1;     /*--- 2     R/W ? 1 : Width is measured during Active High ---*/
                                                /*---           ? 0 : Width is measured during Active Low ---*/
        volatile unsigned short MODE_FIELD:2;   /*--- 1:0   R/W ? 00 : Reset state - unused ---*/
                                                /*---           ? 01 : PWM_OUT mode ---*/
                                                /*---           ? 10 : WDTH_CAP mode ---*/
                                                /*---           ? 11 : Reserved ---*/
                                                /*--- (Values are binary) ---*/
        unsigned short Reserved1;
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_TIM_CPE_COUNTER_LOW_BASE(a)             KSEG1ADDR(0x19070008 + ((a) * 0x20))
union __vx180_tim_cpe_counter_low_register {
    struct _vx180_tim_cpe_counter_low_register {
        volatile unsigned short Counter;       /*--- 15:0 RO Timer x Counter Register (Low Word) ---*/
        unsigned short Reserved;
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_TIM_CPE_COUNTER_HIGH_BASE(a)             KSEG1ADDR(0x1907000C + ((a) * 0x20))
union __vx180_tim_cpe_counter_high_register {
    struct _vx180_tim_cpe_counter_high_register {
        volatile unsigned int Counter:16;       /*--- 31:16 RO Timer x Counter Register (high Word) ---*/
        unsigned int Reserved:16;
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_TIM_CPE_PERIOD_LOW_BASE(a)             KSEG1ADDR(0x19070010 + ((a) * 0x20))
union __vx180_tim_cpe_period_low_register {
    struct _vx180_tim_cpe_period_low_register {
        volatile unsigned short Period;       /*--- 15:0 RW Timer x Period Register (Low Word) ---*/
        unsigned short Reserved;
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_TIM_CPE_PERIOD_HIGH_BASE(a)             KSEG1ADDR(0x19070014 + ((a) * 0x20))
union __vx180_tim_cpe_period_high_register {
    struct _vx180_tim_cpe_period_high_register {
        volatile unsigned int Period:16;       /*--- 31:16 RW Timer x Period Register (high Word) ---*/
        unsigned int Reserved:16;
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_TIM_CPE_WIDTH_LOW_BASE(a)             KSEG1ADDR(0x19070018 + ((a) * 0x20))
union __vx180_tim_cpe_width_low_register {
    struct _vx180_tim_cpe_width_low_register {
        volatile unsigned short Width;       /*--- 15:0 RW Timer x Width Register (Low Word) ---*/
        unsigned short Reserved;
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_TIM_CPE_WIDTH_HIGH_BASE(a)             KSEG1ADDR(0x1907001C + ((a) * 0x20))
union __vx180_tim_cpe_width_high_register {
    struct _vx180_tim_cpe_width_high_register {
        volatile unsigned int Width:16;       /*--- 31:16 RW Timer x Width Register (high Word) ---*/
        unsigned int Reserved:16;
    } Bits;
    volatile unsigned int Reg;
};

/*------------------------------------------------------------------------------------------*\
 * Asynchronous Memory Global Control Register (AMC_AMGCTL)
\*------------------------------------------------------------------------------------------*/
#define VX180_ASYNC_MEMORY_BASE             (0x19148000)

#define VX180_ASYNC_GENERAL_STATUS_BASE            KSEG1ADDR(0x00 + VX180_ASYNC_MEMORY_BASE)
union __vx180_async_general_status_register {
    struct _vx180_async_general_status_register {
        volatile unsigned int Reserved3 :16;  /*---  ?            Reserved ---*/
        volatile unsigned int Reserved2 : 8;  /*--- 15:8             Reserved ---*/
        volatile unsigned int B3PEN : 1;      /*--- 7    R/W         Enables Bank 3 16-bit packing.  0 : Disables 16-bit packing 1 : Enables 16-bit packing See [NOTE !]. ---*/
        volatile unsigned int B2PEN : 1;      /*--- 6    R/W         Enables Bank 2 16-bit packing.  0 : Disables 16-bit packing 1 : Enables 16-bit packing See [NOTE !]. ---*/
        volatile unsigned int B1PEN : 1;      /*--- 5    R/W         Enables Bank 1 16-bit packing.  0 : Disables 16-bit packing 1 : Enables 16-bit packing See [NOTE !]. ---*/
        volatile unsigned int B0PEN : 1;      /*--- 4   R/W Enables Bank 0 16-bit packing.  0 : Disables 16-bit packing 1 : Enables 16-bit packing See [NOTE !]. ---*/
        volatile unsigned int Reserved1 : 1;  /*--- 3       Reserved ---*/
        volatile unsigned int AMBEN : 2;      /*--- 2:1 R/W Enables asynchronous memory banks.  00 : Disables all banks 01 : Enables Bank 0 10 : Enabled Bank 1 11 : Enables all the 4 banks ---*/
        volatile unsigned int Reserved0 : 1;  /*--- 0   R/W Reserved ---*/
        /*------------------------------------------------------------------------------------------*\
        NOTE ! The BxPEN bits are used to enable 16-bit packing mode for each bank independently. In
        16-bit packing mode, 32-bit transactions to AMC space that are requested on internal buses are
        converted into two sequential 16-bit accesses on the external bus. All 8-bit and 16-bit requests are
        still processed with a single external transaction. However, for upper 16-bit addresses, the data is
        transferred to the lower 16 bits of the bus (as compared with a 32-bit transaction).
        \*------------------------------------------------------------------------------------------*/
    } Bits;
    volatile unsigned int Reg;
};


#define VX180_ASYNC_BANK0_CTL_BASE            KSEG1ADDR(0x04 + VX180_ASYNC_MEMORY_BASE)
union __vx180_async_bank_ctl_register {
    struct _vx180_async_bank_ctl_register {
        volatile unsigned int B1WAT : 4;           /*--- 31:28 R/W         Enables Bank 1 write access time.  Represents the number of cycles AWE is held asserted.  0000 : Reserved 0001 – 1111 : One to fifteen cycles NOTE: The write time programmed must have two additional cycles when wait states are included as per the design aspect. ---*/
        volatile unsigned int B1RAT : 4;           /*--- 27:24 R/W         Enables Bank 1 read access time.  Represents the number of cycles ARE is held asserte 0000 : Reserved 0001 – 1111 : One to fifteen cycles NOTE: The read-time programmed must have two additional cycles when wait states are included as pe the design aspect ---*/
        volatile unsigned int B1HT : 2;            /*--- 23:22 R/W 11 : Three cycles Enables Bank 1 hold time.  Represents the number of cycles inserted after strobe is de-asserted, before AOE is de-asserted.  00 : Zero cycles 01 : One cycle 10 : Two cycles 11 : Three cycles ---*/
        volatile unsigned int B1ST : 2;            /*--- 21:20 R/W Enables Bank 1 setup time.  Represents the number of cycles inserted after AOE asserted, before AWE or ARE is asserted.  to  00 : Four cycles 01 : One cycle 10 : Two cycles ---*/
        volatile unsigned int B1TT : 2;            /*--- 19:18 R/W         Enables Bank 1 memory transition time.  Represents number of cycles inserted after a read access to this bank, and before a write access to this bank, or a read access of any other bank.  00 : Four cycles allocated for bank transition 01 : One cycle allocated for bank transition 10 : Two cycles allocated for bank transition 11 : Three cycles allocated for bank transition ---*/
        volatile unsigned int B1RDYPOL : 1;        /*--- 17    R/W         Enables Bank 1 ARDY active high.  1 : Transaction completes if ARDY sampled high 0 : Transaction completes if ARDY sampled low ---*/
        volatile unsigned int B1RDYEN : 1;         /*--- 16    R/W         Enables Bank 1 ARDY.  1 : After access time countdown, use state of ARDY to determine completion of access to this memory bank 0 : Ignore ARDY for accesses to this memory ban ---*/
        volatile unsigned int B0WAT : 4;           /*--- 15:12 R/W         Enables Bank 0 write access time.  Represents the number of cycles AWE is held asserted.  0000 : Reserved 0001 – 1111 : One to fifteen cycles NOTE: The write time programmed must have two additional cycles when wait states are included as per the design aspect. ---*/
        volatile unsigned int B0RAT : 4;           /*--- 11:8  R/W         Enables Bank 0 read access time.  Represents the number of cycles ARE is held asserted 0000 : Reserved 0001 – 1111 : One to fifteen cycles NOTE: The read-time programmed must have two additional cycles when wait states are included as per the design aspect. ---*/
        volatile unsigned int B0HT : 2;            /*--- 7:6   R/W         Enables Bank 0 hold time.  Represents the number of cycles inserted after strobe is de-asserted, before AOE is de-asserted.  00 : Zero cycles 01 : One cycle 10 : Two cycles 11 : Three cycles ---*/
        volatile unsigned int B0ST : 2;            /*--- 5:4   R/W         Enables Bank 0 setup time.  Represents the number of cycles inserted after AOE i asserted, before AWE or ARE is asserted.  00 : Four cycles 01 : One cycle 10 : Two cycles 11 : Three cycles ---*/
        volatile unsigned int B0TT : 2;            /*--- 3:2   R/W         Enables Bank 0 memory transition time.  Represents the number of cycles inserted after a read access to this bank, and before a write access to this bank, or a read access of any other bank.  00 : Four cycles allocated for bank transition 01 : One cycle allocated for bank transition 10 : Two cycles allocated for bank transition 11 : Three cycles allocated for bank transition ---*/
        volatile unsigned int B0RDYPOL : 1;        /*--- 1     R/W         Enables Bank 0 ARDY active high.  1 : Transaction completes if ARDY sampled high 0 : Transaction completes if ARDY sampled low. ---*/
        volatile unsigned int B0RDYEN : 1;         /*--- 0     R/W         Enables Bank 0 ARDY.  1 : After access time countdown, use state of ARDY to determine completion of access to this memory bank.  0 : Ignore ARDY for accesses to this memory bank. ---*/
    } Bits;
    volatile unsigned int Reg;
};


#define VX180_ASYNC_BANK1_CTL_BASE            KSEG1ADDR(0x08 + VX180_ASYNC_MEMORY_BASE)
/*--- identisch zu VX180_ASYNC_BANK0_CTL_BASE ---*/            


/*------------------------------------------------------------------------------------------*\
 * DDR 
\*------------------------------------------------------------------------------------------*/
#define VX180_DDR_MEMORY_BASE             (0x19250000)

#define VX180_DDR_AHB_ARBITER_CTL_BASE    KSEG1ADDR(0x80 + VX180_DDR_MEMORY_BASE)
union __vx180_ddr_ahb_arbiter_ctl_register {
    struct _vx180_ddr_ahb_arbiter_ctl_register {
        volatile unsigned int ENMsec : 1;              /*--- 31            R/W      Sets 1 milli sec timer in the TTTL counter to 1 micro sec clock instead of 166 Mhz ---*/
        volatile unsigned int Reserved2 : 6;           /*--- 30:25         —        Reserved ---*/
        volatile unsigned int DisableDDRPrec : 5;      /*--- 24:20         R/W      Disables precharge on completion of a AHB bus access to SDRAM. If precharge is disabled, then the page remains Open until the next access.  If page is missed: 1. Precharge is performed.  2. Access is completed.  Port number corresponding to offset: port0: Bit 20 port1: Bit 21 port2: Bit 22 port3: Bit 23 port4: Bit 24 ---*/
        volatile unsigned int Reserved1 : 3;           /*--- 19:17         —        Reserved ---*/
        volatile unsigned int StaticPri3 : 5;          /*--- 16:12         R/W      Enables ports with Static Prority Level 3 (StaticPri3).  NOTE: Ports configured with StaticPri2 have higher priority than ports configured with StaticPri3.  Port Numbers corresponding to offset: port0: Bit 12 port1: Bit 13 port2: Bit 14 port3: Bit 15 port4: Bit 16 Reserved            11:9          —        Reserved ---*/
        volatile unsigned int Reserved0 : 3;           /*--- 11:9          —        Reserved ---*/
        volatile unsigned int StaticPri2 : 5;          /*--- 8:4           R/W      Enables ports with Static Prority Level 2 (StaticPri2).  NOTE: Ports configured with StaticPri2 have higher priority than ports configured with StaticPri3.  Port Numbers corresponding to offset: port0: Bit 4 port1: Bit 5 port2: Bit 6 port3: Bit 7 port4: Bit 8 ---*/
        volatile unsigned int DynamicPri1 : 2;         /*--- 3:2           R/W      Enables bits with Dynamic priority1.  NOTE: Dynamic Priority1 is only assigned for MIPS and xDSL processors.  Port Numbers corresponding to offset: port0 (MIPS): Bit 2 port1(xDSL): Bit 3 ---*/
        volatile unsigned int DynamicPri0 : 2;         /*--- 1:0           R/W      Enables bits with Dynamic priority0.  NOTE: Dynamic Priority0 is only assigned for MIPS and xDSL processors.  Port Numbers corresponding to offset: port0 (MIPS): Bit 0 port1(xDSL): Bit 1 ---*/
    } Bits;
    volatile unsigned int Reg;
};


#define VX180_DDR_AHB0_HIGH_PRIO_COUNTER_BASE    KSEG1ADDR(0x84 + VX180_DDR_MEMORY_BASE)
union __vx180_ddr_ahb0_high_prio_counter {
    struct _vx180_ddr_ahb0_high_prio_counter {
        unsigned short Reserved;
        unsigned short TimeToLiveZeroPri;
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DDR_AHB1_HIGH_PRIO_COUNTER_BASE    KSEG1ADDR(0x88 + VX180_DDR_MEMORY_BASE)
union __vx180_ddr_ahb1_high_prio_counter {
    struct _vx180_ddr_ahb1_high_prio_counter {
        unsigned short Reserved;
        unsigned short TimeToLiveZeroPri;
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DDR_AHB0_LOW_PRIO_COUNTER_BASE    KSEG1ADDR(0x8C + VX180_DDR_MEMORY_BASE)
union __vx180_ddr_ahb0_low_prio_counter {
    struct _vx180_ddr_ahb0_low_prio_counter {
        unsigned short Reserved;
        unsigned short TimeToLiveZeroPri;
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DDR_AHB1_LOW_PRIO_COUNTER_BASE    KSEG1ADDR(0x90 + VX180_DDR_MEMORY_BASE)
union __vx180_ddr_ahb1_low_prio_counter {
    struct _vx180_ddr_ahb1_low_prio_counter {
        unsigned short Reserved;
        unsigned short TimeToLiveZeroPri;
    } Bits;
    volatile unsigned int Reg;
};


#define VX180_DDR_MEMORY_CTRL0_BASE              KSEG1ADDR(0x00 + VX180_DDR_MEMORY_BASE)
union __vx180_ddr_memory_ctrl0 {
    struct _vx180_ddr_memory_ctrl0 {
        volatile unsigned int Active_to_active : 4;    /*--- 31:28            R/W        time[3:0], (tRC), Number of clock cycles from an active command to ---*/
                                                       /*--- next active command. ---*/
                                                       /*--- (Default: 0xb) ---*/
        volatile unsigned int Minimum_active : 5;      /*--- 27:23            R/W        to precharge time[4:0], (tRAS), The number of clock cycles from an active command ---*/
                                                       /*--- until a precharge command is issued. ---*/
                                                       /*--- To calculate this value, divide the minimum RAS to ---*/
                                                       /*--- pre-charge delay of SDRAM by clock cycle time. ---*/
                                                       /*--- (Deafult:0x2) ---*/
        volatile unsigned int Precharge_to_active : 4; /*--- 22:19            R/W        command time[3:0], (iRP) The number of clock cycles needed for SDRAM to: ---*/
                                                       /*--- Recover from a precharge command, and ---*/
                                                       /*--- Prepare to accept next active command. ---*/
                                                       /*--- (Default: 0x2) ---*/
        volatile unsigned int Refresh_to_active : 6;   /*--- 18:13            R/W        command_delay[5:0], The number of clock cycles needed for SDRAM to: ---*/
                                                       /*--- Recover from a refresh signal, and ---*/
                                                       /*--- Prepare to accept next active command (tRC/Clock ---*/
                                                       /*--- Period). ---*/
                                                       /*--- (Default: 0x1C) ---*/
        volatile unsigned int Refresh_Interval : 13;   /*--- 12:0             R/W        The number of clock cycles from one refresh cycle to ---*/
                                                       /*--- next refresh cycle. ---*/
                                                       /*--- To obtain this value: ---*/
                                                       /*--- 1. Divide the SDRAM refresh period (Tref) by total ---*/
                                                       /*--- number of rows to be refreshed. ---*/
                                                       /*--- 2. Divide the result by total cycle time. ---*/
                                                       /*--- (Default: 0x618) ---*/
    } Bits;
    volatile unsigned int Reg;
};


#define VX180_DDR_MEMORY_CTRL1_BASE              KSEG1ADDR(0x04 + VX180_DDR_MEMORY_BASE)
union __vx180_ddr_memory_ctrl1 {
    struct _vx180_ddr_memory_ctrl1 {
        volatile unsigned int Write_to_ReadDelay : 4;   /*--- 31:28 R/W The write-to-read delay.
                                                                Represents the last write data to the next read
                                                                command as specified by SDRAM Datasheet.
                                                                (Default: 4?b0010) ---*/
        volatile unsigned int Reserved : 7;             /*--- 27:21 ---*/
        volatile unsigned int SDRAM_Device_Size:3;      /*--- 20:18 R/W ? 000 : Reserved 
                                                                ? 001 : Individual SDRAM is 64 Mbit
                                                                ? 010 : Individual SDRAM is 128 Mbit
                                                                ? 011 : Individual SDRAM is 256 Mbit
                                                                ? 100 :Individual SDRAM is 512 Mbit
                                                                (Default: 3?b010) ---*/
        volatile unsigned int SDRAM_Device_Width : 2;   /*--- 17:16           R/W           00: Individual SDRAM is 4-bit wide ---*/
                                                        /*--- 01: Individual SDRAM is 8-bit wide ---*/
                                                        /*--- 10: Individual SDRAM is 16-bit wide ---*/
                                                        /*--- 11: Reserved ---*/
                                                        /*--- (Default: 2’b10) ---*/
        volatile unsigned int External_Banks : 2;       /*--- 15:14           R/W           00: 1 external bank (use CS0) ---*/
                                                        /*--- 01: 2 external bank (use CS1, CS0) ---*/
                                                        /*--- 10: Reserved ---*/
                                                        /*--- 11: Reserved ---*/
                                                        /*--- (Default:2’b01) ---*/
        volatile unsigned int Total_SDRAM : 2;          /*--- 13:12           R         Data Width ---*/
                                                        /*--- 00: Reserved ---*/
                                                         /*--- 01 : 16-bit SDRAM ---*/
                                                        /*--- 10 : Reserved ---*/
                                                        /*--- 11: 32-bit SDRAM ---*/
                                                        /*--- (Default: 2’b01) ---*/
        volatile unsigned int Write_Recovery : 4;       /*--- 11:8            R/W         Time [3:0], The number of clock cycles required for SDRAM to: ---*/
                                                        /*--- Recover from a write, and ---*/
                                                        /*--- Accept a precharge command. ---*/
                                                        /*--- (Default: 2’b11) ---*/
        volatile unsigned int Mode_Register : 4;        /*--- 7:4             R/W         set to active [3:0], (Trsc), The number of clock cycles: ---*/
                                                        /*--- After the setting of the Mode Register in SDRAM, ---*/
                                                             /*--- and ---*/
                                                        /*--- Before the issue of next command. ---*/
                                                        /*--- (Default: 4’b10) ---*/
        volatile unsigned int RAS_to_CAS : 4;           /*--- 3:0 R/W         delay Time [3:0], (Trcd), The number of clock cycles from an active command ---*/
                                                        /*--- to a read/write assertion. ---*/
                                                        /*--- To calculate this value, divide the RAS# delay to ---*/
                                                        /*--- CAS# delay time (tRCD) by the clock cycle time. ---*/
                                                        /*--- (Default: 4’b0011) ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DDR_MEMORY_CTRL2_BASE              KSEG1ADDR(0x08 + VX180_DDR_MEMORY_BASE)
union __vx180_ddr_memory_ctrl2 {
    struct _vx180_ddr_memory_ctrl2 {

        volatile unsigned int REGE : 1;                 /*--- 31   R/W This bit must be high when external registers are ---*/
                                                        /*--- inserted in the control and address signals between ---*/
                                                        /*--- Fusiv Vx180 and DDR SDRAM for instance, when ---*/
                                                        /*--- register mode DDR SDRAM is used. ---*/
                                                        /*--- (Default: 0) ---*/
        volatile unsigned int Reserved1 : 24;           /*--- 30:7 R/W IMPORTANT: User must write 00000 to these bits. ---*/
        volatile unsigned int CAS_Latency : 3;          /*--- 6:4  R/W [2:0], The number of clock cycles from assertion of ---*/
                                                        /*--- read/write signal to the SDRAM to first valid data ---*/
                                                        /*--- output from SDRAM. ---*/
                                                        /*--- “101”: 1.5 ---*/
                                                        /*--- “010”: 2 ---*/
                                                        /*--- “110”: 2.5 ---*/
                                                        /*--- “011”: 3 ---*/
                                                        /*--- (Default: 3’b010) ---*/
        volatile unsigned int Wrap_Type_Register : 1;   /*--- 5    R/W    1: Sequential Wrap type SDRAM. ---*/
                                                        /*--- 0: Interleaved Wrap type SDRAM. ---*/
        volatile unsigned int Reserved0 : 2;            /*--- 3:4 ---*/ 
        volatile unsigned int Burst_length : 3;         /*--- 2:0  R/W [2:0], IMPORTANT: User must write 001 to these bits. ---*/
                                                        /*--- Any other burst length is not supported. ---*/
                                                        /*--- (Default: 3’b001). ---*/
    } Bits;
    volatile unsigned int Reg;
};


#define VX180_DDR_MEMORY_CTRL3_BASE              KSEG1ADDR(0x0C + VX180_DDR_MEMORY_BASE)
union __vx180_ddr_memory_ctrl3 {
    struct _vx180_ddr_memory_ctrl3 {
        volatile unsigned int Reserved : 31;            /*--- 31:1              R           Reserved ---*/
        volatile unsigned int DS : 1;                   /*--- 0                 R/W             0: Impedance match ---*/
                                                        /*--- 1: Reduced Impedance ---*/
                                                        /*--- If SDRAM does not support the DS bit, the user must ---*/
                                                        /*--- program this bit to 0. ---*/
                                                        /*--- (Default: 1) ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DDR_MEMORY_CTRL4_BASE              KSEG1ADDR(0x10 + VX180_DDR_MEMORY_BASE)
union __vx180_ddr_memory_ctrl4 {
    struct _vx180_ddr_memory_ctrl4 {
        unsigned int reserved;
    } Bits;
    volatile unsigned int Reg;
};


/*------------------------------------------------------------------------------------------*\
 * DDR 
\*------------------------------------------------------------------------------------------*/
#define VX180_GPIO_MEMORY_BASE             (0x19000000)
#define VX180_GPIO_DIR_BASE(a)           ((a) == 0 ? KSEG1ADDR(0x40000 + VX180_GPIO_MEMORY_BASE) : KSEG1ADDR(0xB0000 + VX180_GPIO_MEMORY_BASE))
union __vx180_gpio_dir {
    struct _vx180_gpio_dir {
        /*--- If DIR [n]=1, then GPIO[n] is in output direction. ---*/
        /*--- If DIR [n]= 0, then GPIO[n] is in input direction. ---*/
        volatile unsigned short dir;
        unsigned short reserved;
    } Bits;
    volatile unsigned int Reg;
};
#define VX180_GPIO_AS_INPUT        0
#define VX180_GPIO_AS_OUTPUT       1
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void VX180_GPIO_SETDIR(unsigned int gpio, unsigned int direction) {
    union __vx180_gpio_dir *pdir = (union __vx180_gpio_dir *)VX180_GPIO_DIR_BASE(gpio < 16 ? 0 : 1);
    if(direction == VX180_GPIO_AS_INPUT) {
        pdir->Bits.dir &= ~(1 << (gpio % 16));
        /*--- printk(KERN_ERR "[VX180_GPIO_SETDIR:dir=VX180_GPIO_AS_INPUT] ptr-size %d ptr-addr 0x%p value 0x%x\n", ---*/ 
            /*--- sizeof(pdir->Bits.dir), &pdir->Bits.dir, ~(1 << (gpio % 16))); ---*/
    } else {
        pdir->Bits.dir |= (1 << (gpio % 16));
        /*--- printk(KERN_ERR "[VX180_GPIO_SETDIR:dir=VX180_GPIO_AS_OUTPUT] ptr-size %d ptr-addr 0x%p value 0x%x\n", ---*/ 
            /*--- sizeof(pdir->Bits.dir), &pdir->Bits.dir, (1 << (gpio % 16))); ---*/
    }
}
#define VX180_GPIO_FLAG_BASE(a)        ((a) == 0 ? KSEG1ADDR(0x40004 + VX180_GPIO_MEMORY_BASE) : KSEG1ADDR(0xB0004 + VX180_GPIO_MEMORY_BASE))
union __vx180_gpio_flag {
    struct _vx180_gpio_outputflag {
        /*--- Writing ?one? to the lower half Word address (0x190x0004) clears the bit ---*/
        /*--- ? Writing ?one? to the upper half Word address (0x190x0006) sets the bit ---*/
        volatile unsigned short flag_clr;
        volatile unsigned short flag_set;
    } outputBits;
    struct _vx180_gpio_flag {
        /*--- If GPIO[n] is in the input direction, this register ---*/
        /*--- stores the value transmitted to GPIO[n] by an ---*/
        /*--- external Source ---*/
        /*--- If GPIO[n] is in the output direction, this register ---*/
        /*--- to stores the value transmitted by Fusiv Vx180 to ---*/
        /*--- GPIO[n]. ---*/
        volatile unsigned short flag;
        unsigned short reserved;
    } Bits;
    volatile unsigned int Reg;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void VX180_GPIO_OUTPUT(unsigned int gpio, unsigned int set) {
    union __vx180_gpio_flag *pflag = (union __vx180_gpio_flag *)VX180_GPIO_FLAG_BASE(gpio < 16 ? 0 : 1);
    if(set) {
        pflag->outputBits.flag_set = 1 << (gpio % 16);
    } else {
        pflag->outputBits.flag_clr = 1 << (gpio % 16);
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned int VX180_GPIO_INPUT(unsigned int gpio) {
    union __vx180_gpio_flag *pflag = (union __vx180_gpio_flag *)VX180_GPIO_FLAG_BASE(gpio < 16 ? 0 : 1);
    return (pflag->Bits.flag & (1 << (gpio % 16))) ? 1 : 0;
}

#define VX180_GPIO_IRQ_ENABLE_BASE(a)          ((a) == 0 ? KSEG1ADDR(0x40008 + VX180_GPIO_MEMORY_BASE) : KSEG1ADDR(0xb0008 + VX180_GPIO_MEMORY_BASE))
union __vx180_gpio_irq_enable {
    struct _vx180_irq_outputflag {
        /*---  Writing "one" at the lower half-Word address (0x190x0008) allows clearing the bit ---*/
        /*--- ? Writing ?one? at the upper half-Word address (0x190x000A) allows setting the bit ---*/
        volatile unsigned short flag_clr;
        volatile unsigned short flag_set;
    } outputBits;
    struct _vx180_gpio_irq_enable {
        /*--- Enable flag as interrupt Source: ---*/
        /*--- If INTREN [n]= 0, then Interrupt masked. ---*/
        /*--- If INTREN(n)= 1, then Interrupt enabled. ---*/
        /*--- NOTE: Based on the INTREN[n]and the FLAG[n] ---*/
        /*--- value, CPE interrupt is generated. ---*/
        volatile unsigned short irq_enable;
        unsigned short reserved;
    } Bits;
    volatile unsigned int Reg;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void VX180_GPIO_IRQEN(unsigned int gpio, unsigned int enable) {
    union __vx180_gpio_irq_enable *pirq = (union __vx180_gpio_irq_enable *)VX180_GPIO_IRQ_ENABLE_BASE(gpio < 16 ? 0 : 1);
    if(enable) {
        pirq->outputBits.flag_set = 1 << (gpio % 16);
        /*--- printk(KERN_ERR "[VX180_GPIO_IRQEN:set] ptr-size %d ptr-addr 0x%p value 0x%x\n", ---*/ 
            /*--- sizeof(pirq->outputBits.flag_set), &pirq->outputBits.flag_set, 1 << (gpio % 16)); ---*/
    } else {
        pirq->outputBits.flag_clr = 1 << (gpio % 16);
        /*--- printk(KERN_ERR "[VX180_GPIO_IRQEN:clr] ptr-size %d ptr-addr 0x%p value 0x%x\n", ---*/ 
            /*--- sizeof(pirq->outputBits.flag_clr), &pirq->outputBits.flag_clr, 1 << (gpio % 16)); ---*/
    }
}

#define VX180_GPIO_POLAR_BASE(a)         ((a) == 0 ? KSEG1ADDR(0x40010 + VX180_GPIO_MEMORY_BASE) : KSEG1ADDR(0xb0010 + VX180_GPIO_MEMORY_BASE))
union __vx180_gpio_polar {
    struct _vx180_gpio_polar {
        /*--- Select polarity of flag Source input signal: ---*/
        /*--- If POLAR[n] = 0, then active HIGH. ---*/
        /*--- If POLAR[n] = 1, then active LOW.s ---*/
        volatile unsigned short polar;
        unsigned short reserved;
    } Bits;
    volatile unsigned int Reg;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void VX180_GPIO_POLAR(unsigned int gpio, unsigned int mode) {
    union __vx180_gpio_polar *ppolar = (union __vx180_gpio_polar *)VX180_GPIO_POLAR_BASE(gpio < 16 ? 0 : 1);
    if(mode == 0) {
        ppolar->Bits.polar &= ~(1 << (gpio % 16));
        /*--- printk(KERN_ERR "[VX180_GPIO_POLAR:mode=0] ptr-size %d ptr-addr 0x%p value 0x%x\n", ---*/ 
            /*--- sizeof(ppolar->Bits.polar), &ppolar->Bits.polar, ~(1 << (gpio % 16))); ---*/
    } else {
        ppolar->Bits.polar |= (1 << (gpio % 16));
        /*--- printk(KERN_ERR "[VX180_GPIO_POLAR:mode=1] ptr-size %d ptr-addr 0x%p value 0x%x\n", ---*/ 
            /*--- sizeof(ppolar->Bits.polar), &ppolar->Bits.polar, (1 << (gpio % 16))); ---*/
    }
}

#define VX180_GPIO_SENSITIVITY_BASE(a)        ((a) == 0 ? KSEG1ADDR(0x40014 + VX180_GPIO_MEMORY_BASE) : KSEG1ADDR(0xb0014 + VX180_GPIO_MEMORY_BASE))
union __vx180_gpio_sensitivity {
    struct _vx180_gpio_sensitivity {
        /*--- Flag Source input edge/level sensitivity: ---*/
        /*--- If EDGE2[n] = 1, then edge sensitive. ---*/
        /*--- If EDGE2[n]= 0, then level sensitive. ---*/
        volatile unsigned short sensitivity;
        unsigned short reserved;
    } Bits;
    volatile unsigned int Reg;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void VX180_GPIO_SENSITIVE(unsigned int gpio, unsigned int edge) {
    union __vx180_gpio_sensitivity *psens = (union __vx180_gpio_sensitivity *)VX180_GPIO_SENSITIVITY_BASE(gpio < 16 ? 0 : 1);
    if(edge == 0) {
        psens->Bits.sensitivity &= ~(1 << (gpio % 16));
        /*--- printk(KERN_ERR "[VX180_GPIO_SENSITIVE:edge=0] ptr-size %d ptr-addr 0x%p value 0x%x\n", ---*/ 
            /*--- sizeof(psens->Bits.sensitivity), &psens->Bits.sensitivity, ~(1 << (gpio % 16))); ---*/
    } else {
        psens->Bits.sensitivity |= (1 << (gpio % 16));
        /*--- printk(KERN_ERR "[VX180_GPIO_SENSITIVE:edge=1] ptr-size %d ptr-addr 0x%p value 0x%x\n", ---*/ 
            /*--- sizeof(psens->Bits.sensitivity), &psens->Bits.sensitivity, (1 << (gpio % 16))); ---*/
    }
}

#define VX180_GPIO_BOTH_EDGES_BASE(a)         ((a) == 0 ? KSEG1ADDR(0x40018 + VX180_GPIO_MEMORY_BASE) : KSEG1ADDR(0xb0018 + VX180_GPIO_MEMORY_BASE))
union __vx180_gpio_both_edges {
    struct _vx180_gpio_both_edges {
        /*--- Flag Source input two edge sensitive. ---*/
        /*--- If BOTH2[n] = 1, then both-edge sensitive. ---*/
        volatile unsigned short both_edges;
        unsigned short reserved;
    } Bits;
    volatile unsigned int Reg;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void VX180_GPIO_BOTHEDGES(unsigned int gpio, unsigned int bothedges) {
    union __vx180_gpio_both_edges *pboth = (union __vx180_gpio_both_edges *)VX180_GPIO_BOTH_EDGES_BASE(gpio < 16 ? 0 : 1);
    if(bothedges == 0) {
        pboth->Bits.both_edges &= ~(1 << (gpio % 16));
        /*--- printk(KERN_ERR "[VX180_GPIO_BOTHEDGES:both=0] ptr-size %d ptr-addr 0x%p value 0x%x\n", ---*/ 
            /*--- sizeof(pboth->Bits.both_edges), &pboth->Bits.both_edges, ~(1 << (gpio % 16))); ---*/
    } else {
        pboth->Bits.both_edges |= (1 << (gpio % 16));
        /*--- printk(KERN_ERR "[VX180_GPIO_BOTHEDGES:both=0] ptr-size %d ptr-addr 0x%p value 0x%x\n", ---*/ 
            /*--- sizeof(pboth->Bits.both_edges), &pboth->Bits.both_edges, (1 << (gpio % 16))); ---*/
    }
}

#define VX180_GPIO_DIR_NEW_BASE                 KSEG1ADDR(0x40024 + VX180_GPIO_MEMORY_BASE)
union __vx180_gpio_dir_new {
    struct _vx180_gpio_dir_new {
        /*--- ‘1’: Configures pci_clkrun pin as output. ---*/
        /*--- ‘0’ : Configures pci_clkrun pin as input. ---*/
        unsigned short reserved0 : 15;     /*--- 15:1  ---*/
        volatile unsigned int short : 1;  /*--- 0:0        RW   Reset Value 0x0001 (If Host) 0x0000 (If PCI Device) ---*/
        unsigned short reserved1;
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_GPIO_FLAG_NEW_BASE                 KSEG1ADDR(0x40028 + VX180_GPIO_MEMORY_BASE)
union __vx180_gpio_flag_new {
    struct _vx180_gpio_flag_new {
        volatile unsigned short reserved0 : 14;        /*--- 15:2                          Reserved ---*/
        volatile unsigned short PCI_CLKMUX : 1;        /*--- 1:1                 R/W         PIN CONTROL, Reset Value is 1. ---*/
                                                       /*--- This value appears on the pci_clkmux pin of the chip. ---*/
                                                       /*--- This bit can be used to control the external pci clock ---*/
                                                       /*--- generator/mux to select and/or turn on/off the pci ---*/
                                                       /*--- clock. This bit can be set to one by writing “one” to ---*/
                                                       /*--- this bit using either the upper or lower half word ---*/
                                                       /*--- address. This bit can be set to zero by writing zero to ---*/
                                                       /*--- this bit again using either the lower or upper halfword ---*/
                                                       /*--- address of this register. The value written to this bit is ---*/
                                                       /*--- driven on the pci_clkmux pin of the chip. ---*/
        volatile unsigned short PCI_CLKRUN : 1;        /*--- 0:0                 Sticky      PIN FLAG, This bit maintains the status of the pci_clkrun pin.This ---*/
                                                       /*--- R/W         bit is sticky. ---*/
                                                       /*--- When the pci_clkrun pin is configured as output: ---*/
                                                       /*--- Writing “one” to this bit in the upper half address ---*/
                                                       /*--- (0x1904002A) sets this bit and drives 1 on the pci_ ---*/
                                                       /*--- clkrun pin. ---*/
                                                       /*--- Writing “one” to this bit in the lower half address ---*/
                                                       /*--- (0x19040028) resets this bit and drive 0 to the pci_ ---*/
                                                       /*--- clkrun pin. ---*/
                                                       /*--- Writing “zero” has no effect. When the pin is ---*/
                                                       /*--- configured as input, this bit stores the value at the ---*/
                                                       /*--- pin driven from the chip external sources. ---*/
        unsigned short reserved1;
    } Bits;
    volatile unsigned int Reg;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define VX180_GPIO_INTREN_NEW_BASE                 KSEG1ADDR(0x4002C + VX180_GPIO_MEMORY_BASE)
#define VX180_GPIO_POLAR_NEW_BASE                  KSEG1ADDR(0x40034 + VX180_GPIO_MEMORY_BASE)
#define VX180_GPIO_EDGE_NEW_BASE                   KSEG1ADDR(0x40038 + VX180_GPIO_MEMORY_BASE)
#define VX180_GPIO_BOTH_NEW_BASE                   KSEG1ADDR(0x4003C + VX180_GPIO_MEMORY_BASE)

/*------------------------------------------------------------------------------------------*\
 * DMA Configuration
\*------------------------------------------------------------------------------------------*/
#define VX180_DMA_CONFIG_BASE             (0x19120000)

#define VX180_DMA_ID_BASE                 KSEG1ADDR(0x00000 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_id {
    struct _vx180_dma_id {
        volatile unsigned int DMA_VER:16;        /*--- 31:16 R DMA Identification Register is used to identify DMA ---*/
        volatile unsigned int DMA_ID:16;         /*--- 15:0 R DMA Version indicates the current version of DMA ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_CONTROL_BASE                 KSEG1ADDR(0x00004 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_control {
    struct _vx180_dma_control {
        unsigned int Reserved:29;           /*--- 31:3 ---*/  
        volatile unsigned int BURST8:1;     /*--- 2:2 R/W The maximum burst size for the DMA.
                                                          ? 1 : DMA performs data transfers in bursts of 8.
                                                          ? 0: DMA is limited to bursts of 4.
                                                          The numbers 8 and 4 refer to words of 32-bit size each ---*/
        volatile unsigned int DMA_RST:1;    /*--- 1:1 R/W DMA_RST is an active high reset and software uses
                                                          this to reset the DMA controller completely except CSR --*/
        volatile unsigned int DMA_EN:1;     /*--- 0:0 R/W DMA_EN enables the DMA.  This is 0 when the chip comes up from reset and must
                                                          be set to 1 before initiating any data transfers ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_PERIPHERAL_CHAN_NO_BASE          KSEG1ADDR(0x00008 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_peripheral_chan_no {
    struct _vx180_dma_peripheral_chan_no {
        unsigned int Reserved0:5;                   /*--- 31:27 ---*/
        volatile unsigned int UTP_RX_CH_NO :3;      /*--- 26:24 R/W Receive Utopia channel number.
                                                                    Assigns one of the seven channels to be used to
                                                                    receive the data from Utopia to internal/external
                                                                    Memory.  ---*/
        unsigned int Reserved1:5;                   /*--- 23:19 ---*/
        volatile unsigned int UTP_TX_CH_NO :3;      /*--- 18:16 R/W Transmit Utopia channel number.
                                                                    Assigns one of the seven channels to be used to
                                                                    transmit data from internal/external memory to
                                                                    Utopia.---*/
        unsigned int Reserved2:1;                   /*--- 15:15 ---*/
        volatile unsigned int EXT_DMA_RX_CH_NO:3;   /*--- 14:12 R/W External DMA receive channel number.
                                                                    Assigns one of the seven channels to be used to
                                                                    receive the data from an external peripheral to
                                                                    internal/external memory.---*/
        unsigned int Reserved3:1;                   /*--- 11:11 ---*/
        volatile unsigned int SPORT2_RX_CH_NO :3;   /*--- 10:8 R/W Receive SPORT2 channel number.
                                                                   Indicates one of the 7 channels to be used to receive
                                                                   the data from SPORT to Internal/External Memory. ---*/
        unsigned int Reserved4:1;                   /*--- 7:7 ---*/
        volatile unsigned int EXT_DMA_TX_CH_NO:3;   /*--- 6:4 R/W  External DMA transmit channel number.
                                                                   Assigns one of the seven channels to be used to
                                                                   transmit data from internal/external memory to an
                                                                   external peripheral.---*/
        unsigned int Reserved5:1;                   /*--- 3:3 ---*/
        volatile unsigned int SPORT2_TX_CH_NO :3;   /*--- 2:0 R/W Transmit SPORT2 channel number.
                                                                  Indicates one of the 7 channels to be used to transmit
                                                                  the data from Internal/External Memory to SPORT Tx ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_PROCESSOR_INTERRUPT_EN_BASE       KSEG1ADDR(0x00120 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_processor_interrupt_en {
    struct _vx180_dma_processor_interrupt_en {
        volatile unsigned int CPE_INT_EN:1;          /*--- 31:31 R/W Interrupt to CPE enable.
                                                                     Enables the interrupt to CPE from a DMA channel.  ---*/
        unsigned int Reserved:24;                    /*--- 30:7    ---*/
        volatile unsigned int CH_CPE_INT_EN:7;       /*--- 6:0 R/W Channel interrupt to CPE enable.
                                                                   Enables the interrupt from a channel. Bit 0
                                                                   corresponds to channel 0, and so on.---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_PROCESSOR_INTERRUPT_BASE         KSEG1ADDR(0x00124 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_processor_interrupt {
    struct _vx180_dma_processor_interrupt {
        volatile unsigned int CPE_INT:1;           /*--- 31:31 R/W Interrupt to CPE.  Set when any of the bits listed above are set.  ---*/
        volatile unsigned int CH_CPE_INT:7;        /*--- 6:0 R/W Channel interrupt to CPE.
                                                    Each bit in this field corresponds to a channel. A bit is
                                                    set depending on the channel number that generates
                                                    the interrupt to the CPE. On this interrupt, the CPE
                                                    can read this register and determine the channel that
                                                    has generated the interrupt.
                                                    Writing 0 to the appropriate bit field clears the
                                                    register.---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_APU_INTERRUPT_EN_BASE       KSEG1ADDR(0x00128 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_apu_interrupt_en {
    struct _vx180_dma_apu_interrupt_en {
        volatile unsigned int APU_INT_EN:1;        /*--- 31:31 R/W Interrupt to APU enable.
                                                Enables the interrupt to the APU from a DMA
                                                channel.  ---*/
        unsigned int Reserved:24;         /*--- 30:7    ---*/
        volatile unsigned int CH_APU_INT_EN:7;     /*--- 6:0 R/W Channel interrupt to APU enable.
                                                Enables the interrupt from a DMA channel. ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_APU_INTERRUPT_BASE         KSEG1ADDR(0x0012C + VX180_DMA_CONFIG_BASE)
union __vx180_dma_apu_interrupt {
    struct _vx180_dma_apu_interrupt {
        volatile unsigned int APU_INT:1;           /*--- 31:31 R/W Interrupt to APU.
                                                Set when any of the bits listed above are set   ---*/
        unsigned int Reserved:24;         /*--- 30:7    ---*/
        volatile unsigned int CH_APU_INT:7;        /*--- 6:0 R/W Channel interrupt to APU.
                                                Each bit in this field corresponds to a channel. A bit is
                                                set depending the channel number that generates the
                                                interrupt to APU. On this interrupt, the APU can read
                                                this register and determine the channel that has
                                                generated the interrupt.
                                                Writing 0 to the appropriate bit field clears this
                                                register. ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_ERROR_ADDRESS_BASE         KSEG1ADDR(0x00130 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_error_address {
    struct _vx180_dma_error_address {
        volatile unsigned int ERR_ADDR:32;        /*--- R/W Error address.
                                                Indicates the address where an error occurred. The
                                                address may not be the exact address where the error
                                                occurred. For example, in the case of burst transfers,
                                                this register can point to one or two locations
                                                preceding the address where the error occurred. ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_ACTIVE_CHAN_NO_BASE         KSEG1ADDR(0x00134 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_active_chan_no {
    struct _vx180_dma_active_chan_no {
        unsigned int Reserved0:23;                  /*--- 31:9 ---*/
        volatile unsigned int DMA_BUSY:1;           /*--- 8:8 R DMA busy.
                                                                Indicates that the DMA controller is currently busy
                                                                with a transfer---*/
        unsigned int Reserved1:1;                   /*--- 7:7 ---*/
        volatile unsigned int LAST_ACTV_CH_NO:3;    /*--- 6:4 R Last serviced channel number.
                                                                If the DMA controller is active, it points to the last
                                                                serviced channel number. If it is idle, this register
                                                                points to the penultimate channel that the DMA
                                                                controller serviced ---*/
        unsigned int Reserved2:1;                   /*--- 3:3  ---*/
        volatile unsigned int ACTV_CH_NO:3;         /*--- 2:0 R Active channel number.
                                                                Indicates the channel number that is currently active.
                                                                If the DMA controller is idle, it indicates the last
                                                                serviced channel ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_BUFFER_STATUS_BASE         KSEG1ADDR(0x00138 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_buffer_status {
    struct _vx180_dma_buffer_status {
        volatile unsigned int BUFFER_LENGTH:16;             /*--- 31:16 R Indicates the number of data bytes that are yet to be
                                                        fetched from the Source. When the DMA controller is
                                                        idle, this bit is 0.---*/
        volatile unsigned int BD_CONTROL:9;                 /*--- 15:7 R Indicates BD Control Information from the last BD
                                                        fetched---*/
        unsigned int Reserved0:1;                  /*--- 6:6 ---*/
        volatile unsigned int BUFFER_POINTER:6;             /*--- 5:0 R Indicates the number of bytes of data that are fetched
                                                        from the Source and are waiting in the DMA internal
                                                        buffer to be transferred to the Destination. When the
                                                        DMA controller is idle, this register is 0.---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_SOURCE_ADDRESS_BASE         KSEG1ADDR(0x0013C + VX180_DMA_CONFIG_BASE)
union __vx180_dma_source_address {
    struct _vx180_dma_source_address {
        volatile unsigned int SOURCE_ADRESS:32;        /*--- 31:0 R Indicates the address of
                                                                    from where the DMA fetches for the next transfer ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_DESTINATION_ADDRESS_BASE    KSEG1ADDR(0x00140 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_destination_address {
    struct _vx180_dma_destination_address {
        volatile unsigned int DEST_ADRESS:32;        /*--- 31:0 R Indicates the address of the location where the
                                                                  DMA transfers the data for the next transfer---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_CRC_RESULT_BASE             KSEG1ADDR(0x00144 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_crc_result {
    struct _vx180_dma_crc_result {
        volatile unsigned int CRC_RESULT:32;        /*--- 31:0 R Indicates the current value of the CRC Result
                                                                 Register. When DMA is IDLE or not calculating
                                                                 on any channel, it must be 0xFFFFFFFF.---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DMA_DEBUG_BASE             KSEG1ADDR(0x00148 + VX180_DMA_CONFIG_BASE)
union __vx180_dma_debug {
    struct _vx180_dma_debug {
        unsigned int Reserved1:6;              /*--- 31:26 ---*/
        volatile unsigned int DMA_TX:1;                /*--- 25 R Indicates that UTP_TX channel is active ---*/
        volatile unsigned int DMA_UTP_TSTAT:1;         /*--- 24:24 R DMA indicates to Utopia that the next data is
                                                            available---*/
        volatile unsigned int UTP_DMA_TSTB:1;          /*--- 23:23 R Utopia indicates to the DMA that it can accept the
                                                        next data---*/
        volatile unsigned int UTP_RX:1;                /*--- 22:22 R Indicates that UTP_RX channel is active ---*/
        volatile unsigned int UTP_DMA_RSTAT:1;         /*--- 21:21 R DMA indicates to Utopia that it can accept the next
                                                            data---*/
        volatile unsigned int DMA_UTP_RSTB:1;          /*--- 20:20 R Utopia indicates to the DMA controller that the next
                                                            data is available---*/
        volatile unsigned int PER_BYTE_NBL:4;          /*--- 19:16 R Indicates the DMA controller next access size to the
                                                            peripherals. It can be 0, 2, 3 or 4 bytes---*/
        volatile unsigned int DMA_LAST_IN_FRAME:1;     /*--- 15:15 R DMA indicates to Utopia that the last word of a
                                                            cell/frame is being transferred---*/
        volatile unsigned int RE_ARB_REG:7;            /*--- 14:8 R Indicates the status of the seven DMA channels. If any
                                                        of the channels is re-arbitrated or disabled, the
                                                        corresponding bit is set to 1 in this register. Once the
                                                        disabled/re-arbitrated channel is re-invoked, this bit is
                                                        cleared---*/
        unsigned int Reserved0:8;                        /*--- 7:0 ---*/
    } Bits;
    volatile unsigned int Reg;
};

/*--- chan from 1 to 16 ! ---*/
#define VX180_DMA_CHAN_BASE_ADDR_BASE(chan)             KSEG1ADDR(((chan) * 0x10) + VX180_DMA_CONFIG_BASE)
union __vx180_dma_chan_addr {
    struct _vx180_dma_chan_addr {
        volatile unsigned int CH_BASE_ADDR:32;       /*--- 31:0 R/W Indicates the base address of the DMA Buffer
                                                            Descriptors.
                                                            This address is in the internal/external memory. The
                                                            DMA control retrieves the first Buffer Descriptor
                                                            from this location.---*/
    } Bits;
    volatile unsigned int Reg;
};

/*--- chan from 1 to 16 ! ---*/
#define VX180_DMA_CHAN_CTRL_BASE(chan)             KSEG1ADDR(0x0004 + ((chan) * 0x10) + VX180_DMA_CONFIG_BASE)
union __vx180_dma_chan_control {
    struct _vx180_chan_control {
        unsigned int Reserved0:7;                  /*--- 25:31 ---*/
        volatile unsigned int CH_EN:1;             /*--- 24 R/W     Channel enable.
                                                                    Indicates that the channel parameters are valid and the
                                                                    channel is active. The channel participates in
                                                                    arbitration qualified---*/
        unsigned int Reserved1:7;                  /*--- 17:23 ---*/
        volatile unsigned int CH_START:1;          /*--- 16:16 R/W  Channel start.
                                                                    Indicates the start of a channel. The channel
                                                                    participates in arbitration qualified by this signal.
                                                                    DMA Controller resets (writes ?0?) this bit on
                                                                    completion of processing of a channel. It is also reset
                                                                    if a disabled BD is fetched. To re-initiate the transfer
                                                                    on this channel, re-enable (writes ?1?) this bit. ---*/
        volatile unsigned int BD_INT_EN:1;         /*---15:14 R/W   Buffer Descriptor Processing Done Enable enables the
                                                                    interrupt generated by the completion of the current
                                                                    Buffer Descriptor. ---*/
        volatile unsigned int DONE_INT_EN:1;       /*---14:14 R/W   Done Interrupt Enable enables the interrupt generated
                                                                    on processing of all the Buffer Descriptors in a channel. ---*/
        unsigned int Reserved2:2;                  /*--- 12:13 ---*/
        volatile unsigned int INT_DST:2;           /*--- 11:10 R/W  Interrupt Destination indicates to which processor, the
                                                                    interrupt is to generated
                                                                    ? 00 : CPE
                                                                    ? 01 : APU
                                                                    ? 10 : Reserved
                                                                    ? 11 : Reserved---*/
        volatile unsigned int DMA_SD:2;             /*--- 9:8 R/W   DMA Source-Destination indicates the Source and
                                                                    Destination type for the current data transfer
                                                                    ? 00 : Memory-to-Memory
                                                                    ? 01 : Memory-to-Peripheral
                                                                    ? 10 : Peripheral-to-Memory
                                                                    ? 11 : Reserved---*/
        volatile unsigned int FLY_BY_SOURCE:1;      /*--- 7:7 R/W   Fly By Source indicates whether the DMA controller
                                                                    must increment the Source address after fetching data
                                                                    from Source location
                                                                    ? 1 : Do not increment
                                                                    ? 0 : Increment---*/
        volatile unsigned int FLY_BY_DEST:1;        /*--- 6:6 R/W   Fly By DESTINATION indicates whether the DMA
                                                                    controller must increment the Destination address
                                                                    after transferring the data to the Destination
                                                                    ? 1 : Do not increment
                                                                    ? 0 : Increment---*/
        volatile unsigned int BYTE_EN:1;            /*--- 5:4 R/W   Byte Enable indicates the size of the transfer for each
                                                                    of the bus cycles initiated by the DMA controller.
                                                                    ? 00 : 32 bit access
                                                                    ? 01 : 16 bit access
                                                                    ? 10 : 8 bit access
                                                                    ? 11 : Reserved---*/
        volatile unsigned int CRC_SINK_MODE:1;      /*--- 3:3 R/W   Enables the CRC Sink Mode where data fetched from
                                                                    memory is sunk in the DMA controller, that is, it is
                                                                    not transferred to the Destination.---*/
        volatile unsigned int CRC_EN:1;             /*--- 2:2 R/W   Enables CRC-32 on the data that has been read from
                                                                    the memory. The CRC value is written back to the
                                                                    Buffer Descriptor area in the memory.---*/
        unsigned int Reserved3:1;                    /*--- 1:1 ---*/
        volatile unsigned int CH_RST:1;              /*--- 0:0 R/W  Channel Reset. This signal resets the channel
                                                                    completely. It aborts the data transfer even if it is in
                                                                    the middle of a transfer. It also resets all the relevant
                                                                    registers such as per-channel number, base and control
                                                                    address, channel status, current buffer descriptor, etc.---*/
    } Bits;
    volatile unsigned int Reg;
};

/*--- chan from 1 to 16 ! ---*/
#define VX180_DMA_CHAN_STATUS_BASE(chan)             KSEG1ADDR(0x0008 + ((chan) * 0x10) + VX180_DMA_CONFIG_BASE)
union __vx180_dma_chan_status {
    struct _vx180_chan_status {
        unsigned int Reserved2:23;                  /*--- 31:9 ---*/
        volatile unsigned int DMA_ACTIVE:1;         /*--- 8:8 R     DMA Active indicates that this DMA channel is
                                                                    currently active.---*/
        volatile unsigned int CURR_DONE:1;          /*--- 7:7 R/W   Current DONE indicates that the processing of the
                                                          c         urrent Buffer Descriptor is completed.---*/
        volatile unsigned int DONE:1;               /*--- 6:6 R/W   DONE indicates that all the Buffer Descriptors in a
                                                                    channel have been processed.---*/
        unsigned int Reserved1:2;                   /*--- 5:4 ---*/
        volatile unsigned int CH_APU_INT:1;         /*--- 3:3 R/W   Interrupt caused by the particular channel.
                                                                    It is generated to the APU. This bit is set only if either
                                                                    CURR_DONE or DONE bit is set.---*/
        volatile unsigned int CH_CPE_INT:1;         /*--- 2:2 R/W   Interrupt caused by the particular channel.
                                                                    It is generated to the CPE. This bit is set only if either
                                                                    CURR_DONE or DONE bit is set. ---*/
        unsigned int Reserved0:2;                   /*--- 1:0 ---*/
    } Bits;
    volatile unsigned int Reg;
};

/*--- chan from 1 to 16 ! ---*/
#define VX180_DMA_CHAN_CBD_ADDRESS_BASE(chan)             KSEG1ADDR(0x000C + ((chan) * 0x10) + VX180_DMA_CONFIG_BASE)
union __vx180_dma_chan_cbd_address {
    struct _vx180_dma_chan_cbd_address {
        volatile unsigned int CH_CBD_ADDR:32;         /*--- 31:0 R/W    Current Buffer Descriptor Address indicates the
                                                                        address of the Buffer Descriptor that is currently being
                                                                        processed. On a DONE interrupt, this register points
                                                                        to the last processed BD. If a BD is disabled, it points
                                                                        to the disabled BD.---*/
    } Bits;
    volatile unsigned int Reg;
};

union __vx180_dma_chan_bd_control {
    struct _vx180_dma_chan_bd_control {
        volatile unsigned int ENDCON:1;         /*--- 31:31 R/W   Indicates the DMA controller to convert the Endianess of the Word while
                                                                  transferring data to the Destination.
                                                                  ? 0: Big Endian
                                                                  ? 1: Little Endian
                                                                  Default is Big Endian---*/
        unsigned int Reserved0:6;               /*--- 30:25 ---*/
        volatile unsigned int UTP_WB:1;         /*--- 24:24 R/W   Setting this bit enables the DMA controller to write the Utopia Receive
                                                                  status to the CRC_Result Word of the BD. ---*/
        volatile unsigned int DESC_WB:1;        /*--- 23:23 R/W   Descriptor write back.
                                                                  If this bit is set, the DMA controller clears the DESC_EN bit after
                                                                  processing a BD. This can be used by the software to detect if a BD has been processed  ---*/
        volatile unsigned int DESC_EN:1;        /*--- 22   R/W    Buffer descriptor enable.
                                                                  Indicates the signal validating the Buffer Descriptor. If this bit is set to 0,
                                                                  the Buffer Descriptor is not processed and the DMA Controller suspends
                                                                  the processing of this channel until the Processor re-enables it by setting
                                                                  the CH_START bit in the CH_CTRL Register.  ---*/
        volatile unsigned int CRC_WR:1;         /*--- 21 R/W      CRC write back.
                                                                  ? 1 : Write back CRC result and reset the CRC Register.
                                                                  ? 0 : Continue accumulating the CRC.  ---*/
        volatile unsigned int RE_ARB:1;         /*---  20 R/W     Re-arbitration.
                                                                  ? 0 : Continues with Buffer Descriptor processing.
                                                                  ? 1 : Re-arbitrates for the channel after processing this Buffer Descriptor.
                                                                  If CHAIN_EN is set, the current Buffer Descriptor address is switched to
                                                                  the Channel Base Address. Else, it points to the next Buffer Descriptor. ---*/
        volatile unsigned int LIFRM:1;          /*--- 19 R/W      Last in frame.
                                                                  If the current Buffer Descriptor corresponds to the last cell in an AAL5
                                                                  frame (Utopia), this bit is set. The same indication is passed to the
                                                                  appropriate peripheral so that any required padding can be provided.  ---*/
        volatile unsigned int LAST_BD:1;        /*--- 18 R/W Last Buffer Descriptor indicates that the current Buffer Descriptor is the
                                                                  last BD for this channel.  ---*/
        volatile unsigned int INT_EN:1;         /*--- 17 R/W      Interrupt Enable for the particular Buffer Descriptor. An interrupt to the
                                                                  Processor/APU is generated only if this bit is set.  ---*/
        volatile unsigned int CHAIN_EN:1;       /*--- 16 R/W      Chain enable.
                                                                  Enables circular looping of the Buffer Descriptors after the last BD in a
                                                                  chain is processed.  ---*/
        volatile unsigned int BUF_LEN:16;       /*--- 15:0 R/W    Buffer length.
                                                                  Indicates the block length for the current data transfer of BD. The
                                                                  maximum buffer length is 64 Kbytes  ---*/
    } Bits;
    volatile unsigned int Reg;
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _vx180_dma_bufferdescriptor {
    union __vx180_dma_chan_bd_control BD_Ctrl;
    union __vx180_dma_source_address Src;
    union __vx180_dma_destination_address Dest;
    union __vx180_dma_crc_result CRC_Result;
};
/*--------------------------------------------------------------------------------*\
 * DSP + IDMA
\*--------------------------------------------------------------------------------*/
#define VX180_IDMA_ERROR_STATUS_REGISTER(idma) (volatile unsigned int *)((idma == 0) ? KSEG1ADDR(0x19130014) :\
                                                                         (idma == 1) ? KSEG1ADDR(0x19130030)  : KSEG1ADDR(0x1913003C))

#define VX180_IDMA_ERROR_ADDR_REGISTER(idma)   (volatile unsigned int *)((idma == 0) ? KSEG1ADDR(0x19130018) :\
                                                                         (idma == 1) ? KSEG1ADDR(0x19130034)  : KSEG1ADDR(0x19130040))

#define VX180_IACK_MAXIMUMCOUNT_REGISTER          (volatile unsigned int *)KSEG1ADDR(0x19050400)

#define VX180_IDMA_BASE             (0x191C0000)

#define VX180_IDMA_BASE_REGISTER(dsp)             ((volatile unsigned int *)KSEG1ADDR(0x0 + ((dsp) * 0x10) + VX180_IDMA_BASE))
#define VX180_IDMA_PROG_MEMORY_ACCESS(dsp)        ((volatile unsigned int *)KSEG1ADDR(0x4 + ((dsp) * 0x10) + VX180_IDMA_BASE))
#define VX180_IDMA_DATA_MEMORY_ACCESS(dsp)        ((volatile unsigned short *)KSEG1ADDR(0x8 + ((dsp) * 0x10) + VX180_IDMA_BASE))

#define DSPx_DATAMEM_OFFSET     0x4000
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void adsp_writedataword(unsigned int dsp, unsigned int addr, unsigned short value) {
    /*--- printk("dw%d %p %p addr %x val=%x\n", dsp, VX180_IDMA_BASE_REGISTER(dsp), VX180_IDMA_DATA_MEMORY_ACCESS(dsp), addr, value); ---*/
    *VX180_IDMA_BASE_REGISTER(dsp)      = addr + DSPx_DATAMEM_OFFSET;
    *VX180_IDMA_DATA_MEMORY_ACCESS(dsp) = (unsigned short)value;
    /*--- printk("dw%d status=%x err_addr=%x iack=%d\n", dsp, *VX180_IDMA_ERROR_STATUS_REGISTER(dsp), *VX180_IDMA_ERROR_ADDR_REGISTER(dsp), *VX180_IACK_MAXIMUMCOUNT_REGISTER); ---*/
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void adsp_writeprogword(unsigned int dsp, unsigned int addr, unsigned int value) {
    /*--- printk("pw%d %p %p addr %x val=%x\n", dsp, VX180_IDMA_BASE_REGISTER(dsp), VX180_IDMA_PROG_MEMORY_ACCESS(dsp), addr, value); ---*/
    *VX180_IDMA_BASE_REGISTER(dsp) = addr;
    *VX180_IDMA_PROG_MEMORY_ACCESS(dsp) = value;
    /*--- printk("dw%d status=%x err_addr=%x iack=%d\n", dsp, *VX180_IDMA_ERROR_STATUS_REGISTER(dsp), *VX180_IDMA_ERROR_ADDR_REGISTER(dsp), *VX180_IACK_MAXIMUMCOUNT_REGISTER); ---*/
}
/*--------------------------------------------------------------------------------*\
 * ab  0x4000 DataMem ??
\*--------------------------------------------------------------------------------*/
static inline void adsp_writedatamemory(unsigned int dsp, unsigned int destaddr, unsigned short *srcaddr, unsigned short words) {
    volatile unsigned short *accessaddr;
    if(words == 0) {
        return;
    }
    *VX180_IDMA_BASE_REGISTER(dsp) = destaddr + DSPx_DATAMEM_OFFSET;
    accessaddr = VX180_IDMA_DATA_MEMORY_ACCESS(dsp);
    while(words--) {
        *accessaddr = *srcaddr++;
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void adsp_writeprogmemory(unsigned int dsp, unsigned int destaddr, unsigned int *srcaddr, unsigned short words) {
    if(words == 0) {
        return;
    }
    *VX180_IDMA_BASE_REGISTER(dsp) = destaddr;
    while(words--) {
        *VX180_IDMA_PROG_MEMORY_ACCESS(dsp) = *srcaddr++;
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned short adsp_readdataword(unsigned int dsp, unsigned int addr) {
    *VX180_IDMA_BASE_REGISTER(dsp) = addr + DSPx_DATAMEM_OFFSET;
    /*--- printk("dr%d %p %p addr %x val=%x\n", dsp, VX180_IDMA_BASE_REGISTER(dsp), VX180_IDMA_DATA_MEMORY_ACCESS(dsp), addr, *VX180_IDMA_DATA_MEMORY_ACCESS(dsp)); ---*/
    return *VX180_IDMA_DATA_MEMORY_ACCESS(dsp);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned int adsp_readprogword(unsigned int dsp, unsigned int addr) {
    /*--- printk("pr%d %p %p addr %x val=%x\n", dsp, VX180_IDMA_BASE_REGISTER(dsp), VX180_IDMA_PROG_MEMORY_ACCESS(dsp), addr, *VX180_IDMA_PROG_MEMORY_ACCESS(dsp)); ---*/
    *VX180_IDMA_BASE_REGISTER(dsp) = addr;
    return *VX180_IDMA_PROG_MEMORY_ACCESS(dsp);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void adsp_readdatamemory(unsigned int dsp, unsigned short *destaddr, unsigned int srcaddr, unsigned short words) {
    volatile unsigned short *acessaddr;
    if(words == 0) {
        return;
    }
    *VX180_IDMA_BASE_REGISTER(dsp) = srcaddr + DSPx_DATAMEM_OFFSET;
    acessaddr = VX180_IDMA_DATA_MEMORY_ACCESS(dsp);
    while(words--) {
        *destaddr++ = (unsigned short)*acessaddr;
    }
}
#define VX180_DSP_TO_CPE_PERIPHERAL_INTERRUPT_BASE KSEG1ADDR(0x19050300)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_cpe_peripheral_interrupt_register {
    struct _vx180_cpe_peripheral_interrupt_register {
        volatile unsigned int Reserved0:2;      /*--- 30: 31 ---*/
        volatile unsigned int IDMA_PRTY:2;      /*--- 29:28 R/W IDMA Priority between Data Bus requests and
                                                            Control Bus requests.
                                                            ? 00 : Data Bus 2 has priority
                                                            ? 01 : Control Bus has priority
                                                            ? 10 : Round robin priority
                                                            ? 11 : Reserved           ---*/
        volatile unsigned int Reserved1:2;      /*--- 27:26 R   Reserved ---*/
        volatile unsigned int DSP1_RESET_N:1;   /*--- 25    R/W DSP1 Reset (active HIGH). ---*/
        volatile unsigned int DSP1_PWDACK:1;    /*--- 24    R   DSP1 PWD acknowledgment. ---*/
        volatile unsigned int DSP1_PDWN:1;      /*--- 23    R/W DSP1 Power Down (Active HIGH). ---*/
        volatile unsigned int Reserved2:2;      /*--- 22:21 R Reserved ---*/
        volatile unsigned int DSP1_NINTE:1;     /*--- 20 R/W Edge Sensitive interrupt from CPE to DSP1.  Toggles when 1 is written. ---*/
        volatile unsigned int DSP1_NINTL:1;     /*--- 19 R/W Level sensitive interrupt1 from CPE to DSP1.
                                                             Write zero to clear.
                                                             Write 1 to generate interrupt. ---*/
        volatile unsigned int Reserved3:1;      /*--- 18 R Reserved ---*/
        volatile unsigned int IPC_DSP1_PF1:1;   /*--- 17 R/W Programmable Flag interrupt-1 from CPE to DSP1. ---*/
        volatile unsigned int IPC_DSP1_PF0:1;   /*--- 16 R/W Programmable Flag interrupt-0 from CPE to DSP1. ---*/
        volatile unsigned int Reserved4:5;      /*--- 15:11 Reserved ---*/
        volatile unsigned int VPHY_FLAG_POLARITY:1; /*--- 10 WO VDSL Flag Polarity.
                                                           1 : Invert the flag/programmable flag interrupts received from VDSL. ---*/
        volatile unsigned int DSP0_RESET_N:1;    /*--- 9 R/W DSP0 Reset (Active HIGH). ---*/
        volatile unsigned int DSP0_PWDACK:1;     /*--- 8 R DSP0 PWD Acknowledgment ---*/
        volatile unsigned int DSP0_PDWN:1;       /*--- 7 R/W DSP0 Power down. (Active HIGH). ---*/
        volatile unsigned int Reserved5:2;       /*--- 6:5 R Reserved. ---*/
        volatile unsigned int DSP0_NINTE:1;      /*--- 4 R/W Edge Sensitive interrupt from CPE to DSP0. Toggles when 1 is written. ---*/
        volatile unsigned int DSP0_NINTL1:1;     /*--- 3 R/W Level sensitive interrupt-1 from CPE to DSP0.
                                                            Write ?0? to clear.
                                                            Write ?1? to generate interrupt. ---*/
        volatile unsigned int Reserved6:1;       /*--- 2 R Reserved ---*/
        volatile unsigned int IPC_DSP0_PF1:1;    /*--- 1 R/W Programmable Flag interrupt-1 from CPE to DSP0 ---*/
        volatile unsigned int IPC_DSP0_PF0:1;    /*--- 0 R/W Programmable Flag interrupt-0 from CPE to DSP0 ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_DSP_TO_CPE_INTERRUPT_FLAG_BASE             KSEG1ADDR(0x19050634)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_dsp_to_cpe_flag_interrupt_register {
    struct _vx180_dsp_to_cpe_flag_interrupt_register {
        volatile unsigned int Reserved0:  7;    /*--- 31:25 ---*/
        volatile unsigned int DSP1_FL1_INT:1;   /*--- 24 R/W Flag-1 Interrupt from DSP1 to CPE ---*/
        volatile unsigned int Reserved1:  7;    /*--- 17:23 ---*/
        volatile unsigned int DSP1_FL0_INT:1;   /*--- 16 R/W Flag-0 Interrupt from DSP1 to CPE ---*/
        volatile unsigned int Reserved2:  7;    /*--- 9:15 ---*/
        volatile unsigned int DSP0_FL1_INT:1;   /*--- 8 R/W Flag-1 Interrupt from DSP0 to CPE ---*/
        volatile unsigned int Reserved3:  7;    /*--- 1:7 ---*/
        volatile unsigned int DSP0_FL0_INT:1;   /*--- 0 R/W Flag-0 Interrupt from DSP0 to CPE ---*/
    } Bits;
    volatile unsigned int Reg;
};
#define VX180_DSP_TO_CPE_PROGRAMMABLE_INTERRUPT_FLAG_BASE             KSEG1ADDR(0x19050638)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_dsp_to_cpe_programmable_flag_interrupt_register {
    struct _vx180_dsp_to_cpe_programmable_flag_interrupt_register {
        volatile unsigned int Reserved0:  7;    /*--- 31:25 ---*/
        volatile unsigned int DSP1_PFL1_INT:1;  /*--- 24 R/W Programmable Flag-1 Interrupt from DSP1 to CPE ---*/
        volatile unsigned int Reserved1:  7;    /*--- 17:23 ---*/
        volatile unsigned int DSP1_PFL0_INT:1;  /*--- 16 R/W Programmable Flag-0 Interrupt from DSP1 to CPE ---*/
        volatile unsigned int Reserved2:  7;    /*--- 9:15 ---*/
        volatile unsigned int DSP0_PFL1_INT:1;  /*--- 8 R/W Programmable Flag-1 Interrupt from DSP0 to CPE ---*/
        volatile unsigned int Reserved3:  7;    /*--- 1:7 ---*/
        volatile unsigned int DSP0_PFL0_INT:1;  /*--- 0 R/W Programmable Flag-0 Interrupt from DSP0 to CPE ---*/
    } Bits;
    volatile unsigned int Reg;
};


/*--------------------------------------------------------------------------------*\
 * ARBITER BUS CONTROL
\*--------------------------------------------------------------------------------*/
#define VX180_ARBITER_ARB_3_BASE    0x19130000
#define VX180_ARBITER_ARB_A_BASE    0x19131000
#define VX180_ARBITER_ARB_B_BASE    0x19132000
#define VX180_ARBITER_ARB_C_BASE    0x19133000


#define VX180_ARBITER_SBUS_ERROR_ADDR_BASE(BUS_BASE)       KSEG1ADDR(0x08 + (BUS_BASE))
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_arbiter_sbus_error_addr_register {
    struct _vx180_arbiter_sbus_error_addr_register {
        volatile unsigned int SBUS_ERR_ADDR : 32;    /*--- 31:0 R/W SBUS bus error address ---*/
    } Bits;
    volatile unsigned int Reg;
};



#define VX180_ARBITER_INTR_ENBL_BASE(BUS_BASE)       KSEG1ADDR(0x0C + (BUS_BASE))
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_arbiter_intr_enbl_register {
    struct _vx180_arbiter_intr_enbl_register {
        volatile unsigned int Reserved      : 31;    /*--- 31:1 ---*/
        volatile unsigned int ERR_INTR_ENBL :  1;    /*--- 0 R/W Error response interrupt enable ---*/
    } Bits;
    volatile unsigned int Reg;
};



#define VX180_ARBITER_ERROR_MASTER_BASE(BUS_BASE)       KSEG1ADDR(0x10 + (BUS_BASE))
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_arbiter_error_master_register {
    struct _vx180_arbiter_error_master_register {
        volatile unsigned int Reserved          : 25;    /*--- 31:7 ---*/
        volatile unsigned int PCI_RETRY_TIMEOUT :  1;    /*--- 6   R PCI retry timeout ---*/
        volatile unsigned int ARB_TIMEOUT       :  1;    /*--- 5   R Arbiter timeout ---*/
        volatile unsigned int SBUS_ERR_MSTR     :  5;    /*--- 4:0 R Master Number of the Master that generates an error address on the Data Bus A. A Write to Error Address Register clears this register also. ---*/
    } Bits;
    volatile unsigned int Reg;
};



#define VX180_ARBITER_TIMEOUT_COUNTER_BASE(BUS_BASE)       KSEG1ADDR(0x1C + (BUS_BASE))
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_arbiter_timeout_counter_register {
    struct _vx180_arbiter_timeout_counter_register {
        volatile unsigned int Reserved  : 16;    /*--- 31:16---*/
        volatile unsigned int TO_CNT    : 16;    /*--- 15:0 RW Timeout counter ---*/
    } Bits;
    volatile unsigned int Reg;
};



#define VX180_ARBITER_CONTROL_BASE(BUS_BASE)               KSEG1ADDR(0x20 + (BUS_BASE))
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_arbiter_control_register {
    struct _vx180_arbiter_control_register {
        volatile unsigned int NO_OF_GR2_GRANTS  : 16;    /*--- 31:16 W Number of group 2 grants ---*/
        volatile unsigned int ARB_SAMPLE_TIME   : 16;    /*--- 15:0  W Arbitration sample time ---*/
    } Bits;
    volatile unsigned int Reg;
};



#define VX180_ARBITER_PERFORMANCE_COUNTER_BASE(BUS_BASE)       KSEG1ADDR(0x28 + (BUS_BASE))
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_arbiter_performance_counter_register {
    struct _vx180_arbiter_performance_counter_register {
        volatile unsigned int PERF_COUNT_TIME   : 32;    /*--- 31:0 RW Time window for counting the number of IDLE slots on the bus ---*/
    } Bits;
    volatile unsigned int Reg;
};



#define VX180_ARBITER_PERFORMANCE_IDLE_SLOTS_BASE(BUS_BASE)       KSEG1ADDR(0x2C + (BUS_BASE))
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_arbiter_performance_idle_slots_register {
    struct _vx180_arbiter_performance_idle_slots_register {
        volatile unsigned int PERF_IDLE_SLOTS   : 32;    /*--- 31:0 R Number of IDLE slots on the bus for a specific window of time. ---*/
    } Bits;
    volatile unsigned int Reg;
};



#define VX180_ARBITER_ARB_3_IDMA1_ERROR_STATUS_BASE       KSEG1ADDR(0x14 + VX180_ARBITER_ARB_3_BASE )
#define VX180_ARBITER_ARB_3_IDMA2_ERROR_STATUS_BASE       KSEG1ADDR(0x30 + VX180_ARBITER_ARB_3_BASE )
#define VX180_ARBITER_ARB_3_IDMA3_ERROR_STATUS_BASE       KSEG1ADDR(0x3C + VX180_ARBITER_ARB_3_BASE )
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_arbiter_idma_error_status_register {
    struct _vx180_arbiter_idma_error_status_register {
        volatile unsigned int Reserved0         : 15;    /*--- 31:17 ---*/
        volatile unsigned int IDMA_DM_SIZE_ERR  :  1;    /*--- 16   R IDMA data memory size error ---*/
        volatile unsigned int Reserved1         :  7;    /*--- 15:9 ---*/
        volatile unsigned int IDMA_PM_SIZE_ERR  :  1;    /*--- 8    R IDMA program memory size error ---*/
        volatile unsigned int Reserved2         :  7;    /*--- 7:1 ---*/
        volatile unsigned int IACK_ERR          :  1;    /*--- 0    R Acknowledge error ---*/
    } Bits;
    volatile unsigned int Reg;
};



#define VX180_ARBITER_ARB_3_IDMA1_ERROR_ADDRESS_BASE       KSEG1ADDR(0x18 + VX180_ARBITER_ARB_3_BASE)
#define VX180_ARBITER_ARB_3_IDMA2_ERROR_ADDRESS_BASE       KSEG1ADDR(0x34 + VX180_ARBITER_ARB_3_BASE)
#define VX180_ARBITER_ARB_3_IDMA3_ERROR_ADDRESS_BASE       KSEG1ADDR(0x40 + VX180_ARBITER_ARB_3_BASE)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_arbiter_idma_error_address_register {
    struct _vx180_arbiter_idma_error_address_register {
        volatile unsigned int IDMA1_ERR_ADDR    : 32;    /*--- 31:0 R IDMA error address ---*/
    } Bits;
    volatile unsigned int Reg;
};



#define VX180_ARBITER_ARB_3_FIXED_PRIORITY_BASE           KSEG1ADDR(0x24 + VX180_ARBITER_ARB_3_BASE)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union __vx180_arbiter_fixed_priority_register {
    struct _vx180_arbiter_fixed_priority_register {
        volatile unsigned int MASTER4   : 8;    /*--- 31:24 RW Master 4 Priority ---*/
        volatile unsigned int MASTER3   : 8;    /*--- 23:16 RW Master 3 Priority ---*/
        volatile unsigned int MASTER2   : 8;    /*--- 15:8  RW Master 2 Priority ---*/
        volatile unsigned int MASTER1   : 8;    /*--- 7:0   RW Master 1 Priority (High Priority in Group 2) ---*/
    } Bits;
    volatile unsigned int Reg;
};



/*--------------------------------------------------------------------------------*\
 * PCI BUS CONTROL
\*--------------------------------------------------------------------------------*/
#define VX180_PCI_BASE    0x19170000

#define VX180_PCI_ERROR_BASE                KSEG1ADDR(0x1C + VX180_PCI_BASE)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
union __vx180_pci_error_register {
    struct _vx180_pci_error_register {
        volatile unsigned int Reserved          : 30; /*--- 31:2 R   Reserved ---*/
        volatile unsigned int PCI_ERROR_MESSAGE : 2;  /*--- 1:0  RW  Error Message indicating that an error condition occurred on the PCI bus. 00: No Error; 01: Last PCI transaction had fatal error; 10: Last PCI transaction had parity error; 11: Reserved. These bits are write 1 to clear (W1C). ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_PCI_ERROR_ADDR_BASE           KSEG1ADDR(0x20 + VX180_PCI_BASE)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
union __vx180_pci_error_addr_register {
    struct _vx180_pci_error_addr_register {
        volatile unsigned int PCI_ERROR_ADDR    : 32; /*--- 31:0 RW  For an error signaled by the bridge, the system looks up the start address of the last PCI transaction with the error. ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_PCI_AHB_ERROR_BASE            KSEG1ADDR(0x24 + VX180_PCI_BASE)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
union __vx180_pci_ahb_error_register {
    struct _vx180_pci_ahb_error_register {
        volatile unsigned int Reserved          : 31; /*--- 31:1 R   Reserved ---*/
        volatile unsigned int AHB_ERROR_MESSAGE : 1;  /*--- 0    RW  Error Message indicating that an error condition occurred on PCI. NOTE: AHB Master receives an error response on AHB side. These bits are write 1 to clear (W1C). ---*/
    } Bits;
    volatile unsigned int Reg;
};

#define VX180_PCI_AHB_ERROR_ADDR_BASE       KSEG1ADDR(0x28 + VX180_PCI_BASE)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
union __vx180_pci_ahb_error_addr_register {
    struct _vx180_pci_ahb_error_addr_register {
        volatile unsigned int AHB_ERROR_ADDR    : 32; /*--- 31:0 RW  For an error signaled by the bridge, the system looks up the start address of the last AHB transaction with the error. ---*/
    } Bits;
    volatile unsigned int Reg;
};



#define VX180_PCI_CONTROL2_BASE             KSEG1ADDR(0x38 + VX180_PCI_BASE)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
union __vx180_pci_control2_register {
    struct _vx180_pci_control2_register {
        volatile unsigned int Reserved0       : 2 ; /*--- 31:30 R   Reserved ---*/
        volatile unsigned int CIS             : 18; /*--- 29:12 R/W These 18 bits form the CIS PTR bits [17:0] in the PCI configuration space. These bits indicate the MBAR that is used to access CIS and address offset. Refer to the PCI configuration space in PCI Local Bus Specification Version 2.2. ---*/
        volatile unsigned int IPC_INT_EN      : 1 ; /*--- 11    R/W Interrupt from IPC to INTA line enable ---*/
        volatile unsigned int INTA_TO_PCI     : 1 ; /*--- 10    R/W INTA signal assertion to PCI ---*/
        volatile unsigned int BDG_IRQ_EN      : 1 ; /*--- 9     R/W Internal bridge interrupt enable for interrupts to IPC ---*/
        volatile unsigned int SERR_EN         : 1 ; /*--- 8     R/W SERR interrupt enable for interrupts to IPC ---*/
        volatile unsigned int PME_TO_IPC_EN   : 1 ; /*--- 7     R/W PME interrupt enable ---*/
        volatile unsigned int Reserved1       : 2 ; /*--- 6:5   R/W Reserved ---*/
        volatile unsigned int INTA_EN         : 1 ; /*--- 4     R/W PCI_INTA interrupt enable for interrupts to IPC ---*/
        volatile unsigned int OUTB_END_EN     : 1 ; /*--- 3     R/W Endian conversion enable for outbound transactions. No Endian or all byte lane swap. ---*/
        volatile unsigned int Reserved2       : 1 ; /*--- 2     R/W Reserved ---*/
        volatile unsigned int INB_ADR_TR_EN   : 1 ; /*--- 1     R/W Inbound address translation enable. The address that is received is replaced with the register values of the inbound transaction address translation. ---*/
        volatile unsigned int PCI_EN          : 1 ; /*--- 0     R/W PCI enable. Until this bit is set, the PCI module continues retrying inbound PCI transactions. ---*/
    } Bits;
    volatile unsigned int Reg;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define VX180_PCI_STATUS_BASE               KSEG1ADDR(0x3C + VX180_PCI_BASE)
union __vx180_pci_status_register {
    struct _vx180_pci_status_register {
        volatile unsigned int Reserved0       : 14; /*--- 31:18   Reserved ---*/
        volatile unsigned int MXMT_FIFO_FULL  : 1 ; /*--- 17    R Indicates FIFO is FULL, for outbound transaction of PCI Master during PCI Write ---*/
        volatile unsigned int MXMT_FIFO_EMPTY : 1 ; /*--- 16    R Indicates FIFO is EMPTY, for outbound transaction of PCI Master during PCI Write ---*/
        volatile unsigned int MRCV_FIFO_EMPTY : 1 ; /*--- 15    R Indicates FIFO is EMPTY, for outbound transaction of PCI Master during PCI Read ---*/
        volatile unsigned int TXMT_FIFO_FULL  : 1 ; /*--- 14    R Indicates FIFO is FULL, for outbound transaction of PCI Target during Memory Read ---*/
        volatile unsigned int TXMT_FIFO_EMPTY : 1 ; /*--- 13    R Indicates FIFO is EMPTY for outbound transaction of the PCI Target during Memory Read ---*/
        volatile unsigned int TRCV_FIFO_EMPTY : 1 ; /*--- 12    R Indicates FIFO is EMPTY for inbound transaction of PCI Target during Memory Write ---*/
        volatile unsigned int Reserved1       : 2 ; /*--- 11:10   Reserved ---*/
        volatile unsigned int PCI_BDG_INTR    : 1 ; /*--- 9     R Indicates the internal interrupt generated for PERR, error conditions etc. ---*/
        volatile unsigned int PCI_SERR_STAT   : 1 ; /*--- 8     R Indicates the SERR interrupt ---*/
        volatile unsigned int PCI_PME_STAT    : 1 ; /*--- 7     R PME status indication. ---*/
        volatile unsigned int Reserved2       : 2 ; /*--- 6:5     Reserved ---*/
        volatile unsigned int PCI_INTA_STAT   : 1 ; /*--- 4     R Indicates the state of PCI_INTA input signal ---*/
        volatile unsigned int Reserved3       : 2 ; /*--- 3:2     Reserved ---*/
        volatile unsigned int PCI_INT_ARB_SEL : 1 ; /*--- 1     R Indicates the enabled PCI internal arbiter. ---*/
        volatile unsigned int HOST_DEV_MODE   : 1 ; /*--- 0     R Indicates the following chip modes: PCI Host (central resource) mode, or PCI Device Mode ---*/
    } Bits;
    volatile unsigned int Reg;
};



/*--------------------------------------------------------------------------------*\
 * Interrupt-Numbers
\*--------------------------------------------------------------------------------*/
#define INT_DMA_CPE          0         /*--- CPE interrupt from DMA ---*/
#define INT_CBUS_ARB         1         /*--- Control Bus Arbiter Interrupt ---*/
#define INT_SCU              2         /*--- System Control Unit Interrupt ---*/
#define INT_MIPS_TIMER MIPS  3         /*--- Timer Request Interrupt ---*/
#define INT_GPIO2            4         /*--- General purpose I/O 16-31 Flags ---*/
#define INT_DBUSA_ARB        5         /*--- Interrupt from Data Bus-A Arbiter ---*/
#define INT_UART1            6         /*--- Interrupt from UART1 ---*/
#define INT_SPI              7         /*--- SPI Interrupt (part of PHER_G) ---*/
#define INT_GPIO             8         /*--- General purpose I/O 0-15 Flags ---*/
#define INT_GPT0             9         /*--- Interrupt-0 from Timer ---*/
#define INT_GPT1            10         /*--- Interrupt-1 from Timer ---*/
#define INT_GPT2            11         /*--- Interrupt-2 from Timer ---*/
#define INT_DBUSB_ARB       12         /*--- Data Bus-B Arbiter Interrupt ---*/
#define INT_GIGE2           13         /*--- Interrupt from GIGE-2 ---*/
#define INT_GIGE1           14         /*--- Interrupt from GIGE-1 ---*/
#define INT_VDSP_IDMA2      15         /*--- VDSP IDMA-2 Interrupt ---*/
#define INT_VDSP_IDMA1      16         /*--- VDSP IDMA-1 Interrupt ---*/
#define INT_FL0_DSP0        17         /*--- Flag-0 of DSP1 ---*/
#define INT_FL1_DSP0        18         /*--- Flag-1 of DSP1 ---*/
#define INT_PFL0_DSP0       19         /*--- Programmable Flag-0 of DSP1 ---*/
#define INT_PFL1_DSP0       20         /*--- Programmable Flag-1 of DSP1 ---*/
#define INT_FL0_DSP1        21         /*--- Flag-0 of DSP2 ---*/
#define INT_FL1_DSP1        22         /*--- Flag-1 of DSP2 ---*/
#define INT_PFL0_DSP1       23         /*--- Programmable Flag-0 of DSP2 ---*/
#define INT_PFL1_DSP1       24         /*--- Programmable Flag-1 of DSP2 ---*/
#define INT_PCI             25         /*--- Interrupt from PCI ---*/
#define INT_VDSL_PHY        26         /*--- (hic_int) Interrupt from VPHY (hic_int) ---*/
#define INT_DBUSC_ARB       27         /*--- Data Bus-C Arbiter ---*/
#define INT_SPA             28         /*--- Security Processor AP Interrupt ---*/
#define INT_UART2           29         /*--- Interrupt from UART2 ---*/
#define INT_WAP_GIGE_AP     30         /*--- Wireless LAN AP Interrupt ---*/
#define INT_BMU_GIGE_AP     31         /*--- Buffer Management Unit AP Interrupt ---*/
#define INT_DMA_AP          32         /*--- AP interrupt from DMA ---*/
#define INT_MIPS_SW0        33         /*--- MIPS Software Interrupt ? 0 ---*/
#define INT_MIPS_SW1        34         /*--- MIPS Software Interrupt ? 1 ---*/
#define INT_USBH_EHCI       35         /*--- Interrupt from EHCI Interface of USB 2.0 Host ---*/
#define INT_VDSL_AP         36         /*--- Interrupt from VDSL AP ---*/
#define INT_MIPS_PC         37         /*--- MIPS Performance Counter Interrupt---*/

#endif /*--- #ifndef _asm_mips_mach_ikan_mips_vx180_h_ ---*/
