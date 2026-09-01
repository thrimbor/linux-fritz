/*
 * Copyright (C) 2006  Ikanos Communications. All rights reserved.
 * The information and source code contained herein is the property
 * of Ikanos Communications.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <linux/module.h>
#include <linux/env.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/pmon.h>
#include <asm/prom.h>
#include <linux/initrd.h>

//AVMbk
unsigned int xtensa_mem_start = 0;
unsigned int xtensa_mem_size  = 0;
#ifdef CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY
unsigned int wlan_mem_start = 0;
#endif
//end AVMbk
int board_memsize=0;
int board_flash_size;
extern int adi6843_setup(void);
extern int plat_setup(void);
#define ENABLE_JATAG_DEBUG  0

unsigned long g_fusiv_nor_flash_start = -1;
unsigned long g_fusiv_nor_flash_size = 0;

void __init prom_init(void)
{
    char *arg_env = NULL;
	int argc = fw_arg0;
	char **arg = (char **)( fw_arg1);
	int i;
    
    printk("argc %d arg %s env %s \n",argc,*arg, *(char**)fw_arg2);
	arcs_cmdline[0] = '\0';

    env_init((int*)fw_arg2, ENV_LOCATION_PHY_RAM);

    if((arg_env = prom_getenv("memsize")) != NULL) {
        board_memsize = simple_strtoul((const char*)arg_env, NULL, 0);
        board_memsize = board_memsize  & 0x1FFFFFFF;
        printk("memsize  board_memsize = %d\n",board_memsize);
#if defined(CONFIG_FUSIV_KERNEL_BME_DRIVER_VX180_MODULE) || defined(CONFIG_FUSIV_KERNEL_BME_DRIVER_VX180)
        #define PSB_CTL                     0xb90000EC
        unsigned int psb_value = *(volatile unsigned int *)PSB_CTL;

        xtensa_mem_start = psb_value & 0x1FFF0000;
        xtensa_mem_size  = ((psb_value << 16) | 0xFFFF) + 1;

        printk("psb_value = 0x%08x memsize = 0x%08x xtensa_mem_start = 0x%08x xtensa_mem_size 0x%08x\n", 
                psb_value, board_memsize,
                xtensa_mem_start, xtensa_mem_size);

        if(xtensa_mem_start + xtensa_mem_size  > board_memsize) {
           panic("xtensa memory not in avalible memory space, update boot loader\n");
        }

#ifdef CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY
        wlan_mem_start = (xtensa_mem_start - (CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY_SIZE << 10)) & PAGE_MASK;
        printk("wlan_mem_start = 0x%08x\n", wlan_mem_start);
#endif
#else
        board_memsize = simple_strtoul((const char*)arg_env, NULL, 0);
        printk("memsize  board_memsize = %d\n",board_memsize);
#endif

    }

#ifdef CONFIG_BLK_DEV_INITRD		
    if((arg_env = prom_getenv("initrd_start")) != NULL) {
        initrd_start = simple_strtoul((const char*)arg_env, NULL, 16) - 0x20000000;
        printk("initrd_start %x\n",initrd_start);
    }
    if((arg_env = prom_getenv("initrd_size")) != NULL) {
        initrd_end = initrd_start + simple_strtoul((const char*)arg_env, NULL, 16);
        printk("initrd_size start %x end %x\n",initrd_start,initrd_end);
    }
#endif		
	for (i = 1; i < argc; i++) {
              printk("arg[%d] %s \n",i,arg[i]);
		if (strlen(arcs_cmdline) + strlen(arg[i] + 1)
		    >= sizeof(arcs_cmdline))
			break;
		strcat(arcs_cmdline, arg[i]);
		strcat(arcs_cmdline, " ");
		
	}

    if((arg_env = prom_getenv("flashsize")) != NULL) {
        char tmp_buf[20];
        board_flash_size = simple_strtoul((const char*)arg_env, NULL, 16);
        printk("board_flash_size 0x%08x\n",board_flash_size);

        snprintf(tmp_buf, 20, "nor_size=%u ", board_flash_size);
        strcat(arcs_cmdline, (const char*)tmp_buf);
        strcat(arcs_cmdline, "nand_size=512MB ");
    }

    set_wlan_dect_config_address((unsigned int *)fw_arg3);
	
	/* anyone need this ? , OEM board parameters */
	//mips_machgroup = 0x1234;
	mips_machtype = 0x5678;

#if defined(CONFIG_FUSIV_KERNEL_BME_DRIVER_VX180_MODULE) || defined(CONFIG_FUSIV_KERNEL_BME_DRIVER_VX180)
#ifdef CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY
	add_memory_region(0, wlan_mem_start, BOOT_MEM_RAM);
#else
	add_memory_region(0, xtensa_mem_start, BOOT_MEM_RAM);
