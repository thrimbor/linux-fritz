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

#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>
#include <asm/atomic.h>
#include <asm/mach_avm.h>
#if defined(CONFIG_AVM_POWER)
#include <linux/avm_power.h>
#endif /*--- #if defined(CONFIG_AVM_POWER) ---*/

#if !defined(CONFIG_NETCHIP_ADM69961)
#define CONFIG_NETCHIP_ADM69961
#endif
#include <linux/avm_event.h>
#include <linux/avm_cpmac.h>
#include <linux/adm_reg.h>
#include "cpmac_if.h"
#include "cpmac_const.h"
#include "cpmac_debug.h"
#include "cpphy_const.h"
#include "cpphy_types.h"
#include "cpphy_mdio.h"
#include "cpphy_mgmt.h"
#include "cpphy_if.h"
#include "cpphy_if_g.h"
#include "adm6996.h"
#include "tantos.h"
#include "cpphy_adm6996.h"
#include "cpphy_ar8216.h"
#include "cpmac_eth.h"
#include <linux/ar_reg.h>

#if (defined(CONFIG_MIPS_UR8) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7))
/*------------------------------------------------------------------------------------------*\
 * Register cache
\*------------------------------------------------------------------------------------------*/
#define ADM_TANTOS_GET(mdio, addr) tantos_read((mdio), (unsigned short *) &(addr))
#define ADM_TANTOS_PUT(mdio, addr, dat) tantos_write((mdio), (unsigned short *) &(addr), (dat))
#define ADM_TANTOS_SYNC(mdio, addr) tantos_write((mdio), (unsigned short *) &(addr), *((unsigned short *) &(addr)))

#define ADM_TANTOS_PHY_GET(mdio, phy, addr) tantos_phy_access((mdio), TANTOS_MIIAC_READ, (unsigned short *) &(addr), (phy), 0)
#define ADM_TANTOS_PHY_PUT(mdio, phy, addr, data) tantos_phy_access((mdio), TANTOS_MIIAC_WRITE, (unsigned short *) &(addr), (phy), (data))
#define ADM_TANTOS_PHY_SYNC(mdio, phy, addr) tantos_phy_access((mdio), TANTOS_MIIAC_SYNC, (unsigned short *) &(addr), (phy), 0)

#define PRINT_REGISTER(x) DEB_SUPPORT("Reg %#3x = %#4x\n", x, ADM_GET_EEPROM_REG(mdio, (x)));
#define PRINT_TANTOS_REGISTER(x) DEB_SUPPORT("Reg %#3x = %#4x\n", x, ADM_TANTOS_GET(mdio, (x)));
#define NUMBER_OF_SERIAL_REGISTERS (0x3c + 1)
/* alle 200ms Register aktualisieren */
#define MAX_SERIAL_REGISTER_AGE (HZ / 4)
struct adm_struct adm_serial_register;
static unsigned int *serial_register = (unsigned int *) &adm_serial_register;
#define ADM_SERIAL_SIZE 42
static unsigned short adm_serial_registers[ADM_SERIAL_SIZE] = {
/* 3 */  REG_ADM_LC_ID, REG_ADM_LC_STATUS0, REG_ADM_LC_STATUS2,
/* 6 */  REG_ADM_LC_RXPKTCNT0, REG_ADM_LC_RXPKTCNT1, REG_ADM_LC_RXPKTCNT2, REG_ADM_LC_RXPKTCNT3, REG_ADM_LC_RXPKTCNT4, REG_ADM_LC_RXPKTCNT5,
/* 6 */  REG_ADM_LC_RXPKTBYTECNT0, REG_ADM_LC_RXPKTBYTECNT1, REG_ADM_LC_RXPKTBYTECNT2, REG_ADM_LC_RXPKTBYTECNT3, REG_ADM_LC_RXPKTBYTECNT4, REG_ADM_LC_RXPKTBYTECNT5,
/* 6 */  REG_ADM_LC_TXPKTCNT0, REG_ADM_LC_TXPKTCNT1, REG_ADM_LC_TXPKTCNT2, REG_ADM_LC_TXPKTCNT3, REG_ADM_LC_TXPKTCNT4, REG_ADM_LC_TXPKTCNT5,
/* 6 */  REG_ADM_LC_TXPKTBYTECNT0, REG_ADM_LC_TXPKTBYTECNT1, REG_ADM_LC_TXPKTBYTECNT2, REG_ADM_LC_TXPKTBYTECNT3, REG_ADM_LC_TXPKTBYTECNT4, REG_ADM_LC_TXPKTBYTECNT5,
/* 6 */  REG_ADM_LC_COLLISIONCNT0, REG_ADM_LC_COLLISIONCNT1, REG_ADM_LC_COLLISIONCNT2, REG_ADM_LC_COLLISIONCNT3, REG_ADM_LC_COLLISIONCNT4, REG_ADM_LC_COLLISIONCNT5,
/* 6 */  REG_ADM_LC_ERRCNT0, REG_ADM_LC_ERRCNT1, REG_ADM_LC_ERRCNT2, REG_ADM_LC_ERRCNT3, REG_ADM_LC_ERRCNT4, REG_ADM_LC_ERRCNT5,
/* 3 */  REG_ADM_LC_OVERFLOWFLAG0, REG_ADM_LC_OVERFLOWFLAG2, REG_ADM_LC_OVERFLOWFLAG4
/* => 42 */
};

static const unsigned short adm_port_registers[] = {
    REG_ADM_LC_PORT0_CONF, REG_ADM_LC_PORT1_CONF, REG_ADM_LC_PORT2_CONF,
    REG_ADM_LC_PORT3_CONF, REG_ADM_LC_PORT4_CONF, REG_ADM_LC_PORT5_CONF
};

static const unsigned int adm_port_link_bits[] = {
    ADM_STATUS0_PORT0_LINKUP, ADM_STATUS0_PORT1_LINKUP, ADM_STATUS0_PORT2_LINKUP,
    ADM_STATUS0_PORT3_LINKUP, ADM_STATUS0_PORT4_LINKUP, 0, 0
};

static const unsigned int adm_vlan_port_map[] = {
    (1 << 0), (1 << 2), (1 << 4), (1 << 6), (1 << 7), (1 << 8)
};

static tantos_switch_struct tantos_switch_memory;
tantos_ports_struct *tantos_ports = (tantos_ports_struct *) &tantos_switch_memory;
static tantos_switch_struct *tantos_switch = &tantos_switch_memory;
static tantos_phy_struct tantos_phy_memory[7];
static tantos_phy_struct *tantos_phys = tantos_phy_memory;
static tantos_counter_struct tantos_counter_memory[7];


/*------------------------------------------------------------------------------------------*\
 * The configuration is defined in cpphy_switch.c                                           *
\*------------------------------------------------------------------------------------------*/
extern cpmac_switch_configuration_t switch_configuration[CPMAC_SWITCH_CONF_MAX_PRODUCTS][CPMAC_MODE_MAX_NO];

/*------------------------------------------------------------------------------------------*\
 * initialize GPIO pins.  output mode, low
\*------------------------------------------------------------------------------------------*/
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
static void adm_gpio_finit(void) {
    avm_gpio_ctrl(GPIO_BIT_FSER_CLK, FUNCTION_PIN, GPIO_OUTPUT_PIN);
    avm_gpio_ctrl(GPIO_BIT_MII_DIO, FUNCTION_PIN, GPIO_OUTPUT_PIN);
    avm_gpio_ctrl(GPIO_BIT_MII_DCLK, FUNCTION_PIN, GPIO_OUTPUT_PIN);
}
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int adm_read_32Bit_cached(cpphy_mdio_t *mdio, unsigned int addr) {
    unsigned int address, data;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_READ))
        return 0;

    /* Counter Register lesen */
    address = (addr - 0xa0) >> 1;
    data =   mdio_read(mdio, (unsigned short) (addr >> 5), addr & 0x1f) 
          + (mdio_read(mdio, (unsigned short) ((addr + 1)) >> 5, (addr + 1) & 0x1f) << 16);
    if(address >= sizeof(adm_serial_register) / sizeof(serial_register[0])) {
        panic("read: serial_register[%x] out of index (max 0x%x)\n", address, sizeof(adm_serial_register) / sizeof(serial_register[0]));
    }
    serial_register[address] = data;
    return data;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned short adm_read(cpphy_mdio_t *mdio, unsigned short addr) {
    if(!(mdio->Mode & CPPHY_SWITCH_MODE_READ)) {
        DEB_INFO("adm_read called without being allowed to read!\n");
        return 0;
    }

    return mdio_read(mdio, (unsigned short) (addr >> 5), addr & 0x1f);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int adm_read_32(cpphy_mdio_t *mdio, unsigned int addr) {
    if(!(mdio->Mode & CPPHY_SWITCH_MODE_READ)) {
        DEB_INFO("[%s] called without being allowed to read!\n", __FUNCTION__);
        return 0;
    }
    if(addr >= (1u << 16)) {
        DEB_WARN("[%s] No 32 bit reads possible\n", __FUNCTION__);
        return 0;
    }

    return mdio_read(mdio, (unsigned short) (addr >> 5), addr & 0x1f);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if 0
static unsigned int adm_safe_read(cpphy_mdio_t *mdio, unsigned short addr) {
    unsigned int count = 0, first, second;

    first = adm_read(mdio, addr);
    do {
        count++;
        second = adm_read(mdio, addr);
        if(first == second) {
            if(count > 1) {
                DEB_TEST("Value correct for register %#x on read number %u\n", addr, count);
            }
            return first;
        } else {
            DEB_TEST("Addr %#x: %#x != %#x\n", addr, first, second);
        }
        first = second;
    } while(count < 5);
    DEB_ERR("Could not read correct value for register %#x\n", addr);
    return 0;
}
#endif /*--- #if 0 ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned short tantos_read(cpphy_mdio_t *mdio, unsigned short *ptr) {
    unsigned short addr    = (unsigned short) (ptr - (unsigned short *) tantos_switch);
    unsigned short value = adm_read(mdio, addr);

    /*--- DEB_TEST("tantos_read: ptr = %p, tantos_switch = %p\n", ptr, tantos_switch); ---*/
    /*--- DEB_TEST("tantos_read: addr = %x, value = %x\n", addr, value); ---*/
    if(addr > sizeof(tantos_switch_memory) / sizeof(unsigned short))
        panic("[%s] addr %x out of range (max %x)\n", __FUNCTION__, addr, sizeof(tantos_switch_memory) / sizeof(unsigned short));
    ((unsigned short *) tantos_switch)[addr] = value;
    return value;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void adm_update_error_status(cpphy_mdio_t *mdio, unsigned char port) {
    if(!(mdio->Mode & CPPHY_SWITCH_MODE_READ))
        return;

    switch(port) {
        case 0:
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT0);
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT0);
            break;
        case 1:
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT1);
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT1);
            break;
        case 2:
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT2);
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT2);
            break;
        case 3:
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT3);
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT3);
            break;
        case 4:
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT4);
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT4);
            break;
        case 5:
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT5);
            ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT5);
            break;
        default:
            DEB_ERR("adm_update_error_status, unhandled port %u\n", port);
            break;
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void adm_update_cache(cpphy_mdio_t *mdio) {
    unsigned short i = 0;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_READ))
        return;

    for(i = 0; i < ADM_SERIAL_SIZE; i++) {
        ADM_GET_SERIAL_REG(mdio, adm_serial_registers[i]);
        PRINT_REGISTER(adm_serial_registers[i]);
    }
}


