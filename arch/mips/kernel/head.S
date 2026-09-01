/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 1995 Waldorf Electronics
 * Written by Ralf Baechle and Andreas Busse
 * Copyright (C) 1994 - 99, 2003, 06 Ralf Baechle
 * Copyright (C) 1996 Paul M. Antoine
 * Modified for DECStation and hence R3000 support by Paul M. Antoine
 * Further modifications by David S. Miller and Harald Koerfgen
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Kevin Kissell, kevink@mips.com and Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 */
#include <linux/init.h>
#include <linux/threads.h>

#include <asm/addrspace.h>
#include <asm/asm.h>
#include <asm/asmmacro.h>
#include <asm/irqflags.h>
#include <asm/regdef.h>
#include <asm/page.h>
#include <asm/pgtable-bits.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>

#include <kernel-entry-init.h>

	/*
	 * inputs are the text nasid in t1, data nasid in t2.
	 */
	.macro MAPPED_KERNEL_SETUP_TLB
#ifdef CONFIG_MAPPED_KERNEL
	/*
	 * This needs to read the nasid - assume 0 for now.
	 * Drop in 0xffffffffc0000000 in tlbhi, 0+VG in tlblo_0,
	 * 0+DVG in tlblo_1.
	 */
	dli	t0, 0xffffffffc0000000
	dmtc0	t0, CP0_ENTRYHI
	li	t0, 0x1c000		# Offset of text into node memory
	dsll	t1, NASID_SHFT		# Shift text nasid into place
	dsll	t2, NASID_SHFT		# Same for data nasid
	or	t1, t1, t0		# Physical load address of kernel text
	or	t2, t2, t0		# Physical load address of kernel data
	dsrl	t1, 12			# 4K pfn
	dsrl	t2, 12			# 4K pfn
	dsll	t1, 6			# Get pfn into place
	dsll	t2, 6			# Get pfn into place
	li	t0, ((_PAGE_GLOBAL|_PAGE_VALID| _CACHE_CACHABLE_COW) >> 6)
	or	t0, t0, t1
	mtc0	t0, CP0_ENTRYLO0	# physaddr, VG, cach exlwr
	li	t0, ((_PAGE_GLOBAL|_PAGE_VALID| _PAGE_DIRTY|_CACHE_CACHABLE_COW) >> 6)
	or	t0, t0, t2
	mtc0	t0, CP0_ENTRYLO1	# physaddr, DVG, cach exlwr
	li	t0, 0x1ffe000		# MAPPED_KERN_TLBMASK, TLBPGMASK_16M
	mtc0	t0, CP0_PAGEMASK
	li	t0, 0			# KMAP_INX
	mtc0	t0, CP0_INDEX
	li	t0, 1
	mtc0	t0, CP0_WIRED
	tlbwi
#else
	mtc0	zero, CP0_WIRED
#endif
	.endm

	/*
	 * For the moment disable interrupts, mark the kernel mode and
	 * set ST0_KX so that the CPU does not spit fire when using
	 * 64-bit addresses.  A full initialization of the CPU's status
	 * register is done later in per_cpu_trap_init().
	 */
	.macro	setup_c0_status set clr
	.set	push
#ifdef CONFIG_MIPS_MT_SMTC
	/*
	 * For SMTC, we need to set privilege and disable interrupts only for
	 * the current TC, using the TCStatus register.
	 */
	mfc0	t0, CP0_TCSTATUS
	/* Fortunately CU 0 is in the same place in both registers */
	/* Set TCU0, TMX, TKSU (for later inversion) and IXMT */
	li	t1, ST0_CU0 | 0x08001c00
	or	t0, t1
	/* Clear TKSU, leave IXMT */
	xori	t0, 0x00001800
	mtc0	t0, CP0_TCSTATUS
	_ehb
	/* We need to leave the global IE bit set, but clear EXL...*/
	mfc0	t0, CP0_STATUS
	or	t0, ST0_CU0 | ST0_EXL | ST0_ERL | \set | \clr
	xor	t0, ST0_EXL | ST0_ERL | \clr
	mtc0	t0, CP0_STATUS
#else
	mfc0	t0, CP0_STATUS
	or	t0, ST0_CU0|\set|0x1f|\clr
	xor	t0, 0x1f|\clr
#ifdef CONFIG_CPU_HAS_DSP_ASE
	or      t0, ST0_MX
#endif
	mtc0	t0, CP0_STATUS
	.set	noreorder
	sll	zero,3				# ehb
