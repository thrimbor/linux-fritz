#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/env.h>

#include <asm/bootinfo.h>
#include <asm/addrspace.h>
#include <asm/traps.h>
#include <asm/cacheflush.h>

#include <asm/prom.h>

#include "atheros.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern void serial_print(char *fmt, ...);

static void __init set_wlan_dect_config_address(unsigned int *pConfig);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void  __init ath_init_cmdline(int argc, char *argv[]) {
	char *cp;
	int actr;

    /*--- serial_print("[%s]:\n", __FUNCTION__); ---*/
    /*--- serial_print("[%s]: argv[0] = '%s'\n", __FUNCTION__, argv[0]); ---*/
    /*--- serial_print("[%s]: argv[1] = '%s'\n", __FUNCTION__, argv[1]); ---*/
    /*--- serial_print("[%s]: argv[2] = '%s'\n", __FUNCTION__, argv[2]); ---*/
	actr = 1; /* Always ignore argv[0] */

	cp = &(arcs_cmdline[0]);
	while (actr < argc) {
		strcpy(cp, argv[actr]);
		cp += strlen(argv[actr]);
		*cp++ = ' ';
		actr++;
	}
	if (cp != &(arcs_cmdline[0])) {
		/* get rid of trailing space */
		--cp;
		*cp = '\0';
	}
    /*--- serial_print("[%s]:commandline = '%s'\n", __FUNCTION__, arcs_cmdline); ---*/
}

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
    printk(KERN_ERR "[%s] setup NMI vector to base 0x%p\n", __FUNCTION__, base);
	flush_icache_range((unsigned long)base, (unsigned long)base + 0x80);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __init prom_init(void) {

    /*--- serial_print("[%s]:\n", __FUNCTION__); ---*/
	ath_init_cmdline(fw_arg0, (char **)fw_arg1);
    /*--- serial_print("[%s]: call env_init\n", __FUNCTION__); ---*/
    env_init((int *)fw_arg2, ENV_LOCATION_FLASH);

    /*--- serial_print("[%s]: call set_wlan_dect_config_address\n", __FUNCTION__); ---*/
    set_wlan_dect_config_address((unsigned int *)fw_arg3);
    mips_machtype  = CONFIG_ATH_MACH_TYPE;

	board_nmi_handler_setup = mips_nmi_setup;

    /*--- serial_print("[%s]: done\n", __FUNCTION__); ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __init prom_free_prom_memory(void) { 

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
//#define DEBUG_WLAN_DECT_CONFIG
#if defined(DEBUG_WLAN_DECT_CONFIG)
#define DBG_WLAN_DECT(arg...) printk(arg)
#else
#define DBG_WLAN_DECT(arg...) 
#endif

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/vmalloc.h>

#if defined(CONFIG_SERIAL_8250)
extern void serial_print(char *fmt, ...);
#endif
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
extern struct mtd_info *ath_urlader_mtd;
static unsigned int wlan_dect_config[XR9_MAX_CONFIG_ENTRIES];

static void __init set_wlan_dect_config_address(unsigned int *pConfig) {
    int i = 0;

    while (i < XR9_MAX_CONFIG_ENTRIES) {
        wlan_dect_config[i] = pConfig[i] & ((128 << 10) - 1);
#if defined(DEBUG_WLAN_DECT_CONFIG) && defined(CONFIG_SERIAL_8250)
        serial_print("[set_wlan_dect_config] pConfig[%d] 0x%x\n", i, wlan_dect_config[i]);
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

    ath_urlader_mtd->read(  ath_urlader_mtd,
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
int get_wlan_dect_config(enum wlan_dect_type Type, unsigned char *buffer, unsigned short len) {

    int i;
    unsigned int readlen = 0;
    struct wlan_dect_config config;
    int offset;
    unsigned char tmpbuffer[2 * sizeof(struct wlan_dect_config)];

    DBG_WLAN_DECT("[%s] Type %d buffer 0x%p len %d\n", __FUNCTION__, Type, buffer , len);

    for (i=0;i<XR9_MAX_CONFIG_ENTRIES;i++) {
        DBG_WLAN_DECT("[%s] wlan_dect_config[%d] 0x%x\n", __FUNCTION__, i , wlan_dect_config[i]);
        if (wlan_dect_config[i]) {    /*--- Eintrag vorhanden und nicht leer ---*/
            offset = wlan_dect_config[i];
            ath_urlader_mtd->read(ath_urlader_mtd, offset & ~1, 2 * sizeof(struct wlan_dect_config), &readlen, tmpbuffer);
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

    if (!config) {
        printk( KERN_ERR "[%s] ERROR: no configbuffer\n", __FUNCTION__);
        return -1;
    }

    for (i=0;i<XR9_MAX_CONFIG_ENTRIES;i++) {
        DBG_WLAN_DECT("[%s] wlan_dect_config[%d] 0x%x\n", __FUNCTION__, i , wlan_dect_config[i]);
        if (wlan_dect_config[i]) {    /*--- Eintrag vorhanden und nicht leer ---*/
            offset = wlan_dect_config[i];
            ath_urlader_mtd->read(ath_urlader_mtd, offset & ~1, 2 * sizeof(struct wlan_dect_config), &readlen, tmpbuffer);
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
#include <linux/sched.h>
#include <asm/current.h>

int copy_wlan_dect_config2user(char *buffer, size_t bufferlen) {

    struct wlan_dect_config config;
    char *ConfigStrings[MAX_TYPE] = { "WLAN", "DECT", "WLAN2", "ZERTIFIKATE", "DOCSIS", "DSL"};
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

    /*--- printk("test 0x%p\n", current->mm); ---*/

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

EXPORT_SYMBOL(copy_wlan_dect_config2user);
EXPORT_SYMBOL(test_wlan_dect_config);
EXPORT_SYMBOL(get_wlan_dect_config);


