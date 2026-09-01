/******************************************************************************
**
** FILE NAME    : ifxmips_prom.c
** PROJECT      : IFX UEIP
** MODULES      : BSP Basic
**
** DATE         : 27 May 2009
** AUTHOR       : Xu Liang
** DESCRIPTION  : common source file
** COPYRIGHT    :       Copyright (c) 2009
**                      Infineon Technologies AG
**                      Am Campeon 1-12, 85579 Neubiberg, Germany
**
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
** HISTORY
** $Date        $Author         $Comment
** 27 May 2009   Xu Liang        The first UEIP release
*******************************************************************************/



/*
 * PROM interface routines.
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bootmem.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <asm/cpu.h>
#include <asm/traps.h>
#include <asm/cacheflush.h>
#include <asm/prom.h>

#ifdef CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY
unsigned int wlan_mem_start = 0;
#endif

#include <ifx_regs.h>
#include <ifx_types.h>
#include <ifx_gpio.h>
#include <model.h>
#ifndef USE_BUILTIN_PARAMETER
    #include <linux/env.h>
#endif /*--- #ifdef USE_BUILTIN_PARAMETER ---*/

extern void prom_printf(const char * fmt, ...);
extern void ifx_chip_setup(void);

#ifdef CONFIG_AR9
extern void ifx_enhance_phy_clock(void);
#endif

/* for Multithreading (APRP), vpe.c will use it */
unsigned long physical_memsize = 0;
unsigned long cp0_memsize = 0;

/* Glag to indicate whether the user put mem= in the command line */
#ifdef CONFIG_BLK_DEV_INITRD
extern unsigned long initrd_start, initrd_end;
#endif

unsigned long g_ifx_nor_flash_start = -1;
unsigned long g_ifx_nor_flash_size = 0;

static unsigned int *chip_cp1_base = NULL;
static unsigned int chip_cp1_size = 0;

// #define USE_BUILTIN_PARAMETER
/*--- #if defined(CONFIG_ATM_BONDING_BM) || defined(CONFIG_USE_EMULATOR) ---*/
/*--- #  define USE_BUILTIN_PARAMETER ---*/
/*--- #else ---*/
/*--- #  undef  USE_BUILTIN_PARAMETER ---*/
/*--- #endif ---*/