/*------------------------------------------------------------------------------------------*\
 * write register to ADM6996 eeprom registers
\*------------------------------------------------------------------------------------------*/
int adm_write(cpphy_mdio_t *mdio, unsigned short addr, unsigned short dat) {
    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE)) {
        return -1;
    }

    /*--- DEB_TEST("[%s] Write reg %#x: %#x -> %#x\n", __FUNCTION__, addr, cpphy_mdio_user_access_read(mdio, addr & 0x1f, addr >> 5), dat); ---*/
    mdio_write(mdio, (unsigned short) (addr >> 5), addr & 0x1f, dat);
    /*--- cpphy_mdio_user_access_write(mdio, addr & 0x1f, addr >> 5, dat); ---*/
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int adm_write_32(cpphy_mdio_t *mdio, unsigned int addr, unsigned int dat) {
    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE)) {
        DEB_INFO("[%s] called without being allowed to write!\n", __FUNCTION__);
        return 0;
    }
    if((addr >= (1u << 16)) || (dat >= (1u << 16))) {
        DEB_WARN("[%s] No 32 bit writes possible\n", __FUNCTION__);
        return 0;
    }

    return mdio_write(mdio, (unsigned short) (addr >> 5), addr & 0x1f, (unsigned short) dat);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned short tantos_write(cpphy_mdio_t *mdio, unsigned short *ptr, unsigned short value) {
    unsigned short addr = (unsigned short) (ptr - (unsigned short *) tantos_switch);

    if(addr > sizeof(tantos_switch_memory) / sizeof(unsigned short))
        panic("[%s] addr %x out of range (max %x)\n", __FUNCTION__, addr, sizeof(tantos_switch_memory) / sizeof(unsigned short));
    ((unsigned short *) tantos_switch)[addr] = value;
    return adm_write(mdio, addr, value);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void tantos_wait_for_access_complete(cpphy_mdio_t *mdio) {
    for( ;; ) {
        ADM_TANTOS_GET(mdio, tantos_switch->MIIAC);
        if(tantos_switch->MIIAC.MBUSY == 0)
            break;
        schedule();
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int tantos_phy_access(cpphy_mdio_t   *mdio,
                                      unsigned char   method,
                                      unsigned short *regadr,
                                      unsigned short  phyadr,
                                      unsigned short  data) {
    unsigned short value = 0;
    unsigned short addr  = (unsigned short) (regadr - (unsigned short *) tantos_phys);

    if(phyadr >= sizeof(tantos_phy_memory) / sizeof(tantos_phy_memory[0])) {
        panic("[%s] illegal phyadr %x (max 0x%x)\n", __FUNCTION__, phyadr, sizeof(tantos_phy_memory) / sizeof(tantos_phy_memory[0]));
    }
    if(addr >= sizeof(tantos_phy_memory[0]) / sizeof(unsigned short)) {
        panic("[%s] illegal addr %x (max 0x%x)\n", __FUNCTION__, addr, sizeof(tantos_phy_memory[0]) / sizeof(unsigned short));
    }

    if(method == TANTOS_MIIAC_SYNC) {
        data = ((unsigned short *) &(tantos_phy_memory[phyadr]))[addr];
        method = TANTOS_MIIAC_WRITE;
    }
    value = data;
    if(down_interruptible(&mdio->semaphore_switch) != 0) {
        DEB_WARN("[%s] Semaphore interrupted!\n", __FUNCTION__);
        return 0;
    }
    tantos_wait_for_access_complete(mdio);  /* Wait until UserAccess ready */
    tantos_switch->MIIAC.OP    = method;
    tantos_switch->MIIAC.PHYAD = phyadr;
    tantos_switch->MIIAC.REGAD = addr;
    if(method == TANTOS_MIIAC_WRITE) {
        ((unsigned short *) &(tantos_phy_memory[phyadr]))[addr] = data;
        ADM_TANTOS_PUT(mdio, tantos_switch->MIIWD, data);
    }
    ADM_TANTOS_SYNC(mdio, tantos_switch->MIIAC);
    tantos_wait_for_access_complete(mdio);  /* Wait for Read to complete */

    if(method == TANTOS_MIIAC_READ) {
        value = ADM_TANTOS_GET(mdio, tantos_switch->MIIRD);
        ((unsigned short *) &(tantos_phy_memory[phyadr]))[addr] = value;
    }
    /*--- DEB_TRC("[%s] %s phy %u, reg %#x = %#x\n", __FUNCTION__, ---*/
    /*--- (method == TANTOS_MIIAC_WRITE) ? "Writing" : "Reading", ---*/
    /*--- phyadr, ---*/
    /*--- addr, ---*/
    /*--- value); ---*/
    up(&mdio->semaphore_switch);
    return value;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void tantos_wait_for_counter_access_complete(cpphy_mdio_t *mdio) {
    for( ;; ) {
        ADM_TANTOS_GET(mdio, tantos_switch->RCC);
        if(tantos_switch->RCC.BAS == 0)
            break;
        schedule();
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int tantos_counter_access(cpphy_mdio_t   *mdio,
                                          unsigned short  method,
                                          unsigned short  phy,
                                          unsigned short  offset) {
    unsigned int value = 0;
    /*--- unsigned int addr  = (unsigned int) (regadr - (unsigned short *) tantos_phys); ---*/

    /*--- if(phyadr >= sizeof(tantos_phy_memory) / sizeof(tantos_phy_memory[0])) { ---*/
        /*--- panic("[%s] illegal phyadr %x (max 0x%x)\n", __FUNCTION__, phyadr, sizeof(tantos_phy_memory) / sizeof(tantos_phy_memory[0])); ---*/
    /*--- } ---*/
    /*--- if(addr >= sizeof(tantos_phy_memory[0]) / sizeof(unsigned short)) { ---*/
        /*--- panic("[%s] illegal addr %x (max 0x%x)\n", __FUNCTION__, addr, sizeof(tantos_phy_memory[0]) / sizeof(unsigned short)); ---*/
    /*--- } ---*/

    if(phy >= 7) {
        panic("[%s] Illegal PHY number %u\n", __FUNCTION__, phy);
    }
    if(offset > 27) {
        panic("[%s] Illegal offset %u\n", __FUNCTION__, offset);
    }

    switch(method) {
        case TANTOS_COUNTER_READ:
            if(down_interruptible(&mdio->semaphore_switch) != 0) {
                DEB_WARN("[%s] Semaphore interrupted!\n", __FUNCTION__);
                return 0;
            }
            tantos_wait_for_counter_access_complete(mdio);
            tantos_switch->RCC.CAC = method;
            tantos_switch->RCC.PORTC = phy;
            tantos_switch->RCC.OFFSET = offset;
            tantos_switch->RCC.BAS = 1;
            ADM_TANTOS_SYNC(mdio, tantos_switch->RCC);
            tantos_wait_for_counter_access_complete(mdio);
            value =    ADM_TANTOS_GET(mdio, tantos_switch->RCSL)
                    + (ADM_TANTOS_GET(mdio, tantos_switch->RCSH) << 16);
            up(&mdio->semaphore_switch);
            return value;
        case TANTOS_COUNTER_READ_ALL:
            if(down_interruptible(&mdio->semaphore_switch) != 0) {
                DEB_WARN("[%s] Semaphore interrupted!\n", __FUNCTION__);
                return 0;
            }
            tantos_wait_for_counter_access_complete(mdio);
            tantos_switch->RCC.CAC = method;
            tantos_switch->RCC.PORTC = phy;
            tantos_switch->RCC.OFFSET = 0;
            tantos_switch->RCC.BAS = 1;
            ADM_TANTOS_SYNC(mdio, tantos_switch->RCC);
            tantos_counter_memory[phy].RxGoodByte = 0;
            tantos_counter_memory[phy].RxBadByte = 0;
            tantos_counter_memory[phy].TxGoodByte = 0;
            for(offset = 0; offset <= 0x27; offset++) {
                tantos_wait_for_counter_access_complete(mdio);
                value =   ADM_TANTOS_GET(mdio, tantos_switch->RCSL)
                    + (ADM_TANTOS_GET(mdio, tantos_switch->RCSH) << 16);
                ((unsigned int *) &(tantos_counter_memory[phy]))[offset] = value;
            }
            tantos_counter_memory[phy].RxGoodByte =   ((unsigned long long) (((unsigned int *) &(tantos_counter_memory[phy]))[23]) << 32)
                + ((unsigned long long)  ((unsigned int *) &(tantos_counter_memory[phy]))[22]);
            tantos_counter_memory[phy].RxBadByte  =   ((unsigned long long) (((unsigned int *) &(tantos_counter_memory[phy]))[25]) << 32)
                + ((unsigned long long)  ((unsigned int *) &(tantos_counter_memory[phy]))[24]);
            tantos_counter_memory[phy].TxGoodByte =   ((unsigned long long) (((unsigned int *) &(tantos_counter_memory[phy]))[27]) << 32)
                + ((unsigned long long)  ((unsigned int *) &(tantos_counter_memory[phy]))[26]);
            up(&mdio->semaphore_switch);
            return 0;
        case TANTOS_COUNTER_RENEW_PORT_COUNTERS:
        case TANTOS_COUNTER_RENEW_ALL_COUNTERS:
            printk("[%s] Method %u not yet implemented\n", __FUNCTION__, method);
            return 0;
            break;
        default:
            printk("[%s] Illegal method %u\n", __FUNCTION__, method);
            break;
    }
    return value;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void adm_prepare_reboot(cpphy_mdio_t *mdio) {
    unsigned int i;

    adm_vlan_fbox_reset(mdio);
    adm_write(mdio, REG_ADM_LC_VLAN_MODE, ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_VLAN_MODE) & 0xffcf);
    mdio->switch_config.enable_vlan = 0;
    cpmac_global.power.roundrobin = 0;
    for(i = 0; i < 5; i++) {
        cpmac_global.power.setup.mode[i] = ADM_PHY_POWER_ON;
    }
    cpphy_mgmt_power(mdio);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if 0
#if defined(CONFIG_MIPS_OHIO)
static irqreturn_t adm_interrupt(int irq, void *p_param, struct pt_regs *regs) {
    cpphy_mdio_t *mdio = (cpphy_mdio_t *) p_param;
    unsigned short status = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_IS);

    DEB_TRC("adm_interrupt: %#x\n", status);
    /* Reset time stamp to ensure that the just changed value is requested */
    serial_register_timestamp[(REG_ADM_LC_STATUS0 - 0xa0) >> 1] = 0;

    return IRQ_HANDLED;
}
#endif /*--- #if defined(CONFIG_MIPS_OHIO) ---*/
#endif /*--- #if 0 ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void switch_dump_adm6996(cpphy_mdio_t *mdio __attribute__ ((unused))) {
    unsigned short i;

    DEB_SUPPORT("Registerdump ADM6996: All values are hexadecimal!\n");
    PRINT_REGISTER(0x000);
    DEB_SUPPORT("Port control:\n");
    PRINT_REGISTER(0x001);
    PRINT_REGISTER(0x003);
    PRINT_REGISTER(0x005);
    PRINT_REGISTER(0x007);
    PRINT_REGISTER(0x008);
    PRINT_REGISTER(0x009);
    DEB_SUPPORT("\n");
    for(i = 0x00a; i <= 0x027; i++) PRINT_REGISTER(i);
    DEB_SUPPORT("Additional VID info\n");
    for(i = 0x028; i <= 0x02c; i++) PRINT_REGISTER(i);
    DEB_SUPPORT("\n");
    for(i = 0x02d; i <= 0x03f; i++) PRINT_REGISTER(i);
    DEB_SUPPORT("VLAN Filters:\n");
    for(i = 0x040; i <= 0x05f; i+=2) {
        unsigned short vfl = ADM_GET_EEPROM_REG(mdio, (i));
        unsigned short vfh = ADM_GET_EEPROM_REG(mdio, (i + 1));
        DEB_SUPPORT("  Filter %u: VFL = %#4x    VFH = %#4x\n", (i - 0x40) / 2, vfl, vfh);
    }
    DEB_SUPPORT("\n");
    for(i = 0x060; i <= 0x09c; i++) PRINT_REGISTER(i);
    for(i = 0x0a0; i <= 0x0ad; i++) PRINT_REGISTER(i);
    for(i = 0x0b0; i <= 0x0b1; i++) PRINT_REGISTER(i);
    for(i = 0x0b4; i <= 0x0bb; i++) PRINT_REGISTER(i);
    for(i = 0x0be; i <= 0x0bf; i++) PRINT_REGISTER(i);
    for(i = 0x0c2; i <= 0x0c3; i++) PRINT_REGISTER(i);
    for(i = 0x0c6; i <= 0x0cd; i++) PRINT_REGISTER(i);
    for(i = 0x0d0; i <= 0x0d1; i++) PRINT_REGISTER(i);
    for(i = 0x0d4; i <= 0x0d5; i++) PRINT_REGISTER(i);
    for(i = 0x0d8; i <= 0x0df; i++) PRINT_REGISTER(i);
    for(i = 0x0e2; i <= 0x0e3; i++) PRINT_REGISTER(i);
    for(i = 0x0e6; i <= 0x0e7; i++) PRINT_REGISTER(i);
    for(i = 0x0ea; i <= 0x0f1; i++) PRINT_REGISTER(i);
    for(i = 0x0f4; i <= 0x0f5; i++) PRINT_REGISTER(i);
    for(i = 0x0f8; i <= 0x0f9; i++) PRINT_REGISTER(i);
    for(i = 0x0fc; i <= 0x103; i++) PRINT_REGISTER(i);
    for(i = 0x106; i <= 0x107; i++) PRINT_REGISTER(i);
    for(i = 0x10a; i <= 0x10b; i++) PRINT_REGISTER(i);
    for(i = 0x10e; i <= 0x119; i++) PRINT_REGISTER(i);
    for(i = 0x130; i <= 0x143; i++) PRINT_REGISTER(i);
    for(i = 0x200; i <= 0x208; i++) PRINT_REGISTER(i);
    for(i = 0x220; i <= 0x228; i++) PRINT_REGISTER(i);
    for(i = 0x240; i <= 0x248; i++) PRINT_REGISTER(i);
    for(i = 0x260; i <= 0x268; i++) PRINT_REGISTER(i);
    for(i = 0x280; i <= 0x288; i++) PRINT_REGISTER(i);

#if 0
    down_interruptible(&mdio->semaphore_switch);
    for(i = 0; i < 4; i++) { /* Search through all four possible FIDs */
        unsigned short ATS5;

        DEB_SUPPORT("MAC table entries for FID = %u\n", i);
        do { ; } while(ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ATS5) & ADM_ATS5_BUSY_MASK);

        DEB_SUPPORT("Initialize search\n"); /* FIXME */
        /* Initialize the MAC table access engine */
        adm_write(mdio, REG_ADM_LC_ATC5, TANTOS_MACTABLE_INITIAL_FIRST << ADM_ATC5_AC_CMD_SHIFT);
        do { ; } while(ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ATS5) & ADM_ATS5_BUSY_MASK);

        do {
            DEB_SUPPORT("Search initialize start\n"); /* FIXME */
            /* Enter search criteria and start the search, then wait until it finished */
            adm_write(mdio, REG_ADM_LC_ATC3, i << ADM_ATC3_PMAP_SHIFT);
            adm_write(mdio, REG_ADM_LC_ATC5, TANTOS_MACTABLE_SEARCH_FID << ADM_ATC5_AC_CMD_SHIFT);
            DEB_SUPPORT("Search start\n"); /* FIXME */
            do { 
                ATS5 = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ATS5);
            } while(   (ATS5 & ADM_ATS5_BUSY_MASK)
                    || (((ATS5 & ADM_ATS5_RSLT_MASK) >> ADM_ATS5_RSLT_SHIFT) == TANTOS_MACTABLE_RESULT_TEMPORARY_STATE));

            DEB_SUPPORT("Result (ATS5 = %#x)('ATS6' = %#x)\n", ATS5, ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ATS5 + 1)); /* FIXME */
            if(((ATS5 & ADM_ATS5_RSLT_MASK) >> ADM_ATS5_RSLT_SHIFT) == TANTOS_MACTABLE_RESULT_OK) {
                unsigned short ATS0, ATS1, ATS2, ATS3, ATS4;
                ATS4 = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ATS4);
                if(!(ATS4 & ADM_ATS4_OCP_MASK)) { /* Unoccupied entries are not interesting */
                    continue;
                }
                ATS0 = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ATS4);
                ATS1 = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ATS4);
                ATS2 = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ATS4);
                ATS3 = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ATS4);
                if(ATS4 & ADM_ATS4_INFOTS_MASK) {
                    DEB_SUPPORT("  static : FID %#x, ports %#2x, %04x%04x%04x, no details yet\n",
                            (ATS3 & ADM_ATS3_FIDS_MASK) >> ADM_ATS3_FIDS_SHIFT,
                            (ATS3 & ADM_ATS3_PMAPS_MASK) >> ADM_ATS3_PMAPS_SHIFT,
                            ATS2, ATS1, ATS0);
                } else {
                    DEB_SUPPORT("  dynamic: FID %#x, ports %#2x, %04x%04x%04x, age %u\n",
                            (ATS3 & ADM_ATS3_FIDS_MASK) >> ADM_ATS3_FIDS_SHIFT,
                            (ATS3 & ADM_ATS3_PMAPS_MASK) >> ADM_ATS3_PMAPS_SHIFT,
                            ATS2, ATS1, ATS0,
                            (ATS4 & ADM_ATS4_INFOTS_MASK) >> ADM_ATS4_INFOTS_SHIFT);
                }
            }
        } while(((ATS5 & ADM_ATS5_RSLT_MASK) >> ADM_ATS5_RSLT_SHIFT) == TANTOS_MACTABLE_RESULT_OK);
    }
    up(&mdio->semaphore_switch);
