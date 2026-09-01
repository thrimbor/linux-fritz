/*
 *	include/asm-mips/mach-generic/ioremap.h
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
#ifndef __ASM_MACH_GENERIC_IOREMAP_H
#define __ASM_MACH_GENERIC_IOREMAP_H

#include <linux/types.h>

/*
 * Allow physical addresses to be fixed up to help peripherals located
 * outside the low 32-bit range -- generic pass-through version.
 */
static inline phys_t fixup_bigphys_addr(phys_t phys_addr, phys_t size __attribute__ ((unused)))
{
	return phys_addr;
}

static inline void __iomem *plat_ioremap(phys_t offset __attribute__ ((unused)),
                                         unsigned long size __attribute__ ((unused)),
                                         unsigned long flags __attribute__ ((unused)))
{
	return NULL;
}

static inline int plat_iounmap(const volatile void __iomem *addr __attribute__ ((unused)))
{
	return 0;
}

#endif /* __ASM_MACH_GENERIC_IOREMAP_H */
