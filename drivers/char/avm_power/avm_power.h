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

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifndef _AVM_POWER_H_
#define _AVM_POWER_H_

#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
#include <linux/avm_event.h>
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/

/*--- #define AVM_POWER_DEBUG ---*/

#if defined(AVM_POWER_DEBUG)
#define DEB_ERR(args...)     printk(KERN_ERR args)
#define DEB_WARN(args...)    printk(KERN_WARNING args)
#define DEB_NOTE(args...)    printk(KERN_NOTICE args)
#define DEB_INFO(args...)    printk(KERN_INFO args)
#define DEB_TRACE(args...)   printk(KERN_INFO args)
#else /*--- #if defined(AVM_POWER_DEBUG) ---*/
#define DEB_ERR(args...)     printk(KERN_ERR args)
#define DEB_WARN(args...)    printk(KERN_WARNING args)   
#define DEB_NOTE(args...)
#define DEB_INFO(args...)
#define DEB_TRACE(args...)
#endif /*--- #else ---*/ /*--- #if defined(AVM_POWER_DEBUG) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <asm/atomic.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_power {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 19)
    struct class *osclass;
#endif/*--- #if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 19) ---*/
    dev_t         device;
    struct cdev  *cdev;
    void         *event_handle;
};
extern struct _avm_power avm_power;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_power_open_data {
    unsigned int reserved;
};

/*-------------------------------------------------------------------------------------*\
 * Eintrag in _power_managment
\*-------------------------------------------------------------------------------------*/
struct _power_managment_dest_entry {
    char *client_name;  /*--- (registrierter) client-name ---*/
#define AVM_PM_CB_IGNORE                       0
    /*--- 1: falls Aktion scheitert(return-Wert von CallBackPowerManagmentControl): Tabelle nicht weiter abarbeiten ---*/
#define AVM_PM_CB_FAILED                       1
    /*--- 2: falls Aktion scheitert UND Funktion nicht registriert ---*/
#define AVM_PM_CB_UNINSTALLED_OR_FAILED        2
    /*--- 4: locke Operation ab hier ---*/
#define AVM_PM_LOCK                            4
    int  mandatory;     
    int  state;         /*--- Übergabeparameter der fuer CallBackPowerManagmentControl ---*/
};

/*------------------------------------------------------------------------------------------*\
 * powermode führt folgende Tabelle (d.h. CallBackPowerManagmentControl() aus)
\*------------------------------------------------------------------------------------------*/
struct _power_managment {
    char *powermode;
    struct _power_managment_dest_entry *dests;
#define PM_ACESS_DRIVER    0x1
#define PM_ACESS_APPL      0x2
#define PM_ACESS_ALL       (PM_ACESS_DRIVER | PM_ACESS_APPL)
    unsigned int Access;   /*--- veroderbar ---*/
};

/*-------------------------------------------------------------------------------------*\
 * sammelt PowerManagmentRegister
\*-------------------------------------------------------------------------------------*/
struct _power_managment_clients {
    struct _power_managment_clients *next;
    char *client_name;
    int (*CallBackPowerManagmentControl)(int state);
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
enum _load_control_set {
    load_control_auto = 0x0,
    load_control_off  = 0x1,
    load_control_manu = 0x2 /*--- (+ Offset fuer welcher Mode) ---*/
};
void avm_powermanager_load_control_set(enum _load_control_set mode, int scale);
int avm_powermanager_load_control_handler(int run);

#ifdef CONFIG_AVM_POWERMETER
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _power_managment_device_info {
    int power_rate;  /*--- durch PowerManagmentRessourceInfo gesetzt ---*/
    /*--- Umrechnungsfaktoren lineare Fkt. y = mx + n: (multiplier * power_rate) / divider + offset -> mWatt ---*/ 
    int multiplier;           
    int divider;
    int offset;
    int norm_power_rate; /*--- Zur Ermittlung der Norm-Watt-Wertes ---*/
};

#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
#define PM_TIME_INTERVALL          (24 * 3600) /*--- 24 h Intervall ---*/
#define PM_TIME_ENTRIES            (24 * 6)    /*--- alle 10 Minuten ein Teilzeitfenster ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _power_managment_cum_info {
	unsigned long rate_sum;
    unsigned long rate_system;
    unsigned long rate_dsp;
    unsigned long rate_wlan;
    unsigned long rate_eth;
    unsigned long rate_ab;
    unsigned long rate_dect;
    unsigned long rate_battcharge;
    unsigned long rate_usbhost;
    unsigned long rate_lte;
    long messure_count; /*--- Anzahl der Messungen ---*/
    signed long rate_temp;
    signed char min_temp;
    signed char max_temp;
};
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _power_managment_ressource_info {
    struct _power_managment_device_info deviceinfo[powerdevice_maxdevices];
    unsigned int NormP;             /*--- Norm-Leistungsverbrauch (entspricht 100 %) ---*/
    int thread_id;
    struct completion on_exit;
    wait_queue_head_t wait_queue;
    void *event_handle;
    void  *temperature_eventhandle;
    void  *cpu_idle_eventhandle;
    unsigned int Changes;
#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
    struct _avm_event_pm_info_stat stat;
    unsigned long LastMessureTimeStamp;     /*--- letzte Messzeit in Sekunden ---*/
    unsigned long ActTimeEntry;
    struct _power_managment_cum_info cum[PM_TIME_ENTRIES];
#define AVMPOWER_MAX_ETHERNETPORTS   8 
    unsigned char eth_status[AVMPOWER_MAX_ETHERNETPORTS];
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/
    int speedstep_sema;      
    volatile unsigned int speedstep_state;      
    volatile unsigned int act_gov_cpufreqstate; /*--- Kontext des Threads ---*/
    volatile unsigned int loadcntrl; /*--- Kontext des Threads ---*/
};

#if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE) 
void display_pminfostat(char *prefix);
#endif/*--- #if defined(CONFIG_AVM_EVENT) || defined(CONFIG_AVM_EVENT_MODULE)  ---*/
#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/

/*--- ueber die Console: ---*/
#define ETH_THROTTLE_DEMAND_POLICY  (0x1 << 2)
#define ETH_NORMAL_DEMAND_POLICY    (0x1 << 3)
#define ETH_THROTTLE_ACTIVE_POLICY  (0x1 << 4)
/*--- per Policy: ---*/
#define ETH_THROTTLE_DEMAND_CON     (0x1 << 5)
#define ETH_NORMAL_DEMAND_CON       (0x1 << 6)
#define ETH_THROTTLE_ACTIVE_CON     (0x1 << 7)

#define ETH_THROTTLE_DEMAND(a) ((a) & (ETH_THROTTLE_DEMAND_POLICY | ETH_THROTTLE_DEMAND_CON)) ? 1 : 0
#define ETH_NORMAL_DEMAND(a)   ((a) & (ETH_NORMAL_DEMAND_POLICY | ETH_NORMAL_DEMAND_CON)) ? 1 : 0

#define ETH_THROTTLE_ACTIVE(a) ((a) & (ETH_THROTTLE_ACTIVE_POLICY | ETH_THROTTLE_ACTIVE_CON)) ? 1 : 0

#define ETH_MASK_BITS(a, clearmask, setmask) (((a) & ~(clearmask)) | (setmask))

#if defined(CONFIG_AR10)
unsigned int temperature_policy(signed int temperature);
#else/*--- #if defined(CONFIG_AR10) ---*/
#define temperature_policy(a) 0
#endif/*--- #else ---*//*--- #if defined(CONFIG_AR10) ---*/
#endif /*--- #ifndef _AVM_POWER_H_ ---*/