#endif /*--- #if 0 ---*/
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void switch_dump_tantos(cpphy_mdio_t *mdio __attribute__ ((unused))) {
    unsigned char phy;

    if(mdio->switch_dump_value == 1) { /* Register dump */
        unsigned short reg;
        DEB_SUPPORT("Registerdump Tantos: All values are hexadecimal!\n");
        for(reg = 0x00; reg < 0x20; reg++) {
            DEB_SUPPORT("Reg %3x = %4x   %3x = %4x   %3x = %4x   %3x = %4x   %3x = %4x   %3x = %4x   %3x = %4x\n",
                    0x00 + reg, ADM_GET_EEPROM_REG(mdio, 0x00 + reg),
                    0x20 + reg, ADM_GET_EEPROM_REG(mdio, 0x20 + reg),
                    0x40 + reg, ADM_GET_EEPROM_REG(mdio, 0x40 + reg),
                    0x60 + reg, ADM_GET_EEPROM_REG(mdio, 0x60 + reg),
                    0x80 + reg, ADM_GET_EEPROM_REG(mdio, 0x80 + reg),
                    0xa0 + reg, ADM_GET_EEPROM_REG(mdio, 0xa0 + reg),
                    0xc0 + reg, ADM_GET_EEPROM_REG(mdio, 0xc0 + reg));
        }
        for(reg = 0xe0; reg <= 0x122; reg++) PRINT_REGISTER(reg);
        DEB_SUPPORT("Additional register dump:\n");
        for(phy = 0; phy < 7; phy++) {
            DEB_SUPPORT("  For PHY %u\n", phy);
            for(reg = 0x00; reg < 0x18; reg++) {
                DEB_SUPPORT("    Reg %3x = %4x\n", reg, tantos_phy_access(mdio, TANTOS_MIIAC_READ, ((unsigned short *) tantos_phys) + reg, (phy), 0));
            }
            tantos_counter_access(mdio, TANTOS_COUNTER_READ_ALL, phy, 0);
            for(reg = 0x00; reg <= 0x27; reg++) {
                DEB_SUPPORT("    Counter %3x = %4x\n", reg, ((unsigned int *) &(tantos_counter_memory[phy]))[reg]);
            }
        }
    } else if(mdio->switch_dump_value == 2) { /* MAC Hash table */
        unsigned char FID;
        if(down_interruptible(&mdio->semaphore_switch) != 0) {
            DEB_WARN("[%s] Semaphore interrupted!\n", __FUNCTION__);
            return;
        }
        for(FID = 0; FID < 4; FID++) { /* Search through all four possible FIDs */
            DEB_SUPPORT("MAC table entries for FID = %u\n", FID);
            do { ADM_TANTOS_GET(mdio, tantos_switch->ATS5); } while(tantos_switch->ATS5.BUSY);

            tantos_switch->ATC5.AC_CMD = TANTOS_MACTABLE_INITIAL_FIRST;
            ADM_TANTOS_SYNC(mdio, tantos_switch->ATC5);
            do { ADM_TANTOS_GET(mdio, tantos_switch->ATS5); } while(tantos_switch->ATS5.BUSY);

            do {
                tantos_switch->ATC3.FID = FID;
                tantos_switch->ATC5.AC_CMD = TANTOS_MACTABLE_SEARCH_FID;
                ADM_TANTOS_SYNC(mdio, tantos_switch->ATC3);
                ADM_TANTOS_SYNC(mdio, tantos_switch->ATC5);
                do { ADM_TANTOS_GET(mdio, tantos_switch->ATS5); } while(   tantos_switch->ATS5.BUSY
                        || (tantos_switch->ATS5.RSLT == TANTOS_MACTABLE_RESULT_TEMPORARY_STATE));
                if(tantos_switch->ATS5.RSLT == TANTOS_MACTABLE_RESULT_OK) {
                    ADM_TANTOS_GET(mdio, tantos_switch->ATS4);
                    if(!tantos_switch->ATS4.OCP) { /* Unoccupied entries are not interesting */
                        continue;
                    }
                    ADM_TANTOS_GET(mdio, tantos_switch->ATS0);
                    ADM_TANTOS_GET(mdio, tantos_switch->ATS1);
                    ADM_TANTOS_GET(mdio, tantos_switch->ATS2);
                    ADM_TANTOS_GET(mdio, tantos_switch->ATS3);
                    if(tantos_switch->ATS4.INFOTS) {
                        DEB_SUPPORT("  static : FID %#x, ports %#2x, %04x%04x%04x, no details yet\n",
                                tantos_switch->ATS3.FIDS,
                                tantos_switch->ATS3.PMAPS,
                                tantos_switch->ATS2.ADDR47_32,
                                tantos_switch->ATS1.ADDR31_16,
                                tantos_switch->ATS0.ADDR15_0);
                    } else {
                        DEB_SUPPORT("  dynamic: FID %#x, ports %#2x, %04x%04x%04x, age %u\n",
                                tantos_switch->ATS3.FIDS,
                                tantos_switch->ATS3.PMAPS,
                                tantos_switch->ATS2.ADDR47_32,
                                tantos_switch->ATS1.ADDR31_16,
                                tantos_switch->ATS0.ADDR15_0,
                                tantos_switch->ATS4.ITATS);
                    }
                }
            } while(tantos_switch->ATS5.RSLT == TANTOS_MACTABLE_RESULT_OK);
        }
        up(&mdio->semaphore_switch);
    } else if(mdio->switch_dump_value == 3) { /* Counter dump */
        /* TODO */
    } else { /* Unknown, probably everything */
        unsigned char i;
        for(i = 1; i < 4; i++) {
            mdio->switch_dump_value = i;
            switch_dump_tantos(mdio);
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void adm_generic_init(cpphy_mdio_t *mdio) {
    unsigned int i;

    for(i = 0; i < NUMBER_OF_SERIAL_REGISTERS; i++) {
        serial_register[i] = 0;
    }
    init_MUTEX(&mdio->semaphore_switch);
    mdio->state = CPPHY_MDIO_ST_LINKED;
    if(mdio->f->check_link)
        mdio->f->check_link(mdio);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void adm_adm6996xc_init(cpphy_mdio_t *mdio) {
    DEB_INFOTRC("[%s]\n", __FUNCTION__);

#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    adm_gpio_finit();
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/

    adm_generic_init(mdio);

    /* TODO: Would we benefit? Or is this workqueue variant better anyway? */
#   if 0
#   if defined(CONFIG_MIPS_OHIO)
    /* Check, if we can use the interrupt */
    if(hwrev && !(strncmp("94", hwrev, 2)) && (strlen(hwrev) > 2)) {
        /* We need write access to configure the switch */
        if(mdio->Mode | CPPHY_SWITCH_MODE_WRITE) {
            if(request_irq(OHIOINT_EXT_1, adm_interrupt, SA_INTERRUPT | SA_SHIRQ, "ADM6996 Driver", mdio)) {
                DEB_ERR("adm_init, failed to register the irq %u\n", OHIOINT_EXT_1);
                return;
            }
            adm_write(mdio, REG_ADM_LC_IE, ADM_IE_PStatIE);
            adm_write(mdio, REG_ADM_LC_VLAN_MODE, ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_VLAN_MODE) | 0x0004);
            /* Make sure, that all values are zero by reading the register */
            ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_IS);
            mdio->cpmac_irq = 1;
        }
    }
#   endif /*--- #if defined(CONFIG_MIPS_OHIO) ---*/
#   endif /*--- #if 0 ---*/
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void adm_tantos_init(cpphy_mdio_t *mdio) {
    unsigned short i;

    DEB_INFOTRC("[%s]\n", __FUNCTION__);

    /* "Normal" switch with 16 bit access and hardware mdio */
#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
    adm_gpio_finit();
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/

    adm_generic_init(mdio);

    adm_write(mdio, 0xa1, 0x4);
    adm_write(mdio, 0xf5, 0x777);
    for(i = 0; i < 5; i++) {
        ADM_TANTOS_PHY_GET(mdio, i, tantos_phys->PHY_CR);
        tantos_phy_memory[i].PHY_CR.DISPMG = 1;
        ADM_TANTOS_PHY_SYNC(mdio, i, tantos_phys->PHY_CR);
    }
    ADM_TANTOS_GET(mdio, tantos_switch->MIICR);
    *((unsigned short *)&tantos_switch->MIICR) = 0x777; /* MIIs: 100MBits/FD/FC */
    ADM_TANTOS_SYNC(mdio, tantos_switch->MIICR);
    for(i = 5; i <= 6; i++) {
        if(!(mdio->switch_config.used_portset & (1 << i)))
            continue;
        ADM_TANTOS_GET(mdio, tantos_ports->port[i].PBC);
        tantos_ports->port[i].PBC.FLP = 1; /* Force link up for MIIs */
        ADM_TANTOS_SYNC(mdio, tantos_ports->port[i].PBC);
    }

}


#if 0
/*------------------------------------------------------------------------------------------*\
 * default VLAN setting
 * port 0~3 as untag port and PVID = 1
 * VLAN1: port 0~3 and port 5 (MII)
\*------------------------------------------------------------------------------------------*/
void adm_init(cpphy_mdio_t *mdio) {
    unsigned int i;
    char *hwrev, *cpu_nr;

    for(i = 0; i < NUMBER_OF_SERIAL_REGISTERS; i++) {
        serial_register[i] = 0;
        write_serial_register_timestamp(i, 0);
    }

    mdio->mode_pppoa = 0;

    hwrev = prom_getenv("HWRevision");
    if(hwrev && (   (!(strncmp( "94", hwrev, 2)) && (strlen(hwrev) < 4)) /* First 7170 revision */
                 || (!(strncmp( "95", hwrev, 2)) && (strlen(hwrev) < 4)) /* First 7140 revision */
                 || (!(strncmp("107", hwrev, 3)) && (strlen(hwrev) < 5)) /* First 7140 Annex A revision */
      )         ) {
        DEB_INFO("switch works in read only 32 bit mode\n");
        mdio->Mode &= ~CPPHY_SWITCH_MODE_WRITE;
#       if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
        adm_gpio_init();
#       endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
        if(mdio->f->check_link)
            mdio->f->check_link(mdio);
        return;
    }

    if(hwrev && (   !(strncmp("79", hwrev, 2)) /* 3070 with ADM6996L */
                 || !(strncmp("84", hwrev, 2)) /* 2070 with ADM6996L */
                )
      ) {
        DEB_INFO("switch works in read/write 32 bit mode\n");
#       if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
        adm_gpio_init();
#       endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
    } else {
        /* "Normal" switch with 16 bit access and hardware mdio */
#       if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO)
        adm_gpio_finit();
#       endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) ---*/
        mdio->Mode |= CPPHY_SWITCH_MODE_16BIT;
        DEB_INFO("switch is in 16bit Mode\n");
    }

    /* Check the kind of switch */
    init_MUTEX(&mdio->semaphore_switch);
    if(   (ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ID2) == 0x0007) 
       && (ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ID)  == 0x1022)) {
        DEB_INFOTRC("Identified ADM6996LC switch\n");
        mdio->cpmac_switch = AVM_CPMAC_SWITCH_ADM6996;
    } else if(   (ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ID2) == 0x0007) 
              && (ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_ID)  == 0x1023)) {
        DEB_INFOTRC("Identified ADM6996FC switch\n");
        mdio->cpmac_switch = AVM_CPMAC_SWITCH_ADM6996;
    } else if(   (   (ADM_TANTOS_GET(mdio, tantos_switch->CI0) == 0x1)  /* Tantos 1.2 */
                  || (ADM_TANTOS_GET(mdio, tantos_switch->CI0) == 0x2)) /* Tantos 1.3 */
              && (ADM_TANTOS_GET(mdio, tantos_switch->CI1) == 0x2599)) {
        DEB_INFOTRC("Identified TANTOS switch family\n");
        mdio->cpmac_switch = AVM_CPMAC_SWITCH_TANTOS;
        adm_write(mdio, 0xa1, 0x4);
        adm_write(mdio, 0xf5, 0x777);
        for(i = 0; i < 5; i++) {
            ADM_TANTOS_PHY_GET(mdio, i, tantos_phys->PHY_CR);
            tantos_phy_memory[i].PHY_CR.DISPMG = 1;
            ADM_TANTOS_PHY_SYNC(mdio, i, tantos_phys->PHY_CR);
        }
        ADM_TANTOS_GET(mdio, tantos_switch->MIICR);
        *((unsigned short *)&tantos_switch->MIICR) = 0x777; /* MIIs: 100MBits/FD/FC */
        ADM_TANTOS_SYNC(mdio, tantos_switch->MIICR);
        for(i = 5; i <= 6; i++) {
            if((i == 6) && !(mdio->is_vinax))
                continue;
            ADM_TANTOS_GET(mdio, tantos_ports->port[i].PBC);
            tantos_ports->port[i].PBC.FLP = 1; /* Force link up for MIIs */
            ADM_TANTOS_SYNC(mdio, tantos_ports->port[i].PBC);
        }

#if 0
        /* FIXME Test mirror option */
        ADM_TANTOS_GET(mdio, tantos_switch->CMH);
        DEB_TEST("[%s] before CMH = %#x\n", __FUNCTION__, *((unsigned int *) &tantos_switch->CMH));
        /*--- tantos_switch->CMH.CPN = 0x5; ---*/ /* CPU port is number 5     */
        tantos_switch->CMH.MSA = 0x1; /* Mirror short packets     */
        tantos_switch->CMH.MPA = 0x1; /* Mirror pause packets     */
        tantos_switch->CMH.SPN = 0x5; /* Mirror to the CPU port 5 */
        ADM_TANTOS_SYNC(mdio, tantos_switch->CMH);
        DEB_TEST("[%s] after CMH = %#x\n", __FUNCTION__, *((unsigned int *) &tantos_switch->CMH));
        ADM_TANTOS_GET(mdio, tantos_ports->port[6].PEC);
        tantos_ports->port[6].PEC.IPMO = 0x1; /* Enable receive mirroring */
        ADM_TANTOS_SYNC(mdio, tantos_ports->port[6].PEC);
#endif /*--- #if 0 ---*/
    } else {
        DEB_INFOTRC("Could not identify switch. Assuming ADM6996 family\n");
        mdio->cpmac_switch = AVM_CPMAC_SWITCH_ADM6996;
        mdio->switch_ports = 6;
    }

    /* TODO: Would we benefit? Or is this workqueue variant better anyway? */
