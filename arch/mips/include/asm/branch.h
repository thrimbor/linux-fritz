/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 1998, 2001 by Ralf Baechle
 */
#ifndef _ASM_BRANCH_H
#define _ASM_BRANCH_H

#include <asm/ptrace.h>
#include <asm/uaccess.h>
#include <asm/inst_mips16.h>

static inline int delay_slot(struct pt_regs *regs)
{
	return regs->cp0_cause & CAUSEF_BD;
}

static inline unsigned long exception_epc(struct pt_regs *regs)
{
	if (!delay_slot(regs))
		return regs->cp0_epc;

    if(regs->cp0_epc & 0x1) {
        printk(KERN_ERR "[%s]\n", __FUNCTION__);
        return regs->cp0_epc + compute_mips16_insn_size(regs->cp0_epc);
    }

    return regs->cp0_epc + 4;
}

extern int __compute_return_epc(struct pt_regs *regs);

static inline int compute_return_epc(struct pt_regs *regs)
{
	if (!delay_slot(regs)) {
        if(regs->cp0_epc & 0x1) 
    		regs->cp0_epc += compute_mips16_insn_size(regs->cp0_epc);
        else
    		regs->cp0_epc += 4;
		return 0;
	}

	return __compute_return_epc(regs);
}

#endif /* _ASM_BRANCH_H */
