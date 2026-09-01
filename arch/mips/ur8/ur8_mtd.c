/*
 * linux/arch/mips/mips-boards/ur8/ur8_mtd.c
 * based on:
 *
 * linux/arch/arm/mach-davinci/board-evm.c
 * TI DaVinci EVM board
 *
 * Copyright (C) 2006 Texas Instruments.
 * Copyright (C) 2007 AVM GmbH
 *
 * ----------------------------------------------------------------------------
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ----------------------------------------------------------------------------
 *
 */

/**************************************************************************
 * Included Files
 **************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/env.h>
#include <linux/root_dev.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#include <asm/setup.h>
#include <asm/io.h>

#if defined(CONFIG_MTD_PHYSMAP) || defined(CONFIG_MTD_PHYSMAP_MODULE)
#define DO_MTD

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/plat-ram.h>
#include <asm/prom.h>
#include <linux/squashfs_fs.h>
#include <linux/jffs2.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #define UR8_MTD_DEBUG ---*/

#if defined(UR8_MTD_DEBUG)
    #define DEBUG_MTD(fmt, arg...) printk(KERN_ERR "[%d:%s/%d] " fmt "\n", smp_processor_id(), __func__, __LINE__, ##arg);
#else
    #define DEBUG_MTD(fmt, arg...)
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define MAX_FLASH_MTD   6
#define JFFS2_MIN_SIZE  6
#define JFFS2_MAX_SIZE  16

static struct mtd_partition ur8_partitions[MAX_FLASH_MTD];
/*-------------------------------------------------------------------------------------*\
 * Zuerst wird das JFFS2 gesucht, dann das Squash-FS!
\*-------------------------------------------------------------------------------------*/
static const char *probes[] = { "find_jffs2", "find_squashfs" , NULL };
static unsigned int my_atoi(char *p);
static unsigned int flash_erase_block_size = 0;

static struct physmap_flash_data ur8_flash_data = {
	.width		= 2,
	.parts		= ur8_partitions,
	.nr_parts	= ARRAY_SIZE(ur8_partitions),
    .probes     = probes
};

/* NOTE: CFI probe will correctly detect flash part as 32M, but EMIF
 * limits addresses to 16M, so using addresses past 16M will wrap */
static struct resource ur8_flash_resource[2] = {
    {
        .start		= 0x10000000,
        .end		= 0x10000000 + (8 << 20) - 1,    /* 8 MB */
        .flags		= IORESOURCE_MEM,
    },
    {   /* für ins RAM geladenes Filesystem */
        .start		= 0x00000000,
        .end		= 0x00000000, 
        .flags		= IORESOURCE_MEM,
        /*--- .parent     = &iomem_resource ---*/
    }
};

void ur8_ram_mtd_set_rw(struct device *pdev __attribute__ ((unused)), int mode __attribute__ ((unused)));

static int use_ur8_flash_device = 0;
struct platform_device ur8_flash_device = {
	.name		= "physmap-flash",
	.id		    = 0,
	.dev		= {
		.platform_data	= &ur8_flash_data,
	},
	.num_resources	= 1,
	.resource	= &ur8_flash_resource[0],
};

#ifdef CONFIG_MTD_SPI
struct platform_device ur8_spiflash_device = {
	.name		= "spimap-flash",
	.id		    = 1,
	.dev		= {
		.platform_data	= &ur8_flash_data,
	},
	.num_resources	= 1,
	.resource	= &ur8_flash_resource[0],
};
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct mtd_partition ur8_ram_partitions[3];

static int use_ur8_ram_device = 0;
static struct platdata_mtd_ram ur8_ram_data = {
	.mapname       = "ram-filesystem",
	.bankwidth	   = 4,
	.partitions    = ur8_ram_partitions,
	.nr_partitions = 0, /*--- ARRAY_SIZE(ur8_ram_partitions), ---*/
    .set_rw        = ur8_ram_mtd_set_rw,
    .probes        = probes
};