#   if 0
#   if defined(CONFIG_MIPS_OHIO)
    /* Check, if we can use the interrupt */
    if(hwrev && !(strncmp("94", hwrev, 2)) && (strlen(hwrev) > 2)) {
        /* We need write access to configure the switch */
        if(mdio->Mode | CPPHY_SWITCH_MODE_WRITE) {
            if(request_irq(OHIOINT_EXT_1, adm_interrupt, SA_INTERRUPT | SA_SHIRQ, "ADM6996 Driver", mdio)) {
                DEB_ERR("adm_init, failed to register the irq %u\n", OHIOINT_EXT_1);
                return;
            }
            adm_write(mdio, REG_ADM_LC_IE, ADM_IE_PStatIE);
            adm_write(mdio, REG_ADM_LC_VLAN_MODE, ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_VLAN_MODE) | 0x0004);
            /* Make sure, that all values are zero by reading the register */
            ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_IS);
            mdio->cpmac_irq = 1;
        }
    }
#   endif /*--- #if defined(CONFIG_MIPS_OHIO) ---*/
#   endif /*--- #if 0 ---*/
    if(mdio->f->check_link)
        mdio->f->check_link(mdio);
}
#endif /*--- #if 0 ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int adm_6996xc_check_link(cpphy_mdio_t *mdio) {
    unsigned int adm_status = 0;
    unsigned int linked = 0;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_READ))
        return 0xff;

    adm_status = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_STATUS0);
    linked |= ((adm_status & ADM_LC_STATUS0_PORT0_LINKUP) ? 1 : 0) << 0;
    linked |= ((adm_status & ADM_LC_STATUS0_PORT1_LINKUP) ? 1 : 0) << 1;
    cpmac_global.event_data.port[0].link       = ((adm_status & ADM_LC_STATUS0_PORT0_LINKUP) ? 1 : 0);
    cpmac_global.event_data.port[0].speed100   = ((adm_status & ADM_LC_STATUS0_PORT0_SPEED) ? 1 : 0);
    cpmac_global.event_data.port[0].speed      =   (adm_status & ADM_LC_STATUS0_PORT0_SPEED)
                                                 ? avm_event_ethernet_speed_100M
                                                 : avm_event_ethernet_speed_10M;
    cpmac_global.event_data.port[0].fullduplex = ((adm_status & ADM_LC_STATUS0_PORT0_DUPLEX) ? 1 : 0);
    cpmac_global.event_data.port[1].link       = ((adm_status & ADM_LC_STATUS0_PORT1_LINKUP) ? 1 : 0);
    cpmac_global.event_data.port[1].speed100   = ((adm_status & ADM_LC_STATUS0_PORT1_SPEED) ? 1 : 0);
    cpmac_global.event_data.port[1].speed      =   (adm_status & ADM_LC_STATUS0_PORT1_SPEED)
                                                 ? avm_event_ethernet_speed_100M
                                                 : avm_event_ethernet_speed_10M;
    cpmac_global.event_data.port[1].fullduplex = ((adm_status & ADM_LC_STATUS0_PORT1_DUPLEX) ? 1 : 0);
    adm_status = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_STATUS1);
    linked |= ((adm_status & ADM_LC_STATUS1_PORT2_LINKUP) ? 1 : 0) << 2;
    linked |= ((adm_status & ADM_LC_STATUS1_PORT3_LINKUP) ? 1 : 0) << 3;
    linked |= ((adm_status & ADM_LC_STATUS1_PORT4_LINKUP) ? 1 : 0) << 4;
    cpmac_global.event_data.port[2].link       = ((adm_status & ADM_LC_STATUS1_PORT2_LINKUP) ? 1 : 0);
    cpmac_global.event_data.port[2].speed100   = ((adm_status & ADM_LC_STATUS1_PORT2_SPEED) ? 1 : 0);
    cpmac_global.event_data.port[2].speed      =   (adm_status & ADM_LC_STATUS1_PORT2_SPEED)
                                                 ? avm_event_ethernet_speed_100M
                                                 : avm_event_ethernet_speed_10M;
    cpmac_global.event_data.port[2].fullduplex = ((adm_status & ADM_LC_STATUS1_PORT2_DUPLEX) ? 1 : 0);
    cpmac_global.event_data.port[3].link       = ((adm_status & ADM_LC_STATUS1_PORT3_LINKUP) ? 1 : 0);
    cpmac_global.event_data.port[3].speed100   = ((adm_status & ADM_LC_STATUS1_PORT3_SPEED) ? 1 : 0);
    cpmac_global.event_data.port[3].speed      =   (adm_status & ADM_LC_STATUS1_PORT3_SPEED)
                                                 ? avm_event_ethernet_speed_100M
                                                 : avm_event_ethernet_speed_10M;
    cpmac_global.event_data.port[3].fullduplex = ((adm_status & ADM_LC_STATUS1_PORT3_DUPLEX) ? 1 : 0);
    cpmac_global.event_data.port[4].link       = ((adm_status & ADM_LC_STATUS1_PORT4_LINKUP) ? 1 : 0);
    cpmac_global.event_data.port[4].speed100   = ((adm_status & ADM_LC_STATUS1_PORT4_SPEED) ? 1 : 0);
    cpmac_global.event_data.port[4].speed      =   (adm_status & ADM_LC_STATUS1_PORT4_SPEED)
                                                 ? avm_event_ethernet_speed_100M
                                                 : avm_event_ethernet_speed_10M;
    cpmac_global.event_data.port[4].fullduplex = ((adm_status & ADM_LC_STATUS1_PORT4_DUPLEX) ? 1 : 0);
    adm_status = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_STATUS2);
    linked |= ((adm_status & ADM_LC_STATUS2_PORT5_LINKUP) ? 1 : 0) << 5;
    cpmac_global.event_data.port[5].link       = ((adm_status & ADM_LC_STATUS2_PORT5_LINKUP) ? 1 : 0);
    cpmac_global.event_data.port[5].speed100   = ((adm_status & ADM_LC_STATUS2_PORT5_SPEED) ? 1 : 0);
    cpmac_global.event_data.port[5].speed      =   (adm_status & ADM_LC_STATUS2_PORT5_SPEED)
                                                 ? avm_event_ethernet_speed_100M
                                                 : avm_event_ethernet_speed_10M;
    cpmac_global.event_data.port[5].fullduplex = ((adm_status & ADM_LC_STATUS2_PORT5_DUPLEX) ? 1 : 0);

    if(mdio->linked != linked) {
        mdio->linked = linked;
        cpmac_main_event_update();
    }

    mdio->linked = linked;

    return mdio->linked;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int adm_tantos_check_link(cpphy_mdio_t *mdio) {
    unsigned int port;
    unsigned int linked = 0;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_READ))
        return 0xff;

    for(port = 0; port < TANTOS_MAX_PORTS ; port++) {
        ADM_TANTOS_GET(mdio, tantos_ports->port[port].PS);
        if(tantos_ports->port[port].PS.link) {
            linked |= 1 << port;
        }
        if(port < 5) {
            cpmac_global.event_data.port[port].link = tantos_ports->port[port].PS.link;
            cpmac_global.event_data.port[port].speed100 = tantos_ports->port[port].PS.speed;
            cpmac_global.event_data.port[port].speed =   tantos_ports->port[port].PS.speed
                                                       ? avm_event_ethernet_speed_100M
                                                       : avm_event_ethernet_speed_10M;
            cpmac_global.event_data.port[port].fullduplex = tantos_ports->port[port].PS.duplex;
        }
    }
    /* All ports except the CPU port */
    linked &= 0x05f;

    if(mdio->linked != linked) {
        mdio->linked = linked;
        cpmac_main_event_update();
    }

    mdio->linked = linked;

    return mdio->linked;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void adm_switch_port_power_6996(cpphy_mdio_t *mdio, 
                                unsigned char port, 
                                unsigned char power_on) {
    unsigned short value;

    assert(port < AVM_CPMAC_MAX_PORTS); /* To ease Klocwork checking */

    value =   ADM_PHY_CONTROL_DPLX
            | ADM_PHY_CONTROL_ANEN
            | ADM_PHY_CONTROL_SPEED_LSB
            | (power_on ? ADM_PHY_CONTROL_RST : ADM_PHY_CONTROL_PDN);
    adm_write(mdio, REG_ADM_LC_PHY_CONTROL_PORT0 + (0x20 * port), value);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void adm_switch_port_power_tantos(cpphy_mdio_t *mdio, 
                                  unsigned char port, 
                                  unsigned char power_on) {
    unsigned short value;

    assert(port < AVM_CPMAC_MAX_PORTS); /* To ease Klocwork checking */

    value =   ADM_PHY_CONTROL_DPLX
            | ADM_PHY_CONTROL_ANEN
            | ADM_PHY_CONTROL_SPEED_LSB
            | (power_on ? ADM_PHY_CONTROL_RST : ADM_PHY_CONTROL_PDN);
    ADM_TANTOS_PHY_PUT(mdio, port, tantos_phys->CR, value);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if 0
static void adm_vlan_reset(cpphy_mdio_t *mdio) {
    unsigned int group, port;

    mdio->switch_config.enable_vlan = 0;
    for(group = 0; group < AVM_CPMAC_MAX_VLAN_GROUPS; group++) {
        mdio->switch_status.vlan[group].written = 0;
        mdio->switch_status.vlan[group].active = 0;
        mdio->switch_status.vlan[group].VFL.Bits.VV = 0;
        mdio->switch_status.vlan[group].VFL.Bits.VP = 0;
        mdio->switch_status.vlan[group].VFL.Bits.VID = group + 16;
        mdio->switch_status.vlan[group].VFH.Bits.FID = 0;
        mdio->switch_status.vlan[group].VFH.Bits.TM = 0;
        mdio->switch_status.vlan[group].VFH.Bits.M = (mdio->cpmac_switch == AVM_CPMAC_SWITCH_TANTOS) ? 0x7F : 0x3F;
    }
    for(port = 0; port <= 5; port++) {
        mdio->switch_status.port[port].written = 0;
        mdio->switch_status.port[port].vid = port + 16;
        mdio->switch_status.port[port].keep_tag_outgoing = 0;
        mdio->cpmac_priv->map_port_out[port] = port + 16;
    }
};
#endif /*--- #if 0 ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void adm_switch_wan_keep_tagging_adm6996(cpphy_mdio_t *mdio, unsigned char port) {
    unsigned short RegData;

    RegData = ADM_GET_EEPROM_REG(mdio, adm_port_registers[port]);
    if(mdio->switch_status.port[port].keep_tag_outgoing)
        RegData |= ADM_SW_PORT_TAG;
    else
        RegData &= ~ADM_SW_PORT_TAG;
    adm_write(mdio, adm_port_registers[port], RegData);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned long adm_set_wan_keep_tagging(cpphy_mdio_t *mdio) {
    struct avm_switch_struct *status = &(mdio->switch_status);
    unsigned char port;

    assert(mdio != NULL);
    port = mdio->switch_config.wanport;
    assert(port < 6); /* The ADM6996xC have a maximum of six ports */
    cpphy_mgmt_work_del(mdio, CPMAC_WORK_TOGGLE_VLAN);
    if(status->port[port].keep_tag_outgoing == mdio->wan_tagging_enable) {
        DEB_WARN("[%s] Already correct setting: %stagged\n", __FUNCTION__,
                 mdio->wan_tagging_enable ? "" : "un");
    } else {
        tasklet_disable(&mdio->cpmac_priv->tasklet);

        status->port[port].keep_tag_outgoing = mdio->wan_tagging_enable;
        assert(mdio->f->switch_wan_keep_tagging);
        mdio->f->switch_wan_keep_tagging(mdio, port);

        tasklet_enable(&mdio->cpmac_priv->tasklet);

        DEB_INFO("Configured wan keep_tagging to %d\n", mdio->wan_tagging_enable);
    }

#   if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8)
    cpphy_if_data_from_queues(mdio->cpmac_priv->cppi);
#   endif /*--- #if defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_UR8) ---*/
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned long adm_set_mode_pppoa_work(cpphy_mdio_t *mdio) {
    t_cpphy_switch_config *config = &(mdio->switch_config);
    unsigned char wanport = mdio->switch_predefined_configs[config->user_given_config.cpmac_mode].wanport;

    cpphy_mgmt_work_del(mdio, CPMAC_WORK_TOGGLE_PPPOA);
    tasklet_disable(&mdio->cpmac_priv->tasklet);

    DEB_INFOTRC("[%s] %sabling PPPoA\n", __FUNCTION__, mdio->mode_pppoa ? "En" : "Dis");
    
    /* Asserts, to make Klocwork happy */
    assert(config->cpu_port < AVM_CPMAC_MAX_PORTS);
    assert(wanport < AVM_CPMAC_MAX_PORTS);
    /* Change MAC learning off for PPPoA, on otherwise */
    ADM_TANTOS_GET(mdio, tantos_ports->port[config->cpu_port].PEC);
    tantos_ports->port[config->cpu_port].PEC.LD = mdio->mode_pppoa;
    ADM_TANTOS_SYNC(mdio, tantos_ports->port[config->cpu_port].PEC);
    ADM_TANTOS_GET(mdio, tantos_ports->port[wanport].PEC);
    tantos_ports->port[wanport].PEC.LD = mdio->mode_pppoa;
    ADM_TANTOS_SYNC(mdio, tantos_ports->port[wanport].PEC);

    /* Switch flow control off for PPPoA, on otherwise */
    ADM_TANTOS_GET(mdio, tantos_ports->port[wanport].PS);
    tantos_ports->port[wanport].PS.flowcontrol = mdio->mode_pppoa ? 0 : 1;
    ADM_TANTOS_SYNC(mdio, tantos_ports->port[wanport].PS);

    tasklet_enable(&mdio->cpmac_priv->tasklet);

    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t adm_set_mode_pppoa(cpphy_mdio_t *mdio, unsigned int enable_pppoa) {
    if(mdio->switch_predefined_configs[mdio->switch_config.user_given_config.cpmac_mode].wanport > AVM_CPMAC_MAX_PORTS) {
        DEB_ERR("[%s] There seems to be no WAN port for PPPoA!?\n", __FUNCTION__);
        return CPMAC_ERR_ILL_CONTROL;
    }

    mdio->mode_pppoa = enable_pppoa ? 1 : 0;
    DEB_INFOTRC("[%s] Enqueue %sabling PPPoA\n", __FUNCTION__, mdio->mode_pppoa ? "en" : "dis");
    cpphy_mgmt_work_add(mdio, CPMAC_WORK_TOGGLE_PPPOA, adm_set_mode_pppoa_work, 0);

    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
 * Configure VLAN on the switch                                                             *
\*------------------------------------------------------------------------------------------*/
#if 0
static void adm_vlan_config(cpphy_mdio_t *mdio) {
    unsigned int vlan_mapping, group, port, RegData;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE))
        return; /*--- CPMAC_ERR_NO_VLAN_POSSIBLE; ---*/

    if(mdio->cpmac_switch == AVM_CPMAC_SWITCH_TANTOS) {
        for(port = 0; port < mdio->switch_ports; port++) {
            ADM_TANTOS_GET(mdio, tantos_ports->port[port].PBVM);
            tantos_ports->port[port].PBVM.TBVE = mdio->cpmac_priv->enable_vlan;
            ADM_TANTOS_SYNC(mdio, tantos_ports->port[port].PBVM);
        }

        for(port = 0; port < mdio->switch_ports; port++) {
            DEB_INFOTRC("Configure port %u with VLAN group %#x %stagged %s\n",
                        port,
                        mdio->switch_status.port[port].vid,
                        mdio->switch_status.port[port].keep_tag_outgoing ? "" : "un",
                        mdio->switch_status.port[port].written ? "" : "(new)");

            if(mdio->switch_status.port[port].written)
                continue;
            mdio->switch_status.port[port].written = 1;
            /* Set the default VID */
            ADM_TANTOS_GET(mdio, tantos_ports->port[port].PDVID);
            tantos_ports->port[port].PDVID.PVID = mdio->switch_status.port[port].vid;
            ADM_TANTOS_SYNC(mdio, tantos_ports->port[port].PDVID);
        }

        for(group = 0; group < AVM_CPMAC_MAX_VLAN_GROUPS; group++) {
            DEB_INFOTRC("Configure VLAN group %#x (%s): FID %#x VID %#x target 0x%x tagged 0x%x %s\n",
                        group,
                        mdio->switch_status.vlan[group].VFL.Bits.VV ? "  active" : "inactive",
                        mdio->switch_status.vlan[group].VFH.Bits.FID,
                        mdio->switch_status.vlan[group].VFL.Bits.VID,
                        mdio->switch_status.vlan[group].VFH.Bits.M,
                        mdio->switch_status.vlan[group].VFH.Bits.TM,
                        mdio->switch_status.vlan[group].written ? "" : "(new)");

            if(mdio->switch_status.vlan[group].written)
                continue;
            mdio->switch_status.vlan[group].written = 1;

            if(group < 8) {
                ADM_TANTOS_PUT(mdio, tantos_switch->VF0[group].VFH, mdio->switch_status.vlan[group].VFH.Register);
                ADM_TANTOS_PUT(mdio, tantos_switch->VF0[group].VFL, mdio->switch_status.vlan[group].VFL.Register);
            } else {
                ADM_TANTOS_PUT(mdio, tantos_switch->VF8[group - 8].VFH, mdio->switch_status.vlan[group].VFH.Register);
                ADM_TANTOS_PUT(mdio, tantos_switch->VF8[group - 8].VFL, mdio->switch_status.vlan[group].VFL.Register);
            }
        }
    } else {
        /*--- Select two bits MAC Clone, when cloning (for VLANs) is enabled ---*/
        RegData = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_MISC_CONF);
        RegData |= ADM_CONF_MISC_MCEB;
        adm_write(mdio, REG_ADM_LC_MISC_CONF, RegData);
        if(mdio->cpmac_priv->enable_vlan == 1) {
            /* MAC clone, 802.1q based VLAN */
            adm_write(mdio, REG_ADM_LC_VLAN_MODE, ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_VLAN_MODE) | 0x0030);
            /* MAC clone, 802.1q based VLAN, address aging timer 1 second */
            /*--- adm_write(mdio, REG_ADM_LC_VLAN_MODE, 0xff33); ---*/
        } else {
            /* MAC clone, Port based by-pass mode */
            adm_write(mdio, REG_ADM_LC_VLAN_MODE, ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_VLAN_MODE) & 0xffcf);
        }

        for(port = 0; port < mdio->switch_ports; port++) {
            assert(port < 6); /* The ADM6996xC have a maximum of six ports */
            DEB_INFOTRC("Configure port 0x%x with VID 0x%x %stagged %s\n",
                        port,
                        mdio->switch_status.port[port].vid,
                        mdio->switch_status.port[port].keep_tag_outgoing ? "" : "un",
                        mdio->switch_status.port[port].written ? "" : "(new)");

            if(mdio->switch_status.port[port].written)
                continue;
            mdio->switch_status.port[port].written = 1;
            RegData = ADM_GET_EEPROM_REG(mdio, adm_port_registers[port]);
            RegData &= ~(ADM_CONF_OUTPUT_PKT_TAG | ADM_CONF_PORT_VLAN_ID);
            if(mdio->switch_status.port[port].keep_tag_outgoing)
                RegData |= ADM_SW_PORT_TAG;
            RegData |= (mdio->switch_status.port[port].vid & 0xf) << ADM_SW_PORT_PVID_SHIFT;
            adm_write(mdio, adm_port_registers[port], RegData);
            if(port == 5) {
                RegData  = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_RACP) & 0xff00;
                RegData |= (mdio->switch_status.port[port].vid & 0xff0) >> 4;
                adm_write(mdio, REG_ADM_LC_RACP, RegData);
            } else if(port == 4) {
                RegData  = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_PORT34_PVID) & 0x00ff;
                RegData |= (mdio->switch_status.port[port].vid & 0xff0) << 4;
                adm_write(mdio, REG_ADM_LC_PORT34_PVID, RegData);
            } else {
                RegData  = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_PORT0_PVID + port) & 0xff00;
                RegData |= (mdio->switch_status.port[port].vid & 0xff0) >> 4;
                adm_write(mdio, REG_ADM_LC_PORT0_PVID + port, RegData);
            }
        }

        for(group = 0; group < AVM_CPMAC_MAX_VLAN_GROUPS; group++) {
            unsigned short target_register;
            DEB_INFOTRC("Configure VLAN group %#x (%s): VID 0x%x target 0x%x %s\n",
                        group,
                        mdio->switch_status.vlan[group].active ? "  active" : "inactive",
                        mdio->switch_status.vlan[group].vid,
                        mdio->switch_status.vlan[group].VFH.Bits.M,
                        mdio->switch_status.vlan[group].written ? "" : "(new)");

            if(mdio->switch_status.vlan[group].written)
                continue;
            mdio->switch_status.vlan[group].written = 1;

            vlan_mapping  = mdio->switch_status.vlan[group].VFH.Bits.M;
            vlan_mapping |= ADM_GET_TAG_GROUP(mdio->switch_status.vlan[group].vid) << 12;
            target_register = REG_ADM_LC_VLAN_FILTER0_LOW + 2 * group;
            adm_write(mdio, target_register, vlan_mapping);
            DEB_INFOTRC("[%s] Writing register %#x = %#x\n", __FUNCTION__, target_register, vlan_mapping);
            vlan_mapping = (mdio->switch_status.vlan[group].active << 15) | mdio->switch_status.vlan[group].vid;
            target_register = REG_ADM_LC_VLAN_FILTER0_HIGH + 2 * group;
            adm_write(mdio, target_register, vlan_mapping);
            DEB_INFOTRC("[%s] Writing register %#x = %#x\n", __FUNCTION__, target_register, vlan_mapping);
        }
    }
}
#endif /*--- #if 0 ---*/


