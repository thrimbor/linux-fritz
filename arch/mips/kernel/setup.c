/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 Linus Torvalds
 * Copyright (C) 1995 Waldorf Electronics
 * Copyright (C) 1994, 95, 96, 97, 98, 99, 2000, 01, 02, 03  Ralf Baechle
 * Copyright (C) 1996 Stoned Elipot
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) 2000, 2001, 2002, 2007  Maciej W. Rozycki
 */
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/screen_info.h>
#include <linux/bootmem.h>
#include <linux/initrd.h>
#include <linux/root_dev.h>
#include <linux/highmem.h>
#include <linux/console.h>
#include <linux/pfn.h>
#include <linux/debugfs.h>
#include <linux/avm_kernel_config.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/bugs.h>
#include <asm/cache.h>
#include <asm/cpu.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/smp-ops.h>
#include <asm/system.h>
#include <asm/prom.h>

#if defined(CONFIG_LANTIQ)
    #include "../mach-infineon/common/ifxmips_core.h"
#endif /*--- #if defined(CONFIG_LANTIQ) ---*/

struct cpuinfo_mips cpu_data[NR_CPUS] __read_mostly;

EXPORT_SYMBOL(cpu_data);

#ifdef CONFIG_VT
struct screen_info screen_info;
#endif

/*
 * Despite it's name this variable is even if we don't have PCI
 */
unsigned int PCI_DMA_BUS_IS_PHYS;

EXPORT_SYMBOL(PCI_DMA_BUS_IS_PHYS);

/*
 * Setup information
 *
 * These are initialized so they are in the .data section
 */
unsigned long mips_machtype __read_mostly = MACH_UNKNOWN;

EXPORT_SYMBOL(mips_machtype);

struct boot_mem_map boot_mem_map;

static char command_line[CL_SIZE];
       char arcs_cmdline[CL_SIZE]=CONFIG_CMDLINE;

/*
 * mips_io_port_base is the begin of the address space to which x86 style
 * I/O ports are mapped.
 */
const unsigned long mips_io_port_base __read_mostly = -1;
EXPORT_SYMBOL(mips_io_port_base);

static struct resource code_resource = { .name = "Kernel code", };
static struct resource data_resource = { .name = "Kernel data", };

void __init add_memory_region(phys_t start, phys_t size, long type)
{
	int x = boot_mem_map.nr_map;
	struct boot_mem_map_entry *prev = boot_mem_map.map + x - 1;

	/* Sanity check */
	if (start + size < start) {
		pr_warning("Trying to add an invalid memory region, skipped\n");
		return;
	}

	/*
	 * Try to merge with previous entry if any.  This is far less than
	 * perfect but is sufficient for most real world cases.
	 */
	if (x && prev->addr + prev->size == start && prev->type == type) {
		prev->size += size;
		return;
	}

	if (x == BOOT_MEM_MAP_MAX) {
		pr_err("Ooops! Too many entries in the memory map!\n");
		return;
	}

	boot_mem_map.map[x].addr = start;
	boot_mem_map.map[x].size = size;
	boot_mem_map.map[x].type = type;
	boot_mem_map.nr_map++;
}

#if 0
static void __init print_memory_map(void)
{
	int i;
	const int field = 2 * sizeof(unsigned long);

	for (i = 0; i < boot_mem_map.nr_map; i++) {
		printk(KERN_INFO " memory: %0*Lx @ %0*Lx ",
		       field, (unsigned long long) boot_mem_map.map[i].size,
		       field, (unsigned long long) boot_mem_map.map[i].addr);

		switch (boot_mem_map.map[i].type) {
		case BOOT_MEM_RAM:
			printk(KERN_CONT "(usable)\n");
			break;
		case BOOT_MEM_ROM_DATA:
			printk(KERN_CONT "(ROM data)\n");
			break;
		case BOOT_MEM_BOOTLOADER:
			printk(KERN_CONT "(bootloader)\n");
			break;
		case BOOT_MEM_RESERVED:
			printk(KERN_CONT "(reserved)\n");
			break;
		default:
			printk(KERN_CONT "type %lu\n", boot_mem_map.map[i].type);
			break;
		}
	}
}
#endif

/*
 * Manage initrd
 */
#ifdef CONFIG_BLK_DEV_INITRD

static int __init rd_start_early(char *p)
{
	unsigned long start = memparse(p, &p);

#ifdef CONFIG_64BIT
	/* Guess if the sign extension was forgotten by bootloader */
	if (start < XKPHYS)
		start = (int)start;
#endif
	initrd_start = start;
	initrd_end += start;
	return 0;
}
early_param("rd_start", rd_start_early);

