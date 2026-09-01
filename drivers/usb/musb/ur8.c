/*
 * Copyright (C) 2005-2006 by Texas Instruments
 *
 * This file is part of the Inventra Controller Driver for Linux.
 *
 * The Inventra Controller Driver for Linux is free software; you
 * can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * The Inventra Controller Driver for Linux is distributed in
 * the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with The Inventra Controller Driver for Linux ; if not,
 * write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <asm/io.h>

#include <ur8.h>
#include <hw_reset.h>
#include <ur8_reset.h>

#ifdef CONFIG_AVM_POWER
#include <linux/avm_power.h>
#endif

#include "musb_core.h"
#include "musbhdrc.h"

#include "ur8.h"
#include "cppi_dma.h"

/* REVISIT (PM) we should be able to keep the PHY in low power mode most
 * of the time (24 MHZ oscillator and PLL off, etc) by setting POWER.D0
 * and, when in host mode, autosuspending idle root ports... PHYPLLON
 * (overriding SUSPENDM?) then likely needs to stay off.
 */

static inline void phy_on(void)
{
    /* start the on-chip PHY and its PLL */
    /* Enable entire phy including the  charg pump reset D1 bit */
    
    void    *__iomem tbase = (void *__iomem)UR8_DEVICE_CONFIG_BASE;
    musb_writel(tbase, UR8_USBPHYDISABLE_REG, 0);
        
    /* Check D0 bit if D0 =1 power is stable and PLL is locked */       
    while ((musb_readl(tbase, UR8_USBPWRCLKGOOD_REG)   
        & UR8_USBPWRCLKGOOD_USB_POWERCLOCKGOOD) == 0)
        cpu_relax();
}

static int cp_on = 0;

static inline void phy_off(void)
{
    /* powerdown the on-chip PHY and its oscillator */
    void    *__iomem tbase = (void *__iomem)UR8_DEVICE_CONFIG_BASE;
    musb_writel(tbase, UR8_USBPHYDISABLE_REG, UR8_USBPHYDISABLE_USB_PHY_ALL_DISABLE);
}

static inline void phy_charge(u8 on)
{
    /* enable charge pump */
    void    *__iomem tbase = (void *__iomem)UR8_DEVICE_CONFIG_BASE;
    cp_on = on;
    musb_writel(tbase, UR8_USBPHYDISABLE_REG, on? 0: UR8_USBPHYDISABLE_USB_CP_DISABLE);
}

static int dma_off = 1;

void musb_platform_enable(struct musb *musb)
{
	u32	tmp, old, val;

	/* workaround:  setup irqs through both register sets */
	tmp = (musb->epmask & DAVINCI_USB_TX_ENDPTS_MASK)
			<< DAVINCI_USB_TXINT_SHIFT;
	musb_writel(musb->ctrl_base, DAVINCI_USB_INT_MASK_SET_REG, tmp);
	old = tmp;
	tmp = (musb->epmask & (0xfffe & DAVINCI_USB_RX_ENDPTS_MASK))
			<< DAVINCI_USB_RXINT_SHIFT;
	musb_writel(musb->ctrl_base, DAVINCI_USB_INT_MASK_SET_REG, tmp);
	tmp |= old;

	val = ~MGC_M_INTR_SOF;
   
//  tmp |= ((val & 0x01ff) << DAVINCI_USB_USBINT_SHIFT);
  tmp |= ((val & 0x00ff) << DAVINCI_USB_USBINT_SHIFT);
	musb_writel(musb->ctrl_base, DAVINCI_USB_INT_MASK_SET_REG, tmp);

	if (is_dma_capable() && !dma_off)
		printk(KERN_WARNING "%s %s: dma not reactivated\n",
				__FILE__, __FUNCTION__);
	else
		dma_off = 0;
}

/*
 * Disable the HDRC and flush interrupts
 */
void musb_platform_disable(struct musb *musb)
{
	/* because we don't set CTRLR.UINT, "important" to:
	 *  - not read/write INTRUSB/INTRUSBE
	 *  - (except during initial setup, as workaround)
	 *  - use INTSETR/INTCLRR instead
	 */
	musb_writel(musb->ctrl_base, DAVINCI_USB_INT_MASK_CLR_REG,
			  DAVINCI_USB_USBINT_MASK
			| DAVINCI_USB_TXINT_MASK
			| DAVINCI_USB_RXINT_MASK);
	musb_writeb(musb->mregs, MGC_O_HDRC_DEVCTL, 0);
	musb_writel(musb->ctrl_base, DAVINCI_USB_EOI_REG, 0);

	if (is_dma_capable() && !dma_off)
		printk("[%s] dma still active\n", __FUNCTION__);
}


