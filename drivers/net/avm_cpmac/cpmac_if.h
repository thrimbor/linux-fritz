/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,...,2013 AVM GmbH <fritzbox_info@avm.de>
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

#ifndef _CPMAC_IF_H_
#define _CPMAC_IF_H_

/*-------------------------------------------------------------------------------------*\
    Interface between kernel related cpmac driver and hardware related cpphy driver
    (although common part of cpphy drivers is linked with cpmac driver for performance reasons)
\*-------------------------------------------------------------------------------------*/

#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include "cpmac_const.h"
#ifdef CONFIG_IP_MULTICAST_FASTFORWARD
#include <linux/mcfastforward.h>
#endif
#include "cpmac_priority.h"
#include "cpgmac_f.h"
#include <linux/avm_cpmac.h>
#include "cpphy_switch.h"
#if defined(CONFIG_MIPS_UR8)
#include <asm/mach-ur8/hw_nwss.h>
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/
#if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)
#include "cpmac_fusiv_if.h"
#endif /*--- #if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) ---*/

typedef enum {
  CPMAC_ERR_NOERR = 0,
  CPMAC_ERR_ALREADY_REGISTERED = 1,
  CPMAC_ERR_NOMEM = 2,
  CPMAC_ERR_EXCEEDS_LIMIT = 3,
  CPMAC_ERR_REGISTER_FAILED = 4,
  CPMAC_ERR_ILL_HANDLE = 5,
  CPMAC_ERR_CHAN_NOT_OPEN = 6,
  CPMAC_ERR_NO_BUFFER = 7,
  CPMAC_ERR_INTERNAL = 8,
  CPMAC_ERR_DROPPED = 9,
  CPMAC_ERR_NEED_BUFFER = 10,
  CPMAC_ERR_ILL_CONTROL = 11,
  CPMAC_ERR_NO_SWITCH = 12,
  CPMAC_ERR_TEARDOWN = 13,
  CPMAC_ERR_NO_TEARDOWN = 14,
  CPMAC_ERR_HW_NOT_INITIALIZED = 15,
  CPMAC_ERR_NO_VLAN_POSSIBLE = 16,
  CPMAC_ERR_MISC = 17,
} cpmac_err_t;

typedef struct {
    int promiscous;
    int broadcast;
    int multicast;
    int short_frames;
    int long_frames;
    int auto_negotiation;
} cpmac_capabilities_t;

typedef struct {
#   if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185)
    struct {
        void                   *port;
        fusiv_mdio_read_t       mdio_read;
        fusiv_mdio_write_t      mdio_write;
        fusiv_set_port_speed_t  set_port_speed;
    } fusiv;
#   endif /*--- #if defined(CONFIG_FUSIV_VX180) || defined(CONFIG_FUSIV_VX185) ---*/
#   if defined(CONFIG_ARCH_PUMA5)
#   endif /*--- #if defined(CONFIG_ARCH_PUMA5) ---*/
#   if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32))
    struct napi_struct napi;
#   endif /*--- #if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)) ---*/
    unsigned int pseudo_device;
    unsigned int intr;
    unsigned int inst;
    struct cpphy_cppi_struct *cppi;
    struct {
        int           handle;
        unsigned char on;
    } led[CPMAC_MAX_LED_HANDLES];
    int irq_pace_handle;
    char irq_pace_value;
    struct net_device *owner;
    struct proc_dir_entry *procfs_dir;
#   if defined(CONFIG_MIPS_UR8)
    struct cpgmac_f_regs *CPGMAC_F;
    struct ur8_queue_manager *UR8_QUEUE;
    struct ur8_nwss_register *UR8_NWSS;
#   endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/
    unsigned int mac_reset_bit;
             int phy_reset_bit;
    unsigned int mac_pdown_bit;
    unsigned long set_to_close;
    unsigned long dev_size;
    struct tasklet_struct tasklet;
    unsigned long local_stats_rx_errors;
    unsigned long local_stats_rx_length_errors;
    unsigned long local_stats_tx_errors;
    struct net_device_stats net_dev_stats;
    cpmac_capabilities_t capabilities;
    volatile unsigned int DoTick;
    cpphy_tcb_prio_queues_t *TxPrioQueues;
#   ifdef CPMAC_SIGNAL_CONGESTION
    volatile unsigned TxCount;
    volatile unsigned TxComplete;
    volatile unsigned TxProbe;
    volatile unsigned TxProbeComplete;
#   endif /*--- #ifdef CPMAC_SIGNAL_CONGESTION ---*/
} cpmac_priv_t;

typedef void * cpmac_phy_handle_t;