#ifdef USE_BUILTIN_PARAMETER
static char * chip_arg[3] =
{
#if 0
    "<ignored>",
    //"root=/dev/ram rw initcall_debug 1 loglevel=10 ip=172.20.80.222:172.17.69.210::255.255.252.0::eth0:off ether=0,0,eth0 ethaddr=00:01:02:03:04:05 console=ttyS1,115200 panic=1 rd_start=0xa1000000 rd_size=532480",
    //"root=/dev/ram rw ip=172.20.80.222:172.17.69.210::255.255.252.0::eth0:off ether=0,0,eth0 ethaddr=00:01:02:03:04:05 console=ttyS1,115200 panic=1 rd_start=0x81000000 rd_size=1166337",
    //goodramdisk "root=/dev/ram rw ip=172.20.80.222:172.17.69.210::255.255.252.0::eth0:off ether=0,0,eth0 ethaddr=00:01:02:03:04:05 console=ttyS1,115200 panic=1",
    //"initrd=0xa1000000, 441156 root=/dev/ram rw ip=172.20.80.222:172.17.69.210::255.255.252.0::eth0:off ether=0,0,eth0 ethaddr=00:01:02:03:04:05 console=ttyS1,115200 panic=1",
   #if 1
    //"root=/dev/ram rw ip=172.20.80.222:172.17.69.210::255.255.252.0::eth0:off ether=0,0,eth0 ethaddr=00:01:02:03:04:05 console=ttyS1,4800 panic=1 mem=16M initcall_debug=1",
    //"root=/dev/ram rw ip=172.20.80.222:172.17.69.210::255.255.252.0::eth0:off ether=0,0,eth0 ethaddr=00:01:02:03:04:05 console=ttyS1,4800 panic=1 mem=16M nowait=1",
    //"root=/dev/nfs rw nfsroot=172.20.80.7:/mnt/root26_small ip=172.20.80.28:172.20.80.7::::eth0:on console=ttyS1,115200 ethaddr=00:E0:92:01:02:4F mem=31M panic=1",
    //"root=/dev/ram rw ip=172.20.80.222:172.20.80.1::::eth0:off console=ttyS0,115200 ethaddr=00:01:02:03:04:05 phym=16M mem=14M panic=1 nowait=1 migration_debug=1 max_cache_size=1048576",
    //"root=/dev/ram rw ip=172.20.80.222:172.20.80.1::::eth0:off console=ttyS0,38400 ethaddr=00:01:02:03:04:05 phym=16M mem=14M panic=1 nowait=1",
    "root=/dev/ram rw ip=172.20.80.222:172.20.80.1::::eth0:off console=ttyS0,38400 ethaddr=00:01:02:03:04:05 phym=16M mem=16M panic=1 nowait=1",
    //"root=/dev/ram rw ip=172.20.80.222:172.20.80.1::::eth0:off console=ttyUSIF0,38400 ethaddr=00:01:02:03:04:05 phym=16M mem=14M panic=1 nowait=1",
    //"root=/dev/nfs rw nfsroot=172.20.80.1:/mnt/root26_20 ip=172.20.80.222:172.20.80.1::::eth0:on console=ttyS1,9600 ethaddr=00:01:02:03:04:05 phym=16M mem=14M panic=1 nowait=1 migration_debug=1 migration_cost=5000000,6000000 max_cache_size=1048576",
   #else
    "root=/dev/mtdblock2 ip=172.20.80.28:172.20.80.7::::eth0:on console=ttyS1,115200 ethaddr=00:E0:92:01:02:4F mem=31M panic=1",
   #endif
    NULL
#endif
    "<ignored>",
#if 1
    "root=/dev/nfs rw "
#ifdef CONFIG_AR9
    "nfsroot=192.168.178.20:/mnt/rootfs_ar9 "
#endif
#ifdef CONFIG_VR9
    "nfsroot=192.168.178.20:/mnt/rootfs_vr9 "
#endif
    "ip=192.168.178.1:::::eth0:on "
    "ethaddr=00:04:0e:ff:ab:cd "
#else
    "root=/dev/mtdblock1 "
#endif
    "phym=32M "
    "mem=30M "
    /*--- "panic=1 " ---*/
    /*--- "nowait=1 " ---*/
    "migration_debug=1 "
#ifdef CONFIG_VR9
    "console=ttyS0,115200 ",
#else /*--- #ifdef CONFIG_VR9 ---*/
    "console=ttyS1,115200 ",
#endif /*--- #else ---*/ /*--- #ifdef CONFIG_VR9 ---*/
    NULL
};

static char * chip_env[] =
{
    "flash_start=0x10000000",
    "flash_size=0x00400000",
    "memsize=32 *1024 *1024",
    NULL
};
#endif /* USE_BUILTIN_PARAMETER */

static struct callvectors *debug_vectors;

void init_gpio_config(void);
void set_wlan_dect_config_address(unsigned int *pConfig);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int* ifx_get_cp1_base(void)
{
    return chip_cp1_base;
}
EXPORT_SYMBOL(ifx_get_cp1_base);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ifx_get_cp1_size(void)
{
    return chip_cp1_size;
}
EXPORT_SYMBOL(ifx_get_cp1_size);

