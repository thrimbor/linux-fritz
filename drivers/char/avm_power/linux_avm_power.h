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
#ifndef _AR7_AVMPOWER_H_
#define _AR7_AVMPOWER_H_

/*-------------------------------------------------------------------------------------*\
 * Powermanagment des Treibers anmelden
 * adsl: state
 *  0	Power Off
 *  1	Low Power (kein DSL)
 *  20	nur Request (kein State aendern!): IsATA  Returnwert 0: ja sonst anderer Mode
 *  10	Full Speed (jetziger Stand)
 *
 *  piglet: state 
 *  0 normal clocks
 *  1 high clocks 
 *  2 low  clocks 
 *  0x10 Abfrage ob Piglet unabhängig von Systemfrequenz liefere 1 sonst 0
 *  verodert mit 0x80: Moeglichkeit testen, wenn möglich, so werden die Codecs allerdings auch gleich gestoppt!
 *  UR8:
 *  0x100: 40V disable
 *  0x101: 40V enable
 *
 * 0x200: Abfrage "2 Bitfile-Mode"
 * 0x201: Abfrage ob TE aktiv (0x81 : verodert mit 0x20, falls tefile geladen, 0x40 falls Auto-Mode aus) 
 * 0x202: Abfrage ob POTS-File (0 - ja)
 * 0x203: Abfrage ob TE-File (0 - ja)
 * 0x204: Auto-Mode an
 * 0x205: Auto-Mode aus
 * 0x210: POTS-File laden
 * 0x211: TE-File laden
 * 0x1000: Piglet entladen
 *  
 *  speedup: 
 * state:
 *  0 normal clocks
 *  1 high clocks 
 *  2 low  clocks 
 *  0x8x setze clock (0,1,2) - ab jetzt kein idleabhängiges Speedup-Control
 *  0x10x Setzen der unterstuetzten Modi: (1 Fast, 2 Slow (verodern))
 *  0x18x Setzen der unterstuezten Modi - allerdings nur manuelle Switching (idleunabhaengig)
 *  0x201 kein speed-down mehr - nur noch speedup (Telefonapplikation)
 *  0x200 speed-down entlocken
 *  0x40x Abfrage ob diese entsprechende Speedup erlaubt     
 *  0x801 kein speedup/down       (USB-Treiber)
 *  0x800 speed-down entlocken
 *  0x1001 kein speedup/down       (ATM-Treiber)
 *  0x1000 speed-down entlocken
 * Initial sind Änderungen an der Clock nicht freigeschaltet
 *  ethernet: 
 *  siehe union  _powermanagment_ethernet_state
 *
 *  isdn:
 *  state:  
 *  0x10        SLIC 1 aus
 *  0x11        SLIC 1 an
 *  0x20        SLIC 2 aus
 *  0x21        SLIC 2 an
 *  0x100       entladen
 *  pcmlink
 *  0x0:  PCM-Bus deaktivieren (nur Ur8 - fuer Bitfilechange)
 *  0x1:  PCM-Bus  aktivieren   (nur Ur8 - fuer Bitfilechange)    
 *  0x100 pcmlink PCM-Bus aus, evtl. DSP's reseten
\*-------------------------------------------------------------------------------------*/
void *PowerManagmentRegister(char *client_name, int (*CallBackPowerManagmentControl)(int state));

/*-------------------------------------------------------------------------------------*\
 * Treiber abmelden
\*-------------------------------------------------------------------------------------*/
void PowerManagmentRelease(void *Handle);

/*--------------------------------------------------------------------------------*\
 * Load-Control-Callback-Schnittstelle
 * load_reduce: 0 - 10 (0 keine Lastreduzierung, 10 max. Lastreduzierung) 
 * context:     Pointer der in avm_powermanager_load_control_register angegeben wurde
\*--------------------------------------------------------------------------------*/
typedef void (*load_control_callback_t)(int load_reduce, void *context);
/*--------------------------------------------------------------------------------*\
 * Callback registrieren
 * name: Name des Treibers
 * load_control_callback_t: Callback (s.o.)
 * context: Parameter fuer Callback
\*--------------------------------------------------------------------------------*/
void *avm_powermanager_load_control_register(char *name, load_control_callback_t, void *context) __attribute__ ((weak));
/*--------------------------------------------------------------------------------*\
 * Load-Control-Callback abmelden
\*--------------------------------------------------------------------------------*/
void avm_powermanager_load_control_release(void *handle) __attribute__ ((weak));


#define POWERMANAGEMENT_THROTTLE_ETH

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
union  _powermanagment_ethernet_state {
    unsigned int Register;
    struct {
        unsigned int port:8;        /*--- auszuwaehlender port ---*/ 
        unsigned int status:2;      /*--- 0: aus, 1: power_save (+throttle), 2: normal 3: power_save (aber kein throttle) ---*/
#if defined(POWERMANAGEMENT_THROTTLE_ETH)
        unsigned int throttle_eth:1; /*--- reduziere Speed ---*/
        unsigned int reserved:21; 
#else/*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/
        unsigned int reserved:22; 
#endif/*--- #else ---*//*--- #if defined(POWERMANAGEMENT_THROTTLE_ETH) ---*/
    } Bits;
};
/*-------------------------------------------------------------------------------------*\
 * vom Kernel den Powermode ändern
 * Returnwert: 0 ok sonst Abbruch mit Fehler
\*-------------------------------------------------------------------------------------*/
int PowerManagmentActivatePowerMode(char *powermodename);

