/*
 * Copyright (C) 2006, 2007 Florian Fainelli <florian@openwrt.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __PROM_H__
#define __PROM_H__

extern char *prom_getcmdline(void);
extern char *prom_getenv(char *name);
extern void prom_init_cmdline(void);
extern void prom_meminit(void);
extern void prom_fixup_mem_map(unsigned long start_mem, unsigned long end_mem);
extern void mips_display_message(const char *str);
extern void mips_display_word(unsigned int num);
extern void mips_scroll_message(void);
extern int get_ethernet_addr(char *ethernet_addr);

/* Memory descriptor management. */
#define PROM_MAX_PMEMBLOCKS    32
struct prom_pmemblock {
        unsigned long base; /* Within KSEG0. */
        unsigned int size;  /* In bytes. */
        unsigned int type;  /* free or prom memory */
};
/*------------------------------------------------------------------------------------------*\
 * Header WLAN - DECT - Config
\*------------------------------------------------------------------------------------------*/
#define UR8_MAX_CONFIG_ENTRIES  4
#define IKANOS_MAX_CONFIG_ENTRIES  8
#define XR9_MAX_CONFIG_ENTRIES  8

enum wlan_dect_type {
    WLAN,
    DECT,
    WLAN2,
    ZERTIFIKATE,
    DOCSIS,
    DSL,
    MAX_TYPE
};

struct __attribute__ ((packed)) wlan_dect_config {
    unsigned char           Version;        /*--- z.Z. 1 ---*/
    enum  wlan_dect_type    Type :8;        /*--- 0 - WLAN; 1 - DECT ---*/
    unsigned short          Len;            /*--- 384 - WLAN, 128 - DECT ---*/
};

/*--- extern void set_wlan_dect_config_address(unsigned int *pConfig); ---*/
extern int get_wlan_dect_config(enum wlan_dect_type Type, unsigned char *buffer, unsigned short len);

#include <asm/prealloc_memory.h>
#endif /* __PROM_H__ */

