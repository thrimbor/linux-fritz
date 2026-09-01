/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2013 AVM GmbH <fritzbox_info@avm.de>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <asm/mach-ur8/ur8.h>
#include <asm/mach-ur8/hw_nwss.h>
#include <asm/mach-ur8/ur8_cppi.h>

#define VIRT_TO_PHYS(addr) CPHYSADDR((addr))

static struct __nwss_td_desc (*TDArray)[2 * UR8_NWSS_CHANNELS] = NULL;
static struct __nwss_td_desc *TDArrayInMemory = NULL;
struct ur8_nwss_register *UR8_NWSS = (struct ur8_nwss_register *) &(*(volatile unsigned int *)(UR8_NWSS_BASE));


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ur8_teardown_init(void) {
    unsigned int size = 0;
    unsigned char channel;

    if(TDArrayInMemory != NULL) {
        return 0;
    }
    size = ((sizeof (struct __nwss_td_desc) * UR8_NWSS_CHANNELS * 2) + 512);
    TDArrayInMemory = kmalloc(size, GFP_ATOMIC);
    if(TDArrayInMemory == NULL) {
        printk(KERN_ERR "Could not allocate memory for CPPI teardown descriptors.\n");
        return 1;
    }
    memset(TDArrayInMemory, 0, size);

    TDArray = (struct __nwss_td_desc (*)[])(((unsigned int) TDArrayInMemory & 0xFFFFFF00) + 512);

    /* initialize teardown array with channel numbers */
    for(channel = 0; channel < UR8_NWSS_CHANNELS; channel++) {
        /* store channel number in teardown descriptor */
        (*TDArray)[2 * channel].channel_no = channel; /* Tx teardown descriptor */
        (*TDArray)[2 * channel].channel_type = 0;
        (*TDArray)[2 * channel + 1].channel_no = channel; /* Rx teardown descriptor */
        (*TDArray)[2 * channel + 1].channel_type = 0;
    }

    UR8_NWSS->TearDown_Array_Pointer = (volatile struct __nwss_td_desc *) VIRT_TO_PHYS((unsigned int) TDArray);
    UR8_NWSS->TearDown_Array_pSize.td_desc_size = (sizeof(struct __nwss_td_desc) / 4) - 1;

    return 0;
}
EXPORT_SYMBOL(ur8_teardown_init);


/*------------------------------------------------------------------------------------------*\
 * Return the Address of the TX Teardown Buffer Descriptor from the SW array
\*------------------------------------------------------------------------------------------*/
void *ur8_get_tx_teardown_BD(unsigned char chNum) {
    return (void *) &(*TDArray)[chNum];
}
EXPORT_SYMBOL(ur8_get_tx_teardown_BD);


/*------------------------------------------------------------------------------------------*\
 * Return the Address of the RX Teardown Buffer Descriptor from the SW array
\*------------------------------------------------------------------------------------------*/
void *ur8_get_rx_teardown_BD(unsigned char chNum) {
    return (void *) &(*TDArray)[chNum + 1];
}
EXPORT_SYMBOL(ur8_get_rx_teardown_BD);


