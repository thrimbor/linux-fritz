/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,2007,2008,2009,2010 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation version 2 of the License.
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

#ifndef _CPPHY_TYPES_H_
#define _CPPHY_TYPES_H_

#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/etherdevice.h>
#include <linux/avm_cpmac.h>
#include <linux/avm_event.h>
#include "cpmac_if.h"
#include "cpmac_priority.h"
#include "cpmac_product_conf.h"

typedef struct {
  struct net_device *p_dev;
  cpmac_phy_handle_t phy_handle;
  cpmac_service_funcs_t service_funcs;
} dev_desc_t;

typedef enum {
    CPPHY_HW_ST_INITIALIZED = 1,
    CPPHY_HW_ST_OPENED
} cpphy_hw_state_t;

typedef enum {
    CPPHY_MDIO_ST_FINDING    = 2,
    CPPHY_MDIO_ST_FOUND      = 3,
    CPPHY_MDIO_ST_NWAY_START = 4,
    CPPHY_MDIO_ST_NWAY_WAIT  = 5,
    CPPHY_MDIO_ST_LINK_WAIT  = 6,
    CPPHY_MDIO_ST_LINKED     = 7,
    CPPHY_MDIO_ST_LOOPBACK   = 8,
    CPPHY_MDIO_ST_PDOWN      = 9
} cpphy_mdio_state_t;

typedef enum {
    CPPHY_SWITCH_MODE_16BIT = (1 << 0),
    CPPHY_SWITCH_MODE_READ  = (1 << 1),
    CPPHY_SWITCH_MODE_WRITE = (1 << 2)
} cpphy_switch_mode_t;

/* Restructured switch configuration struct */
struct avm_switch_struct {
    struct {
        unsigned char  written;           /* Flag, if info is already written */
        unsigned char  keep_tag_outgoing; /* Tag outgoing packets from this port */
        unsigned char  use_tag_incoming;  /* Tag incoming packets into this port if untagged */
        unsigned short vid;               /* Tag untagged incoming packets with this VID */
        unsigned short port_vlan_mask;    /* Configured ports for port VLAN */
    } port[AVM_CPMAC_MAX_PORTS];
    struct {
        unsigned char  written;      /* Flag, if info is already written */
        unsigned char  active;       /* Flag, if info is already written */
        unsigned short vid;          /* VLAN ID (0 - 4095) */
        unsigned short fid;          /* Relevant bits for MAC table */
        unsigned short target_mask;  /* Mask for the ports that belong to this VLAN */
        unsigned short tagged_mask;  /* Mask for the ports that should receive tagged packets */
    } vlan[CPMAC_MAX_VLAN_GROUPS];
};

typedef enum {
    CPPHY_POWER_GLOBAL_NONE     = 0,
    CPPHY_POWER_GLOBAL_SET_OFF  = 1,
    CPPHY_POWER_GLOBAL_OFF      = 2,
    CPPHY_POWER_GLOBAL_ON       = 3
} cpmac_power_mode_t;

typedef enum {
    CPPHY_POWER_MODE_OFF = 0,
    CPPHY_POWER_MODE_ON,
    CPPHY_POWER_MODE_ON_IDLE_MDI,
    CPPHY_POWER_MODE_ON_IDLE_MDIX,
    CPPHY_POWER_MODE_ON_REDUCE_EMI,
    CPPHY_POWER_MODE_ON_TESTMODE_10,
    CPPHY_POWER_MODE_ON_TESTMODE_100,
    CPPHY_POWER_MODE_ON_STOP_XOVER,
    CPPHY_POWER_MODE_LINK_LOST,
    CPPHY_POWER_MODE_ON_MDI,
    CPPHY_POWER_MODE_ON_MDI_MLT3,
    CPPHY_POWER_MODE_ON_MDI_NLP,
    CPPHY_POWER_MODE_ON_MDIX,
    CPPHY_POWER_MODES
} cpphy_switch_powermodes_t;

