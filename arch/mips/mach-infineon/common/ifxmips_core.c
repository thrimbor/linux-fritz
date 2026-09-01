#include "ifxmips_core.h"
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/prom.h>
#include <ifx_regs.h>


#if defined(CONFIG_VR9)
static DEFINE_SPINLOCK(ifx_yield_lock);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void ifx_yield_en(unsigned int reg, unsigned int bit, unsigned int on) {
    unsigned long flags;
    spin_lock_irqsave(&ifx_yield_lock, flags);
    if(on) {
        *IFX_YIELDEN(reg) |= 0x1 << (bit);
    } else {
        *IFX_YIELDEN(reg) &= ~(0x1 << (bit));
    }
    spin_unlock_irqrestore(&ifx_yield_lock, flags);
}
EXPORT_SYMBOL(ifx_yield_en);
#endif /*--- #if defined(CONFIG_VR9) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_AR9VR9_C55_MEMORY_SIZE) && (CONFIG_AR9VR9_C55_MEMORY_SIZE != 0) && defined(CONFIG_AR9VR9_C55_MEMORY_START)
static int var_is_in_area(unsigned long test, unsigned long range_min, unsigned long range_max) {
    if((test >= range_min) && (test < range_max)) {
        return 1;
    }
    return 0;
}
#endif

#ifdef HAVE_prom_c55_get_base_memory
static unsigned int c55_mem_start = 0;
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int prom_c55_get_base_memory(unsigned int *base, unsigned int *len) {
    if(len)*len  = CONFIG_AR9VR9_C55_MEMORY_SIZE * (1 << 10);
    if(base)*base = c55_mem_start;
    return 0;
}
EXPORT_SYMBOL(prom_c55_get_base_memory);
#endif

/*--------------------------------------------------------------------------------*\
 * keine Ueberschneidung mit Kernel!
 *
 *
 * Hinweis zur Adress-Angabe:
 *
 *     +---+ 16MB aligned (0xXX000000)  ^ (oberes Ende)
 *     |   |                            |
 *     |   |                   CONFIG_C55_MEMORY_SIZE
 *     |   |                            |
 *     +---+ CONFIG_C55_MEMORY_START    v
 *     |   |
 *     |   |
 *     |   |
 *     |   |
 *     |   |
 *     |   |
 *     +---+ 16MB aligned (0xXX000000)    (unteres Ende)
 *
\*--------------------------------------------------------------------------------*/
#define SZ_1K                           0x00000400
#define SZ_1M                           0x00100000
#define ALIGN_16MBYTE(addr) ((addr) &  ~((16 * SZ_1M) - 1))
extern unsigned long _text, _stext, _etext, __data_start, _end, __init_begin, __init_end;
struct resource *ar9vr9_alloc_c55_code(unsigned long ramstart, unsigned long ramend) {
#if defined(CONFIG_AR9VR9_C55_MEMORY_SIZE) && (CONFIG_AR9VR9_C55_MEMORY_SIZE != 0) && defined(CONFIG_AR9VR9_C55_MEMORY_START)
    static struct resource c55_pram;
    unsigned long c55_codsize  = SZ_1K * CONFIG_AR9VR9_C55_MEMORY_SIZE;
	unsigned long kcode_start  = virt_to_phys(&_text);
	unsigned long kcode_end    = virt_to_phys(&_etext - 1);
	unsigned long kdata_start  = virt_to_phys(&__data_start);
	unsigned long kdata_end    = virt_to_phys(&_end - 1);
    
    c55_mem_start = CONFIG_AR9VR9_C55_MEMORY_START;

    BUG_ON(c55_mem_start & 0x0000FFFF);

    if(!var_is_in_area(c55_mem_start, kcode_start, kcode_end) && 
       !var_is_in_area(c55_mem_start + c55_codsize, kcode_start, kcode_end) &&
       !var_is_in_area(c55_mem_start, kdata_start, kdata_end) && 
       !var_is_in_area(c55_mem_start + c55_codsize, kdata_start, kdata_end)) {
        printk("[c55] c55_mem_start = 0x%x\n", c55_mem_start);
        c55_pram.name  = "c55 text";
        c55_pram.start = c55_mem_start;
        c55_pram.end   = c55_pram.start + c55_codsize - 1;
        c55_pram.flags = IORESOURCE_MEM | IORESOURCE_BUSY;
        return &c55_pram;
    }
#endif /*--- #if defined(CONFIG_AR9VR9_C55_MEMORY) && (CONFIG_AR9VR9_C55_MEMORY != 0) ---*/
    return NULL;
}

extern void __ifx_fill_lock_icache(void *addr, u32 size);
extern void __ifx_unlock_icache(void *addr, u32 size);
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void ifx_fill_lock_icache(void *addr, u32 size){
    __ifx_fill_lock_icache(addr, size);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void ifx_unlock_icache(void *addr, u32 size){
    __ifx_unlock_icache(addr, size);
}