static int __init rd_size_early(char *p)
{
	initrd_end += memparse(p, &p);
	return 0;
}
early_param("rd_size", rd_size_early);

/* it returns the next free pfn after initrd */
static unsigned long __init init_initrd(void)
{
	unsigned long end;

	/*
	 * Board specific code or command line parser should have
	 * already set up initrd_start and initrd_end. In these cases
	 * perfom sanity checks and use them if all looks good.
	 */
	if (!initrd_start || initrd_end <= initrd_start) {
#ifdef CONFIG_PROBE_INITRD_HEADER
		u32 *initrd_header;

		/*
		 * See if initrd has been added to the kernel image by
		 * arch/mips/boot/addinitrd.c. In that case a header is
		 * prepended to initrd and is made up by 8 bytes. The first
		 * word is a magic number and the second one is the size of
		 * initrd.  Initrd start must be page aligned in any cases.
		 */
		initrd_header = __va(PAGE_ALIGN(__pa_symbol(&_end) + 8)) - 8;
		if (initrd_header[0] != 0x494E5244)
			goto disable;
		initrd_start = (unsigned long)(initrd_header + 2);
		initrd_end = initrd_start + initrd_header[1];
#else
		goto disable;
#endif
	}

	if (initrd_start & ~PAGE_MASK) {
		pr_err("initrd start must be page aligned\n");
		goto disable;
	}
	if (initrd_start < PAGE_OFFSET) {
		pr_err("initrd start < PAGE_OFFSET\n");
		goto disable;
	}

	/*
	 * Sanitize initrd addresses. For example firmware
	 * can't guess if they need to pass them through
	 * 64-bits values if the kernel has been built in pure
	 * 32-bit. We need also to switch from KSEG0 to XKPHYS
	 * addresses now, so the code can now safely use __pa().
	 */
	end = __pa(initrd_end);
	initrd_end = (unsigned long)__va(end);
	initrd_start = (unsigned long)__va(__pa(initrd_start));

	ROOT_DEV = Root_RAM0;
	return PFN_UP(end);
disable:
	initrd_start = 0;
	initrd_end = 0;
	return 0;
}

static void __init finalize_initrd(void)
{
	unsigned long size = initrd_end - initrd_start;

	if (size == 0) {
		printk(KERN_INFO "Initrd not found or empty");
		goto disable;
	}
	if (__pa(initrd_end) > PFN_PHYS(max_low_pfn)) {
		printk(KERN_ERR "Initrd extends beyond end of memory");
		goto disable;
	}

	reserve_bootmem(__pa(initrd_start), size, BOOTMEM_DEFAULT);
	initrd_below_start_ok = 1;

	pr_info("Initial ramdisk at: 0x%lx (%lu bytes)\n",
		initrd_start, size);
	return;
disable:
	printk(KERN_CONT " - disabling initrd\n");
	initrd_start = 0;
	initrd_end = 0;
}

#else  /* !CONFIG_BLK_DEV_INITRD */

static unsigned long __init init_initrd(void)
{
	return 0;
}

#define finalize_initrd()	do {} while (0)

#endif

/*
 * Initialize the bootmem allocator. It also setup initrd related data
 * if needed.
 */
#ifdef CONFIG_SGI_IP27

static void __init bootmem_init(void)
{
	init_initrd();
	finalize_initrd();
}

#else  /* !CONFIG_SGI_IP27 */

