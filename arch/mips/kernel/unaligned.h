#ifndef _ARCH_MIPS_KERNEL_UNALIGNED_H_
#define _ARCH_MIPS_KERNEL_UNALIGNED_H_


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define UNALIGNED_WARN  0x1 
enum {
    UNALIGNED_ACTION_IGNORED        = 0,
    UNALIGNED_ACTION_IGNORED_WARN   = 1 | UNALIGNED_WARN,  /*-- verodern eigentlich Dummy :-) ---*/
	UNALIGNED_ACTION_FIXUP          = 2,
	UNALIGNED_ACTION_FIXUP_WARN     = 3 | UNALIGNED_WARN, /*-- verodern eigentlich Dummy :-) ---*/
	UNALIGNED_ACTION_SIGNAL         = 4,
	UNALIGNED_ACTION_SIGNAL_WARN    = 5 | UNALIGNED_WARN, /*-- verodern eigentlich Dummy :-) ---*/
};


#ifdef CONFIG_DEBUG_FS
extern u32 unaligned_instructions;
#endif
extern int ai_usermode;
extern int ai_kernelmode;

#define unaligned_action ai_usermode

#endif /*--- #ifndef _ARCH_MIPS_KERNEL_UNALIGNED_H_ ---*/
