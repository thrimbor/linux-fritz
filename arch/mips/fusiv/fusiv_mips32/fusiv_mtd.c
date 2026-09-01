/*
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
#include <linux/root_dev.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/env.h>

#include <asm/setup.h>
#include <asm/io.h>

#if defined(CONFIG_MTD_PHYSMAP) || defined(CONFIG_MTD_PHYSMAP_MODULE)
#define DO_MTD

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/plat-ram.h>
#include <linux/squashfs_fs.h>
#include <linux/jffs2.h>

#include <linux/mtd/nand.h>
#include <linux/mtd/nand-direct-avm.h>
#include <asm/mach_avm.h>

#define FUSIV_MTD_DEBUG

#if defined(FUSIV_MTD_DEBUG)
    #define DEBUG_MTD(fmt, arg...) printk(KERN_ERR "[%d:%s/%d] " fmt "\n", smp_processor_id(), __func__, __LINE__, ##arg);
#else
    #define DEBUG_MTD(fmt, arg...)
#endif

#define FUSIV_MTD_NAND_PARTS        1
#define FUSIV_MTD_NOR_PARTS         6
#define FUSIV_MTD_JFFS2_MIN_SIZE  6
#define FUSIV_MTD_JFFS2_MAX_SIZE  50
#define FUSIV_MTD_SPI_PARTS   0

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _flash_map_enum {
    MAP_UNKNOWN,
    MAP_RAM,
    MAP_NOR_FLASH,
    MAP_NAND_FLASH,
    MAP_SPI_FLASH
};

/*-------------------------------------------------------------------------------------*\
 * Zuerst wird das JFFS2 gesucht, dann das Squash-FS!
\*-------------------------------------------------------------------------------------*/
/*--- static const char *probes[] = { NULL }; ---*/
static const char *probes[] = { "find_jffs2", "find_squashfs", NULL };

void fusiv_ram_mtd_set_rw(struct device *pdev, int);
extern int __init root_dev_setup(char *line);
static unsigned int flash_erase_block_size = (1U << 16);
static unsigned int fusiv_nor_flashsize;
extern unsigned long g_fusiv_nor_flash_start;
extern unsigned long g_fusiv_nor_flash_size;


/*------------------------------------------------------------------------------------------*\
 * NOR
\*------------------------------------------------------------------------------------------*/
struct mtd_partition fusiv_nor_partitions[FUSIV_MTD_NOR_PARTS];

static struct resource fusiv_nor_resource[] = {
    {
        .start		= 0,
        .end		= 0 + (128),    /* 128 MB */
        .flags		= IORESOURCE_MEM,
        .parent     = &iomem_resource
    },
};

static struct physmap_flash_data fusiv_nor_data = {
	.width		= 2,
	.parts		= fusiv_nor_partitions,
	.nr_parts	= ARRAY_SIZE(fusiv_nor_partitions),
    .probes     = probes
};

struct platform_device fusiv_nor_device[] = {
    {
        .name		= "physmap-flash",
        .id		    = -1,
        .dev		= {
            .platform_data	= &fusiv_nor_data,
        },
        .num_resources	= 1,
        .resource	= &fusiv_nor_resource[0],
    }
};


/*------------------------------------------------------------------------------------------*\
 * NAND
\*------------------------------------------------------------------------------------------*/
struct mtd_partition fusiv_nand_partitions[FUSIV_MTD_NAND_PARTS];

static struct resource fusiv_nand_resource[] = {
    [0] = {
            .start  = CONFIG_MTD_NAND_DIRECT_AVM_DATA,
            .end    = CONFIG_MTD_NAND_DIRECT_AVM_DATA + (16<<20)-1,
            .flags  = IORESOURCE_MEM,
        },
#if defined(CONFIG_MTD_NAND_DIRECT_AVM_ADDR)
    [1] = {
            .start = CONFIG_MTD_NAND_DIRECT_AVM_CONTROL,
            .end = CONFIG_MTD_NAND_DIRECT_AVM_CONTROL + (16 << 20) - 1,
            .flags = IORESOURCE_MEM,
        },
#endif /*--- #if defined(CONFIG_MTD_NAND_DIRECT_AVM_ADDR) ---*/
};

