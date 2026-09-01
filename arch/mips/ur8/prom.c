/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Putting things on the screen/serial line using YAMONs facilities.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/serial_reg.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/env.h>
#include <linux/bootmem.h>
#include <asm/bootinfo.h>
#include <asm/prom.h>
#include <asm/mach-ur8/ur8.h>

#define MAX_ENTRY 80

#define WLAN_DECT_CONFIG_DATA_SPACE 8192

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void __init set_wlan_dect_config_address(unsigned int *pConfig);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void  __init ur8_init_cmdline(int argc, char *argv[])
{
char *cp;
int actr;

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
}

char *getcmdline(void)
{
	return &(arcs_cmdline[0]);
}
	
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void __init console_config(void)
{
#ifdef CONFIG_SERIAL_8250_CONSOLE
	char console_string[40];
	int baud = 0;
	char parity = '\0', bits = '\0', flow = '\0';
	char *s, *p;

	if (strstr(getcmdline(), "console="))
		return;

#ifdef CONFIG_KGDB
	if (!strstr(getcmdline(), "nokgdb")) {
		strcat(getcmdline(), " console=kgdb");
		kgdb_enabled = 1;
		return;
	}
#endif

	s = prom_getenv("modetty0");
	if (s) {
		baud = simple_strtoul(s, &p, 10);
		s = p;
		if (*s == ',')
			s++;
		if (*s)
			parity = *s++;
		if (*s == ',')
			s++;
		if (*s)
			bits = *s++;
		if (*s == ',')
			s++;
		if (*s == 'h')
			flow = 'r';
	}

	if (baud == 0)
		baud = 38400;
	if (parity != 'n' && parity != 'o' && parity != 'e')
		parity = 'n';
	if (bits != '7' && bits != '8')
		bits = '8';

	if (flow == 'r')
		sprintf(console_string, " console=ttyS0,%d%c%c%c", baud,
			parity, bits, flow);
	else
		sprintf(console_string, " console=ttyS0,%d%c%c", baud, parity,
			bits);
	strcat(getcmdline(), console_string);
#endif
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __init prom_init(void) {
	ur8_init_cmdline(fw_arg0, (char **)fw_arg1);
    set_wlan_dect_config_address((unsigned int *)fw_arg3);
    env_init((int *)fw_arg2, ENV_LOCATION_FLASH);
	console_config();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define PORT(offset) (KSEG1ADDR(UR8_UART0_BASE + (offset * 4)))
static inline unsigned int serial_in(int offset) {
	return readl((void *)PORT(offset));
}

static inline void serial_out(int offset, int value) {
	writel(value, (void *)PORT(offset));
}

char prom_getchar(void) {
	while (!(serial_in(UART_LSR) & UART_LSR_DR))
		;
	return serial_in(UART_RX);
}

char* prom_getcmdline(void) {
    return &(arcs_cmdline[0]);
}

int prom_putchar(char c) {
	while ((serial_in(UART_LSR) & UART_LSR_TEMT) == 0)
		;
	serial_out(UART_TX, c);
	return 1;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #define DEBUG_WLAN_DECT_CONFIG ---*/
#if defined(DEBUG_WLAN_DECT_CONFIG)
#define PRINTK(args...)  printk(args)
#else
#define PRINTK(args...)
#endif

static unsigned int wlan_dect_config[UR8_MAX_CONFIG_ENTRIES + (UR8_MAX_CONFIG_ENTRIES * sizeof(unsigned char *))];

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int get_wlan_dect_config(enum wlan_dect_type Type, unsigned char *buffer, unsigned short len) {

    int i;
    struct wlan_dect_config config;

    PRINTK("[get_wlan_dect_config] buffer 0x%p len %d\n", buffer , len);

    for (i=0;i<UR8_MAX_CONFIG_ENTRIES;i++) {
        PRINTK("[get_wlan_dect_config] wlan_dect_config[%d] 0x%x\n", i , wlan_dect_config[i]);
        if (wlan_dect_config[i]) {    /*--- Eintrag vorhanden und nicht leer ---*/
            memcpy(&config, (char *)wlan_dect_config[i], sizeof(struct wlan_dect_config));
            switch (config.Version) {
	            case 1:
                case 2:
                    /*--- PRINTK("[get_wlan_dect_config] Type %d Len 0x%x\n", config.Type, config.Len); ---*/
                    if (Type != config.Type) {
                        break;                      /*--- nächster Konfigeintrag ---*/
                    }
                    if (!(len >= config.Len + sizeof(struct wlan_dect_config)))
                        return -2;                  /*--- buffer zu klein ---*/
                    /*--- PRINTK("[get_wlan_dect_config] read "); ---*/
                    switch (config.Type) {
                        case WLAN:
                        case WLAN2:
                            PRINTK("WLAN\n");
                            memcpy(buffer, (char *)wlan_dect_config[i], config.Len + sizeof(struct wlan_dect_config));
#if defined(DEBUG_WLAN_DECT_CONFIG)
                            {
                                int x;
                                for (x=0;x<(config.Len+sizeof(struct wlan_dect_config));x++)
                                    printk("0x%x ", buffer[x]);
                                printk("\n");
                            }
#endif
                            return 0;
                        case DECT:
                            PRINTK("DECT\n");
                            memcpy(buffer, (char *)wlan_dect_config[i], config.Len + sizeof(struct wlan_dect_config));
#if defined(DEBUG_WLAN_DECT_CONFIG)
                            {
                                int x;
                                for (x=0;x<(config.Len+sizeof(struct wlan_dect_config));x++)
                                    printk("0x%x ", buffer[x]);
                                printk("\n");
                            }
#endif
                            return 0;
                        default:
                            PRINTK("Type unknown\n");
                            return -3;
                    }
                    break;
                default:
                    PRINTK("[get_wlan_dect_config] unknown Version %x\n", config.Version);
                    return -3;
            }
        }
    }
    return -1;
}


/*------------------------------------------------------------------------------------------*\
 *  wird benoetigt fuer initiales umkopieren der WLAN_DECT_CONFIG u. zum Anlegegen 
 *  des Environment vor einem SDK-Reboot
\*------------------------------------------------------------------------------------------*/
char **clone_wlan_dect_config( void* (*alloc_func)(size_t, unsigned int) ) {
    struct wlan_dect_config config;
	size_t current_config_len;
	unsigned int i;
	char **new_pconfig = NULL;

	size_t new_pconfig_size = sizeof(char *) * (UR8_MAX_CONFIG_ENTRIES + 1);
	new_pconfig = (*alloc_func)(new_pconfig_size, 1);
	if (!new_pconfig)
		return NULL;
	memset(new_pconfig , 0, new_pconfig_size);

    for (i=0; i < UR8_MAX_CONFIG_ENTRIES; i++) {
        if (wlan_dect_config[i]) {    /*--- Eintrag vorhanden und nicht leer ---*/
            memcpy(&config, (char *)wlan_dect_config[i], sizeof(struct wlan_dect_config));
            PRINTK("{%s} config.Len %d\n", __FUNCTION__, config.Len);
			current_config_len = config.Len + sizeof(struct wlan_dect_config);
			new_pconfig[i] = (*alloc_func)(current_config_len, 1);
			if (new_pconfig[i] == NULL )
				return NULL;
            memcpy(new_pconfig[i], (char *)wlan_dect_config[i], current_config_len);
		}
    }
    return new_pconfig;
}

/*------------------------------------------------------------------------------------------*\
 * zu frue fuer bootmem_alloc --> benutze eigenes alloc
\*------------------------------------------------------------------------------------------*/
unsigned char wlan_dect_config_data[WLAN_DECT_CONFIG_DATA_SPACE];

void __init *alloc_bootmem_wrapper(size_t size, unsigned int dummy){
	static int next_alloc_pos = 0;
	int current_alloc_pos = next_alloc_pos;

	size = ((size + 3) / 4) * 4; /* align it :-) */

	if ( size + current_alloc_pos < WLAN_DECT_CONFIG_DATA_SPACE ){
		next_alloc_pos += size;
		return &wlan_dect_config_data[current_alloc_pos];
	} else {
		printk(KERN_ERR "[%s] WLAN_DECT_CONFIG_DATA_SPACE zu klein\n", __FUNCTION__);
		return NULL;
	}
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void __init set_wlan_dect_config_address(unsigned int *pConfig) {

    int i = 0;
	char **new_pConfig; 

	/*---------------------------------------------------------------------------*\
	 * initialisieren der wlan_dect_config pointer 
	\*---------------------------------------------------------------------------*/
    while (pConfig[i] && (i < UR8_MAX_CONFIG_ENTRIES)) {
        wlan_dect_config[i] = pConfig[i];

        PRINTK("[set_wlan_dect_config] pConfig[%d] 0x%x\n", i, wlan_dect_config[i]);
        i++;
    }

	while( i < UR8_MAX_CONFIG_ENTRIES ){
		wlan_dect_config[i] = 0;
		i++;
	}

	/*---------------------------------------------------------------------------*\
	 * wlan_dect_config muss in einen von uns alloziierten Bereich kopiert werden: 
	\*---------------------------------------------------------------------------*/
	new_pConfig = clone_wlan_dect_config(alloc_bootmem_wrapper);
    PRINTK("[%s] new_pConfig=%#x 0x%x\n", __FUNCTION__, (unsigned int)new_pConfig, (unsigned int)wlan_dect_config_data);

	/*---------------------------------------------------------------------------*\
	 * aktualisieren der wlan_dect_config pointer 
	\*---------------------------------------------------------------------------*/
	i = 0;
    while (new_pConfig[i]) {
	    PRINTK("[%s] new_pConfig[%d]=%#x\n", __FUNCTION__, i, (unsigned int)(new_pConfig[i]));
        wlan_dect_config[i] = (unsigned int)new_pConfig[i];
		i++;
	}
}

EXPORT_SYMBOL(clone_wlan_dect_config);
EXPORT_SYMBOL(get_wlan_dect_config);
