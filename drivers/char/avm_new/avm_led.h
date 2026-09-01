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
#ifndef _AVM_LED_H_
#define _AVM_LED_H_

#include "avm_sammel.h"

#if !defined(DEBUG_SAMMEL)
#define DEBUG_SAMMEL
#if defined(AVM_LED_DEBUG)
#define DEB_ERR(args...)     printk(KERN_ERR args)
#define DEB_WARN(args...)    printk(KERN_WARNING args)
#define DEB_NOTE(args...)    printk(KERN_NOTICE args)
#define DEB_INFO(args...)    printk(KERN_INFO args)
#define DEB_TRACE(args...)   printk(KERN_INFO args)
#else /*--- #if defined(AVM_LED_DEBUG) ---*/
#define DEB_ERR(args...)     printk(KERN_ERR args)
#define DEB_WARN(args...)
#define DEB_NOTE(args...)
#define DEB_INFO(args...)
#define DEB_TRACE(args...)
#endif /*--- #else ---*/ /*--- #if defined(AVM_LED_DEBUG) ---*/
#endif /*--- #if !defined(DEBUG_SAMMEL) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <linux/avm_led.h>
#include <linux/avm_event.h>
#include <asm/atomic.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_list_header {
    struct _avm_led_list_header *next;
    struct _avm_led_list_header *prev;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_list {
    struct _avm_led_list_header *first;
    struct _avm_led_list_header *last;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_configs {
    char Name[64];
    unsigned int Nr;
    unsigned int instance;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_low_level_driver {
    char *name;
    int (*init)(unsigned int, unsigned int, char *);
    char *(*show)(unsigned int, unsigned int *);
    void (*exit)(unsigned int);
    int (*action)(unsigned int, unsigned int);
    int (*sync)(unsigned int, unsigned int);
    int (*get_flags)(void);
#define AVM_LED_DRIVER_FLAGS_PRIO_0_INSTANCE        0
#define AVM_LED_DRIVER_FLAGS_PRIO_1_INSTANCE        1
#define AVM_LED_DRIVER_FLAGS_PRIO_2_INSTANCE        2
#define AVM_LED_DRIVER_FLAGS_PRIO_3_INSTANCE        3

#define AVM_LED_DRIVER_FLAGS_DISABLE_INSTANCES      4
};

/*------------------------------------------------------------------------------------------*\
 * forward decl.
\*------------------------------------------------------------------------------------------*/
struct _avm_virt_led;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_blink_mode_driver {
    int (*init)(unsigned int, struct _avm_virt_led *);
    void (*exit)(int);
    void (*stop)(int);
    void (*show)(int);
    void (*action)(int, unsigned int, unsigned int);
    void (*sync)(int);
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led {
    dev_t         device;
    struct cdev  *cdev;
    void         *event_handle;
};
extern struct _avm_led avm_led;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_instance_handle {
    atomic_t link_count;
    struct _avm_virt_led *V[AVM_LED_MAX_INSTANCE];
    unsigned int mask[AVM_LED_MAX_INSTANCE];
    unsigned int enable[AVM_LED_MAX_INSTANCE];
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_map_state {
    struct _avm_led_list_header list;
    unsigned int from_state;
    unsigned int to_state;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_virt_led {
    struct _avm_led_list_header list;
    char name[32];
    unsigned int instance;
    int mapped_to_handle;
    int mapped_both;
#define AVM_LED_PRIVIOUS_STACK_SIZE     16
    struct _avm_led_state *privious_state_stack[AVM_LED_PRIVIOUS_STACK_SIZE];
    unsigned int privious_state_stack_pointer;
    unsigned int current_index;
    struct _avm_led_list map_states;
    struct _avm_led_state *current_state;
    struct _avm_led_state *led_states;
    struct _avm_led_instance_handle *instance_handle;
    struct _avm_led_low_level_driver *driver;
    unsigned int driver_handle;
    struct _avm_led_blink_mode_driver *mode;
    unsigned int mode_handle[AVM_LED_MAX_MODE_DRIVER];
    unsigned int event_length;
    struct _avm_event_led_status *event_ptr;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_state {
    struct _avm_virt_led *virt_led;
    unsigned int index;
    unsigned int mode_index;
    unsigned int param1;
    unsigned int param2;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _avm_led_open_data {
    unsigned int reserved;
};

extern int avm_led_monitor_level;

/*------------------------------------------------------------------------------------------*\
 * avm_led_gpio_mask_driver.c
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_mask_driver_init(unsigned int gpio, unsigned int pos, char *name);
void avm_led_gpio_mask_driver_exit(unsigned int handle);
int avm_led_gpio_mask_driver_action(unsigned int handle, unsigned int on);
char *avm_led_gpio_mask_driver_show(unsigned int handle, unsigned int *pPos);
int avm_led_gpio_mask_driver_sync(unsigned int handle, unsigned int state_id);

/*------------------------------------------------------------------------------------------*\
 * avm_led_gpio_bit_driver.c
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_bit_driver_init(unsigned int gpio, unsigned int pos, char *name);
void avm_led_gpio_bit_driver_exit(unsigned int handle);
int avm_led_gpio_bit_driver_action(unsigned int handle, unsigned int on);
char *avm_led_gpio_bit_driver_show(unsigned int handle, unsigned int *pPos);
int avm_led_gpio_bit_driver_sync(unsigned int handle, unsigned int state_id);

/*------------------------------------------------------------------------------------------*\
 * avm_led_gpio_bit_driver.c
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_dual_bit_driver_init(unsigned int gpio, unsigned int pos, char *name);
void avm_led_gpio_dual_bit_driver_exit(unsigned int handle);
int avm_led_gpio_dual_bit_driver_action(unsigned int handle, unsigned int on);
char *avm_led_gpio_dual_bit_driver_show(unsigned int handle, unsigned int *pPos);
int avm_led_gpio_dual_bit_driver_sync(unsigned int handle, unsigned int state_id);
int avm_led_gpio_dual_bit_driver_get_flags(void);

/*------------------------------------------------------------------------------------------*\
 * avm_led_shift_register_bit_driver.c
\*------------------------------------------------------------------------------------------*/
int avm_led_shift_register_bit_driver_init(unsigned int gpio, unsigned int pos, char *name);
void avm_led_shift_register_bit_driver_exit(unsigned int handle);
int avm_led_shift_register_bit_driver_action(unsigned int handle, unsigned int on);
char *avm_led_shift_register_bit_driver_show(unsigned int handle, unsigned int *pPos);
int avm_led_shift_register_bit_driver_sync(unsigned int handle, unsigned int state_id);

/*------------------------------------------------------------------------------------------*\
 * avm_led_shift_register_mask_driver.c
\*------------------------------------------------------------------------------------------*/
int avm_led_shift_register_mask_driver_init(unsigned int gpio, unsigned int pos, char *name);
void avm_led_shift_register_mask_driver_exit(unsigned int handle);
int avm_led_shift_register_mask_driver_action(unsigned int handle, unsigned int on);
char *avm_led_shift_register_mask_driver_show(unsigned int handle, unsigned int *pPos);
int avm_led_shift_register_mask_driver_sync(unsigned int handle, unsigned int state_id);

/*------------------------------------------------------------------------------------------*\
 * avm_led_gpio_bier_holen_driver.c
\*------------------------------------------------------------------------------------------*/
int avm_led_gpio_bier_holen_driver_init(unsigned int gpio_mask, unsigned int virtled, char *name);
char *avm_led_gpio_bier_holen_driver_show(unsigned int handle, unsigned int *pPos);
void avm_led_gpio_bier_holen_driver_exit(unsigned int handle);
int avm_led_gpio_bier_holen_driver_action(unsigned int handle, unsigned int on);
int avm_led_gpio_bier_holen_driver_sync(unsigned int handle, unsigned int state_id);

/*------------------------------------------------------------------------------------------*\
 * avm_led_shift_register_bier_holen_driver.c
\*------------------------------------------------------------------------------------------*/
int avm_led_shift_register_bier_holen_driver_init(unsigned int gpio_mask, unsigned int virtled, char *name);
char *avm_led_shift_register_bier_holen_driver_show(unsigned int handle, unsigned int *pPos);
void avm_led_shift_register_bier_holen_driver_exit(unsigned int handle);
int avm_led_shift_register_bier_holen_driver_action(unsigned int handle, unsigned int on);
int avm_led_shift_register_bier_holen_driver_sync(unsigned int handle, unsigned int state_id);

/*------------------------------------------------------------------------------------------*\
 * avm_led_mode.c
\*------------------------------------------------------------------------------------------*/
int avm_led_mode_init(unsigned int table_index, struct _avm_virt_led *);
void avm_led_mode_exit(int handle);
void avm_led_mode_stop(int handle);
void avm_led_mode_timer(unsigned long handle);
void avm_led_mode_action(int handle, unsigned int param1, unsigned int param2);
void avm_led_mode_sync(int virt_led_handle);

/*------------------------------------------------------------------------------------------*\
 * avm_led_profile.c
\*------------------------------------------------------------------------------------------*/
int avm_led_profile_init(unsigned int table_index, struct _avm_virt_led *);
void avm_led_profile_exit(int handle);
void avm_led_profile_stop(int handle);
void avm_led_profile_timer(unsigned long handle);
void avm_led_profile_action(int handle, unsigned int param1, unsigned int param2);
void avm_led_profile_sync(int virt_led_handle);

/*------------------------------------------------------------------------------------------*\
 * avm_led_file.c
\*------------------------------------------------------------------------------------------*/
int avm_led_init(void);
void avm_led_cleanup(void);
char *avm_led_monitor_user(void);


/*------------------------------------------------------------------------------------------*\
 * avm_led_init.c
\*------------------------------------------------------------------------------------------*/
void avm_led_init_tables(void);
int avm_led_register_led(char *virt_led_name, unsigned int virt_led_instance, unsigned int driver_type, unsigned int gpio, unsigned int pin_pos, char *pin_name);
void avm_led_release_led(int handle);
int avm_led_get_virt_led_handle(char *name, unsigned int instance);
void avm_led_virt_led_action(int handle, unsigned int state);
void avm_led_virt_led_ctrl(int handle, unsigned int master, unsigned int enable);
int avm_led_map_led(int org_handle, int mapped_to_handle, int both);
struct _avm_virt_led *avm_led_get_first_virt_led(void);
#ifdef CONFIG_AVM_LED_MODULE
void avm_led_release_all_leds(void);
#endif /*--- #ifdef CONFIG_AVM_LED_MODULE ---*/

void avm_led_show_virt_led(int handle);
int avm_led_virt_led_instance(int handle);
char *avm_led_virt_led_name(int handle);


struct _avm_led_instance_handle *avm_led_alloc_instance(char *name);
int avm_led_free_instance(struct _avm_led_instance_handle *I);


int avm_led_map_states(int handle, unsigned int from_state, unsigned int to_state);
struct _avm_led_map_state *avm_led_find_remaped_state(int handle, unsigned int state);
int avm_led_del_all_remaped_states(int handle);
enum _avm_event_led_id avalanche_led_get_new_id_from_handle(int handle);

/*------------------------------------------------------------------------------------------*\
 * avm_led_init.c listen verwaltung
\*------------------------------------------------------------------------------------------*/
int avm_led_add_to_list(struct _avm_led_list *L, struct _avm_led_list_header *V);
int avm_led_del_from_list(struct _avm_led_list *L, struct _avm_led_list_header *V);
struct _avm_virt_led *avm_led_get_first_virt_led(void);
struct _avm_virt_led *avm_led_get_next_virt_led(struct _avm_virt_led *V);
struct _avm_led_map_state *avm_led_get_first_mapped_state(struct _avm_virt_led *V);
struct _avm_led_map_state *avm_led_get_next_mapped_state(struct _avm_led_map_state *M);


/*------------------------------------------------------------------------------------------*\
 * avm_led_if.c
\*------------------------------------------------------------------------------------------*/
void avm_led_notify_unvalid_handle(int handle);

/*------------------------------------------------------------------------------------------*\
 * avm_led_write.c
\*------------------------------------------------------------------------------------------*/
struct file;  /* forward definition */
ssize_t avm_led_write(struct file *, const char *, size_t , loff_t *);

/*------------------------------------------------------------------------------------------*\
 * avm_led_event.c
\*------------------------------------------------------------------------------------------*/
int avm_led_event_init(void);
int avm_led_event_exit(void);
int avm_led_info_event(struct _avm_virt_led *);
int avm_led_status_event(struct _avm_virt_led *V);

/*------------------------------------------------------------------------------------------*\
 * avm_led_old.c
\*------------------------------------------------------------------------------------------*/
void *avalanche_led_register (const char *module_name, int instance_num);
int avalanche_led_get_handle_from_old_id(int old_handle);
int avalanche_led_get_old_id_from_handle(int handle);
void  avalanche_led_action (void *handle, int state_id);
int avalanche_led_unregister(void *handle);

/*------------------------------------------------------------------------------------------*\
 * avm_led_hardware_error.c
\*------------------------------------------------------------------------------------------*/
void hardware_error_log(char *error_msg, unsigned int blink_code);

#endif /*--- #ifndef _AVM_LED_H_ ---*/
