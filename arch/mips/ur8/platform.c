/*
 * Copyright (C) 2006,2007 Felix Fietkau <nbd@openwrt.org>
 * Copyright (C) 2006,2007 Eugene Konev <ejka@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/mtd/physmap.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/vlynq.h>
#include <linux/string.h>
#include <linux/etherdevice.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>

#include <asm/addrspace.h>
#include <ur8.h>
#include <ur8int.h>
#include <hw_gpio.h>
#include <ur8_clk.h>
#include <ur8_gpio.h>
#include <asm/prom.h>
#include <asm/mach_avm.h>
#include <linux/env.h>

#ifdef CONFIG_SERIAL_8250
static struct resource ur8_uart_resource[] __initdata = {
	{
		.start	= 0x08610E00,
		.end	= 0x08610E1C,
		.flags	= IORESOURCE_MEM,
	},
};
static struct plat_serial8250_port ur8_serial8250_port[] = {
	{
		.type		= PORT_16550A,
		.irq		= UR8_IRQ_UART0,
		.iotype		= UPIO_MEM32,
		.membase	= (char *)(CKSEG1ADDR(UR8_UART0_BASE)),
		.flags		= UPF_FIXED_TYPE,
		/*--- .flags		= UPF_IOREMAP | UPF_BOOT_AUTOCONF | UPF_SKIP_TEST, ---*/
		.regshift	= 2,
	},
	{},
};
#endif /*--- #ifdef CONFIG_SERIAL_8250 ---*/

static int __init ur8_register_devices(void)
{
	int res = 0;
#ifdef CONFIG_SERIAL_8250
	struct platform_device *pdev;

	pdev = platform_device_alloc("serial8250", -1);
	if (!pdev)
		return -ENOMEM;

	ur8_serial8250_port[0].uartclk = ur8_get_clock(avm_clock_id_peripheral);
	pdev->id = PLAT8250_DEV_PLATFORM;
	pdev->dev.platform_data = ur8_serial8250_port;

	res = platform_device_add_resources(pdev, ur8_uart_resource, ARRAY_SIZE(ur8_uart_resource));
	if (res)
		return res;	

	res = platform_device_add(pdev);
	if (res)
		return res;	

	printk("[%s] uart 8250 platformdevice registirert\n", __FUNCTION__);

#endif /* CONFIG_SERIAL_8250 */
	return res;
}



arch_initcall(ur8_register_devices);