#endif
	.set	pop
	.endm

	.macro	setup_c0_status_pri
#ifdef CONFIG_64BIT
	setup_c0_status ST0_KX 0
#else
	setup_c0_status 0 0
#endif
	.endm

	.macro	setup_c0_status_sec
#ifdef CONFIG_64BIT
	setup_c0_status ST0_KX ST0_BEV
#else
	setup_c0_status 0 ST0_BEV
#endif
	.endm

	j kernel_entry
	nop
#ifndef CONFIG_NO_EXCEPT_FILL
	/*
	 * Reserved space for exception handlers.
	 * Necessary for machines which link their kernels at KSEG0.
	 */
	.fill	0x400
#endif

EXPORT(_stext)

#ifdef CONFIG_BOOT_RAW
	/*
	 * Give us a fighting chance of running if execution beings at the
	 * kernel load address.  This is needed because this platform does
	 * not have a ELF loader yet.
	 */
FEXPORT(__kernel_entry)
	j	kernel_entry
#endif

#ifdef CONFIG_IMAGE_CMDLINE_HACK
	.ascii	"CMDLINE:"
EXPORT(__image_cmdline)
	.fill	0x400
#endif /* CONFIG_IMAGE_CMDLINE_HACK */

	__REF

NESTED(kernel_entry, 16, sp)			# kernel entry point

	kernel_entry_setup			# cpu specific setup

	setup_c0_status_pri

	/* We might not get launched at the address the kernel is linked to,
	   so we jump there.  */
	PTR_LA	t0, 0f
	jr	t0
0:

#ifdef CONFIG_MIPS_MT_SMTC
	/*
	 * In SMTC kernel, "CLI" is thread-specific, in TCStatus.
	 * We still need to enable interrupts globally in Status,
	 * and clear EXL/ERL.
	 *
	 * TCContext is used to track interrupt levels under
	 * service in SMTC kernel. Clear for boot TC before
	 * allowing any interrupts.
	 */
	mtc0	zero, CP0_TCCONTEXT

	mfc0	t0, CP0_STATUS
	ori	t0, t0, 0xff1f
	xori	t0, t0, 0x001e
	mtc0	t0, CP0_STATUS
#endif /* CONFIG_MIPS_MT_SMTC */

#ifdef CONFIG_MIPS_UR8
    /* 
     * Wir kopieren das Urlader-Environment (Pointer in a2) und die Kernel-commandline (Pointer in argc=a0 und argv=a1) 
     * an einen neuen Bereich, da er sonst mit dem BSS kollidiert. Die neuen Bereiche dst_cmdline_buff, dst_cmdline, dst_env_buff
     * und dst_env sind in .init.data und werden in kernel/env.c statisch reserviert.
     *
     * void copy_env ( void ) {
     *     volatile register int i asm("t0"); // temporary register
     *     volatile register char *buff_ptr asm("t2");
     *     volatile register int s asm("t3");
     * 
     *     i = 0;
     *     buff_ptr = dst_env_buff_reg;
     *     while ( fw_arg2_reg[i] ) {
     *         s = 0;
     * 
     *         dst_env_reg[i] = buff_ptr;
     *         do {
     *             *(buff_ptr++) = fw_arg2_reg[i][s];
     *         } while ( fw_arg2_reg[i][s++] );
     * 
     *         i++;
     *     }
     *     fw_arg2_reg = dst_env_reg;
     * 
     * }
     * 
     * void copy_cmdline ( void ) {
     *     volatile register int i asm("t0"); // temporary register
     *     volatile register char *buff_ptr asm("t2");
     *     volatile register int s asm("t3");
     * 
     *     buff_ptr = dst_cmdline_buff_reg;
     *     for (i = 0; i < fw_arg0_reg; i++ ) {
     *         dst_cmdline_reg[i] = buff_ptr;
     *         s = 0;
     *         do {
     *             *(buff_ptr++) = fw_arg1_reg[i][s];
     *         } while ( fw_arg1_reg[i][s++] );
     *     }
     * 
     *     fw_arg1_reg = dst_cmdline_reg;
     * }
     * 
     *
     */
# copy pointer for wlan and dect-config
    la t4, dst_config
    lw t5, 0(a3)
    sw t5, 0(t4)

    lw t5, 4(a3)
    sw t5, 4(t4)

    lw t5, 8(a3)
    sw t5, 8(t4)

    lw t5, 12(a3)
    sw t5, 12(t4)

    la a3, dst_config

    la t4, dst_cmdline_buff
    la t5, dst_cmdline

    la t6, dst_env_buff
    la t7, dst_env