static void __init bootmem_init(void)
{
	unsigned long reserved_end;
	unsigned long mapstart = ~0UL;
	unsigned long bootmap_size;
	int i;

	/*
	 * Init any data related to initrd. It's a nop if INITRD is
	 * not selected. Once that done we can determine the low bound
	 * of usable memory.
	 */
	reserved_end = max(init_initrd(),
			   (unsigned long) PFN_UP(__pa_symbol(&_end)));

	/*
	 * max_low_pfn is not a number of pages. The number of pages
	 * of the system is given by 'max_low_pfn - min_low_pfn'.
	 */
	min_low_pfn = ~0UL;
	max_low_pfn = 0;

	/*
	 * Find the highest page frame number we have available.
	 */
	for (i = 0; i < boot_mem_map.nr_map; i++) {
		unsigned long start, end;

		if (boot_mem_map.map[i].type != BOOT_MEM_RAM)
			continue;

		start = PFN_UP(boot_mem_map.map[i].addr);
		end = PFN_DOWN(boot_mem_map.map[i].addr
				+ boot_mem_map.map[i].size);

		if (end > max_low_pfn)
			max_low_pfn = end;
		if (start < min_low_pfn)
			min_low_pfn = start;
		if (end <= reserved_end)
			continue;
		if (start >= mapstart)
			continue;
		mapstart = max(reserved_end, start);
	}

	if (min_low_pfn >= max_low_pfn)
		panic("Incorrect memory mapping !!!");
	if (min_low_pfn > ARCH_PFN_OFFSET) {
		pr_info("Wasting %lu bytes for tracking %lu unused pages\n",
			(min_low_pfn - ARCH_PFN_OFFSET) * sizeof(struct page),
			min_low_pfn - ARCH_PFN_OFFSET);
	} else if (min_low_pfn < ARCH_PFN_OFFSET) {
		pr_info("%lu free pages won't be used\n",
			ARCH_PFN_OFFSET - min_low_pfn);
	}
	min_low_pfn = ARCH_PFN_OFFSET;

	/*
	 * Determine low and high memory ranges
	 */
	max_pfn = max_low_pfn;
	if (max_low_pfn > PFN_DOWN(HIGHMEM_START)) {
#ifdef CONFIG_HIGHMEM
		highstart_pfn = PFN_DOWN(HIGHMEM_START);
		highend_pfn = max_low_pfn;
#endif
		max_low_pfn = PFN_DOWN(HIGHMEM_START);
	}

	/*
	 * Initialize the boot-time allocator with low memory only.
	 */
	bootmap_size = init_bootmem_node(NODE_DATA(0), mapstart,
					 min_low_pfn, max_low_pfn);


	for (i = 0; i < boot_mem_map.nr_map; i++) {
		unsigned long start, end;

		start = PFN_UP(boot_mem_map.map[i].addr);
		end = PFN_DOWN(boot_mem_map.map[i].addr
				+ boot_mem_map.map[i].size);

		if (start <= min_low_pfn)
			start = min_low_pfn;
		if (start >= end)
			continue;

#ifndef CONFIG_HIGHMEM
		if (end > max_low_pfn)
			end = max_low_pfn;

		/*
		 * ... finally, is the area going away?
		 */
		if (end <= start)
			continue;
#endif

		add_active_range(0, start, end);
	}

	/*
	 * Register fully available low RAM pages with the bootmem allocator.
	 */
	for (i = 0; i < boot_mem_map.nr_map; i++) {
		unsigned long start, end, size;

		/*
		 * Reserve usable memory.
		 */
		if (boot_mem_map.map[i].type != BOOT_MEM_RAM)
			continue;

		start = PFN_UP(boot_mem_map.map[i].addr);
		end   = PFN_DOWN(boot_mem_map.map[i].addr
				    + boot_mem_map.map[i].size);
		/*
		 * We are rounding up the start address of usable memory
		 * and at the end of the usable range downwards.
		 */
		if (start >= max_low_pfn)
			continue;
		if (start < reserved_end)
			start = reserved_end;
		if (end > max_low_pfn)
			end = max_low_pfn;

		/*
		 * ... finally, is the area going away?
		 */
		if (end <= start)
			continue;
		size = end - start;

		/* Register lowmem ranges */
		free_bootmem(PFN_PHYS(start), size << PAGE_SHIFT);
		memory_present(0, start, end);
	}

	/*
	 * Reserve the bootmap memory.
	 */
	reserve_bootmem(PFN_PHYS(mapstart), bootmap_size, BOOTMEM_DEFAULT);

	/*
	 * Reserve initrd memory if needed.
	 */
	finalize_initrd();
}

#endif	/* CONFIG_SGI_IP27 */

/*
 * arch_mem_init - initialize memory management subsystem
 *
 *  o plat_mem_setup() detects the memory configuration and will record detected
 *    memory areas using add_memory_region.
 *
 * At this stage the memory configuration of the system is known to the
 * kernel but generic memory management system is still entirely uninitialized.
 *
 *  o bootmem_init()
 *  o sparse_init()
 *  o paging_init()
 *
 * At this stage the bootmem allocator is ready to use.
 *
 * NOTE: historically plat_mem_setup did the entire platform initialization.
 *       This was rather impractical because it meant plat_mem_setup had to
 * get away without any kind of memory allocator.  To keep old code from
 * breaking plat_setup was just renamed to plat_setup and a second platform
 * initialization hook for anything else was introduced.
 */

