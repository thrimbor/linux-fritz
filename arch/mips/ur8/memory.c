/*
 * Copyright (C) 2007 Felix Fietkau <nbd@openwrt.org>
 * Copyright (C) 2007 Eugene Konev <ejka@openwrt.org>
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
#include <linux/bootmem.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pfn.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/swap.h>
#include <linux/resource.h>
#include <linux/ioport.h>

#include <asm/bootinfo.h>
#include <asm/page.h>
#include <asm/sections.h>

#include <asm/mach-ur8/ur8.h>
#include <asm/prom.h>

#if 0
static int __init memsize(void)
{
	u32 size = (64 << 20);
	u32 *addr = (u32 *)KSEG1ADDR(UR8_SDRAM_BASE + size - 4);
	u32 *kernel_end = (u32 *)KSEG1ADDR(CPHYSADDR((u32)&_end));
	u32 *tmpaddr = addr;

	while (tmpaddr > kernel_end) {
		*tmpaddr = (u32)tmpaddr;
		size >>= 1;
		tmpaddr -= size >> 2;
	}

	do {
		tmpaddr += size >> 2;
		if (*tmpaddr != (u32)tmpaddr)
			break;
		size <<= 1;
	} while (size < (64 << 20));

	writel((u32)tmpaddr, &addr);

	return size;
}
# endif



/*------------------------------------------------------------------------------------------*\
 *  wir verwenden nicht die automatische Erkennung, sondern den vom Urlader uebergebenen
 *  env-Parameter 'memsize', da gegebenenfalls noch Speicher fuers MTD-RAM freigehalten
 *  werden muss.
\*------------------------------------------------------------------------------------------*/
static int __init memsize(void)
{
	u32 size;
	char *s, *p;
	s = prom_getenv("memsize");
	if (s) {
		size = simple_strtoul(s, &p, 16);
	} else {
      size = 0;
	  printk("[%s] memsize konnte nicht aus env ermittelt werden\n",__FUNCTION__);
	  BUG_ON(1);
	}
	printk("[%s] memsize=%#x\n", __FUNCTION__, size);

	return size;
}

#if defined(CONFIG_MIPS_UR8_C55_MEMORY) && (CONFIG_MIPS_UR8_C55_MEMORY != 0)
static unsigned int c55_memory_start;
static unsigned int wlan_memory_start;
#endif /*--- #if defined(CONFIG_MIPS_UR8_C55_MEMORY) && (CONFIG_MIPS_UR8_C55_MEMORY != 0) ---*/



void __init prom_meminit(void)
{
	unsigned long pages;
	pages = memsize() >> PAGE_SHIFT;

#if !defined(CONFIG_MIPS_UR8_C55_MEMORY) && !defined(CONFIG_MIPS_UR8_WLAN_MEMORY)
    {
        unsigned int start;
        unsigned int len;
        start            = PHYS_OFFSET;
        len              = __pa_symbol(&_text) - (0x1FFFFFFF & PHYS_OFFSET);
	    add_memory_region(start, len, BOOT_MEM_BOOTLOADER);  /*--- kernel memory recource index 0 ---*/

        start            = PHYS_OFFSET + len;
        len              = memsize() - len;
	    add_memory_region(start, len, BOOT_MEM_RAM); /*--- kernel memory recource index 1 ---*/
    }
#else /*--- #if !defined(CONFIG_MIPS_UR8_C55_MEMORY) && !defined(CONFIG_MIPS_UR8_WLAN_MEMORY) ---*/
    {
        unsigned int start;
        unsigned int len;
        unsigned int rsvd_memory_size  = ((CONFIG_MIPS_UR8_C55_MEMORY + CONFIG_MIPS_UR8_WLAN_MEMORY) << 10);
        unsigned int rsvd_memory_start = ((pages << PAGE_SHIFT) & ~((16 * (1 << 20)) - 1)) - rsvd_memory_size;

        start            = PHYS_OFFSET;
        len              = __pa_symbol(&_text) - (0x1FFFFFFF & PHYS_OFFSET);
	    add_memory_region(start, len, BOOT_MEM_BOOTLOADER);  /*--- kernel memory recource index 0 ---*/

        start           += len;
        len              = rsvd_memory_start - len;
	    add_memory_region(start, len, BOOT_MEM_RAM); /*--- kernel memory recource index 1 ---*/

        start += len;
        len    = rsvd_memory_size;
	    add_memory_region(start, len, BOOT_MEM_RESERVED); /*--- kernel memory recource index 2 ---*/

        start += len;
        len    = memsize() - (rsvd_memory_start + rsvd_memory_size); 
	    add_memory_region(start, len, BOOT_MEM_RAM);

        wlan_memory_start = rsvd_memory_start;
        /*--------------------------------------------------------------------------------*\
         * Startaddresse C55: 16 MByte aligned Adresse - (CONFIG_MIPS_UR8_C55_MEMORY << 10)
         * Achtung! Darf NICHT geaendert werden!!!
        \*--------------------------------------------------------------------------------*/
        c55_memory_start  = rsvd_memory_start + (CONFIG_MIPS_UR8_WLAN_MEMORY << 10);
    }
#endif /*--- #else ---*/ /*--- #if !defined(CONFIG_MIPS_UR8_C55_MEMORY) && !defined(CONFIG_MIPS_UR8_WLAN_MEMORY) ---*/
}

void __init prom_free_prom_memory(void) {
	free_init_pages("bootloader memory", (0x1FFFFFFF & PHYS_OFFSET), __pa_symbol(&_text));
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_MIPS_UR8_C55_MEMORY) && (CONFIG_MIPS_UR8_C55_MEMORY != 0)
int prom_c55_get_base_memory(unsigned int *base, unsigned int *len) {
    if(base)
        *base = PHYS_OFFSET + c55_memory_start;
    if(len)
        *len = (CONFIG_MIPS_UR8_C55_MEMORY << 10);
    return 0;
}

EXPORT_SYMBOL(prom_c55_get_base_memory);
#endif /*--- #if defined(CONFIG_MIPS_UR8_C55_MEMORY) && (CONFIG_MIPS_UR8_C55_MEMORY != 0) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_MIPS_UR8_WLAN_MEMORY) && (CONFIG_MIPS_UR8_WLAN_MEMORY != 0)
int prom_wlan_get_base_memory(unsigned int *base, unsigned int *len) {
    if(base)
        *base = PHYS_OFFSET + wlan_memory_start;
    if(len)
        *len = (CONFIG_MIPS_UR8_WLAN_MEMORY << 10);
    return 0;
}

EXPORT_SYMBOL(prom_wlan_get_base_memory);
#endif /*--- #if defined(CONFIG_MIPS_UR8_WLAN_MEMORY) && (CONFIG_MIPS_UR8_WLAN_MEMORY != 0) ---*/