struct platform_device ur8_ram_device = {
	.name		= "mtd-ram",
	.id		    = -1,
	.dev		= {
		.platform_data	= &ur8_ram_data,
	},
	.num_resources	= 1,
	.resource	= &ur8_flash_resource[1],
};


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ur8_ram_mtd_set_rw(struct device *pdev __attribute__ ((unused)), int mode __attribute__ ((unused))) {
#if defined(UR8_MTD_DEBUG)
    if(mode == PLATRAM_RO) {
        printk(KERN_ERR "[ur8_ram_mtd_set_rw] PLATRAM_RO\n");
    } else if(mode == PLATRAM_RW) {
        printk(KERN_ERR "[ur8_ram_mtd_set_rw] PLATRAM_RW\n");
    }
#endif /*--- #if defined(UR8_MTD_DEBUG) ---*/
}

enum _flash_map_enum {
    MAP_UNKNOWN,
    MAP_RAM,
    MAP_FLASH
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ur8_squashfs_parser_function(struct mtd_info *mtd, struct mtd_partition **p_mtd_pat, unsigned long param) {
    enum _flash_map_enum maptype = MAP_UNKNOWN;
    unsigned count = 1, maxcount = 0;
    
    /*--- printk("[ur8_squashfs_parser_function] mtd_info->name %s mtd_info->index %u param=%lu p_mtd_pat=0x%p\n", mtd->name, mtd->index, param, p_mtd_pat); ---*/
    
    if (!strcmp(mtd->name, "ram-filesystem")) {
        maptype = MAP_RAM;
    } else if (!strcmp(mtd->name, "physmap-flash.0")) {
        maptype = MAP_FLASH;
        flash_erase_block_size = mtd->erasesize;
    } else {
        printk(KERN_WARNING "[ur8_squashfs_parser_function] with unknown mtd type %s\n", mtd->name);
        return 0;
    }

    if(p_mtd_pat) {
        unsigned int magic = 0, readlen = 0;
        char* p;
        loff_t pos, start_offset;

        /*--- if(*p_mtd_pat) ---*/ 
            /*--- printk("[ur8_squashfs_parser_function] *p_mtd_pat->name %s\n", (*p_mtd_pat)->name); ---*/

        switch (maptype) {
            case MAP_FLASH:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ur8_partitions;
                }
                maxcount = ARRAY_SIZE(ur8_partitions);
                break;
            case MAP_RAM:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ur8_ram_partitions;
                }
                maxcount = ARRAY_SIZE(ur8_ram_partitions);
                break;
            default:
                break;
        }

#if defined(UR8_MTD_DEBUG)
        printk("[ur8_squashfs_parser_function] try partition %s (offset 0x%lx len %lu blocksize=%x) read='%pF'\n", 
                (*p_mtd_pat)[count].name,
                (unsigned long)((*p_mtd_pat)[count].offset),
                (unsigned long)((*p_mtd_pat)[count].size),
                mtd->erasesize, mtd->read);
#endif /*--- #if defined(UR8_MTD_DEBUG) ---*/