//#define DEBUG_PROM

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void __init mips_nmi_setup(void)
{
	void *base;
	extern char except_vec_nmi;

	base = cpu_has_veic ?
		(void *)(CAC_BASE + 0xa80) :
		(void *)(CAC_BASE + 0x380);
	memcpy(base, &except_vec_nmi, 0x80);
	flush_icache_range((unsigned long)base, (unsigned long)base + 0x80);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void __init mips_ejtag_setup(void)
{
	void *base;
	extern char except_vec_ejtag_debug;

	base = cpu_has_veic ?
		(void *)(CAC_BASE + 0xa00) :
		(void *)(CAC_BASE + 0x300);
	memcpy(base, &except_vec_ejtag_debug, 0x80);
	flush_icache_range((unsigned long)base, (unsigned long)base + 0x80);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __init prom_init(void)
{
    int argc = fw_arg0;
    char **argv = (char **) fw_arg1;
    char **envp = (char **) fw_arg2;
    struct callvectors *cv = (struct callvectors *) fw_arg3;

    int i;
#if defined(USE_BUILTIN_PARAMETER)
    unsigned long memsz = 0;
    char *scr;
#endif /*--- #if defined(USE_BUILTIN_PARAMETER) ---*/
#ifdef CONFIG_BLK_DEV_INITRD
    unsigned long rdstart = 0, rdsize = 0;
#endif
    char *scr __attribute__((unused));

    prom_printf("\nLantiq xDSL CPE " BOARD_SYSTEM_TYPE "\n");

    ifx_chip_setup();

#ifdef CONFIG_USE_EMULATOR
    prom_printf("press any key to continue...\n");
    while (((*IFX_ASC1_FSTAT)& 0x003F /* ASCFSTAT_RXFFLMASK */) == 0x00) ;
#endif

    debug_vectors = cv;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
    mips_machgroup = MACH_GROUP_IFX;
#else
    mips_machtype  = MACH_TYPE_IFX;
#endif
	board_nmi_handler_setup = mips_nmi_setup;
	board_ejtag_handler_setup = mips_ejtag_setup;

#if defined(USE_BUILTIN_PARAMETER) && defined(CONFIG_IFX_ASC_DEFAULT_BAUDRATE) \
    && !defined(CONFIG_IFX_USIF_UART_CONSOLE)
    //  change some paramter to default settings for emulation purpose
    scr= (char *)KSEG1ADDR((unsigned long)chip_arg[1]);
    scr = strstr(scr, "console=");
    if ( scr != NULL )
    {
        scr += 14;  //  bypass "console=ttyS0,"
        i = sprintf(scr, "%d", CONFIG_IFX_ASC_DEFAULT_BAUDRATE);
        for ( ; i < 6; i++ )
            scr[i] = ' ';
    }

    argv = (char **)KSEG1ADDR((unsigned long)chip_arg);
    envp = (char **)KSEG1ADDR((unsigned long)chip_env);
    argc = 2;
    /* USIF has fixed clock input, no need to parse baudrate settting */
#elif defined(USE_BUILTIN_PARAMETER) && defined(CONFIG_SERIAL_IFX_USIF_UART)
    argv = (char **)KSEG1ADDR((unsigned long)chip_arg);
    envp = (char **)KSEG1ADDR((unsigned long)chip_env);
    argc = 2;
#else
    argv = (char **)KSEG1ADDR((unsigned long)argv);
    envp = (char **)KSEG1ADDR((unsigned long)envp);
    env_init((int*)envp, ENV_LOCATION_PHY_RAM);

#ifdef CONFIG_AR9
    ifx_enhance_phy_clock();
#endif

    init_gpio_config();

#if !defined(CONFIG_AR10)
    /*--- Configure 25MHz Clk-Out on GPIO3 ---*/
    ifx_gpio_register(IFX_GPIO_MODULE_EXTPHY_25MHZ_CLOCK);
#endif

    set_wlan_dect_config_address((unsigned int *)fw_arg3);
#endif
#ifdef DEBUG_PROM
    prom_printf("[%s %s %d]: argc %d, fw_arg0 %p, fw_arg1 %p, fw_arg2 %p fw_arg3 %p\n", __FILE__, __func__, __LINE__,
                argc, fw_arg0, fw_arg1, fw_arg2, fw_arg3);
#endif
    /* arg[0] is "g", the rest is boot parameters */
    arcs_cmdline[0] = '\0';
    for (i = 1; i < argc; i++) {
        argv[i] = (char *)KSEG1ADDR(argv[i]);
        if (!argv[i])
            continue;
        if (strlen(arcs_cmdline) + strlen(argv[i] + 1) >= sizeof(arcs_cmdline))
            break;
        strcat(arcs_cmdline, argv[i]);
        strcat(arcs_cmdline, " ");
    }
#ifdef DEBUG_PROM
    prom_printf("[%s %d]: arcs_cmdline - %s\n", __func__, __LINE__, arcs_cmdline);
#endif

#if defined(USE_BUILTIN_PARAMETER)
    if ( (scr = strstr(arcs_cmdline, "phym=")) )
    {
        scr += 5;
        memsz = 0;
        while ( *scr >= '0' && *scr <= '9' )
        {
            memsz = memsz * 10 + *scr - '0';
            scr++;
        }
        if ( *scr == 'm' || *scr == 'M' )
            memsz *= 1024 * 1024;
        else if ( *scr == 'k' || *scr == 'K' )
            memsz *= 1024;
        physical_memsize = memsz;
    }
    if ( (scr = strstr(arcs_cmdline, "mem=")) )
    {
        scr += 4;
        memsz = 0;
        while ( *scr >= '0' && *scr <= '9' )
        {
            memsz = memsz * 10 + *scr - '0';
            scr++;
        }
        if ( *scr == 'm' || *scr == 'M' )
            memsz *= 1024 * 1024;
        else if ( *scr == 'k' || *scr == 'K' )
            memsz *= 1024;
        cp0_memsize = memsz;
    }

    /* now handle envp */
    if (envp != (char **)KSEG1ADDR(0)) {
        /* assume for now that exactly 3 values get passed */
        while (*envp) {
            *envp = (char *)KSEG1ADDR(*envp);
            /* check for memsize */
            if (physical_memsize == 0 && strncmp(*envp, "memsize=", 8) == 0) {
                scr = *envp + 8;
                memsz = (int)simple_strtoul(scr, NULL, 0);
                physical_memsize = memsz * 1024 * 1024;
#ifdef DEBUG_PROM
                prom_printf("[%s %s %d]: memsize=%d\n", __FILE__, __func__, __LINE__, memsz);
#endif
            }
#ifdef CONFIG_BLK_DEV_INITRD
            /* check for initrd_start */
            if (strncmp(*envp, "initrd_start=", 13) == 0) {
                scr = *envp + 13;
                rdstart = (int)simple_strtoul(scr, NULL, 0);
                rdstart = KSEG1ADDR(rdstart);
  #ifdef DEBUG_PROM
                prom_printf("initrd_start=%#x\n", (unsigned int)rdstart);
  #endif
            }
            /* check for initrd_size */
            if (strncmp(*envp, "initrd_size=", 12) == 0)
            {
                scr = *envp + 12;
                rdsize = (int)simple_strtoul(scr, NULL, 0);
  #ifdef DEBUG_PROM
                prom_printf("initrd_size=%ul\n", (unsigned int)rdsize);
  #endif
            }
#endif /* CONFIG_BLK_DEV_INITRD */

#if 0  /*--- wir nutzen unseren eigenen Mechanismuss ---*/
#ifdef CONFIG_MTD_IFX_NOR
            /* check for flash address and size */
            if (strncmp(*envp, "flash_start=", 12) == 0) {
                scr = *envp + 12;
                g_ifx_nor_flash_start = simple_strtoul(scr, NULL, 0);
  #ifdef DEBUG_PROM
                prom_printf("flash_start=%#x\n", g_ifx_nor_flash_start);
  #endif
            }
            if (strncmp(*envp, "flash_size=", 11) == 0) {
                scr = *envp + 11;
                g_ifx_nor_flash_size = simple_strtoul(scr, NULL, 0);
  #ifdef DEBUG_PROM
                prom_printf("flash_size=%ul\n", g_ifx_nor_flash_size);
  #endif
            }
#endif  /* CONFIG_MTD_IFX_NOR */
#endif
            envp++;
        }
    }
#else /*--- #if defined(USE_BUILTIN_PARAMETER) ---*/
    {
        char *arg = NULL;

        cp0_memsize = 0;
        physical_memsize = 0;
        if((arg = prom_getenv("memsize")) != NULL) {
            physical_memsize = simple_strtoul(arg, NULL, 0);
            cp0_memsize = physical_memsize;
        }
    }
#endif /*--- #else ---*/ /*--- #if defined(USE_BUILTIN_PARAMETER) ---*/

    if ( physical_memsize ==  0 )
        physical_memsize = cp0_memsize;
    else if ( cp0_memsize ==  0 )
        cp0_memsize = physical_memsize;
    max_pfn = PFN_DOWN(cp0_memsize);
    printk("phym = %08lx, mem = %08lx, max_pfn = %08lx\n", physical_memsize, cp0_memsize, max_pfn);

    chip_cp1_base = (unsigned int*)(KSEG1 | cp0_memsize);
    chip_cp1_size = physical_memsize - cp0_memsize;
    printk("Reserving memory for CP1 @0x%08x, size 0x%08x\n", (unsigned int)chip_cp1_base, chip_cp1_size);

#ifdef CONFIG_BLK_DEV_INITRD
    /* u-boot always passes a non-zero start, but a 0 size if there */
    /* is no ramdisk */
    if (rdstart != 0 && rdsize != 0)
    {
        initrd_start = rdstart;
        initrd_end = rdstart + rdsize;
    }
#endif
    /* Set the I/O base address */
    set_io_port_base(0);

    /* Set memory regions */
    ioport_resource.start = 0;      /* Should be KSEGx ???  */
    ioport_resource.end = 0xffffffff;   /* Should be ???    */
#ifdef CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY
    wlan_mem_start = (cp0_memsize - (CONFIG_AVM_ARCH_STATIC_WLAN_MEMORY_SIZE << 10)) & PAGE_MASK;
    printk("wlan_mem_start = 0x%08x\n", wlan_mem_start);
    add_memory_region(0, wlan_mem_start, BOOT_MEM_RAM);
#else
    add_memory_region(0, cp0_memsize, BOOT_MEM_RAM);
#endif


#ifdef CONFIG_MIPS_CMP
	register_smp_ops(&cmp_smp_ops);
#endif
#ifdef CONFIG_MIPS_MT_SMP
	register_smp_ops(&vsmp_smp_ops);
#endif
#ifdef CONFIG_MIPS_MT_SMTC
    register_smp_ops(&smtc_smp_ops);
#endif
#ifdef DEBUG_PROM
    prom_printf("[%s %s %d]: finished\n", __FILE__, __func__, __LINE__);
#endif
}

void __init prom_free_prom_memory(void)
{
    return;
}

const char *get_system_type(void)
{
    return BOARD_SYSTEM_TYPE;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #define DEBUG_WLAN_DECT_CONFIG ---*/
#if defined(DEBUG_WLAN_DECT_CONFIG)
#define DBG_WLAN_DECT(arg...) printk(KERN_ERR arg)
#else
#define DBG_WLAN_DECT(arg...) 
#endif

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/vmalloc.h>

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
extern struct mtd_info *ifx_urlader_mtd;
static unsigned int wlan_dect_config[XR9_MAX_CONFIG_ENTRIES];

void set_wlan_dect_config_address(unsigned int *pConfig) {
    int i = 0;

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
        printk(KERN_ERR "[%s] ERROR: no mem %d\n", __FUNCTION__, len);
        return -1;
    }

    ifx_urlader_mtd->read(ifx_urlader_mtd, offset & ~1, len + sizeof(unsigned int), &readlen, tmpbuffer);

    if (readlen != (len + sizeof(unsigned int))) {
        DBG_WLAN_DECT("[%s] ERROR: read Data\n", __FUNCTION__);
        return -5;
    }

    memcpy(buffer, &tmpbuffer[offset & 1], len);
    vfree(tmpbuffer);

#if defined(DEBUG_WLAN_DECT_CONFIG) 
    {
        int x;
        printk(KERN_ERR "0x");
        for (x=0;x<len;x++)
            printk("%02x ", buffer[x]);
        printk("\n");
    }
#endif
    
    return 0;

}

/*------------------------------------------------------------------------------------------*\
 * die dect_wlan_config kann an einer ungeraden Adresse beginnen
\*------------------------------------------------------------------------------------------*/
int get_wlan_dect_config(enum wlan_dect_type Type, unsigned char *buffer, unsigned short len) {

    int i;
    unsigned int readlen = 0;
    struct wlan_dect_config config;
    int offset;
    unsigned char tmpbuffer[2 * sizeof(struct wlan_dect_config)];

    //printk(KERN_ERR "ifx_urlader_mtd:%pF\n", ifx_urlader_mtd->read);
    DBG_WLAN_DECT("[%s] Type %d buffer 0x%p len %d\n", __FUNCTION__, Type, buffer , len);

    for (i=0;i<XR9_MAX_CONFIG_ENTRIES;i++) {
        DBG_WLAN_DECT("[%s] wlan_dect_config[%d] 0x%x\n", __FUNCTION__, i , wlan_dect_config[i]);
        if (wlan_dect_config[i]) {    /*--- Eintrag vorhanden und nicht leer ---*/
            offset = wlan_dect_config[i];
            ifx_urlader_mtd->read(ifx_urlader_mtd, offset & ~1, 2 * sizeof(struct wlan_dect_config), &readlen, tmpbuffer);

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
                        DBG_WLAN_DECT("[%s/%d] config.Type(%d) != Type(%d)\n", __FUNCTION__, __LINE__, config.Type, Type);
                        break;                      /*--- nächster Konfigeintrag ---*/
                    }
                    if (!(len >= config.Len + sizeof(struct wlan_dect_config))) {
                        DBG_WLAN_DECT("[%s/%d] config.Type(%d) != Type(%d)\n", __FUNCTION__, __LINE__, config.Type, Type);
                        return -2;                  /*--- buffer zu klein ---*/
                    }
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
                        case DSL:
                            DBG_WLAN_DECT("DSL\n");
                            break;
                        case ZERTIFIKATE:
                            DBG_WLAN_DECT("ZERTIFIKATE\n");
                            break;
                        default:
                            DBG_WLAN_DECT("Type unknown\n");
                            return -3;
                    }
                    DBG_WLAN_DECT("[%s] ifx_uralder_mtd='%s', offset=%d, len=%d \n", __FUNCTION__, ifx_urlader_mtd->name, offset,config.Len + sizeof(struct wlan_dect_config));
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

    if (!config) {
        printk( KERN_ERR "[%s] ERROR: no configbuffer\n", __FUNCTION__);
        return -1;
    }

    for (i=0;i<XR9_MAX_CONFIG_ENTRIES;i++) {
        DBG_WLAN_DECT("[%s] wlan_dect_config[%d] 0x%x\n", __FUNCTION__, i , wlan_dect_config[i]);
        if (wlan_dect_config[i]) {    /*--- Eintrag vorhanden und nicht leer ---*/
            offset = wlan_dect_config[i];
            ifx_urlader_mtd->read(ifx_urlader_mtd, offset & ~1, 2 * sizeof(struct wlan_dect_config), &readlen, tmpbuffer);
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
                        case DSL:
                            strcat(buffer, "DSL\n");
                            tmp = strlen("DSL\n");
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

int copy_wlan_dect_config2user(char *buffer, size_t bufferlen) {

    struct wlan_dect_config config;
    char *ConfigStrings[7] = { "WLAN", "DECT", "WLAN2", "ZERTIFIKATE", "DOCSIS", "DSL" , "UNKNOWN"};
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
EXPORT_SYMBOL(prom_wlan_get_base_memory);
#endif

EXPORT_SYMBOL(copy_wlan_dect_config2user);
EXPORT_SYMBOL(test_wlan_dect_config);
EXPORT_SYMBOL(set_wlan_dect_config_address);
EXPORT_SYMBOL(get_wlan_dect_config);