/* REVISIT this file shouldn't modify the OTG state machine ...
 *
 * The OTG infrastructure needs updating, to include things like
 * offchip DRVVBUS support and replacing MGC_OtgMachineInputs with
 * musb struct members (so e.g. vbus_state vanishes).
 */
static int vbus_state = UR8_vbus_power_invalid;

#ifdef CONFIG_USB_MUSB_HDRC_HCD
#define	portstate(stmt)		stmt
#else
#define	portstate(stmt)
#endif

static void session(struct musb *musb, int is_on)
{
	if (musb->xceiv != NULL) {
		void	*__iomem mregs = musb->mregs;
	
		if (musb->xceiv->default_a) {
			u8  devctl = musb_readb(mregs, MGC_O_HDRC_DEVCTL);

			if (is_on)
				devctl |= MGC_M_DEVCTL_SESSION;
			else
				devctl &= ~MGC_M_DEVCTL_SESSION;
			musb_writeb(mregs, MGC_O_HDRC_DEVCTL, devctl);
			//printk ("session on %u devctl pre %x post %x\n", is_on, devctl,  musb_readb(mregs, MGC_O_HDRC_DEVCTL));
		} else
			is_on = 0;

		if (is_on) {
			/* NOTE: assumes VBUS already exceeds A-valid */
			if (musb->xceiv->state != OTG_STATE_A_HOST) {
				musb->xceiv->state = OTG_STATE_A_WAIT_BCON;
			}
			portstate(musb->port1_status |= USB_PORT_STAT_POWER);
			MUSB_HST_MODE(musb);
		} else {
			switch (musb->xceiv->state) {
			case OTG_STATE_UNDEFINED:
			case OTG_STATE_B_IDLE:
				MUSB_DEV_MODE(musb);
				musb->xceiv->state = OTG_STATE_B_IDLE;
				break;
			case OTG_STATE_A_IDLE:
				break;
			default:
				musb->xceiv->state = OTG_STATE_A_WAIT_VFALL;
				break;
			}
			portstate(musb->port1_status &= ~USB_PORT_STAT_POWER);
		}

		DBG(2, "Default-%c, VBUS power %s, %s, devctl %02x, %s\n",
			musb->xceiv->default_a ? 'A' : 'B',
			is_on ? "on" : "off",
			MUSB_MODE(musb),
			musb_readb(musb->mregs, MGC_O_HDRC_DEVCTL),
			otg_state_string(musb));
	}
}

static struct timer_list power_timer;

static void ur8musb_vbus_power(struct musb *musb, int is_on)
{
    unsigned long flags;

    spin_lock_irqsave(&musb->lock, flags);
    if (vbus_state == is_on) {
        spin_unlock_irqrestore(&musb->lock, flags);
        return;
    }

    del_timer(&power_timer);

#ifdef CONFIG_USB_MUSB_AVM_BOARD_UR8_VBUS_NOT_CONNECTED
	if (is_on == UR8_vbus_power_trigger_cp) {
		is_on = UR8_vbus_power_on;
	}
#endif
    vbus_state = is_on;
        
	switch (is_on) {
#ifndef CONFIG_USB_MUSB_AVM_BOARD_UR8_VBUS_NOT_CONNECTED
    case UR8_vbus_power_trigger_cp:
        DBG(2,"try just phy charge on in 30 s\n");
        #ifdef CONFIG_AVM_POWER
            PowerManagmentActivatePowerMode("usb_poweron");
        #endif
        mod_timer(&power_timer, jiffies + HZ*30);
        break;
#endif
    case UR8_vbus_power_trigger_restart:
        DBG(2,"vbus power restart\n");
        mod_timer(&power_timer, jiffies + HZ/2);
        /* no break;*/
    case UR8_vbus_power_off:
        DBG(2,"vbus power off\n");
        phy_charge (0);
        
        #ifdef  CONFIG_AVM_POWER
            PowerManagmentActivatePowerMode("usb_poweroff");
        #endif
        session (musb, 0);
        break;
    case UR8_vbus_power_on:
        DBG(2,"vbus power on\n");

        #ifdef CONFIG_AVM_POWER
            PowerManagmentActivatePowerMode("usb_poweron");
        #endif

#ifndef CONFIG_USB_MUSB_AVM_BOARD_UR8_VBUS_NOT_CONNECTED
        /* CHG: 20081120 WK
        delay phy_power off 100 ms due to Trekstor HDD
        */
        mod_timer(&power_timer, jiffies + HZ/10);
#else
        phy_charge (1);
        session (musb, 1);
#endif
        break;
    case UR8_vbus_power_cp_only:
        DBG(2,"just phy charge on\n");
        //vbus_state = 1;
        phy_charge (1);

        #ifdef CONFIG_AVM_POWER
            PowerManagmentActivatePowerMode("usb_poweroff");
        #endif
        break;
        
    default: /* precharge vbus */
        DBG(0,"bogus VBUS state 0x%x\n", vbus_state);
		printk("[%s] WARN!\n", __FUNCTION__);
        break;
    }
    spin_unlock_irqrestore(&musb->lock, flags);
}

