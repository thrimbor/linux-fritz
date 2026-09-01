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

#if defined(CONFIG_MTD_PHYSMAP) || defined(CONFIG_MTD_PHYSMAP_MODULE) || defined(CONFIG_VR9) || defined(CONFIG_AR9) || defined(CONFIG_AR10)
#define DO_MTD

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/plat-ram.h>
#include <linux/squashfs_fs.h>
#include <linux/jffs2.h>

#include <ifx_board.h>

#if defined(CONFIG_TFFS) && defined(CONFIG_TFFS_PANIC_LOG) && (defined(CONFIG_VR9) || defined(CONFIG_AR10))
#include <linux/tffs.h>
#endif

/*--- #define IFX_MTD_DEBUG ---*/

#if defined(IFX_MTD_DEBUG)
    #define DEBUG_MTD(fmt, arg...) printk(KERN_ERR "[%d:%s/%d] " fmt "\n", smp_processor_id(), __func__, __LINE__, ##arg);
#else
    #define DEBUG_MTD(fmt, arg...)
#endif

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

void ifx_ram_mtd_set_rw(struct device *pdev, int);
extern int __init root_dev_setup(char *line);
static unsigned int flash_erase_block_size = (1U << 16);
static unsigned int ifx_nor_flashsize;
extern unsigned long g_ifx_nor_flash_start;
extern unsigned long g_ifx_nor_flash_size;
unsigned long long ifxmips_flashsize_nand;


/*------------------------------------------------------------------------------------------*\
 * NOR
\*------------------------------------------------------------------------------------------*/
struct mtd_partition ifx_nor_partitions[IFX_MTD_NOR_PARTS]; /*--- wird in ifxmips_mtd_nor.c genutzt ---*/

static struct resource ifx_nor_resource[] = {
    {
        .start		= 0,
        .end		= 0 + (128),    /* 128 MB */
        .flags		= IORESOURCE_MEM,
        .parent     = &iomem_resource
    },
};

static struct physmap_flash_data ifx_nor_data = {
	.width		= 2,
	.parts		= ifx_nor_partitions,
	.nr_parts	= ARRAY_SIZE(ifx_nor_partitions),
    .probes     = probes
};

struct platform_device ifx_nor_device[] = {
    {
        /*--- TODO ---*/
        .name		= "mtd-nor",
        .id		    = -1,
        .dev		= {
            .platform_data	= &ifx_nor_data,
        },
        .num_resources	= 1,
        .resource	= &ifx_nor_resource[0],
    }
};


/*------------------------------------------------------------------------------------------*\
 * NAND
\*------------------------------------------------------------------------------------------*/
struct mtd_partition ifx_nand_partitions[IFX_MTD_NAND_PARTS]; /*--- wird in ifxmips_mtd_nand.c genutzt ---*/

static struct resource ifx_nand_resource[] = {
    {
        .start		= 0,
        .end		= 0 + (128),    /* 128 MB */
        .flags		= IORESOURCE_MEM,
        .parent     = &nand_flash_resource
    },
};

static struct physmap_flash_data ifx_nand_data = {
	.width		= 2,
	.parts		= ifx_nand_partitions,
	.nr_parts	= ARRAY_SIZE(ifx_nand_partitions),
    /*--- .probes     = probes ---*/
    .probes     = NULL
};

struct platform_device ifx_nand_device[] = {
    {
        /*--- TODO ---*/
        .name		= "ifx_nand",
        .id		    = -1,
        .dev		= {
            .platform_data	= &ifx_nand_data,
        },
        .num_resources	= 1,
        .resource	= &ifx_nand_resource[0],
    }
};


/*------------------------------------------------------------------------------------------*\
 * SPI
\*------------------------------------------------------------------------------------------*/
struct mtd_partition ifx_spi_partitions[IFX_MTD_SPI_PARTS]; /*--- wird in ifxmips_sflash.c genutzt ---*/

/* NOTE: CFI probe will correctly detect flash part as 32M, but EMIF
 * limits addresses to 16M, so using addresses past 16M will wrap */
