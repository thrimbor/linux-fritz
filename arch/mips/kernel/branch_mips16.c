/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 97, 2000, 2001 by Ralf Baechle
 * Copyright (C) 2001 MIPS Technologies, Inc.
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <asm/branch.h>
#include <asm/cpu.h>
#include <asm/cpu-features.h>
#include <asm/fpu.h>
#include <asm/inst_mips16.h>
#include <asm/ptrace.h>
#include <asm/uaccess.h>
#include "unaligned.h"

/*
 * Compute the return address and do emulate branch simulation, if required.
 */
int __compute_return_epc_mips16(struct pt_regs *regs)
{
    union mips16_instruction insn, next_insn;
    long epc;
#ifdef __BIG_ENDIAN
    short offset, rx;
    unsigned int test = 0;
#endif

    /*--- unsigned int bit, fcr31, dspcontrol; ---*/

    epc = regs->cp0_epc;

    if(read_mips16_insn(&insn, epc&~0x1 ))
        goto sigsegv;
    if(read_mips16_insn(&next_insn, (epc&~0x1) + MIPS16_INSN_SIZE(insn.meta.opcode)))
        goto sigsegv;

    regs->regs[0] = 0;

    switch (insn.meta.opcode) {
#ifdef __BIG_ENDIAN

        case mips16_op_jal:
            regs->regs[31] = epc + 4 + MIPS16_INSN_SIZE(next_insn.meta.opcode);
            epc = (epc+4) & ~((1 << 28) - 1);
            epc |= ( insn.j_format.target0 | 
                    (insn.j_format.target1 << 16) | 
                    (insn.j_format.target2 << 21)) << 2;

            if(unaligned_action & UNALIGNED_WARN) {
                printk(KERN_ERR "[%s] JAL%s 0x%x  =>  GPR=0x%lx, EPC=0x%lx [word=0x%x] at 0x%lx\n", __FUNCTION__,
                        insn.j_format.x_mips16 ? "X" : "",
                        insn.j_format.target0 | (insn.j_format.target1 << 16) | (insn.j_format.target2 << 21),
                        regs->regs[31], epc | (0x1 ^ insn.j_format.x_mips16), insn.word, epc);
            }
            regs->cp0_epc = epc | (0x1 ^ insn.j_format.x_mips16);
            break;

        case mips16_op_rr: 
            {
                unsigned int delay_slot_size = 0;
                if(insn.rr_format.offset != mips16_rr_jr)
                    goto sigbus;
                switch (insn.rr_format.ry) {
                    case mips16_jr_jalr: 
                        delay_slot_size = MIPS16_INSN_SIZE(next_insn.meta.opcode);
                        // no break;
                    case mips16_jr_jalrc:
                        regs->regs[31] = epc + 2 + delay_slot_size;
                        // no break;
                    case mips16_jr_jr_rx:
                    case mips16_jr_jrc_rx:
                        regs->cp0_epc = regs->regs[MIPS16_REG(insn.rr_format.rx)];
                        if(unaligned_action & UNALIGNED_WARN) {
                            printk(KERN_ERR "[%s] JR(%d) r%d  =>  JR 0x%lx GPR=0x%lx delay_slot_size=%d [word=0x%x] at 0x%lx\n", __FUNCTION__,
                                   insn.rr_format.ry, insn.rr_format.rx, regs->regs[MIPS16_REG(insn.rr_format.rx)], regs->regs[31], delay_slot_size, insn.word, epc);
                        }
                        break;
                    case mips16_jr_jr_ra:
                    case mips16_jr_jrc_ra:
                        regs->cp0_epc = regs->regs[31];
                        if(unaligned_action & UNALIGNED_WARN) {
                            printk(KERN_ERR "[%s] JR(%d) ra  =>  JR 0x%lx [word=0x%x] at 0x%lx\n", __FUNCTION__, 
                                   insn.rr_format.ry, regs->regs[31], insn.word, epc);
                        }
                        break;

                    default: goto sigbus;
                }
            }
            break;

        case mips16_op_beqz: 
            test = 1;
            // no break;
        case mips16_op_bnez:
            get_mips16_rx(&insn, &rx, &offset, 0);
            if(regs->regs[MIPS16_REG(insn.rr_format.rx)] ^ test)
                regs->cp0_epc += (offset << 1) + MIPS16_INSN_SIZE(insn.meta.opcode);
            break;

        case mips16_op_b:
            get_mips16_offset(&insn, &offset);
            regs->cp0_epc += (offset << 1) + MIPS16_INSN_SIZE(insn.meta.opcode);
            break;

        case mips16_op_i8:
            switch (insn.rr_format.rx) {
                case mips16_i8_bteqz:
                    test = 1;
                    // no break;
                case mips16_i8_btnez:
                    get_mips16_rx(&insn, &rx, &offset, 0);
                    if(regs->regs[24] ^ test)
                        regs->cp0_epc += (offset << 1) + MIPS16_INSN_SIZE(insn.meta.opcode);
                    break;
                default: goto sigbus;
            }
            break;
#endif /*--- #ifdef __BIG_ENDIAN ---*/

        default: goto sigbus;
    }

    return 0;

sigbus:
    printk(KERN_ERR "[%s] ERROR - no unaligned handling for instruction 0x%x (opcode=%d%s) at address 0x%x\n",
            __FUNCTION__,  insn.word, insn.meta.opcode, insn.meta.is_extended ? ", extended" : "", (unsigned int)epc);
    force_sig(SIGBUS, current);
    return -EFAULT;
sigsegv:
    printk("[%s] %s: ERROR on reading instruction - sending SIGSEGV.\n", __FUNCTION__, current->comm);
    force_sig(SIGSEGV, current);
    return -EFAULT;
}
