
#ifndef SERIAL_AVM_8250_H
#define SERIAL_AVM_8250_H

void uart_avm_post_setup_ports(void);

#if 0
#define THR 0x0000
#define RBR 0x0000
#define DLL 0x0000
#define DLH 0x0004
#define IER 0x0004
#define IID 0x0008
#define FCR 0x0008
#define LCR 0x000C
#define MCR 0x0010
#define LSR 0x0014
#define MSR 0x0018
#define SCR 0x001C
#endif


/*------------------------------------------------------------------------------------------*\
 * Register Map fuer DLAB = 0
\*------------------------------------------------------------------------------------------*/
struct avm_8250_regs {
	volatile unsigned int rbr_thr;   /*  0x0000  */ 
	volatile unsigned int ier;       /*  0x0004  */
	volatile unsigned int iir_fcr;   /*  0x0008  */
	volatile unsigned int lcr;       /*  0x000C  */
	volatile unsigned int mcr;       /*  0x0010  */
	volatile unsigned int lsr;       /*  0x0014  */
	volatile unsigned int msr;       /*  0x0018  */
	volatile unsigned int scr;       /*  0x001C  */
};                                    

/*------------------------------------------------------------------------------------------*\
 * Register Map fuer DLAB = 1
\*------------------------------------------------------------------------------------------*/
struct avm_8250_regs_dlab {
	volatile unsigned int dll;		 /*  0x0000  */ 
	volatile unsigned int dlh;       /*  0x0004  */
	volatile unsigned int iir_fcr;   /*  0x0008  */
	volatile unsigned int lcr;       /*  0x000C  */
	volatile unsigned int mcr;       /*  0x0010  */
	volatile unsigned int lsr;       /*  0x0014  */
	volatile unsigned int msr;       /*  0x0018  */
	volatile unsigned int scr;       /*  0x001C  */
};
                                      
#define NR_MAX_UARTS 2
/*--- #define NR_MAX_UARTS 1 ---*/
#endif