#endif
    if(xtensa_mem_start + xtensa_mem_size < board_memsize) {
        add_memory_region(xtensa_mem_start + xtensa_mem_size, 
                          board_memsize - (xtensa_mem_start + xtensa_mem_size), BOOT_MEM_RAM); /*--- normal RAM ---*/
    }
#else /*--- #if defined(CONFIG_FUSIV_KERNEL_BME_DRIVER_VX180_MODULE) || defined(CONFIG_FUSIV_KERNEL_BME_DRIVER_VX180) ---*/
    board_memsize=board_memsize - 8;     //reserve 8MB for VDSL BME
	add_memory_region(0, board_memsize, BOOT_MEM_RAM);
#endif

 	change_c0_status(ST0_BEV,0);
	// adi6843_setup();
	plat_setup();
}

void __init prom_free_prom_memory(void)
{
	return ;
}
EXPORT_SYMBOL(board_memsize);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #define DEBUG_WLAN_DECT_CONFIG ---*/
#if defined(DEBUG_WLAN_DECT_CONFIG)
#define DBG_WLAN_DECT(arg...) printk(arg)
#else
#define DBG_WLAN_DECT(arg...) 
#endif

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/vmalloc.h>

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
extern struct mtd_info *fusiv_urlader_mtd;
static unsigned int wlan_dect_config[XR9_MAX_CONFIG_ENTRIES];