/*------------------------------------------------------------------------------------------*\
 * Reset the VLAN settings of the switch                                                    *
\*------------------------------------------------------------------------------------------*/
void adm_vlan_fbox_reset(cpphy_mdio_t *mdio __attribute__ ((unused))) {
    /* switch off VLAN */
#   if 0
    adm_vlan_reset(mdio);
    adm_vlan_config(mdio);
#   endif /*--- #if 0 ---*/
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/* Find unused VLAN ID group */
#define find_unused_vlan_old() \
    for(vid_group = 0; vid_group < AVM_CPMAC_MAX_VLAN_GROUPS; vid_group++) { \
        if(!(used_vid_mask & (1 << vid_group))) { \
            break; \
        } \
    } \
    if(vid_group == AVM_CPMAC_MAX_VLAN_GROUPS) { \
        DEB_ERR("Error! I need more VLAN groups!\n"); \
        return CPMAC_ERR_NO_VLAN_POSSIBLE; \
    } \
    used_vid_mask |= 1 << vid_group;

#define adm_get_new_vid() \
    /* Do not use VIDs < 2; ensure it by adding 4, that does not change the lower two bits */ \
    new_vid = (((vid_group << ADM_6996_TAGSHIFT) + bit_combination) & 0xfff); \
    for( ; ; ) { \
        new_vid += (1 << 2); \
        new_vid &= 0xfff; /* To make Klockworks happy */ \
        if(mdio->cpmac_priv->map_in[new_vid].used) \
            continue; \
        if(((new_vid >> ADM_6996_TAGSHIFT) & 0xf) != vid_group) { \
            DEB_ERR("Internal VLAN configuration error! Too many VIDs for this device %u?\n", device); \
        } \
        DEB_TRC("[%s] new_vid = %#x\n", __FUNCTION__, new_vid); \
        break; \
    }


/*------------------------------------------------------------------------------------------*\
 * Try to find a bit shift that allows to minimize the numbers of VID groups to use for the *
 * WAN device. This means finding four consecutive bits that match as good as possible      *
 * between as many VID as possible. Prefer higher bitshifts, to prevent problems between    *
 * vid_group and the lower two bits.                                                        *
\*------------------------------------------------------------------------------------------*/
void adm_set_good_vid_bitshift(cpphy_mdio_t *mdio, struct avm_cpmac_config_struct *config) {
    unsigned int needed_groups, optimal_needed_groups = CPMAC_MAX_VLAN_GROUPS;
    unsigned short i, j, bits_A, bits_B, RegData;
    unsigned char bitshift, optimal_bitshift = 8;

    mdio->switch_config.adm_bitshift = 8;
    for(bitshift = 0; bitshift <= 8; bitshift++) {
        needed_groups = 0;
        for(i = 0; i < config->wan_vlan_number; i++) {
            bits_A = config->wan_vlan_id[i] & (0xf << bitshift);
            for(j = 0; j < config->wan_vlan_number; j++) {
                bits_B = config->wan_vlan_id[j] & (0xf << bitshift);

                if(bits_A != bits_B) {
                    needed_groups++;
                }
            }
        }
        needed_groups /= 2;
        needed_groups++;
        DEB_TRC("[%s] Bitshift %u -> %u needed groups\n", __FUNCTION__, bitshift, needed_groups);
        if(needed_groups <= optimal_needed_groups) {
            optimal_needed_groups = needed_groups;
            optimal_bitshift = bitshift;
        }
    }
    mdio->switch_config.adm_bitshift = optimal_bitshift;
    DEB_INFOTRC("[%s] Setting bitshift to %u (bitmask %#x)\n", 
                __FUNCTION__, optimal_bitshift, 0xf << optimal_bitshift);
    RegData = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_RACP);
    RegData &= ~ADM_CONF_RACP_TAG_SHIFT;
    RegData |= (optimal_bitshift << ADM_CONF_RACP_TAG_SHIFT_SHIFT);
    adm_write(mdio, REG_ADM_LC_RACP, RegData);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned char cpphy_adm_get_bitcombination(cpphy_mdio_t *mdio, unsigned char device) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned char bitcombination;

    DEB_INFOTRC("[%s] Checking device bit combinations: %#x\n", __FUNCTION__,
                switch_config->device[device].adm_used_bit_combinations);
    for(bitcombination = 0; bitcombination < 4; bitcombination++) {
        if(switch_config->device[device].adm_used_bit_combinations & (1 << bitcombination)) {
            goto use_bitcombination;
        }
    }
    /* Need to find a free bit combination */
    DEB_INFOTRC("[%s] Checking for free bit combination: %#x\n", __FUNCTION__,
                switch_config->adm_used_bit_combinations);
    for(bitcombination = 0; bitcombination < 4; bitcombination++) {
        if(!(switch_config->adm_used_bit_combinations & (1 << bitcombination))) {
            goto use_bitcombination;
        }
    }
    DEB_ERR("[%s] Error! There is no usable bit combination for this device! Using 0 anyway.\n", __FUNCTION__);
    bitcombination = 0;

use_bitcombination:
    switch_config->device[device].adm_used_bit_combinations |= (1 << bitcombination);
    switch_config->adm_used_bit_combinations                |= (1 << bitcombination);
    DEB_INFOTRC("[%s] Returning bit combination %u\n", __FUNCTION__, bitcombination);
    return bitcombination;
}


