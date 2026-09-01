/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */
#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/pm.h>
//#include <linux/time.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/serial_core.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/mips-boards/generic.h>
#include <asm/prom.h>
#include <asm/mach-ur8/ur8.h>
#include <asm/mach-ur8/ur8int.h>
#include <asm/mach_avm.h>
#include <asm/time.h>
#include <linux/delay.h>


#include <asm/reboot.h>
//#include <ur8.h>
//#include <prom.h>
#include <asm/mach-ur8/hw_bbif.h>
#include <asm/mach-ur8/hw_boot.h>
#include <asm/mach-ur8/hw_clock.h>
#include <asm/mach-ur8/hw_emif.h>
#include <asm/mach-ur8/hw_gpio.h>
#include <asm/mach-ur8/hw_i2c.h>
#include <asm/mach-ur8/hw_irq.h>
#include <asm/mach-ur8/hw_mdio.h>
#include <asm/mach-ur8/hw_reset.h>
#include <asm/mach-ur8/hw_stack.h>
#include <asm/mach-ur8/hw_timer.h>
#include <asm/mach-ur8/hw_uart.h>
#include <asm/mach-ur8/hw_usb.h>
#include <asm/mach-ur8/hw_vbus.h>

struct _hw_boot *UR8_hw_boot  = (struct _hw_boot *)UR8_DEVICE_CONFIG_BASE;
struct _hw_clock *UR8_hw_clock  = (struct _hw_clock *)UR8_CLOCK_BASE;
struct EMIF_register_memory_map *UR8_EMIF_register_memory_map = (struct EMIF_register_memory_map *)UR8_EMIF_BASE;
struct _hw_gpio *UR8_hw_gpio  = (struct _hw_gpio *)UR8_GPIO_BASE;
/*--- struct _hw_i2c *UR8_hw_i2c = (struct _hw_i2c *)UR8_I2C_BASE; ---*/
struct _irq_hw *UR8_irq_hw = (struct _irq_hw *)UR8_IRQ_CTRL_BASE;
union _hw_non_reset *UR8_hw_non_reset = (union _hw_non_reset *)UR8_RESET_BASE;
struct _timer_hw *UR8_timer_hw[2] = { (struct _timer_hw  *)UR8_TIMER1_BASE, (struct _timer_hw  *)UR8_TIMER2_BASE };
struct _hw_uart *UR8_hw_uart = (struct _hw_uart *)UR8_UART0_BASE;
struct _usb_hw *UR8_usb_hw = (struct _usb_hw *)UR8_USB_BASE;
/*--- struct _usbdma_hw *UR8_usbdma_hw = (struct _usbdma_hw *)UR8_USBDMA_BASE; ---*/

/*------------------------------------------------------------------------------------------*\
 * USB
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_USB_MUSB_HDRC) || defined(CONFIG_USB_MUSB_HDRC_MODULE)

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/usb/musb.h>

static struct musb_hdrc_eps_bits musb_eps[] = {
	{ "ep1_tx", 8, },
	{ "ep1_rx", 8, },
	{ "ep2_tx", 8, },
	{ "ep2_rx", 8, },
	{ "ep3_tx", 5, },
	{ "ep3_rx", 5, },
	{ "ep4_tx", 5, },
	{ "ep4_rx", 5, },
};

struct musb_hdrc_config usb_config = {
	.multipoint	= true,
	.dyn_fifo	= true,
	.soft_con	= true,
	.dma		= true,

	.num_eps	= 5,
	.dma_channels	= 8,
	.ram_bits	= 10,
	.eps_bits	= musb_eps,
};

static struct musb_hdrc_platform_data usb_data = {
#if     defined(CONFIG_USB_MUSB_OTG)
    /* OTG requires a Mini-AB connector */
    .mode           = MUSB_OTG,
#elif   defined(CONFIG_USB_MUSB_PERIPHERAL)
    .mode           = MUSB_PERIPHERAL,
#elif   defined(CONFIG_USB_MUSB_HOST)
    .mode           = MUSB_HOST,
#endif
    /* irlml6401 switches 5V */
    .power          = 250,          /* sustains 3.0+ Amps (!) */
    .potpgt         = 4,            /* ~8 msec */

    /* REVISIT multipoint is a _chip_ capability; not board specific */
    .config         = &usb_config,
};

