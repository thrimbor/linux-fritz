#ifndef __EXPORTS_H__
#define __EXPORTS_H__

#ifndef __ASSEMBLY__

typedef struct bd_info {
	int		bi_baudrate;	/* serial console baudrate */
	unsigned long	bi_ip_addr;	/* IP Address */
	unsigned char	bi_enetaddr[6];	/* Ethernet adress */
	unsigned long	bi_arch_number;	/* unique id for this board */
	unsigned long	bi_boot_params;	/* where this board expects params */
	unsigned long	bi_memstart;	/* start of DRAM memory */
	unsigned long	bi_memsize;	/* size	 of DRAM memory in bytes */
	unsigned long	bi_flashstart;	/* start of FLASH memory */
	unsigned long	bi_flashsize;	/* size  of FLASH memory */
	unsigned long	bi_flashoffset;	/* reserved area for startup monitor */
} bd_t;

typedef	struct	global_data {
	bd_t		*bd;
	unsigned long	flags;
	unsigned long	baudrate;
	unsigned long	have_console;	/* serial_init() was called */
	unsigned long	ram_size;	/* RAM size */
	unsigned long	reloc_off;	/* Relocation Offset */
	unsigned long	env_addr;	/* Address  of Environment struct */
	unsigned long	env_valid;	/* Checksum of Environment valid? */
	void		**jt;		/* jump table */
} gd_t;



/* These are declarations of exported functions available in C code */
unsigned long get_version(void);
int  getc0(void);
int  tstc0(void);
void putc0(const char);
void puts0(const char*);
void printf0(const char* fmt, ...);
void install_hdlr0(int, void*, void*);
void free_hdlr0(int);
void *malloc0(size_t);
void free0(void*);
void udelay0(unsigned long);
unsigned long get_timer0(unsigned long);
void vprintf0(const char *, va_list);
void do_reset0 (void);

void app_startup(char **);

#endif    /* ifndef __ASSEMBLY__ */

enum {
#define EXPORT_FUNC(x) XF_ ## x ,
#include <_exports.h>
#undef EXPORT_FUNC

	XF_MAX
};

#define EXPORT_FUNC(x) \
	asm volatile (			\
"	.globl " #x "\n"		\
#x ":\n"				\
"	lw	$25, %0($26)\n"		\
"	lw	$25, %1($25)\n"		\
"	jr	$25\n"			\
	: : "i"(offsetof(gd_t, jt)), "i"(XF_ ## x * sizeof(void *)) : "t9");


#define XF_VERSION	2


#endif	/* __EXPORTS_H__ */