/*------------------------------------------------------------------------------------------*\
 * Configure VLAN group for ADM6996                                                         *
\*------------------------------------------------------------------------------------------*/
void adm_adm6996_config_vlan_group(cpphy_mdio_t   *mdio,
                                   unsigned short  vid,
                                   unsigned short  portset,
                                   unsigned short  tagged_portset __attribute__ ((unused)),
                                   unsigned char   port,
                                   unsigned char   FID __attribute__ ((unused)),
                                   unsigned char   use_as_default) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vlan_mapping, target_register, vid_mask, vid_compare, pseudovid, group;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE))
        return; /*--- CPMAC_ERR_NO_VLAN_POSSIBLE; ---*/

    /* Because the ADM6996 family uses four consecutive bits of  *\
     * the VID as vidgroup to define VLAN behaviour, assignment  *
     * of a single VID is not possible and affects a range of    *
     * VID. Therefor the mapping should reflect all affected     *
     * VID. And it follows, that just one VID needs to be in     *
    \* the vidgroup.                                             */
    group = (vid >> switch_config->adm_bitshift) & 0xf;
    /*--- switch_config->map_vid_to_port[vid] = port; ---*/
    /*--- DEB_INFOTRC("[%s] Mapping VID %#x (%u) to port %u\n", __FUNCTION__, ---*/ 
                /*--- vid, vid, switch_config->map_vid_to_port[vid]); ---*/
    switch_config->vidgroup[group].used = 1;
    switch_config->vidgroup[group].vid = vid;
    switch_config->vidgroup[group].portset = portset;
    switch_config->vidgroup[group].port_in = port;
    switch_config->map_vid_to_group[vid] = group;
    DEB_INFOTRC("[%s] Mapping VID %#x (%u) to port %u\n", __FUNCTION__, vid, vid, port);

    if(use_as_default) {
        DEB_INFOTRC("[%s] Mapping all similar VID->port mappings with group %#x and bitshift %u\n", __FUNCTION__,
                    group, switch_config->adm_bitshift);
        vid_mask = 0xf << switch_config->adm_bitshift;
        vid_compare = group << switch_config->adm_bitshift;
        for(pseudovid = vid_compare; pseudovid < 4096; pseudovid++) {
            if((pseudovid & vid_mask) == vid_compare) {
                /*--- switch_config->map_vid_to_port[pseudovid] = port; ---*/
                switch_config->map_vid_to_group[pseudovid] = group;
            }
        }
    }

    DEB_INFOTRC("[%s] Assign VLAN group %#x: VID %#x (%u) target 0x%x\n", __FUNCTION__,
                group, vid, vid, portset);

    vlan_mapping  = portset;
    vlan_mapping |= (1 << switch_config->cpu_port) << 6;
    vlan_mapping |= group << 12;
    target_register = REG_ADM_LC_VLAN_FILTER0_LOW + 2 * group;
    adm_write(mdio, target_register, vlan_mapping);
    DEB_INFOTRC("[%s] Writing register %#x = %#x\n", __FUNCTION__, target_register, vlan_mapping);
    vlan_mapping =   (1 << 15) /* active bit */
                   | vid;
    target_register = REG_ADM_LC_VLAN_FILTER0_HIGH + 2 * group;
    adm_write(mdio, target_register, vlan_mapping);
    DEB_INFOTRC("[%s] Writing register %#x = %#x\n", __FUNCTION__, target_register, vlan_mapping);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned short cpphy_adm6996_get_vid_group(cpphy_mdio_t *mdio, unsigned char bitcombination) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid_group;

    bitcombination >>= switch_config->adm_bitshift;
    for(vid_group = 0; vid_group < switch_config->max_vid_groups; vid_group++) {
        if(   (!switch_config->vidgroup[vid_group].used)
           && (   (switch_config->adm_bitshift >= 2)
               || ((vid_group & (0x3 >> switch_config->adm_bitshift)) == bitcombination))) {
            break;
        }
    }
    if(vid_group >= switch_config->max_vid_groups) {
        DEB_ERR("[%s] Error! No free VLAN groups! Using 0 anyway!\n", __FUNCTION__);
        vid_group = 0;
    }
    switch_config->vidgroup[vid_group].used = 1;
    return vid_group;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned short cpphy_adm_get_new_vid_for_group(cpphy_mdio_t   *mdio,
                                                      unsigned char   device,
                                                      unsigned short  vid_group) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid, vid_mask, vid_compare;
    unsigned char bitcombination;

    bitcombination = cpphy_adm_get_bitcombination(mdio, device);
    vid_mask = 0xf << switch_config->adm_bitshift;
    vid_compare = vid_group << switch_config->adm_bitshift;

    for(vid = 4; vid < 4096; vid++) {
        if((   (vid & vid_mask) == vid_compare)
                && ((vid & 0x3) == bitcombination)
                && !cpphy_switch_check_assigned_vid(mdio, vid)) {
            switch_config->assigned_vids[switch_config->assigned_vids_count++] = vid;
            return vid;
        }
    }
    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned short cpphy_adm_get_new_vid(cpphy_mdio_t  *mdio,
                                     unsigned char  device) {
    unsigned short vid_group;
    unsigned char bitcombination;

    bitcombination = cpphy_adm_get_bitcombination(mdio, device);
    vid_group = cpphy_adm6996_get_vid_group(mdio, bitcombination);
    return cpphy_adm_get_new_vid_for_group(mdio, device, vid_group);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_adm_set_default_vid(cpphy_mdio_t   *mdio, 
                               unsigned char   device, 
                               unsigned short  portset) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid, vid_group = 0;
    unsigned char port, first_port;

    first_port = 1;
    for(port = 0; port < AVM_CPMAC_MAX_PORTS; port++) {
        if((portset & (1 << port)) == 0) {
            continue; /* Port is not in portset */
        }
        switch_config->port[port].port_vlan_mask = portset;
        if(port == switch_config->cpu_port)  {
            continue; /* No tag for the CPU port needed */
        }
        if(first_port) {
            vid = cpphy_adm_get_new_vid(mdio, device);
            vid_group = (vid >> switch_config->adm_bitshift) & 0xf;
        } else {
            vid = cpphy_adm_get_new_vid_for_group(mdio, device, vid_group);
        }
        if(vid == 0) {
            DEB_ERR("[%s] Error! No unassigned VID (for default) available!\n", __FUNCTION__);
            return;
        }
        assert(mdio->f->config_vlan_group);
        DEB_INFOTRC("[%s] Setting default VID %#x for '%s' (portset %#x)\n",
                    __FUNCTION__, vid, mdio->switch_config.device[device].name, portset);
        mdio->f->config_vlan_group(mdio, vid, portset, 0, port, vid & 0x3, first_port);
        if(first_port) {
            switch_config->port[switch_config->cpu_port].vid = vid;
            switch_config->device[device].vid = vid;
        }
        switch_config->port[port].vid = vid;
        switch_config->port[port].keep_tag_outgoing = 0;
        /*--- switch_config->map_vid_to_port[vid] = port; ---*/
        /*--- DEB_INFOTRC("[%s] Mapping VID %#x (%u) to port %u\n", __FUNCTION__, ---*/ 
                    /*--- vid, vid, switch_config->map_vid_to_port[vid]); ---*/
        first_port = 0;
    }
    switch_config->port[switch_config->cpu_port].keep_tag_outgoing = 1;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t cpphy_adm_set_port_vid_mapping(cpphy_mdio_t   *mdio, 
                                           unsigned char   device,
                                           unsigned char   port) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid;

    vid = cpphy_adm_get_new_vid(mdio, device);
    if(vid == 0) {
        DEB_ERR("[%s] Error! No unassigned VID (for portout) available!\n", __FUNCTION__);
        return CPMAC_ERR_EXCEEDS_LIMIT;
    }
    assert(mdio->f->config_vlan_group);
    mdio->f->config_vlan_group(mdio, 
                               vid,
                               (1 << switch_config->cpu_port) | (1 << port),
                               0,
                               port,
                               vid & 0x3,
                               0);
    /*--- switch_config->map_vid_to_port[vid] = port; ---*/
    /*--- switch_config->map_port_out[port] = vid; ---*/
    /*--- DEB_INFOTRC("[%s] Mapping VID %#x (%u) to port %u (for outgoing)\n", __FUNCTION__, ---*/ 
                /*--- vid, vid, switch_config->map_vid_to_port[vid]); ---*/
    return CPMAC_ERR_NOERR;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void adm_vlan_clear(cpphy_mdio_t *mdio) {
    unsigned short group;

    mdio->switch_config.enable_vlan = 0;
    for(group = 0; group < mdio->switch_config.max_vid_groups; group++) {
        unsigned short vlan_mapping, target_register;
        vlan_mapping  = 0x3f;
        vlan_mapping |= group << 12;
        target_register = REG_ADM_LC_VLAN_FILTER0_LOW + 2 * group;
        adm_write(mdio, target_register, vlan_mapping);
        DEB_INFOTRC("[%s] Writing register %#x = %#x\n", __FUNCTION__, target_register, vlan_mapping);
        vlan_mapping = (group + 16);
        target_register = REG_ADM_LC_VLAN_FILTER0_HIGH + 2 * group;
        adm_write(mdio, target_register, vlan_mapping);
        DEB_INFOTRC("[%s] Writing register %#x = %#x\n", __FUNCTION__, target_register, vlan_mapping);
    }
};


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void adm_vlan_reset(cpphy_mdio_t *mdio) {
    adm_vlan_clear(mdio);
    adm_write(mdio, REG_ADM_LC_VLAN_FILTER0_HIGH, (1 << 15) | 16);
    DEB_INFOTRC("[%s] Writing register %#x = %#x\n", __FUNCTION__, REG_ADM_LC_VLAN_FILTER0_HIGH, (1 << 15) | 16);
};


/*------------------------------------------------------------------------------------------*\
 * Configure ports on the ADM6996 switch                                                    *
\*------------------------------------------------------------------------------------------*/
void adm_config_ports(cpphy_mdio_t *mdio) {
    t_cpphy_switch_config *switch_config = &mdio->switch_config;
    unsigned char port;
    unsigned short RegData;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE))
        return; /*--- CPMAC_ERR_NO_VLAN_POSSIBLE; ---*/

    /*--- Select two bits MAC Clone, when cloning (for VLANs) is enabled ---*/
    RegData = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_MISC_CONF);
    RegData |= ADM_CONF_MISC_MCEB;
    adm_write(mdio, REG_ADM_LC_MISC_CONF, RegData);
    /* MAC clone, 802.1q based VLAN */
    adm_write(mdio, REG_ADM_LC_VLAN_MODE, ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_VLAN_MODE) | 0x0030);

    for(port = 0; port < switch_config->ports; port++) {
        if(!(switch_config->used_portset & (1 << port)))
            continue;
        assert(port < 6); /* The ADM6996xC have a maximum of six ports */
        DEB_INFOTRC("[%s] Configure port 0x%x with VID 0x%x %stagged\n",
                    __FUNCTION__,
                    port,
                    switch_config->port[port].vid,
                    switch_config->port[port].keep_tag_outgoing ? "" : "un");
        RegData = ADM_GET_EEPROM_REG(mdio, adm_port_registers[port]);
        RegData &= ~(ADM_CONF_OUTPUT_PKT_TAG | ADM_CONF_PORT_VLAN_ID);
        if(switch_config->port[port].keep_tag_outgoing)
            RegData |= ADM_SW_PORT_TAG;
        RegData |= (switch_config->port[port].vid & 0xf) << ADM_SW_PORT_PVID_SHIFT;
        adm_write(mdio, adm_port_registers[port], RegData);
        if(port == 5) {
            RegData  = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_RACP) & 0xff00;
            RegData |= (switch_config->port[port].vid & 0xff0) >> 4;
            adm_write(mdio, REG_ADM_LC_RACP, RegData);
        } else if(port == 4) {
            RegData  = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_PORT34_PVID) & 0x00ff;
            RegData |= (switch_config->port[port].vid & 0xff0) << 4;
            adm_write(mdio, REG_ADM_LC_PORT34_PVID, RegData);
        } else {
            RegData  = ADM_GET_EEPROM_REG(mdio, REG_ADM_LC_PORT0_PVID + port) & 0xff00;
            RegData |= (switch_config->port[port].vid & 0xff0) >> 4;
            adm_write(mdio, REG_ADM_LC_PORT0_PVID + port, RegData);
        }
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned short cpphy_tantos_get_vid_group(cpphy_mdio_t *mdio) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid_group;

    for(vid_group = 0; vid_group < switch_config->max_vid_groups; vid_group++) {
        if(!switch_config->vidgroup[vid_group].used) {
            break;
        }
    }
    if(vid_group >= switch_config->max_vid_groups) {
        DEB_ERR("[%s] Error! No free VLAN groups! Using 0 anyway!\n", __FUNCTION__);
        vid_group = 0;
    }
    switch_config->vidgroup[vid_group].used = 1;
    return vid_group;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned short cpphy_tantos_get_new_vid(cpphy_mdio_t  *mdio,
                                        unsigned char  device __attribute__ ((unused))) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid;

    for(vid = 4; vid < 4096; vid++) {
        if(!cpphy_switch_check_assigned_vid(mdio, vid)) {
            switch_config->assigned_vids[switch_config->assigned_vids_count++] = vid;
            return vid;
        }
    }
    return 0;
}