typedef struct {
    unsigned int   timeout;
    unsigned char  current_on;
    unsigned char  current_mode;
    unsigned char  is_mdix;
    unsigned char  current_port;
    unsigned int   linked_ports;
    unsigned int   powersaving_on_any_port;
    unsigned int   roundrobin;
    adm_phy_power_setup_t setup;
    adm_phy_power_setup_t setup_asked_for;
    unsigned short global_portset;
    unsigned char             port_on[AVM_CPMAC_MAX_PORTS];
    unsigned char             port_modes_possible[AVM_CPMAC_MAX_PORTS];
    cpphy_switch_powermodes_t port_mode[AVM_CPMAC_MAX_PORTS];
    unsigned char             port_throttle_req[AVM_CPMAC_MAX_PORTS];
    unsigned char             port_throttle[AVM_CPMAC_MAX_PORTS];
    unsigned long  time_to_wait_to[AVM_CPMAC_MAX_PORTS];
} cpmac_power;

typedef enum {
    CPMAC_WORK_TICK = 0,
    CPMAC_WORK_UPDATE_MAC_TABLE, 
    CPMAC_WORK_SWITCH_DUMP,
    CPMAC_WORK_TOGGLE_PPPOA,
    CPMAC_WORK_TOGGLE_VLAN,
    CPMAC_WORK_PORT_MIRROR,
    CPMAC_WORK_DEBUG,
    CPMAC_WORK_MAX_ITEMS
} cpmac_work_ident_t;

typedef struct cpphy_mdio_t_struct *cpphy_mdio_t_struct_ptr;
typedef unsigned long (*cpmac_work_function) (cpphy_mdio_t_struct_ptr mdio);
typedef struct {
    unsigned int        active;
    cpmac_work_function item;
    unsigned long       next_run;
    unsigned long       last_dump;
    unsigned int        number_of_calls;
} cpmac_work_item_t;

typedef struct cpmac_funcs_struct cpmac_funcs_t;

typedef struct cpphy_mdio_t_struct {
    cpmac_funcs_t                 *f;
    cpphy_mdio_state_t             state;
    unsigned int                   high_phy;
    unsigned int                   reset_bit;
    unsigned int                   nway_mode;
    unsigned int                   control;
    unsigned int                   timeout;
    unsigned char                  phy_num;
    unsigned char                  inst;
    cpmac_priv_t                  *cpmac_priv;
    cpmac_work_item_t              work[CPMAC_WORK_MAX_ITEMS];
#   if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE)
    struct delayed_work            cpmac_work;
#   else /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
    struct work_struct             cpmac_work;
#   endif /*--- #else ---*/ /*--- #if (KERNEL_VERSION(2,6,28) <= LINUX_VERSION_CODE) ---*/
    volatile unsigned int          stop_work;
    volatile unsigned int          work_to_do;
    wait_queue_head_t              waitqueue;
    unsigned int                   switch_dump_value;
    volatile unsigned char         wan_tagging_enable;
    struct sk_buff_head            ar_waiting_list;
    int                            ar_waiting_list_count_at_update;
    unsigned int                   run_ar_queue_without_removal_counter;
    struct semaphore               semaphore;
    struct semaphore               semaphore_switch;
    unsigned int                   switch_keep_tagging;
    unsigned int                   linked;
    unsigned int                   half_duplex;
    unsigned int                   slow_speed;
    unsigned int                   mdix;
    unsigned int                   power_down;
    void                          *power_handle;
    cpphy_switch_mode_t            Mode;
    unsigned int                   cpmac_irq;
    struct avm_switch_struct       switch_status;
    cpmac_switch_configuration_t  *switch_predefined_configs;
    t_cpphy_switch_config          switch_config;
    unsigned char                  mode_pppoa;
    unsigned char                  event_update_needed;
} cpphy_mdio_t;

/* Defined in cpmac_priority.h */
/*--- typedef struct { ...  } cpphy_tcb_t; ---*/
/*--- typedef struct { ...  } cpphy_rcb_t; ---*/

