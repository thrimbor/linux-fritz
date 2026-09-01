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
/*------------------------------------------------------------------------------------------*\
 *
 *  $Id$
 *
 *  $Log$
 *
\*------------------------------------------------------------------------------------------*/
#ifndef _linux_ar7wdt_h_
#define _linux_ar7wdt_h_



/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define MAX_WDT_NAME_LEN        64
#define MAX_WDT_APPLS           31   /*--- Position 31 ist fuer init-ueberwachung reserviert ---*/
#if defined(CONFIG_MIPS_FUSIV)
#define WDT_DEFAULT_TIME         5   /*---  5 Sekunden ---*/
#else /*--- #if defined(CONFIG_MIPS_FUSIV) ---*/
#define WDT_DEFAULT_TIME        10   /*--- 10 Sekunden ---*/
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_MIPS_FUSIV) ---*/

struct _ar7wdt_command_queue {
    unsigned int rd, wr;
    unsigned char cmd[8];
};

struct _ar7wdt_appl_data {
    char Name[MAX_WDT_NAME_LEN + 1];
    unsigned int default_time;
	struct timer_list Timer;
    unsigned int *pf_owner;
    struct fasync_struct *fasync;
	wait_queue_head_t wait_queue;
    unsigned long req_jiffies;
    unsigned long avg_trigger;
    unsigned long pagefaults;
};

struct _ar7wdt_data {
    volatile unsigned int mask;
    volatile unsigned int triggered;
    volatile unsigned int requested;
    volatile unsigned int states; /*--- timer schon einmal erfolglos abgelaufen ---*/
    struct _ar7wdt_appl_data appl[MAX_WDT_APPLS + 1];
#if MAX_WDT_APPLS > 31
#error MAX_WDT_APPLS darf maximal 31 sein
#endif
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern int AVM_WATCHDOG_register(int, char *, int);
extern int AVM_WATCHDOG_release(int, char *, int);
extern int AVM_WATCHDOG_set_timeout(int, char *, int);
extern int AVM_WATCHDOG_trigger(int, char *, int);
extern int AVM_WATCHDOG_disable(int, char *, int);
extern int AVM_WATCHDOG_read(int, char *, int);
extern int AVM_WATCHDOG_init_start(int, char *, int);
extern int AVM_WATCHDOG_init_done(int, char *, int);
extern int AVM_WATCHDOG_reboot(int);
extern int AVM_WATCHDOG_poll(int);
extern void AVM_WATCHDOG_ungraceful_release(int);
extern void AVM_WATCHDOG_emergency_retrigger(void);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern void AVM_WATCHDOG_init(void);
extern void AVM_WATCHDOG_deinit(void);
extern struct fasync_struct **AVM_WATCHDOG_get_fasync_ptr(int handle);
extern wait_queue_head_t *AVM_WATCHDOG_get_wait_queue(int handle);
extern int ar7wdt_no_reboot;



/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7wdt_hw_trigger(void);
void ar7wdt_hw_reboot(void);
void ar7wdt_hw_deinit(void);
void ar7wdt_hw_init(void);
int ar7wdt_hw_is_wdt_running(void);








#endif /*--- #ifndef _linux_ar7wdt_h_ ---*/
