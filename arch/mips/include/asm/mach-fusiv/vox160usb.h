/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Include file for Alchemy Semiconductor's Au1k CPU.
 *	modified for ADI's vox160 CPU.
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

#ifndef _VOX160USB_H
#define _VOX160USB_H

#include <linux/autoconf.h>

#include <linux/delay.h>
#include <asm/io.h>

/* IRQ for USB host controller on VOX160 */
#define VOX160_USB_HOST_INT  35

/* Physical Base address of OHCI operational registers in vox160 */
#define VOX160_USB_OHCI_BASE             0x19240800
/* Total size in bytes of all OHCI operational registers in vox160 */
#define VOX160_USB_OHCI_LEN              0x00000058

/* Physical Base address of EHCI capability  registers in vox160 */
/* The location of EHCI operational registers is derived from ehci caplen reg */
#define VOX160_USB_EHCI_BASE             0x19230000

/* Total size in bytes of all EHCI capabilty registers in vox160 */
#define VOX160_USB_EHCI_LEN              0x00000058

/* IRQ for USBD on VOX160 */
#define VOX160_USBD_INT  3

/* Physical Base address of USBD general registers in vox160 */
#define VOX160_USBD_GENERAL_REG_BASE             0x19160000
/* Total size in bytes of USBD general registers in vox160 */
#define VOX160_USBD_GENERAL_REG_LEN              0x00000020

/* Physical Base address of USBD DMA registers in vox160 */
#define VOX160_USBD_DMA_REG_BASE             0x19160080
/* Total size in bytes of USBD DMA registers in vox160 */
#define VOX160_USBD_DMA_REG_LEN              0x00000014

/* Physical Base address of USBD end point interrupt registers in vox160 */
#define VOX160_USBD_INTRN_REG_BASE             0x19160100
/* Physical Base address of USBD end point mask registers in vox160 */
#define VOX160_USBD_MASKN_REG_BASE             0x19160104
/* Physical Base address of USBD end point control registers in vox160 */
#define VOX160_USBD_EPCFGN_REG_BASE            0x19160108
/* Physical Base address of USBD end point address offset registers in vox160 */
#define VOX160_USBD_EPADRN_REG_BASE            0x1916010c
/* Physical Base address of USBD end point buffer length registers in vox160 */
#define VOX160_USBD_EPLEN_REG_BASE             0x19160110
/* Total size in bytes of USBD end point buffer length registers in vox160 */
#define VOX160_USBD_EP_REG_LEN              0x00000004

#endif
