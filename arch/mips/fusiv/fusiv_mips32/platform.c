/*
 * Copyright (C) 2006  Ikanos Communications. All rights reserved.
 * The information and source code contained herein is the property
 * of Ikanos Communications.
 */


/*
 * Platform device support for Ikanos's  IKF68XX SoCs.
 *
 * Copyright 2004, Matt Porter <mporter@kernel.crashing.org>
 * Modified  2005, Vivek Dharmadhikari <mporter@kernel.crashing.org>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>

#include <ikf68xx_usb.h>

static struct resource ikf68xx_usb_ohci_hcd_resources[] = {
	[0] = {
	       .start = IKF68XX_USB_OHCI_BASE,
	       .end = IKF68XX_USB_OHCI_BASE + IKF68XX_USB_OHCI_LEN - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = IKF68XX_USB_HOST_INT,
	       .end = IKF68XX_USB_HOST_INT,
	       .flags = IORESOURCE_IRQ,
	       },
};

static struct resource ikf68xx_usb_ehci_hcd_resources[] = {
	[0] = {
	       .start = IKF68XX_USB_EHCI_BASE,
	       .end = IKF68XX_USB_EHCI_BASE + IKF68XX_USB_EHCI_LEN - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = IKF68XX_USB_HOST_INT,
	       .end = IKF68XX_USB_HOST_INT,
	       .flags = IORESOURCE_IRQ,
	       },
};

static u64 ikf68xx_dmamask = ~(u32) 0;

static struct platform_device ikf68xx_usb_ohci_hcd_device = {
	.name = "ikf68xx-ohci-hcd",
	.id = 0,
	.dev = {
		.dma_mask = &ikf68xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(ikf68xx_usb_ohci_hcd_resources),
	.resource = ikf68xx_usb_ohci_hcd_resources,
};

static struct platform_device ikf68xx_usb_ehci_hcd_device = {
	.name = "ikf68xx-ehci-hcd",
	.id = 0,
	.dev = {
		.dma_mask = &ikf68xx_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = ARRAY_SIZE(ikf68xx_usb_ehci_hcd_resources),
	.resource = ikf68xx_usb_ehci_hcd_resources,
};

static struct platform_device *ikf68xx_platform_devices[] __initdata = {
	&ikf68xx_usb_ohci_hcd_device,
	&ikf68xx_usb_ehci_hcd_device,
};

int
ikf68xx_platform_init(void)
{
	return platform_add_devices(ikf68xx_platform_devices,
				    ARRAY_SIZE(ikf68xx_platform_devices));
}

arch_initcall(ikf68xx_platform_init);
