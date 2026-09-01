#ifndef __ASM_BUG_H
#define __ASM_BUG_H

#include <linux/compiler.h>
#include <asm/sgidefs.h>

#ifdef CONFIG_BUG
#define CONFIG_BUG_EXTRA_INFO

#include <asm/break.h>

struct bug_debug_table_entry {
    unsigned long addr;
    char *filename;
    unsigned int line;
    char *functionname;
    char *condition;
};

extern void register_bug_debug_table(char *name, unsigned long start, unsigned long end) __attribute__ ((weak));
extern void release_bug_debug_table(char *name) __attribute__ ((weak));


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define BUG() ( { \
	__asm__ __volatile__( \
            "1:  \n" \
            "    break %0\n" \
            "   .section __bug_debug_table, \"a\"\n" \
            "   .word 1b\n" \
            "   .word %1\n" \
            "   .word %2\n" \
            "   .word %3\n" \
            "   .word 0\n" \
            "   .previous\n" \
            : : "i" (BRK_BUG), "i" (__FILE__), "i" (__LINE__), "i" (__FUNCTION__)); \
	/* Fool GCC into thinking the function doesn't return. */ \
	while (1) \
		; \
        } ) 

#define HAVE_ARCH_BUG

#if (_MIPS_ISA > _MIPS_ISA_MIPS1)

#define __BUG_ON(condition) \
( { \
	__asm__ __volatile__(       \
            "1:  \n" \
            "   tne $0, %0, %1\n" \
            "   .section __bug_debug_table, \"a\"\n" \
            "   .word 1b\n" \
            "   .word %2\n" \
            "   .word %3\n" \
            "   .word %4\n" \
            "   .word %5\n" \
            "   .previous\n" \
			     : : "r" (condition), "i" (BRK_BUG), "i" (__FILE__), "i" (__LINE__), "i" (__FUNCTION__), "i" ( #condition ));       \
} )

#define BUG_ON(C) __BUG_ON((unsigned long)(C))

#define HAVE_ARCH_BUG_ON

#endif /* _MIPS_ISA > _MIPS_ISA_MIPS1 */

#endif

#include <asm-generic/bug.h>

#endif /* __ASM_BUG_H */