static int usermem __initdata;

static int __init early_parse_mem(char *p)
{
	unsigned long start, size;

	/*
	 * If a user specifies memory size, we
	 * blow away any automatically generated
	 * size.
	 */
	if (usermem == 0) {
		boot_mem_map.nr_map = 0;
		usermem = 1;
 	}
	start = 0;
	size = memparse(p, &p);
	if (*p == '@')
		start = memparse(p + 1, &p);

	add_memory_region(start, size, BOOT_MEM_RAM);
	return 0;
}
early_param("mem", early_parse_mem);

static void __init arch_mem_init(char **cmdline_p)
{
	extern void plat_mem_setup(void);

	/* call board setup routine */
	plat_mem_setup();

	pr_info("Determined physical RAM map:\n");
	/*--- print_memory_map(); ---*/

	strlcpy(command_line, arcs_cmdline, sizeof(command_line));
	strlcpy(boot_command_line, command_line, COMMAND_LINE_SIZE);

	*cmdline_p = command_line;

	parse_early_param();

	if (usermem) {
		pr_info("User-defined physical RAM map:\n");
		/*--- print_memory_map(); ---*/
	}

	bootmem_init();
	sparse_init();
	paging_init();
}

static void __init resource_init(void)
{
	int i;

	if (UNCAC_BASE != IO_BASE)
		return;

	code_resource.start = __pa_symbol(&_text);
	code_resource.end = __pa_symbol(&_etext) - 1;
	data_resource.start = __pa_symbol(&_etext);
	data_resource.end = __pa_symbol(&_edata) - 1;

	/*
	 * Request address space for all standard RAM.
	 */
	for (i = 0; i < boot_mem_map.nr_map; i++) {
		struct resource *res;
		unsigned long start, end;

		start = boot_mem_map.map[i].addr;
		end = boot_mem_map.map[i].addr + boot_mem_map.map[i].size - 1;

        /*--- printk("[%s] boot_mem_map.map[%d].addr = 0x%lx\n", __FUNCTION__, i, start); ---*/
        /*--- printk("[%s] boot_mem_map.map[%d].end  = 0x%lx\n", __FUNCTION__, i, end); ---*/
        /*--- printk("[%s] boot_mem_map.map[%d].type = %s\n", __FUNCTION__, i, ---*/ 
            /*--- boot_mem_map.map[i].type == BOOT_MEM_RAM ?  "BOOT_MEM_RAM" : ---*/
            /*--- boot_mem_map.map[i].type == BOOT_MEM_ROM_DATA ?  "BOOT_MEM_ROM_DATA" : ---*/
            /*--- boot_mem_map.map[i].type == BOOT_MEM_BOOTLOADER ?  "BOOT_MEM_BOOTLOADER" : ---*/
            /*--- boot_mem_map.map[i].type == BOOT_MEM_RESERVED ?  "BOOT_MEM_RESERVED" : "BOOT_MEM_UNKNWON"); ---*/

		if (start >= HIGHMEM_START) {
            /*--- printk("[%s] start >= HIGHMEM_START (0x%lx)\n", __FUNCTION__, HIGHMEM_START); ---*/
			continue;
        }
		if (end >= HIGHMEM_START)
			end = HIGHMEM_START - 1;

		res = alloc_bootmem(sizeof(struct resource));
		switch (boot_mem_map.map[i].type) {
		case BOOT_MEM_RAM:
		case BOOT_MEM_ROM_DATA:
			res->name = "System RAM";
			break;
        case BOOT_MEM_BOOTLOADER:
			res->name = "bootloader";
			break;
		case BOOT_MEM_RESERVED:
		default:
			res->name = "reserved";
		}

		res->start = start;
		res->end = end;


        /*--- printk("[%s] reserve '%s'\n", __FUNCTION__, res->name); ---*/
		res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;
		request_resource(&iomem_resource, res);

        /*
         *  We don't know which RAM region contains kernel data,
         *  so we try it repeatedly and let the resource manager
         *  test it.
         */
        request_resource(res, &code_resource);
        request_resource(res, &data_resource);

        {
            static char run_once = 0;

            if(!run_once) {
#if defined(CONFIG_MIPS_VPE_LOADER)
                extern struct resource *ar9vr9_alloc_c55_code(unsigned long start, unsigned long end);
                struct resource *pc55req;
                /*--- struct resource *pmodulereq; ---*/
                pc55req = ar9vr9_alloc_c55_code(res->start, res->end);
                if(pc55req) {
                    request_resource(res, pc55req);
                    /*--- reserved memory for all time: ---*/
                    if(reserve_bootmem(pc55req->start, pc55req->end - pc55req->start + 1, BOOTMEM_DEFAULT)) {
                        printk(KERN_ERR "[c55-alloc] reserve memory for module-load failed (start 0x%x end 0x%x)\n",
                                pc55req->start, pc55req->end);
                    }
                }
#endif/*--- #if defined(CONFIG_MIPS_VPE_LOADER) ---*/


                {
                    extern unsigned long module_alloc_size_list_base;
                    extern unsigned int module_alloc_size_list_size;
                    static struct resource module_param;
                    char *arg;
                    if(kernel_modulmemory_config) {
                        struct _kernel_modulmemory_config *C = kernel_modulmemory_config;
                        module_alloc_size_list_size = 0;
                        while(C->name) {
                            module_alloc_size_list_size += (C->size + 4096) & ~(4096 - 1);
                            C++;
                        }
                        printk("[module-alloc-by-name] 'modulemem' not in use, use AVM Kernel Config (%d bytes, reserved for module mem)\n", module_alloc_size_list_size );
                    } else if((arg = prom_getenv("modulemem")) != NULL) {
                        module_alloc_size_list_size = simple_strtoul(arg, NULL, 0);
                    }
                    if(module_alloc_size_list_size == 0UL) {
                        printk("[module-alloc-by-name] 'modulemem' not set, function disabled\n");
                    } else {
                        module_alloc_size_list_base = (unsigned long)alloc_bootmem_pages((module_alloc_size_list_size >> PAGE_SHIFT) + 1);
                        module_param.start = CPHYSADDR(module_alloc_size_list_base);
                        module_param.end = CPHYSADDR(module_alloc_size_list_base + module_alloc_size_list_size);
                        module_param.name  = "module memory";
                        module_param.flags = IORESOURCE_MEM | IORESOURCE_BUSY;
                        if(request_resource(res, &module_param)) {
                            printk(KERN_ERR "[module-alloc] failed 0x%x bytes at 0x%lx\n", module_alloc_size_list_size, module_alloc_size_list_base);
                        } else {
                            printk(KERN_ERR "[module-alloc] use 0x%x bytes at 0x%lx\n", module_alloc_size_list_size, module_alloc_size_list_base);
                        }
                        if(reserve_bootmem(module_param.start, module_param.end - module_param.start + 1, BOOTMEM_DEFAULT)) {
                            printk(KERN_ERR "[module-alloc] reserve memory for module-load failed (start 0x%x end 0x%x)\n",
                                    module_param.start, module_param.end);
                        }
                    }

                }
                run_once = 1;
            }
        }
    }
}