        start_offset = pos = (*p_mtd_pat)[count].offset;
        while(pos < (*p_mtd_pat)[1].offset + (*p_mtd_pat)[count].size) {
            mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&magic);
#if defined(UR8_MTD_DEBUG)
			if(maptype == MAP_RAM) {
				printk("[ur8_squashfs_parser_function] read %u bytes, magic = 0x%08x index %u pos 0x%x\n", readlen, magic, mtd->index, (unsigned int)pos);
			}
#endif /*--- #if defined(UR8_MTD_DEBUG) ---*/
			if(magic == 0x73717368) {
                /*-------------------------------------------------------------------------------------*\
                 *
                 *    +---+---------------------+-----------------------+--------------------+
                 *    |   |     Kernel          |     SquashFS          |      JFFS2         |
                 *    +---+---------------------+-----------------------+--------------------+
                 *        A                     ^_pos                                        E
                 *
                 *    Zu Beginn ist das Layout obiges:
                 *    start_offset = A
                 *    MTD1 mit Kernel reicht von A bis E
                 *    MTD5 für JFFS2 kann gesetzt sein, wenn JFFS2 Parser vorher schon was gefunden hat
                 *
                 *    Wenn SquashFS gefunden wird, wird MTD1 auf den Kernel verkleinert,
                 *    MTD0 für das FS wird von pos bis E angelegt
                 *    Wenn noch kein MTD5 mit JFFS2 existiert wird dieses innerhalb von MTD0 angelegt
                 *
                \*-------------------------------------------------------------------------------------*/
                (*p_mtd_pat)[0].offset = pos;
                (*p_mtd_pat)[0].size	 = (u_int32_t)start_offset + (u_int32_t)(*p_mtd_pat)[1].size - (u_int32_t)pos;
                (*p_mtd_pat)[0].name	 = "rootfs";
                (*p_mtd_pat)[1].size	 = (u_int32_t)pos - (u_int32_t)start_offset;
                (*p_mtd_pat)[1].name     = "kernel";
#if defined(UR8_MTD_DEBUG)
                printk("[ur8_squashfs_parser_function] magic found @pos 0x%x\n", (unsigned int)pos);
#endif /*--- #if defined(UR8_MTD_DEBUG) ---*/
                if ((maptype == MAP_FLASH) && (memcmp(ur8_partitions[5].name, "jffs2", 4) != 0)) {
                    /* JFFS2 nicht gefunden: Wenn jffs2_size gesetzt ist, ggf. verkleinern */
                    /* sonst anlegen mit der verbleibenden Flash Grösse nach Filesystem % 64k */
                    u_int32_t   jffs2_size, jffs2_start, jffs2_earliest_start;
                    struct squashfs_super_block squashfs_sb;

                    p = prom_getenv((char*)"jffs2_size");
                    /*--- printk("jffs2_size not set\n"); ---*/
                    mtd->read(mtd, (loff_t)pos, sizeof(struct squashfs_super_block), &readlen, (u_char*)&squashfs_sb);
                    jffs2_earliest_start = (u_int32_t)pos + (u_int32_t)squashfs_sb.bytes_used;
                    /*--- printk("squashfs pos: %x\n", (u_int32_t)pos); ---*/
                    /*--- printk("squashfs size: %x\n", (u_int32_t)squashfs_sb.bytes_used); ---*/
                    /*--- printk("jffs2_start (squashfs pos + len) = %x\n", (u_int32_t)jffs2_earliest_start); ---*/
                    if (jffs2_earliest_start & (mtd->erasesize-1)) {
                        /*--- printk("align jffs: start: %x\n", jffs2_earliest_start); ---*/
                        jffs2_earliest_start = (jffs2_earliest_start & ~(mtd->erasesize-1)) + mtd->erasesize;
                    }
                    /*--- printk("jffs2_earliest_start (aligned) = %x\n", jffs2_earliest_start); ---*/
                    jffs2_size = ((*p_mtd_pat)[0].offset + (*p_mtd_pat)[0].size - jffs2_earliest_start) >> 16;
                    /* jffs2_size in 64k Blöcken. Muss ggf. um 1 veringert werden für 128k Block Flash */
                    /*--- printk("jffs2_size = %x\n", jffs2_size); ---*/
                    jffs2_size = jffs2_size & ~((mtd->erasesize / 0x10000)-1);
                    /*--- printk("jffs2_size = %x\n", jffs2_size); ---*/
                    if (jffs2_size < (JFFS2_MIN_SIZE * (mtd->erasesize/0x10000))) {
                        printk(KERN_WARNING "[ur8_squashfs_parser_function]: not enough space for JFFS2!\n");
                    } else {
                        char *p;
                        int flashsize;
                        p = prom_getenv((char*)"flashsize");
                        if(p) {
                            flashsize = my_atoi(p);
                        } else {
                            flashsize = 0x800000;
                        }
#if defined(UR8_MTD_DEBUG)
                        printk("[ur8_squashfs_parser_function] flashsize=%x\n", flashsize);
#endif /*--- #if defined(UR8_MTD_DEBUG) ---*/
                        if ((flashsize <= 0x800000) && (jffs2_size > (JFFS2_MIN_SIZE * (mtd->erasesize/0x10000)))) {
                            /* Für 7270 und W920V mit nur 8MB Flash das JFFS2 auf Minimalgröße halten/verringern */
                            jffs2_start = jffs2_earliest_start + ((jffs2_size - JFFS2_MIN_SIZE) * mtd->erasesize);
                            jffs2_size = JFFS2_MIN_SIZE * (mtd->erasesize/0x10000);
                        } else {
                            /* Für 7270v3 mit vergeigter Produktion (ohne JFFS_SIZE im Urlader-Env.) die Größe
                             * auf 50 begrenzen und nach hinten schieben, damit nicht bei jedem FW Update das
                             * JFFS überschrieben wird */
                            if (jffs2_size > JFFS2_MAX_SIZE) {
                                jffs2_start = jffs2_earliest_start + (jffs2_size - JFFS2_MAX_SIZE) * 0x10000;
                                jffs2_size = JFFS2_MAX_SIZE;
                            } else {
                                jffs2_start = jffs2_earliest_start;
                            }
                        }
                        ur8_partitions[5].offset = jffs2_start;
                        ur8_partitions[5].size   = jffs2_size * 0x10000;
                        ur8_partitions[5].name   = "jffs2";
#if defined(UR8_MTD_DEBUG)
                        printk("[ur8_squashfs_parser_function] jffs2_start@%x size: %d\n", jffs2_start, jffs2_size); 
#endif /*--- #if defined(UR8_MTD_DEBUG) ---*/
                        {
                            struct erase_info instr;
                            int ret;

                            memset(&instr, 0, sizeof(instr));
                            instr.mtd = mtd;
                            instr.addr = jffs2_start;
                            instr.len = jffs2_size * 0x10000;
                            instr.callback = NULL;
                            instr.fail_addr = 0xffffffff;

                            ret = mtd->erase(mtd, &instr);
                            if (ret) {
                                printk(KERN_ERR "jffs mtd erase failed %d\n", ret);
                            }
                        }
                    }
                }
                return maxcount;
            }
            pos += 256;
        }
        
    }
    return 0;
}

