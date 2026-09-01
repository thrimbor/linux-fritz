/*** LED-Events Version 2.10 ***/

#ifndef _avm_led_event_h_
#define _avm_led_event_h_

#ifndef LED_EVENT_VERSION
#define LED_EVENT_VERSION 2
#define LED_EVENT_SUBVERSION 10
#endif

#ifndef AVM_LED_INTERNAL

enum _led_event {
	event_hardware_error = 2,
	event_update_no_action = 5,
	event_update_fw_available = 6,
	event_update_running = 7,
	event_update_error = 8,
	event_ata_disable = 11,
	event_ata_enable = 12,
	event_tr69_no_message = 13,
	event_tr69_connecting = 14,
	event_tr69_autoconf_runnning = 15,
	event_tr69_acs_not_avail = 16,
	event_tr69__internal__ = 17,
	event_pin_wait_for_pin = 19,
	event_pin_wait_for_authentication = 20,
	event_pin_pppoe_auth_ongoing = 21,
	event_pin_ok = 22,
	event_pin_done = 23,
	event_vpn_disconnected = 24,
	event_vpn_connected = 25,
	event_pppoe_off = 27,
	event_pppoe_on = 28,
	event_pppoe_auth_failed = 29,
	event_pppoe_isp_error = 30,
	event_pppoe_mac_address_error = 31,
	event_pppoe_auth_start = 32,
	event_budget_notreached = 34,
	event_budget_reached = 35,
	event_voip_con_register_start = 37,
	event_voip_con_not_registered = 38,
	event_voip_con_call_in_progress = 39,
	event_voip_con_call_finished = 40,
	event_voip_con_register_fail = 41,
	event_voip_con_registered = 42,
	event_voip_srtp_active = 43,
	event_voip_srtp_inactive = 44,
	event_voip_mwi_no_message = 45,
	event_voip_mwi_speech_msg = 46,
	event_voip_mwi_mail = 47,
	event_voip_mwi_mail_and_speech_msg = 48,
	event_voip_info_freecall_start = 51,
	event_voip_info_freecall_end = 52,
	event_fritz_media_no_scan = 60,
	event_fritz_media_scan_in_progress = 61,
	event_gsm_off = 65,
	event_gsm_on = 66,
	event_gsm_auth_failed = 67,
	event_gsm_isp_error = 68,
	event_gsm_syncing = 69,
	event_gsm_sync_failed = 70,
	event_internet_nicht_verfuegbar = 90,
	event_internet_verfuegbar = 91,
	event_internet_abgebaut = 92,
	event_internet_aufgebaut = 93,
	event_internet_fehler = 94,
	event_festnetz_nicht_verfuegbar = 97,
	event_festnetz_verfuegbar = 98,
	event_festnetz_abgebaut = 99,
	event_festnetz_aufgebaut = 100,
	event_festnetz_fehler = 101,
	event_festnetz_mwi_set = 104,
	event_festnetz_mwi_reset = 105,
	event_fon_info_tam_set_mwi = 108,
	event_fon_info_tam_reset_mwi = 109,
	event_fon_info_fax_set_mwi = 110,
	event_fon_info_fax_reset_mwi = 111,
	event_fon_info_klingelsperre_aktiv = 112,
	event_fon_info_klingelsperre_inaktiv = 113,
	event_fon_info_missed_call_set = 114,
	event_fon_info_missed_call_reset = 115,
	event_dect_abgeschaltet = 118,
	event_dect_aktiv = 119,
	event_dect_fehler = 120,
	event_dect_stick_and_surf_start = 123,
	event_dect_stick_and_surf_error = 124,
	event_dect_stick_and_surf_success = 125,
	event_dect_stick_and_surf_done = 126,
	event_ab_linedown = 121,
	event_ab_lineup = 122,
	event_ab_fehler = 127,
	event_ab_onhook = 128,
	event_ab_offhook = 129,
	event_ab_active = 130,
	event_isdn_fehler = 131,
	event_isdn_d1_down = 132,
	event_isdn_d1_up = 133,
	event_wlan_sta_iptv_good = 135,
	event_wlan_sta_iptv_medium = 136,
	event_wlan_sta_iptv_bad = 137,
	event_wlan_sta_no_iptv = 138,
	event_wlan_sta_starting = 139,
	event_wlan_sta_register = 140,
	event_wlan_sta_stopping = 141,
	event_wlan_sta_unregister = 142,
	event_wlan_sta_wps_start = 143,
	event_wlan_sta_wps_error = 144,
	event_wlan_sta_wps_timeout = 145,
	event_wlan_sta_wps_success = 146,
	event_wlan_sta_wps_done = 147,
	event_wlan_off = 150,
	event_wlan_on = 151,
	event_wlan_starting = 152,
	event_wlan_stopping = 153,
	event_wlan_configuring = 154,
	event_wlan_dfs_starting = 155,
	event_wlan_device_init = 157,
	event_wlan_device_present = 158,
	event_wlan_device_gone = 159,
	event_wlan_wds_repeater_enable = 161,
	event_wlan_wds_repeater_disable = 162,
	event_wps_start = 163,
	event_wps_error = 164,
	event_wps_timeout = 165,
	event_wps_success = 166,
	event_wps_done = 167,
	event_wlan_rssi_disconnected = 169,
	event_wlan_rssi_level = 170,
	event_wireless_stick_and_surf_start = 173,
	event_wireless_stick_and_surf_error = 174,
	event_wireless_stick_and_surf_timeout = 175,
	event_wireless_stick_and_surf_success = 176,
	event_wireless_stick_and_surf_done = 177,
	event_wlan_guest_enabled = 178,
	event_wlan_guest_disabled = 179,
	event_wlan_guest_device_present = 180,
	event_wlan_guest_device_allgone = 181,
	event_wlan_device_max_reached = 182,
	event_wlan_device_max_not_reached = 183,
	event_usb_host_stick_and_surf_start = 184,
	event_usb_host_stick_and_surf_error = 185,
	event_usb_host_stick_and_surf_done = 186,
	event_usb_host_stick_and_surf_success = 187,
	event_usb_host_connected = 190,
	event_usb_host_disconnected = 191,
	event_usb_host_error = 192,
	event_usb_client_connected = 195,
	event_usb_client_disconnected = 196,
	event_usb_client_error = 197,
	event_filesystem_mounted = 200,
	event_filesystem_mount_failure = 201,
	event_filesystem_unmounting = 202,
	event_filesystem_unmount_failure = 203,
	event_filesystem_unmounted = 204,
	event_filesystem_done = 205,
	event_vu_level = 210,
	event_update_led1 = 220,
	event_update_led2 = 221,
	event_update_led3 = 222,
	event_update_led4 = 223,
	event_update_led5 = 224,
	event_dsl_verbunden = 232,
	event_dsl_training = 233,
	event_dsl_kein_kabel = 234,
	event_dsl_fehler = 235,
	event_dsl_nicht_verbunden = 236,
	event_power_on = 230,
	event_power_off = 231,
	event_lan1_active = 238,
	event_lan1_inactive = 239,
	event_lan2_active = 240,
	event_lan2_inactive = 241,
	event_lan3_active = 242,
	event_lan3_inactive = 243,
	event_lan4_active = 244,
	event_lan4_inactive = 245,
	event_device_init_start = 247,
	event_device_init_end = 248,
	event_device_reset = 249,
	event_device_factory_defaults = 250,
	event_switch_info_to_silence = 251,
	event_switch_info_to_tam_mwi = 252,
	event_switch_info_to_fax_mwi = 253,
	event_switch_info_to_klingelsperre_aktiv = 254,
	event_switch_info_to_missed_call = 255,
	event_switch_info_to_tam_fax_missed_call = 256,
	event_switch_info_to_voip_free = 257,
	event_switch_info_to_voip_srtp = 258,
	event_switch_info_to_dect = 259,
	event_switch_info_to_lan = 260,
	event_switch_info_to_wlan_active = 261,
	event_switch_info_to_online = 262,
	event_switch_info_to_usb_host = 263,
	event_switch_info_to_usb_client = 264,
	event_switch_info_to_budget = 265,
	event_switch_info_to_wlan_guest_active = 266,
	event_switch_info_to_wlan_guest_device = 267,
	event_irda_pulse = 270,
	event_irda___internal___ = 271,
	event_play_start = 274,
	event_play_stop = 275,
	event_standby_start = 278,
	event_standby_stop = 279,
	event_error__internal__ = 280,
	event_error__internal_wps__ = 281,
	event_error__internal_pppoe__ = 282,
	event_error__internal_voip__ = 283,
	event_error__internal_tr69__ = 284,
	event_temperature_warning_off = 290,
	event_temperature_warning_level1 = 291,
	event_temperature_warning_level2 = 292,
	event_temperature_warning_reboot = 293,
	event_display_suspend = 295,
	event_display_wakeup = 296,
	event_display_suspend_on_idle = 297,
	event_festnetz_led_enable = 300,
	event_festnetz_led_disable = 301,
	event_dect_led_enable = 302,
	event_dect_led_disable = 303,
	event_socket_on = 310,
	event_socket_off = 311,
	event_socket_overcurrent_protection = 312,
	event_plc_on = 330,
	event_plc_off = 331,
	event_plc_standby_start = 332,
	event_plc_standby_stop = 333,
	event_plc_not_connect = 334,
	event_plc_pairing_start = 335,
	event_plc_pairing_stop = 336,
	event_plc_pairing_done = 337,
	event_acmeter_ctrl_run = 340,
	event_acmeter_ctrl_reset = 341,
	event_acmeter_ctrl_prog_start = 342,
	event_acmeter_ctrl_prog_run = 343,
	event_lte_antenna_external = 350,
	event_lte_antenna_internal = 351,
	event_button_events_enable = 360,
	event_button_events_disable = 361,
	LastEvent = 362
};
#endif /*--- #ifndef AVM_LED_INTERNAL ---*/


