/*
 * Handle unaligned accesses by emulation.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1998, 1999, 2002 by Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 *
 * This file contains exception handler for address error exception with the
 * special capability to execute faulting instructions in software.  The
 * handler does not try to handle the case when the program counter points
 * to an address not aligned to a word boundary.
 *
 * Putting data to unaligned addresses is a bad practice even on Intel where
 * only the performance is affected.  Much worse is that such code is non-
 * portable.  Due to several programs that die on MIPS due to alignment
 * problems I decided to implement this handler anyway though I originally
 * didn't intend to do this at all for user code.
 *
 * For now I enable fixing of address errors by default to make life easier.
 * I however intend to disable this somewhen in the future when the alignment
 * problems with user programs have been fixed.  For programmers this is the
 * right way to go.
 *
 * Fixing address errors is a per process option.  The option is inherited
 * across fork(2) and execve(2) calls.  If you really want to use the
 * option in your user programs - I discourage the use of the software
 * emulation strongly - use the following code in your userland stuff:
 *
 * #include <sys/sysmips.h>
 *
 * ...
 * sysmips(MIPS_FIXADE, x);
 * ...
 *
 * The argument x is 0 for disabling software emulation, enabled otherwise.
 *
 * Below a little program to play around with this feature.
 *
 * #include <stdio.h>
 * #include <sys/sysmips.h>
 *
 * struct foo {
 *         unsigned char bar[8];
 * };
 *
 * main(int argc, char *argv[])
 * {
 *         struct foo x = {0, 1, 2, 3, 4, 5, 6, 7};
 *         unsigned int *p = (unsigned int *) (x.bar + 3);
 *         int i;
 *
 *         if (argc > 1)
 *                 sysmips(MIPS_FIXADE, atoi(argv[1]));
 *
 *         printf("*p = %08lx\n", *p);
 *
 *         *p = 0xdeadface;
 *
 *         for(i = 0; i <= 7; i++)
 *         printf("%02x ", x.bar[i]);
 *         printf("\n");
 * }
 *
 * Coprocessor loads are not supported; I think this case is unimportant
 * in the practice.
 *
 * TODO: Handle ndc (attempted store to doubleword in uncached memory)
 *       exception for the R6000.
 *       A store crossing a page boundary might be executed only partially.
 *       Undo the partial store in this case.
 */
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

#define STR(x)  __STR(x)
#define __STR(x)  #x

static unsigned long ai_user;
static unsigned long ai_sys;
static unsigned long ai_skipped;
static unsigned long ai_word;
static unsigned long ai_dword;


int ai_usermode = UNALIGNED_ACTION_FIXUP 
                  /*--- | UNALIGNED_WARN ---*/
                                ;
int ai_kernelmode = UNALIGNED_ACTION_FIXUP 
                  /*---   | UNALIGNED_WARN ---*/
                   ;
#ifdef CONFIG_DEBUG_FS
u32 unaligned_instructions;
#endif

extern void show_registers(struct pt_regs *regs);

