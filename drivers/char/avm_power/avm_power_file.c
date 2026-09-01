/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2006 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
\*------------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/file.h>
#include <linux/avm_power.h>
#include <linux/avm_event.h>
#include <linux/netdevice.h>
#include <linux/mm.h>
#include <linux/swap.h>
/*--- #include <asm/semaphore.h> ---*/
#include <asm/errno.h>
/*--- #include <linux/wait.h> ---*/
/*--- #include <linux/vmalloc.h> ---*/
/*--- #include <linux/poll.h> ---*/
#include <linux/completion.h>
#include "avm_power.h"
#include "wyatt_earp.h"

#if defined(CONFIG_MIPS_UR8)
#include <asm/mach-ur8/hw_gpio.h>
#include <asm/mach-ur8/hw_irq.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
#include <asm/mach-ur8/ur8_clk.h>
#endif
#if defined(CONFIG_AVM_SIMPLE_PROFILING) 
#include <linux/avm_profile.h>
#endif/*--- #if defined(CONFIG_AVM_SIMPLE_PROFILING)  ---*/
#if defined(CONFIG_AVM_PA)
#include <linux/avm_pa.h>
#endif /*--- defined(CONFIG_AVM_PA) ---*/

#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/

#if defined(CONFIG_AR10) || defined(CONFIG_VR9)
extern int ifx_ts_get_temp(int *p_temp);
/*--------------------------------------------------------------------------------*\
 * CPU-Temperatur verwenden
\*--------------------------------------------------------------------------------*/
static inline int get_temperature(struct _power_managment_ressource_info *pm_info __attribute__((unused))) {
    int temperature, rest;
    if(ifx_ts_get_temp(&temperature)){
        return 0;
    }
    rest = temperature % 10;
    temperature /= 10;
    if(rest >= 5)  {
        /*--- aufrunden ---*/
        rest = 1;
    } else {
        /*--- unter Null Grad wird der Chip sowieso nicht arbeiten ... ---*/
        rest = 0;
    }
    /*--- printk("[avm_power]temp %d\n", temperature + rest); ---*/
    return temperature + rest; 
}
#else
/*--------------------------------------------------------------------------------*\
 * Temperatursensor nicht unterstuetzt
 * langfristig powerdevice_temperature aus Dect-Treiber entfernen
\*--------------------------------------------------------------------------------*/
static inline int get_temperature(struct _power_managment_ressource_info *pm_info __attribute__((unused))) {
    return 0;
/*--- 	return pm_info->deviceinfo[powerdevice_temperature].power_rate; ---*/
}
#endif/*--- #if defined(CONFIG_AR10) || defined(CONFIG_VR9) ---*/

/*--- fiktive Werte: ---*/
#define MIN_SYSTEMCLK           120000000
#define STD_SYSTEMCLK           120000000
#define MAX_SYSTEMCLK           120000000

#define STD_DSPCLK              360000000
#define MAX_DSPCLK              360000000

#define STD_MIPSCLK             360000000
#define MAX_MIPSCLK             360000000

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 19)
#define AVM_POWER_UDEV
#endif/*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <linux/cdev.h>
#include <asm/mach_avm.h>
#define LOCAL_MAJOR     AVM_POWER_MAJOR

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define MODULE_NAME     "avm_power"
MODULE_DESCRIPTION("AVM powermanagment module");
MODULE_LICENSE("GPL");

/*--- #define DEBUG_TRACE_POWERTAB ---*/
#if defined(DEBUG_TRACE_POWERTAB)
#define DEB_TRC_PT      DEB_ERR
#else/*--- #if defined(DEBUG_TRACE_POWERTAB) ---*/
#define DEB_TRC_PT(args...)
#endif/*--- #else ---*//*--- #if defined(DEBUG_TRACE_POWERTAB) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#define SKIP_SPACES(p) while((p) && *(p) && ((*(p) == ' ') || (*(p) == '\t'))) (p)++;
#define CMD_MODE     "MODE"
#define WE_CMD_MODE  "WE_MODE"
#define PMINFO_MODE  "PMINFO_MODE"
#define PMINFO_SET   "PMINFO_SET"
#define ETH_MODE     "ETH_MODE"
#define IDLE_MODE    "IDLE_MODE"
#define LOAD_MODE    "LOAD_MODE"

static volatile struct _power_managment_clients *PwClientAnker;
static DEFINE_SPINLOCK(avmpower_lock);

#if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_FUSIV_VX180)
static unsigned int WyattEarpMode = 0x1FFF;
#endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_FUSIV_VX180) ---*/

int avm_power_disp_loadrate = 0; 

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _telefonieprofile {
    void *handle;
    unsigned int on;
    unsigned int dectsyncmode;
} telefonevent;

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _powermanagment_status_event {
    void *handle;
    int dsl_status;
} powermanagment_status_event;

#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
static void avmevent_telefonprofile_notify(void *context, enum _avm_event_id id);
static void avm_event_powermanagment_status_notify(void *context, enum _avm_event_id id);
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/

#if defined(DECTSYNC_PATCH)
#include "dectsync.h"
#endif/*--- #if defined(DECTSYNC_PATCH) ---*/

static int limit_set_eth(struct _power_managment_ressource_info *pm_info, unsigned int eth_idx, unsigned int force);

/*-------------------------------------------------------------------------------------*\
 * Liste von registrierten Treibern, die den PowermanagmentCallback-Befehl bekommen
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry dslmode_entries[] =  {
#if defined(CONFIG_MIPS_UR8)
    { "speedstep",AVM_PM_CB_IGNORE,                   0 },  /*--- kein Aendern der CPU-Frequenz per governor erlauben ---*/
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
    { "adsl_event", AVM_PM_CB_UNINSTALLED_OR_FAILED, 10 },  /*--- DSL an: erzeuge Event ---*/
    { "adsl",       AVM_PM_CB_UNINSTALLED_OR_FAILED, 10 },  /*--- DSL an ---*/
    { NULL, 0, 0 }
};

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry atamode_entries[] =  {
    { "adsl_event",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 1 },   /*--- DSL komplett aus: erzeuge Event ---*/
    { "adsl",        AVM_PM_CB_IGNORE,                1 },   /*--- DSL komplett aus ---*/
#if defined(CONFIG_MIPS_UR8)
    { "speedstep", AVM_PM_CB_IGNORE,                  1 },   /*--- Aendern der CPU-Frequenz per governor erlaubt ---*/
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
    { NULL, 0, 0 }
};

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry vdslmode_entries[] =  {
#if defined(CONFIG_MIPS_UR8)
    { "speedstep", AVM_PM_CB_IGNORE,                 0 },     /*--- kein Aendern der CPU-Frequenz per governor erlaubt ---*/
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
    { "adsl_event",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 10 },     /*--- VDSL oder DSL (FUSIV) an: erzeuge Event ---*/
    { "adsl",        AVM_PM_CB_IGNORE,                 3 },     /*--- (VDSL) OHIO/UR8-DSL aus aber Spannung an ---*/
    { NULL, 0, 0 }
};

#if defined(CONFIG_MIPS_UR8)
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry powerup_wlan_entries[] =  {
    { "wlanboost",  AVM_PM_CB_IGNORE, 0x01 },        /*--- DSP Spannung auf 1.2 V) 7212 ---*/
    { NULL, 0, 0 }
};

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry powerdown_wlan_entries[] =  {
    { "wlanboost",  AVM_PM_CB_IGNORE, 0x00 },        /*--- DSP Spannung auf 1.2 V) 7212 ---*/
    { NULL, 0, 0 }
};
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry updatemode_entries[] =  {
    { "speedstep", AVM_PM_CB_IGNORE,                 0 },     /*--- kein Aendern der CPU-Frequenz per governor erlaubt ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry speedstepstatus_entries[] =  {
    { "speedstep",    AVM_PM_CB_IGNORE,                   0x80 },  /*--- Speedstep-Sema: per printk ausgeben ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry speedstepon_entries[] =  {
    { "speedstep",    AVM_PM_CB_IGNORE,                   0x21 },  /*--- Speedstep-Sema: hochzaehlen ---*/
    { NULL, 0, 0 }
};
static struct _power_managment_dest_entry speedstepoff_entries[] =  {
    { "speedstep",    AVM_PM_CB_IGNORE,                   0x20 },  /*---  Speedstep-Sema: auf Defaultclock ---*/
    { NULL, 0, 0 }
};

/*-------------------------------------------------------------------------------------*\
 * alles wie gehabt
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry telefon_profile_off[] =  {
    { "avm_event", AVM_PM_CB_FAILED, 0x0 },                   /*--- Eventmanager bekommt telefon-profil aus mit ---*/
