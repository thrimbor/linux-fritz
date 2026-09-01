/*
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

#define DO_MTD

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>

#include <linux/mtd/plat_ar7240_flash.h>
#include <linux/mtd/plat-ram.h>
#include <asm/prom.h>
#include <linux/squashfs_fs.h>
#include <linux/jffs2.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #define ATH_MTD_DEBUG ---*/

/*--- #define DUMMY_MAGIC(pos, end_pos) (pos > (end_pos / 2)) ---*/
#define DUMMY_MAGIC(pos, end_pos) 0

#if defined(ATH_MTD_DEBUG)
    #define DEBUG_MTD(fmt, arg...) printk(KERN_ERR "[%d:%s/%d] " fmt "\n", smp_processor_id(), __func__, __LINE__, ##arg);
#else
    #define DEBUG_MTD(fmt, arg...)
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _nmi_vector_location {
    unsigned int firmware_length;
    unsigned int vector_gap;
    char vector_id[32];
};


struct _nmi_vector_location *nmi_vector_location = (struct _nmi_vector_location *)0xbfc00040;
extern void set_nmi_vetor_gap(unsigned int start, unsigned int firmware_size, unsigned int gap_size);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define MAX_FLASH_MTD   6
#define JFFS2_MIN_SIZE  6
#define JFFS2_MAX_SIZE  16

static struct mtd_partition ath_partitions[MAX_FLASH_MTD];
/*-------------------------------------------------------------------------------------*\
 * Zuerst wird das JFFS2 gesucht, dann das Squash-FS!
\*-------------------------------------------------------------------------------------*/
static const char *probes[] = { "find_jffs2", "find_squashfs" , NULL };
static unsigned int my_atoi(char *p);
static unsigned int flash_erase_block_size = 64 << 10;
extern int __init root_dev_setup(char *line);

static struct ar7240_flash_data _ath_flash_data = {
	.flash_size	= 16, /*--- __ath_flash_size ---*/
	.parts		= ath_partitions,
	.nr_parts	= ARRAY_SIZE(ath_partitions),
    .probes     = probes
};

/* NOTE: CFI probe will correctly detect flash part as 32M, but EMIF
 * limits addresses to 16M, so using addresses past 16M will wrap */
static struct resource ath_flash_resource[2] = {
    {
        .start		= 0x1f000000,
        .end		= 0x1f000000 + (16 << 20) - 1,    /* 16 MB */
        .flags		= IORESOURCE_MEM,
    },
    {   /* für ins RAM geladenes Filesystem */
        .start		= 0x00000000,
        .end		= 0x00000000, 
        .flags		= IORESOURCE_MEM,
        /*--- .parent     = &iomem_resource ---*/
    }
};

void ath_ram_mtd_set_rw(struct device *pdev, int);

static int use_ath_flash_device = 0;
struct platform_device ath_flash_device = {
	.name		= "ath-nor",
	.id		    = 0,
	.dev		= {
		.platform_data	= &_ath_flash_data,
	},
	.num_resources	= 1,
	.resource	= &ath_flash_resource[0],
};


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct mtd_partition ath_ram_partitions[3];

static int use_ath_ram_device = 0;
static struct platdata_mtd_ram ath_ram_data = {
	.mapname       = "ram-filesystem",
	.bankwidth	   = 4,
	.partitions    = ath_ram_partitions,
	.nr_partitions = 0, /*--- ARRAY_SIZE(ath_ram_partitions), ---*/
    .set_rw        = ath_ram_mtd_set_rw,
    .probes        = probes
};

struct platform_device ath_ram_device = {
	.name		= "mtd-ram",
	.id		    = -1,
	.dev		= {
		.platform_data	= &ath_ram_data,
	},
	.num_resources	= 1,
	.resource	= &ath_flash_resource[1],
};


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ath_ram_mtd_set_rw(struct device *pdev, int mode) {
    if(mode == PLATRAM_RO) {
        printk(KERN_ERR "[ath_ram_mtd_set_rw] PLATRAM_RO\n");
    } else if(mode == PLATRAM_RW) {
        printk(KERN_ERR "[ath_ram_mtd_set_rw] PLATRAM_RW\n");
    }
}