static void emulate_load_store_insn(struct pt_regs *regs,
	void __user *addr, unsigned int __user *pc)
{
	union mips_instruction insn;
	unsigned long value;
	unsigned int res;
	siginfo_t info;

	regs->regs[0] = 0;

    /*--------------------------------------------------------------------------------------*\
     * An address error exception occurs on an instruction or data access when an attempt 
     * is made to execute one of the following:
     *
        • Fetch an instruction, load a word, or store a word that is not aligned on a word boundary
        • Load or store a halfword that is not aligned on a halfword boundary
        • Reference the kernel address space from user mode

     * Note that in the case of an instruction fetch that is not aligned on a word boundary,
     * PC is updated before the condition is detected. Therefore, both EPC and BadVAddr point 
     * to the unaligned instruction address. In the case of a data access the exception is 
     * taken if either an unaligned address or an address that was inaccessible in the 
     * current processor mode was referenced by a load or store instruction.
    \*--------------------------------------------------------------------------------------*/
    if (user_mode(regs) && !unlikely(access_ok(VERIFY_READ, pc, 4))) {
        printk(KERN_ERR "[%s] illegal address 0x%p (sigill)\n", __FUNCTION__, pc);
		goto sigill;
    }

    if(__get_user(insn.word, pc)) {
        printk(KERN_ERR "[%s] load from address 0x%p failed (sigbus)\n", __FUNCTION__, pc);
        goto sigbus;
    }

	switch (insn.i_format.opcode) {
	/*
	 * These are instructions that a compiler doesn't generate.  We
	 * can assume therefore that the code is MIPS-aware and
	 * really buggy.  Emulating these instructions would break the
	 * semantics anyway.
	 */
	case ll_op:
	case lld_op:
	case sc_op:
	case scd_op:

	/*
	 * For these instructions the only way to create an address
	 * error is an attempted access to kernel/supervisor address
	 * space.
	 */
	case ldl_op:
	case ldr_op:
	case lwl_op:
	case lwr_op:
	case sdl_op:
	case sdr_op:
	case swl_op:
	case swr_op:
	case lb_op:
	case lbu_op:
	case sb_op:
		goto sigbus;

        /* 
         * DSP instructions
         */
    case spec3_op:
        /*--- printk("[%s] special opcode 3 found\n", __FUNCTION__); ---*/
	    switch (insn.sp3_format.sp3_opcode) {
            case lxx_op:
                    /*--- if(unaligned_action & UNALIGNED_WARN) { ---*/
                /*--- printk(KERN_INFO "[%s] opcode group LX found opcode=0x%x base=%d index=%d rd=%d lx_opcode=0x%x sp3_opcode=0x%x at 0x%x\n", ---*/ 
                        /*--- __FUNCTION__, ---*/
                        /*--- insn.lx_format.opcode, insn.lx_format.base, insn.lx_format.index, insn.lx_format.rd, ---*/
                        /*--- insn.lx_format.lx_opcode, insn.lx_format.sp3_opcode, regs->cp0_epc); ---*/
                    /*--- } ---*/
	            switch (insn.lx_format.lx_opcode) {
                    case lwx_op: {
                                     unsigned int dest = 0;
                                     unsigned char *base  = (unsigned char *)(regs->regs[insn.lx_format.base]);
                                     unsigned int index   = regs->regs[insn.lx_format.index];

                                         if (user_mode(regs) && !access_ok(VERIFY_READ, addr, 4))
                                         goto sigbus;

                                     __asm__ __volatile__ (".set\tnoat\n"
#ifdef __BIG_ENDIAN
                                     /*--- dest |= base[index + 0] << 24; ---*/
                                     /*--- dest |= base[index + 1] << 16; ---*/
                                     /*--- dest |= base[index + 2] <<  8; ---*/
                                     /*--- dest |= base[index + 3] <<  0; ---*/
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
#endif
#ifdef __LITTLE_ENDIAN
                                     /*--- dest |= base[index + 3] << 24; ---*/
                                     /*--- dest |= base[index + 2] << 16; ---*/
                                     /*--- dest |= base[index + 1] <<  8; ---*/
                                     /*--- dest |= base[index + 0] <<  0; ---*/
                                             "1:\tlbu\t$1, 3(%2)\n"
                                             "sll\t$1, 0x18\n\t"
                                             "or\t%0, $1\n\t"
                                             "2:\tlbu\t$1, 2(%2)\n\t"
                                             "sll\t$1, 0x10\n\t"
                                             "or\t%0, $1\n\t"
                                             "3:\tlbu\t$1, 1(%2)\n\t"
                                             "sll\t$1, 0x8\n\t"
                                             "or\t%0, $1\n\t"
                                             "4:\tlbu\t$1, 0(%2)\n\t"
                                             "or\t%0, $1\n\t"
#endif
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
                                             : "r" (&base[index]), "i" (-EFAULT));
                                     if (res)
                                         goto fault;
                                     regs->regs[insn.lx_format.rd] = dest;
                                         if(unaligned_action & UNALIGNED_WARN) {
                                             printk(KERN_INFO "[%s] LWX r%d, r%d ( r%d ) #   0x%lx = *(unsigned int *)(0x%lx + 0x%lx)\n",
                                                     __FUNCTION__, 
                                                     insn.lx_format.rd, insn.lx_format.index, insn.lx_format.base,
                                                     regs->regs[insn.lx_format.rd], regs->regs[insn.lx_format.index], 
                                                     regs->regs[insn.lx_format.base]);
                                 }
                                     }
		                         compute_return_epc(regs);
                                 break;
                    case lhx_op: {
                                     unsigned int dest = 0;
                                     unsigned char *base  = (unsigned char *)(regs->regs[insn.lx_format.base]);
                                     unsigned int index   = regs->regs[insn.lx_format.index];

                                         if (user_mode(regs) && !access_ok(VERIFY_READ, addr, 2))
                                         goto sigbus;

                                     __asm__ __volatile__ (".set\tnoat\n"
#ifdef __BIG_ENDIAN
                                     /*--- dest |= base[index + 0] <<  8; ---*/
                                     /*--- dest |= base[index + 1] <<  0; ---*/
                                             "1:\tlbu\t$1, 0(%2)\n"
                                             "sll\t$1, 0x8\n\t"
                                             "or\t%0, $1\n\t"
                                             "2:\tlbu\t$1, 1(%2)\n\t"
                                             "or\t%0, $1\n\t"
#endif
#ifdef __LITTLE_ENDIAN
                                     /*--- dest |= base[index + 1] <<  8; ---*/
                                     /*--- dest |= base[index + 0] <<  0; ---*/
                                             "1:\tlbu\t$1, 1(%2)\n"
                                             "sll\t$1, 0x8\n\t"
                                             "or\t%0, $1\n\t"
                                             "2:\tlbu\t$1, 0(%2)\n\t"
                                             "or\t%0, $1\n\t"
#endif
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
                                             : "r" (&base[index]), "i" (-EFAULT));
                                     if (res)
                                         goto fault;
                                     regs->regs[insn.lx_format.rd] = dest;
                                         if(unaligned_action & UNALIGNED_WARN) {
                                             printk(KERN_INFO "[%s] LHX r%d, r%d ( r%d ) #   0x%lx = (unsigned int)*(unsigned short *)(0x%lx + 0x%lx)\n",
                                                     __FUNCTION__, 
                                                     insn.lx_format.rd, insn.lx_format.index, insn.lx_format.base,
                                                     regs->regs[insn.lx_format.rd], regs->regs[insn.lx_format.index], 
                                                     regs->regs[insn.lx_format.base]);
                                 }
                                     }
		                         compute_return_epc(regs);
                                 break;

                    case lbux_op: /*--- 8 Bit zugriffe können nicht analigned sein ---*/
                    default:
                        printk(KERN_ERR "[%s] opcode group LX found, unknown element 0x%x.\n", __FUNCTION__, insn.lx_format.lx_opcode);
                         goto sigbus;
                }
                break;

            case ext_op:
            case dextm_op:
            case dextu_op:
            case dext_op:
            case ins_op:
            case dinsm_op:
            case dinsu_op:
            case dins_op:
            case insv_op:
            case adduqb_op:
            case bshfl_op:
            case dbshfl_op:
            case extrwph_op:
            case rdhwr_op:
            default:
                    goto sigbus;
        }
        break;
        /*
         * The remaining opcodes are the ones that are really of interest.
         */
    case lh_op:
            if (user_mode(regs) && !access_ok(VERIFY_READ, addr, 2))
            goto sigbus;
         ai_word++;
		__asm__ __volatile__ (".set\tnoat\n"
#ifdef __BIG_ENDIAN
			"1:\tlb\t%0, 0(%2)\n"
			"2:\tlbu\t$1, 1(%2)\n\t"
#endif
#ifdef __LITTLE_ENDIAN
			"1:\tlb\t%0, 1(%2)\n"
			"2:\tlbu\t$1, 0(%2)\n\t"
#endif
			"sll\t%0, 0x8\n\t"
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
			: "=&r" (value), "=r" (res)
			: "r" (addr), "i" (-EFAULT));
		if (res)
			goto fault;
		compute_return_epc(regs);
		regs->regs[insn.i_format.rt] = value;
		break;

	case lw_op:
            if (user_mode(regs) && !access_ok(VERIFY_READ, addr, 4))
			goto sigbus;
         ai_dword++;
		__asm__ __volatile__ (
#ifdef __BIG_ENDIAN
#ifdef CONFIG_MACH_FUSIV_MIPS1
			"\tli\t %1, 1\n\t"
			"1:\tlbu\t%0, (%2)\n"
			"2:\tlbu\t$23, 1(%2)\n\t"
			"5:\tlbu\t$22, 2(%2)\n\t"
			"6:\tlbu\t$21, 3(%2)\n\t"
#else
			"1:\tlwl\t%0, (%2)\n"
			"2:\tlwr\t%0, 3(%2)\n\t"
#endif
#endif
#ifdef __LITTLE_ENDIAN
			"1:\tlwl\t%0, 3(%2)\n"
			"2:\tlwr\t%0, (%2)\n\t"
#endif
#ifdef CONFIG_MACH_FUSIV_MIPS1
			"sll\t%0,0x8\n\t"
			"or\t%0,$23\n\t"
			"sll\t%0,0x8\n\t"
			"or\t%0,$22\n\t"
			"sll\t%0,0x8\n\t"
			"or\t%0,$21\n\t"
#endif
			"li\t%1, 0\n"
#ifdef CONFIG_MACH_FUSIV_MIPS1
			"3:\t.set\tat\n\t"
			"\t.section\t.fixup,\"ax\"\n\t"
#else
			"3:\t.section\t.fixup,\"ax\"\n\t"
#endif
			"4:\tli\t%1, %3\n\t"
			"j\t3b\n\t"
			".previous\n\t"
			".section\t__ex_table,\"a\"\n\t"
			STR(PTR)"\t1b, 4b\n\t"
			STR(PTR)"\t2b, 4b\n\t"
#ifdef CONFIG_MACH_FUSIV_MIPS1
			STR(PTR)"\t5b, 4b\n\t"
			STR(PTR)"\t6b, 4b\n\t"
#endif
			".previous"
			: "=&r" (value), "=r" (res)
#ifdef CONFIG_MACH_FUSIV_MIPS1
			: "r" (addr), "i" (-EFAULT)
			:"$23","$22","$21");
#else
			: "r" (addr), "i" (-EFAULT));
#endif
		if (res)
			goto fault;

		compute_return_epc(regs);
		regs->regs[insn.i_format.rt] = value;
		break;

	case lhu_op:
            if (user_mode(regs) && !access_ok(VERIFY_READ, addr, 2))
			goto sigbus;
         ai_word++;
		__asm__ __volatile__ (
			".set\tnoat\n"
#ifdef __BIG_ENDIAN
			"1:\tlbu\t%0, 0(%2)\n"
			"2:\tlbu\t$1, 1(%2)\n\t"
#endif
#ifdef __LITTLE_ENDIAN
			"1:\tlbu\t%0, 1(%2)\n"
			"2:\tlbu\t$1, 0(%2)\n\t"
#endif
			"sll\t%0, 0x8\n\t"
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
			: "=&r" (value), "=r" (res)
			: "r" (addr), "i" (-EFAULT));
		if (res)
			goto fault;
		compute_return_epc(regs);
		regs->regs[insn.i_format.rt] = value;
		break;

	case lwu_op:
#ifdef CONFIG_64BIT
		/*
		 * A 32-bit kernel might be running on a 64-bit processor.  But
		 * if we're on a 32-bit processor and an i-cache incoherency
		 * or race makes us see a 64-bit instruction here the sdl/sdr
		 * would blow up, so for now we don't handle unaligned 64-bit
		 * instructions on 32-bit kernels.
		 */
            if (user_mode(regs) && !access_ok(VERIFY_READ, addr, 4))
			goto sigbus;

		__asm__ __volatile__ (
#ifdef __BIG_ENDIAN
			"1:\tlwl\t%0, (%2)\n"
			"2:\tlwr\t%0, 3(%2)\n\t"
#endif
#ifdef __LITTLE_ENDIAN
			"1:\tlwl\t%0, 3(%2)\n"
			"2:\tlwr\t%0, (%2)\n\t"
#endif
			"dsll\t%0, %0, 32\n\t"
			"dsrl\t%0, %0, 32\n\t"
			"li\t%1, 0\n"
			"3:\t.section\t.fixup,\"ax\"\n\t"
			"4:\tli\t%1, %3\n\t"
			"j\t3b\n\t"
			".previous\n\t"
			".section\t__ex_table,\"a\"\n\t"
			STR(PTR)"\t1b, 4b\n\t"
			STR(PTR)"\t2b, 4b\n\t"
			".previous"
			: "=&r" (value), "=r" (res)
			: "r" (addr), "i" (-EFAULT));
		if (res)
			goto fault;
		compute_return_epc(regs);
		regs->regs[insn.i_format.rt] = value;
		break;
#endif /* CONFIG_64BIT */

		/* Cannot handle 64-bit instructions in 32-bit kernel */
		goto sigill;

	case ld_op:
#ifdef CONFIG_64BIT
		/*
		 * A 32-bit kernel might be running on a 64-bit processor.  But
		 * if we're on a 32-bit processor and an i-cache incoherency
		 * or race makes us see a 64-bit instruction here the sdl/sdr
		 * would blow up, so for now we don't handle unaligned 64-bit
		 * instructions on 32-bit kernels.
		 */
            if (user_mode(regs) && !access_ok(VERIFY_READ, addr, 8))
			goto sigbus;

		__asm__ __volatile__ (
#ifdef __BIG_ENDIAN
			"1:\tldl\t%0, (%2)\n"
			"2:\tldr\t%0, 7(%2)\n\t"
#endif
#ifdef __LITTLE_ENDIAN
			"1:\tldl\t%0, 7(%2)\n"
			"2:\tldr\t%0, (%2)\n\t"
#endif
			"li\t%1, 0\n"
			"3:\t.section\t.fixup,\"ax\"\n\t"
			"4:\tli\t%1, %3\n\t"
			"j\t3b\n\t"
			".previous\n\t"
			".section\t__ex_table,\"a\"\n\t"
			STR(PTR)"\t1b, 4b\n\t"
			STR(PTR)"\t2b, 4b\n\t"
			".previous"
			: "=&r" (value), "=r" (res)
			: "r" (addr), "i" (-EFAULT));
		if (res)
			goto fault;
		compute_return_epc(regs);
		regs->regs[insn.i_format.rt] = value;
		break;
#endif /* CONFIG_64BIT */

		/* Cannot handle 64-bit instructions in 32-bit kernel */
		goto sigill;

	case sh_op:
            if (user_mode(regs) && !access_ok(VERIFY_WRITE, addr, 2))
			goto sigbus;
        ai_word++;
		value = regs->regs[insn.i_format.rt];
		__asm__ __volatile__ (
#ifdef __BIG_ENDIAN
			".set\tnoat\n"
			"1:\tsb\t%1, 1(%2)\n\t"
			"srl\t$1, %1, 0x8\n"
			"2:\tsb\t$1, 0(%2)\n\t"
			".set\tat\n\t"
#endif
#ifdef __LITTLE_ENDIAN
			".set\tnoat\n"
			"1:\tsb\t%1, 0(%2)\n\t"
			"srl\t$1,%1, 0x8\n"
			"2:\tsb\t$1, 1(%2)\n\t"
			".set\tat\n\t"
#endif
			"li\t%0, 0\n"
			"3:\n\t"
			".section\t.fixup,\"ax\"\n\t"
			"4:\tli\t%0, %3\n\t"
			"j\t3b\n\t"
			".previous\n\t"
			".section\t__ex_table,\"a\"\n\t"
			STR(PTR)"\t1b, 4b\n\t"
			STR(PTR)"\t2b, 4b\n\t"
			".previous"
			: "=r" (res)
			: "r" (value), "r" (addr), "i" (-EFAULT));
		if (res)
			goto fault;
		compute_return_epc(regs);
		break;

	case sw_op:
            if (user_mode(regs) && !access_ok(VERIFY_WRITE, addr, 4))
			goto sigbus;
        ai_dword++;
		value = regs->regs[insn.i_format.rt];
		__asm__ __volatile__ (
#ifdef __BIG_ENDIAN
#ifdef CONFIG_MACH_FUSIV_MIPS1
			"1:\tsb\t%1,3(%2)\n"
			"srl\t$23,%1,0x8\n"
			"2:\tsb\t$23, 2(%2)\n\t"
			"srl\t$23,%1,0x10\n"
			"5:\tsb\t$23, 1(%2)\n\t"
			"srl\t$23,%1,0x18\n"
			"6:\tsb\t$23, 0(%2)\n\t"
#else
			"1:\tswl\t%1,(%2)\n"
			"2:\tswr\t%1, 3(%2)\n\t"
#endif
#endif
#ifdef __LITTLE_ENDIAN
			"1:\tswl\t%1, 3(%2)\n"
			"2:\tswr\t%1, (%2)\n\t"
#endif
			"li\t%0, 0\n"
			"3:\n\t"
			".section\t.fixup,\"ax\"\n\t"
			"4:\tli\t%0, %3\n\t"
			"j\t3b\n\t"
			".previous\n\t"
			".section\t__ex_table,\"a\"\n\t"
			STR(PTR)"\t1b, 4b\n\t"
			STR(PTR)"\t2b, 4b\n\t"
#ifdef CONFIG_MACH_FUSIV_MIPS1
			STR(PTR)"\t5b, 4b\n\t"
			STR(PTR)"\t6b, 4b\n\t"
#endif
			".previous"
		: "=r" (res)
#ifdef CONFIG_MACH_FUSIV_MIPS1
		: "r" (value), "r" (addr), "i" (-EFAULT)
		: "$23");
#else
		: "r" (value), "r" (addr), "i" (-EFAULT));
