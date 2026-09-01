/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */
#ifndef __ASM_MACH_UR8_DMA_COHERENCE_H
#define __ASM_MACH_UR8_DMA_COHERENCE_H

#include <asm/addrspace.h>

struct device;

static inline dma_addr_t plat_map_dma_mem(struct device *dev, void *addr, size_t size) {
    switch((unsigned long)addr & (KSEG0 | KSEG1 | KSEG2 | KSEG3)) {
        case KSEG2: /*  0xC... Adresse */
            /*--- printk(KERN_ERR "[%s] addr 0x%p => 0x%lx\n", __FUNCTION__, addr, virt_to_phys(addr)); ---*/
	        return virt_to_phys(addr);
        default:
            /*--- printk(KERN_ERR "[%s] addr 0x%p => 0x%x\n", __FUNCTION__, addr, CPHYSADDR((unsigned long)addr)); ---*/
            return (dma_addr_t)CPHYSADDR((unsigned long)addr);
    }
}

static inline dma_addr_t plat_map_dma_mem_page(struct device *dev, struct page *page) {
	return page_to_phys(page);
}

static inline unsigned long plat_dma_addr_to_phys(struct device *dev, dma_addr_t dma_addr) {
	return dma_addr;
}

static inline void plat_unmap_dma_mem(struct device *dev, dma_addr_t dma_addr, size_t size, enum dma_data_direction direction) {
}

static inline int plat_dma_supported(struct device *dev, u64 mask) {
	/*
	 * we fall back to GFP_DMA when the mask isn't all 1s,
	 * so we can't guarantee allocations that must be
	 * within a tighter range than GFP_DMA..
	 */
	if (mask < DMA_BIT_MASK(24))
		return 0;

	return 1;
}

static inline void plat_extra_sync_for_device(struct device *dev) {
	return;
}

static inline int plat_dma_mapping_error(struct device *dev, dma_addr_t dma_addr) {
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

#endif /* __ASM_MACH_UR8_DMA_COHERENCE_H */

