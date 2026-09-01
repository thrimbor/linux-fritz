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

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
/*--- #include <linux/cdev.h> ---*/
#include <linux/fs.h>
/*--- #include <asm/semaphore.h> ---*/
#include <asm/errno.h>
/*--- #include <linux/wait.h> ---*/
/*--- #include <linux/vmalloc.h> ---*/
/*--- #include <linux/poll.h> ---*/

#include "avm_sammel.h"
#include "avm_led.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#if defined(CONFIG_MIPS)
#include <asm/mips-boards/hardware_error.h>
#endif /*--- #if defined(CONFIG_MIPS) ---*/
#if defined(CONFIG_ARM)
#include <asm/arch/hardware_error.h>
#endif /*--- #if defined(CONFIG_ARM) ---*/

#include <asm/mach_avm.h>
#else
#endif


#define MAX_HARDWARE_ERRORS     20

static char *hardware_errors[MAX_HARDWARE_ERRORS];
static unsigned int hardware_errors_count = 0;
static int hardware_error_led;
static unsigned int hardware_error_init_done = 0;


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int hardware_error_info(char *buf, char **start, off_t offset, int count, int *eof, void *data)        {                                                                                              
	int len = 0;                                                                                 
    int i;
    for ( i = 0 ; i < hardware_errors_count ; i++) {
	    len += sprintf(buf + len, "%s ", hardware_errors[i]);
    }
	return len;                                                                                
}                                                                                              

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void hardware_error_init(void) {
    printk("[hardware_error_init]\n");
    create_proc_read_entry("hardware_errors", 0, NULL, hardware_error_info, NULL);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void hardware_error_log(char *error_msg, unsigned int blink_code) {
    unsigned int len = strlen(error_msg) + 1;
    if(hardware_error_init_done == 0) {
        hardware_error_led = avm_led_get_virt_led_handle("error", 0);
    }

    if(blink_code)
        avm_led_virt_led_action(hardware_error_led, AVM_LED_BLINK_CODE_2Hz(blink_code));

    if(hardware_errors_count >= MAX_HARDWARE_ERRORS)
        return;
    hardware_errors[hardware_errors_count] = vmalloc(len);
    if(hardware_errors[hardware_errors_count]) {
        memcpy(hardware_errors[hardware_errors_count], error_msg, len);
        hardware_errors_count++;
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
EXPORT_SYMBOL(hardware_error_log);