#define JFFS_NODES ( JFFS2_NODETYPE_DIRENT | JFFS2_NODETYPE_INODE | JFFS2_NODETYPE_CLEANMARKER | JFFS2_NODETYPE_PADDING | JFFS2_NODETYPE_SUMMARY | JFFS2_NODETYPE_XATTR | JFFS2_NODETYPE_XREF) 
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ur8_jffs2_parser_function(struct mtd_info *mtd, struct mtd_partition **p_mtd_pat, unsigned long param) {
    enum _flash_map_enum maptype = MAP_UNKNOWN;
    unsigned count = 1, maxcount = 0;
    /*--- static unsigned int erasesize = 0; ---*/
    
#if defined(UR8_MTD_DEBUG)
    printk("[ur8_jffs2_parser_function] mtd_info->name %s mtd_info->index %u param=%lu p_mtd_pat=0x%p\n", mtd->name, mtd->index, param, p_mtd_pat);
#endif /*--- #if defined(UR8_MTD_DEBUG) ---*/
    
    if (!strcmp(mtd->name, "ram-filesystem")) {
        maptype = MAP_RAM;
        /*--- if(erasesize) { ---*/
            /*--- printk(KERN_WARNING "[ur8_jffs2_parser_function] set mtd-ram erase size from 0x%x to 0x%x\n", mtd->erasesize, erasesize); ---*/
            /*--- mtd->erasesize = erasesize; ---*/
        /*--- } ---*/
    } else if (!strcmp(mtd->name, "physmap-flash.0")) {
        /*--- erasesize = mtd->erasesize; ---*/
        maptype = MAP_FLASH;
    } else {
        printk(KERN_WARNING "[ur8_jffs2_parser_function] with unknown mtd type %s\n", mtd->name);
        return 0;
    }

    if(p_mtd_pat) {
        unsigned int magic = 0, readlen = 0;
        loff_t pos;
#if defined(UR8_MTD_DEBUG)
        if(*p_mtd_pat) 
            printk("[ur8_jffs2_parser_function] *p_mtd_pat->name %s\n", (*p_mtd_pat)->name);
#endif /*--- #if defined(UR8_MTD_DEBUG) ---*/

        switch (maptype) {
            case MAP_FLASH:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ur8_partitions;
                }
                maxcount = ARRAY_SIZE(ur8_partitions);
                break;
            case MAP_RAM:
                count = 2;
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ur8_ram_partitions;
                }
                maxcount = ARRAY_SIZE(ur8_ram_partitions);
                /*--- return 0; ---*/   /* nicht im RAM suchen */
                break;
            default:
                break;
        }

