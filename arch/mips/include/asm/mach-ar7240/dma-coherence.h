/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2006  Ralf Baechle <ralf@linux-mips.org>
 * Copyright (C) 2007  Felix Fietkau <nbd@openwrt.org>
 *
 */
#ifndef __ASM_MACH_AR7240_GENERIC_DMA_COHERENCE_H
#define __ASM_MACH_AR7240_GENERIC_DMA_COHERENCE_H

#include <asm/types.h>

#define PCI_DMA_OFFSET	0x20000000

struct device;

static dma_addr_t plat_map_dma_mem(struct device *dev, void *addr, size_t size)
{
	return virt_to_phys(addr) + (dev != NULL ? PCI_DMA_OFFSET : 0);
}

static dma_addr_t plat_map_dma_mem_page(struct device *dev, struct page *page)
{
	return page_to_phys(page) + (dev != NULL ? PCI_DMA_OFFSET : 0);
}

static unsigned long plat_dma_addr_to_phys(struct device *dev, dma_addr_t dma_addr)
{
	return (dma_addr > PCI_DMA_OFFSET ? dma_addr - PCI_DMA_OFFSET : dma_addr);
}

static inline void plat_unmap_dma_mem(struct device *dev, dma_addr_t dma_addr,
	size_t size, enum dma_data_direction direction)
{
}

static inline int plat_dma_supported(struct device *dev, u64 mask)
{
    return 1;
}

static inline void plat_extra_sync_for_device(struct device *dev)
{
    return;
}

static inline int plat_dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
    return 0;
}

static inline int plat_device_is_coherent(struct device *dev)
{
#ifdef CONFIG_DMA_COHERENT
    return 1;
#endif

#ifdef CONFIG_DMA_NONCOHERENT
    return 0;
#endif
}

static inline int plat_addr_is_coherent(unsigned long addr)
{
    if((addr & 0xE0000000) == 0xA0000000) {
        return 1;
    }
	return 0;
}

#endif /* __ASM_MACH_AR7240_GENERIC_DMA_COHERENCE_H */
