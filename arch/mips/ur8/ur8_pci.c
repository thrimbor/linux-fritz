#ifdef CONFIG_PCI_DEBUG
	#define DbgPrint  printk 
#else
	#define DbgPrint(x...) 
#endif
/*
*	This is a file specific for yamuna soc. other soc's MAY work with this or may need modification
*	what this means is that CONFIG_AVALANCHE_MIPS_PCI is assumed defined, and hence the EVM_III related
*	code has been removed
*/
/* 
*	the kernel include files for definitions 
*/
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/kernel_stat.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/pci.h>
#include <asm/cacheflush.h>

#include <hw_pci.h>
#include <ur8.h>
#include <ur8_vlynq.h>
#include <ur8int.h>



/*
* this routine is called to map the irq that is to be used for this device
*/
#define INTX     	(0xFF)
#define INTA		(UR8_PCI_INTA_SWIRQ)
#define INTB		(UR8_PCI_INTA_SWIRQ + 1)
#define INTC		(UR8_PCI_INTA_SWIRQ + 2)
#define INTD 		(UR8_PCI_INTA_SWIRQ + 3)

#define MAX_SLOTS 	3	

#define INT_DEV1		(INTA + CONFIG_AVM_PCI_DEV1_INT)
#define INT_DEV2		(INTA + CONFIG_AVM_PCI_DEV2_INT)
#define INT_DEV3		(INTA + CONFIG_AVM_PCI_DEV3_INT)

static unsigned char irq_tab_ur8[][5] = {
#ifdef CONFIG_UR8_EVM
	[6]={-1,INTA,INTB,INTC,INTD},
#else
	/* only first irq line of PCI device is used on AVM boards*/
#	if CONFIG_AVM_PCI_DEVICE_COUNT > 0
	[CONFIG_AVM_PCI_DEV1_SLOT] = {-1,INT_DEV1,INTX,INTX,INTX},

#	if CONFIG_AVM_PCI_DEVICE_COUNT > 1
	[CONFIG_AVM_PCI_DEV2_SLOT] = {-1,INT_DEV2,INTX,INTX,INTX},

#	if CONFIG_AVM_PCI_DEVICE_COUNT > 2
	[CONFIG_AVM_PCI_DEV3_SLOT] = {-1,INT_DEV3,INTX,INTX,INTX},

#	endif // > 2
#	endif // > 1
#	endif // > 0
#endif
};
 
int pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	/* the slot to interrupt mapping depends on how the inta/b/c/d lines are
	* connected in hardware. the physical first slot shows up at 0x20000 address
	* physical second slot will show up at 0x40000 and so on.
	* for each slot int a/b/c/d lines can be rotated, so that the card
	* actually asserts intA, but on the cpu it shows up as intB /C /D depending on
	* how the board connectors are wired.
	*/

	DbgPrint (KERN_ERR " map_irq PCI %u %u -> %u\n", slot, pin,irq_tab_ur8[slot][pin]);
	return irq_tab_ur8[slot][pin];	

}

/* 
*  Do platform specific device initialization at pci_enable_device() time, when a driver is claiming the device
*  at the moment we do nothing. later on really speaking, we could use this to update the master's memory
*  space for vbus responding. since the bridge responds to all the space anyway, we let it go.
*  this needs to be NOT static
*  at the moment we just see what irq this device has, just to be sure.
*/
int pcibios_plat_dev_init(struct pci_dev *dev)
{
	u8 pin = 0;
	
	pci_read_config_byte( dev, 0x3f, &pin );
	DbgPrint( "Platform Dev Init Time IRQ = %d\n", pin );
	
	return 0;
}
/* #endif *//* CONFIG_PCI */