#ifdef CONFIG_AVM_POWERMETER
#if defined(CONFIG_MIPS_UR8)
#if defined(CONFIG_AVM_DECT_SYNC)
#if CONFIG_AVM_DECT_SYNC != -1
#define DECTSYNC_PATCH 
#endif/*--- #if CONFIG_AVM_DECT_SYNC != -1 ---*/
#endif/*--- #if defined(CONFIG_AVM_DECT_SYNC) ---*/
#endif/*--- #if defined(CONFIG_MIPS_UR8) ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
enum _powermanagment_device {
    powerdevice_none          = 0,
    powerdevice_cpuclock      = 1,        /*--- power_rate in % Bezug: NormFrequenz 212 MHz ---*/
    powerdevice_dspclock      = 2,        /*--- power_rate in % Bezug: NormFrequenz 250 MHz ---*/
    powerdevice_systemclock   = 3,        /*--- power_rate in % Bezug: NormFrequenz 150 MHz ---*/
    powerdevice_wlan          = 4,        /*--- power_rate in % Maximal-Last ---*/
    powerdevice_isdnnt        = 5,        /*--- power_rate 0 oder 100 % (Ebene 1 aktiv)  ---*/
    powerdevice_isdnte        = 6,        /*--- power_rate 0 oder 100 % (Ebene 1 aktiv)  ---*/
    powerdevice_analog        = 7,        /*--- power_rate 100 % pro abgehobenen Telefon ---*/    
    powerdevice_dect          = 8,        /*--- power_rate in % Maximal-Last ---*/
    powerdevice_ethernet      = 9,        /*--- power_rate 100 % pro aktiven Port ---*/
    powerdevice_dsl           = 10,       /*--- power_rate in % Maximal-Last (????) ---*/
    powerdevice_usb_host      = 11,       /*--- power_rate in Milli-Ampere ---*/ 
    powerdevice_usb_client    = 12,       /*--- power_rate 100 % der Maximal-Last ---*/   
    powerdevice_charge        = 13,       /*--- power_rate in Milli-Watt ---*/
    powerdevice_loadrate      = 14,       /*--- power_rate in % (100 - % Idle-Wert) falls SMP: je 8 Bit eine CPU ---*/    
    powerdevice_temperature   = 15,       /*--- power_rate in Grad Celcius ---*/    
    powerdevice_dectsync      = 16,       /*--- power_rate clks_per_jiffies---*/    
    powerdevice_usb_host2     = 17,       /*--- power_rate in Milli-Ampere ---*/ 
    powerdevice_usb_host3     = 18,       /*--- power_rate in Milli-Ampere ---*/ 
    powerdevice_dsp_loadrate  = 19,       /*--- (ADSL/VDSL-)DSP power_rate in % (100 - % Idle-Wert) ---*/    
    powerdevice_vdsp_loadrate = 20,       /*--- Voice-DSP power_rate in % (100 - % Idle-Wert) ---*/    
    powerdevice_lte           = 21,       /*--- power_rate in Milli-Ampere ---*/
    powerdevice_maxdevices    = 22
};

/*--------------------------------------------------------------------------------*\
 * Funktion wird von Treibern aufgerufen um Infos ueber den aktuellen Power-Status zu liefern
 *  powerdevice_wlan: 
\*--------------------------------------------------------------------------------*/
#define FREQUENZ_TO_PERCENT(freq, ref_freq)     (freq) / ((ref_freq) / 100)
#define PM_RATE_MASK                      0xFFFF  
#define PM_GET_RATE(param)                (((param)) & PM_RATE_MASK)
#define PM_WLAN_PARAM(eco, devices, rate) (((eco) ? (1 << 31) : 0) | ((devices & 0x7F) << 24) | ((rate) & PM_RATE_MASK))
#define PM_WLAN_GET_ECO(param)            ((param) & (1 << 31) ? 1 : 0)
#define PM_WLAN_GET_DEVICES(param)        (((param) >> 24) & 0x7F)  
#define PM_WLAN_TRANSMITPOWER_TO_RATE(rate) (rate) == 0 ? 0 : (100 >> ((rate) - 1))

#define PM_USBHOST_DEVICE_1                   (0x0 << 30)  
#define PM_USBHOST_DEVICE_2                   (0x1 << 30)  
#define PM_USBHOST_DEVICE_3                   (0x2 << 30)  
#define PM_USBHOST_DEVICE_4                   (0x3 << 30)  
#define PM_USBHOST_DEVICE_MASK                ((0x3 << 30))
#define PM_USBHOST_GET_DEVICES(a)             (param >> 30)

#define PM_DECT_STATUS(eco, rate)             (((eco) ? (1 << 31) : 0) | ((rate) & PM_RATE_MASK))
#define PM_DECT_GET_ECO(param)                ((param) & (1 << 31) ? 1 : 0)
/*--- Controller zaehlt ab 1 !! ---*/
#define PM_E1STATUS(Controller)               (1 << (16 + ((Controller) - 1)))
#define PM_ISDN_SET_E1STATUS(Controller)      ((1 << 31) | PM_E1STATUS(Controller))
#define PM_ISDN_RESET_E1STATUS(Controller)    ((0 << 31) | PM_E1STATUS(Controller))

#define PM_E3STATUS(Controller)               (1 << (24 + ((Controller) - 1)))
#define PM_ISDN_SET_E3STATUS(Controller)      ((1 << 31) | PM_E3STATUS(Controller))
#define PM_ISDN_RESET_E3STATUS(Controller)    ((0 << 31) | PM_E3STATUS(Controller))

#define PM_ETHERNET_PARAM(devicemask, rate) ((((devicemask) & 0xFF) << 24) | (rate))
#define PM_ETHERNET_GET_DEVICEMASK(param)   (((param) >> 24) & 0xFF) 
int PowerManagmentRessourceInfo(enum _powermanagment_device device, int power_rate);

#endif/*--- #ifdef CONFIG_AVM_POWERMETER ---*/

#endif/*--- #ifndef _AR7_AVMPOWER_H_ ---*/