/* timer callback */
static void power_timer_func (unsigned long _musb)
{
    struct musb *musb = (struct musb *) _musb;
    void    *__iomem mregs = musb->mregs;
    u8 bTimerStop = 0;

    unsigned long flags;

    spin_lock_irqsave(&musb->lock, flags);
    {
        u8 devctl = musb_readb(mregs, MGC_O_HDRC_DEVCTL);

        switch (vbus_state) {
        case UR8_vbus_power_trigger_restart:
            if ((devctl&MGC_M_DEVCTL_VBUS) == 0) {
                bTimerStop = 1;
                spin_unlock_irqrestore(&musb->lock, flags);
                ur8musb_vbus_power (musb,  UR8_vbus_power_on);
            }
            break;
#ifndef CONFIG_USB_MUSB_AVM_BOARD_UR8_VBUS_NOT_CONNECTED
        case UR8_vbus_power_on:
            if ((devctl&MGC_M_DEVCTL_VBUS) == MGC_M_DEVCTL_VBUS) {
				phy_charge (0);

                DBG(2, "vbus power OK: start session\n");
                session (musb, 1);
                bTimerStop = 1;
                spin_unlock_irqrestore(&musb->lock, flags);
            }
            break;
        case UR8_vbus_power_trigger_cp:
            bTimerStop = 1;
            spin_unlock_irqrestore(&musb->lock, flags);

            if (((devctl&MGC_M_DEVCTL_VBUS) == MGC_M_DEVCTL_VBUS)
               && (!(devctl&MGC_M_DEVCTL_BDEVICE))
               && (!(devctl&MGC_M_DEVCTL_HM))
            ) {
                ur8musb_vbus_power (musb,  UR8_vbus_power_cp_only);
            } else {
                ur8musb_vbus_power (musb,  UR8_vbus_power_on);
            }
            break;
#endif
        default:
            bTimerStop = 1;
            spin_unlock_irqrestore(&musb->lock, flags);
            ERR("invalid vbus_state %d\n", vbus_state);

            break;
        }
    }

    if (!bTimerStop) {
        mod_timer(&power_timer, jiffies + HZ/2);
        spin_unlock_irqrestore(&musb->lock, flags);
    }
}




static void ur8musb_set_vbus(struct musb *musb, int is_on)
{
	WARN_ON(is_on && is_peripheral_active(musb));
	return ur8musb_vbus_power(musb, is_on);
}

unsigned ur8musb_is_chargepump_enabled (void) {
    return cp_on != 0;
}