/*------------------------------------------------------------------------------------------*\
 * Configure VLAN group for TANTOS                                                          *
\*------------------------------------------------------------------------------------------*/
void adm_tantos_config_vlan_group(cpphy_mdio_t   *mdio,
                                  unsigned short  vid,
                                  unsigned short  portset,
                                  unsigned short  tagged_portset,
                                  unsigned char   port,
                                  unsigned char   FID,
                                  unsigned char   use_as_default) {
    unsigned short group;
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    tantos_vlan_filter filter;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE))
        return; /*--- CPMAC_ERR_NO_VLAN_POSSIBLE; ---*/

    group = cpphy_tantos_get_vid_group(mdio);
    assert(group < 16); /* Tantos has 16 VID groups */
    switch_config->vidgroup[group].vid = vid;
    switch_config->vidgroup[group].portset = portset;
    switch_config->vidgroup[group].port_in = port;
    switch_config->vidgroup[group].FID = FID;
    switch_config->map_vid_to_group[vid] = group;

    filter.VFL.Bits.VV = 1;
    filter.VFH.Bits.FID = FID;
    filter.VFL.Bits.VID = vid;
    filter.VFH.Bits.M = portset;
    filter.VFH.Bits.TM = tagged_portset;

    if(group < 8) {
        ADM_TANTOS_PUT(mdio, tantos_switch->VF0[group].VFH, filter.VFH.Register);
        ADM_TANTOS_PUT(mdio, tantos_switch->VF0[group].VFL, filter.VFL.Register);
    } else {
        ADM_TANTOS_PUT(mdio, tantos_switch->VF8[group - 8].VFH, filter.VFH.Register);
        ADM_TANTOS_PUT(mdio, tantos_switch->VF8[group - 8].VFL, filter.VFL.Register);
    }

    DEB_INFOTRC("[%s] Assign VLAN group %#x: VID %#x (%u) FID %u target %#x tagged %#x port_in %u\n", 
                __FUNCTION__,
                group, vid, vid, FID, portset, tagged_portset, port);

    /* Set the default VID */
    if(use_as_default) {
        ADM_TANTOS_GET(mdio, tantos_ports->port[port].PDVID);
        tantos_ports->port[port].PDVID.PVID = mdio->switch_status.port[port].vid;
        ADM_TANTOS_SYNC(mdio, tantos_ports->port[port].PDVID);
        DEB_INFOTRC("[%s] Mapping VID %#x (%u) to port %u\n", __FUNCTION__, vid, vid, port);
    }
}


