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
#ifndef _AVM_EVENT_H_
#define _AVM_EVENT_H_


#include "avm_sammel.h"

#if !defined(DEBUG_SAMMEL)
#define DEBUG_SAMMEL
#if defined(AVM_EVENT_DEBUG)
#define DEB_ERR(args...)     printk(KERN_ERR args)
#define DEB_WARN(args...)    printk(KERN_WARNING args)
#define DEB_NOTE(args...)    printk(KERN_NOTICE args)
#define DEB_INFO(args...)    printk(KERN_INFO args)
#define DEB_TRACE(args...)   printk(KERN_INFO args)
#else /*--- #if defined(AVM_EVENT_DEBUG) ---*/
#define DEB_ERR(args...)     printk(KERN_ERR args)
#define DEB_WARN(args...)
#define DEB_NOTE(args...)
#define DEB_INFO(args...)
#define DEB_TRACE(args...)
#endif /*--- #else ---*/ /*--- #if defined(AVM_EVENT_DEBUG) ---*/
#endif /*--- #if !defined(DEBUG_SAMMEL) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int __init avm_event_init(void);
void avm_event_cleanup(void);



#endif /*--- #ifndef _AVM_EVENT_H_ ---*/