static irqreturn_t ur8musb_interrupt(int irq, void *__hci)
{
    unsigned long   flags;
    irqreturn_t retval = IRQ_NONE;
    struct musb *musb = __hci;
    void        *__iomem tibase = musb->ctrl_base;
    u32     tmp;

    spin_lock_irqsave(&musb->lock, flags);

    /* NOTE: DaVinci shadows the Mentor IRQs.  Don't manage them through
     * the Mentor registers (except for setup), use the TI ones and EOI.
     *
     * Docs describe irq "vector" registers asociated with the CPPI and
     * USB EOI registers.  These hold a bitmask corresponding to the
     * current IRQ, not an irq handler address.  Would using those bits
     * resolve some of the races observed in this dispatch code??
     */
/* == AVM/WK 20101105 FIX: CPPI on UR8 has own IRQ line, no need to call here == */
#if 0
    /* CPPI interrupts share the same IRQ line, but have their own
     * mask, state, "vector", and EOI registers.
     */
	{
		struct cppi	*cppi;
		cppi = container_of(musb->dma_controller, struct cppi, controller);
		if (is_cppi_enabled() && musb->dma_controller && !cppi->irq)
			retval = cppi_interrupt(irq, __hci);
	}
#endif

    /* ack and handle non-CPPI interrupts */
    tmp = musb_readl(tibase, DAVINCI_USB_INT_SRC_MASKED_REG);
    musb_writel(tibase, DAVINCI_USB_INT_SRC_CLR_REG, tmp);

    musb->int_rx = (tmp & DAVINCI_USB_RXINT_MASK)
            >> DAVINCI_USB_RXINT_SHIFT;
    musb->int_tx = (tmp & DAVINCI_USB_TXINT_MASK)
            >> DAVINCI_USB_TXINT_SHIFT;
    musb->int_usb = (tmp & DAVINCI_USB_USBINT_MASK)
            >> DAVINCI_USB_USBINT_SHIFT;

    if (musb->int_tx || musb->int_rx || musb->int_usb)
        retval |= musb_interrupt(musb);

    /* irq stays asserted until EOI is written */
    musb_writel(tibase, DAVINCI_USB_EOI_REG, 0);

    spin_unlock_irqrestore(&musb->lock, flags);

    /* REVISIT we sometimes get unhandled IRQs
     * (e.g. ep0).  not clear why...
     */
    if (retval != IRQ_HANDLED)
        DBG(4, "unhandled? %08x\n", tmp);
    return IRQ_HANDLED;
}

int musb_platform_set_mode(struct musb *musb, u8 mode)
{
	/* EVM can't do this (right?) */
	return -EIO;
}
int __devinit musb_platform_init(struct musb *musb)
{
	void	*__iomem tibase = musb->ctrl_base;
	u32	revision;

	usb_nop_xceiv_register();

	musb->xceiv = otg_get_transceiver();
	if (!musb->xceiv)
		return -ENODEV;
	
    ur8_take_device_out_of_reset (USB_RESET_BIT);
    ur8_take_device_out_of_reset (USB_PHY_RESET_BIT);

	musb->mregs += DAVINCI_BASE_OFFSET;

	/* returns zero if e.g. not clocked */
	revision = musb_readl(tibase, DAVINCI_USB_VERSION_REG);
	if (revision == 0) {
		usb_nop_xceiv_unregister();	
		return -ENODEV;
	}

    init_timer(&power_timer);
    power_timer.function = power_timer_func;
    power_timer.data = (unsigned long) musb;

	musb->board_set_vbus = ur8musb_set_vbus;

    // phy charge on, ext. power off
    ur8musb_vbus_power(musb, UR8_vbus_power_cp_only);

	/* reset the controller */
	musb_writel(tibase, DAVINCI_USB_CTRL_REG, 0x1);

	/* start the on-chip PHY and its PLL */
	phy_on();

	msleep(5);

	/* NOTE:  irqs are in mixed mode, not bypass to pure-musb */

	pr_debug("UR8 OTG revision %08x phy %03x control %02x\n",
		revision,
        musb_readb(tibase, UR8_USBPHYDISABLE_REG),
        musb_readb(tibase, DAVINCI_USB_CTRL_REG));
    musb->isr = ur8musb_interrupt;

	return 0;
}

int musb_platform_exit(struct musb *musb)
{
    ur8musb_vbus_power(musb, UR8_vbus_power_off);

    del_timer(&power_timer);
 
	/* delay, to avoid problems with module reload */
	if (is_host_enabled(musb)) {
		int	maxdelay = 30;
		u8	devctl, warn = 0;

		/* if there's no peripheral connected, this can take a
		 * long time to fall, especially on EVM with huge C133.
		 */
		do {
			devctl = musb_readb(musb->mregs, MGC_O_HDRC_DEVCTL);
			if (!(devctl & MGC_M_DEVCTL_VBUS))
				break;
			if ((devctl & MGC_M_DEVCTL_VBUS) != warn) {
				warn = devctl & MGC_M_DEVCTL_VBUS;
				DBG(1, "VBUS %d\n", warn >> MGC_S_DEVCTL_VBUS);
			}
			msleep(1000);
			maxdelay--;
		} while (maxdelay > 0);

		/* in OTG mode, another host might be connected */
		if (devctl & MGC_M_DEVCTL_VBUS)
			DBG(1, "VBUS off timeout (devctl %02x)\n", devctl);
	}

	phy_off();
	usb_nop_xceiv_unregister();
	return 0;
}
