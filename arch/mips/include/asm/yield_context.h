/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2013 AVM GmbH <fritzbox_info@avm.de>
 *
 *   author: mbahr@avm.de
 *   description: yield-thread-interface mips34k
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

#ifndef __yield_context_h__
#define __yield_context_h__

#if defined(CONFIG_LANTIQ)
#include <ifx_yield.h>
#endif/*--- #if defined(CONFIG_LANTIQ) ---*/
#define YIELD_MAX_TC            2

#define YIELD_HANDLED           1
/*--------------------------------------------------------------------------------*\
 * start function in non-linux-yield-context
 * Attention ! Only kseg0/kseg1 segment allowed - also data access in yield_handler !
 * ret: >= 0 number of registered signal < 0: errno
 *
 * return of request_yield_handler() handled -> YIELD_HANDLED
\*--------------------------------------------------------------------------------*/
int request_yield_handler(int signal, int (*yield_handler)(int signal, void *ref), void *ref);
/*--------------------------------------------------------------------------------*\
 * ret: == 0 ok
\*--------------------------------------------------------------------------------*/
int free_yield_handler(int signal, void *ref);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void disable_yield_handler(int signal);
void enable_yield_handler(int signal);

/*--------------------------------------------------------------------------------*\
 * yield_tc: tc to use for yield
 * yield_mask: wich signal(s) would be catched
 *
 * actually YIELD_MAX_TC tc possible, no crossover of yield_mask allowed
 * only on yield per cpu possible
\*--------------------------------------------------------------------------------*/
int yield_context_init_on(int cpu, unsigned int yield_tc, unsigned int yield_mask);

/*--------------------------------------------------------------------------------*\
 * same as cat /proc/yield/stat - but dump as printk
\*--------------------------------------------------------------------------------*/
void yield_context_dump(void);
#endif/*--- #ifndef __yield_context_h__ ---*/