#ifndef __KERNEL__
int led_event_init(int version);  /* Aufruf: led_event_init(LED_EVENT_VERSION)*/
int led_event_deinit(void);

int led_event_set(enum _led_event event, unsigned int value);
int led_event(enum _led_event event);  /* macht implizit: value=1 */

int led_event_set_with_event(enum _led_event event, unsigned int value, unsigned int param_len, void *param);
int led_event_with_event(enum _led_event event, unsigned int param_len, void *param);  /* macht implizit: value=1 */
#endif /*--- #ifndef __KERNEL__ ---*/


#ifdef __KERNEL__
/*------------------------------------------------------------------------------------------*\
 * KONTROLLE FÜR EVENT-GESTEUERTE LEDs
\*------------------------------------------------------------------------------------------*/
#include "linux/errno.h"
extern int (*led_event_action)(int version, enum _led_event event, unsigned int value);
void led_event_disable_timer(void) __attribute__((weak));


/*------------------------------------------------------------------------------------------*\
 * KONTROLLE FÜR EXTERN GESTEUERTE LEDs
\*------------------------------------------------------------------------------------------*/
enum _led_action {
    led_ext_enable,
    led_ext_disable,
    led_ext_on,
    led_ext_off,
    led_ext_error
};

enum _led_groups {
    led_cpu_gpio_leds  = 0,
    led_cpu_shift_leds = 1,
    led_wlan_leds      = 2,
    led_lan_leds       = 3,
    led_dsl_leds       = 4,
    led_dect_leds      = 5,
    led_usb_leds       = 6,
};