/*------------------------------------------------------------------------------------------*\
 * Configure ports on the Tantos switch                                                     *
\*------------------------------------------------------------------------------------------*/
void adm_tantos_config_ports(cpphy_mdio_t *mdio) {
    t_cpphy_switch_config *switch_config = &mdio->switch_config;
    unsigned int port;

    if(!(mdio->Mode & CPPHY_SWITCH_MODE_WRITE))
        return; /*--- CPMAC_ERR_NO_VLAN_POSSIBLE; ---*/

    for(port = 0; port < switch_config->ports; port++) {
        ADM_TANTOS_GET(mdio, tantos_ports->port[port].PBVM);
        tantos_ports->port[port].PBVM.TBVE = switch_config->enable_vlan;
        ADM_TANTOS_SYNC(mdio, tantos_ports->port[port].PBVM);
    }

    for(port = 0; port < switch_config->ports; port++) {
        DEB_INFOTRC("Configure port %u with VID %#x %stagged\n",
                port,
                mdio->switch_config.port[port].vid,
                mdio->switch_config.port[port].keep_tag_outgoing ? "" : "un");

        /*--- Set the default VID ---*/
        ADM_TANTOS_GET(mdio, tantos_ports->port[port].PDVID);
        tantos_ports->port[port].PDVID.PVID = mdio->switch_config.port[port].vid;
        ADM_TANTOS_SYNC(mdio, tantos_ports->port[port].PDVID);
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void cpphy_tantos_set_default_vid(cpphy_mdio_t   *mdio, 
                                  unsigned char   device, 
                                  unsigned short  portset) {
    t_cpphy_switch_config *switch_config  = &mdio->switch_config;
    unsigned short vid, tagged_portset;
    unsigned char port, first_port;

    first_port = 1;
    for(port = 0; port < AVM_CPMAC_MAX_PORTS; port++) {
        if((portset & (1 << port)) == 0) {
            continue; /* Port is not in portset */
        }
        DEB_TEST("[%s] port %u\n", __FUNCTION__, port);
        switch_config->port[port].port_vlan_mask = portset;
        if(port == switch_config->cpu_port)  {
            continue; /* No tag for the CPU port needed */
        }
        vid = cpphy_tantos_get_new_vid(mdio, 0);
        if(vid == 0) {
            DEB_ERR("[%s] Error! No unassigned VID (for default) available!\n", __FUNCTION__);
            return;
        }
        tagged_portset = portset & (  (1 << switch_config->cpu_port)
                                    | (switch_config->wanport_keeptags ? (1 << switch_config->wanport) : 0));
        assert(mdio->f->config_vlan_group);
        mdio->f->config_vlan_group(mdio, vid, portset, tagged_portset,
                                   port, mdio->switch_config.device[device].FID, first_port);
        if(first_port) {
            DEB_INFOTRC("[%s] Setting default VID %#x for '%s' (portset %#x)\n",
                        __FUNCTION__, vid, mdio->switch_config.device[device].name, portset);
            switch_config->port[switch_config->cpu_port].vid = vid;
            switch_config->device[device].vid = vid;
        }
        switch_config->port[port].vid = vid;
        switch_config->port[port].keep_tag_outgoing = 0;
        /*--- DEB_INFOTRC("[%s] Mapping VID %#x (%u) to port %u\n", __FUNCTION__, ---*/ 
                    /*--- vid, vid, switch_config->map_vid_to_port[vid]); ---*/
        first_port = 0;
    }
    switch_config->port[switch_config->cpu_port].keep_tag_outgoing = 1;
}


/*------------------------------------------------------------------------------------------*\
 * Statistics for ADM6996 switches                                                          *
\*------------------------------------------------------------------------------------------*/
void cpphy_adm_update_hw_status(cpphy_mdio_t *mdio, struct net_device_stats *stats) {
    stats->collisions +=   ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT0)
                         + ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT1)
                         + ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT2)
                         + ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT3)
                         + ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT4)
                         + ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_COLLISIONCNT5);
    stats->rx_errors +=   ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT0)
                        + ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT1)
                        + ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT2)
                        + ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT3)
                        + ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT4)
                        + ADM_GET_SERIAL_REG(mdio, REG_ADM_LC_ERRCNT5);
    /* unfortunately switch has no distinction for rx/tx errors */
    stats->tx_errors = stats->rx_errors;
}
#endif /*--- #if (defined(CONFIG_MIPS_UR8) || defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7)) ---*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
cpmac_err_t adm_add_port_to_wan(cpphy_mdio_t *mdio, unsigned char port, unsigned short vid) {
    unsigned int group, found = 12345;
    t_cpphy_switch_config *switch_config = &mdio->switch_config;
    unsigned char wanport = switch_config->wanport;
    unsigned short tagged_portset;
    unsigned char use_tag_incoming = (vid == 0) ? 0 : 1;

    DEB_INFOTRC("[%s] Asked to add port %u to WAN for VLAN ID %u (%#x)\n",
                __FUNCTION__, port, vid, vid);

    /* Check port */
    if(port > 3) {
        DEB_ERR("[%s] Illegal port %u given\n", __FUNCTION__, port);
        return CPMAC_ERR_EXCEEDS_LIMIT;
    }
    port += switch_config->external_port_offset;

    if(port >= AVM_CPMAC_MAX_PORTS) {
        DEB_ERR("[%s] Port + port offset exceeds limit! (%u >= %u)\n", 
                __FUNCTION__, port, AVM_CPMAC_MAX_PORTS);
        return CPMAC_ERR_EXCEEDS_LIMIT;
    }
    
    /* This function is only intended for a kind of ATA mode */
    if(wanport == 0xff) {
        DEB_ERR("[%s] No WAN port defined!\n", __FUNCTION__);
        return CPMAC_ERR_ILL_CONTROL;
    }
    /* Find group for this VID */
    for(group = 0; group < switch_config->max_vid_groups; group++) {
        if(!switch_config->vidgroup[group].used) 
            continue;
        /* VID == 0 => Use first VLAN group that includes the WAN port */
        if((vid == 0) && (switch_config->vidgroup[group].portset & (1 << wanport))) {
            vid = switch_config->vidgroup[group].vid & 0xfff; /* & 0xfff for Klocwork */
        }
        if(switch_config->vidgroup[group].vid != vid) 
            continue;
        found = group;
        DEB_TRC("[%s] Found group %u for VID %u\n", __FUNCTION__, group, vid);
        break;
    }
    if(found == 12345) {
        DEB_ERR("[%s] Unknown VID %u given!\n", __FUNCTION__, vid);
        return CPMAC_ERR_EXCEEDS_LIMIT;
    }

    assert(mdio->f->config_vlan_group);
    /* Remove port from generic ethernet VLAN */
    for(group = 0; group < switch_config->max_vid_groups; group++) {
        unsigned int bit, count = 0;

        /* Only valid groups are interesting */
        if(!switch_config->vidgroup[group].used) 
            continue;

        /* Group should include port and exclude wanport */
        if(   !(switch_config->vidgroup[group].portset & (1 << port))
           ||  (switch_config->vidgroup[group].portset & (1 << wanport))) {
            continue;
        }

        /* Group should include more than two ports */
        for(bit = 0; bit < 16; bit++) {
            if(switch_config->vidgroup[group].portset & (1 << bit))
                count++;
        }
        if(count < 3) {
            DEB_INFOTRC("[%s] Not removing port from portset %#x, because only one port would be left!\n",
                        __FUNCTION__, switch_config->vidgroup[group].portset);
            continue;
        }

        /* Remove port from portmasks */
        switch_config->vidgroup[group].portset  &= ~(1 << port);
        tagged_portset =   switch_config->vidgroup[group].portset 
                         & (  (1 << switch_config->cpu_port)
                            | (switch_config->wanport_keeptags ? (1 << switch_config->wanport) : 0));
        mdio->f->config_vlan_group(mdio, switch_config->vidgroup[group].vid,
                                   switch_config->vidgroup[group].portset, 
                                   tagged_portset,
                                   switch_config->vidgroup[group].port_in, 
                                   switch_config->vidgroup[group].FID, 0);
    }

    /* Add port to given VID */
    switch_config->vidgroup[found].portset |= 1 << port;
    tagged_portset =   switch_config->vidgroup[found].portset 
                     & (  (1 << switch_config->cpu_port)
                        | (switch_config->wanport_keeptags ? (1 << switch_config->wanport) : 0));
    mdio->f->config_vlan_group(mdio, switch_config->vidgroup[found].vid,
                               switch_config->vidgroup[found].portset, 
                               tagged_portset,
                               switch_config->vidgroup[found].port_in, 
                               switch_config->vidgroup[found].FID, 0);


    /* Change default VID for the given port */
    DEB_INFOTRC("[%s] Setting default VID %u for port %u\n", __FUNCTION__, vid, port);
    switch_config->port[port].vid = vid;
    switch_config->port[port].keep_tag_outgoing = 0;
    switch_config->port[port].use_tag_incoming = use_tag_incoming;

    for(port = 0; port < switch_config->ports; port++) {
        unsigned int portset;
        vid     = switch_config->port[port].vid;
        group   = switch_config->map_vid_to_group[vid];
        portset = switch_config->vidgroup[group].portset;
        switch_config->port[port].port_vlan_mask = portset & 0xffff;
    }
    mdio->f->config_ports(mdio);

    return CPMAC_ERR_NOERR;
}