static struct resource usb_resources [] = {
    {
        /* physical address */
        .start          = UR8_USB_BASE,
        .end            = UR8_USB_BASE + 0x5ff,
        .flags          = IORESOURCE_MEM,
    },
    {
        .start          = UR8INT_USB,
        .flags          = IORESOURCE_IRQ,
    },
    {
        .start          = UR8INT_USB_DMA,
        .flags          = IORESOURCE_IRQ,
    },
};

static u64 usb_dmamask = DMA_BIT_MASK(32);

static struct platform_device usb_dev = {
    .name           = "musb_hdrc",
    .id             = -1,
    .dev = {
        .platform_data      = &usb_data,
        .dma_mask       = &usb_dmamask,
        .coherent_dma_mask      = DMA_BIT_MASK(32),
    },
    .resource       = usb_resources,
    .num_resources  = ARRAY_SIZE(usb_resources),
};

#if defined(CONFIG_PCI)
extern int  ur8_pci_init(void);
#endif	

static int ur8_musb_init(void) {
    /* REVISIT:  everything except platform_data setup should be
    * shared between all DaVinci boards using the same core.
    */
    int status;

    status = platform_device_register(&usb_dev);
    if (status != 0)
        pr_debug("setup_usb --> %d\n", status);
    else
        //board_setup_psc(DAVINCI_GPSC_ARMDOMAIN, DAVINCI_LPSC_USB, 1);
        printk ("MUSB UR8 pdev active!\n");

    return 0;
}

subsys_initcall(ur8_musb_init);
#endif /* (CONFIG_USB_MUSB_HDRC) || (CONFIG_USB_MUSB_HDRC_MODULE)*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ur8_late_init(void) {

    return 0;
}

late_initcall(ur8_late_init);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ur8_machine_restart(char *command)
{
    u32 periph_reset_val = ((1U << 31) | (1 << 0) | (1 << 1) | (1 << 6));

    __printk("[%s] reset all 0x%08x at address %p\n", __FUNCTION__, periph_reset_val,(unsigned int *)(UR8_RESET_BASE + UR8_RESET_PEREPHERIAL));
	writel(periph_reset_val, (unsigned int *)(UR8_RESET_BASE + UR8_RESET_PEREPHERIAL));
    __printk("[%s] reboot\n", __FUNCTION__);
	writel(1, (unsigned int *)(UR8_RESET_BASE + UR8_RESET_SOFTWARE));
}

static void ur8_machine_halt(void)
{
    __printk("[%s]\n", __FUNCTION__);
	while (1)
		;
}

static void ur8_machine_power_off(void)
{
	u32 *power_reg = (u32 *)ioremap(UR8_CLOCK_BASE, 1);
	u32 power_state = readl(power_reg) | (3 << 30);
    __printk("[%s]\n", __FUNCTION__);
	writel(power_state, power_reg);
	ur8_machine_halt();
}

const char *get_system_type(void)
{
	u16 chip_id = ur8_chip_id();
	switch (chip_id) {
	case AR7_CHIP_7300:
		return "TI AR7 (TNETD7300)";
	case AR7_CHIP_7100:
		return "TI AR7 (TNETD7100)";
	case AR7_CHIP_7200:
		return "TI AR7 (TNETD7200)";
	case UR8_CHIP_7270:
		return "TI UR8 (7270)";
	default:
		return "TI UR8 (Unknown)";
	}
}

static int __init ur8_init_console(void)
{
	return 0;
}
console_initcall(ur8_init_console);

/*
 * Initializes basic routines and structures pointers, memory size (as
 * given by the bios and saves the command line.
 */

void __init plat_mem_setup(void)
{
	unsigned long io_base;

	_machine_restart = ur8_machine_restart;
	_machine_halt = ur8_machine_halt;
	pm_power_off = ur8_machine_power_off;
	panic_timeout = 3;

	io_base = (unsigned long)ioremap(UR8_REGS_BASE, 0x10000);
	if (!io_base)
		panic("Can't remap IO base!\n");
	set_io_port_base(io_base);

	prom_meminit();

#if defined(CONFIG_PCI)
	ur8_pci_init();
#endif	

    ur8_vbus_init();

	printk(KERN_INFO "%s, ID: 0x%04x, Revision: 0x%02x\n",
					get_system_type(),
		ur8_chip_id(), ur8_chip_rev());
}


/*--- Kernel-Schnittstelle für das neue LED-Modul ---*/
enum _led_event { /* DUMMY DEFINITION */ LastEvent = 0 };
int (*led_event_action)(int, enum _led_event , unsigned int ) = NULL;
EXPORT_SYMBOL(led_event_action);