void __init setup_arch(char **cmdline_p)
{
	cpu_probe();
	prom_init();


    init_avm_kernel_config();  /* init avm kernel config */

#ifdef CONFIG_EARLY_PRINTK
	setup_early_printk();
#endif
	cpu_report();
	check_bugs_early();

#if defined(CONFIG_VT)
#if defined(CONFIG_VGA_CONSOLE)
	conswitchp = &vga_con;
#elif defined(CONFIG_DUMMY_CONSOLE)
	conswitchp = &dummy_con;
#endif
#endif

	arch_mem_init(cmdline_p);

	resource_init();
	plat_smp_setup();
}

static int __init fpu_disable(char *s)
{
	int i;

	for (i = 0; i < NR_CPUS; i++)
		cpu_data[i].options &= ~MIPS_CPU_FPU;

	return 1;
}

__setup("nofpu", fpu_disable);

static int __init dsp_disable(char *s)
{
	cpu_data[0].ases &= ~MIPS_ASE_DSP;

	return 1;
}

__setup("nodsp", dsp_disable);

unsigned long kernelsp[NR_CPUS];
unsigned long fw_arg0, fw_arg1, fw_arg2, fw_arg3;
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
EXPORT_SYMBOL(kernelsp);
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/

#ifdef CONFIG_DEBUG_FS
struct dentry *mips_debugfs_dir;
static int __init debugfs_mips(void)
{
	struct dentry *d;

	d = debugfs_create_dir("mips", NULL);
	if (!d)
		return -ENOMEM;
	mips_debugfs_dir = d;
	return 0;
}
arch_initcall(debugfs_mips);
#endif