void set_wlan_dect_config_address(unsigned int *pConfig) {
    int i = 0;

    if(pConfig == NULL)
        return;

    while (i < XR9_MAX_CONFIG_ENTRIES) {
        wlan_dect_config[i] = pConfig[i] & ((128 << 10) - 1);
#if defined(DEBUG_WLAN_DECT_CONFIG)
        prom_printf("[set_wlan_dect_config] pConfig[%d] 0x%x\n", i, wlan_dect_config[i]);
#endif
        i++;
    }

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int wlan_dect_read_config(int offset, unsigned int len, unsigned char *buffer) {

    unsigned int readlen;
    unsigned char *tmpbuffer = (unsigned char *)vmalloc(len + sizeof(unsigned int));       /*--- wir brauchen einen Buffer zum umkopieren ---*/

    if (!tmpbuffer) {
        printk("[%s] ERROR: no mem %d\n", __FUNCTION__, len);
        return -1;
    }

    fusiv_urlader_mtd->read(  fusiv_urlader_mtd,
                                offset & ~1, 
                                len + sizeof(unsigned int),
                                &readlen,
                                tmpbuffer);

    if (readlen != (len + sizeof(unsigned int))) {
        DBG_WLAN_DECT("[%s] ERROR: read Data\n", __FUNCTION__);
        return -5;
    }

    memcpy(buffer, &tmpbuffer[offset & 1], len);

    vfree(tmpbuffer);

#if defined(DEBUG_WLAN_DECT_CONFIG) 
    {
        int x;
        for (x=0;x<len;x++)
            printk("0x%x ", buffer[x]);
        printk("\n");
    }
#endif
    
    return 0;

}

/*------------------------------------------------------------------------------------------*\
 * die dect_wlan_config kann an einer ungeraden Adresse beginnen
\*------------------------------------------------------------------------------------------*/
int get_wlan_dect_config(enum wlan_dect_type Type, unsigned char *buffer, unsigned int len) {

    int i;
    unsigned int readlen = 0;
    struct wlan_dect_config config;
    int offset;
    unsigned char tmpbuffer[2 * sizeof(struct wlan_dect_config)];

    if(fusiv_urlader_mtd == NULL) {
        return -1;
    }

    DBG_WLAN_DECT("[%s] Type %d buffer 0x%p len %d\n", __FUNCTION__, Type, buffer , len);

    for (i=0;i<XR9_MAX_CONFIG_ENTRIES;i++) {
        DBG_WLAN_DECT("[%s] wlan_dect_config[%d] 0x%x\n", __FUNCTION__, i , wlan_dect_config[i]);
        if (wlan_dect_config[i]) {    /*--- Eintrag vorhanden und nicht leer ---*/
            offset = wlan_dect_config[i];
            fusiv_urlader_mtd->read(fusiv_urlader_mtd, offset & ~1, 2 * sizeof(struct wlan_dect_config), &readlen, tmpbuffer);
            DBG_WLAN_DECT("[%s] offset 0x%x readlen %d\n", __FUNCTION__, offset, readlen);
            
            if (readlen != 2 * sizeof(struct wlan_dect_config)) {
                DBG_WLAN_DECT("[%s] ERROR: read wlan_dect_config\n", __FUNCTION__);
                return -1;
            }
            memcpy(&config, &tmpbuffer[offset & 1], sizeof(struct wlan_dect_config));

            DBG_WLAN_DECT("[%s] Version 0x%x Type %d Len 0x%x\n", __FUNCTION__, config.Version, config.Type, config.Len);
            switch (config.Version) {
	            case 1:
                case 2:
                    if (Type != config.Type) {
                        break;                      /*--- nächster Konfigeintrag ---*/
                    }
                    if (!(len >= config.Len + sizeof(struct wlan_dect_config)))
                        return -2;                  /*--- buffer zu klein ---*/
                    DBG_WLAN_DECT("[%s] read ", __FUNCTION__);
                    switch (config.Type) {
                        case WLAN:
                        case WLAN2:
                            DBG_WLAN_DECT("WLAN\n");
                            break;
                        case DECT:
                            DBG_WLAN_DECT("DECT\n");
                            break;
                        case DOCSIS:
                            DBG_WLAN_DECT("DOCSIS\n");
                            break;
                        case ZERTIFIKATE:
                            DBG_WLAN_DECT("ZERTIFIKATE\n");
                            break;
                        default:
                            DBG_WLAN_DECT("Type unknown\n");
                            return -3;
                    }
                    if (wlan_dect_read_config(offset, config.Len + sizeof(struct wlan_dect_config), buffer) < 0) {
                        DBG_WLAN_DECT("ERROR: read Data\n");
                        return -5;
                    }
                    return 0;
                default:
                    DBG_WLAN_DECT("[%s] unknown Version %x\n", __FUNCTION__, config.Version);
                    return -3;
            }
        }
    }
    return -1;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int search_wlan_dect_config(enum wlan_dect_type Type, struct wlan_dect_config *config) {

    int i;
    unsigned int readlen = 0;
    int offset;
    unsigned char tmpbuffer[2 * sizeof(struct wlan_dect_config)];

    if(fusiv_urlader_mtd == NULL) {
        return -1;
    }

    if (!config) {
        printk( KERN_ERR "[%s] ERROR: no configbuffer\n", __FUNCTION__);
        return -1;
    }

    for (i=0;i<XR9_MAX_CONFIG_ENTRIES;i++) {
        DBG_WLAN_DECT("[%s] wlan_dect_config[%d] 0x%x\n", __FUNCTION__, i , wlan_dect_config[i]);
        if (wlan_dect_config[i]) {    /*--- Eintrag vorhanden und nicht leer ---*/
            offset = wlan_dect_config[i];
            fusiv_urlader_mtd->read(fusiv_urlader_mtd, offset & ~1, 2 * sizeof(struct wlan_dect_config), &readlen, tmpbuffer);
            if (readlen != 2 * sizeof(struct wlan_dect_config)) {
                DBG_WLAN_DECT("[%s] ERROR: read wlan_dect_config\n", __FUNCTION__);
                return -2;
            }

            memcpy(config, &tmpbuffer[offset & 1], sizeof(struct wlan_dect_config));

            switch (config->Version) {
	            case 1:
                case 2:
                    DBG_WLAN_DECT("[%s] Type %d Len 0x%x\n", __FUNCTION__, config->Type, config->Len);
                    if (Type != config->Type) {
                        break;                      /*--- nächster Konfigeintrag ---*/
                    }
                    return 1;
                default:
                    printk( KERN_ERR "[%s] ERROR: unknown ConfigVersion 0x%x\n", __FUNCTION__, config->Version);
                    break;
            }
        }
    }

    /*--- nix gefunden ---*/
    memset(config, 0, sizeof(struct wlan_dect_config));
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int test_wlan_dect_config(char *buffer, size_t *bufferlen) {

    struct wlan_dect_config config;
    enum wlan_dect_type count = WLAN;
    int tmp = 0, len, error = 0;

    len = *bufferlen;
    *bufferlen = 0;
    buffer[0] = 0;  /*--- damit strcat auch funktioniert ---*/

    while (count < MAX_TYPE) {
        if (search_wlan_dect_config(count, &config)) {
            switch (config.Version) {
                case 1:
                case 2:
                    switch (config.Type) {
                        case WLAN:
                            strcat(buffer, "WLAN\n");
                            tmp = strlen("WLAN\n");
                            break;
                        case WLAN2:
                            strcat(buffer, "WLAN2\n");
                            tmp = strlen("WLAN2\n");
                            break;
                        case DECT:
                            strcat(buffer, "DECT\n");
                            tmp = strlen("DECT\n");
                            break;
                        case DOCSIS:
                            strcat(buffer, "DOCSIS\n");
                            tmp = strlen("DOCSIS\n");
                            break;
                        case ZERTIFIKATE:
                            strcat(buffer, "ZERTIFIKATE\n");
                            tmp = strlen("ZERTIFIKATE\n");
                            break;
                        default:
                            printk( KERN_ERR "[%s] ERROR: unknown ConfigVersion 0x%x\n", __FUNCTION__, config.Version);
                            error = -1;
                    }
                    break;
                default:
                    printk( KERN_ERR "[%s] ERROR: unknown ConfigVersion 0x%x\n", __FUNCTION__, config.Version);
                    error = -1;
            }
            if (len > tmp) {
                len -= tmp;
                *bufferlen += tmp;
            } else {
                DBG_WLAN_DECT( KERN_ERR "[%s] ERROR: Buffer\n", __FUNCTION__);
                error = -1;
            }
        }
        count++;
    }

    return error;

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <linux/fs.h>
#include <linux/file.h>
#include <asm/io.h>
#include <asm/fcntl.h>
#include <asm/errno.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/vmalloc.h>

int copy_wlan_dect_config2user(char *buffer, size_t bufferlen) {

    struct wlan_dect_config config;
    char *ConfigStrings[5] = { "WLAN", "DECT", "WLAN2", "ZERTIFIKATE", "DOCSIS" };
    enum wlan_dect_type Type;
    char *p, *vbuffer, *map_buffer;
    struct file *fp;
    int    configlen, written;

    if (!bufferlen)
        return -1;

    if (buffer[bufferlen-1] == '\n') {      /*--- \n entfernen ---*/
        buffer[bufferlen-1] = 0; 
        bufferlen--;
    }

    for (Type = WLAN; Type < MAX_TYPE; Type++) {
        p = strstr(buffer, ConfigStrings[Type]);
        if (p) {
            if ((Type == WLAN) && (buffer[4] == '2'))   /*--- WLAN & WLAN2 unterscheiden ---*/
                continue;
            p += strlen(ConfigStrings[Type]);
            break;
        }
    }

    if (!p) {
        printk(KERN_ERR "ERROR: Type unknown\n");
        return -1;
    }

    while (*p && (*p == ' ') && (p < &buffer[bufferlen]))   /*--- die spaces im Pfadnamen löschen ---*/
       p++; 

    if (!search_wlan_dect_config(Type, &config)) {
        printk(KERN_ERR "ERROR: no Config found\n");
        return -1;  /*--- keine Config gefunden ---*/
    }

    configlen = config.Len + sizeof(struct wlan_dect_config);     /*--- wir müssen den Header mitlesen ---*/

    fp = filp_open(p, O_CREAT, FMODE_READ|FMODE_WRITE);  /*--- open read/write ---*/
    if(IS_ERR(fp)) {
        printk("ERROR: Could not open file %s\n", p);
        return -1;
    }

    map_buffer = (unsigned char *)do_mmap(0, 0, configlen, PROT_READ|PROT_WRITE, MAP_SHARED, 0);
    if (IS_ERR(buffer)) {
        printk("ERROR: no mem 0x%p\n", map_buffer);
        return -1;
    }

    vbuffer = (char *)vmalloc(configlen);       /*--- wir brauchen einen Buffer zum umkopieren ---*/
    if (!vbuffer) {
        printk("ERROR: no mem\n");
        return -1;
    }

    printk("test 0x%p\n", current->mm);

    if (!get_wlan_dect_config(Type, vbuffer, configlen)) {
        memcpy(map_buffer, &vbuffer[sizeof(struct wlan_dect_config)], config.Len);   /*--- umkopieren & den Header verwerfen ---*/
        written = fp->f_op->write(fp, map_buffer, config.Len, &fp->f_pos);      /*--- die Datei schreiben ---*/

        do_munmap(current->mm, (unsigned long)map_buffer, configlen);    /*--- den buffer wieder frei geben ---*/
        vfree(vbuffer);
        if (written != config.Len) {
            printk("ERROR: write Config\n");
            return -1;
        }
    } else {
        do_munmap(current->mm, (unsigned long)map_buffer, configlen);    /*--- den buffer wieder frei geben ---*/
        vfree(vbuffer);
        printk("ERROR: read Config\n");
        return -1;
    }

    return 0;

}

#ifdef CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int prom_wlan_get_base_memory(unsigned int *base, unsigned int *len) {
    if(base)
        *base = wlan_mem_start;
    if(len)
        *len = (CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY_SIZE << 10);
    return 0;
}
#endif

EXPORT_SYMBOL(copy_wlan_dect_config2user);
EXPORT_SYMBOL(test_wlan_dect_config);
EXPORT_SYMBOL(set_wlan_dect_config_address);
EXPORT_SYMBOL(get_wlan_dect_config);
#ifdef CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY
EXPORT_SYMBOL(prom_wlan_get_base_memory);
#endif
//AVMbk
EXPORT_SYMBOL(xtensa_mem_start);
EXPORT_SYMBOL(xtensa_mem_size);
//end AVMbk