#if defined(UR8_MTD_DEBUG)
        printk("[ur8_jffs2_parser_function] try partition %s (offset 0x%lx len %lu)\n", 
                (*p_mtd_pat)[count].name,
                (unsigned long)((*p_mtd_pat)[count].offset),
                (unsigned long)((*p_mtd_pat)[count].size));
#endif /*--- #if defined(UR8_MTD_DEBUG) ---*/

        pos = (*p_mtd_pat)[count].offset;
        while(pos < (*p_mtd_pat)[count].offset + (*p_mtd_pat)[count].size) {
            mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&magic);
            /*--- printk("[ur8_jffs2_parser_function] read %u bytes, magic = 0x%08x index %u pos 0x%x\n", readlen, magic, mtd->index, pos); ---*/
            if ((((magic >> 16) & ~JFFS_NODES) == 0) && ((magic & 0xFFFF) == JFFS2_MAGIC_BITMASK)) {
                switch (maptype) {
                    case MAP_FLASH:
                        (*p_mtd_pat)[5].size	 = (*p_mtd_pat)[1].offset + (*p_mtd_pat)[1].size - pos;
                        (*p_mtd_pat)[5].offset   = pos;
                        (*p_mtd_pat)[5].name	 = "jffs2";
                        /*--- printk("mtd1: size %d\n", (*p_mtd_pat)[1].size); ---*/
                        /*--- printk("[ur8_jffs2_parser_function] magic %04x found @pos 0x%x, size %ld\n", magic, (unsigned int)pos, (unsigned long)((*p_mtd_pat)[5].size)); ---*/
                        break;
                    case MAP_RAM:
                        (*p_mtd_pat)[2].size	 = (*p_mtd_pat)[count].offset + (*p_mtd_pat)[count].size - pos;
                        (*p_mtd_pat)[2].offset   = pos;
                        (*p_mtd_pat)[2].name	 = "ram-jffs2";
                        /*--- printk("mtd1: size %d\n", (*p_mtd_pat)[1].size); ---*/
                        /*--- printk("[ur8_jffs2_parser_function] magic %04x found @pos 0x%x, size %ld\n", magic, (unsigned int)pos, (unsigned long)((*p_mtd_pat)[2].size)); ---*/
                        break;
                    default:
                        break;
                }
                return 0;
            }
            pos += mtd->erasesize;
        }
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int get_erase_block_size_on_ram_device(struct mtd_info *mtd) {
    unsigned int readlen = 0;
    unsigned int pos = 0;
    unsigned int value1, value2;

    mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&value1);
    if(readlen != sizeof(unsigned int))
        return 0;
    /*--- printk("[get_erase_block_size_on_ram_device] name=%s pos=0x%x value=0x%x\n", mtd->name, pos, value1); ---*/

    pos += 0x10000;
    mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&value2);
    if(readlen != sizeof(unsigned int))
        return 0;
    /*--- printk("[get_erase_block_size_on_ram_device] name=%s pos=0x%x value2=0x%x\n", mtd->name, pos, value2); ---*/

    if(value1 == value2) {
        pos += 0x10000;
        mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&value2);
        if(readlen != sizeof(unsigned int))
            return 0;
        /*--- printk("[get_erase_block_size_on_ram_device] name=%s pos=0x%x value2=0x%x (check)\n", mtd->name, pos, value2); ---*/

        if(value1 == value2) {
#if defined(UR8_MTD_DEBUG)
            printk("[get_erase_block_size_on_ram_device] eraseblocksize=0x10000\n");
#endif /*--- #if defined(UR8_MTD_DEBUG) ---*/
            return 0x10000;
        }
        return 0;
    }

    pos += 0x10000;
    mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&value2);
    if(readlen != sizeof(unsigned int))
        return 0;
    /*--- printk("[get_erase_block_size_on_ram_device] name=%s pos=0x%x value2=0x%x\n", mtd->name, pos, value2); ---*/

    if(value1 == value2) {
#if defined(UR8_MTD_DEBUG)
        printk("[get_erase_block_size_on_ram_device] eraseblocksize=0x20000\n");
#endif /*--- #if defined(UR8_MTD_DEBUG) ---*/
        return 0x20000;
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct mtd_part_parser ur8_squashfs_parser = {
	.name     = "find_squashfs",
	.parse_fn = ur8_squashfs_parser_function
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct mtd_part_parser ur8_jffs2_parser = {
	.name     = "find_jffs2",
	.parse_fn = ur8_jffs2_parser_function
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int my_atoi(char *p) {
    unsigned int base, zahl;
    /*--- printk("[my_atoi] %s -> ", p); ---*/
    if(p[0] == '0') {
        if((p[1] == 'x') || (p[1] == 'X')) {
            base = 16;
            p += 2;
        } else {
            p += 1;
            base = 8;
        }
    } else {
        base = 10;
    }
    zahl = 0;
    while(*p) {
        if((*p >= '0') && (*p <= '9')) {
            zahl *= base;
            zahl += *p - '0';
        } else if((*p >= 'A') && (*p <= 'F')) {
            zahl *= base;
            zahl += *p - 'A' + 10;
        } else if((*p >= 'a') && (*p <= 'f')) {
            zahl *= base;
            zahl += *p - 'a' + 10;
        } else {
            break;
        }
        p++;
    }
    /*--- printk(" %u(0x%x)\n", zahl, zahl); ---*/
    return zahl;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ur8_mtd_add_notifier(struct mtd_info *mtd) {
    /*--- printk("[ur8_mtd_add_notifier] name %s\n", mtd->name); ---*/
    if(!strcmp(mtd->name, "rootfs")) {
        extern struct mtd_info *mtd_table[MAX_MTD_DEVICES];
        unsigned int i;
        /*--- printk("[ur8_mtd_add_notifier] use %s\n", mtd->name); ---*/
        for(i = 0 ; i < MAX_MTD_DEVICES ; i++) {
            if(mtd_table[i] == mtd) {
                extern int __init root_dev_setup(char *line);
                static char root_device[64];
                char *cl = prom_getcmdline();
                if (strstr(cl, "root=") == NULL) {
                    strcpy(root_device, "/dev/mtdblock_");
                    *strchr(root_device, '_') = '0' + i;
                    /*--- printk("[ur8_mtd_add_notifier] root device: %s (%s)\n", root_device, mtd_table[i]->name); ---*/
                    root_dev_setup(root_device);
                /*--- } else { ---*/
                    /*--- printk("[ur8_mtd_add_notifier] root device: skipped (is set in cmdline)\n"); ---*/
                }
                return;
            /*--- } else { ---*/
                /*--- printk("[ur8_mtd_add_notifier] %s is not my root device\n", ---*/ 
                        /*--- mtd_table[i] ? mtd_table[i]->name : "<NULL>"); ---*/
            }
        }
    } else if(!strcmp(mtd->name, "ram-jffs2")) {
        mtd->erasesize = get_erase_block_size_on_ram_device(mtd);
        if(mtd->erasesize == 0)
            mtd->erasesize = flash_erase_block_size;
        /*--- printk("[ur8_mtd_add_notifier] %s: set erasesize to 0x%x\n", mtd->name, flash_erase_block_size); ---*/
    } else if(!strcmp(mtd->name, "ram-filesystem")) {
        mtd->erasesize = get_erase_block_size_on_ram_device(mtd);
        if(mtd->erasesize == 0)
            mtd->erasesize = flash_erase_block_size;
        /*--- printk("[ur8_mtd_add_notifier] %s: set erasesize to 0x%x\n", mtd->name, flash_erase_block_size); ---*/
    /*--- } else { ---*/
        /*--- printk("[ur8_mtd_add_notifier] skip %s\n", mtd->name); ---*/
    }
}

void ur8_mtd_rm_notifier(struct mtd_info *mtd __attribute__ ((unused))) {
    /*--- printk("[ur8_mtd_rm_notifier] ignore %s\n", mtd->name); ---*/
}

struct mtd_notifier ur8_mtd_notifier = {
    add: ur8_mtd_add_notifier,
    remove: ur8_mtd_rm_notifier
};

/*------------------------------------------------------------------------------------------*\
 * Parst die erste Größe in einem Größenangaben String vom Urlader
 * Format der Größenangaben: xxx_size=<nn>{,KB,MB}
\*------------------------------------------------------------------------------------------*/
unsigned long long parse_mtd_size(char *p) {
    unsigned long long size;

    if((p[0] == '0') && (p[1] == 'x')) {
        size = simple_strtoul(p, NULL, 16);
    } else {
        size = simple_strtoul(p, NULL, 10);
    }
    
    p = strchr(p, 'B');
    if(p) {
        /*--- Die Größe enthält mindestens eine KB Angabe ---*/
        size *= 1024;
        if(p[-1] == 'M')  {
            size *= 1024;
        }
    }

    return size; 
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init mtdflash_setup(char *p);



int __init ur8_mtd_init(void) {

	/*--- printk("[%s]\n", __FUNCTION__); ---*/
    mtdflash_setup(prom_getenv((char*)"flashsize"));
    register_mtd_parser(&ur8_squashfs_parser);
    register_mtd_parser(&ur8_jffs2_parser);
    register_mtd_user(&ur8_mtd_notifier);

	if ( use_ur8_flash_device ) {
		/*--- printk("[%s] flash: type=%ld, start=%#x, end=%#x \n", __FUNCTION__, ur8_flash_device.resource->flags, ur8_flash_device.resource->start,  ur8_flash_device.resource->end ); ---*/
#ifdef CONFIG_MTD_SPI
		platform_device_register( &ur8_spiflash_device );
#else
		platform_device_register( &ur8_flash_device );
#endif
	}

	if ( use_ur8_ram_device ) {
		/*--- printk("[%s] ram: type=%ld, start=%#x, end=%#x \n", __FUNCTION__, ur8_ram_device.resource->flags, ur8_ram_device.resource->start,  ur8_ram_device.resource->end ); ---*/
		platform_device_register( &ur8_ram_device );
	} 
    return 0;
}
subsys_initcall(ur8_mtd_init);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init mtdflash_setup(char *p) {
    unsigned long flashsize;
    unsigned long mtd_start, mtd_end;
    unsigned long flashoffset = 0;

    if(!p)
        return 0;

    ur8_partitions[0].name = (char *)"filesystem";
    ur8_partitions[1].name = (char *)"kernel";
    ur8_partitions[2].name = (char *)"urlader";
    ur8_partitions[3].name = (char *)"tffs (1)";
    ur8_partitions[4].name = (char *)"tffs (2)";
    ur8_partitions[5].name = (char *)"reserved";

    flashsize = parse_mtd_size(p);

    /*--------------------------------------------------------------------------------------*\
     * Größen ermitteln
    \*--------------------------------------------------------------------------------------*/
    p = prom_getenv("mtd2");
    if(p) {
        DEBUG_MTD("mtd2 = %s", p);
        mtd_start  = CPHYSADDR(CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16)));
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = CPHYSADDR(CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16)));
            flashoffset = mtd_start;
            ur8_partitions[2].size       = mtd_end - mtd_start;
            ur8_partitions[2].offset     = mtd_start - flashoffset;
            ur8_flash_resource[0].start = mtd_start;
            ur8_flash_resource[0].end   = mtd_start + flashsize;
        }
    }
    p = prom_getenv("mtd0");
    if(p) {
        DEBUG_MTD("mtd0 = %s", p);
        mtd_start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
            ur8_partitions[0].size       = mtd_end - mtd_start;
            ur8_partitions[0].offset     = mtd_start - flashoffset;
        }
    }
    p = prom_getenv("mtd1");
    if(p) {
        DEBUG_MTD("mtd1 = %s", p);
        mtd_start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
            ur8_partitions[1].size       = mtd_end - mtd_start;
            ur8_partitions[1].offset     = mtd_start - flashoffset;
        }
    }
    p = prom_getenv("mtd3");
    if(p) {
        DEBUG_MTD("mtd3 = %s", p);
        mtd_start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
            ur8_partitions[3].size       = mtd_end - mtd_start;
            ur8_partitions[3].offset     = mtd_start - flashoffset;
        }
    }
    p = prom_getenv("mtd4");
    if(p) {
        DEBUG_MTD("mtd4 = %s", p);
        mtd_start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
            ur8_partitions[4].size       = mtd_end - mtd_start;
            ur8_partitions[4].offset     = mtd_start - flashoffset;
        }
    }
    p = prom_getenv("mtd5");
    if(p) {
        DEBUG_MTD("mtd5 = %s", p);
        mtd_start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
            ur8_partitions[5].size       = mtd_end - mtd_start;
            ur8_partitions[5].offset     = mtd_start - flashoffset;
        }
    }
	use_ur8_flash_device = 1;
    return 0;
}
/*--- __setup("mtdflash=", mtdflash_setup); ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init mtdram_setup(char *p) {
    /*--- printk("[mtdram_setup] str=\"%s\"\n", p); ---*/
    if(p) {
        /*--- printk("[ur8_mtd_init] mtdram1 %s\n", p); ---*/
        ur8_flash_resource[1].start  = my_atoi(p);
        ur8_flash_resource[1].start &= ~0xE0000000;
        ur8_flash_resource[1].flags  = IORESOURCE_MEM,
        p = strchr(p, ',');
        if(p) {
            p++;
            ur8_flash_resource[1].end  = my_atoi(p);
            ur8_flash_resource[1].end &= ~0xE0000000;
            ur8_flash_resource[1].end -= 1;
        } else {
            ur8_flash_resource[1].start = 0;
        }
        /*--- printk("[ur8_mtd_init] mtdram1 0x%08x - 0x%08x\n", ur8_flash_resource[1].start, ur8_flash_resource[1].end ); ---*/
        ur8_ram_partitions[0].name		 = "filesystem";
        ur8_ram_partitions[0].offset	 = 0;
        ur8_ram_partitions[0].size		 = ur8_flash_resource[1].end - ur8_flash_resource[1].start + 1;
        ur8_ram_partitions[0].mask_flags = MTD_ROM;
        ur8_ram_partitions[1].name		 = "unused";
        ur8_ram_partitions[1].offset	 = 0;
        ur8_ram_partitions[1].size		 = ur8_flash_resource[1].end - ur8_flash_resource[1].start + 1;
        ur8_ram_partitions[1].mask_flags = MTD_ROM;
        ur8_ram_partitions[2].name		 = "extra";
        ur8_ram_partitions[2].offset	 = 0;
        ur8_ram_partitions[2].size		 = ur8_flash_resource[1].end - ur8_flash_resource[1].start + 1;
        ur8_ram_partitions[2].mask_flags = MTD_ROM;
    }


	use_ur8_ram_device = 1;
    return 0;
}
__setup("mtdram1=", mtdram_setup);

#endif