static struct direct_avm_nand_platdata direct_avm_nand_platform_data = {
#if defined(CONFIG_MTD_NAND_DIRECT_AVM_ADDR)
    .addr_nce = (1 << 16),
    .addr_nwp = (1 << 18),
    .addr_cle = (1 << 17),
    .addr_ale = (1 << 15),
#endif /*--- #if defined(CONFIG_MTD_NAND_DIRECT_AVM_ADDR) ---*/
#if defined(CONFIG_MTD_NAND_DIRECT_AVM_GPIO) || defined(CONFIG_MTD_NAND_COMPLETE_AVM)
    .gpio_rdy0 = CONFIG_MTD_NAND_DIRECT_AVM_READY0,
    .gpio_rdy1 = CONFIG_MTD_NAND_DIRECT_AVM_READY1,
    .gpio_nwp  = CONFIG_MTD_NAND_DIRECT_AVM_NWP,
    .gpio_nce0 = CONFIG_MTD_NAND_DIRECT_AVM_NCE0,
    .gpio_nce1 = CONFIG_MTD_NAND_DIRECT_AVM_NCE1,
    .gpio_cle  = CONFIG_MTD_NAND_DIRECT_AVM_CLE,
    .gpio_ale  = CONFIG_MTD_NAND_DIRECT_AVM_ALE,
#endif /*--- #if defined(CONFIG_MTD_NAND_DIRECT_AVM_GPIO) || defined(CONFIG_MTD_NAND_COMPLETE_AVM) ---*/
    /*--- .adjust_parts = direct_avm_nand_adjust_partitions, ---*/ /*--- => NOP aus platform.c ---*/
    .parts      = fusiv_nand_partitions,
    .num_parts  = ARRAY_SIZE(fusiv_nand_partitions),
    .options    = 0, /*--- NAND_USE_FLASH_BBT | NAND_BBT_SCANALLPAGES | NAND_BBT_SCANEMPTY, ---*/
    .chip_delay = 0
};

struct platform_device fusiv_nand_device[] = {
    {
        .name = "direct-avm-nand",
        .id   = -1,
        .dev  = {
                .platform_data  = &direct_avm_nand_platform_data
            },
        .num_resources = ARRAY_SIZE(fusiv_nand_resource),
        .resource = &fusiv_nand_resource[0],
    }
};

#if defined (FUSIV_MTD_SPI_PARTS) && (FUSIV_MTD_SPI_PARTS > 0 )
/*------------------------------------------------------------------------------------------*\
 * SPI
\*------------------------------------------------------------------------------------------*/
struct mtd_partition fusiv_spi_partitions[FUSIV_MTD_SPI_PARTS];

/* NOTE: CFI probe will correctly detect flash part as 32M, but EMIF
 * limits addresses to 16M, so using addresses past 16M will wrap */
static struct resource fusiv_spi_resource[] = {
    {
        .start		= 0,
        .end		= 0 + (256 << 10),    /* 256 KB */
        .flags		= IORESOURCE_MEM,
        .parent     = &sflash_resource
    },
};

static struct physmap_flash_data fusiv_spi_data = {
	.width		= 2,
	.parts		= fusiv_spi_partitions,
	.nr_parts	= ARRAY_SIZE(fusiv_spi_partitions),
    .probes     = NULL
};

struct platform_device fusiv_spi_device[] = {
    {
        .name		= "macronix",
        .id		    = 0,
        .dev		= {
            .platform_data	= &fusiv_spi_data,
        },
        .num_resources	= 1,
        .resource	= &fusiv_spi_resource[0],
    }
};
#endif /*--- #if defined (FUSIV_MTD_SPI_PARTS) && (FUSIV_MTD_SPI_PARTS > 0 ) ---*/

/*------------------------------------------------------------------------------------------*\
 * RAM
\*------------------------------------------------------------------------------------------*/
static struct mtd_partition fusiv_ram_partitions[1];

static struct resource fusiv_ram_resource[] = {
    {   /* für ins RAM geladenes Filesystem */
        .start		= 0,
        .end		= 0 + (32 << 20),
        .flags		= IORESOURCE_MEM,
    }
};