enum _flash_map_enum {
    MAP_UNKNOWN,
    MAP_RAM,
    MAP_FLASH
};


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

static int ath_squashfs_parser_function(struct mtd_info *mtd, struct mtd_partition **p_mtd_pat, unsigned long param) {
    enum _flash_map_enum maptype = MAP_UNKNOWN;
    unsigned count = 1, maxcount = 0;
    
    printk("[%s] mtd_info->name %s mtd_info->index %u param=%lu p_mtd_pat=0x%p\n",__FUNCTION__, mtd->name, mtd->index, param, p_mtd_pat);
    
    if (!strcmp(mtd->name, "ram-filesystem")) {
        maptype = MAP_RAM;
    } else if (!strcmp(mtd->name, "ath-nor")) {
        maptype = MAP_FLASH;
        flash_erase_block_size = mtd->erasesize;
    } else {
        printk(KERN_WARNING "[%s] with unknown mtd type %s\n",__FUNCTION__, mtd->name);
        return 0;
    }

    if(p_mtd_pat) {
        unsigned int magic = 0, readlen = 0;
        loff_t pos, start_offset, end_offset;

        if(*p_mtd_pat) 
            printk("[%s] *p_mtd_pat->name %s\n",__FUNCTION__, (*p_mtd_pat)->name);

        switch (maptype) {
            case MAP_FLASH:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ath_partitions;
                }
                maxcount = ARRAY_SIZE(ath_partitions);
                break;
            case MAP_RAM:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ath_ram_partitions;
                }
                maxcount = ARRAY_SIZE(ath_ram_partitions);
                break;
            default:
                break;
        }

        printk("[%s] try partition %s (offset 0x%lx len %lu blocksize=%x) read='%pF'\n", 
				__FUNCTION__,
                (*p_mtd_pat)[count].name,
                (unsigned long)((*p_mtd_pat)[count].offset),
                (unsigned long)((*p_mtd_pat)[count].size),
                mtd->erasesize, mtd->read);

        start_offset = pos = (*p_mtd_pat)[count].offset;
		end_offset = (*p_mtd_pat)[1].offset + (*p_mtd_pat)[count].size;

        while( pos < end_offset ) {
            mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&magic);
			if(maptype == MAP_RAM) {
				/*--- printk("[%s] read %u bytes, magic = 0x%08x index %u pos 0x%x\n",__FUNCTION__, readlen, magic, mtd->index, (unsigned int)pos); ---*/
			}
			if(magic == 0x73717368 || DUMMY_MAGIC(pos, end_offset)) {
                (*p_mtd_pat)[0].offset = pos;
                (*p_mtd_pat)[0].size	 = (u_int32_t)start_offset + (u_int32_t)(*p_mtd_pat)[1].size - (u_int32_t)pos;
				if(maptype == MAP_RAM) {
					(*p_mtd_pat)[0].name	 = "rootfs_ram";
				}
				else {
					(*p_mtd_pat)[0].name	 = "rootfs";
				}
                (*p_mtd_pat)[1].size	 = (u_int32_t)pos - (u_int32_t)start_offset;
                (*p_mtd_pat)[1].name     = "kernel";
                printk("[%s] magic found @pos 0x%x\n",__FUNCTION__, (unsigned int)pos);
                if ((maptype == MAP_FLASH) && (memcmp(ath_partitions[5].name, "jffs2", 4) != 0)) {
                    /* JFFS2 nicht gefunden: Wenn jffs2_size gesetzt ist, ggf. verkleinern */
                    /* sonst anlegen mit der verbleibenden Flash Grösse nach Filesystem % 64k */
                    u_int32_t   jffs2_size, jffs2_start, jffs2_earliest_start;
                    struct squashfs_super_block squashfs_sb;

                    prom_getenv((char*)"jffs2_size");
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
                        printk(KERN_WARNING "[%s]: not enough space for JFFS2!\n",__FUNCTION__);
                    } else {
                        char *p;
                        int flashsize;
                        p = prom_getenv((char*)"flashsize");
                        if(p) {
                            flashsize = my_atoi(p);
                        } else {
                            flashsize = 0x800000;
                        }
                        printk("[%s] flashsize=%x\n",__FUNCTION__, flashsize);
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
                        ath_partitions[5].offset = jffs2_start;
                        ath_partitions[5].size   = jffs2_size * 0x10000;
                        ath_partitions[5].name   = "jffs2";
                        printk("[%s] jffs2_start@%x size: %d\n",__FUNCTION__, jffs2_start, jffs2_size); 
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
	return maxcount;
}

#define JFFS_NODES ( JFFS2_NODETYPE_DIRENT | JFFS2_NODETYPE_INODE | JFFS2_NODETYPE_CLEANMARKER | JFFS2_NODETYPE_PADDING | JFFS2_NODETYPE_SUMMARY | JFFS2_NODETYPE_XATTR | JFFS2_NODETYPE_XREF) 
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ath_jffs2_parser_function(struct mtd_info *mtd, struct mtd_partition **p_mtd_pat, unsigned long param) {
    enum _flash_map_enum maptype = MAP_UNKNOWN;
    unsigned count = 1;
    /*--- static unsigned int erasesize = 0; ---*/
    
    printk("[%s] mtd_info->name %s mtd_info->index %u param=%lu p_mtd_pat=0x%p\n", __FUNCTION__, mtd->name, mtd->index, param, p_mtd_pat);
    
    if (!strcmp(mtd->name, "ram-filesystem")) {
        maptype = MAP_RAM;
        /*--- if(erasesize) { ---*/
            /*--- printk(KERN_WARNING "[%s] set mtd-ram erase size from 0x%x to 0x%x\n",__FUNCTION__, mtd->erasesize, erasesize); ---*/
            /*--- mtd->erasesize = erasesize; ---*/
        /*--- } ---*/
    } else if (!strcmp(mtd->name, "ath-nor")) {
        /*--- erasesize = mtd->erasesize; ---*/
        maptype = MAP_FLASH;
    } else {
        printk(KERN_WARNING "[%s] with unknown mtd type %s\n",__FUNCTION__, mtd->name);
        return 0;
    }

    if(p_mtd_pat) {
        unsigned int magic = 0, readlen = 0;
        loff_t pos;
        if(*p_mtd_pat) 
            printk("[%s] *p_mtd_pat->name %s\n",__FUNCTION__ , (*p_mtd_pat)->name);

        switch (maptype) {
            case MAP_FLASH:
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ath_partitions;
                }
                break;
            case MAP_RAM:
                count = 2;
                if(*p_mtd_pat == NULL) {
                    *p_mtd_pat = ath_ram_partitions;
                }
                /*--- return 0; ---*/   /* nicht im RAM suchen */
                break;
            default:
                break;
        }

        printk("[%s] try partition %s (offset 0x%lx len %lu)\n", 
				__FUNCTION__,
                (*p_mtd_pat)[count].name,
                (unsigned long)((*p_mtd_pat)[count].offset),
                (unsigned long)((*p_mtd_pat)[count].size));

        pos = (*p_mtd_pat)[count].offset;
        while(pos < (*p_mtd_pat)[count].offset + (*p_mtd_pat)[count].size) {
            mtd->read(mtd, (loff_t)pos, sizeof(unsigned int), &readlen, (u_char*)&magic);
            /*--- printk("[%s] read %u bytes, magic = 0x%08x index %u pos 0x%x\n",__FUNCTION__, readlen, magic, mtd->index, pos); ---*/
            if ((((magic >> 16) & ~JFFS_NODES) == 0) && ((magic & 0xFFFF) == JFFS2_MAGIC_BITMASK)) {
                switch (maptype) {
                    case MAP_FLASH:
                        (*p_mtd_pat)[5].size	 = (*p_mtd_pat)[1].offset + (*p_mtd_pat)[1].size - pos;
                        (*p_mtd_pat)[5].offset   = pos;
                        (*p_mtd_pat)[5].name	 = "jffs2";
                        /*--- printk("mtd1: size %d\n", (*p_mtd_pat)[1].size); ---*/
                        printk("[%s] magic %04x found @pos 0x%x, size %ld\n",__FUNCTION__, magic, (unsigned int)pos, (unsigned long)((*p_mtd_pat)[5].size));
                        break;
                    case MAP_RAM:
                        (*p_mtd_pat)[2].size	 = (*p_mtd_pat)[count].offset + (*p_mtd_pat)[count].size - pos;
                        (*p_mtd_pat)[2].offset   = pos;
                        (*p_mtd_pat)[2].name	 = "ram-jffs2";
                        /*--- printk("mtd1: size %d\n", (*p_mtd_pat)[1].size); ---*/
                        printk("[%s] magic %04x found @pos 0x%x, size %ld\n",__FUNCTION__ ,	magic, (unsigned int)pos, (unsigned long)((*p_mtd_pat)[2].size));
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
            printk("[get_erase_block_size_on_ram_device] eraseblocksize=0x10000\n");
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
        printk("[get_erase_block_size_on_ram_device] eraseblocksize=0x20000\n");
        return 0x20000;
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct mtd_part_parser ath_squashfs_parser = {
	.name     = "find_squashfs",
	.parse_fn = ath_squashfs_parser_function
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct mtd_part_parser ath_jffs2_parser = {
	.name     = "find_jffs2",
	.parse_fn = ath_jffs2_parser_function
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
static int found_rootfs_ram = 0;
char *str_rootfs[] = { "rootfs_ram", "rootfs", "filesystem" };
struct mtd_info * ath_urlader_mtd;

void __init ath_mtd_add_notifier(struct mtd_info *mtd) {
	extern int tffs_mtd[2];
	extern struct mtd_info *mtd_table[MAX_MTD_DEVICES];
    int i, index;
    if(!mtd->name) {
        DEBUG_MTD("Leeres MTD übergeben!");
        return;
    }
    DEBUG_MTD("name %s" , mtd->name);

    for (i = 0; i < sizeof(str_rootfs) / sizeof(char*) ; i++) {
        if (!strcmp(mtd->name, str_rootfs[i])) {
            if (found_rootfs_ram) {      /*--- we found a rootfs in RAM and use only this ---*/
				DEBUG_MTD("found %s but prefer rootfs from RAM", mtd->name);
                return;
			} else {
				DEBUG_MTD("found %s", mtd->name);
			}
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
            } 
#if defined(ATH_MTD_DEBUG)
            else {
                DEBUG_MTD("%s is not my root device" , mtd_table[index] ? mtd_table[index]->name : "<NULL>");
            }
#endif
        }
    }

    if(!strcmp(mtd->name, "urlader")) {
        DEBUG_MTD("set urlader_mtd");
        ath_urlader_mtd = mtd;
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

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __init ath_mtd_rm_notifier(struct mtd_info *mtd) {
    printk("[ath_mtd_rm_notifier] ignore %s\n", mtd->name);
}

struct mtd_notifier __initdata ath_mtd_notifier = {
    add: ath_mtd_add_notifier,
    remove: ath_mtd_rm_notifier
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
        if(p[-1] == 'K')  {
            size *= 1024;
        } else if(p[-1] == 'M')  {
            size *= (1024 * 1024);
        }
    }
	printk("[%s] %Ld\n",__FUNCTION__,  size);

    return size; 
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void find_nmi_vector(void) {
    unsigned int len;
    if(strcmp(nmi_vector_location->vector_id, "NMI Boot Vector")) {
        printk(KERN_ERR "[%s] no nmi vector found\n", __FUNCTION__);
        return;
    }
    len  = (nmi_vector_location->firmware_length + flash_erase_block_size) & ~(flash_erase_block_size  - 1);
    printk(KERN_ERR "[%s] nmi vector found. Firmware length 0x%x bytes (erase block align 0x%x) vector gap size 0x%x bytes.\n", 
            __FUNCTION__, nmi_vector_location->firmware_length, len, nmi_vector_location->vector_gap);
    len += ath_partitions[2].size;  /*--- urlader size ---*/
    printk(KERN_ERR "[%s] add '%s' size 0x%llx to length\n", __FUNCTION__, ath_partitions[2].name, ath_partitions[2].size);
    
    set_nmi_vetor_gap(0xbfc00000, len, nmi_vector_location->vector_gap);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void find_nmi_vector_gap(unsigned int base, unsigned int end) {
    unsigned int len;
    struct _nmi_vector_location *first_loc = (struct _nmi_vector_location *)(base + 0x40);
    struct _nmi_vector_location *last_loc = (struct _nmi_vector_location *)(base + 0xbe0040);
    while((unsigned long)first_loc <= (unsigned long)last_loc) {
        if((unsigned int)first_loc >= end)
            break;
        /*--- if(((unsigned long)first_loc > 0x87d95a00UL) && ((unsigned long)first_loc < 0x87d96000UL)) ---*/
            /*--- printk(KERN_ERR "[NMI] %p => %10pB\n", first_loc, first_loc); ---*/
        if(!strcmp(first_loc->vector_id, "NMI Boot Vector")) {

            len = end - ((unsigned int)first_loc - 0x40) + first_loc->vector_gap;

            printk(KERN_ERR "[%s] nmi vector found. Firmware length 0x%x bytes (move length 0x%x) vector gap size 0x%x bytes.\n", 
                __FUNCTION__, first_loc->firmware_length, len, first_loc->vector_gap);

            memmove((void *)((unsigned int)first_loc - 0x40), 
                    (void *)((unsigned int)first_loc + first_loc->vector_gap - 0x40), 
                    len);
            return;
        }
        first_loc = (struct _nmi_vector_location *)((unsigned int)first_loc + 256);
    }
    printk(KERN_ERR "[%s] no nmi vector found at base %#x\n", __FUNCTION__, base);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init mtdflash_setup(char *p);


int __init ath_mtd_init(void) {

	printk("[%s]\n", __FUNCTION__);
    /*--- mtdflash_setup(prom_getenv((char*)"flashsize")); ---*/
    register_mtd_parser(&ath_squashfs_parser);
    register_mtd_parser(&ath_jffs2_parser);
    register_mtd_user(&ath_mtd_notifier);

	if ( use_ath_flash_device ) {
		printk("[%s] flash: type=%ld, start=%#x, end=%#x \n", __FUNCTION__, ath_flash_device.resource->flags, ath_flash_device.resource->start,  ath_flash_device.resource->end );
		platform_device_register( &ath_flash_device );
	}

	if ( use_ath_ram_device ) {
		printk("[%s] ram: type=%ld, start=%#x, end=%#x \n", __FUNCTION__, ath_ram_device.resource->flags, ath_ram_device.resource->start,  ath_ram_device.resource->end );
		platform_device_register( &ath_ram_device );
	} 

    find_nmi_vector();
    return 0;
}
subsys_initcall(ath_mtd_init);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init mtdflash_setup(char *p) {
    unsigned long flashsize;
    unsigned long mtd_start, mtd_end;
    unsigned long flashoffset = 0;

    if(!p)
        return 0;

    ath_partitions[0].name = (char *)"filesystem";
    ath_partitions[1].name = (char *)"kernel";
    ath_partitions[2].name = (char *)"urlader";
    ath_partitions[3].name = (char *)"tffs (1)";
    ath_partitions[4].name = (char *)"tffs (2)";
    ath_partitions[5].name = (char *)"reserved";

    flashsize = parse_mtd_size(p);

    /*--------------------------------------------------------------------------------------*\
     * Größen ermitteln
    \*--------------------------------------------------------------------------------------*/
    p = prom_getenv("mtd2");
    if(p) {
        DEBUG_MTD("mtd2 = %s", p);
        mtd_start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16) - 1);
            flashoffset = mtd_start;
            ath_partitions[2].size       = mtd_end - mtd_start + 1;
            ath_partitions[2].offset     = mtd_start - flashoffset;
            ath_flash_resource[0].start = mtd_start;
            ath_flash_resource[0].end   = mtd_start + flashsize;
        }
    }
    p = prom_getenv("mtd0");
    if(p) {
        DEBUG_MTD("mtd0 = %s", p);
        mtd_start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16) - 1);
            ath_partitions[0].size       = mtd_end - mtd_start + 1;
            ath_partitions[0].offset     = mtd_start - flashoffset;
        }
    }
    p = prom_getenv("mtd1");
    if(p) {
        DEBUG_MTD("mtd1 = %s", p);
        mtd_start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16) - 1);
            ath_partitions[1].size       = mtd_end - mtd_start + 1;
            ath_partitions[1].offset     = mtd_start - flashoffset;
        }
    }
    p = prom_getenv("mtd3");
    if(p) {
        DEBUG_MTD("mtd3 = %s", p);
        mtd_start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16) - 1);
            ath_partitions[3].size       = mtd_end - mtd_start + 1;
            ath_partitions[3].offset     = mtd_start - flashoffset;
        }
    }
    p = prom_getenv("mtd4");
    if(p) {
        DEBUG_MTD("mtd4 = %s", p);
        mtd_start  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16));
        p = strchr(p, ',');
        if(p) {
            p++;
            mtd_end  = CPHYSADDR((unsigned int)simple_strtoul(p, NULL, 16) - 1);
            ath_partitions[4].size       = mtd_end - mtd_start + 1;
            ath_partitions[4].offset     = mtd_start - flashoffset;
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
            ath_partitions[5].size       = mtd_end - mtd_start + 1;
            ath_partitions[5].offset     = mtd_start - flashoffset;
        }
    }
	use_ath_flash_device = 1;
    return 0;
}
__setup("sflash_size=", mtdflash_setup);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init mtdram_setup(char *p) {
    char *start;
    DEBUG_MTD("[mtdram_setup] str=\"%s\"\n", p);
    if(p) {
		start = prom_getenv("linux_fs_start");
	    if(start && !strcmp(start, "nfs")) {
		    DEBUG_MTD(KERN_ERR "dont use RAM filesystem, use NFS\n");
			return 0;
	    }

        DEBUG_MTD("[%s] mtdram1 %s\n", __FUNCTION__, p);
        ath_flash_resource[1].start  = my_atoi(p);
        ath_flash_resource[1].start &= ~0xE0000000;
        ath_flash_resource[1].flags  = IORESOURCE_MEM,
        p = strchr(p, ',');
        if(p) {
            p++;
            ath_flash_resource[1].end  = my_atoi(p);
            ath_flash_resource[1].end &= ~0xE0000000;
            ath_flash_resource[1].end -= 1;
        } else {
            ath_flash_resource[1].start = 0;
        }
        DEBUG_MTD("[%s] mtdram1 0x%08x - 0x%08x\n", __FUNCTION__, ath_flash_resource[1].start, ath_flash_resource[1].end );
        ath_ram_partitions[0].name		 = "rootfs_ram";
        ath_ram_partitions[0].offset	 = 0;
        ath_ram_partitions[0].size		 = ath_flash_resource[1].end - ath_flash_resource[1].start + 1;
        ath_ram_partitions[0].mask_flags = MTD_ROM;
        ath_ram_partitions[1].name		 = "unused";
        ath_ram_partitions[1].offset	 = 0;
        ath_ram_partitions[1].size		 = ath_flash_resource[1].end - ath_flash_resource[1].start + 1;
        ath_ram_partitions[1].mask_flags = MTD_ROM;
        ath_ram_partitions[2].name		 = "extra";
        ath_ram_partitions[2].offset	 = 0;
        ath_ram_partitions[2].size		 = ath_flash_resource[1].end - ath_flash_resource[1].start + 1;
        ath_ram_partitions[2].mask_flags = MTD_ROM;
    }
	use_ath_ram_device = 1;
    find_nmi_vector_gap(ath_flash_resource[1].start | 0x80000000, ath_flash_resource[1].end | 0x80000000);
    return 0;
}
__setup("mtdram1=", mtdram_setup);