typedef enum {
  CPMAC_CONTROL_IND_MDIO_STATUS = 1,
  CPMAC_CONTROL_IND_EVENT_UPDATE = 2,
  CPMAC_CONTROL_IND_GET_INSTANCE_COUNT = 3,
} cpmac_control_ind_t;

typedef enum {
  CPMAC_CONTROL_REQ_UPDATE = 3,
  /*--- CPMAC_CONTROL_REQ_UPDATE_DETAIL = 4, ---*/
  CPMAC_CONTROL_REQ_IS_SWITCH = 5,
  CPMAC_CONTROL_REQ_MULTI_SINGLE = 6,
  CPMAC_CONTROL_REQ_MULTI_ALL = 7,
  CPMAC_CONTROL_REQ_PROMISCOUS = 8,
  CPMAC_CONTROL_REQ_HW_STATUS = 9,
  /*--- CPMAC_CONTROL_REQ_TEARDOWN = 10, ---*/
  /*--- CPMAC_CONTROL_REQ_START_DMA = 11, ---*/
  CPMAC_CONTROL_REQ_PORT_COUNT = 12,
  CPMAC_CONTROL_REQ_VLAN_CONFIG = 13,
  CPMAC_CONTROL_REQ_VLAN_GET_CONFIG = 14,
  CPMAC_CONTROL_REQ_SET_CONFIG = 15,
  CPMAC_CONTROL_REQ_GET_CONFIG = 16,
  CPMAC_CONTROL_REQ_GENERIC_CONFIG = 17
} cpmac_control_req_t;

/*-------------------------------------------------------------------------------------*\
  service functions offered by the cpphy driver (by callback via cpmac_register ())
\*-------------------------------------------------------------------------------------*/

/*--- start of cpphy (register int, alloc mem, ect.) ---*/
typedef cpmac_err_t (*cpphy_func_init_t)(cpmac_phy_handle_t phy_handle,
    cpmac_priv_t *mac_handle);

/*--- end of cpphy (free int, mem, etc.) ---*/
typedef void (*cpphy_func_deinit_t)(cpmac_phy_handle_t phy_handle);

/*--- request of cpmac to be done in cpphy ---*/
typedef cpmac_err_t (*cpphy_func_control_req_t)(cpmac_phy_handle_t phy_handle,
    cpmac_control_req_t cmd_request, ...);

/*--- send data ---*/
typedef cpmac_err_t (*cpphy_func_data_to_phy_t)(cpmac_phy_handle_t phy_handle,
    struct sk_buff *skb);

/*--- deferred int handling of rx/tx ---*/
typedef void (*cpphy_func_isr_tasklet_t)(unsigned long context);

/*--- in case of no int handling: let cpphy set eoi (to check: necessary?) ---*/
typedef void (*cpphy_func_isr_end_t)(cpmac_phy_handle_t phy_handle);

typedef struct {
  cpphy_func_init_t init;
  cpphy_func_deinit_t deinit;
  cpphy_func_control_req_t control_req;
  cpphy_func_data_to_phy_t data_to_phy;
  cpphy_func_isr_tasklet_t isr_tasklet;
  cpphy_func_isr_end_t isr_end;
} cpmac_service_funcs_t;


/*-------------------------------------------------------------------------------------*\
  service functions offered by cpmac (by name)
\*-------------------------------------------------------------------------------------*/

/*--- registration of a cpphy driver at the cpmac driver ---*/
cpmac_err_t cpmac_if_register (cpmac_phy_handle_t phy_handle, cpmac_service_funcs_t *service_funcs);

/*--- deregistration of a cpphy driver ---*/
cpmac_err_t cpmac_if_release (cpmac_priv_t *cpmac_priv);

/*--- cpphy driver has received data and wants to transfer it to the kernel ---*/
cpmac_err_t cpmac_if_data_from_phy (cpmac_priv_t *cpmac_priv, struct sk_buff *skb, unsigned int length);

/*--- cpphy driver has sent data from kernel ---*/
cpmac_err_t cpmac_if_data_to_phy_complete (cpmac_priv_t *cpmac_priv, struct sk_buff *skb, cpmac_err_t status);

/*--- indications and responses to commands from cpmac (line status, etc.) ---*/
cpmac_err_t cpmac_if_control_ind (cpmac_priv_t *cpmac_priv, cpmac_control_ind_t control, ...);

/*--- reset the tx queues ---*/
void cpmac_if_reset_tx_queues(cpmac_priv_t *cpmac_priv);

/*--- cpphy driver informs about end of teardown ---*/
cpmac_err_t cpmac_if_teardown_complete (cpmac_priv_t *cpmac_priv);

#ifdef CONFIG_AVM_PA
void cpmac_pa_dev_transmit(void *arg, struct sk_buff *skb);
#endif

#endif

