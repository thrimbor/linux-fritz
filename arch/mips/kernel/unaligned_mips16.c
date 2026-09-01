/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2011 AVM GmbH <fritzbox_info@avm.de>
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
 *   
 *
 *   Handle unaligned accesses in mips16 mode by emulation.
\*------------------------------------------------------------------------------------------*/


#include <linux/mm.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <asm/asm.h>
#include <asm/branch.h>
#include <asm/byteorder.h>
#include <asm/inst.h>
#include <asm/inst_mips16.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/proc_fs.h>
#include "unaligned.h"

#define REG_SP 29
#define REG_RA 31

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void emulate_load_store_insn_mips16(struct pt_regs *regs,
	void __user *addr, unsigned int __user *pc)
{
	union mips16_instruction insn;
    char inst_buf[16];

#ifdef __BIG_ENDIAN
    short offset, rx, ry;
    unsigned char *base, *val;
    unsigned int dest = 0;
    unsigned int res;
#endif
	siginfo_t info;

    inst_buf[0]   = 0;
	regs->regs[0] = 0;

    if(read_mips16_insn(&insn, (unsigned int)pc & ~0x1 ))
        goto sigsegv;

    if(unaligned_action & UNALIGNED_WARN) {
        if(MIPS16_INSN_SIZE(insn.meta.opcode) > 2) {
            sprintf(inst_buf, " 0x%04x 0x%04x", insn.halfword[0], insn.halfword[1]);
        } else {
            sprintf(inst_buf, " 0x%04x", insn.halfword[0]);
        }
    }
    switch (insn.meta.opcode) {
#ifdef __BIG_ENDIAN
        case mips16_op_lwsp: /*--- load word SP-relative ---*/
        case mips16_op_lwpc: /*--- load word PC-relative ---*/
            get_mips16_rx(&insn, &rx, &offset, 2);
            if(insn.meta.opcode == mips16_op_lwsp)
                base = (unsigned char *)regs->regs[REG_SP];
            else
                base = (unsigned char *)regs->cp0_epc; /*--- in delay slot: addr of jump instruction ---*/

            if(unaligned_action & UNALIGNED_WARN) {
                printk(KERN_ERR "[%s] %s:LW%s%s r%d, 0x%x ( %s ) #   0x%lx = *(unsigned int *)(0x%x + 0x%x)\n", __FUNCTION__,
                         inst_buf,
                        (insn.meta.opcode == mips16_op_lwsp) ? "SP" : "PC",
                        insn.meta.is_extended ? "_EXT" : "", rx,  offset, 
                        (insn.meta.opcode == mips16_op_lwsp) ? "SP" : "PC",
                        regs->regs[rx], (unsigned int)base,  offset);
            }

            if (!access_ok(VERIFY_READ, addr, 4))
                goto sigbus;

            /*--- dest |= base[offset + 0] << 24; ---*/
            /*--- dest |= base[offset + 1] << 16; ---*/
            /*--- dest |= base[offset + 2] <<  8; ---*/
            /*--- dest |= base[offset + 3] <<  0; ---*/

            __asm__ __volatile__ (".set\tnoat\n"
                    "1:\tlbu\t$1, 0(%2)\n"
                    "sll\t$1, 0x18\n\t"
                    "or\t%0, $1\n\t"
                    "2:\tlbu\t$1, 1(%2)\n\t"
                    "sll\t$1, 0x10\n\t"
                    "or\t%0, $1\n\t"
                    "3:\tlbu\t$1, 2(%2)\n\t"
                    "sll\t$1, 0x8\n\t"
                    "or\t%0, $1\n\t"
                    "4:\tlbu\t$1, 3(%2)\n\t"
                    "or\t%0, $1\n\t"
                    "li\t%1, 0\n"
                    "5:\t.set\tat\n\t"
                    ".section\t.fixup,\"ax\"\n\t"
                    "6:\tli\t%1, %3\n\t"
                    "j\t5b\n\t"
                    ".previous\n\t"
                    ".section\t__ex_table,\"a\"\n\t"
                    STR(PTR)"\t1b, 6b\n\t"
                    STR(PTR)"\t2b, 6b\n\t"
                    STR(PTR)"\t3b, 6b\n\t"
                    STR(PTR)"\t4b, 6b\n\t"
                    ".previous"
                    : "+r" (dest), "=r" (res)
                    : "r" (&base[offset]), "i" (-EFAULT));
            if (res)
                goto fault;

            if((insn.meta.opcode == mips16_op_lwpc) && (((int)base+offset) & 0x11)) {
                /*--- => XXX auch wenn (!insn.meta.is_extended)? Oder dann nur unteren 2 Bits löschen? ---*/
                goto sigbus;
            }

            regs->regs[rx] = dest;

            compute_return_epc(regs);
            break;

        case mips16_op_lw:
            get_mips16_rxy(&insn, &rx, &ry, &offset, 2);
            base  = (unsigned char *)(regs->regs[rx]);

            if (!access_ok(VERIFY_READ, addr, 4))
                goto sigbus;

            /*--- dest |= base[offset + 0] << 24; ---*/
            /*--- dest |= base[offset + 1] << 16; ---*/
            /*--- dest |= base[offset + 2] <<  8; ---*/
            /*--- dest |= base[offset + 3] <<  0; ---*/

            __asm__ __volatile__ (".set\tnoat\n"
                    "1:\tlbu\t$1, 0(%2)\n"
                    "sll\t$1, 0x18\n\t"
                    "or\t%0, $1\n\t"
                    "2:\tlbu\t$1, 1(%2)\n\t"
                    "sll\t$1, 0x10\n\t"
                    "or\t%0, $1\n\t"
                    "3:\tlbu\t$1, 2(%2)\n\t"
                    "sll\t$1, 0x8\n\t"
                    "or\t%0, $1\n\t"
                    "4:\tlbu\t$1, 3(%2)\n\t"
                    "or\t%0, $1\n\t"
                    "li\t%1, 0\n"
                    "5:\t.set\tat\n\t"
                    ".section\t.fixup,\"ax\"\n\t"
                    "6:\tli\t%1, %3\n\t"
                    "j\t5b\n\t"
                    ".previous\n\t"
                    ".section\t__ex_table,\"a\"\n\t"
                    STR(PTR)"\t1b, 6b\n\t"
                    STR(PTR)"\t2b, 6b\n\t"
                    STR(PTR)"\t3b, 6b\n\t"
                    STR(PTR)"\t4b, 6b\n\t"
                    ".previous"
                    : "+r" (dest), "=r" (res)
                    : "r" (&base[offset]), "i" (-EFAULT));

            if(unaligned_action & UNALIGNED_WARN) {
                printk(KERN_ERR "[%s] %s:LW%s r%d, 0x%x ( r%d ) #   0x%lx = *(unsigned int *)(0x%x + 0x%x)\n", __FUNCTION__,  
                         inst_buf,
                        insn.meta.is_extended ? "_EXT" : "", ry, offset, rx, regs->regs[ry], (unsigned int)base,  offset);
            }

            if (res)
                goto fault;
            regs->regs[ry] = dest;

            compute_return_epc(regs);
            break;

        case mips16_op_lh:
        case mips16_op_lhu:
            get_mips16_rxy(&insn, &rx, &ry, &offset, 1);
            base  = (unsigned char *)(regs->regs[rx]);

            /*--- dest |= base[offset + 0] << 8; ---*/
            /*--- dest |= base[offset + 1] << 0; ---*/

            if (!access_ok(VERIFY_READ, addr, 2))
                goto sigbus;

            __asm__ __volatile__ (".set\tnoat\n"
                    "1:\tlbu\t$1, 0(%2)\n"
                    "sll\t$1, 0x8\n\t"
                    "or\t%0, $1\n\t"
                    "2:\tlbu\t$1, 1(%2)\n\t"
                    "or\t%0, $1\n\t"
                    "li\t%1, 0\n"
                    "3:\t.set\tat\n\t"
                    ".section\t.fixup,\"ax\"\n\t"
                    "4:\tli\t%1, %3\n\t"
                    "j\t3b\n\t"
                    ".previous\n\t"
                    ".section\t__ex_table,\"a\"\n\t"
                    STR(PTR)"\t1b, 4b\n\t"
                    STR(PTR)"\t2b, 4b\n\t"
                    ".previous"
                    : "+r" (dest), "=r" (res)
                    : "r" (&base[offset]), "i" (-EFAULT));

            if (res)
                goto fault;
            regs->regs[ry] = (insn.meta.opcode == mips16_op_lh) ? (short)dest : (unsigned short)dest;
            if(unaligned_action & UNALIGNED_WARN) {
                printk(KERN_ERR "[%s] %s: LH%s r%d, 0x%x ( r%d ) #   0x%lx = *(unsigned int *)(0x%x + 0x%x)\n", __FUNCTION__,  
                         inst_buf,
                        insn.meta.is_extended ? "_EXT" : "", ry, offset, rx, regs->regs[ry], (unsigned int)base,  offset);
            }
            compute_return_epc(regs);
            break;

        case mips16_op_sh:
            get_mips16_rxy(&insn, &rx, &ry, &offset, 1);
            base = (unsigned char *)(regs->regs[rx]);
            val  = (unsigned char *)&(regs->regs[ry]);
            val += 2;  /* big endian, offset in word for lower-halfword */

            if(unaligned_action & UNALIGNED_WARN) {
                printk(KERN_ERR "[%s] %s: SH%s r%d, 0x%x ( r%d ) #   *(unsigned int *)(0x%x + 0x%x) = 0x%lx\n", __FUNCTION__,  
                         inst_buf,
                        insn.meta.is_extended ? "_EXT" : "", ry, offset, rx, (unsigned int)base,  offset, regs->regs[ry]);
            }

            if (!access_ok(VERIFY_WRITE, addr, 2))
                goto sigbus;

            /*--- base[offset + 0] = val[0]; ---*/
            /*--- base[offset + 1] = val[1]; ---*/

            __asm__ __volatile__ (".set\tnoat\n"
                    "lbu\t$1, 0(%3)\n"
                    "1:\tsb\t$1, 0(%1)\n"
                    "lbu\t$1, 1(%3)\n"
                    "2:\tsb\t$1, 1(%1)\n"
                    "li\t%0, 0\n"
                    "3:\t.set\tat\n\t"
                    ".section\t.fixup,\"ax\"\n\t"
                    "4:\tli\t%0, %2\n\t"
                    "j\t3b\n\t"
                    ".previous\n\t"
                    ".section\t__ex_table,\"a\"\n\t"
                    STR(PTR)"\t1b, 4b\n\t"
                    STR(PTR)"\t2b, 4b\n\t"
                    ".previous"
                    : "=r" (res) 
                    : "r" (&base[offset]), "i" (-EFAULT), "r" (&val[0]));
            if (res)
                goto fault;
            compute_return_epc(regs);
            break;

        case mips16_op_sw:
            get_mips16_rxy(&insn, &rx, &ry, &offset, 2);
            base = (unsigned char *)(regs->regs[rx]);
            val  = (unsigned char *)&(regs->regs[ry]);

            if(unaligned_action & UNALIGNED_WARN) {
                printk(KERN_ERR "[%s] %s: SW%s r%d, 0x%x ( r%d ) #   *(unsigned int *)(0x%x + 0x%x) = 0x%lx\n", __FUNCTION__,  
                         inst_buf,
                        insn.meta.is_extended ? "_EXT" : "", ry, offset, rx, (unsigned int)base,  offset, regs->regs[ry]);
            }

            if (!access_ok(VERIFY_WRITE, addr, 4))
                goto sigbus;

            /*--- base[offset + 0] = val[0]; ---*/
            /*--- base[offset + 1] = val[1]; ---*/
            /*--- base[offset + 2] = val[2]; ---*/
            /*--- base[offset + 3] = val[3]; ---*/

            __asm__ __volatile__ (".set\tnoat\n"
                    "lbu\t$1, 0(%3)\n"
                    "1:\tsb\t$1, 0(%1)\n"
                    "lbu\t$1, 1(%3)\n"
                    "2:\tsb\t$1, 1(%1)\n"
                    "lbu\t$1, 2(%3)\n"
                    "3:\tsb\t$1, 2(%1)\n"
                    "lbu\t$1, 3(%3)\n"
                    "4:\tsb\t$1, 3(%1)\n"
                    "li\t%0, 0\n"
                    "5:\t.set\tat\n\t"
                    ".section\t.fixup,\"ax\"\n\t"
                    "6:\tli\t%0, %2\n\t"
                    "j\t5b\n\t"
                    ".previous\n\t"
                    ".section\t__ex_table,\"a\"\n\t"
                    STR(PTR)"\t1b, 6b\n\t"
                    STR(PTR)"\t2b, 6b\n\t"
                    STR(PTR)"\t3b, 6b\n\t"
                    STR(PTR)"\t4b, 6b\n\t"
                    ".previous"
                    : "=r" (res) 
                    : "r" (&base[offset]), "i" (-EFAULT), "r" (&val[0]));
            if (res)
                goto fault;
            compute_return_epc(regs);
            break;

        case mips16_op_i8: 
            if((insn.meta.is_extended && (insn.ext_rr_format.rx != mips16_i8_swrasp)) ||
                    (insn.rr_format.rx != mips16_i8_swrasp)) {
                goto sigbus;
            }

        case mips16_op_swsp: /*--- store word SP-relative ---*/
            get_mips16_rx(&insn, &rx, &offset, 2);
            if(insn.meta.opcode == mips16_op_lwsp)
                rx = regs->regs[REG_RA]; /*--- SWRASP: store RA-Register SP-relatve---*/ 
            base = (unsigned char *)(regs->regs[REG_SP]);
            val  = (unsigned char *)&(regs->regs[rx]);

            if(unaligned_action & UNALIGNED_WARN) {
                printk(KERN_ERR "[%s] %s: SWSP%s r%d, 0x%x ( SP ) #   *(unsigned int *)(0x%x + 0x%x) = 0x%lx\n", __FUNCTION__,  
                         inst_buf,
                        insn.meta.is_extended ? "_EXT" : "", rx, offset, (unsigned int)base,  offset, regs->regs[rx]);
            }


            if (!access_ok(VERIFY_WRITE, addr, 4))
                goto sigbus;

            /*--- base[offset + 0] = val[0]; ---*/
            /*--- base[offset + 1] = val[1]; ---*/
            /*--- base[offset + 2] = val[2]; ---*/
            /*--- base[offset + 3] = val[3]; ---*/
            __asm__ __volatile__ (".set\tnoat\n"
                    "lbu\t$1, 0(%3)\n"
                    "1:\tsb\t$1, 0(%1)\n"
                    "lbu\t$1, 1(%3)\n"
                    "2:\tsb\t$1, 1(%1)\n"
                    "lbu\t$1, 2(%3)\n"
                    "3:\tsb\t$1, 2(%1)\n"
                    "lbu\t$1, 3(%3)\n"
                    "4:\tsb\t$1, 3(%1)\n"
                    "li\t%0, 0\n"
                    "5:\t.set\tat\n\t"
                    ".section\t.fixup,\"ax\"\n\t"
                    "6:\tli\t%0, %2\n\t"
                    "j\t5b\n\t"
                    ".previous\n\t"
                    ".section\t__ex_table,\"a\"\n\t"
                    STR(PTR)"\t1b, 6b\n\t"
                    STR(PTR)"\t2b, 6b\n\t"
                    STR(PTR)"\t3b, 6b\n\t"
                    STR(PTR)"\t4b, 6b\n\t"
                    ".previous"
                    : "=r" (res) 
                    : "r" (&base[offset]), "i" (-EFAULT), "r" (&val[0]));

            if (res)
                goto fault;
            compute_return_epc(regs);
            break;

#endif /*--- #ifdef __BIG_ENDIAN ---*/
        default: goto sigbus;
    }

#ifdef CONFIG_DEBUG_FS
	unaligned_instructions++;
#endif

	return;

#ifdef __BIG_ENDIAN
fault: 
	/*--- Did we have an exception handler installed? ---*/
	if (fixup_exception(regs))
		return;

	die_if_kernel("Unhandled kernel unaligned access", regs);

    info.si_signo = SIGSEGV;
    info.si_errno = 0;
    info.si_code = SEGV_MAPERR;
    info.si_addr = (void __user *) addr;
	force_sig_info(SIGSEGV, &info, current);

	return;
#endif /*--- #ifdef __BIG_ENDIAN ---*/

sigbus:
    if(unaligned_action & UNALIGNED_WARN) {
        printk(KERN_ERR "[%s] ERROR - no unaligned handling for instruction 0x%x (opcode=%d%s) at address 0x%x\n",
                __FUNCTION__,  insn.word, insn.meta.opcode, insn.meta.is_extended ? ", extended" : "", (unsigned int)pc);
    }
	die_if_kernel("Unhandled kernel unaligned access", regs);
    info.si_signo = SIGBUS;
    info.si_errno = 0;
    info.si_code = BUS_ADRERR;
    info.si_addr = (void __user *) addr;
	force_sig_info(SIGBUS, &info, current);

	return;

sigsegv:
    if(unaligned_action & UNALIGNED_WARN) {
        printk("[%s] %s: ERROR on decoding instruction - sending SIGSEGV.\n", __FUNCTION__, current->comm);
    }
    info.si_signo = SIGSEGV;
    info.si_errno = 0;
    info.si_code = SEGV_MAPERR;
    info.si_addr = (void __user *) addr;
    force_sig_info(SIGSEGV, &info, current);
	return;

/*--- sigill: ---*/
	/*--- die_if_kernel("Unhandled kernel unaligned access or invalid instruction", regs); ---*/
	/*--- send_sig(SIGILL, current, 1); ---*/

}


