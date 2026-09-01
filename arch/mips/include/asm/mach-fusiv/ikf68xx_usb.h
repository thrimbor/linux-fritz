/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Include file for Alchemy Semiconductor's Au1k CPU.
 *	modified for Ikanos's IKF68xx SoCs.
 *
 * Copyright 2000,2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

 /*
  * some definitions add by takuzo@sm.sony.co.jp and sato@sm.sony.co.jp
  */

#ifndef _IKF68XX_H
#define _IKF68XX_H

#include <linux/autoconf.h>

#include <linux/delay.h>
#include <asm/io.h>

/* IRQ for USB host controller on IKF68XX */
#define IKF68XX_USB_HOST_INT  35

/* Physical Base address of OHCI operational registers in ik68xx */
#define IKF68XX_USB_OHCI_BASE             0x19240800
/* Total size in bytes of all OHCI operational registers in ik68xx */
#define IKF68XX_USB_OHCI_LEN              0x0000005C

/* Physical Base address of EHCI capability  registers in ik68xx */
/* The location of EHCI operational registers is derived from ehci caplen reg */
#define IKF68XX_USB_EHCI_BASE             0x19230000

/* Total size in bytes of all EHCI capabilty registers in ik68xx */
#define IKF68XX_USB_EHCI_LEN              0x0000005C

#endif
