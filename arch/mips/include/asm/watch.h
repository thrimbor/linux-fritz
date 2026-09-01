/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2008 David Daney
 */
#ifndef _ASM_WATCH_H
#define _ASM_WATCH_H

#include <linux/bitops.h>

#include <asm/mipsregs.h>

void mips_install_watch_registers(void);
void mips_read_watch_registers(void);
void mips_clear_watch_registers(void);
void mips_probe_watch_registers(struct cpuinfo_mips *c);

#ifdef CONFIG_HARDWARE_WATCHPOINTS
#define __restore_watch() do {						\
	if (unlikely(test_bit(TIF_LOAD_WATCH,				\
			      &current_thread_info()->flags))) {	\
		mips_install_watch_registers();				\
	}								\
} while (0)

#else
#define __restore_watch() do {} while (0)
#endif


#if defined(CONFIG_AVM_WP)

#define SET_WATCHPOINT_FAIL_WRONG_TYPE 	-1
#define SET_WATCHPOINT_FAIL_NO_REG	 	-2
#define SET_WATCHPOINT_SUCCESS 			 0
#define NR_WATCHPOINTS 4

#define WP_TYPE_W (1 << 0)
#define WP_TYPE_R (1 << 1)
#define WP_TYPE_I (1 << 2)


struct avm_wp {
	int addr;
	int mask;
	int type;
	void (*handler)( int, int , int );
};

void default_wp_handler( int status, int deferred, int epc );
int avm_wp_dispatcher( void );

int watchpoint_busy( int watch_nr );
void del_watchpoint( int watch_nr );

int set_watchpoint(int watch_nr, int addr, int mask, int type);
int set_watchpoint_handler(int watch_nr, int addr, int mask, int type, void (*wp_handler)(int,int,int));
#endif

#endif /* _ASM_WATCH_H */