#endif
		if (res)
			goto fault;

		compute_return_epc(regs);
		break;

	case sd_op:
#ifdef CONFIG_64BIT
		/*
		 * A 32-bit kernel might be running on a 64-bit processor.  But
		 * if we're on a 32-bit processor and an i-cache incoherency
		 * or race makes us see a 64-bit instruction here the sdl/sdr
		 * would blow up, so for now we don't handle unaligned 64-bit
		 * instructions on 32-bit kernels.
		 */
            if (user_mode(regs) && !access_ok(VERIFY_WRITE, addr, 8))
			goto sigbus;

		value = regs->regs[insn.i_format.rt];
		__asm__ __volatile__ (
#ifdef __BIG_ENDIAN
			"1:\tsdl\t%1,(%2)\n"
			"2:\tsdr\t%1, 7(%2)\n\t"
#endif
#ifdef __LITTLE_ENDIAN
			"1:\tsdl\t%1, 7(%2)\n"
			"2:\tsdr\t%1, (%2)\n\t"
#endif
			"li\t%0, 0\n"
			"3:\n\t"
			".section\t.fixup,\"ax\"\n\t"
			"4:\tli\t%0, %3\n\t"
			"j\t3b\n\t"
			".previous\n\t"
			".section\t__ex_table,\"a\"\n\t"
			STR(PTR)"\t1b, 4b\n\t"
			STR(PTR)"\t2b, 4b\n\t"
			".previous"
		: "=r" (res)
		: "r" (value), "r" (addr), "i" (-EFAULT));
		if (res)
			goto fault;
		compute_return_epc(regs);
		break;