typedef struct cpphy_cppi_struct {
    cpmac_funcs_t          *f;
    int                     RxTeardownPending;
    cpphy_rcb_t            *RxPrevEnqueue;
    cpphy_rcb_t            *RxCurrDequeue;
    cpphy_rcb_t            *RxFirst;
    cpphy_rcb_t            *RxLast;
    char                   *RcbStart;
    cpphy_tcb_t            *TxPrevEnqueue;
    cpphy_tcb_t            *TxCurrDequeue;
    volatile cpphy_tcb_t   *TxFirst;
    volatile cpphy_tcb_t   *TxLast;
    volatile cpphy_tcb_t   *TxFirstFree;
    volatile cpphy_tcb_t   *TxLastFree;
    cpphy_tcb_prio_queues_t TxPrioQueues;
    spinlock_t              skb_transit_spinlock;
    struct sk_buff_head skbs_in_transit;
    unsigned long           TxLastCompleted;
    unsigned int            NeededDMAtcbs;
    struct {
        unsigned int        tcbs_alloced;
        unsigned int        tcbs_freed;
        unsigned int        tcbs_alloced_dynamic;
        unsigned int        tcbs_freed_dynamic;
    } support;
    atomic_t                dequeue_running;
    atomic_t                dma_send_running;
    volatile unsigned int   TxDmaActive;
    int                     TxTeardownPending;
    char                   *TcbStart;
    int                     TxOpen;
    int                     RxOpen;
    unsigned char           TxChannel;
    unsigned int            MaxNeedCount;
    unsigned int            CurrNeedCount;
    cpphy_hw_state_t        hw_state;
    unsigned int            hash1;
    unsigned int            hash2;
    cpmac_priv_t           *cpmac_priv;
    cpphy_mdio_t           *mdio;
} cpphy_cppi_t;

typedef void           (*cpmac_function_init_phy_t)(cpphy_mdio_t *mdio);
typedef void           (*cpmac_function_deinit_phy_t)(cpphy_mdio_t *mdio);
typedef void           (*cpmac_function_switch_dump_t)(cpphy_mdio_t *mdio);
typedef void           (*cpmac_function_set_good_vid_bitshift)(cpphy_mdio_t *mdio, struct avm_cpmac_config_struct *config);
typedef void           (*cpmac_function_config_ports)(cpphy_mdio_t *mdio);
typedef void           (*cpmac_function_switch_port_power)(cpphy_mdio_t *mdio, unsigned char port, unsigned char power_on);
typedef void           (*cpmac_function_power_on_boot)(cpphy_mdio_t *mdio, unsigned short portset);
typedef void           (*cpmac_function_prepare_reboot)(cpphy_mdio_t *mdio);
typedef unsigned char  (*cpmac_function_get_phy_port)(cpphy_mdio_t *mdio, struct sk_buff *skb);
typedef unsigned long  (*cpmac_function_ar_work_item)(cpphy_mdio_t *mdio);
typedef unsigned int   (*cpmac_function_mgmt_check_link)(cpphy_mdio_t *mdio);
typedef unsigned int   (*cpmac_function_check_link)(cpphy_mdio_t *mdio);
typedef void           (*cpmac_function_config_vlan_group)(cpphy_mdio_t   *mdio,
                                                          unsigned short  vid,
                                                          unsigned short  portset,
                                                          unsigned short  tagged_portset,
                                                          unsigned char   port,
                                                          unsigned char   FID,
                                                          unsigned char   use_as_default);
typedef unsigned short (*cpmac_function_get_new_vid)(cpphy_mdio_t *mdio, unsigned char device);
typedef void           (*cpmac_function_set_default_vid)(cpphy_mdio_t   *mdio, 
                                                         unsigned char   device, 
                                                         unsigned short  portset);
typedef cpmac_err_t    (*cpmac_function_set_port_vid_mapping)(cpphy_mdio_t *mdio, 
                                                              unsigned char device,
                                                              unsigned char port);