static struct platdata_mtd_ram fusiv_ram_data = {
	.mapname       = "ram-filesystem",
	.bankwidth	   = 4,
	.partitions    = fusiv_ram_partitions,
	.nr_partitions = ARRAY_SIZE(fusiv_ram_partitions),
    .set_rw        = fusiv_ram_mtd_set_rw,
    .probes        = probes
};

struct platform_device fusiv_ram_device = {
	.name		= "mtd-ram",
	.id		    = -1,
	.dev		= {
		.platform_data	= &fusiv_ram_data,
	},
	.num_resources	= 1,
	.resource	= &fusiv_ram_resource[0],
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int fusiv_squashfs_parser_function(struct mtd_info *mtd, struct mtd_partition **p_mtd_pat, unsigned long param) {
    enum _flash_map_enum maptype = MAP_UNKNOWN;
    unsigned count = 1, maxcount = 0;
    
    printk("[fusiv_squashfs_parser_function] mtd_info->name %s mtd_info->index %u param=%lu p_mtd_pat=0x%p\n", mtd->name, mtd->index, param, p_mtd_pat);
    
    if (!strcmp(mtd->name, "mtd-ram")) {
        maptype = MAP_RAM;
    } else if (!strcmp(mtd->name, "physmap-flash")) {
        maptype = MAP_NOR_FLASH;
        flash_erase_block_size = mtd->erasesize;
    } else {
        printk(KERN_WARNING "[fusiv_squashfs_parser_function] with unknown mtd type %s\n", mtd->name);
        return 0;
    }

    if(p_mtd_pat) {
        unsigned int magic = 0, readlen = 0;
        char* p;
        loff_t pos, start_offset;

        if(*p_mtd_pat) 
            printk("[fusiv_squashfs_parser_function] *p_mtd_pat->name %s\n", (*p_mtd_pat)->name);

        switch (maptype) {
            case MAP_NOR_FLASH:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = fusiv_nor_partitions;
                }
                maxcount = ARRAY_SIZE(fusiv_nor_partitions);
                break;
            case MAP_RAM:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = fusiv_ram_partitions;
                }
                maxcount = ARRAY_SIZE(fusiv_ram_partitions);
                break;
            default:
                break;
        }

        printk("[fusiv_squashfs_parser_function] try partition %s (offset 0x%x len %u blocksize=%x)\n", 
                (*p_mtd_pat)[count].name,
                (*p_mtd_pat)[count].offset,
                (*p_mtd_pat)[count].size,
                mtd->erasesize);

