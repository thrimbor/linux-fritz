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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <asm/bootinfo.h>

#ifdef CONFIG_FUSIV_VX160
#include <asm/mach-adi_fusiv/adi6843.h>
#include <asm/mach-adi_fusiv/fusivpci.h>
#endif

#ifdef CONFIG_FUSIV_VX180
#include <ikan6850.h>
#include <fusivpci.h>
#ifdef CONFIG_ATM
#if !defined(CONFIG_FUSIV_KERNEL_VX180_AVDSL_MODULE) && (defined(CONFIG_FUSIV_KERNEL_AP_2_AP) || defined(CONFIG_FUSIV_KERNEL_AP_2_AP_MODULE))
#define VX180A
#endif
#endif
/* for all TARGETS other than Vx180 & its derivatives */
#else
#include <adi6843.h>
#include <fusivpci.h>
#endif

#define PCI_ACCESS_READ  0
#define PCI_ACCESS_WRITE 1
#define PCI_MAX_SLOTS 	4

extern int board_memsize;
static spinlock_t fusiv_pci_lock;

static unsigned int first_pci_pin = 21;

static int __init setpci_pin(char *str)
{
	get_option(&str, &first_pci_pin);
	return 1;
}


#ifdef VX180A

/* Vx180A Modifications*/
#define GPIO_DIR_REG1             *(volatile unsigned short *)0xb9040000
#define GPIO_FLAG_SET_REG1        *(volatile unsigned short *)0xb9040006
#define GPIO_FLAG_CLEAR_REG1      *(volatile unsigned short *)0xb9040004

#define GPIO_DIR_REG2             *(volatile unsigned short *)0xb90b0000
#define GPIO_FLAG_SET_REG2        *(volatile unsigned short *)0xb90b0006
#define GPIO_FLAG_CLEAR_REG2      *(volatile unsigned short *)0xb90b0004

#define GPIO_VX180A_SYNC_PIN      8

__inline void  vx180AGpioDirFlag( unsigned short pin)
{
    if(pin < 16)
    {
        GPIO_DIR_REG1 |= (0x1 << pin);
    }
    else
        GPIO_DIR_REG2 |= (0x1 << (pin -16));
}
__inline void  vx180AGpioSetFlag( unsigned short pin)
{
    if(pin < 16)
    {
        GPIO_FLAG_SET_REG1 = (0x1 << pin);
    }
    else
        GPIO_FLAG_SET_REG2 = (0x1 << (pin -16));
}
__inline void  vx180AGpioClearFlag( unsigned short pin)
{
    if(pin < 16)
    {
        GPIO_FLAG_CLEAR_REG1 = (0x1 << pin);
    }
    else
        GPIO_FLAG_CLEAR_REG2 = (0x1 << (pin -16));
}

#endif