typedef void           (*cpmac_function_vlan_clear)(cpphy_mdio_t *mdio);
typedef void           (*cpmac_function_vlan_add)(cpphy_mdio_t *mdio, unsigned int vid, unsigned int mem_mask);
typedef cpmac_err_t    (*cpmac_function_configure_special)(cpphy_mdio_t *mdio, cpmac_switch_configuration_t *config);
typedef int            (*cpmac_function_set_switch_register)(cpphy_mdio_t *mdio, unsigned int addr, unsigned int dat);
typedef unsigned int   (*cpmac_function_get_switch_register)(cpphy_mdio_t *mdio, unsigned int addr);
typedef unsigned int   (*cpmac_function_check_external_tagging)(cpphy_cppi_t *cppi, cpphy_tcb_t *tcb);
typedef void           (*cpmac_function_update_hw_status)(cpphy_mdio_t *mdio, struct net_device_stats *stats);
typedef unsigned long  (*cpmac_function_set_wan_keep_tagging)(cpphy_mdio_t *mdio);
typedef cpmac_err_t    (*cpmac_function_set_mode_pppoa)(cpphy_mdio_t *mdio, unsigned int enable_pppoa);
typedef void           (*cpmac_function_switch_wan_keep_tagging)(cpphy_mdio_t *mdio, unsigned char port);
typedef void           (*cpmac_function_print_mac_table)(cpphy_mdio_t *mdio);
typedef void           (*cpmac_function_transfer_startstop)(unsigned int start);
typedef void           (*cpmac_function_dump_counters)(cpphy_mdio_t *mdio);
typedef void           (*cpmac_function_mirror_port_t)(cpphy_mdio_t *mdio);
typedef void           (*cpmac_function_switch_port_speed_throttle)(cpphy_mdio_t *mdio,
                                                                    unsigned char port,
                                                                    unsigned char do_throttle);
typedef cpmac_err_t    (*cpmac_function_add_port_to_wan)(cpphy_mdio_t *mdio, unsigned char port, unsigned short vid);
typedef void           (*cpmac_function_set_igmp_fwd)(cpphy_mdio_t *mdio, unsigned int fwd_portmap);
typedef cpmac_err_t    (*cpmac_function_add_trunk_port)(cpphy_mdio_t *mdio, cpmac_trunk_port_configuration_t trunk_config);


struct cpmac_funcs_struct {
    cpmac_function_add_port_to_wan            add_port_to_wan;
    cpmac_function_add_trunk_port             add_trunk_port;
    cpmac_function_ar_work_item               ar_work_item;
    cpmac_function_check_external_tagging     check_external_tagging;
    cpmac_function_check_link                 check_link;
    cpmac_function_config_ports               config_ports;
    cpmac_function_configure_special          configure_special;
    cpmac_function_config_vlan_group          config_vlan_group;
    cpmac_function_transfer_startstop         cpmac_transfer_startstop;
    cpmac_function_dump_counters              dump_counters;
    cpmac_function_get_new_vid                get_new_vid;
    cpmac_function_get_phy_port               get_phy_port;
    cpmac_function_get_switch_register        get_switch_register;
    cpmac_function_init_phy_t                 init_phy;
    cpmac_function_deinit_phy_t               deinit_phy;
    cpmac_function_mgmt_check_link            mgmt_check_link;
    cpmac_function_power_on_boot              power_on_boot;
    cpmac_function_prepare_reboot             prepare_reboot;
    cpmac_function_print_mac_table            print_mac_table;
    cpmac_function_set_default_vid            set_default_vid;
    cpmac_function_set_good_vid_bitshift      set_good_vid_bitshift;
    cpmac_function_set_igmp_fwd               set_igmp_fwd;
    cpmac_function_set_mode_pppoa             set_mode_pppoa;
    cpmac_function_set_port_vid_mapping       set_port_vid_mapping;
    cpmac_function_set_switch_register        set_switch_register;
    cpmac_function_set_wan_keep_tagging       set_wan_keep_tagging;
    cpmac_function_mirror_port_t              mirror_port;
    cpmac_function_switch_dump_t              switch_dump;
    cpmac_function_switch_port_power          switch_port_power;
    cpmac_function_switch_port_speed_throttle switch_port_speed_throttle;
    cpmac_function_switch_wan_keep_tagging    switch_wan_keep_tagging;
    cpmac_function_update_hw_status           update_hw_status;
    cpmac_function_vlan_add                   vlan_add;
    cpmac_function_vlan_clear                 vlan_clear;
};