        start_offset = pos = (*p_mtd_pat)[count].offset;
        while(pos < (*p_mtd_pat)[1].offset + (*p_mtd_pat)[count].size) {
            mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&magic);
            /*--- printk("[fusiv_squashfs_parser_function] read %u bytes, magic = 0x%08x index %u pos 0x%x\n", readlen, magic, mtd->index, (unsigned int)pos); ---*/
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
                printk("[fusiv_squashfs_parser_function] magic found @pos 0x%x\n", (unsigned int)pos);
                if ((maptype == MAP_NOR_FLASH) && (memcmp(fusiv_nor_partitions[5].name, "jffs2", 4) != 0)) {
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
                    if (jffs2_size < (FUSIV_MTD_JFFS2_MIN_SIZE * (mtd->erasesize/0x10000))) {
                        printk(KERN_WARNING "[fusiv_squashfs_parser_function]: not enough space for JFFS2!\n");
                    } else {
                        printk("[fusiv_squashfs_parser_function] flashsize=%x\n", fusiv_nor_flashsize);
                        if ((fusiv_nor_flashsize <= 0x800000) && (jffs2_size > (FUSIV_MTD_JFFS2_MIN_SIZE * (mtd->erasesize/0x10000)))) {
                            /* Für 7270 und W920V mit nur 8MB Flash das JFFS2 auf Minimalgröße halten/verringern */
                            jffs2_start = jffs2_earliest_start + ((jffs2_size - FUSIV_MTD_JFFS2_MIN_SIZE) * mtd->erasesize);
                            jffs2_size = FUSIV_MTD_JFFS2_MIN_SIZE * (mtd->erasesize/0x10000);
                        } else {
                            /* Für 7270v3 mit vergeigter Produktion (ohne JFFS_SIZE im Urlader-Env.) die Größe
                             * auf 50 begrenzen und nach hinten schieben, damit nicht bei jedem FW Update das
                             * JFFS überschrieben wird */
                            if (jffs2_size > FUSIV_MTD_JFFS2_MAX_SIZE) {
                                jffs2_start = jffs2_earliest_start + (jffs2_size - FUSIV_MTD_JFFS2_MAX_SIZE) * 0x10000;
                                jffs2_size = FUSIV_MTD_JFFS2_MAX_SIZE;
                            } else {
                                jffs2_start = jffs2_earliest_start;
                            }
                        }
                        fusiv_nor_partitions[5].offset = jffs2_start;
                        fusiv_nor_partitions[5].size   = jffs2_size * 0x10000;
                        fusiv_nor_partitions[5].name   = "jffs2";
                        printk(KERN_ERR "[fusiv_squashfs_parser_function] jffs2_start@%x size: %d\n", jffs2_start, jffs2_size); 
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
static int fusiv_jffs2_parser_function(struct mtd_info *mtd, struct mtd_partition **p_mtd_pat, unsigned long param) {
    enum _flash_map_enum maptype = MAP_UNKNOWN;
    unsigned int count = 1;
    /*--- static unsigned int erasesize = 0; ---*/
    
    printk("[%s] mtd_info->name %s mtd_info->index %u param=%lu p_mtd_pat=0x%p\n", __func__, mtd->name, mtd->index, param, p_mtd_pat);

    if (!strcmp(mtd->name, "ram-filesystem")) {
        maptype = MAP_RAM;
        /*--- if(erasesize) { ---*/
            /*--- printk(KERN_WARNING "[%s] set mtd-ram erase size from 0x%x to 0x%x\n", mtd->erasesize, erasesize); ---*/
            /*--- mtd->erasesize = erasesize; ---*/
        /*--- } ---*/
    } else if (!strcmp(mtd->name, "physmap-flash")) {
        /*--- erasesize = mtd->erasesize; ---*/
        maptype = MAP_NOR_FLASH;
    } else {
        printk(KERN_WARNING "[%s] with unknown mtd type %s\n", __func__, mtd->name);
        return 0;
    }

    if(p_mtd_pat) {
        unsigned int magic = 0, readlen = 0;
        loff_t pos;
        if(*p_mtd_pat) 
            printk("[%s] *p_mtd_pat->name %s\n", __func__, (*p_mtd_pat)->name);

        switch (maptype) {
            case MAP_NOR_FLASH:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = fusiv_nor_partitions;
                }
                break;
            case MAP_RAM:
                count = 2;
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = fusiv_ram_partitions;
                }
                /*--- return 0; ---*/   /* nicht im RAM suchen */
                break;
            default:
                break;
        }

        printk("[%s] try partition %s (offset 0x%x len %u)\n", 
                __func__,
                (*p_mtd_pat)[count].name,
                (*p_mtd_pat)[count].offset,
                (*p_mtd_pat)[count].size);

        pos = (*p_mtd_pat)[count].offset;
        while(pos < (*p_mtd_pat)[count].offset + (*p_mtd_pat)[count].size) {
            mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&magic);
            /*--- printk("[%s] read %u bytes, magic = 0x%08x index %u pos 0x%x\n", __func__, readlen, magic, mtd->index, pos); ---*/
#ifdef __LITTLE_ENDIAN
            if ((((magic >> 16) & ~JFFS_NODES) == 0) && ((magic & 0xFFFF) == JFFS2_MAGIC_BITMASK)) {
#else
            if (((magic >> 16) == JFFS2_MAGIC_BITMASK) && (((magic & 0xFFFF) & ~JFFS_NODES) == 0)) {
#endif
                switch (maptype) {
                    case MAP_NOR_FLASH:
                        (*p_mtd_pat)[5].size	 = (*p_mtd_pat)[1].offset + (*p_mtd_pat)[1].size - pos;
                        (*p_mtd_pat)[5].offset   = pos;
                        (*p_mtd_pat)[5].name	 = "jffs2";
                        /*--- printk("mtd1: size %d\n", (*p_mtd_pat)[1].size); ---*/
                        printk("[%s] magic %04x found @pos 0x%x, size %d\n", __func__, magic, (unsigned int)pos, (*p_mtd_pat)[5].size);
                        break;
                    case MAP_RAM:
                        (*p_mtd_pat)[2].size	 = (*p_mtd_pat)[count].offset + (*p_mtd_pat)[count].size - pos;
                        (*p_mtd_pat)[2].offset   = pos;
                        (*p_mtd_pat)[2].name	 = "ram-jffs2";
                        /*--- printk("mtd1: size %d\n", (*p_mtd_pat)[1].size); ---*/
                        printk("[%s] magic %04x found @pos 0x%x, size %d\n", __func__, magic, (unsigned int)pos, (*p_mtd_pat)[2].size);
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
struct mtd_part_parser fusiv_squashfs_parser = {
	.name     = "find_squashfs",
	.parse_fn = fusiv_squashfs_parser_function
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct mtd_part_parser fusiv_jffs2_parser = {
	.name     = "find_jffs2",
	.parse_fn = fusiv_jffs2_parser_function
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct platform_device *fusiv_platform_devices[20];
unsigned int fusiv_platform_devices_count = 0;

void add_to_platform_device_list(struct platform_device *device){ 
    printk(KERN_INFO "[FUSIV] add %s to the platform device list\n", device->name);
    fusiv_platform_devices[fusiv_platform_devices_count++] = device;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void fusiv_init_platform_devices(void) {
    printk(KERN_INFO "[FUSIV] register %d platform device(s)\n", fusiv_platform_devices_count);
	platform_add_devices(fusiv_platform_devices, fusiv_platform_devices_count);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline unsigned int get_flash_base(unsigned int flash_size) {
    return 0x48000000; 
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void fusiv_ram_mtd_set_rw(struct device *pdev, int mode) {
    if(mode == PLATRAM_RO) {
        DEBUG_MTD("PLATRAM_RO");
    } else if(mode == PLATRAM_RW) {
        DEBUG_MTD("PLATRAM_RW");
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int get_erase_block_size_on_ram_device(struct mtd_info *mtd) {
    unsigned int readlen = 0;
    loff_t pos = 0;
    unsigned int value1, value2;

    mtd->read(mtd, pos, sizeof(unsigned int), &readlen, (u_char*)&value1);
    if(readlen != sizeof(unsigned int))
        return 0;
    /*--- DEBUG_MTD("name=%s pos=0x%x value=0x%x" , mtd->name, pos, value1); ---*/

    pos += 0x10000ULL;
    mtd->read(mtd, pos, sizeof(unsigned int), &readlen, (u_char*)&value2);
    if(readlen != sizeof(unsigned int))
        return 0;
    /*--- DEBUG_MTD("name=%s pos=0x%x value2=0x%x" , mtd->name, pos, value2); ---*/

    if(value1 == value2) {
        pos += 0x10000ULL;
        mtd->read(mtd, pos, sizeof(unsigned int), &readlen, (u_char*)&value2);
        if(readlen != sizeof(unsigned int))
            return 0;
        /*--- DEBUG_MTD("name=%s pos=0x%x value2=0x%x (check)" , mtd->name, pos, value2); ---*/

        if(value1 == value2) {
            DEBUG_MTD("eraseblocksize=0x10000");
            return 0x10000;
        }
        return 0;
    }

    pos += 0x10000ULL;
    mtd->read(mtd, pos, sizeof(unsigned int), &readlen, (u_char*)&value2);
    if(readlen != sizeof(unsigned int))
        return 0;
    DEBUG_MTD("name=%s pos=0x%Lx value2=0x%x", mtd->name, pos, value2);

    if(value1 == value2) {
        DEBUG_MTD("eraseblocksize=0x20000" );
        return 0x20000;
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int get_partition_index(struct mtd_info *mtd) {

    extern struct mtd_info *mtd_table[MAX_MTD_DEVICES];
    unsigned int i;

    for(i = 0 ; i < MAX_MTD_DEVICES ; i++) {
        if(mtd_table[i] == mtd) {
            return i;
        }
    }
    return -1;

} 

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern struct mtd_info *mtd_table[MAX_MTD_DEVICES];
extern int tffs_mtd[2];
static int found_rootfs_ram = 0;
char *str_rootfs[] = { "rootfs_ram", "rootfs", "filesystem" };
struct mtd_info *fusiv_urlader_mtd;

void fusiv_mtd_add_notifier(struct mtd_info *mtd) {

    int i, index;

    if(!mtd->name) {
        DEBUG_MTD("Leeres MTD übergeben!");
        return;
    }
    DEBUG_MTD("name %s" , mtd->name);

    for (i = 0; i < sizeof(str_rootfs) / sizeof(char*) ; i++) {
        if (!strcmp(mtd->name, str_rootfs[i])) {
            DEBUG_MTD("found %s", mtd->name);
            if (found_rootfs_ram)       /*--- we found a rootfs in RAM and use only this ---*/
                return;
            if (!strcmp(mtd->name, str_rootfs[0]))
                found_rootfs_ram = 1;   /*--- signal that we found a rootfs in RAM ---*/

            index = get_partition_index(mtd);
            DEBUG_MTD("use %s" , mtd->name);
            if (index >= 0) {
                static char root_device[64];
                sprintf(root_device, "/dev/mtdblock%d", index);
                DEBUG_MTD("root device: %s (%s)" , root_device, mtd_table[index]->name);
                root_dev_setup(root_device);
                return;
            } else {
                DEBUG_MTD("error: could not find any root device for %s" , mtd->name);
            }
        }
    }

    if(!strcmp(mtd->name, "urlader")) {
        DEBUG_MTD("set fusiv_urlader_mtd");
        fusiv_urlader_mtd = mtd;
#if defined(CONFIG_TFFS)
    } else if(!strcmp(mtd->name, "tffs (1)")) {
        index = get_partition_index(mtd);

        if (index >= 0) {
            tffs_mtd[0] = index;
            DEBUG_MTD("tffs (1) on Index %d", index);
        }
    } else if(!strcmp(mtd->name, "tffs (2)")) {
        index = get_partition_index(mtd);

        if (index >= 0) {
            tffs_mtd[1] = index;
            DEBUG_MTD("tffs (2) on Index %d", index);
        }
#endif /*--- #if defined(CONFIG_TFFS) ---*/
    } else {
        DEBUG_MTD("skip %s" , mtd->name);
    }
}

void fusiv_mtd_rm_notifier(struct mtd_info *mtd) {
    DEBUG_MTD("ignore %s", mtd->name);
}

struct mtd_notifier fusiv_mtd_notifier = {
    add: fusiv_mtd_add_notifier,
    remove: fusiv_mtd_rm_notifier
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int __init fusiv_mtd_init(void) {

    register_mtd_user(&fusiv_mtd_notifier);
    register_mtd_parser(&fusiv_jffs2_parser);
    register_mtd_parser(&fusiv_squashfs_parser);
    fusiv_init_platform_devices();

    return 0;
}
subsys_initcall(fusiv_mtd_init);

/*------------------------------------------------------------------------------------------*\
 * Parst die erste Größe in einem Größenangaben String vom Urlader
 * Format der Größenangaben: xxx_size=<nn>{,KB,MB}
\*------------------------------------------------------------------------------------------*/
unsigned long long parse_mtd_size(char *p) {
    unsigned long long size;

    DEBUG_MTD("'%s'", p);

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
static int __init mtdram_setup(char *p) {
    char *start;
    if(!p)
        return 0;

    start = prom_getenv("linux_fs_start");
    if(start && !strcmp(start, "nfs")) {
        printk(KERN_ERR "dont use RAM filesystem, use NFS\n");
        return 0;
    }
    DEBUG_MTD("mtdram1 %s" , p);
    fusiv_ram_resource[0].start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
    fusiv_ram_resource[0].flags  = IORESOURCE_MEM;
    p = strchr(p, ',');
    if(p) {
        p++;
        fusiv_ram_resource[0].end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        /*--- fusiv_ram_resource[0].end -= 1; ---*/
    } else {
        fusiv_ram_resource[0].start = 0;
    }
    DEBUG_MTD("mtdram1 0x%08x-0x%08x" , fusiv_ram_resource[0].start, fusiv_ram_resource[0].end );
    fusiv_ram_partitions[0].name		 = "rootfs_ram";
    fusiv_ram_partitions[0].offset	 = 0;
    fusiv_ram_partitions[0].size		 = fusiv_ram_resource[0].end - fusiv_ram_resource[0].start;
    fusiv_ram_partitions[0].mask_flags = MTD_ROM;

    add_to_platform_device_list(&fusiv_ram_device);
    return 0;
}
__setup("mtdram1=", mtdram_setup);

/*------------------------------------------------------------------------------------------*\
     * NAND Parameter parsen
\*------------------------------------------------------------------------------------------*/
static int __init mtdnand_setup(char *p) {
    unsigned long long flashsize_nand = parse_mtd_size(p);


    fusiv_nand_partitions[0].name   = (char *)"nand-filesystem";
    fusiv_nand_partitions[0].size   = flashsize_nand;
    fusiv_nand_partitions[0].offset = 0;
    DEBUG_MTD("nand_size = 0x%llx" , flashsize_nand);

    add_to_platform_device_list(&fusiv_nand_device[0]);
    return 0;
}
__setup("nand_size=", mtdnand_setup);

/*------------------------------------------------------------------------------------------*\
     * NOR Parameter parsen
\*------------------------------------------------------------------------------------------*/
static int __init mtdnor_setup(char *p) {
    unsigned long flashsize_nor;
    unsigned long mtd_start, mtd_end;
    unsigned long flashoffset_nor = 0;

    if(!p)
        return 0;

    fusiv_nor_partitions[0].name = (char *)"filesystem";
    fusiv_nor_partitions[1].name = (char *)"kernel";
    fusiv_nor_partitions[2].name = (char *)"urlader";
    fusiv_nor_partitions[3].name = (char *)"tffs (1)";
    fusiv_nor_partitions[4].name = (char *)"tffs (2)";
    fusiv_nor_partitions[5].name = (char *)"reserved";    /* Nie mit "jffs2" initialisieren! */
    /* Die mtds werden vom Userland nach jffs2 gegrept und ggf. beschrieben, daher das
     * mtd nie jffs2 nennen, solange die Position/Größe noch nicht stimmt. */

    flashsize_nor = parse_mtd_size(p);
    DEBUG_MTD("nor_size = 0x%lx" , flashsize_nor);

    /*--------------------------------------------------------------------------------------*\
     * Größen ermitteln
    \*--------------------------------------------------------------------------------------*/
    p = prom_getenv("mtd2");
    if(p) {
        DEBUG_MTD("mtd2 = %s", p);
        mtd_start  = (unsigned int)simple_strtoul(p, NULL, 16);
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = (unsigned int)simple_strtoul(p, NULL, 16);
            flashoffset_nor = mtd_start;
            g_fusiv_nor_flash_start = mtd_start;
            fusiv_nor_partitions[2].size   = mtd_end - mtd_start;
            fusiv_nor_partitions[2].offset = mtd_start - flashoffset_nor;
            fusiv_nor_resource[0].start    = mtd_start & ~0xE0000000;
            fusiv_nor_resource[0].end      = fusiv_nor_resource[0].start + flashsize_nor;
        }
    }
    p = prom_getenv("mtd0");
    if(p) {
        DEBUG_MTD("mtd0 = %s", p);
        mtd_start  = (unsigned int)simple_strtoul(p, NULL, 16);
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = (unsigned int)simple_strtoul(p, NULL, 16);
            fusiv_nor_partitions[0].size   = mtd_end - mtd_start;
            fusiv_nor_partitions[0].offset = mtd_start - flashoffset_nor;
        }
    }
    p = prom_getenv("mtd1");
    if(p) {
        DEBUG_MTD("mtd1 = %s", p);
        mtd_start  = (unsigned int)simple_strtoul(p, NULL, 16);
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = (unsigned int)simple_strtoul(p, NULL, 16);
            fusiv_nor_partitions[1].size   = mtd_end - mtd_start;
            fusiv_nor_partitions[1].offset = mtd_start - flashoffset_nor;
        }
    }
    p = prom_getenv("mtd3");
    if(p) {
        DEBUG_MTD("mtd3 = %s", p);
        mtd_start  = (unsigned int)simple_strtoul(p, NULL, 16);
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = (unsigned int)simple_strtoul(p, NULL, 16);
            fusiv_nor_partitions[3].size   = mtd_end - mtd_start;
            fusiv_nor_partitions[3].offset = mtd_start - flashoffset_nor;
        }
    }
    p = prom_getenv("mtd4");
    if(p) {
        DEBUG_MTD("mtd4 = %s", p);
        mtd_start  = (unsigned int)simple_strtoul(p, NULL, 16);
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = (unsigned int)simple_strtoul(p, NULL, 16);
            fusiv_nor_partitions[4].size       = mtd_end - mtd_start;
            fusiv_nor_partitions[4].offset     = mtd_start - flashoffset_nor;
        }
    }
    p = prom_getenv("mtd5");
    if(p) {
        DEBUG_MTD("mtd5 = %s", p);
        mtd_start  = (unsigned int)simple_strtoul(p, NULL, 16);
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = (unsigned int)simple_strtoul(p, NULL, 16);
            fusiv_nor_partitions[5].size       = mtd_end - mtd_start;
            fusiv_nor_partitions[5].offset     = mtd_start - flashoffset_nor;
        }
    }
    fusiv_nor_flashsize = flashsize_nor;
    g_fusiv_nor_flash_size = flashsize_nor;
    add_to_platform_device_list(&fusiv_nor_device[0]);
    return 0;
}
__setup("nor_size=", mtdnor_setup);

/*------------------------------------------------------------------------------------------*\
 * SPI Flash Parameter parsen
\*------------------------------------------------------------------------------------------*/
#if defined (FUSIV_MTD_SPI_PARTS) && (FUSIV_MTD_SPI_PARTS > 0 )
static int __init mtdspi_setup(char *p) {

    unsigned long flashsize_spi, mtd_start, mtd_end;

    if(!p)
        return 0;

    flashsize_spi = (unsigned long)parse_mtd_size(p);

    DEBUG_MTD("sflash_size = 0x%lx" , flashsize_spi);

    p = prom_getenv("mtd2");
    if(p) {
        DEBUG_MTD("mtd2 = %s", p);
        mtd_start  = (unsigned int)simple_strtoul(p, NULL, 16);

        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = (unsigned int)simple_strtoul(p, NULL, 16);
            if(mtd_end && (mtd_end - mtd_start <= flashsize_spi)) {
                fusiv_spi_partitions[0].name       = (char*)"urlader";
                fusiv_spi_partitions[0].size       = mtd_end - mtd_start;
                fusiv_spi_partitions[0].offset     = 0;
                fusiv_spi_partitions[0].mask_flags = 0;

                fusiv_spi_partitions[1].name       = (char*)"tffs (1)";
                fusiv_spi_partitions[1].size       = (flashsize_spi - fusiv_spi_partitions[0].size) / 2;
                fusiv_spi_partitions[1].offset     = fusiv_spi_partitions[0].size; 
                fusiv_spi_partitions[1].mask_flags = 0;

                fusiv_spi_partitions[2].name       = (char*)"tffs (2)";
                fusiv_spi_partitions[2].size       = (flashsize_spi - fusiv_spi_partitions[0].size) / 2;
                fusiv_spi_partitions[2].offset     = fusiv_spi_partitions[0].size + fusiv_spi_partitions[1].size;
                fusiv_spi_partitions[2].mask_flags = 0;
            }
        }
    }
    {
        int i;
        for(i = 0 ; i < FUSIV_MTD_SPI_PARTS ; i++)
            DEBUG_MTD("mtd%d: %20s: 0x%08x - 0x%08x (size 0x%x)",
                    i + FUSIV_MTD_NAND_PARTS,
                    fusiv_spi_partitions[i].name,
                    fusiv_spi_partitions[i].offset,
                    fusiv_spi_partitions[i].offset + fusiv_spi_partitions[i].size,
                    fusiv_spi_partitions[i].size);
    }

    add_to_platform_device_list(&fusiv_spi_device[0]);
    return 0;
}
__setup("sflash_size=", mtdspi_setup);

#endif /*--- #if defined (FUSIV_MTD_SPI_PARTS) && (FUSIV_MTD_SPI_PARTS > 0 ) ---*/
#endif
