/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

#include <linux/types.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>
#include <asm/system.h>
#include <asm/prom.h>
#include <linux/netdevice.h>

#include "athrs_vlan_igmp.h"

#if defined(CONFIG_MACH_AR7240) || defined(CONFIG_MACH_HORNET)
#include "ag7240.h"
#include <ar7240.h>
#endif

#if defined(CONFIG_MACH_AR724x)
#include "ag7240.h"
#include "atheros.h"
#endif

#if defined(CONFIG_MACH_AR934x)
#include "ag934x.h"
#include "atheros.h"
#endif

#ifdef CONFIG_MACH_HORNET
#define CONFIG_ETH_SOFT_LED 1
#endif

#if defined(CONFIG_MACH_AR7100)
#include "ag7100.h"
#include "ar7100.h"
#define is_ar7100() 1
#endif

#ifdef CONFIG_AR9100
#define is_ar9100() 1
#endif

#include "athrs_mac.h"
#include "athrs_phy.h"


int athr_gmac_phy_attach(void *arg, int unit) {
    int check_is_ar7240(void) {
        return is_ar7240();
    }
    int check_is_ar7241(void) {
        return is_ar7241();
    }
    int check_is_ar7242(void) {
        return is_ar7242();
    }
    int check_is_ar7100(void) {
        return is_ar7100();
    }
    int check_is_ar9330(void) {
        return is_ar9330();
    }
    int check_is_ar9340(void) {
        return is_ar9340();
    }
    int check_is_ar9341(void) {
        return is_ar9341();
    }
    int check_is_ar9342(void) {
        return is_ar9342();
    }
    int check_is_ar9344(void) {
        return is_ar9344();
    }
    int check_is_ar934x_1_0(void) {
        return is_ar934x_1_0();
    }
    int check_is_ar934x_1_1(void) {
        return is_ar934x_1_1();
    }
    int check_is_ar934x(void) {
        return is_ar934x();
    }
    struct _mac_config {
        int         (*check)(void);
        int         unit;
        int         hwrev;
        uint32_t    irq;
        uint32_t    flags;
        int         (*register_device)(struct athr_gmac *mac);
        char        *text;
    } mac_config [] = {
    /*--------------------------------------------------------------------------------------*\
     * if (is_ar7240() || is_ar7241() || (is_ar7242() && unit == 1) || is_ar7100() || is_ar933x()) {
     *    athrs26_register_ops(arg);
     * }
    \*--------------------------------------------------------------------------------------*/
#if defined(CONFIG_AR7240_S26_PHY) || defined(CONFIG_ATHRS26_PHY)
        {
            .check    = check_is_ar7240,
            .unit     = -1, /* on all units */
            .register_device = athrs26_register_ops,
            .text     = "Atheros internal S26 switch"
        },
        {
            .check    = check_is_ar7241,
            .unit     = -1, /* on all units */
            .register_device = athrs26_register_ops,
            .text     = "Atheros internal S26 switch"
        },
        {
            .check    = check_is_ar7242,
            .unit     = 1,
            .register_device = athrs26_register_ops,
            .text     = "Atheros internal S26 switch"
        },
        {
            .check    = check_is_ar7100,
            .unit     = -1, /* on all units */
            .register_device = athrs26_register_ops,
            .text     = "Atheros internal S26 switch"
        },
        {
            .check    = check_is_ar9330,
            .unit     = -1, /* on all units */
            .register_device = athrs26_register_ops,
            .text     = "Atheros internal S26 switch"
        },
#endif
    /*--------------------------------------------------------------------------------------*\
#if defined(CONFIG_AR7242_RGMII_PHY) || defined(CONFIG_ATHRF1_PHY)
   if((is_ar7242() && unit == 0) || is_ar7100()) {
      athrsf1_register_ops(arg);
   }
#endif
    \*--------------------------------------------------------------------------------------*/
#if defined(CONFIG_AR7242_RGMII_PHY) || defined(CONFIG_ATHRF1_PHY)
        {
            .check    = check_is_ar7242,
            .unit     = 0,
            .register_device = athrsf1_register_ops,
            .text     = "Atheros external F1 PHY"
        },
        {
            .check    = check_is_ar7100,
            .unit     = -1,
            .register_device = athrsf1_register_ops,
            .text     = "Atheros external F1 PHY"
        },
        {
            .check    = check_is_ar9341,
            .unit     = 0,
            .register_device = athrsf1_register_ops,
            .text     = "Atheros F1 PHY"
        },
#endif
    /*--------------------------------------------------------------------------------------*\
#ifdef CONFIG_ATHRF2_PHY
     athrsf2_register_ops(arg);
#endif
    \*--------------------------------------------------------------------------------------*/
#ifdef CONFIG_ATHRF2_PHY
        {
            .check    = check_is_ar934x,
            .hwrev    = 184,   /*--- Fritz Box 6841 LTE ---*/
            .unit     = 0,
            .irq      = ATHR_GPIO_IRQn(11), 
            .flags    = ETH_LINK_INTERRUPT,
            .register_device = athrsf2_register_ops,
            .text     = "Atheros F2 PHY"
        },
#endif
    /*--------------------------------------------------------------------------------------*\
#if defined(CONFIG_ATHRS16_PHY) || defined(CONFIG_AR7242_S16_PHY)
   if((is_ar7242() && unit == 0) || is_ar7100() || (is_ar934x() && unit == 0)){
      athrs16_register_ops(arg);
   }
#endif
    \*--------------------------------------------------------------------------------------*/
#if defined(CONFIG_ATHRS16_PHY) || defined(CONFIG_AR7242_S16_PHY)
        {
            .check    = check_is_ar7242,
            .unit     = 0,
            .register_device = athrs16_register_ops,
            .text     = "Atheros external S16 PHY"
        },
        {
            .check    = check_is_ar7100,
            .unit     = -1,
            .register_device = athrs16_register_ops,
            .text     = "Atheros external S16 PHY"
        },
        {
            .check    = check_is_ar934x_1_0,
            .unit     = 0,
            .register_device = athrs16_register_ops,
            .text     = "Atheros external S16 PHY"
        },
#endif
    /*--------------------------------------------------------------------------------------*\
#if defined(CONFIG_LANTIQ_11G_PHY)
      lantiq_11g_register_ops(arg);
#endif
    \*--------------------------------------------------------------------------------------*/
#if defined(CONFIG_LANTIQ_11G_PHY)
        {
            .check    = check_is_ar7242,
            .unit     = 0,
            .irq      = ATHR_GPIO_IRQn(CONFIG_AR7242_MDIOINT_PIN),
            .flags    = ETH_LINK_INTERRUPT,
            .register_device = lantiq_11g_register_ops,
            .text     = "Lantiq external PHY"
        },
#endif
    /*--------------------------------------------------------------------------------------*\
#if defined(CONFIG_ATHRS17_PHY)
      athrs17_register_ops(arg);
#endif
    \*--------------------------------------------------------------------------------------*/
#if defined(CONFIG_ATHRS17_PHY)
        {
            .check    = check_is_ar934x,
            .hwrev    = 96,   /*--- DB120 ---*/
            .unit     = 0,
            .irq      = ATHR_GPIO_IRQn(4),
            .flags    = CHECK_DMA_STATUS | ETH_LINK_POLL,              /*--- GE0 & AT8326 - no Interrupt ---*/
            .register_device = athrs17_register_ops,
            .text     = "Atheros external S17 PHY"
        },
#endif
    /*--------------------------------------------------------------------------------------*\
#ifdef CONFIG_ATHRS27_PHY
#ifdef CONFIG_ATHR_SUPPORT_DUAL_PHY
   if (is_ar934x() && unit) {
#else
   if (is_ar934x()) {
#endif
      printk("mac:%d Registering S27....\n",unit);
      athrs27_register_ops(arg);
   }
#endif
    \*--------------------------------------------------------------------------------------*/
#ifdef CONFIG_ATHRS27_PHY
        /*--- { ---*/
            /*--- .check    = check_is_ar934x_1_1, ---*/
            /*--- .unit     = 0, ---*/
            /*--- .register_device = athrs27_register_ops, ---*/
            /*--- .text     = "Atheros internal S27 PHY" ---*/
        /*--- }, ---*/
        {
            .check    = check_is_ar9344,
            .hwrev    = 96,   /*--- DB120 ---*/
            .unit     = 1,
            .flags    = ETH_LINK_INTERRUPT,
            .register_device = athrs27_register_ops,
            .text     = "Atheros internal S27 PHY"
        },
        {
            .check    = check_is_ar934x,
            .hwrev    = 184,   /*--- Fritz Box 6841 LTE ---*/
            .unit     = 1,
            .flags    = ETH_LINK_INTERRUPT,
            .register_device = athrs27_register_ops,
            .text     = "Atheros internal S27 PHY"
        },
        {
            .check    = check_is_ar934x,
            .hwrev    = 180,   /*--- Fritz Box 6810 LTE ---*/
            .unit     = 1,
            .flags    = ETH_SWONLY_MODE | ETH_LINK_INTERRUPT,
            .register_device = athrs27_register_ops,
            .text     = "Atheros internal S27 PHY"
        },
#endif
    /*--------------------------------------------------------------------------------------*\
#ifdef CONFIG_ATHRF1_PHY
   if(is_ar934x() && unit == 0){
      athrsf1_register_ops(arg);
   }
#endif
    \*--------------------------------------------------------------------------------------*/
#ifdef CONFIG_ATHRF1_PHY
        {
            .check    = check_is_ar934x,
            .unit     = 0,
            .register_device = athrsf1_register_ops,
            .text     = "Atheros external F1 PHY"
        },
#endif
    /*--------------------------------------------------------------------------------------*\
#if defined(CONFIG_HORNET_EMULATION) && defined(CONFIG_AR8021_PHY)
    if(1) {
       ar8021_register_ops(arg);
    }
#endif
    \*--------------------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------------------------*\
#ifdef CONFIG_ATHRS_VIR_PHY
   if (is_ar934x() && unit == 0){
	printk("Registering Virtual Phy....\n");
        athrs_vir_phy_register_ops(arg);
    }
#endif
    \*--------------------------------------------------------------------------------------*/
        {
            .check    = NULL,
        },

    };

    struct _mac_config *probe = &mac_config[0];
    int fritz_box_hw_revision;
    char *p = prom_getenv("HWRevision");
    athr_gmac_t *mac   = (athr_gmac_t *)arg;

    if (p) {
        fritz_box_hw_revision = simple_strtoul(p, NULL, 10);
    } else {
        fritz_box_hw_revision = 0;
    }

    while(probe->register_device) {
        if(((probe->check == NULL) || (*probe->check)()) && ((probe->unit == unit) || (probe->unit == -1))) {
            if((probe->hwrev == 0) || (probe->hwrev == fritz_box_hw_revision)) {
                printk(KERN_ERR "[%s] use %s for mac unit %d (hwrev %d)\n", __FUNCTION__, probe->text, unit, fritz_box_hw_revision);
                mac->mac_flags = probe->flags; 
                mac->mac_linkirq = probe->irq;
                mac->phy_register_device = *probe->register_device;
                return 0;
            }
        }

        probe++;
    }
   return 1;
}




