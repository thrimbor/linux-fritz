/*
 *
 * Gary Jennejohn <gj@denx.de>
 * Copyright (C) 2003 Gary Jennejohn
 *
 * ########################################################################
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
 * ########################################################################
 *
 * Reset the VR9 reference board.
 *
 */



#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <asm/reboot.h>
#include <asm/system.h>

#include <ifx_types.h>
#include <ifx_regs.h>
#include <ifx_gpio.h>

#include <linux/avm_hw_config.h>

/*== AVM/BC 20120130 Workaround: Some USB devices need a short port-reset before system reboot ==*/

#if defined(CONFIG_USB_ARCH_HAS_HCD)

#include <linux/delay.h>

#if defined(CONFIG_LANTIQ)
#define IFXUSB1_IOMEM_BASE  0x1E101000
#define IFXUSB2_IOMEM_BASE  0x1E106000
#else
#error unknown machine type
#endif

#define IFXUSB1_HOST_PORT  (volatile unsigned long *)(CKSEG1ADDR(IFXUSB1_IOMEM_BASE + 0x440))
#define IFXUSB2_HOST_PORT  (volatile unsigned long *)(CKSEG1ADDR(IFXUSB2_IOMEM_BASE + 0x440))

#define IFXUSB_PORT_RESET_BIT 8
#define IFXUSB_PORT_CONNECT_STATUS 0

#define reset_usb_ports() do {     \
    if (test_bit(IFXUSB_PORT_CONNECT_STATUS, (const volatile unsigned long *)IFXUSB2_HOST_PORT)       \
        || test_bit(IFXUSB_PORT_CONNECT_STATUS, (const volatile unsigned long *)IFXUSB2_HOST_PORT)) { \
        set_bit(IFXUSB_PORT_RESET_BIT, IFXUSB1_HOST_PORT);            \
        set_bit(IFXUSB_PORT_RESET_BIT, IFXUSB2_HOST_PORT);            \
        mdelay(100);                                                  \
        printk(KERN_ERR "usb reset workaround\n");                    \
    }                                                                 \
} while (0)

#else /*-- CONFIG_USB_ARCH_HAS_HCD --*/

#define reset_usb_ports() do { } while (0)

#endif /*-- CONFIG_USB_ARCH_HAS_HCD --*/

extern unsigned int avm_nmi_taken;
char *reboot_cause_written = NULL;

static void set_reboot_status(char *);

#define UPDATE_REBOOT_STATUS_TEXT   "(c) AVM 2013, Reboot Status is: Firmware-Update" \
                                    "(c) AVM 2013, Reboot Status is: Firmware-Update" \
                                    "(c) AVM 2013, Reboot Status is: Firmware-Update"
#define SOFT_REBOOT_STATUS_TEXT     "(c) AVM 2013, Reboot Status is: Software-Reboot" \
                                    "(c) AVM 2013, Reboot Status is: Software-Reboot" \
                                    "(c) AVM 2013, Reboot Status is: Software-Reboot"
#define NMI_REBOOT_STATUS_TEXT      "(c) AVM 2013, Reboot Status is: Software-NMI-Watchdog" \
                                    "(c) AVM 2013, Reboot Status is: Software-NMI-Watchdog" \
                                    "(c) AVM 2013, Reboot Status is: Software-NMI-Watchdog"