#endif /* CONFIG_64BIT */

		/* Cannot handle 64-bit instructions in 32-bit kernel */
		goto sigill;

	case lwc1_op:
	case ldc1_op:
	case swc1_op:
	case sdc1_op:
		/*
		 * I herewith declare: this does not happen.  So send SIGBUS.
		 */
		goto sigbus;

	case lwc2_op:
	case ldc2_op:
	case swc2_op:
	case sdc2_op:
		/*
		 * These are the coprocessor 2 load/stores.  The current
		 * implementations don't use cp2 and cp2 should always be
		 * disabled in c0_status.  So send SIGILL.
                 * (No longer true: The Sony Praystation uses cp2 for
                 * 3D matrix operations.  Dunno if that thingy has a MMU ...)
		 */
	default:
		/*
		 * Pheeee...  We encountered an yet unknown instruction or
		 * cache coherence problem.  Die sucker, die ...
		 */
		goto sigill;
	}

#ifdef CONFIG_DEBUG_FS
	unaligned_instructions++;
#endif

	return;

fault:
	/* Did we have an exception handler installed? */
	if (fixup_exception(regs))
		return;

	die_if_kernel("Unhandled kernel unaligned access", regs);

    info.si_signo = SIGSEGV;
    info.si_errno = 0;
	info.si_code = SEGV_MAPERR;
    info.si_addr = (void __user *) addr;
	force_sig_info(SIGSEGV, &info, current);

	return;