copy_env:
	move	t0,zero
	move	v0,t6
	j	marke_2
marke_0:
	addu	t1,v1,t7
	move	t3,zero
	sw	v0,0(t1)
	addu	v1,v1,a2
marke_1:
	lw	t1,0(v1)
	nop
	addu	t1,t1,t3
	lbu	t1,0(t1)
	nop
	sb	t1,0(v0)
	lw	t1,0(v1)
	addiu	v0,v0,1
	addu	t1,t1,t3
	lbu	t1,0(t1)
	nop
	sll	t1,t1,0x18
	sra	t1,t1,0x18
	addiu	t3,t3,1
	bnez	t1, marke_1
	addiu	t0,t0,1
marke_2:
	sll	v1,t0,0x2
	addu	t1,v1,a2
	lw	t1,0(t1)
	nop
	bnez	t1, marke_0
	move	a2,t7
    nop

copy_cmdline:
	move	t0,zero
	move	v0,t4
	move	v1,a0
	j	marke_c
marke_a:
	sll	t1,t0,0x2
	addu	t2,t1,t5
	sw	v0,0(t2)
	move	t3,zero
	addu	t1,t1,a1
marke_b:
	lw	t2,0(t1)
	nop
	addu	t2,t2,t3
	lbu	t2,0(t2)
	nop
	sb	t2,0(v0)
	lw	t2,0(t1)
	addiu	v0,v0,1
	addu	t2,t2,t3
	lbu	t2,0(t2)
	nop
	sll	t2,t2,0x18
	sra	t2,t2,0x18
	addiu	t3,t3,1
	bnez	t2, marke_b
	addiu	t0,t0,1
marke_c:
	slt	t1,t0,v1
	bnez	t1,marke_a
	move	a1,t5
	nop
#endif

	PTR_LA		t0, __bss_start		# clear .bss
	LONG_S		zero, (t0)
	PTR_LA		t1, __bss_stop - LONGSIZE
1:
	PTR_ADDIU	t0, LONGSIZE
	LONG_S		zero, (t0)
	bne		t0, t1, 1b

	LONG_S		a0, fw_arg0		# number of argument1 (argc)
	LONG_S		a1, fw_arg1     # argv
	LONG_S		a2, fw_arg2     # envp
	LONG_S		a3, fw_arg3     # configp

	MTC0		zero, CP0_CONTEXT	# clear context register
	PTR_LA		$28, init_thread_union
	/* Set the SP after an empty pt_regs.  */
	PTR_LI		sp, _THREAD_SIZE - 32 - PT_SIZE
	PTR_ADDU	sp, $28
	back_to_back_c0_hazard
	set_saved_sp	sp, t0, t1
	PTR_SUBU	sp, 4 * SZREG		# init stack pointer
#ifdef CONFIG_BDI_SWITCH
    /* Setup the PTE pointers for the Abatron bdiGDB.  */  
    la t0, 0x800002f0
    la t1, swapper_pg_dir
    sw t1, (t0)
    addiu t0, 4
    la t1, pgd_current
    sw t1, (t0)
#endif
	j		start_kernel
	END(kernel_entry)

	__CPUINIT

#ifdef CONFIG_SMP
/*
 * SMP slave cpus entry point.  Board specific code for bootstrap calls this
 * function after setting up the stack and gp registers.
 */
NESTED(smp_bootstrap, 16, sp)
#ifdef CONFIG_MIPS_MT_SMTC
	/*
	 * Read-modify-writes of Status must be atomic, and this
	 * is one case where CLI is invoked without EXL being
	 * necessarily set. The CLI and setup_c0_status will
	 * in fact be redundant for all but the first TC of
	 * each VPE being booted.
	 */
	DMT	10	# dmt t2 /* t0, t1 are used by CLI and setup_c0_status() */
	jal	mips_ihb
#endif /* CONFIG_MIPS_MT_SMTC */
	setup_c0_status_sec
	smp_slave_setup
#ifdef CONFIG_MIPS_MT_SMTC
	andi	t2, t2, VPECONTROL_TE
	beqz	t2, 2f
	EMT		# emt
2:
#endif /* CONFIG_MIPS_MT_SMTC */
	j	start_secondary
	END(smp_bootstrap)
#endif /* CONFIG_SMP */

	__FINIT