static struct resource ifx_spi_resource[] = {
    {
        .start		= 0,
        .end		= 0 + (256 << 10),    /* 256 KB */
        .flags		= IORESOURCE_MEM,
        .parent     = &sflash_resource
    },
};

static struct physmap_flash_data ifx_spi_data = {
	.width		= 2,
	.parts		= ifx_spi_partitions,
	.nr_parts	= ARRAY_SIZE(ifx_spi_partitions),
    .probes     = NULL
};

struct platform_device ifx_spi_device[] = {
    {
        .name		= "macronix",
        .id		    = 0,
        .dev		= {
            .platform_data	= &ifx_spi_data,
        },
        .num_resources	= 1,
        .resource	= &ifx_spi_resource[0],
    }
};

/*------------------------------------------------------------------------------------------*\
 * RAM
\*------------------------------------------------------------------------------------------*/
static struct mtd_partition ifx_ram_partitions[2];

static struct resource ifx_ram_resource[] = {
    {   /* für ins RAM geladenes Filesystem */
        .start		= 0,
        .end		= 0 + (32 << 20),
        .flags		= IORESOURCE_MEM,
    }
};

static struct platdata_mtd_ram ifx_ram_data = {
	.mapname       = "ram-filesystem",
	.bankwidth	   = 4,
	.partitions    = ifx_ram_partitions,
	.nr_partitions = 0, // Dieser Wert muss 0 sein, denn sonst wirft er den Parser nicht an.
    .set_rw        = ifx_ram_mtd_set_rw,
    .probes        = probes
};

struct platform_device ifx_ram_device = {
	.name		= "mtd-ram",
	.id		    = -1,
	.dev		= {
		.platform_data	= &ifx_ram_data,
	},
	.num_resources	= 1,
	.resource	= &ifx_ram_resource[0],
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ifx_squashfs_parser_function(struct mtd_info *mtd, struct mtd_partition **p_mtd_pat, unsigned long param) {
    enum _flash_map_enum maptype = MAP_UNKNOWN;
    unsigned count = 1, maxcount = 0;
    
    printk("[%s] mtd_info->name %s mtd_info->index %u param=%lu p_mtd_pat=0x%p\n", __FUNCTION__, mtd->name, mtd->index, param, p_mtd_pat);
    
    if (!strcmp(mtd->name, "ram-filesystem")) {
        maptype = MAP_RAM;
    } else if (!strcmp(mtd->name, "mtd-nor")) {
        maptype = MAP_NOR_FLASH;
        flash_erase_block_size = mtd->erasesize;
    } else {
        printk(KERN_ERR "[%s] with unknown mtd type %s\n", __FUNCTION__, mtd->name);
        return 0;
    }

    if(p_mtd_pat) {
        unsigned int magic = 0, readlen = 0;
        char* p;
        loff_t pos, start_offset;

        /*--- if(*p_mtd_pat) ---*/ 
            /*--- printk("[%s] *p_mtd_pat->name %s\n", __FUNCTION__, (*p_mtd_pat)->name); ---*/

        switch (maptype) {
            case MAP_NOR_FLASH:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ifx_nor_partitions;
                }
                maxcount = ARRAY_SIZE(ifx_nor_partitions);
                break;
            case MAP_RAM:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ifx_ram_partitions;
                }
                maxcount = ARRAY_SIZE(ifx_ram_partitions);
                break;
            default:
                break;
        }

#if 0
        printk("[%s] try partition %s (offset 0x%x len %u blocksize=%x)\n", 
                __FUNCTION__,
                (*p_mtd_pat)[count].name,
                (*p_mtd_pat)[count].offset,
                (*p_mtd_pat)[count].size,
                mtd->erasesize);
#endif

        start_offset = pos = (*p_mtd_pat)[count].offset;