sigbus:
	die_if_kernel("Unhandled kernel unaligned access", regs);
    info.si_signo = SIGBUS;
    info.si_errno = 0;
	info.si_code = BUS_ADRERR;
    info.si_addr = (void __user *) addr;
	force_sig_info(SIGBUS, &info, current);

	return;

sigill:
	die_if_kernel("Unhandled kernel unaligned access or invalid instruction", regs);
    info.si_signo = SIGILL;
    info.si_errno = 0;
	info.si_code = ILL_ILLOPC;
    info.si_addr = (void __user *) addr;
	force_sig_info(SIGILL, &info, current);
}
#define IS_MIPS16_EXTEND_OR_JAL(a) ((((a) >> (27 - 16)) == 30) | (((a) >> (27 - 16)) == 3))  /*--- Opcode EXTEND oder JAL ---*/
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int print_code_range(unsigned int __user *pc, unsigned int mips16, unsigned int usermode, int left_offset, int right_offset, unsigned long *badaddr) {

    if(!mips16) {
        signed int i;
        unsigned long access_addr = (unsigned long)pc + sizeof(unsigned int) * left_offset;
        printk(KERN_ERR"Code(0x%08lx):", access_addr);
        for(i = left_offset; i < right_offset; i++) {
            unsigned int pc_value;

            access_addr = (unsigned long)pc + sizeof(unsigned int) * i;
            if(usermode && !unlikely(access_ok(VERIFY_READ, access_addr, 4))) {
                printk(KERN_ERR "[unaligned-handler] illegal address 0x%lx (sigill)\n", access_addr);
                *badaddr = (unsigned long)access_addr;
                return -1; /*--- sigill; ---*/
            }
            if(__get_user(pc_value, (unsigned int __user *)access_addr)) {
                printk(KERN_ERR "[unaligned-handler] load from address 0x%lx failed (sigbus)\n", access_addr);
                *badaddr = (unsigned long)access_addr;
                return -2; /*--- sigbus; ---*/
            }
            printk(" %s0x%08x%s", i == 0 ? "<" : "", pc_value, i == 0 ? ">" : "");
        }
    } else {
        /*--- wegen EXT-Code nur step by step vorhangeln: ---*/
        unsigned short code0, code1;
        unsigned long pc_addr = (unsigned long)pc & ~0x1;
        unsigned long access_addr = (unsigned long)pc & ~0x1;
        unsigned long end_addr = ((unsigned long)pc & ~0x1) + sizeof(unsigned short) * right_offset;
    
        while(left_offset < 0) {
            if(usermode && !unlikely(access_ok(VERIFY_READ, access_addr, 4))) {
                printk(KERN_ERR "[unaligned-handler] illegal address 0x%lx (sigill)\n", access_addr);
                *badaddr = (unsigned long)access_addr;
                return -1; /*--- sigill; ---*/
            }
            if(__get_user(code1, (unsigned short __user *)(access_addr - sizeof(short)))) {
                printk(KERN_ERR "[unaligned-handler] load from 16 bit address 0x%lx failed (sigbus)\n", access_addr);
                *badaddr = (unsigned long)access_addr;
                return -2; /*--- sigbus; ---*/
            }
            if(__get_user(code0, (unsigned short __user *)(access_addr - 2 * sizeof(short)))) {
                printk(KERN_ERR "[unaligned-handler] load from 16 bit address 0x%lx failed (sigbus)\n", access_addr);
                *badaddr = (unsigned long)access_addr;
                return -2; /*--- sigbus; ---*/
            }
            if(IS_MIPS16_EXTEND_OR_JAL(code0)) {
                access_addr -= 2 * sizeof(short);
            } else {
                access_addr -= sizeof(short);
            }
            left_offset++;
        }
        printk(KERN_ERR"Code(0x%08lx):", access_addr);
        while(access_addr < end_addr) {
            if(__get_user(code0, (unsigned short __user *)(access_addr))) {
                printk(KERN_ERR "[unaligned-handler] load from 16 bit address 0x%lx failed (sigbus)\n", access_addr);
                *badaddr = (unsigned long)access_addr;
                return -2; /*--- sigbus; ---*/
            }
            if(access_addr == pc_addr) {
                if(IS_MIPS16_EXTEND_OR_JAL(code0)) {
                    access_addr += sizeof(short);
                    if(__get_user(code1, (unsigned short __user *)(access_addr))) {
                        printk(KERN_ERR "[unaligned-handler] load from 16 bit address 0x%lx failed (sigbus)\n", access_addr);
                        *badaddr = (unsigned long)access_addr;
                        return -2; /*--- sigbus; ---*/
                    }
                    printk(" <0x%04x %04x>", code0, code1);
                } else {
                    printk(" <0x%04x>", code0);
                }
            } else {
                printk(" 0x%04x", code0);
            }
            access_addr += sizeof(short);
        }
    }
    printk("\n");
    return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int print_mem_config(struct mm_struct *mmm, unsigned long addr) {
    struct vm_area_struct *vm;
    unsigned int i = 0;
    if(mmm == NULL) 
        return 0;
    vm = mmm->mmap;
    while(vm) {
        if((addr >= vm->vm_start) && (addr < vm->vm_end)) {
            printk(KERN_ERR"Adresse-Segment(%d): 0x%lx: 0x%lx :0x%lx (offset 0x%lx)", 
                    i, vm->vm_start, addr, vm->vm_end, addr - vm->vm_start);
            if(vm->vm_file) {
                printk(" Path='%s'", vm->vm_file->f_path.dentry->d_name.name);
            }
            printk("\n");
            return 0;
        }
        vm = vm->vm_next;
        i++;
    }
    return 1;
}

asmlinkage void do_ade(struct pt_regs *regs)
{
	unsigned int __user *pc;
	mm_segment_t seg;
	siginfo_t info;
    unsigned long addr = 0UL;

	/*
	 * Did we catch a fault trying to load an instruction?
	 * Or are we running in MIPS16 mode?
	 */
	if (regs->cp0_badvaddr == regs->cp0_epc)
		goto sigbus;

	pc = (unsigned int __user *) exception_epc(regs);
	/*--- if (user_mode(regs) && !test_thread_flag(TIF_FIXADE)) { ---*/
		/*--- goto sigbus; ---*/
    /*--- } ---*/
    if (!user_mode(regs)) {
        ai_sys++;
        if(ai_kernelmode & UNALIGNED_WARN) {
            int ret;
            printk(KERN_ERR "[kernel-unaligned %lu] pc=0x%p(%pF) addr=0x%08lx task=%s pid=%d ra=0x%08lx(%pF)\n", 
                  ai_sys, pc, pc, regs->cp0_badvaddr, current->comm, current->pid,
                  regs->regs[31], (void *)regs->regs[31]
            );
            ret = print_code_range(pc, regs->cp0_epc & 0x1, user_mode(regs), -2, 3, &addr);
            if(ret == -1) {
                goto sigill;
            } else if(ret == -2) {
                goto sigbus;
            }
	        if(print_mem_config(current->active_mm, (unsigned long)pc))
	            print_mem_config(current->mm, (unsigned long)pc);
        }
    } else {
        ai_user++;
        if(unaligned_action & UNALIGNED_WARN) {
            int ret;
            printk(KERN_ERR"Alignment trap: %s (%d) PC=0x%p Address=0x%08lx\n", current->comm, task_pid_nr(current), pc, regs->cp0_badvaddr);
            ret = print_code_range(pc, regs->cp0_epc & 0x1, user_mode(regs), -2, 3, &addr);
            if(ret == -1) {
                goto sigill;
            } else if(ret == -2) {
                goto sigbus;
            }
            if(print_mem_config(current->active_mm, (unsigned long)pc))
                print_mem_config(current->mm, (unsigned long)pc);
            /*--- show_registers(regs); ---*/
        }
        switch(unaligned_action) {
            case UNALIGNED_ACTION_IGNORED:
            case UNALIGNED_ACTION_IGNORED_WARN:
                ai_skipped++;
                return;   
            case UNALIGNED_ACTION_SIGNAL:
            case UNALIGNED_ACTION_SIGNAL_WARN:
                goto sigbus;
            case UNALIGNED_ACTION_FIXUP:
            case UNALIGNED_ACTION_FIXUP_WARN:
                 break;
        }
    }
	/*
	 * Do branch emulation only if we didn't forward the exception.
	 * This is all so but ugly ...
	 */
	seg = get_fs();
	if (!user_mode(regs)) {
		set_fs(KERNEL_DS);
    }
	if (regs->cp0_epc & 0x1) {
	    emulate_load_store_insn_mips16(regs, (void __user *)regs->cp0_badvaddr, pc);
    } else {
    	emulate_load_store_insn(regs, (void __user *)regs->cp0_badvaddr, pc);
    }
	set_fs(seg);

	return;

sigill:
	die_if_kernel("Unhandled kernel unaligned access or invalid instruction", regs);
    info.si_signo = SIGILL;
    info.si_errno = 0;
	info.si_code = ILL_ILLOPC;
    info.si_addr = (void __user *) addr;
	force_sig_info(SIGILL, &info, current);
	return;

sigbus:
	die_if_kernel("Unhandled kernel unaligned access", regs);
    info.si_signo = SIGBUS;
    info.si_errno = 0;
	info.si_code = BUS_ADRERR;
    info.si_addr = (void __user *) addr;
	force_sig_info(SIGBUS, &info, current);
	return;

}

#ifdef CONFIG_DEBUG_FS
extern struct dentry *mips_debugfs_dir;
static int __init debugfs_unaligned(void)
{
	struct dentry *d;

	if (!mips_debugfs_dir)
		return -ENODEV;
	d = debugfs_create_u32("unaligned_instructions", S_IRUGO,
			       mips_debugfs_dir, &unaligned_instructions);
	if (!d)
		return -ENOMEM;
	d = debugfs_create_u32("unaligned_action", S_IRUGO | S_IWUSR,
			       mips_debugfs_dir, &unaligned_action);
	if (!d)
		return -ENOMEM;
	return 0;
}
__initcall(debugfs_unaligned);
#endif

#ifdef CONFIG_PROC_FS
static const char *usermode_action[] = {
	"ignored",
	"warn",
	"fixup",
	"fixup+warn",
	"signal",
	"signal+warn"
};

static int
proc_alignment_read(char *page, char **start, off_t off, int count, int *eof,
		    void *data)
{
	char *p = page;
	int len;

	p += sprintf(p, "User:\t\t%lu\n", ai_user);
	p += sprintf(p, "System:\t\t%lu\n", ai_sys);
	p += sprintf(p, "Skipped:\t%lu\n", ai_skipped);
	p += sprintf(p, "Word:\t\t%lu\n", ai_word);
	p += sprintf(p, "DWord:\t\t%lu\n", ai_dword);
	p += sprintf(p, "User faults:\t%i (%s)\n", ai_usermode, usermode_action[ai_usermode]);
	p += sprintf(p, "Kernel faults:\t%i (%s)\n", ai_kernelmode, usermode_action[ai_kernelmode]);

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int proc_alignment_write(struct file *file, const char __user *buffer,
				unsigned long count, void *data)
{
	char mode;

	if (count > 0) {
		if (get_user(mode, buffer))
			return -EFAULT;
		if (mode >= '0' && mode <= '5') {
			ai_usermode = mode - '0';
        } else if (mode >= '6' && mode <= '7') {
            ai_kernelmode = (mode - '6') == 0 ? UNALIGNED_ACTION_FIXUP : UNALIGNED_ACTION_FIXUP_WARN;
        }
	}
	return count;
}

/*
 * This needs to be done after sysctl_init, otherwise sys/ will be
 * overwritten.  Actually, this shouldn't be in sys/ at all since
 * it isn't a sysctl, and it doesn't contain sysctl information.
 * We now locate it in /proc/cpu/alignment instead.
 */
static int __init alignment_init(void)
{
	struct proc_dir_entry *res;

	res = proc_mkdir("cpu", NULL);
	if (!res)
		return -ENOMEM;

	res = create_proc_entry("alignment", S_IWUSR | S_IRUGO, res);
	if (!res)
		return -ENOMEM;

	res->read_proc = proc_alignment_read;
	res->write_proc = proc_alignment_write;

	return 0;
}
__initcall(alignment_init);
#endif /* CONFIG_PROC_FS */