#define POWERON_REBOOT_STATUS_TEXT  "(c) AVM 2013, Reboot Status is: Power-On-Reboot" \
                                    "(c) AVM 2013, Reboot Status is: Power-On-Reboot" \
                                    "(c) AVM 2013, Reboot Status is: Power-On-Reboot"


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void set_reboot_status_to_NMI(void) {
    set_reboot_status(NMI_REBOOT_STATUS_TEXT);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void set_reboot_status_to_Update(void) {
    set_reboot_status(UPDATE_REBOOT_STATUS_TEXT);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define IFX_WDT_PW1 0x000000BE /**< First password for access */
#define IFX_WDT_PW2 0x000000DC /**< Second password for access */

static void ifx_machine_restart(char *command)
{

    local_irq_disable();
    if (avm_nmi_taken == ~0xdeadbabe) {
        printk(KERN_ERR "[IFX] double NMI and Oops\n");
    }

    reset_usb_ports();


    if (avm_nmi_taken != 0xdeadbabe) {
        set_reboot_status(SOFT_REBOOT_STATUS_TEXT);
    } else {
        set_reboot_status(NMI_REBOOT_STATUS_TEXT);
    } 
#if defined(CONFIG_AR9)
    *IFX_RCU_PPE_CONF &= ~(3 << 30);   //  workaround for AFE (enable_afe) abnormal behavior
#endif

    {
        u32 wdt_cr;

        /*--- hier den Watchdog ausschalten ---*/
        *IFX_WDT_CR = IFX_WDT_CR_PW_SET(IFX_WDT_PW1);   /* Write first part of password access */

        wdt_cr = *IFX_WDT_CR;
        wdt_cr &= ~IFX_WDT_CR_GEN;
        wdt_cr |=  IFX_WDT_CR_PW_SET(IFX_WDT_PW2);

        /* Set reload value in second password access */
        *IFX_WDT_CR = wdt_cr;
    }


    *IFX_RCU_RST_REQ = IFX_RCU_RST_REQ_ALL;
    for (;;) {
        ; /* Do nothing */
    }
}

static void ifx_machine_halt(void)
{
    /* Disable interrupts and loop forever */
    printk(KERN_NOTICE "System halted.\n");
    local_irq_disable();
    reset_usb_ports();
    for (;;) {
        ; /* Do nothing */
    }
}

/*--- #define POWER_OFF_GPIO      ((16 * 2) + 4) ---*/
/*--- #define POWER_OFF_GPIO      45 ---*/   /*--- ((16 * 2) + 13) ---*/
int POWER_OFF_GPIO;
int ARC_TAG_OVERWRITE_GPIO;

static void ifx_machine_power_off(void)
{
    /* We can't power off without the user's assistance */
    local_irq_disable();
    reset_usb_ports();
    printk(KERN_NOTICE "Power is turned off now.\n");
    if(POWER_OFF_GPIO != -1) {
        if(ifx_gpio_output_set(POWER_OFF_GPIO, IFX_GPIO_MODULE_SYSTEM) == IFX_ERROR) {
            printk(KERN_ERR "Power off failed.\n");
        } else {
            printk(KERN_ERR "Power should be off ????\n");
        }
    }
    for (;;) {
        ; /* Do nothing */
    }
}

void
ifx_reboot_setup(void)
{

    _machine_restart = ifx_machine_restart;
    _machine_halt = ifx_machine_halt;
    pm_power_off = ifx_machine_power_off;
}

int __init ifx_system_gpio_setup(void) {
    int ret;
    ret = ifx_gpio_register(IFX_GPIO_MODULE_SYSTEM);
    if(ret == IFX_ERROR) {
        printk(KERN_ERR "[%s] Error: Registering GPIO module ID %d failed!\n", __FUNCTION__, IFX_GPIO_MODULE_SYSTEM);
        panic("could not register power-ctrl-gpio\n");
    }
    if(avm_get_hw_config(AVM_HW_CONFIG_VERSION, "gpio_avm_system_power_off", &POWER_OFF_GPIO, NULL)) {
        POWER_OFF_GPIO = -1;
    }
    if(POWER_OFF_GPIO != -1)
        ifx_gpio_output_clear(POWER_OFF_GPIO, IFX_GPIO_MODULE_SYSTEM);

    if(avm_get_hw_config(AVM_HW_CONFIG_VERSION, "gpio_avm_arc_jtag_overwrite", &ARC_TAG_OVERWRITE_GPIO, NULL)) {
        ARC_TAG_OVERWRITE_GPIO = -1;
    }
    if(ARC_TAG_OVERWRITE_GPIO != -1)
        ifx_gpio_output_clear(ARC_TAG_OVERWRITE_GPIO , IFX_GPIO_MODULE_SYSTEM);
    return 0;
}

static void no_wait(void) {
    register volatile unsigned int a;
    register volatile unsigned int b;
    register volatile unsigned int c;
    do {
        c = a * b;
        b = a * c;
        a = b * c;
    } while(
            (*((volatile unsigned int *)(0xBF880200 + 0x00)) == 0) &&
            (*((volatile unsigned int *)(0xBF880200 + 0x28)) == 0) &&
            (*((volatile unsigned int *)(0xBF880200 + 0x50)) == 0) &&
            (*((volatile unsigned int *)(0xBF880200 + 0x78)) == 0) &&
            (*((volatile unsigned int *)(0xBF880200 + 0xA0)) == 0));
}


extern void (*cpu_wait)(void);
extern void r4k_wait(void);
extern void r4k_wait_irqoff(void);
static int __init wait_setup(char *p) {
    printk(KERN_ERR "[%s] param: '%s'\n", __FUNCTION__, p);
    if(!strcmp(p, "r4k_wait")) {
        printk(KERN_ERR "[%s] set kernel-idle-function to '%s'\n", __FUNCTION__, p);
		cpu_wait = r4k_wait;
    }
    if(!strcmp(p, "r4k_wait_irqoff")) {
        printk(KERN_ERR "[%s] set kernel-idle-function to '%s'\n", __FUNCTION__, p);
        cpu_wait = r4k_wait_irqoff;
    }
    if(!strcmp(p, "no_wait")) {
        printk(KERN_ERR "[%s] set kernel-idle-function to '%s'\n", __FUNCTION__, p);
		cpu_wait = no_wait;
    }
    return 0;
}


__setup("wait=", wait_setup);

late_initcall(ifx_system_gpio_setup);
unsigned int ifx_reboot_status;

int get_reboot_status(void) {
    static char Buffer[512];
    volatile unsigned char *mailbox = (volatile unsigned char *)(0xA1000000 - 512);
    memcpy(Buffer, (void *)mailbox, 512);
    Buffer[511] = '\0';
    printk("Reboot Status: %s\n", Buffer);

    if(!strcmp(Buffer, UPDATE_REBOOT_STATUS_TEXT)) {
        printk("Reboot Status is: FW-Update\n");
        ifx_reboot_status = 3;
        set_reboot_status(POWERON_REBOOT_STATUS_TEXT);
        reboot_cause_written = NULL;
        return 0;
    }
    if(!strcmp(Buffer, NMI_REBOOT_STATUS_TEXT)) {
        printk("Reboot Status is: NMI-Watchdog-Reboot\n");
        ifx_reboot_status = 2;
        set_reboot_status(POWERON_REBOOT_STATUS_TEXT);
        reboot_cause_written = NULL;
        return 0;
    }
    if(!strcmp(Buffer, SOFT_REBOOT_STATUS_TEXT)) {
        printk("Reboot Status is: Soft-Reboot\n");
        set_reboot_status(POWERON_REBOOT_STATUS_TEXT);
        reboot_cause_written = NULL;
        ifx_reboot_status = 1;
        return 0;
    }
    if(!strcmp(Buffer, POWERON_REBOOT_STATUS_TEXT)) {
        printk("Reboot Status is: Short-PowerOff-Reboot\n");
        set_reboot_status(POWERON_REBOOT_STATUS_TEXT);
        reboot_cause_written = NULL;
        ifx_reboot_status = 0;
        return 0;
    }
    printk("Reboot Status is: Power-On\n");
    set_reboot_status(POWERON_REBOOT_STATUS_TEXT);
    reboot_cause_written = NULL;
    ifx_reboot_status = 0;
    return 0;
}

static void set_reboot_status(char *text) {
    volatile unsigned char *mailbox = (volatile unsigned char *)(0xA1000000 - 512);
    int len;
    if((reboot_cause_written != NULL) && (reboot_cause_written != text)) {
        return;
    }
    reboot_cause_written = text;
    len = strlen(text);
    memcpy((char *)mailbox, text, len);
    mailbox += len;
    *mailbox = '\0';
}

arch_initcall(get_reboot_status);