typedef void (*led_callback_t)(enum _led_groups group_id, unsigned int led_id, enum _led_action action);
typedef int (*led_action_t)(unsigned int led_id, enum _led_action action, led_callback_t callback_func);

int __led_register_external(enum _led_groups group_id, led_action_t action_func) __attribute__ ((weak));
void __led_release_external(enum _led_groups group_id) __attribute__ ((weak));

/*------------------------------------------------------------------------------------------*\
 * Registriert zur group_id eine entsprechende Action-Funktion und gibt 0 bei Erfolg zurück
 * Die Action-Funktion muss im Interrupt-Kontext aufrufbar sein und sollte größere Aktionen
 * asynchron ausführen. 
 * Parameter der Action Funktion sind:
 *   - ĺed_id        => 0..99
 *   - action        => led_ext_enable/led_ext_disable ermöglichen eine De/Initialisierung der 
 *                      LED-Ansteuerung; led_ext_error braucht hier nicht behandelt werden
 *   - callback_func => falls die Aktion asychron ausgeführt wird, soll callback_func()
 *                      anschließen daufgerufen werden (im Fehlerfall mit action = led_ext_error)
 *                      callback_func kann auch ein NULL-Pointer sein
 *   Erwartete Rückgabewerte der Funktion sind
 *     0       Aktion wurde gleich ausgeführt
 *     -EBUSY  Aktion wird asynchron ausgeführt und später per Aufruf von callback_func quittiert
 *     -EIO    Aktion ist fehlgeschlagen (z.B. ungültige led_id)
 * 
\*------------------------------------------------------------------------------------------*/
static inline int led_register_external(enum _led_groups group_id, led_action_t action_func) {
    if(&__led_register_external)
        return __led_register_external(group_id, action_func);
    return -ENODEV;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void led_release_external(enum _led_groups group_id) {
    if(&__led_release_external)
        __led_release_external(group_id);
}
#endif /*--- #ifdef __KERNEL__ ---*/


#define LEDCTL_VERSION  2


#ifdef AVM_LED_INTERNAL
struct led_event {
	int version;
	enum _led_event event;
	int event_value;
	void *event_param;
	unsigned int event_param_size;
};

#define LED_IOC_EVENT            _IOW('E', 0, struct led_event)
#define LED_IOC_EVENT_WITH_PARAM _IOW('E', 1, struct led_event)

#endif /*--- #ifdef AVM_LED_INTERNAL---*/
#endif