#if defined(CONFIG_MIPS_UR8)
    { "speedstep", AVM_PM_CB_IGNORE,                 0x11 },  /*--- Aendern der CPU-Frequenz per governor erlaubt ---*/
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
    { NULL, 0, 0 }
};

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry telefon_profile_on[] =  {
    { "avm_event", AVM_PM_CB_FAILED, 0x1 },                   /*--- Eventmanager bekommt telefon-profil an mit ---*/
#if defined(CONFIG_MIPS_UR8)
    { "speedstep", AVM_PM_CB_IGNORE,                 0x10 },  /*--- kein Aendern der CPU-Frequenz per governor erlaubt ---*/
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
    { NULL, 0, 0 }
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry usbcurrentreq_entries[] =  {
    { "usbpower",  AVM_PM_CB_FAILED, 0x2 },         /*--- Abfrage aktueller Stromverbrauch aller USB-Hosts ----*/
    { NULL, 0, 0 }
};
#if defined(CONFIG_MIPS_UR8)
/*-------------------------------------------------------------------------------------*\
 * schaltet 5V-Versorgung USB an
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry usbpoweron_entries[] =  {
    { "usbpower_req",  AVM_PM_CB_FAILED, 0x1 },         /*--- Abfragen ob Power on erlaubt ----*/
    { "usbpower",      AVM_PM_CB_FAILED, 0x1 },
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry usbpoweroff_entries[] =  {
    { "usbpower_req",  AVM_PM_CB_FAILED, 0x0 },         /*--- Abfrage ob Power off erlaubt ----*/
    { "usbpower",     AVM_PM_CB_FAILED, 0x0 },
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry powerdown_isdnab_entries[] =  {
    { "piglet",  AVM_PM_CB_FAILED, 0x100 },         /*--- 40 V ausschalten ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry powerup_isdnab_entries[] =  {
    { "piglet",  AVM_PM_CB_FAILED, 0x101 },         /*--- 40 V anschalten ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry powerdown_slic1_entries[] =  {
    { "isdn",  AVM_PM_CB_FAILED, 0x10 },         /*--- ueber PCM-Register ausschalten ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry powerup_slic1_entries[] =  {
    { "isdn",  AVM_PM_CB_FAILED, 0x11 },         /*--- ueber PCM-Register anschalten ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry powerdown_slic2_entries[] =  {
    { "isdn",  AVM_PM_CB_FAILED, 0x20 },         /*--- ueber PCM-Register ausschalten ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry powerup_slic2_entries[] =  {
    { "isdn",  AVM_PM_CB_FAILED, 0x21 },         /*--- ueber PCM-Register anschalten ---*/
    { NULL, 0, 0 }
};
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry speedstep_status_entries[] =  {
    { "speedstep",AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x40 },        /*---  Speedstep Status abfragen ! ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry pcmlinkbus_stop_entries[] =  {
    { "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x0 },        /*---  PCM-Bus deaktivieren ---*/
    { "speedstep",AVM_PM_CB_IGNORE,               0x20 },        /*---  Speedstep aus: anschliessend noch Status-Abfrage notwendig (da asynchron) ! ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry pcmlinkbus_start_entries[] =  {
    { "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x1 },         /*---  PCM-Bus reaktivieren ---*/
    { "speedstep",AVM_PM_CB_IGNORE,               0x21 },         /*---  Speedstep reaktivieren ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry pots_load_entries[] =  {
    { "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x200 },        /*--- checke ob 2 Bitfiles ueberhaupt existent ---*/
    { "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x203 },        /*--- nur wenn TE aktuell Wechsel notwendig ---*/
    { "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x0 },         /*---  PCM-Bus deaktivieren ---*/
    { "piglet",   AVM_PM_CB_IGNORE, 0x210 },                      /*--- lade POTS-File ---*/
    { "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x1 },         /*--- PCMBus reaktivieren ---*/
    { "isdn",     AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x100 },       /*--- TE-DKanal aus (inkl. Slot austragen) ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry te_load_entries[] =  {
    { "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x200 },        /*--- checke ob 2 Bitfiles ueberhaupt existent ---*/
    { "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x202 },        /*--- nur wenn POTS aktuell Wechsel notwendig ---*/
    { "pcmlink", AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x0 },         /*---  PCM-Bus deaktivieren ---*/
    { "piglet",  AVM_PM_CB_IGNORE, 0x211 },                       /*--- lade TE-File ---*/
    { "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x1 },         /*--- PCMBus reaktivieren ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry te_reload_entries[] =  {
    { "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x200 },        /*--- checke ob 2 Bitfiles ueberhaupt existent ---*/
    { "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x203 },        /*--- nur wenn TE aktuell reload notwendig ---*/
    { "pcmlink", AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x0 },          /*---  PCM-Bus deaktivieren ---*/
    { "piglet",  AVM_PM_CB_IGNORE, 0x213 },                       /*--- lade TE-File (force) ---*/
    { "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x1 },         /*--- PCMBus reaktivieren ---*/
    { "isdn",     AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x100 },       /*--- TE-DKanal komplett aus -> triggert Reinitialisierung ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry pots_reload_entries[] =  {
    { "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x200 },        /*--- checke ob 2 Bitfiles ueberhaupt existent ---*/
    { "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x202 },        /*--- nur wenn POTS aktuell reload notwendig ---*/
    { "pcmlink", AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x0 },         /*---  PCM-Bus deaktivieren ---*/
    { "piglet",  AVM_PM_CB_IGNORE, 0x212 },                       /*--- lade POTS-File (force) ---*/
    { "pcmlink",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x1 },         /*--- PCMBus reaktivieren ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry tepots_switchauto_entries[] =  {
    { "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x204 },        /*--- AutoMode an ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry tepots_switchmanu_entries[] =  {
    { "piglet",  AVM_PM_CB_UNINSTALLED_OR_FAILED, 0x205 },        /*----AutoMode aus ---*/
    { NULL, 0, 0 }
};
/*--------------------------------------------------------------------------------*\
 * wird vom pcmlink-Treiber ausgewertet:
\*--------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry teactive_entries[] =  {
    { "piglet",  AVM_PM_CB_FAILED, 0x201 },
    { NULL, 0, 0 }
};
/*-------------------------------------------------------------------------------------*\
 * powermode mit nachfolgender Powermanagment-Treiber-Liste
\*-------------------------------------------------------------------------------------*/
static struct _power_managment power[] = {
    { "dsl",                    dslmode_entries,        PM_ACESS_ALL},
    { "ata",                    atamode_entries,        PM_ACESS_ALL},
    { "vdsl",                   vdslmode_entries,       PM_ACESS_ALL},        /*--- im VDSL-Mode DSL auf ATA stellen ---*/
    { "update",                 updatemode_entries,     PM_ACESS_ALL},
    { "speedstep_status",       speedstepstatus_entries,PM_ACESS_APPL},
    { "speedstep_on",           speedstepon_entries,    PM_ACESS_APPL},
    { "speedstep_off",          speedstepoff_entries,   PM_ACESS_APPL},
    { "telefon_profile_on",     telefon_profile_on,     PM_ACESS_ALL},   /*--- schalte auf normalspeed - niemals runter  ---*/ 
    { "telefon_profile_off",    telefon_profile_off,    PM_ACESS_ALL},   /*--- vorhergehendes Profil ---*/
#if defined(CONFIG_MIPS_UR8)
    { "usb_poweron",            usbpoweron_entries,       PM_ACESS_ALL},
    { "usb_poweroff",           usbpoweroff_entries,      PM_ACESS_ALL},
    { "powerdown_isdnab",       powerdown_isdnab_entries, PM_ACESS_ALL},
    { "powerup_isdnab",         powerup_isdnab_entries,   PM_ACESS_ALL},
    { "powerdown_slic1",        powerdown_slic1_entries,  PM_ACESS_ALL},
    { "powerup_slic1",          powerup_slic1_entries,    PM_ACESS_ALL},
    { "powerdown_slic2",        powerdown_slic2_entries,  PM_ACESS_ALL},
    { "powerup_slic2",          powerup_slic2_entries,    PM_ACESS_ALL},
    { "wlan_booston",           powerup_wlan_entries,     PM_ACESS_ALL},    /*--- anheben der Spannung 7212 auf 1.2V ---*/
    { "wlan_boostoff",          powerdown_wlan_entries,   PM_ACESS_ALL},
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
    { "usb_current_req",        usbcurrentreq_entries,    PM_ACESS_ALL},
    { "speedstep_status",       speedstep_status_entries, PM_ACESS_ALL},
    { "pcmlink_bus_off",        pcmlinkbus_stop_entries,  PM_ACESS_DRIVER},
    { "pcmlink_bus_on",         pcmlinkbus_start_entries, PM_ACESS_DRIVER},
    { "pots_load",              pots_load_entries,        PM_ACESS_ALL},
    { "te_load",                te_load_entries,          PM_ACESS_ALL},
    { "te_reload",              te_reload_entries,        PM_ACESS_ALL},
    { "pots_reload",            pots_reload_entries,      PM_ACESS_ALL},
    { "tepots_switchauto",      tepots_switchauto_entries, PM_ACESS_ALL},
    { "tepots_switchmanu",      tepots_switchmanu_entries, PM_ACESS_ALL},
    { "te_active",              teactive_entries,          PM_ACESS_ALL},    /*--- die letzten werden die ersten sein (wird alle 8 msec aufgerufen) ---*/
    { NULL, NULL, 0}
};

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_power_open(struct inode *, struct file *);
static int avm_power_close(struct inode *, struct file *);
static int avm_power_event_Callback(int state);
static int avm_power_adsl_event_Callback(int state);
static int avm_power_speedstep_Callback(int state);
#if defined(CONFIG_MIPS_UR8)
static int avm_power_wlan_Callback(int state);
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
static int avm_power_usb_Callback(int state);

static ssize_t avm_power_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *write_pos);
static struct _power_managment_dest_entry *find_powermode(char *powermode, unsigned int access);
static void avm_power_writeformaterror(char *text);
static struct _power_managment_clients *find_pwclient_by_name(char *client_name);
static int powermode_action(struct _power_managment_dest_entry *powermodetab);
static int powermode_action_nolist(char *client_name, int State);

#ifdef CONFIG_AVM_POWERMETER
static void pm_ressourceinfo_init(void);
static void pm_ressourceinfo_parse(char *line);
static void pm_ressourceinfo_scriptparse(char *line);
static struct _power_managment_ressource_info pm_ressourceinfo;
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_power avm_power;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct file_operations avm_power_fops = {
    owner:   THIS_MODULE,
    open:    avm_power_open,
    release: avm_power_close,
    /*--- read:    avm_power_read, ---*/
    write:   avm_power_write,
    /*--- ioctl:   avm_power_ioctl, ---*/
    /*--- fasync:  avm_power_fasync, ---*/
    /*--- poll: avm_power_poll, ---*/
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
__inline static unsigned long avm_power_lock(void) {
    unsigned long flags;
    spin_lock_irqsave(&avmpower_lock, flags);
    return flags;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
__inline static void avm_power_unlock(unsigned long flags) {
	spin_unlock_irqrestore(&avmpower_lock, flags);
}

/*------------------------------------------------------------------------------------------*\
\*----------------------------------------------------------------------_--------------------*/
__inline static int avm_power_write_check_special_char(char **p, char check_char) {
    SKIP_SPACES(*p); /* ggf. spaces */
    if(**p != check_char) 
        return 1;
    (*p)++;
    return 0;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static __inline int avm_power_write_find_special_char(char **p, char check_char) {
    while(**p && **p != check_char) {
        (*p)++;
    }
    if(**p != check_char) 
        return 1;
    (*p)++;
    return 0;
}
#if defined(CONFIG_MIPS_UR8)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct resource avmpower_gpioressource[] = {
{
    .name  = "usb_power",
    .flags = IORESOURCE_IO,		 
    .start = GPIO_BIT_DRVVBUS,
    .end   = GPIO_BIT_DRVVBUS,
}
#if defined(DECTSYNC_PATCH)
,{
    .name  = "dectsync",
    .flags = IORESOURCE_IO,		 
    .start = DECT_SYNCGPIO,
    .end   = DECT_SYNCGPIO,
}
#endif/*--- #if defined(DECTSYNC_PATCH) ---*/
};
static unsigned int gRequestAvmPower_ok;
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avmpower_requestinit(void) {
    unsigned int i;
    if(gRequestAvmPower_ok == 0) {
        for(i = 0; i < sizeof(avmpower_gpioressource) / sizeof(struct resource); i++) {
            if(request_resource(&gpio_resource, &avmpower_gpioressource[i])) {
                printk(KERN_ERR"[avmpower_init]ERROR: avmpower-resource %d gpio %u-%u not available\n", i, (unsigned int)avmpower_gpioressource[i].start, (unsigned int)avmpower_gpioressource[i].end);
                return 1;
            }
        }
        gRequestAvmPower_ok = 1;
    }
    return 0;
}
#ifdef CONFIG_AVM_POWER_MODULE
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void avmpower_requestrelease(void) {
    unsigned int i;
    if(gRequestAvmPower_ok == 1) {
        for(i = 0; i < sizeof(avmpower_gpioressource) / sizeof(struct resource); i++) {
            release_resource(&avmpower_gpioressource[i]);
        }
        gRequestAvmPower_ok = 0;
    }
}
#endif/*--- #ifdef CONFIG_AVM_POWER_MODULE ---*/
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
int __init avm_power_init(void) {
    int reason;

#if defined(CONFIG_MIPS_UR8)
    if(avmpower_requestinit()) {
        panic("[avmpower]bye bye - can't initialize avmpower-interface!\n"); 
        return -ERESTARTSYS;
    }
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/

    spin_lock_init(&avmpower_lock);
#if defined(AVM_POWER_UDEV)
    reason = alloc_chrdev_region(&avm_power.device, 0, 1, MODULE_NAME);
#else /*--- #if defined(AVM_POWER_UDEV) ---*/
    DEB_INFO("[avm_power] register_chrdev_region()\n");
    avm_power.device = MKDEV(LOCAL_MAJOR, 0);
    reason = register_chrdev_region(avm_power.device, 1, MODULE_NAME);
#endif
    if(reason) {
        DEB_ERR("[avm_power] register_chrdev_region failed: reason %d!\n", reason);
        return -ERESTARTSYS;
    }
	avm_power.cdev = cdev_alloc();
	if (!avm_power.cdev) {
        unregister_chrdev_region(avm_power.device, 1);
        DEB_ERR("[avm_power] cdev_alloc failed!\n");
        return -ERESTARTSYS;
    }
	avm_power.cdev->owner = avm_power_fops.owner;
	avm_power.cdev->ops = &avm_power_fops;
	kobject_set_name(&(avm_power.cdev->kobj), MODULE_NAME);
		
	if(cdev_add(avm_power.cdev, avm_power.device, 1)) {
        kobject_put(&avm_power.cdev->kobj);
        unregister_chrdev_region(avm_power.device, 1);
        DEB_ERR("[avm_power] cdev_add failed!\n");
        return -ERESTARTSYS;
    }
#if defined(AVM_POWER_UDEV)
    /*--- Geraetedatei anlegen: ---*/
    avm_power.osclass = class_create(THIS_MODULE, MODULE_NAME);
    device_create(avm_power.osclass, NULL, 1, NULL, "%s%d", "power", 0);
#endif/*--- #if defined(AVM_POWER_UDEV) ---*/


#if defined(CONFIG_MIPS_UR8)
    PowerManagmentRegister("wlanboost", avm_power_wlan_Callback);        /*--- für die 7212 ---*/
    avm_gpio_ctrl(GPIO_BIT_DRVVBUS, GPIO_PIN, GPIO_OUTPUT_PIN);
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
    PowerManagmentRegister("usbpower", avm_power_usb_Callback);

#ifdef CONFIG_AVM_POWERMETER
    pm_ressourceinfo_init();
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/

#if defined(CONFIG_FUSIV_VX180)
    {
        extern void Wyatt_Earp_Init_GPIO(void);
        Wyatt_Earp_Init_GPIO();
    }
#endif /*--- #if defined(CONFIG_FUSIV_VX180) ---*/

    PowerManagmentRegister("avm_event", avm_power_event_Callback);
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
    telefonevent.handle =  avm_event_source_register( "telefonprofile", (
                           (((unsigned long long) 1) << avm_event_id_telefonprofile)),
                           avmevent_telefonprofile_notify,
						   &telefonevent
						   );
    powermanagment_status_event.handle = avm_event_source_register( "powermanagment_status", (
                           (((unsigned long long) 1) << avm_event_id_powermanagment_status)),
                           avm_event_powermanagment_status_notify,
						   &powermanagment_status_event
						   );
#endif
    PowerManagmentRegister("adsl_event", avm_power_adsl_event_Callback);        /*--- nun wird zusaetzlich ein Event erzeugt ---*/
    DEB_INFO("[avm_power]: major %d (success)\n", MAJOR(avm_power.device));
    pm_ressourceinfo.speedstep_state = 0x2 | 0x8;    /*--- speedstep auch ohne Telefonapplikation ---*/
#if defined(CONFIG_LANTIQ)
    pm_ressourceinfo.speedstep_state |= 0x1;    /*--- speedstep unabhaengig von DSL ---*/
#endif/*--- #if defined(CONFIG_LANTIQ) ---*/
    pm_ressourceinfo.speedstep_sema = -1;
    PowerManagmentRegister("speedstep", avm_power_speedstep_Callback);

    avm_powermanager_load_control_set(0, 1);
    return 0;
}
module_init(avm_power_init);

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#ifdef CONFIG_AVM_POWER_MODULE
static void pm_ressourceinfo_exit(void);

void __exit avm_power_cleanup(void) {
    DEB_INFO("[avm_power]: unregister_chrdev(%u)\n", MAJOR(avm_power.device));
#if defined(AVM_POWER_UDEV)
    device_destroy(avm_power.osclass, 1);
    class_destroy(avm_power.osclass);
#endif/*--- #if defined(AVM_POWER_UDEV) ---*/
    cdev_del(avm_power.cdev); /* Delete char device */
    unregister_chrdev_region(avm_power.device, 1);
    pm_ressourceinfo_exit();

#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
    if(telefonevent.handle) { 
      avm_event_source_release(telefonevent.handle);
      telefonevent.handle = NULL;
    }
    if(powermanagment_status_event.handle) { 
      avm_event_source_release(powermanagment_status_event.handle);
      powermanagment_status_event.handle = NULL;
    }
#endif
#if defined(CONFIG_MIPS_UR8)
    avmpower_requestrelease();
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
    return;
}
module_exit(avm_power_cleanup);
#endif /*--- #ifdef CONFIG_AVM_POWER_MODULE ---*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_power_open(struct inode *inode __attribute__((unused)), struct file *filp) {
    struct _avm_power_open_data *open_data;

    DEB_INFO("[%s]: avm_power_open:\n", MODULE_NAME);

    /*-------------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------------*/
    open_data = (struct _avm_power_open_data *)kmalloc(sizeof(struct _avm_power_open_data), GFP_KERNEL);
    if(!open_data) {
        DEB_ERR("%s: avm_power_open: open malloc failed\n", MODULE_NAME);
        return -EFAULT;
    }
    memset(open_data, 0, sizeof(*open_data));
    filp->private_data = (void *)open_data;

    DEB_INFO("[%s]: avm_power_open: open success flags=0x%x\n", MODULE_NAME, filp->f_flags);
    return 0;
}

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
static int avm_power_close(struct inode *inode __attribute__((unused)), struct file *filp) {

    DEB_INFO("[%s]: avm_power_close:\n", MODULE_NAME);

    /*--- achtung auf ind wartende "gefreien" und warten bis alle fertig ---*/
    if(filp->private_data) {
        kfree(filp->private_data);
        filp->private_data = NULL;
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static ssize_t avm_power_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *write_pos) {
    char Buffer[256], *p;
    unsigned int org_write_length __attribute__((unused));
    struct _power_managment_dest_entry *powermodetab = NULL;
#if defined(CONFIG_FUSIV_VX180)
    int Mask;
#endif/*--- #if defined(CONFIG_FUSIV_VX180) ---*/
     
    if(write_pos != NULL) {
        DEB_INFO("[%s]: write_length = %u *write_pos = 0x%LX\n", "avm_power_write", write_length, *write_pos);
    }
    org_write_length = write_length;

    if(write_length >= sizeof(Buffer)) {
        write_length = sizeof(Buffer) - 1;
        DEB_NOTE("[avm_power] long line reduce to %u bytes\n", write_length);
    }
    if(filp == NULL) {
        memcpy(Buffer, write_buffer, write_length);
    } else {
        if(copy_from_user(Buffer, write_buffer, write_length)) {
            DEB_ERR("[%s]: avm_power_write: copy_from_user failed\n", "avm_power_write");
            return -EFAULT;
        }
    }
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    Buffer[write_length] = '\0';
    DEB_NOTE("[avm_power] avm_power_write org_len=%u len %u = '%s'\n", org_write_length, write_length, Buffer);
    p = strchr(Buffer, 0x0A);
    if(p) {
        *p = '\0';
        write_length = strlen(Buffer) + 1;
        DEB_NOTE("[avm_power] multi line reduce to %u bytes\n", write_length);
    }
    p = Buffer;

    /*--------------------------------------------------------------------------------------*\
     * cmd extrahieren
    \*--------------------------------------------------------------------------------------*/
    SKIP_SPACES(p);
    DEB_NOTE("[avm_power]: process input \"%s\"\n", p);
    
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    if(!strncmp(CMD_MODE, p, sizeof(CMD_MODE) - 1)) {
        p += sizeof(CMD_MODE) - 1;
        if(avm_power_write_find_special_char(&p, '=')) {
            avm_power_writeformaterror("[avm_power] format error: \""CMD_MODE" = <powermode>\"");
            /*--- return -EPERM; ---*/
            return write_length;
        }
        SKIP_SPACES(p);
        powermodetab = find_powermode(p, PM_ACESS_APPL);
#if defined(CONFIG_FUSIV_VX180)
    } else if(!strncmp(WE_CMD_MODE, p, sizeof(WE_CMD_MODE) - 1)) {
        p += sizeof(WE_CMD_MODE) - 1;
        if(avm_power_write_find_special_char(&p, '=')) {
            DEB_ERR("[avm_power] format error: \""WE_CMD_MODE" = <hexmask> : 0x0: help 0x10000: set only we-mask 0x800: simulate dying gasp\n");
            /*--- return -EPERM; ---*/
            return write_length;
        }
        SKIP_SPACES(p);
        sscanf(p, "%x", &Mask);
        if(Mask & 0x10000) {
            DEB_ERR("[avm_power] set we-mask: %x\n", Mask);
            WyattEarpMode = Mask;
        } else {
            Wyatt_Earp(Mask);
        }
        return write_length;
#endif /*--- defined(CONFIG_FUSIV_VX180) ---*/
#ifdef CONFIG_AVM_POWERMETER
    } else if(!strncmp(PMINFO_MODE, p, sizeof(PMINFO_MODE) - 1)) {
        p += sizeof(PMINFO_MODE) - 1;
        if(avm_power_write_find_special_char(&p, '=')) {
            DEB_ERR("[avm_power] format error: '%s'\n", p);
            DEB_ERR("[avm_power] use: \""PMINFO_MODE" = device, norm_rate, multiplier, divider, offset\"\n");
            /*--- return -EPERM; ---*/
            return write_length;
        }
        pm_ressourceinfo_scriptparse(p);
        return write_length;
    } else if(!strncmp(PMINFO_SET, p, sizeof(PMINFO_SET) - 1)) {
        p += sizeof(PMINFO_SET) - 1;
        if(avm_power_write_find_special_char(&p, '=')) {
            DEB_ERR("[avm_power] format error: '%s'\n", p);
            DEB_ERR("[avm_power] use: \""PMINFO_SET" = device, power_rate\n");
            return write_length;
        }
        pm_ressourceinfo_parse(p);
        return write_length;
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/
    } else if(!strncmp(ETH_MODE, p, sizeof(ETH_MODE) - 1)) {
        union  _powermanagment_ethernet_state eth;
        unsigned int tmp, throttle = ETH_NORMAL_DEMAND_CON;
        eth.Register = 0;
        p += sizeof(ETH_MODE) - 1;
        if(avm_power_write_find_special_char(&p, '=')) {
            DEB_ERR("[avm_power] format error: '%s'\n", p);
            DEB_ERR("[avm_power] use: \""ETH_MODE" = port, state\"\n");
            /*--- return -EPERM; ---*/
            return write_length;
        }
        SKIP_SPACES(p);
        sscanf(p, "%u", &tmp);
        if(tmp > 255) {
            DEB_ERR("[avm_power] : unknown port %u:\n", tmp);
            return write_length;
        }
        eth.Bits.port = tmp;
        tmp = 4;
        if(avm_power_write_find_special_char(&p, ',') == 0){
            SKIP_SPACES(p);
            sscanf(p, "%u", &tmp);
        }
        switch(tmp) {
            default:
                DEB_ERR("[avm_power] : unknown status - set status to powered(2)\n");
                tmp = 2;
                /*--- kein break ---*/
            case 0: 
            case 2: 
                eth.Bits.status = tmp;
                break;        
            case 1: 
                throttle = ETH_THROTTLE_DEMAND_CON;
                eth.Bits.status = tmp;
                break;
            case 3:
                eth.Bits.status = 1; /*--- powersave ohne throttle ---*/
                break;
        }

        if(eth.Bits.port < AVMPOWER_MAX_ETHERNETPORTS) {
            pm_ressourceinfo.eth_status[eth.Bits.port] = ETH_MASK_BITS(pm_ressourceinfo.eth_status[eth.Bits.port], 0x3 | ETH_THROTTLE_DEMAND_CON | ETH_NORMAL_DEMAND_CON, throttle) | eth.Bits.status;
            tmp = limit_set_eth(&pm_ressourceinfo, eth.Bits.port, 1);
        } else {
            tmp = 1;
        }
        if(tmp == 2) {
            DEB_ERR("[avm_power] : ethernet not registered\n");
        } else if(tmp) {
            DEB_ERR("[avm_power] : ethernet switch failed\n");
        }
        return write_length;
    } else if(!strncmp(LOAD_MODE, p, sizeof(LOAD_MODE) - 1)) {
        unsigned int val, scale = 1;
        p += sizeof(LOAD_MODE) - 1;
        if(avm_power_write_find_special_char(&p, '=')) {
            DEB_ERR("[avm_power] format error: '%s'\n", p);
            DEB_ERR("[avm_power] use: \""LOAD_MODE" = mode (0 auto, 1 off, > 1 Level\"\n");
            /*--- return -EPERM; ---*/
            return write_length;
        }
        SKIP_SPACES(p);
        sscanf(p, "%u", &val);
        if(avm_power_write_find_special_char(&p, ',') == 0) {
            SKIP_SPACES(p);
            sscanf(p, "%u", &scale);
        }
        avm_powermanager_load_control_set(val, scale);
    } else if(!strncmp(IDLE_MODE, p, sizeof(IDLE_MODE) - 1)) {
        p += sizeof(IDLE_MODE) - 1;
        if(avm_power_write_find_special_char(&p, '=')) {
            DEB_ERR("[avm_power] format error: '%s'\n", p);
            DEB_ERR("[avm_power] use: \""IDLE_MODE" = mode\"\n");
            /*--- return -EPERM; ---*/
            return write_length;
        }
        SKIP_SPACES(p);
        sscanf(p, "%x", &avm_power_disp_loadrate);
        printk(KERN_ERR"mode=0x%x\n", avm_power_disp_loadrate);
        return write_length;
    } else {
        avm_power_writeformaterror("[avm_power] format error: \""CMD_MODE" = <powermode>\"");
        /*--- return -EPERM; ---*/
        return write_length;
    }
    if(powermode_action(powermodetab)) {
        /*--- printk("[avm_power] error on activate powermode"); ---*/
        /*--- return -EPERM; ---*/
        return write_length;
    }
    return write_length;
}

/*-------------------------------------------------------------------------------------*\
 * hier werden die Tabelleneintraege (struct _power_managment_dest_entry ...[]) 
 * "ausgeführt"
\*-------------------------------------------------------------------------------------*/
static int powermode_action(struct _power_managment_dest_entry *powermodetab) {
    struct _power_managment_clients *registered_client;
    int ret = 0, i = 0, locked = 0;
    unsigned long flags;
    if(powermodetab == NULL) {
        return 1;
    }
    while(powermodetab[i].client_name) {
        registered_client = find_pwclient_by_name(powermodetab[i].client_name);
        if(registered_client == NULL) {
            if(powermodetab[i].mandatory & AVM_PM_CB_UNINSTALLED_OR_FAILED) {
                DEB_TRC_PT("[avm_power] '%s' not registered can't execute powermanagment ->stop\n", powermodetab[i].client_name);
                ret = 1;
            } else {
                DEB_TRC_PT("[avm_power] '%s' not registered can't execute powermanagment ->ignore\n", powermodetab[i].client_name);
            }
        } else {
            if((powermodetab[i].mandatory & AVM_PM_LOCK) && (locked == 0)) {
                locked = 1;
                flags = avm_power_lock();
            }
            if((ret = registered_client->CallBackPowerManagmentControl(powermodetab[i].state))) {
                if(powermodetab[i].mandatory & (AVM_PM_CB_UNINSTALLED_OR_FAILED | AVM_PM_CB_FAILED)) {
                    DEB_TRC_PT("[avm_power] '%s'=0x%x powermanagment failed->stop ret=%x\n", powermodetab[i].client_name, powermodetab[i].state, ret);
                } else {
                    DEB_TRC_PT("[avm_power] '%s'=0x%x powermanagment failed->ignore ret=%x\n", powermodetab[i].client_name, powermodetab[i].state, ret);
                    ret = 0;
                }
            } else {
                /*--- DEB_TRC_PT("[avm_power] '%s'=0x%x powermanagment ok\n", powermodetab[i].client_name, powermodetab[i].state); ---*/
            }
        }
        if(ret) {
            break;
        }
        i++;
    }
    if(locked) {
        avm_power_unlock(flags);
    }
    return ret;
}

/*--------------------------------------------------------------------------------*\
 * Powermanagegment nicht ueber Tabelle
 * ret 0: alles ok, 1: Callback-Fehler, 2: Client nicht registriert
 *      
\*--------------------------------------------------------------------------------*/
static int powermode_action_nolist(char *client_name, int State) {
    struct _power_managment_clients *registered_client;
    registered_client = find_pwclient_by_name(client_name);
    if(registered_client == NULL) {
        return 2;
    }
    if(registered_client->CallBackPowerManagmentControl(State)) {
        return 1;
    }
    return 0;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void avm_power_writeformaterror(char *text) {
    char TextBuf[256], *p;
    int writesize = sizeof(TextBuf);
    struct _power_managment *powertab = power;

    p = TextBuf;
    snprintf(p, writesize, "%s\navailable powermode:", text);
    writesize -= strlen(p);
    p         += strlen(p);
    while(powertab->powermode) {
        if(powertab->Access & PM_ACESS_APPL) {
            snprintf(p, writesize, "%s%s", powertab->powermode, (powertab+1)->powermode ? "," : "");
            writesize -= strlen(p);
            p         += strlen(p);
        }
        powertab++;
    }
    snprintf(p, writesize, "\n");
    DEB_ERR("%s", TextBuf);
}

/*-------------------------------------------------------------------------------------*\
 * access: Zugriff vom Treiber/Applikation PM_ACESS_DRIVER oder PM_ACESS_APPL
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_dest_entry *find_powermode(char *powermode_name, unsigned int access) {
    struct _power_managment *powertab = power;

    while(powertab->powermode) {
        if((strcmp(powermode_name, powertab->powermode) == 0) && (powertab->Access & access)) {
            /*--- DEB_ERR("[avm_power] exec powermanagment '%s'\n", powertab->powermode); ---*/
            return powertab->dests;
        }
        powertab++;
    }
    return NULL;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_clients *find_pwclient_by_name(char *client_name) {
    struct _power_managment_clients *client = (struct _power_managment_clients *)PwClientAnker;

    while(client) {
        if(strcmp(client_name, client->client_name) == 0) {
            return client;
        }
        client = client->next;
    }
    return NULL; 
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct _power_managment_clients *add_pwclient(char *client_name, int (*CallBackPowerManagmentControl)(int state)) {
    struct _power_managment_clients *new;
    unsigned long flags;

    new = kmalloc(sizeof(struct _power_managment_clients) + strlen(client_name) + 1, GFP_KERNEL);
    if(new == NULL) {
        return NULL;
    }
    new->client_name = (char *)new + sizeof(struct _power_managment_clients);
    strcpy(new->client_name, client_name);
    new->CallBackPowerManagmentControl = CallBackPowerManagmentControl;
    new->next = NULL;
    flags = avm_power_lock();
    new->next     = (struct _power_managment_clients *)PwClientAnker;
    PwClientAnker = new;
    avm_power_unlock(flags);
    return new; 
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void del_pwclient(struct _power_managment_clients *delclient) {
    struct _power_managment_clients *prevclient = NULL; 
    unsigned long flags = avm_power_lock();
    struct _power_managment_clients *client = (struct _power_managment_clients *)PwClientAnker;
    while(client) {
        if(client == delclient) {
            if(prevclient == NULL) {
                /*--- erste Element ---*/
                PwClientAnker = client->next;
            } else {
                prevclient->next = client->next;
            }
            avm_power_unlock(flags);
            kfree(client);
            return;
        }
        prevclient = client;
        client     = client->next;
    }
    avm_power_unlock(flags);
}

/*-------------------------------------------------------------------------------------*\
 * Powermanagment des Treibers anmelden
 * beide Parameter NULL : dying gasp
 * state: kontextbezogen - siehe (linux_)avm_power.h
\*-------------------------------------------------------------------------------------*/
void *PowerManagmentRegister(char *client_name, int (*CallBackPowerManagmentControl)(int state)){
    struct _power_managment_clients *client;
    DEB_INFO("[avm_power] PowerManagmentRegister(\"%s\", 0x%p)\n", client_name, CallBackPowerManagmentControl);

    if(client_name == NULL || CallBackPowerManagmentControl == NULL) {
        DEB_ERR("[avm_power]PowerManagmentRegister: invalid param %p %p\n", client_name, CallBackPowerManagmentControl);
        return NULL;
    }
    client = find_pwclient_by_name(client_name);
    if(client) {
        return client;
    }
    return add_pwclient(client_name, CallBackPowerManagmentControl);
}
EXPORT_SYMBOL(PowerManagmentRegister);

/*-------------------------------------------------------------------------------------*\
 * Powermanagment des Treibers abmelden
\*-------------------------------------------------------------------------------------*/
void PowerManagmentRelease(void *Handle){
    struct _power_managment_clients *delclient = (struct _power_managment_clients *)Handle;
    if(Handle == NULL) {
        DEB_ERR("[avm_power]PowerManagmentRelease: invalid Handle\n");
        return;
    }
    del_pwclient(delclient);
}
EXPORT_SYMBOL(PowerManagmentRelease);

/*-------------------------------------------------------------------------------------*\
 * vom Kernel den Powermode ändern
 * Returnwert: 0 ok sonst Abbruch mit Fehler
\*-------------------------------------------------------------------------------------*/
int PowerManagmentActivatePowerMode(char *powermode_name){
    struct _power_managment_dest_entry *powermodetab = find_powermode(powermode_name, PM_ACESS_DRIVER);
#if defined(DEBUG_TRACE_POWERTAB)
    if(powermodetab !=  teactive_entries) {
        DEB_TRC_PT("[avm_power]PowerManagmentActivatePowerMode: '%s'\n", powermode_name);
    }
#endif
    return powermode_action(powermodetab);
}

EXPORT_SYMBOL(PowerManagmentActivatePowerMode);
#if defined(CONFIG_MIPS_UR8)
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avm_power_wlan_Callback(int state){

    unsigned int value;

#define DEV_SARAS_NM_PWDREG_ADDR       (0xA1018000 | 0x38)
#define DEV_SARAS_PM_NM_REG1_ADDR      (0xA1018000 | 0x0c)

#define DEV_SARAS_NM_PWD               (0xAB)

    /*--- printk("avm_power_wlan_Callback: %d\n", state); ---*/
    *(volatile unsigned int *)DEV_SARAS_NM_PWDREG_ADDR = DEV_SARAS_NM_PWD;

    switch (state) {
        case 1:     /*--- on ---*/
            value = *(volatile unsigned int *)DEV_SARAS_PM_NM_REG1_ADDR;
            /*--- printk("[wlan_booston] 0x%x 0x%x\n", DEV_SARAS_PM_NM_REG1_ADDR, value); ---*/
            *(volatile unsigned int *)DEV_SARAS_PM_NM_REG1_ADDR = value | (1<<6);  /*--- set 1.2V ---*/ 
            break;
        case 0:     /*--- off ---*/
            value = *(volatile unsigned int *)DEV_SARAS_PM_NM_REG1_ADDR;
            /*--- printk("[wlan_boostoff] 0x%x 0x%x\n", DEV_SARAS_PM_NM_REG1_ADDR, value); ---*/
            *(volatile unsigned int *)DEV_SARAS_PM_NM_REG1_ADDR = value & ~(1<<6);  /*--- set 1.1V ---*/ 
            break;
        default:
            break;
    }

    *(volatile unsigned int *)DEV_SARAS_NM_PWDREG_ADDR = 0;

    return 0;
}
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avm_power_usb_Callback(int state){
    /*--- printk("avm_power_usb_Callback: %d\n", state); ---*/
    if(state < 2) {
#if defined(CONFIG_MIPS_UR8)
        avm_gpio_out_bit(GPIO_BIT_DRVVBUS, state ? 1 : 0);
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
    } else {
        /*--- liefere SUM(mA) zurueck ---*/
        return pm_ressourceinfo.deviceinfo[powerdevice_usb_host].power_rate +
               pm_ressourceinfo.deviceinfo[powerdevice_usb_host2].power_rate +
               pm_ressourceinfo.deviceinfo[powerdevice_usb_host3].power_rate;
    }
    return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avm_power_event_Callback(int state){
    /*--- printk("avm_power_event_Callback: %d\n", state); ---*/
    if(telefonevent.on != (unsigned)state) {
        telefonevent.on = state;
#if defined(DECTSYNC_PATCH) 
        if(telefonevent.dectsyncmode) {
            state = 0;
        } 
        start_dectsync(0, state);
#endif/*--- #if defined(DECTSYNC_PATCH)  ---*/
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
        avmevent_telefonprofile_notify(&telefonevent, avm_event_id_telefonprofile);
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/
#if defined(CONFIG_AVM_PA)
        avm_pa_telefon_state(telefonevent.on);
#endif /*--- defined(CONFIG_AVM_PA) ---*/
    }
    return 0;
}
#define GOVERNOR_CPUFREQ_FILENAME  "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
#define GOVERNOR_CPUFREQ_CURFREQ   "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void write_to_file(char *filename, char *str, int len) {
    struct file *fp;
    mm_segment_t oldfs;

    fp = filp_open(filename, O_WRONLY, 00);
    if(IS_ERR(fp)) {
        /*--- printk(KERN_ERR " failed: open : %s\n", filename); ---*/
        return;
    }
    /* Schreibzugriff auf File(system) erlaubt? */
    if (fp->f_op->write == NULL) {
        printk(KERN_ERR "[avm_power]speedstep failed: write %s\n", filename);
        filp_close(fp, NULL);
        return;
    }
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    fp->f_pos = 0; /* Von Anfang an schreiben*/
    fp->f_op->write(fp, str, len, &fp->f_pos);
    set_fs(oldfs);
    filp_close(fp, NULL); /* Close the file */
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static unsigned int read_from_file(char *filename, char *str, int maxlen) {
    struct file *fp;
    mm_segment_t oldfs;
    int bytesRead;

    fp = filp_open(filename, O_RDONLY, 00);
    if(IS_ERR(fp)) {
        str[0] = 0;
        /*--- printk(KERN_ERR " failed: open scaling_governor: %s\n", filename); ---*/
        return 0;
    }
    /* Lesezugriff auf File(system) erlaubt? */
    if (fp->f_op->read == NULL) {
        str[0] = 0;
        /*--- printk(KERN_ERR "[avm_power]speedstep failed: read %s\n", filename); ---*/
        filp_close(fp, NULL);
        return 0;
    }
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    fp->f_pos = 0; /* Von Anfang an lesen */
    bytesRead = fp->f_op->read(fp, str, maxlen, &fp->f_pos);
    if(bytesRead < 0) {
        bytesRead = 0;
    }
    str[bytesRead] = 0;
    set_fs(oldfs);
    filp_close(fp, NULL); /* Close the file */
    return bytesRead;
}
/*--------------------------------------------------------------------------------*\
 * beliebiger Kontext
 * state: 0 kein Takt umschalten        (dsl)
 *        1 Takt umschaltbar            (ata)
 *
 *      verundet mit Telefonie-Profile:
 *
 *        0x10 kein Takt umschalten
 *        0x11 Takt umschalten
 *
 *        0x20: Cpufreq-Semaphore -> Auschalten
 *        0x21: Cpufreq-Semaphore  hochzaehlen -> wenn 0: wieder anschalten
 *        0x40: Abfrage des aktuellen speedstep-Status: 0 an bzw. pending 1: garantiert aus
 *        0x80: printk-Ausgabe aktueller speedstepstatus
 *        (da nicht im gleichen Kontext) 
\*--------------------------------------------------------------------------------*/
static int avm_power_speedstep_Callback(int state){
    unsigned long flags = avm_power_lock();
    DEB_TRC_PT("%s(%x): speedstep=%x govfreq=%x sema=%d\n", __func__, state, pm_ressourceinfo.speedstep_state,
                                                                      pm_ressourceinfo.act_gov_cpufreqstate,
                                                                      pm_ressourceinfo.speedstep_sema);
    switch(state) {
        case 0x00: pm_ressourceinfo.speedstep_state &= ~0x1; break;
        case 0x10: pm_ressourceinfo.speedstep_state &= ~0x2; break;
        case 0x01: pm_ressourceinfo.speedstep_state |=  0x1; break;
        case 0x11: pm_ressourceinfo.speedstep_state |=  0x2; break;
        case 0x20:
            pm_ressourceinfo.speedstep_sema--; 
            if(pm_ressourceinfo.speedstep_sema == -1) {
                pm_ressourceinfo.speedstep_state |=  0x8; /*--- pending:  ---*/
                pm_ressourceinfo.speedstep_state &= ~0x4;
            }
            break;
        case 0x21: 
            if(pm_ressourceinfo.speedstep_sema < 0) {
                pm_ressourceinfo.speedstep_sema++;
                if(pm_ressourceinfo.speedstep_sema == 0) {
                    pm_ressourceinfo.speedstep_state |= 0x8; /*--- pending ---*/
                    pm_ressourceinfo.speedstep_state |= 0x4;
                }
            }
            break;
        case 0x40:
            if(pm_ressourceinfo.speedstep_state == pm_ressourceinfo.act_gov_cpufreqstate) {
                /*--- also auch nicht pending ---*/
                if((pm_ressourceinfo.act_gov_cpufreqstate & 0x1) == 0 ||
                   (pm_ressourceinfo.act_gov_cpufreqstate & 0x4) == 0
                   ) {
                    avm_power_unlock(flags);
                    /*--- es ist garantiert das speedstep aus ---*/
                    return 1;
                }
            }
            break;
        case 0x80: {
            char buf[128];
            avm_power_unlock(flags);
            read_from_file(GOVERNOR_CPUFREQ_FILENAME, buf, sizeof(buf) - 1), 
            printk("governor: %smask=%x sema=%d\n", buf,
                                    pm_ressourceinfo.act_gov_cpufreqstate,
                                    pm_ressourceinfo.speedstep_sema
                                    );
            return 0;
            }
    }
    pm_ressourceinfo.Changes++;
    avm_power_unlock(flags);
    wake_up_interruptible(&pm_ressourceinfo.wait_queue);
    return 0;
}
/*--------------------------------------------------------------------------------*\
 * im Kontext des Threads
\*--------------------------------------------------------------------------------*/
static void avm_power_speedstep_mode(void) {
    char *p;
    unsigned int act_gov_cpufreqstate;  
    unsigned long flags = avm_power_lock();

    if(pm_ressourceinfo.speedstep_state == pm_ressourceinfo.act_gov_cpufreqstate) {
        avm_power_unlock(flags);
        return;
    }
    pm_ressourceinfo.speedstep_state &= ~0x8; /*--- pending loeschen ---*/
    act_gov_cpufreqstate              = pm_ressourceinfo.speedstep_state;
    avm_power_unlock(flags);
    /* Disable parameter checking */
    p = (act_gov_cpufreqstate == 0x7) ? "ondemand" : "performance";
    write_to_file(GOVERNOR_CPUFREQ_FILENAME, p, strlen(p) + 1);
    flags = avm_power_lock();
    pm_ressourceinfo.act_gov_cpufreqstate = act_gov_cpufreqstate;
    avm_power_unlock(flags);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avm_power_adsl_event_Callback(int state){
    if(powermanagment_status_event.dsl_status == state) {
        /*--- keine gleichen Werte triggern ---*/
        return 1;
    }
    powermanagment_status_event.dsl_status = state; 
    /*--- printk("avm_power_adsl_event_Callback: %d\n", state); ---*/
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
    avm_event_powermanagment_status_notify(&powermanagment_status_event, avm_event_id_powermanagment_status);
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/
    if(pm_ressourceinfo.thread_id) {
        /*--- triggere thread  ---*/
        pm_ressourceinfo.Changes++;
        wake_up_interruptible(&pm_ressourceinfo.wait_queue);
    }
    return 0;
}
#ifdef CONFIG_AVM_POWERMETER
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
static void pm_ressourceinfo_notify(void *context, enum _avm_event_id id) {
    struct _power_managment_ressource_info *pm_info = (struct _power_managment_ressource_info *)context;
    struct _avm_event_pm_info_stat *event;
    int handled;

	if(id != avm_event_id_pm_ressourceinfo_status){
        printk(KERN_WARNING "[avm_power]unknown event: %d\n", id);
        return;
    }
	event = (struct _avm_event_pm_info_stat *)kmalloc(sizeof(struct _avm_event_pm_info_stat), GFP_ATOMIC);
    if(event == NULL) {
        printk(KERN_WARNING "[avm_power]can't alloc event: %d\n", id);
        return;
    }
    memcpy(event, &pm_info->stat, sizeof(struct _avm_event_pm_info_stat));
	event->header.id = id;
	handled = avm_event_source_trigger(pm_info->event_handle, id, sizeof(struct _avm_event_pm_info_stat), event);
    if(handled == 0) {
        printk(KERN_WARNING "[avm_power]event: %d not handled\n", id);
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void avmevent_telefonprofile_notify(void *context, enum _avm_event_id id) {
    struct _telefonieprofile *ptp = (struct _telefonieprofile *)context;
    struct _avm_event_telefonprofile *event;
    int handled;

	if(id != avm_event_id_telefonprofile){
        printk(KERN_WARNING "[avm_power]unknown event: %d\n", id);
        return;
    }
	event = (struct _avm_event_telefonprofile *)kmalloc(sizeof(struct _avm_event_telefonprofile), GFP_ATOMIC);
    if(event == NULL) {
        printk(KERN_WARNING "[avm_power]can't alloc event: %d\n", id);
        return;
    }
	event->event_header.id = id;
	event->on              = ptp->on;
	handled = avm_event_source_trigger(ptp->handle, id, sizeof(struct _avm_event_telefonprofile), event);
    if(handled == 0) {
        /*--- printk(KERN_WARNING "[avm_power]event: %d not handled\n", id); ---*/
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void avm_event_powermanagment_status_notify(void *context, enum _avm_event_id id){
    struct _powermanagment_status_event *ppm = (struct _powermanagment_status_event *)context;
    struct _avm_event_powermanagment_status *event;
    int handled;

	if(id != avm_event_id_powermanagment_status){
        printk(KERN_WARNING "[avm_power]unknown event: %d\n", id);
        return;
    }
	event = (struct _avm_event_powermanagment_status *)kmalloc(sizeof(struct _avm_event_powermanagment_status), GFP_ATOMIC);
    if(event == NULL) {
        printk(KERN_WARNING "[avm_power]can't alloc event: %d\n", id);
        return;
    }
	event->event_header.id = id;
	event->substatus = dsl_status;
	event->param.dsl_status = ppm->dsl_status;
	handled = avm_event_source_trigger(ppm->handle, id, sizeof(struct _avm_event_powermanagment_status), event);
    if(handled == 0) {
        /*--- printk(KERN_WARNING "[avm_power]event: %d not handled\n", id); ---*/
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void avmevent_temperature_notify(void *context, enum _avm_event_id id) {
    struct _power_managment_ressource_info *pm_info = (struct _power_managment_ressource_info *)context;
    struct _avm_event_temperature *event;
    int handled;

	if(id != avm_event_id_temperature){
        printk(KERN_WARNING "[avm_power]unknown event: %d\n", id);
        return;
    }
	event = (struct _avm_event_temperature *)kmalloc(sizeof(struct _avm_event_temperature), GFP_ATOMIC);
    if(event == NULL) {
        printk(KERN_WARNING "[avm_power]can't alloc event: %d\n", id);
        return;
    }
	event->event_header.id = id;
	event->temperature     = get_temperature(pm_info);

    if(avm_power_disp_loadrate & 8) {
        printk(KERN_ERR "[avm_power]temperature event: %d\n", event->temperature);
    }
	handled = avm_event_source_trigger(pm_info->temperature_eventhandle, id, sizeof(struct _avm_event_temperature), event);
    if(handled == 0) {
        printk(KERN_WARNING "[avm_power]event: %d not handled\n", id);
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void avmevent_cpu_idle_notify(void *context, enum _avm_event_id id) {
    struct _power_managment_ressource_info *pm_info = (struct _power_managment_ressource_info *)context;
    struct _avm_event_cpu_idle *event;
    struct sysinfo meminfo;
/*--- 	unsigned long inactive; ---*/
	unsigned long active;
	unsigned long free;
    unsigned long hard_mem;
    int handled;

	if(id != avm_event_id_cpu_idle){
        printk(KERN_WARNING "[avm_power]unknown event: %d\n", id);
        return;
    }
	event = (struct _avm_event_cpu_idle *)kmalloc(sizeof(struct _avm_event_cpu_idle), GFP_ATOMIC);
    if(event == NULL) {
        printk(KERN_WARNING "[avm_power]can't alloc event: %d\n", id);
        return;
    }
	event->event_header.id = id;
	event->cpu_idle       = pm_info->deviceinfo[powerdevice_loadrate].power_rate > 100 ? 0 : 100 - pm_info->deviceinfo[powerdevice_loadrate].power_rate;
	event->dsl_dsp_idle   = pm_info->deviceinfo[powerdevice_dsp_loadrate].power_rate > 100 ? 0 : 100 - pm_info->deviceinfo[powerdevice_dsp_loadrate].power_rate;
	event->voice_dsp_idle = pm_info->deviceinfo[powerdevice_vdsp_loadrate].power_rate > 100 ? 0 : 100 - pm_info->deviceinfo[powerdevice_vdsp_loadrate].power_rate;

	free     = global_page_state(NR_FREE_PAGES);
    /*--- inactive = global_page_state(NR_INACTIVE_ANON) + global_page_state(NR_INACTIVE_FILE); ---*/
    active   = global_page_state(NR_ACTIVE_ANON) + global_page_state(NR_ACTIVE_FILE);

    si_meminfo(&meminfo);
	si_swapinfo(&meminfo);

    event->mem_physfree     = (unsigned char)((meminfo.freeram * 100) / (meminfo.totalram |1));
    hard_mem = global_page_state(NR_FILE_DIRTY) + global_page_state(NR_WRITEBACK) + global_page_state(NR_FILE_MAPPED) +
               global_page_state(NR_SLAB_UNRECLAIMABLE) +
               global_page_state(NR_ANON_PAGES); 
    if(active > hard_mem) hard_mem = active;

    event->mem_strictlyused = (unsigned char)((hard_mem * 100) / (meminfo.totalram |1));
    event->mem_cacheused    = (unsigned char)(100 - event->mem_strictlyused - event->mem_physfree); 

    if(avm_power_disp_loadrate & 4) {
        printk(KERN_ERR"[avm_power_disp_loadrate] cpu-idle-event: MEM: %d %d %d %%, active=%ld free=%ld/%ld\n", 
                        event->mem_strictlyused, event->mem_cacheused, event->mem_physfree, active, free, meminfo.freeram);
    }
	handled = avm_event_source_trigger(pm_info->cpu_idle_eventhandle, id, sizeof(struct _avm_event_cpu_idle), event);

    if(handled == 0) {
        printk(KERN_WARNING "[avm_power]event: %d not handled\n", id);
    }
}
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
__inline static unsigned int PM_GET_MWATT(struct _power_managment_device_info *df) {
    unsigned int ret = 0;
    if(df->divider != 0) {
        ret = (((PM_GET_RATE(df->power_rate))* df->multiplier) / df->divider) + df->offset;
    }
    return ret;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
__inline static unsigned int PM_GET_NORM_MWATT(struct _power_managment_device_info *df) {
    unsigned int ret = 0;
    if(df->divider != 0) {
        ret = (((PM_GET_RATE(df->norm_power_rate))* df->multiplier) / df->divider) + df->offset;
    }
    return ret;
}

#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 

#define PM_CALC_SUM(old, new, messure)  ((old) + ((new) * (messure)))
#define PM_TIME_DIFF(low1, low2) (((low1) - (low2)) < 0 ?  -((low1) - (low2)) : ((low1) - (low2)))
/*--------------------------------------------------------------------------------*\
 * kumulierte Werte berechnen
\*--------------------------------------------------------------------------------*/
static void pm_process_cum(struct _power_managment_ressource_info *pm_info) {
    int i, intervall = 0;
    struct _power_managment_cum_info *pcum;
    long MessureSamples, MessureSampleperPeriod;
    long tail_messure;
    int rate_dspcum = 0;
    int rate_systemcum = 0;
    int rate_sumcum = 0;
    int rate_wlancum = 0;
    int rate_ethcum = 0;
    int rate_abcum = 0;
    int rate_dectcum = 0;
    int rate_battcum = 0; 
    int rate_usbhostcum = 0; 
    int rate_ltecum = 0; 
    int rate_temp = 0;
    signed char min_temp, max_temp; 

    MessureSamples = PM_TIME_DIFF((long)jiffies, (long)pm_info->LastMessureTimeStamp) / HZ;
    if(MessureSamples == 0) {
        /*--- ein Messpunkt pro Sekunde: wenn rate_.. max. 100: 40 * 10e6 Samples (kein Overflow zu berücksichtigen!) ---*/
        return;
    }
    pm_info->LastMessureTimeStamp = jiffies;
    /*--- Gesamt-Zeitfenster: PM_TIME_INTERVALL mit PM_TIME_ENTRIES Teilzeitfenstern ---*/
    MessureSampleperPeriod = PM_TIME_INTERVALL / PM_TIME_ENTRIES; /*--- die Anzahl der Sample pro Teilzeitfenster ---*/
    do {
        pcum = &pm_info->cum[pm_info->ActTimeEntry];
        tail_messure = min(MessureSampleperPeriod - pcum->messure_count, MessureSamples);
/*--- printk("[%ld j:%ld]Samples: %ld tail_messure: %d act_entry %ld cnt %d\n", pm_info->LastMessureTimeStamp, jiffies, MessureSamples,  tail_messure, pm_info->ActTimeEntry, pcum->messure_count); ---*/
        if(tail_messure) {
            /*--- die Werte über Restmesspunkte dieses Teilzeitfensters integrieren ---*/
            pcum->rate_sum          =  PM_CALC_SUM(pcum->rate_sum,    pm_info->stat.rate_sumact,    tail_messure);
            pcum->rate_system       =  PM_CALC_SUM(pcum->rate_system, pm_info->stat.rate_systemact, tail_messure);
            pcum->rate_dsp          =  PM_CALC_SUM(pcum->rate_dsp,    pm_info->stat.rate_dspact,    tail_messure);
            pcum->rate_wlan         =  PM_CALC_SUM(pcum->rate_wlan,   pm_info->stat.rate_wlanact,   tail_messure);
            pcum->rate_eth          =  PM_CALC_SUM(pcum->rate_eth,    pm_info->stat.rate_ethact,    tail_messure);
            pcum->rate_ab           =  PM_CALC_SUM(pcum->rate_ab,     pm_info->stat.rate_abact,     tail_messure);
            pcum->rate_dect         =  PM_CALC_SUM(pcum->rate_dect,   pm_info->stat.rate_dectact,   tail_messure);
            pcum->rate_battcharge   =  PM_CALC_SUM(pcum->rate_battcharge, pm_info->stat.rate_battchargeact, tail_messure);
            pcum->rate_usbhost      =  PM_CALC_SUM(pcum->rate_usbhost,    pm_info->stat.rate_usbhostact,   tail_messure);
            pcum->rate_lte          =  PM_CALC_SUM(pcum->rate_lte,    pm_info->stat.rate_lteact,   tail_messure);
            pcum->rate_temp         =  PM_CALC_SUM(pcum->rate_temp, pm_info->stat.act_temperature,   tail_messure);

            if(pcum->min_temp > pm_info->stat.act_temperature){
                pcum->min_temp = pm_info->stat.act_temperature;
            }
            if(pcum->max_temp < pm_info->stat.act_temperature){
                pcum->max_temp = pm_info->stat.act_temperature;
            }
        }
        MessureSamples      -= tail_messure;
        pcum->messure_count += tail_messure;
        if(MessureSamples) {
            /*--- Überlauf in neues Teilzeitfenster  ---*/
            pm_info->ActTimeEntry++;
            if(pm_info->ActTimeEntry >= PM_TIME_ENTRIES) {
                pm_info->ActTimeEntry = 0;
            }
            pcum = &pm_info->cum[pm_info->ActTimeEntry];
            memset(pcum, 0 , sizeof(pm_info->cum[0]));
            pcum->min_temp = pcum->max_temp = pm_info->stat.act_temperature;
        }
    } while(MessureSamples);

    min_temp = max_temp = pm_info->stat.act_temperature; 
    for(i = 0; i < PM_TIME_ENTRIES; i++) {
        /*--- Mittelwert ueber komplettes TIMER_INTERVALL ---*/
        pcum = &pm_info->cum[i];
        if(pcum->messure_count) {
            /*--- Addition der einzelnen Teilzeitfenster ---*/
            rate_sumcum     += pcum->rate_sum;
            rate_systemcum  += pcum->rate_system; 
            rate_dspcum     += pcum->rate_dsp; 
            rate_wlancum    += pcum->rate_wlan; 
            rate_ethcum     += pcum->rate_eth;
            rate_abcum      += pcum->rate_ab;
            rate_dectcum    += pcum->rate_dect;
            rate_battcum    += pcum->rate_battcharge;
            rate_usbhostcum += pcum->rate_usbhost;
            rate_ltecum     += pcum->rate_lte;
            rate_temp       += pcum->rate_temp;
            if(min_temp > pcum->min_temp){
                min_temp = pcum->min_temp;
            }
            if(max_temp < pcum->max_temp) {
                max_temp = pcum->max_temp;
            }
            intervall += pcum->messure_count;
        }
    }
/*--- printk("intervall: %d\n", intervall); ---*/
    if(intervall) {
        pm_info->stat.rate_sumcum        = (unsigned char)min(100, rate_sumcum     / intervall);  /*--- kumulierter Energieverbrauch in Prozent ---*/
        pm_info->stat.rate_systemcum     = (unsigned char)min(100, rate_systemcum  / intervall);  /*--- (gewichtete) kumulierte Aktivität MIPS, System und idle in Prozent ---*/
        pm_info->stat.rate_dspcum        = (unsigned char)min(100, rate_dspcum     / intervall);  /*--- kumulierte Aktivität DSP in Prozent ---*/
        pm_info->stat.rate_wlancum       = (unsigned char)min(100, rate_wlancum    / intervall);  /*--- kumulierter Verbrauch WLAN in Prozent ---*/
        pm_info->stat.rate_ethcum        = (unsigned char)min(100, rate_ethcum     / intervall);  /*--- kumulierter Ethernet-Verbrauch ---*/
        pm_info->stat.rate_abcum         = (unsigned char)min(100, rate_abcum      / intervall);  /*--- kumulierter Nebenstellen-Verbrauch in Prozent ---*/
        pm_info->stat.rate_dectcum       = (unsigned char)min(100, rate_dectcum    / intervall);  /*--- kumulierter Verbrauch DECT in Prozent ---*/
        pm_info->stat.rate_battchargecum = (unsigned char)min(100, rate_battcum    / intervall);  /*--- kumulierter Verbrauch Battery-Charge in Prozent ---*/
        pm_info->stat.rate_usbhostcum    = (unsigned char)min(100, rate_usbhostcum / intervall);  /*--- kumulierter Verbrauch USB-Host in Prozent ---*/
        pm_info->stat.rate_ltecum        = (unsigned char)min(100, rate_ltecum     / intervall);  /*--- kumulierter Verbrauch LTE in Prozent ---*/
        pm_info->stat.avg_temperature    = rate_temp / intervall;
        pm_info->stat.min_temperature    = min_temp;
        pm_info->stat.max_temperature    = max_temp;
    }
}
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/

/*--------------------------------------------------------------------------------*\
 * die Infos werden erst hier zusammengesammelt, Status wird mit Set/Reset-Flag
 * signalisiert, Power wird hier gesetzt
\*--------------------------------------------------------------------------------*/
static void convert_isdnpowerstate(unsigned int *SaveValue, unsigned int Value) {
    int i;
    /*--- Ebene1/3 State uebernehmen ---*/
    if(Value & (1 << 31)){ /* Set State */
        *SaveValue |= Value; 
    } else {               /* Reset State */
       *SaveValue &= ~Value; 
    }
    *SaveValue &= ~0xFFFF;  /*--- Powerratebereich loeschen ---*/
    for(i = 1; i < 5; i++) {
        /*--- max. 4 Controller E1 aktiv -> power++ ! ---*/
        if(*SaveValue & PM_E1STATUS(i)) {
            *SaveValue += 100;
        }
    }
    /*--- DBG_ERR(("convert_isdnpowerstate: %x\n", *SaveValue)); ---*/
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int limit_set_eth(struct _power_managment_ressource_info *pm_info, unsigned int eth_idx, unsigned int force) {
    union  _powermanagment_ethernet_state eth;
    unsigned int eth_status = pm_info->eth_status[eth_idx];

    if(ETH_THROTTLE_DEMAND(eth_status)) {
        /*--- printk("[avm_power] eth: port %u throttle demand %s%s (t%d) status=%x\n", eth_idx, eth_status & ETH_THROTTLE_DEMAND_POLICY ? " POLICY" : "", eth_status & ETH_THROTTLE_DEMAND_CON    ? " CON" : "", pm_info->stat.act_temperature, eth_status); ---*/
        force   |= !ETH_THROTTLE_ACTIVE(eth_status); /*--- wenn vorher keine aktiv dann mit Sicherheit jetzt ---*/
        if(force) {
            printk("[avm_power] eth: port %u force throttle %s%s t%d\n", eth_idx,   
                    eth_status & ETH_THROTTLE_DEMAND_POLICY ? " POLICY" : "", 
                    eth_status & ETH_THROTTLE_DEMAND_CON    ? " CON" : "",
                    pm_info->stat.act_temperature);
        }
        if(eth_status & ETH_THROTTLE_DEMAND_POLICY) {
            eth_status = ETH_MASK_BITS(eth_status, ETH_THROTTLE_DEMAND_POLICY, ETH_THROTTLE_ACTIVE_POLICY);
        }
        if(eth_status & ETH_THROTTLE_DEMAND_CON) {
            eth_status = ETH_MASK_BITS(eth_status, ETH_THROTTLE_DEMAND_CON, ETH_THROTTLE_ACTIVE_CON);
        }
        pm_info->eth_status[eth_idx] = eth_status;
    } else if(ETH_NORMAL_DEMAND(eth_status)) {
        unsigned int active = ETH_THROTTLE_ACTIVE(eth_status);
        /*--- printk("[avm_power] eth: port %u normal demand %s%s t%d status=%x\n", eth_idx, eth_status & ETH_NORMAL_DEMAND_POLICY ? " POLICY" : "", eth_status & ETH_NORMAL_DEMAND_CON    ? " CON" : "",  pm_info->stat.act_temperature, eth_status); ---*/
        if(eth_status & ETH_NORMAL_DEMAND_POLICY) {
            eth_status = ETH_MASK_BITS(eth_status, ETH_NORMAL_DEMAND_POLICY | ETH_THROTTLE_ACTIVE_POLICY, 0);
        }
        if(eth_status & ETH_NORMAL_DEMAND_CON) {
            eth_status = ETH_MASK_BITS(eth_status, ETH_NORMAL_DEMAND_CON | ETH_THROTTLE_ACTIVE_CON, 0);
        }
        force   |= (active != ETH_THROTTLE_ACTIVE(eth_status)) ? 1 : 0;
        if(force) {
            printk("[avm_power] eth: port %u force normal %s%s t%d\n", eth_idx, 
                    pm_info->eth_status[eth_idx] & ETH_NORMAL_DEMAND_POLICY ? " POLICY" : "", 
                    pm_info->eth_status[eth_idx] & ETH_NORMAL_DEMAND_CON ? " CON" : "",  
                    pm_info->stat.act_temperature );
        }
        pm_info->eth_status[eth_idx] = eth_status;
    }
    if(force == 0) {
        return 0;
    }
    eth.Register           = 0;
    eth.Bits.port          = eth_idx;
    eth.Bits.status        = pm_info->eth_status[eth_idx] & 0x3;
    eth.Bits.throttle_eth  = ETH_THROTTLE_ACTIVE(eth_status);
    if(avm_power_disp_loadrate & 0x10) {
        printk("[avm_power] eth: port %d status %d throttle %d%s%s t%d\n", 
                eth.Bits.port, eth.Bits.status, eth.Bits.throttle_eth, 
                eth_status & ETH_THROTTLE_ACTIVE_POLICY ? " POLICY" : "", 
                eth_status & ETH_THROTTLE_ACTIVE_CON    ? " CON" : "",
                pm_info->stat.act_temperature);
    }
    return powermode_action_nolist("ethernet", eth.Register);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int pm_ressourceinfo_thread( void *data ) {
    struct _power_managment_ressource_info *pm_info = (struct _power_managment_ressource_info *)data;
    unsigned long flags;
    unsigned int policy;
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
    int device, mWatt, norm_p, rate, normmwatt, usbmwatt;
    int init = 1, activ_ethports, i;
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/
    /*--- DEB_ERR("[avm_power]pm_ressourceinfo_thread: start\n"); ---*/
    daemonize("pm_info");
    allow_signal(SIGTERM ); 
    for(;;) {
        if(wait_event_interruptible( pm_info->wait_queue, pm_info->Changes)){
            break;
        }
        flags = avm_power_lock();
        pm_info->Changes--;
        avm_power_unlock(flags);
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
        if(init) {
            /*--- Startphase ignorieren ---*/
            init = 0;
            pm_info->LastMessureTimeStamp = jiffies;
            pm_info->ActTimeEntry = 0;
        }
        pm_info->stat.act_temperature    = (signed char)get_temperature(pm_info);
        /*--- auf Grundlage der letzten Werte kummulierte Werte ermitteln ---*/
        pm_process_cum(pm_info);
        mWatt = 0;
        for(device = 0; device < powerdevice_maxdevices; device++) {
            mWatt += PM_GET_MWATT(&pm_info->deviceinfo[device]);
        }
        pm_info->stat.rate_sumact = (mWatt * 100 ) / (pm_info->NormP | 1);  /*--- aktueller Energieverbrauch in Prozent ---*/
        norm_p= ((pm_info->deviceinfo[powerdevice_cpuclock].norm_power_rate)   + 
                 (pm_info->deviceinfo[powerdevice_systemclock].norm_power_rate) + 
                 (pm_info->deviceinfo[powerdevice_loadrate].norm_power_rate));
        rate= 0;
        if(pm_info->deviceinfo[powerdevice_cpuclock].norm_power_rate) {
            rate += (pm_info->deviceinfo[powerdevice_cpuclock].power_rate);
        }
        if(pm_info->deviceinfo[powerdevice_systemclock].norm_power_rate) {
            rate += (pm_info->deviceinfo[powerdevice_systemclock].power_rate);
        }
        if(pm_info->deviceinfo[powerdevice_loadrate].norm_power_rate) {
            rate += (pm_info->deviceinfo[powerdevice_loadrate].power_rate);
        }
/*--- printk("rate: %d norm_p %d\n", rate, norm_p); ---*/
        pm_info->stat.rate_systemact = (rate * 100) / (norm_p | 1);
        pm_info->stat.system_status = 0xFF;  /*--- unbekannt ---*/
        pm_info->stat.rate_dspact   = PM_GET_RATE(pm_info->deviceinfo[powerdevice_dsl].power_rate);          /*--- aktueller Aktivität DSL in Prozent ---*/
        pm_info->stat.rate_wlanact  = PM_GET_RATE(pm_info->deviceinfo[powerdevice_wlan].power_rate);         /*--- aktueller Verbrauch WLAN in Prozent ---*/
        pm_info->stat.wlan_devices  = PM_WLAN_GET_DEVICES(pm_info->deviceinfo[powerdevice_wlan].power_rate); /*--- angemeldete WLAN-Devices ---*/
        pm_info->stat.wlan_status   = PM_WLAN_GET_ECO(pm_info->deviceinfo[powerdevice_wlan].power_rate);     /*--- WLAN-Status (1= ECO) ---*/
        pm_info->stat.rate_ethact   = (PM_GET_RATE(pm_info->deviceinfo[powerdevice_ethernet].norm_power_rate) == 0) ? 0 : 
                                      (PM_GET_RATE(pm_info->deviceinfo[powerdevice_ethernet].power_rate * 100) / 
                                       PM_GET_RATE(pm_info->deviceinfo[powerdevice_ethernet].norm_power_rate));      /*--- aktueller Ethernet-Verbrauch in Prozent ---*/
        activ_ethports              = PM_ETHERNET_GET_DEVICEMASK(pm_info->deviceinfo[powerdevice_ethernet].power_rate); /*--- Maske je Bit ein LAN-Port Bit0: Lan1 ... ---*/
        policy = temperature_policy(pm_info->stat.act_temperature);
        pm_info->stat.eth_status = 0;
        for(i = 0; i < AVMPOWER_MAX_ETHERNETPORTS; i++) {
            if(policy){
                pm_info->eth_status[i] = ETH_MASK_BITS(pm_info->eth_status[i], ETH_THROTTLE_DEMAND_POLICY | ETH_NORMAL_DEMAND_POLICY, policy);
            }
            limit_set_eth(pm_info, i, 0);
            pm_info->stat.eth_status |=  ((activ_ethports & (1 << i)) ? (3 << (i * 2)) : (pm_info->eth_status[i] & 0x3) << (i * 2));
        }
        pm_info->stat.isdn_status = 0;
        for(i = 1; i < 5; i++) {
            /*--- maximal 4 Controller ---*/
            pm_info->stat.isdn_status |= (PM_E1STATUS(i) & pm_info->deviceinfo[powerdevice_isdnnt].power_rate) ? (2 << ((i-1) * 4)) : 0; /*--- NT-E1 ---*/
            pm_info->stat.isdn_status |= (PM_E3STATUS(i) & pm_info->deviceinfo[powerdevice_isdnnt].power_rate) ? (4 << ((i-1) * 4)) : 0; /*--- NT E3 ---*/
            pm_info->stat.isdn_status |= (PM_E1STATUS(i) & pm_info->deviceinfo[powerdevice_isdnte].power_rate) ? (1 << ((i-1) * 4)) : 0; /*--- TE-Bit ---*/
        }
        pm_info->stat.rate_abact     = (PM_GET_RATE(pm_info->deviceinfo[powerdevice_analog].norm_power_rate) == 0) ? 0 : 
                                       (PM_GET_RATE(pm_info->deviceinfo[powerdevice_analog].power_rate) * 100) / 
                                       (PM_GET_RATE(pm_info->deviceinfo[powerdevice_analog].norm_power_rate)); /*--- aktueller Nebenstellen-Verbrauch in Prozent ---*/
        pm_info->stat.rate_dectact       = PM_GET_RATE(pm_info->deviceinfo[powerdevice_dect].power_rate);     /*--- aktueller Verbrauch DECT in Prozent ---*/
        pm_info->stat.dect_status        = PM_DECT_GET_ECO(pm_info->deviceinfo[powerdevice_dect].power_rate) ? 1 : 0;      /*--- DECT-ECO Status ---*/
        pm_info->stat.rate_battchargeact = (PM_GET_RATE(pm_info->deviceinfo[powerdevice_charge].norm_power_rate) == 0) ? 0 : 
                                           (PM_GET_RATE(pm_info->deviceinfo[powerdevice_charge].power_rate) * 100) / 
                                           (PM_GET_RATE(pm_info->deviceinfo[powerdevice_charge].norm_power_rate));               /*--- Batterieladeverbrauch (DECT) ---*/
        usbmwatt = PM_GET_MWATT(&pm_info->deviceinfo[powerdevice_usb_host]) +
                   PM_GET_MWATT(&pm_info->deviceinfo[powerdevice_usb_host2]) +
                   PM_GET_MWATT(&pm_info->deviceinfo[powerdevice_usb_host3]) +
                   0;
        normmwatt = PM_GET_NORM_MWATT(&pm_info->deviceinfo[powerdevice_usb_host]) +
                    PM_GET_NORM_MWATT(&pm_info->deviceinfo[powerdevice_usb_host2]) +
                    PM_GET_NORM_MWATT(&pm_info->deviceinfo[powerdevice_usb_host3]) +
                    0;
        if(normmwatt) {
            usbmwatt = (usbmwatt * 100) / normmwatt;
            if(usbmwatt > 100) {
                usbmwatt = 100;
            }
        }
        pm_info->stat.rate_usbhostact    = (unsigned char) usbmwatt;
        pm_info->stat.usb_status         = PM_GET_RATE(pm_info->deviceinfo[powerdevice_usb_client].power_rate) ? 1 : 0;  /*--- USB-Client connected ---*/
        pm_info->stat.rate_lteact        = PM_GET_RATE(pm_info->deviceinfo[powerdevice_lte].power_rate);                 /*--- aktueller Verbrauch LTE Prozent ---*/
        avm_power_speedstep_mode();
#if 0
        printk(KERN_DEBUG"SUM:%d(%d) SYST:%d(%d)-%x DSP:%d(%d) WLAN:%d(%d)-%d-%x ETH:%d(%d)-%x ISDN:%x AB:%d(%d) DECT:%d(%d) USB:%d(%d)-%x TEMP(%d, %d min %d max %d) want:%d\n",
                 pm_info->stat.rate_sumact,    pm_info->stat.rate_sumcum,
                 pm_info->stat.rate_systemact, pm_info->stat.rate_systemcum, pm_info->stat.system_status,
                 pm_info->stat.rate_dspact,    pm_info->stat.rate_dspcum,
                 pm_info->stat.rate_wlanact,   pm_info->stat.rate_wlancum, pm_info->stat.wlan_devices, pm_info->stat.wlan_status,
                 pm_info->stat.rate_ethact,    pm_info->stat.rate_ethcum, pm_info->stat.eth_status,
                 pm_info->stat.isdn_status,
                 pm_info->stat.rate_abact,     pm_info->stat.rate_abcum,
                 pm_info->stat.rate_dectact,   pm_info->stat.rate_dectcum,
                 pm_info->stat.rate_usbhostact, pm_info->stat.rate_usbhostcum, pm_info->stat.usb_status,
                 pm_info->stat.act_temperature, pm_info->stat.avg_temperature, pm_info->stat.min_temperature, pm_info->stat.max_temperature,
                 avm_event_source_check_id(pm_info->event_handle, avm_event_id_pm_ressourceinfo_status)
              );
#endif
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/
    }
    pm_info->thread_id = 0;
    DEB_ERR("[avm_power]pm_ressourceinfo_thread: exit\n");
    complete_and_exit(&pm_info->on_exit, 0 );
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void pm_ressourceinfo_init(void) {
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
    int i;
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/
    if(pm_ressourceinfo.deviceinfo[powerdevice_dspclock].power_rate == 0) {
        pm_ressourceinfo.deviceinfo[powerdevice_dspclock].power_rate = FREQUENZ_TO_PERCENT(STD_DSPCLK, MAX_DSPCLK);
    }
    if(pm_ressourceinfo.deviceinfo[powerdevice_cpuclock].power_rate == 0) {
        pm_ressourceinfo.deviceinfo[powerdevice_cpuclock].power_rate = FREQUENZ_TO_PERCENT(STD_MIPSCLK, MAX_MIPSCLK);
    }
    if(pm_ressourceinfo.deviceinfo[powerdevice_systemclock].power_rate == 0) {
        pm_ressourceinfo.deviceinfo[powerdevice_systemclock].power_rate = FREQUENZ_TO_PERCENT(STD_SYSTEMCLK, MAX_SYSTEMCLK);
    }
    init_waitqueue_head(&pm_ressourceinfo.wait_queue);
    init_completion(&pm_ressourceinfo.on_exit );
	pm_ressourceinfo.thread_id = kernel_thread(pm_ressourceinfo_thread, (void *) &pm_ressourceinfo, CLONE_SIGHAND);

#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
    for(i = 0; i < AVMPOWER_MAX_ETHERNETPORTS; i++) {
        pm_ressourceinfo.eth_status[i] = 2; /*--- auf normal vorinitialisieren ---*/
    }
    pm_ressourceinfo.event_handle = avm_event_source_register( "pm_info_stat", (
                                              (((unsigned long long) 1) << avm_event_id_pm_ressourceinfo_status)),
                                               pm_ressourceinfo_notify,
											  &pm_ressourceinfo
											  );
    if(pm_ressourceinfo.event_handle == NULL) {
        printk("[avm_power] avm event register failed !\n");
	}
    pm_ressourceinfo.temperature_eventhandle =  avm_event_source_register( "temperature", (
                                                   (((unsigned long long) 1) << avm_event_id_temperature)),
                                                   avmevent_temperature_notify,
                                                   &pm_ressourceinfo
                                                   );
    if(pm_ressourceinfo.temperature_eventhandle == NULL) {
        printk("[avm_power] avm event register failed !\n");
	}
    pm_ressourceinfo.cpu_idle_eventhandle =  avm_event_source_register( "cpu_idle", (
                                                   (((unsigned long long) 1) << avm_event_id_cpu_idle)),
                                                   avmevent_cpu_idle_notify,
                                                   &pm_ressourceinfo
                                                   );
    if(pm_ressourceinfo.cpu_idle_eventhandle == NULL) {
        printk("[avm_power] avm event register failed !\n");
	}
#endif
}
#ifdef CONFIG_AVM_POWER_MODULE
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void pm_ressourceinfo_exit(void) {
    if(pm_ressourceinfo.thread_id) {
        kill_proc(pm_ressourceinfo.thread_id, SIGTERM, 1 );
        wait_for_completion( &pm_ressourceinfo.on_exit );
    }
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
    if(pm_ressourceinfo.event_handle) { 
      avm_event_source_release(pm_ressourceinfo.event_handle);
      pm_ressourceinfo.event_handle = NULL;
    }
    if(pm_ressourceinfo.temperature_eventhandle) {
      avm_event_source_release(pm_ressourceinfo.temperature_eventhandle);
      pm_ressourceinfo.temperature_eventhandle = NULL;
    }
    if(pm_ressourceinfo.cpu_idle_eventhandle) {
      avm_event_source_release(pm_ressourceinfo.cpu_idle_eventhandle);
      pm_ressourceinfo.cpu_idle_eventhandle = NULL;
    }
#endif
}
#endif/*--- #ifdef CONFIG_AVM_POWER_MODULE ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static const char *pm_name_device(enum  _powermanagment_device device) {
    switch(device) {
        case powerdevice_none:          return "powerdevice_none";
        case powerdevice_cpuclock:      return "powerdevice_cpuclock";
        case powerdevice_dspclock:      return "powerdevice_dspclock";
        case powerdevice_systemclock:   return "powerdevice_systemclock";
        case powerdevice_wlan:          return "powerdevice_wlan";
        case powerdevice_isdnnt:        return "powerdevice_isdnnt";
        case powerdevice_isdnte:        return "powerdevice_isdnte";
        case powerdevice_analog:        return "powerdevice_analog";
        case powerdevice_dect:          return "powerdevice_dect";
        case powerdevice_ethernet:      return "powerdevice_ethernet";
        case powerdevice_dsl:           return "powerdevice_dsl";
        case powerdevice_usb_host:      return "powerdevice_usb_host";
        case powerdevice_usb_client:    return "powerdevice_usb_client";
        case powerdevice_charge:        return "powerdevice_charge";
        case powerdevice_loadrate:      return "powerdevice_loadrate";
        case powerdevice_temperature:   return "powerdevice_temperature";
        case powerdevice_dectsync:      return "powerdevice_dectsync";
        case powerdevice_usb_host2:     return "powerdevice_usb_host2";
        case powerdevice_usb_host3:     return "powerdevice_usb_host3";
        case powerdevice_dsp_loadrate:  return "powerdevice_dsp_loadrate";
        case powerdevice_vdsp_loadrate: return "powerdevice_vdsp_loadrate";
        case powerdevice_lte:           return "powerdevice_lte";
        case powerdevice_maxdevices:    return "powerdevice_maxdevices";
    }
    return "powerdevice_unknown";
}

/*--------------------------------------------------------------------------------*\
 * Zeilenaufbau: alle Werte dezimal (line steht hinter '=')
 * PMINFO= device, act_power_rate
\*--------------------------------------------------------------------------------*/
static void pm_ressourceinfo_parse(char *line) {
    char *p = line;
    int device, power_rate;
    SKIP_SPACES(p);
    sscanf(p, "%d", &device);
    if(device <= powerdevice_none || device >= powerdevice_maxdevices) {
        DEB_ERR("[avm_power] p%s: unknown_device %d: '%s'\n", __func__, device, line);
        return;
    }
    if(avm_power_write_find_special_char(&p, ',')){
        DEB_ERR("[avm_power] %s: invalid format '%s'\n", __func__, p);
        return;
    }
    SKIP_SPACES(p);
    sscanf(p, "%d", &power_rate);
    PowerManagmentRessourceInfo(device, power_rate);
}
/*--------------------------------------------------------------------------------*\
 * Zeilenaufbau: alle Werte dezimal (line steht hinter '=')
 * PMINFO_MODE = device, norm_power_rate, multiplier, divider, offset
\*--------------------------------------------------------------------------------*/
static void pm_ressourceinfo_scriptparse(char *line) {
    struct _power_managment_device_info *df;
    char *p = line;
    int mWatt, device, i;
    SKIP_SPACES(p);
    sscanf(p, "%d", &device);
    if(device <= powerdevice_none || device >= powerdevice_maxdevices) {
        DEB_ERR("[avm_power] pm_ressourceinfo_scriptparse: unknown_device %d: '%s'\n", device, line);
        return;
    }
    df =  &pm_ressourceinfo.deviceinfo[device];
    if(avm_power_write_find_special_char(&p, ',')){
        DEB_ERR("[avm_power] pm_ressourceinfo_scriptparse: invalid format '%s'\n", p);
        return;
    }
    SKIP_SPACES(p);
    sscanf(p, "%d", &df->norm_power_rate);
    if(avm_power_write_find_special_char(&p, ',')){
        DEB_ERR("[avm_power] pm_ressourceinfo_scriptparse: invalid format '%s'\n", p);
        return;
    }
    SKIP_SPACES(p);
    sscanf(p, "%d", &df->multiplier);
    if(avm_power_write_find_special_char(&p, ',')){
        DEB_ERR("[avm_power] pm_ressourceinfo_scriptparse: invalid format '%s'\n", p);
        return;
    }
    SKIP_SPACES(p);
    sscanf(p, "%d", &df->divider);
    if(avm_power_write_find_special_char(&p, ',')){
        DEB_ERR("[avm_power] pm_ressourceinfo_scriptparse: invalid format '%s'\n", p);
        return;
    }
    SKIP_SPACES(p);
    sscanf(p, "%d", &df->offset);

    if(df->divider != 0) {
        int SumNorm_mWatt = 0;
        for(i = 0; i < powerdevice_maxdevices; i++) {
            struct _power_managment_device_info *df1 =  &pm_ressourceinfo.deviceinfo[i];
                SumNorm_mWatt += PM_GET_NORM_MWATT(df1);
        }
        mWatt = PM_GET_NORM_MWATT(df);
        printk(KERN_DEBUG"[avm_power] pm_ressourceinfo_scriptparse: %s: norm_power_rate=%d act_rate=%d mul=%d div=%d offset=%d NormP=%d mW -> SumNormP=%d mW\n",
                pm_name_device(device), df->norm_power_rate, df->power_rate, df->multiplier, df->divider, df->offset, mWatt, SumNorm_mWatt);
        pm_ressourceinfo.NormP = SumNorm_mWatt;
    } else {
        DEB_ERR("[avm_power] pm_ressourceinfo_scriptparse: warning divider is zero '%s'\n", line);
    }
    pm_ressourceinfo.Changes++;
    wake_up_interruptible(&pm_ressourceinfo.wait_queue);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline unsigned int cpusloadrate_to_powerrate(unsigned int cpuspowerrate) {
    unsigned int i;
    unsigned int run = 0;
    for(i = 0; i < NR_CPUS; i++) {
        run += cpuspowerrate & 0xFF;
        cpuspowerrate >>= 8;
    }
    return run / NR_CPUS;
}
/*--------------------------------------------------------------------------------*\
 * Funktion wird von Treibern aufgerufen um Infos ueber den aktuellen Power-Status zu liefern
\*--------------------------------------------------------------------------------*/
int PowerManagmentRessourceInfo(enum _powermanagment_device device, int power_rate){
    int changes = 0, ready;
    unsigned int cpus_loadrate;

    if((unsigned)device >= powerdevice_maxdevices) {
        DEB_ERR("[avm_power]PowerManagmentRessourceInfo: unknown device: %d\n", device);
        return 0;
    }
    ready =  pm_ressourceinfo.thread_id;
#if defined(DECTSYNC_PATCH)
    if(device == powerdevice_dectsync) {
        if(ready == 0) {
            return 0;
        }
        if(power_rate < 0) {
            if(power_rate == -1) {
                /*--- Piglet sagt wir haben DECT ---*/
                telefonevent.dectsyncmode = 0;    /*--- im Fall der Telefonie: nicht auf Run anderer achten! ---*/
                start_dectsync(1, 0);
            } else if(power_rate == -2) {
                /*--- stop dectsync ---*/
                idle_dectsynchandler(1);
            } else if(power_rate == -3) {
                telefonevent.dectsyncmode = 1;  /*--- im Fall der Telefonie: auf Run anderer achten! ---*/
                start_dectsync(1, 0);
            } else {
                display_dectsync(pm_ressourceinfo.loadcntrl);
            }
            power_rate = 0;
        }
        if(power_rate == 0) {
            /*--- Indikator das jetzt im Idle-Kontext ---*/
            return idle_dectsynchandler(0);
        }
        return pm_ressourceinfo.deviceinfo[powerdevice_loadrate].power_rate;
    }
#endif/*--- #if defined(DECTSYNC_PATCH) ---*/
    if(device == powerdevice_loadrate) {
        cpus_loadrate = power_rate;
        power_rate = cpusloadrate_to_powerrate(cpus_loadrate);
        pm_ressourceinfo.loadcntrl = avm_powermanager_load_control_handler(power_rate);
#if defined(DECTSYNC_PATCH)
        {
            int wastepercent = updaterun_dectsync(power_rate);
            if(wastepercent < power_rate) {
                /*--- Korrektur der power-rate ---*/
                power_rate -=  wastepercent;
            }
        }
#endif/*--- #if defined(DECTSYNC_PATCH) ---*/
    }
    if(pm_ressourceinfo.deviceinfo[device].power_rate != power_rate) {
        if((device == powerdevice_isdnte) || (device == powerdevice_isdnnt)) {
            convert_isdnpowerstate(&pm_ressourceinfo.deviceinfo[device].power_rate, power_rate);
        } else {
            pm_ressourceinfo.deviceinfo[device].power_rate = power_rate;
        }
        if((avm_power_disp_loadrate & 1) && (device == powerdevice_loadrate)) {
#if defined(DECTSYNC_PATCH)
            if(display_dectsync(pm_ressourceinfo.loadcntrl)) {
#endif/*--- #else ---*//*--- #if defined(DECTSYNC_PATCH) ---*/
#if NR_CPUS == 2
                printk(KERN_ERR"idle: %d/%d %%(%d/%d %%) loadcntrl %d\n", 
                                       100 - ((cpus_loadrate >> 0) & 0xFF), 100 - ((cpus_loadrate >> 8) & 0xFF), 
                                       (cpus_loadrate >> 0) & 0xFF, (cpus_loadrate >> 8) & 0xFF, 
                                       pm_ressourceinfo.loadcntrl
                                       );
#else
                printk(KERN_ERR"[%lu]idle: %d %%(%d %%) loadcntrl %d\n", jiffies, 100 - power_rate, power_rate, pm_ressourceinfo.loadcntrl);
#endif

#if defined(DECTSYNC_PATCH)
            }
#endif/*--- #if defined(DECTSYNC_PATCH) ---*/
        }
        changes = (ready && pm_ressourceinfo.NormP) ? 1 : 0;   /*--- nur wenn Thread existiert und script geladen ----*/
        pm_ressourceinfo.Changes += changes;
        if((avm_power_disp_loadrate & 2) && (device != powerdevice_loadrate)) printk(KERN_ERR"[avm_power]PowerManagmentRessourceInfo: device: %s value=(0x%x)%d changes=%d\n", pm_name_device(device), power_rate, power_rate, pm_ressourceinfo.Changes);
    }
    if(changes) {
        wake_up_interruptible(&pm_ressourceinfo.wait_queue);
    }
    return 0;
}
EXPORT_SYMBOL(PowerManagmentRessourceInfo);

#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void display_pminfostat(char *prefix) {
    struct _avm_event_pm_info_stat *pstat = &pm_ressourceinfo.stat;
    unsigned int i;
    char tmp[128], *p;
    tmp[0] = 0;
    p = tmp;
    /*--- printk("eth_status=0x%x %x", pstat->eth_status, pm_ressourceinfo.deviceinfo[powerdevice_ethernet].power_rate); ---*/
    for(i = 0; i < AVMPOWER_MAX_ETHERNETPORTS; i++) {
        if((((pstat->eth_status >> (i * 2)) & 0x3) == 0x3)) {
            sprintf(p, "LAN%x ", i+1);
            p += strlen(p);
        }
    }
    printk(KERN_ERR"%sCPUs-Activity=%d %% DSL-Acitvity=%d %% WLAN-Activity=%d %% WLAN-Devices=%d%s USB=%d %% %s\n",
            prefix ? prefix : "",
            pm_ressourceinfo.deviceinfo[powerdevice_loadrate].power_rate,
            pstat->rate_dspact,
            pstat->rate_wlanact,
            pstat->wlan_devices,
            pstat->wlan_status ? "ECO" : "",
            pstat->rate_usbhostact, tmp);
}       
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/
