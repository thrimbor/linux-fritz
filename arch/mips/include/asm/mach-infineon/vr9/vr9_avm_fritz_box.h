/******************************************************************************
**
** FILE NAME    : vr9_avm_fritz_box.h
** PROJECT      : IFX UEIP
** MODULES      : BSP Basic
**
** DATE         : 21 Jan 2010
** AUTHOR       :
** DESCRIPTION  : header file for VR9
** COPYRIGHT    :       Copyright (c) 2009
**                      Infineon Technologies AG
**                      Am Campeon 1-12, 85579 Neubiberg, Germany
**
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
** HISTORY
** $Date        $Author         $Comment
** 27 May 2009   Xu Liang        The first UEIP release
** 21 Jan 2010   Heiko Blobner   New board added
*******************************************************************************/



#ifndef VR9_AVM_FRITZ_BOX_H
#define VR9_AVM_FRITZ_BOX_H

#define IFX_MTD_SPI_PARTS   3
#define IFX_MTD_NAND_PARTS  6
#define IFX_MTD_NOR_PARTS   6
#define IFX_MTD_JFFS2_MIN_SIZE  6
#define IFX_MTD_JFFS2_MAX_SIZE  16

#if defined(CONFIG_USB_HOST_IFX) || defined(CONFIG_USB_HOST_IFX_MODULE)
	#define IFX_GPIO_USB_VBUS1              IFX_GPIO_PIN_ID(0, 5)
	#define IFX_GPIO_USB_VBUS2              IFX_GPIO_PIN_ID(0, 14)
#endif


#endif  /* VR9_AVM_FRITZ_BOX_H */