        // Starten mit einer 256-Byte aligned Adresse.
        // Begruendung:
        // Das Squashfs wird 256-Byte aligned. Der Kernel steht davor. Die Startadresse der MTD-RAM-Partition ist also nicht aligned.
        // Der Suchalgorythmus kann also nicht im schlimmsten Fall das Squashfs-Magic nicht finden.
        // pos wird als auf die ersten 256 Byte NACH dem Kernel-Start positioniert.
        if(maptype == MAP_RAM) {
            if((ifx_ram_resource[0].start & ((1 << 8) - 1))) {
                pos = ((ifx_ram_resource[0].start & ~((1 << 8) - 1)) + 256) - ifx_ram_resource[0].start;

                printk(KERN_ERR "[%s:%d] Use offset of 0x%x to search squashfs magic.\n", __FUNCTION__, __LINE__, (unsigned int)pos);
            }
        }

        while(pos < ((*p_mtd_pat)[1].offset + (*p_mtd_pat)[count].size)) {
            mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&magic);
            /*--- printk(KERN_ERR "[%s] read %u bytes, magic = 0x%08x index %u pos 0x%x\n", __FUNCTION__, readlen, magic, mtd->index, (unsigned int)pos); ---*/
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
                (*p_mtd_pat)[0].size   = ((u_int32_t)start_offset + (u_int32_t)(*p_mtd_pat)[1].size - (u_int32_t)pos);
                (*p_mtd_pat)[1].size   = ((u_int32_t)pos - (u_int32_t)start_offset);
                /*--- if(0 == (*p_mtd_pat)[1].size) { ---*/
                    /*--- (*p_mtd_pat)[1].size = 1; ---*/
                /*--- } ---*/

                printk(KERN_ERR "[%s:%d] pos: 0x%x | offs[0]: 0x%llx | size[0]: %llu | offs[1]: 0x%llx | size[1]: %llu\n", __FUNCTION__, __LINE__,
                        (unsigned int)pos,
                        (*p_mtd_pat)[0].offset,
                        (*p_mtd_pat)[0].size,
                        (*p_mtd_pat)[1].offset,
                        (*p_mtd_pat)[1].size);

                // Die RAM-Partitions sollen nicht umbenannt werden!
                if(maptype != MAP_RAM) {
                    (*p_mtd_pat)[0].name	 = "rootfs";
                    (*p_mtd_pat)[1].name     = "kernel";
                }