typedef struct {
    cpmac_funcs_t   *f;
    cpmac_priv_t    *cpmac_priv;
    unsigned char    instance;
    unsigned char    ports;        /* Number of ports of this PHY/switch */
    unsigned char    port_offset;  /* This PHY starts at the offset ... in the global portmap */
    unsigned char    is_used;
    unsigned char    is_open;
    unsigned char    is_out_of_reset;
    unsigned char    init_ran;
    cpphy_cppi_t     cppi;
    cpphy_mdio_t     mdio;
    cpmac_product_config_phy_struct *config;
} cpphy_global_t;

typedef struct cpmac_device_vlan_change_cb_struct {
    struct cpmac_device_vlan_change_cb_struct *next;
    char                                       devicename[AVM_CPMAC_MAX_DEVICE_NAME_LENGTH + 1];
    cpmac_device_vlan_change_cb                cb;
    void                                      *context;
} cpmac_device_vlan_change_cb_item_t;

typedef struct cpmac_traffic_classify_struct {
    volatile cpmac_wlan_classify_func_ptr wlan_classify;
    signed int                            packet_counter[CPMAC_TRAFFIC_NUMBER_OF_CLASSES];
} cpmac_traffic_classify_struct_t;

typedef struct cpmac_macportmap {
    struct cpmac_macportmap *link; /* link list in hash table */
    struct cpmac_macportmap *lru_next;
    struct cpmac_macportmap *lru_prev;
    struct sk_buff_head      list;
    unsigned                 isinhash     : 1,
                             isonlru      : 1,
                             isinmactable : 1,
                             is_resolved  : 1;
    unsigned char            mac[ETH_ALEN];
    unsigned char            port;   /*--- 0 CPU, 1-4 PHY Port ---*/
    unsigned long            last_access;  /*--- in jiffies ---*/
} cpmac_macportmap_t;

#define MACPORT_HSIZ    256  /* check machash() before change this */

typedef struct cpmac_macportmap_global {
    cpmac_macportmap_t *hashtab[MACPORT_HSIZ];
    cpmac_macportmap_t *lru_head;
    cpmac_macportmap_t *lru_tail;
    atomic_t            mapentries; /* number of map entries */
    atomic_t            nskbs;      /* number of SKBs waiting */
    unsigned long       skb_dropped;
} cpmac_macportmap_global_t;

typedef struct {
    cpphy_global_t                           cpphy[AVM_CPMAC_MAX_PHYS];
    unsigned char                            phys;
    unsigned char                            ports;
    unsigned char                            map_port_to_instance[AVM_CPMAC_MAX_PORTS];
    struct workqueue_struct                 *workqueue;
    unsigned int                             timer_count;
    struct timer_list                        timer;
    cpmac_traffic_classify_struct_t          traffic_classify;
    spinlock_t                               ar8216_spinlock;
    cpmac_macportmap_global_t                macportmap;
    cpmac_power                              power;
    cpmac_power_mode_t                       global_power;
    cpmac_product_struct                    *products;
    unsigned int                             product_id;
    struct cpmac_event_struct                event_data;
    volatile cpmac_fast_forward_rx_func_ptr  ffw_rx_ptr[AVM_CPMAC_MAX_PORTS];
    cpmac_device_vlan_change_cb_item_t      *vlan_change_cb;
    char                                     device_name[AVM_CPMAC_MAX_PHYS][IFNAMSIZ];
    unsigned int                             device_name_length[AVM_CPMAC_MAX_PHYS];
} cpmac_global_t;

extern cpphy_global_t cpmac_cpphy_global[];
extern cpmac_global_t cpmac_global;
extern cpmac_switch_configuration_t switch_configuration[CPMAC_SWITCH_CONF_MAX_PRODUCTS][CPMAC_MODE_MAX_NO];

#endif /*--- #ifndef _CPPHY_TYPES_H_ ---*/