static int fusiv_pcibios_config_access(unsigned char access_type,
	struct pci_bus *bus, unsigned int devfn, int where, u32 * val)
{
	unsigned char busnum = bus->number;
	u32 err;
//	printk("bus %d  slot %d fun %d where %x \n",busnum,PCI_SLOT(devfn),PCI_FUNC(devfn),where);
	if (busnum)	return PCIBIOS_DEVICE_NOT_FOUND;
	if (PCI_SLOT(devfn) > PCI_MAX_SLOTS ){
		/* The addressing scheme chosen leaves room for just
		 * 4 devices on the first busnum (besides the PCI
		 * controller itself) */
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (devfn == PCI_DEVFN(0, 0)) {
		/* Access controller registers directly */
		if (access_type == PCI_ACCESS_WRITE) {
		} else {
		*val = 0;
		}
		return PCIBIOS_SUCCESSFUL;
	}

	/* Clear PCI Error register. This also clears the Error Type
	 * bits in the Control register */
	//PCI_ERR_REG = 0xffffffff;

	/* Setup address */
	PCI_NPC_ADDR = (1 << (PCI_SLOT(devfn)+ first_pci_pin-1)) | (where & ~3) | (PCI_FUNC(devfn)<< 8);
	
	if (access_type == PCI_ACCESS_WRITE){
		PCI_NPC_CTL = PCI_NPC_VAL (PCI_CMD_CFG_WR, 0);
		PCI_NPC_WDAT = *val;
	}
	else{
		PCI_NPC_CTL = PCI_NPC_VAL (PCI_CMD_CFG_RD, 0);
		*val = PCI_NPC_RDAT;
	}

	/* Check for master or target abort */
	err = PCI_ERR_REG & 0x103;
	
	if (err){
		PCI_ERR_REG = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	return PCIBIOS_SUCCESSFUL;
}

static int fusiv_pcibios_read(struct pci_bus *bus, unsigned int devfn,
	int where, int size, u32 * val)
{
	unsigned long flags;
	u32 data = 0;
	int err;
#if  CONFIG_FUSIV_KERNEL_WLAN_ATH_11G || CONFIG_FUSIV_KERNEL_WLAN_ATH_11N
        int read_count = 0;
        const int MAX_READ_COUNT = 3;
#endif

	if ((size == 2) && (where & 1))
		return PCIBIOS_BAD_REGISTER_NUMBER;
	else if ((size == 4) && (where & 3))
		return PCIBIOS_BAD_REGISTER_NUMBER;

#if  CONFIG_FUSIV_KERNEL_WLAN_ATH_11G || CONFIG_FUSIV_KERNEL_WLAN_ATH_11N
        /*
         * There is a problem in bring up Atheros 11g MB55 miniPCI card
         * on Ikanos Vox160/180 platform. The PCI controller access to PCI_VENDOR_ID
         * configure register on MB55, then MB55 replies with "retry" command and
         * retrieves the configure data from EEPROM. Before the MB55 have all data
         * ready, the controller enters some state and stop trying.
         * This code re-read the configure register.
         */
#define PCI_INVALID_ID(data) \
        ((data) == 0xffffffff || (data) == 0x00000000 || (data) == 0x0000ffff || (data) == 0xffff0000)

        do {
                spin_lock_irqsave(&fusiv_pci_lock, flags);
                err = fusiv_pcibios_config_access(PCI_ACCESS_READ, bus, devfn, \
                          where, &data);
                spin_unlock_irqrestore(&fusiv_pci_lock, flags);

                if ( (where != PCI_VENDOR_ID) || err) {
                        break;
                }
                mdelay (100);
         } while ( (read_count++ <= MAX_READ_COUNT) && PCI_INVALID_ID(data) );
#else
	spin_lock_irqsave(&fusiv_pci_lock, flags);
	err = fusiv_pcibios_config_access(PCI_ACCESS_READ, bus, devfn, where,
					&data);
	spin_unlock_irqrestore(&fusiv_pci_lock, flags);
#endif

	if (err)
		return err;

	if (size == 1)
		*val = (data >> ((where & 3) << 3)) & 0xff;
	else if (size == 2)
		*val = (data >> ((where & 3) << 3)) & 0xffff;
	else
		*val = data;

	return PCIBIOS_SUCCESSFUL;
}

static int fusiv_pcibios_write(struct pci_bus *bus, unsigned int devfn,
	int where, int size, u32 val)
{
	unsigned long flags;
	u32 data = 0;
	int err;

	if ((size == 2) && (where & 1))
		return PCIBIOS_BAD_REGISTER_NUMBER;
	else if ((size == 4) && (where & 3))
		return PCIBIOS_BAD_REGISTER_NUMBER;

	spin_lock_irqsave(&fusiv_pci_lock, flags);
	err = fusiv_pcibios_config_access(PCI_ACCESS_READ, bus, devfn, where,
	                                  &data);
	spin_unlock_irqrestore(&fusiv_pci_lock, flags);

	if (err)
		return err;

	if (size == 1)
		data = (data & ~(0xff << ((where & 3) << 3))) |
		    (val << ((where & 3) << 3));
	else if (size == 2)
		data = (data & ~(0xffff << ((where & 3) << 3))) |
		    (val << ((where & 3) << 3));
	else
		data = val;

	if (fusiv_pcibios_config_access
	    (PCI_ACCESS_WRITE, bus, devfn, where, &data))
        {
	     return -1;
        }

	return PCIBIOS_SUCCESSFUL;
}

unsigned char fusiv_inb(u16 port)
{
	int tmp;
	PCI_NPC_ADDR = port; 
	PCI_NPC_CTL = 0x12;
	tmp = PCI_NPC_RDAT;
	printk("inport %x , data %x\n",port,tmp);
	return (tmp&0xff);
}

void fusiv_outb(unsigned val, u16 port)
{
	
	PCI_NPC_ADDR = port; 
	PCI_NPC_CTL = 0x13;
	PCI_NPC_WDAT =val;
}

int fusiv_inw(u16 port)
{
	int tmp,tmp1;
	PCI_NPC_ADDR = port; 

	if (port & 2)
		tmp1 = 0x32;
	else
		tmp1 = 0xc2;
		
	PCI_NPC_CTL = tmp1;
	tmp = PCI_NPC_RDAT;
	if (port & 2)
		return tmp >>16;
	return (tmp&0xffff);
}
void fusiv_outw(u16 val, u16 port)
{
	
	PCI_NPC_ADDR = port; 
	
	if(port & 2) {
	       PCI_NPC_CTL = 0x33;
	       PCI_NPC_WDAT =val<< 16;
	}
	else {
		PCI_NPC_CTL = 0xc3;
		PCI_NPC_WDAT =val;
	}
}

void fusiv_outl(int val, u16 port)
{
	PCI_NPC_ADDR = port; 
	
       PCI_NPC_CTL = PCI_NPC_VAL (PCI_CMD_IO_WR,0);

	 PCI_NPC_WDAT = (val);
	 
}

struct pci_ops fusiv_pci_ops = {
	.read = fusiv_pcibios_read,
	.write = fusiv_pcibios_write,
};

static struct resource fusiv_pci_mem_resource = {
	.name	= "FUSIV PCI MEM",
	.start	= PCI_MEM_BASE,
	.end		= PCI_MEM_END,
	.flags	= IORESOURCE_MEM,
};

static struct resource fusiv_pci_io_resource = {
	.name	= "FUSIV PCI IO",
	.start	= 0x00000100,
	.end		= 0xFFFF,
	.flags	= IORESOURCE_IO,
};

static struct pci_controller fusiv_pci_controller = {
	.mem_resource	= &fusiv_pci_mem_resource,
	.io_resource	= &fusiv_pci_io_resource,
	.pci_ops		= &fusiv_pci_ops,
};

int __init fusiv_pci_setup(void)
{
 	printk("Fusiv PCI: starting...\n");

#ifdef VX180A
        /* As we are booting AT200 from EEPROM SPI interface of AT200 is master
         , So SPI is configured in slave mode for Vx180  */
        // Configure SPI in slave mode
        (*(unsigned int*)(0xb9030000)) &= ~(1<<12);

        /* As AT200 is booting from EEPROM ,and boots before PCI initialization
           of VX180A , and during PCI initialization PCI gets reset which
           caused PCI of AT200 to reset and clears the values on AT200. Through
           GPIO synchronization is handled to start boot on AT200 */

        /* Vx180A syncup */
        vx180AGpioDirFlag(GPIO_VX180A_SYNC_PIN);
#endif

	/* reset PCI controller */
	PCI_RST_CONTROL = 0x1;
	mdelay (1);
	PCI_RST_CONTROL = 0x3;
	mdelay (5);
#ifdef VX180A
        /* VX180A syncup */
        vx180AGpioClearFlag(GPIO_VX180A_SYNC_PIN);
#endif
	PCI_RST_CONTROL = 0x1;

#ifdef VX180A
        /* VX180A syncup */
        vx180AGpioSetFlag(GPIO_VX180A_SYNC_PIN);
#endif

#ifdef VX180A
        mdelay(500);
#else
	mdelay (5);
#endif

	  // Load items used to setup our PCI configuration space
	  // Change the address for PCI_ID, PCI_REV etc., -- VJ
	  PCI_ID = (PCI_OUR_DEV_ID << 16) | PCI_OUR_VENDOR_ID;
	  PCI_REV = (PCI_OUR_CLASS << 8) | PCI_OUR_REV;
	  PCI_SVID = (PCI_OUR_SS_DEV_ID << 16) | PCI_OUR_SS_VENDOR_ID;
	  /*== AVM/SKI 20081029 ==
	   * Enable HW support for endianess swap.
	   * */
#if 1
	  PCI_CONTROL =  PCI_EN | INTA_EN|INB_ADDR_TR_EN;
#else
	  PCI_CONTROL =  PCI_EN | INTA_EN|INB_ADDR_TR_EN |OUTB_END_EN;			//Leo, this and this only
#endif


	  PCI_MBAR_CONF = 0x11;				// standard 
#ifdef VX180A
        /* VX180A Mask and Trans */
        if(board_memsize == 32)
            PCI_IMBAR1_MASK = 0xfe000000; // Inbound memory bar1 mask reg 32M 
        else if(board_memsize == 64)
            PCI_IMBAR1_MASK = 0xfc000000; // Inbound memory bar1 mask reg 64M 
        else if(board_memsize == 128)
            PCI_IMBAR1_MASK = 0xf8000000; // Inbound memory bar1 mask reg 128M 

        PCI_IMEM1_TRANS = 0x00000000; // Inbound memory1 trans reg SDRAM Address

       /* VX180A Mask and Trans */
        PCI_IMBAR2_MASK = 0xffc00000; // Inbound memory bar1 mask reg
        PCI_IMEM2_TRANS = 0x19000000; // Inbound memory2 trans reg for SCU reg
#else
	  PCI_IMBAR1_MASK = 0xf0000000;	// Inbound memory bar1 mask reg 16 M
	  PCI_IMEM1_TRANS = 0x00000000;	// Inbound memory1 trans reg SDRAM Address 
	  PCI_IMBAR2_MASK = 0xff000000;	// Inbound memory bar1 mask reg
	  PCI_IMEM2_TRANS = 0x19000000;	// Inbound memory2 trans reg SDRAM Address 
#endif

	  // Setup the PCI control register to allow us to be a bus master. Don't
	  // enable memory or I/O yet as the chip selects need to be setup.
	  //
	  // Also clear all bits set in status register
	  PCI_CRP_CTL = PCI_CRP_VAL (PCI_COMMAND, PCI_CMD_CFG_WR, 0);
	  PCI_CRP_WDAT = PCI_COMMAND_SERR
	    | PCI_COMMAND_PARITY
	    | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | (0xffff << 16);

#ifdef VX180A
      /*  Vx180A Modifications */
      /* Programming Host 1st MBAR's */
          PCI_CRP_CTL =  0x000b0010;
//          PCI_CRP_WDAT = 0x10000000;
          PCI_CRP_WDAT = 0x00000000;

       /* Programming Host 2nd MBAR's */
          PCI_CRP_CTL =  0x000b0014;
          PCI_CRP_WDAT = 0x20000000;
#endif

	  PCI_MBAP1 = PCI_MEM_BASE;	

#ifdef VX180A
          PCI_MBAP2 = PCI_MEM_BASE + 0x00200000;
          PCI_MBAP3 = PCI_MEM_BASE + 0x00400000;
          PCI_MBAP4 = PCI_MEM_BASE + 0x00600000;
          PCI_MBAP5 = PCI_MEM_BASE + 0x00800000;
          PCI_MBAP6 = PCI_MEM_BASE + 0x00a00000;
          PCI_MBAP7 = PCI_MEM_BASE + 0x00c00000;
          PCI_MBAP8 = PCI_MEM_BASE + 0x00e00000;
          PCI_MBAP9 = PCI_MEM_BASE + 0x01000000;
          PCI_MBAP10 = PCI_MEM_BASE + 0x01200000;
          PCI_MBAP11 = PCI_MEM_BASE + 0x01400000;
          PCI_MBAP12 = PCI_MEM_BASE + 0x01600000;
          PCI_MBAP13 = PCI_MEM_BASE + 0x01800000;
          PCI_MBAP14 = PCI_MEM_BASE + 0x01a00000;
          PCI_MBAP15 = PCI_MEM_BASE + 0x01c00000;
          PCI_MBAP16 = PCI_MEM_BASE + 0x01e00000;
#else
          PCI_MBAP2 = PCI_MEM_BASE + 0x01000000;
          PCI_MBAP3 = PCI_MEM_BASE + 0x02000000;
          PCI_MBAP4 = PCI_MEM_BASE + 0x03000000;
#endif

          /* Below value in PCI Target control register is set by default in
          hardware for VOX160 and VOX200 platforms. Hence the value setting
          is required only for vox150 */
          if((*(unsigned int*)0xb900003c) == 0x6833) // vx150
          {
             PCI_TAR_CTL = 0x07ff00b;
          }

	  mdelay (20);

#define VX180_GPIO_MEMORY_BASE             (0x19000000)
#define VX180_GPIO_DIR_NEW_BASE            KSEG1ADDR(0x40024 + VX180_GPIO_MEMORY_BASE)
#define VX180_GPIO_FLAG_NEW_BASE           KSEG1ADDR(0x40028 + VX180_GPIO_MEMORY_BASE)
#define VX180_GPIO_INTREN_NEW_BASE         KSEG1ADDR(0x4002C + VX180_GPIO_MEMORY_BASE)
#define VX180_GPIO_POLAR_NEW_BASE          KSEG1ADDR(0x40034 + VX180_GPIO_MEMORY_BASE)
#define VX180_GPIO_EDGE_NEW_BASE           KSEG1ADDR(0x40038 + VX180_GPIO_MEMORY_BASE)
#define VX180_GPIO_BOTH_NEW_BASE           KSEG1ADDR(0x4003C + VX180_GPIO_MEMORY_BASE)


    *(unsigned short *)VX180_GPIO_INTREN_NEW_BASE = 1;  /*--- mask pci_clkrun interrupt ---*/
    *(unsigned short *)VX180_GPIO_DIR_NEW_BASE    = 1;  /*--- pci_clkrun as output ---*/
    /*--- *(unsigned short *)VX180_GPIO_DIR_NEW_BASE    = 0; ---*/  /*--- pci_clkrun as input ---*/
    *(unsigned short *)VX180_GPIO_POLAR_NEW_BASE  = 1;  /*--- active low ---*/
    *(unsigned short *)VX180_GPIO_EDGE_NEW_BASE   = 0;  /*--- level ---*/
    *(unsigned short *)VX180_GPIO_BOTH_NEW_BASE   = 0;  /*--- no both, no flanke ---*/

    ((unsigned short *)VX180_GPIO_FLAG_NEW_BASE)[0] = 1 | 2;  /*--- clear clk run pin (leave clkmux 1) ---*/
    /*--- ((unsigned short *)VX180_GPIO_FLAG_NEW_BASE)[1] = 1 | 2; ---*/  /*--- set clk run pin (leave clkmux 1) ---*/




	register_pci_controller(&fusiv_pci_controller);
	/* == AVM/SKI 20091112 WLAN Performance == 
	 * Arbiter prioritisation change to improve WLAN performance.
	 * Suggested and tested by Ikanos.
	 */
	*(volatile unsigned int *)0xb9250080 = (*(volatile unsigned int *)0xb9250080 & ~0x0000c000) | 0x00000c0;

        return 0;
}


int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
    PCI_CONTROL |= INTA_EN ;

    switch (slot) {
        case 1:
        case 2:
        case 3:
        case 4:
            return PCI_INT;   /* the only one*/
        default:
            return -1;            /* Illegal */
    }

}

/* Do platform specific device initialization at pci_enable_device() time */
int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}

__setup("idsel=", setpci_pin);
arch_initcall(fusiv_pci_setup);