                printk("[%s] magic found @pos 0x%x\n", __FUNCTION__, (unsigned int)pos);
                if ((maptype == MAP_NOR_FLASH) && (memcmp(ifx_nor_partitions[5].name, "jffs2", 4) != 0)) {
                    /* JFFS2 nicht gefunden: Wenn jffs2_size gesetzt ist, ggf. verkleinern */
                    /* sonst anlegen mit der verbleibenden Flash Grösse nach Filesystem % 64k */
                    u_int32_t   jffs2_size, jffs2_start, jffs2_earliest_start, urlader_jff2_size;
                    struct squashfs_super_block squashfs_sb;
                    unsigned int jffs_size_changed = 0;

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

                    urlader_jff2_size = 0;
                    p = prom_getenv((char*)"jffs2_size");
                    /*--- printk("jffs2_size not set\n"); ---*/
                    if (p) {
                        urlader_jff2_size = (uint32_t)simple_strtoul(p, NULL, 10);
                    } else {
                        jffs_size_changed = 1;
                        printk(KERN_ERR "[%s] jffs2_size not set.\n", __FUNCTION__);
                    }
                    
                    if (jffs2_size < (IFX_MTD_JFFS2_MIN_SIZE * (mtd->erasesize/0x10000))) {
                        printk(KERN_WARNING "[%s]: not enough space for JFFS2!\n", __FUNCTION__);
                    } else {
                        /*--- printk("[%s] flashsize=%x\n", __FUNCTION__, ifx_nor_flashsize); ---*/
                        if (urlader_jff2_size == 0) {
                            /* Für 7320 ohne JFFS_SIZE im Urlader-Env. die Größe
                             * auf 16 begrenzen und nach hinten schieben, damit nicht bei jedem FW Update das
                             * JFFS überschrieben wird */
                            if (jffs2_size > IFX_MTD_JFFS2_MAX_SIZE) {
                                printk(KERN_WARNING "[%s]: limiting jffs2_size to %d\n", __FUNCTION__, IFX_MTD_JFFS2_MAX_SIZE);
                                jffs2_start = jffs2_earliest_start + (jffs2_size - IFX_MTD_JFFS2_MAX_SIZE) * 0x10000;
                                jffs2_size = IFX_MTD_JFFS2_MAX_SIZE;
                                jffs_size_changed = 1;
                            } else {
                                jffs2_start = jffs2_earliest_start;
                            }
                        } else {
                            /* jffs2_size aus dem Urlader verwenden */
                            if (jffs2_size > urlader_jff2_size) {
                                if (urlader_jff2_size < (IFX_MTD_JFFS2_MIN_SIZE * (mtd->erasesize/0x10000))) {
                                    urlader_jff2_size = (IFX_MTD_JFFS2_MIN_SIZE * (mtd->erasesize/0x10000));
                                    printk(KERN_WARNING "[%s]: jffs2_size too small, use %d\n", __FUNCTION__, urlader_jff2_size);
                                    jffs_size_changed = 1;
                                }
                                jffs2_start = jffs2_earliest_start + (jffs2_size - urlader_jff2_size) * 0x10000;
                                jffs2_size = urlader_jff2_size;
                            } else {
                                if(jffs2_size < urlader_jff2_size) {
                                    jffs_size_changed = 1;
                                }
                                jffs2_start = jffs2_earliest_start;
                            }
                        }
                        ifx_nor_partitions[5].offset = jffs2_start;
                        ifx_nor_partitions[5].name   = "jffs2";
                        ifx_nor_partitions[5].size   = jffs2_size * 0x10000;
                        (*p_mtd_pat)[0].size	     -= jffs2_size * 0x10000;  /*--- File System partition verkleinern ---*/
                        printk("[%s] jffs2_start@%x size: %d\n", __FUNCTION__, jffs2_start, jffs2_size); 
                        if(jffs_size_changed) {
                            struct erase_info instr;
                            int ret;
                            printk(KERN_ERR "[%s] JFFS2 size changed, erase old filesystem\n", __FUNCTION__);

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
    return maxcount;
}

#define JFFS_NODES ( JFFS2_NODETYPE_DIRENT | JFFS2_NODETYPE_INODE | JFFS2_NODETYPE_CLEANMARKER | JFFS2_NODETYPE_PADDING | JFFS2_NODETYPE_SUMMARY | JFFS2_NODETYPE_XATTR | JFFS2_NODETYPE_XREF) 
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ifx_jffs2_parser_function(struct mtd_info *mtd, struct mtd_partition **p_mtd_pat, unsigned long param) {
    enum _flash_map_enum maptype = MAP_UNKNOWN;
    unsigned int count = 1;
    /*--- unsigned int maxcount = 0; ---*/
    /*--- static unsigned int erasesize = 0; ---*/
    
    printk("[ifx_jffs2_parser_function] mtd_info->name %s mtd_info->index %u param=%lu p_mtd_pat=0x%p\n", mtd->name, mtd->index, param, p_mtd_pat);
    
    if (!strcmp(mtd->name, "ram-filesystem")) {
        maptype = MAP_RAM;
        /*--- if(erasesize) { ---*/
            /*--- printk(KERN_WARNING "[ifx_jffs2_parser_function] set mtd-ram erase size from 0x%x to 0x%x\n", mtd->erasesize, erasesize); ---*/
            /*--- mtd->erasesize = erasesize; ---*/
        /*--- } ---*/
    } else if (!strcmp(mtd->name, "mtd-nor")) {
        /*--- erasesize = mtd->erasesize; ---*/
        maptype = MAP_NOR_FLASH;
    } else {
        printk(KERN_WARNING "[ifx_jffs2_parser_function] with unknown mtd type %s\n", mtd->name);
        return 0;
    }

    if(p_mtd_pat) {
        unsigned int magic = 0, readlen = 0;
        loff_t pos;
        if(*p_mtd_pat) 
            printk("[ifx_jffs2_parser_function] *p_mtd_pat->name %s\n", (*p_mtd_pat)->name);

        switch (maptype) {
            case MAP_NOR_FLASH:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ifx_nor_partitions;
                }
                /*--- maxcount = ARRAY_SIZE(ifx_nor_partitions); ---*/
                break;
            case MAP_RAM:
                return 0;   /* nicht im RAM suchen */
                count = 2;
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ifx_ram_partitions;
                }
                /*--- maxcount = ARRAY_SIZE(ifx_ram_partitions); ---*/
                break;
            default:
                break;
        }

        printk("[%s] try partition %s (offset 0x%llx len %llu)\n", 
                __func__,
                (*p_mtd_pat)[count].name,
                (*p_mtd_pat)[count].offset,
                (*p_mtd_pat)[count].size);

        pos = (*p_mtd_pat)[count].offset;
        while(pos < (*p_mtd_pat)[count].offset + (*p_mtd_pat)[count].size) {
            mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&magic);
            /*--- printk("[ifx_jffs2_parser_function] read %u bytes, magic = 0x%08x index %u pos 0x%x\n", readlen, magic, mtd->index, (unsigned int)pos); ---*/
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
                        printk("[ifx_jffs2_parser_function] magic %04x found @pos 0x%x, size %llu\n", magic, (unsigned int)pos, (*p_mtd_pat)[5].size);
                        break;
                    case MAP_RAM:
                        (*p_mtd_pat)[2].size	 = (*p_mtd_pat)[count].offset + (*p_mtd_pat)[count].size - pos;
                        (*p_mtd_pat)[2].offset   = pos;
                        (*p_mtd_pat)[2].name	 = "ram-jffs2";
                        /*--- printk("mtd1: size %d\n", (*p_mtd_pat)[1].size); ---*/
                        printk("[ifx_jffs2_parser_function] magic %04x found @pos 0x%x, size %llu\n", magic, (unsigned int)pos, (*p_mtd_pat)[2].size);
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
struct mtd_part_parser ifx_squashfs_parser = {
	.name     = "find_squashfs",
	.parse_fn = ifx_squashfs_parser_function
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct mtd_part_parser ifx_jffs2_parser = {
	.name     = "find_jffs2",
	.parse_fn = ifx_jffs2_parser_function
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct platform_device *ifx_platform_devices[20];
unsigned int ifx_platform_devices_count = 0;

void add_to_platform_device_list(struct platform_device *device, char *name __attribute__ ((unused))) {
    /*--- printk(KERN_INFO "[IFX] add %s to the platform device list\n", name); ---*/
    ifx_platform_devices[ifx_platform_devices_count++] = device;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void ifx_init_platform_devices(void) {
    /*--- printk(KERN_INFO "[IFX] register %d platform device(s)\n", ifx_platform_devices_count); ---*/
	platform_add_devices(ifx_platform_devices, ifx_platform_devices_count);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ifx_ram_mtd_set_rw(struct device *pdev, int mode) {
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
extern int tffs_mtd_offset[2];
static int found_rootfs_ram = 0;
char *str_rootfs[] = { "rootfs_ram", "rootfs", "filesystem" };
struct mtd_info *ifx_urlader_mtd;

void ifx_mtd_add_notifier(struct mtd_info *mtd) {

    int i, index;

    if(!mtd->name) {
        DEBUG_MTD("Leeres MTD übergeben!");
        return;
    }
    /*--- DEBUG_MTD("name %s" , mtd->name); ---*/
    printk(KERN_ERR "{%s} name %s\n", __func__, mtd->name);

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
                DEBUG_MTD("No root device found. Got a partition index of: %d", index);
            }
        }
    }

    if(!strcmp(mtd->name, "urlader")) {
        DEBUG_MTD("set ifx_urlader_mtd");
        ifx_urlader_mtd = mtd;
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

void  ifx_mtd_rm_notifier(struct mtd_info *mtd) {
    DEBUG_MTD("ignore %s", mtd->name);
}

struct mtd_notifier ifx_mtd_notifier_ops = {
    add: ifx_mtd_add_notifier,
    remove: ifx_mtd_rm_notifier
};

/*------------------------------------------------------------------------------------------*\
 * disable AUTO precharge im Urlader (SPI-Flash)
\*------------------------------------------------------------------------------------------*/
int __init ifx_fix_timing(void) {

    unsigned int buffer[2];
    unsigned int readlen, writelen;

    if (ifx_urlader_mtd && ifx_spi_partitions[0].name && ifx_spi_partitions[0].size) {
        printk("<%s>\n", __func__);
        ifx_urlader_mtd->read(ifx_urlader_mtd, 0x40, 2 * sizeof(unsigned int), &readlen, (unsigned char *)buffer);

        if (readlen == (2 * sizeof(unsigned int))) {
            if ((buffer[0] == 0xbf401000) && (buffer[1] == 0x10101)) {
                buffer[1] &= ~(1<<16);
                printk(KERN_ERR "{%s} try to fix 0x%x = 0x%x\n", __func__, buffer[0], buffer[1]);
                ifx_urlader_mtd->write(ifx_urlader_mtd, 0x44, sizeof(unsigned int), &writelen, (unsigned char *)&buffer[1]);
                if (writelen != sizeof(unsigned int)) {
                    printk(KERN_ERR "{%s} failed to write\n", __func__);
                }
            }
        } else {
            printk(KERN_ERR "{%s} failed to read\n", __func__);
        }
    }
    return 0;
}
late_initcall(ifx_fix_timing);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int __init ifx_mtd_init(void) {

    ifx_init_platform_devices();
    register_mtd_user(&ifx_mtd_notifier_ops);
    register_mtd_parser(&ifx_squashfs_parser);
    register_mtd_parser(&ifx_jffs2_parser);

    return 0;
}
subsys_initcall(ifx_mtd_init);

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
    ifx_ram_resource[0].start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
    ifx_ram_resource[0].flags  = IORESOURCE_MEM;
    p = strchr(p, ',');
    if(p) {
        p++;
        ifx_ram_resource[0].end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        ifx_ram_resource[0].end -= 1;
    } else {
        ifx_ram_resource[0].start = 0;
    }
    printk("[%s] mtdram1 0x%08x-0x%08x" , __FUNCTION__, ifx_ram_resource[0].start, ifx_ram_resource[0].end );
    ifx_ram_partitions[0].name		 = "rootfs_ram";
    ifx_ram_partitions[0].offset	 = 0;
    ifx_ram_partitions[0].size		 = ifx_ram_resource[0].end - ifx_ram_resource[0].start + 1;
    ifx_ram_partitions[0].mask_flags = MTD_ROM;
    ifx_ram_partitions[1].name		 = "kernel_ram";
    ifx_ram_partitions[1].offset	 = 0;
    ifx_ram_partitions[1].size		 = ifx_ram_resource[0].end - ifx_ram_resource[0].start + 1;
    ifx_ram_partitions[1].mask_flags = MTD_ROM;

    add_to_platform_device_list(&ifx_ram_device, "mtd-ram");
    return 0;
}
__setup("mtdram1=", mtdram_setup);

/*------------------------------------------------------------------------------------------*\
     * NAND Parameter parsen
\*------------------------------------------------------------------------------------------*/
static int __init mtdnand_setup(char *p) {
    unsigned long mtd_start, mtd_end;

    char *_start = prom_getenv("linux_fs_start");
    unsigned int config_size = 0, kernel_size = 0, filesystem_size = 0;

    if(!p)
        return 0;

    if (!_start)
        _start = "0";       /*--- default setzen ---*/

    ifx_nand_partitions[0].name = (char *)"kernel (1)";
    ifx_nand_partitions[1].name = (char *)"filesystem (1)";
    ifx_nand_partitions[2].name = (char *)"kernel (2)";
    ifx_nand_partitions[3].name = (char *)"filesystem (2)";
    ifx_nand_partitions[4].name = (char *)"config";
    ifx_nand_partitions[5].name = (char *)"nand-filesystem";

    if(_start[0] == '0') {
        ifx_nand_partitions[0].name = (char *)"kernel";
        ifx_nand_partitions[1].name = (char *)"filesystem";
        ifx_nand_partitions[2].name = (char *)"reserved-kernel";
        ifx_nand_partitions[3].name = (char *)"reserved-filesystem";
    }
    if(_start[0] == '1') {
        ifx_nand_partitions[0].name = (char *)"reserved-kernel";
        ifx_nand_partitions[1].name = (char *)"reserved-filesystem";
        ifx_nand_partitions[2].name = (char *)"kernel";
        ifx_nand_partitions[3].name = (char *)"filesystem";
    }

    ifxmips_flashsize_nand = parse_mtd_size(p);
    DEBUG_MTD("nand_size = 0x%llx" , ifxmips_flashsize_nand);
    printk(KERN_ERR "[NAND] nand_size = 0x%llx" , ifxmips_flashsize_nand);

    if(ifxmips_flashsize_nand == 0) {
        return 0;
    }

    /*--------------------------------------------------------------------------------------*\
     * Größen ermitteln
    \*--------------------------------------------------------------------------------------*/
    p = prom_getenv("mtd1");
    if(p) {
        DEBUG_MTD("mtd1 = %s", p);
        mtd_start  = (unsigned int)simple_strtoul(p, NULL, 16);
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = (unsigned int)simple_strtoul(p, NULL, 16);
            kernel_size = mtd_end - mtd_start;
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
            filesystem_size = mtd_end - mtd_start;
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
            config_size = mtd_end - mtd_start;
        }
    }
    /*--------------------------------------------------------------------------------------*\
     * prüfen ob die einzelnen Teile in das Flash passen
    \*--------------------------------------------------------------------------------------*/
    if(filesystem_size  * 2 + kernel_size * 2 > ifxmips_flashsize_nand) {
        panic("Filesystem and Kernel dont fit in to NAND device\n");
    }

    /*--------------------------------------------------------------------------------------*\
     * Groesen aufsetzen
    \*--------------------------------------------------------------------------------------*/
    ifx_nand_partitions[0].size       = kernel_size;
    ifx_nand_partitions[0].offset     = 0;
    ifxmips_flashsize_nand -= kernel_size;

    ifx_nand_partitions[1].size       = filesystem_size;
    ifx_nand_partitions[1].offset     = kernel_size;
    ifxmips_flashsize_nand -= filesystem_size;

    ifx_nand_partitions[2].size       = kernel_size;
    ifx_nand_partitions[2].offset     = kernel_size + filesystem_size;
    ifxmips_flashsize_nand -= kernel_size;

    ifx_nand_partitions[3].size       = filesystem_size;
    ifx_nand_partitions[3].offset     = kernel_size + kernel_size + filesystem_size;
    ifxmips_flashsize_nand -= filesystem_size;

    if(config_size) {
        ifx_nand_partitions[4].size   = config_size;
        ifx_nand_partitions[4].offset = kernel_size + kernel_size + filesystem_size + filesystem_size;
        ifxmips_flashsize_nand -= config_size;
    }

    if(ifxmips_flashsize_nand) {
        ifx_nand_partitions[5].size   = ifxmips_flashsize_nand;
        ifx_nand_partitions[5].offset = kernel_size + kernel_size + filesystem_size + filesystem_size + config_size;
    }
    add_to_platform_device_list(&ifx_nand_device[0], "mtd-nand");

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

    flashsize_nor = parse_mtd_size(p);
    if (!flashsize_nor)
        return 0;

    ifx_nor_partitions[0].name = (char *)"filesystem";
    ifx_nor_partitions[1].name = (char *)"kernel";
    ifx_nor_partitions[2].name = (char *)"urlader";
    ifx_nor_partitions[3].name = (char *)"tffs (1)";
    ifx_nor_partitions[4].name = (char *)"tffs (2)";
    ifx_nor_partitions[5].name = (char *)"reserved";    /* Nie mit "jffs2" initialisieren! */
    /* Die mtds werden vom Userland nach jffs2 gegrept und ggf. beschrieben, daher das
     * mtd nie jffs2 nennen, solange die Position/Größe noch nicht stimmt. */

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
            g_ifx_nor_flash_start = mtd_start;
            ifx_nor_partitions[2].size       = mtd_end - mtd_start;
            ifx_nor_partitions[2].offset     = mtd_start - flashoffset_nor;
            ifx_nor_resource[0].start = mtd_start;
            ifx_nor_resource[0].end   = mtd_start + flashsize_nor;
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
            ifx_nor_partitions[0].size       = mtd_end - mtd_start;
            ifx_nor_partitions[0].offset     = mtd_start - flashoffset_nor;
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
            ifx_nor_partitions[1].size       = mtd_end - mtd_start;
            ifx_nor_partitions[1].offset     = mtd_start - flashoffset_nor;
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
            ifx_nor_partitions[3].size       = mtd_end - mtd_start;
            ifx_nor_partitions[3].offset     = mtd_start - flashoffset_nor;
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
            ifx_nor_partitions[4].size       = mtd_end - mtd_start;
            ifx_nor_partitions[4].offset     = mtd_start - flashoffset_nor;
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
            ifx_nor_partitions[5].size       = mtd_end - mtd_start;
            ifx_nor_partitions[5].offset     = mtd_start - flashoffset_nor;
        }
    }
    ifx_nor_flashsize = flashsize_nor;
    g_ifx_nor_flash_size = flashsize_nor;
    add_to_platform_device_list(&ifx_nor_device[0], "mtd-nor");
    return 0;
}
__setup("nor_size=", mtdnor_setup);

/*------------------------------------------------------------------------------------------*\
 * SPI Flash Parameter parsen
\*------------------------------------------------------------------------------------------*/
static int __init mtdspi_setup(char *p) {

    unsigned long flashsize_spi, mtd_start, mtd_end;

    if(!p)
        return 0;

    flashsize_spi = (unsigned long)parse_mtd_size(p);
    
    if(flashsize_spi == 0)
        return 0;

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
                ifx_spi_partitions[0].name       = (char*)"urlader";
                ifx_spi_partitions[0].size       = mtd_end - mtd_start;
                ifx_spi_partitions[0].offset     = 0;
                ifx_spi_partitions[0].mask_flags = 0;

                ifx_spi_partitions[1].name       = (char*)"tffs (1)";
                ifx_spi_partitions[1].size       = (flashsize_spi - ifx_spi_partitions[0].size) / 2;
                ifx_spi_partitions[1].offset     = ifx_spi_partitions[0].size; 
                ifx_spi_partitions[1].mask_flags = 0;

                ifx_spi_partitions[2].name       = (char*)"tffs (2)";
                ifx_spi_partitions[2].size       = (flashsize_spi - ifx_spi_partitions[0].size) / 2;
                ifx_spi_partitions[2].offset     = ifx_spi_partitions[0].size + ifx_spi_partitions[1].size;
                ifx_spi_partitions[2].mask_flags = 0;
            }
        }
    }

    /*--- MTD-Offsets für Adressierung = MTD-Offset[3/4] - Urlader-Offset ---*/ 
    tffs_mtd_offset[0] = ifx_spi_partitions[1].offset;
    tffs_mtd_offset[1] = ifx_spi_partitions[2].offset;

    {
        int i;
        for(i = 0 ; i < IFX_MTD_SPI_PARTS ; i++)
            DEBUG_MTD("mtd%d: %20s: 0x%08llx - 0x%08llx (size 0x%llx)",
                    i + IFX_MTD_NAND_PARTS,
                    ifx_spi_partitions[i].name,
                    ifx_spi_partitions[i].offset,
                    ifx_spi_partitions[i].offset + ifx_spi_partitions[i].size,
                    ifx_spi_partitions[i].size);
    }

    add_to_platform_device_list(&ifx_spi_device[0], "mtd-spi");
#if defined(CONFIG_TFFS) && defined(CONFIG_TFFS_PANIC_LOG) && (defined(CONFIG_VR9) || defined(CONFIG_AR10))
    tffs_panic_log_register_spi();
#endif
    return 0;
}
__setup("sflash_size=", mtdspi_setup);

#endif
