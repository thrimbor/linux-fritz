/******************************************************************************
**
** FILE NAME    : avm_membench.c
** AUTHOR       : Christoph Buettner & Heiko Blobner
*******************************************************************************/

#include <asm/io.h>
#include <linux/irqflags.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/gfp.h>
#include <asm/addrspace.h>
#include <asm/uaccess.h>
#include <asm/delay.h>
#if defined(CONFIG_MACH_ATHEROS)
#include <atheros.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#endif
#if defined(CONFIG_MIPS_UR8)
    #include <ur8_clk.h>
#elif defined(CONFIG_MACH_AR724x) || defined(CONFIG_MACH_AR934x) || defined(CONFIG_MACH_QCA955x)
    #include <atheros.h>
    #include <asm/mach_avm.h>
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define TOTAL_BLOCKS        64

#if defined(CONFIG_MACH_AR7240)
    extern unsigned int ar7240_cpu_freq, ar7240_ahb_freq, ar7240_ddr_freq;
    #define CPU_CLOCK           ar7240_cpu_freq
    #define BUS_CLOCK           ar7240_ddr_freq
    #define WORTBREITE          16
#elif defined(CONFIG_MACH_AR724x) 
    extern unsigned int ath_cpu_freq, ath_ahb_freq, ath_ddr_freq;
    #define CPU_CLOCK           ath_cpu_freq
    #define BUS_CLOCK           ath_ddr_freq
    #define WORTBREITE          16
#elif defined(CONFIG_MACH_AR934x) 
    extern unsigned int ath_cpu_freq, ath_ahb_freq, ath_ddr_freq;
    #define CPU_CLOCK           ath_get_clock(avm_clock_id_cpu)
    #define BUS_CLOCK           ath_get_clock(avm_clock_id_ddr)
    #define WORTBREITE          (is_ar9341() ? 16 : 32)
#elif defined(CONFIG_MACH_QCA955x)
    extern unsigned int ath_cpu_freq, ath_ahb_freq, ath_ddr_freq;
    #define CPU_CLOCK           500
    #define BUS_CLOCK           250
    #define WORTBREITE          16
#elif defined(CONFIG_MIPS_UR8)
    #define CPU_CLOCK           ur8_get_clock(avm_clock_id_cpu)
    #define BUS_CLOCK           ur8_get_clock(avm_clock_id_vbus)
    #define WORTBREITE          16
#elif defined(CONFIG_LANTIQ)
    extern u32 ifx_get_ddr_hz(void);
    extern unsigned int ifx_get_cpu_hz(void);

    #define CPU_CLOCK           ifx_get_cpu_hz()
    #define BUS_CLOCK           ifx_get_ddr_hz() * 2
    #define WORTBREITE          16
#else
    #error "Unknown Architecture!!!"
#endif

#define MESS_LAENGE         ((CPU_CLOCK / 2) * 1)

#define ZEIT_S              (MESS_LAENGE / (CPU_CLOCK >> 1))
#define ZEIT_MS             ((MESS_LAENGE % (CPU_CLOCK >> 1)) / ((CPU_CLOCK >> 1) / 1000))
#define KB_PRO_SEC          ((kb / loops) * 1000/(ZEIT_S * 1000 + ZEIT_MS))
#define WORTE_PRO_SEC(wortbreite)       (KB_PRO_SEC * (1024 / (wortbreite / 8)))
#define WORTE_PRO_CLOCK_1(wortbreite)   (BUS_CLOCK / WORTE_PRO_SEC(wortbreite))
#define WORTE_PRO_CLOCK_10(wortbreite)  ((BUS_CLOCK / (WORTE_PRO_SEC(wortbreite) / 1000)) % 1000)

/*------------------------------------------------------------------------------------------*\
 * Pipeline-friendly Read
 *
 *  -16x 4-byte-Werte pro Schleifendurchlauf
 *      -> 16 Lesezugriffe pro Schleifendurchlauf
 *  -4 Register werden abwechselnd als Ziel genutzt -> kein unnoetiges Pipeline-Leeren wg. doppelt genutzter Register
\*------------------------------------------------------------------------------------------*/
static unsigned long do_measure__read_pipe(char *mem, int irqsave, int loops) {
    unsigned short j;
    int i;
    register volatile int *__local_mem;
    unsigned long flags;
    unsigned long kb = 0;


	for (i = 0; i < loops; i++){
		u32 time_in_double_cpu_clocks = 0;
		if(irqsave) { local_irq_save(flags); }

        if(((unsigned long)mem & 0xE0000000UL) != 0xA0000000UL)
            dma_cache_inv((unsigned long)mem, TOTAL_BLOCKS * 1024);

		do {
            u32 tick_value;
            u32 tick_value_end;
			volatile int *local_mem = (volatile int *)mem;
			tick_value = read_c0_count();

			for(j = 0, __local_mem = local_mem; j < 1024 ; j ++ , __local_mem += 16) { /*--- 1024 durchlauefe ---*/
				register int p = (int)__local_mem;
				register int dummy0 asm("v0") ;
				register int dummy1 asm("v1") ;
				register int dummy2 asm("a0") ;
				register int dummy3 asm("a1") ;

				__asm__ __volatile__ ("  lw %0, 0(%1) \n" : "=r" (dummy0) : "r" (p));
				__asm__ __volatile__ ("  lw %0, 4(%1) \n" : "=r" (dummy1) : "r" (p));
				__asm__ __volatile__ ("  lw %0, 8(%1) \n" : "=r" (dummy2) : "r" (p));
				__asm__ __volatile__ ("  lw %0,12(%1) \n" : "=r" (dummy3) : "r" (p));
				__asm__ __volatile__ ("  lw %0,16(%1) \n" : "=r" (dummy0) : "r" (p));
				__asm__ __volatile__ ("  lw %0,20(%1) \n" : "=r" (dummy1) : "r" (p));
				__asm__ __volatile__ ("  lw %0,24(%1) \n" : "=r" (dummy2) : "r" (p));
				__asm__ __volatile__ ("  lw %0,28(%1) \n" : "=r" (dummy3) : "r" (p));
				__asm__ __volatile__ ("  lw %0,32(%1) \n" : "=r" (dummy0) : "r" (p));
				__asm__ __volatile__ ("  lw %0,36(%1) \n" : "=r" (dummy1) : "r" (p));
				__asm__ __volatile__ ("  lw %0,40(%1) \n" : "=r" (dummy2) : "r" (p));
				__asm__ __volatile__ ("  lw %0,44(%1) \n" : "=r" (dummy3) : "r" (p));
				__asm__ __volatile__ ("  lw %0,48(%1) \n" : "=r" (dummy0) : "r" (p));
				__asm__ __volatile__ ("  lw %0,52(%1) \n" : "=r" (dummy1) : "r" (p));
				__asm__ __volatile__ ("  lw %0,56(%1) \n" : "=r" (dummy2) : "r" (p));
				__asm__ __volatile__ ("  lw %0,60(%1) \n" : "=r" (dummy3) : "r" (p));
			}

            if(((unsigned long)mem & 0xE0000000UL) != 0xA0000000UL)
                dma_cache_wback_inv((unsigned long)mem, TOTAL_BLOCKS * 1024);

            tick_value_end = read_c0_count();
            if(tick_value_end > tick_value) {
			    time_in_double_cpu_clocks += (tick_value_end - tick_value);
            } else {
			    time_in_double_cpu_clocks += (tick_value_end + ((u32)0xFFFFFFFF - tick_value));
            }

			kb += TOTAL_BLOCKS;
		} while(time_in_double_cpu_clocks < MESS_LAENGE);
		if(irqsave) { local_irq_restore(flags); }
		printk("*");
	}


	printk("\n");
    return kb;
}

/*------------------------------------------------------------------------------------------*\
 * Extreme Read
 *
 * -16x 4-byte-Werte werden jeweils von 4 unterschiedlichen Adressen gelesen
 *      -> 16*4 Lesezugriffe pro Schleifendurchlauf
\*------------------------------------------------------------------------------------------*/
//static unsigned long do_measure__read_extreme(int memsize_byte, int irqsave, int loops) {
static unsigned long do_measure__read_extreme(char *mem, int irqsave, int loops) {
    unsigned short j;
    int i;
    int x;
    volatile int *local_mem[4];

    unsigned long flags;
    unsigned long kb = 0;

	for (x = 0; x < loops; x++){
		u32 time_in_double_cpu_clocks = 0;
        u32 tick_value;
		u32 tick_value_end;
		if(irqsave) { local_irq_save(flags); }

        if(((unsigned long)mem & 0xE0000000UL) != 0xA0000000UL)
            dma_cache_inv((unsigned long)mem, TOTAL_BLOCKS * 1024);

		do {
			for (i = 0; i < 4 ; i++) {
				//local_mem[i] =(volatile int*) (0x80000000 | (i * memsize_byte / 4) );
				local_mem[i] = (volatile int*)(mem + (i*(16<<10)));
			}

			tick_value = read_c0_count();
			for(j = 0; j < 1024 ; j ++, local_mem[0] += 16, local_mem[1] += 16, local_mem[2] += 16, local_mem[3] += 16) { /*--- 1024 durchlauefe ---*/
				register int dummy0 asm("v0");
				register int dummy1 asm("v1");
				register int dummy2 asm("a0");
				register int dummy3 asm("a1");
			   	register int p0 = (int)local_mem[0];
			   	register int p1 = (int)local_mem[1];
			   	register int p2 = (int)local_mem[2];
			   	register int p3 = (int)local_mem[3];
				__asm__ __volatile__ ("  lw %0, 0(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 0(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 0(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 0(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 4(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 4(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 4(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 4(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 8(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 8(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 8(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 8(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 12(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 12(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 12(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 12(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 16(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 16(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 16(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 16(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 20(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 20(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 20(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 20(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 24(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 24(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 24(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 24(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 28(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 28(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 28(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 28(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 32(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 32(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 32(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 32(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 36(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 36(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 36(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 36(%1) \n" : "=r" (dummy3) : "r" (p3));
				
				__asm__ __volatile__ ("  lw %0, 40(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 40(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 40(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 40(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 44(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 44(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 44(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 44(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 48(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 48(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 48(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 48(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 52(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 52(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 52(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 52(%1) \n" : "=r" (dummy3) : "r" (p3));

				__asm__ __volatile__ ("  lw %0, 56(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 56(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 56(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 56(%1) \n" : "=r" (dummy3) : "r" (p3));
				
				__asm__ __volatile__ ("  lw %0, 60(%1) \n" : "=r" (dummy0) : "r" (p0));
				__asm__ __volatile__ ("  lw %0, 60(%1) \n" : "=r" (dummy1) : "r" (p1));
				__asm__ __volatile__ ("  lw %0, 60(%1) \n" : "=r" (dummy2) : "r" (p2));
				__asm__ __volatile__ ("  lw %0, 60(%1) \n" : "=r" (dummy3) : "r" (p3));
			}

            if(((unsigned long)mem & 0xE0000000UL) != 0xA0000000UL)
                dma_cache_wback_inv((unsigned long)mem, TOTAL_BLOCKS * 1024);

            tick_value_end = read_c0_count();
            if(tick_value_end > tick_value) {
			    time_in_double_cpu_clocks += ( tick_value_end - tick_value );
            } else {
			    time_in_double_cpu_clocks += ( tick_value_end + ((u32)0xFFFFFFFF - tick_value) );
            }

			kb += 256 ; /*--- ( 1024 schleifendurchlauefe * 64 lwops/schleifendurchlauf * 4 byte / 1024 byte/kb ); ---*/ 
		} while(time_in_double_cpu_clocks < MESS_LAENGE);
		if(irqsave) { local_irq_restore(flags); }
		printk(".");
	}

	printk("\n");
    return kb ;
}

/*------------------------------------------------------------------------------------------*\
 * Mixture Read/Write
 *
 * -1x 4-Byte Lesen + 1x 4-Byte Schreiben
 *      -> 2 Zugriffe pro Schleifendurchlauf
\*------------------------------------------------------------------------------------------*/
static unsigned long do_measure__read_mixture(char *mem, int irqsave, int loops) {
    unsigned short j;
    int i;
    register volatile int *__local_mem;
    unsigned long flags;
    unsigned long kb = 0;


    for (i = 0; i < loops; i++){
        u32 time_in_double_cpu_clocks = 0;
        if(irqsave) { local_irq_save(flags); }

        if(((unsigned long)mem & 0xE0000000UL) != 0xA0000000UL)
            dma_cache_inv((unsigned long)mem, TOTAL_BLOCKS * 1024);

        do {
            u32 tick_value;
            u32 tick_value_end;
            int temp;
            volatile int *local_mem = (volatile int *)mem;
            tick_value = read_c0_count();

            for(j = 0, __local_mem = local_mem; j < 1024 * 16; j ++ , __local_mem ++) { /*--- 1024 durchlauefe ---*/
                register int p = (int)__local_mem;
                register int dummy0 asm("v0") ;
                register int *temp_p = &temp;

                __asm__ __volatile__ ("  lw %0, 0(%1) \n" : "=r" (dummy0) : "r" (p));
                __asm__ __volatile__ ("  sw %0, 0(%1) \n" : :  "r" (dummy0), "r" ((unsigned int)temp_p) );
            }

            if(((unsigned long)mem & 0xE0000000UL) != 0xA0000000UL)
                dma_cache_wback_inv((unsigned long)mem, TOTAL_BLOCKS * 1024);

            tick_value_end = read_c0_count();
            if(tick_value_end > tick_value) {
                time_in_double_cpu_clocks += (tick_value_end - tick_value);
            } else {
                time_in_double_cpu_clocks += (tick_value_end + ((u32)0xFFFFFFFF - tick_value));
            }

            kb += TOTAL_BLOCKS;
        } while(time_in_double_cpu_clocks < MESS_LAENGE);
        if(irqsave) { local_irq_restore(flags); }
        printk("*");
    }


    printk("\n");
    return kb;
}

/*------------------------------------------------------------------------------------------*\
 * Simple Write
 *
 * -1x 4-Byte Schreiben
 *      -> 1 Zugriff pro Schleifendurchlauf
\*------------------------------------------------------------------------------------------*/
static unsigned long do_measure__write(char *mem, int irqsave, int loops) {
    unsigned short j;
    int i;
    register volatile int *__local_mem;
    unsigned long flags;
    unsigned long kb = 0;


    for (i = 0; i < loops; i++){
        u32 time_in_double_cpu_clocks = 0;
        if(irqsave) { local_irq_save(flags); }

        if(((unsigned long)mem & 0xE0000000UL) != 0xA0000000UL)
            dma_cache_inv((unsigned long)mem, TOTAL_BLOCKS * 1024);

        do {
            u32 tick_value;
            u32 tick_value_end;
            volatile int *local_mem = (volatile int *)mem;
            tick_value = read_c0_count();

            /*--- for(j = 0, __local_mem = local_mem; j < 1024 * 16; j ++ , __local_mem ++) { ---*/ /*--- 1024 durchlauefe ---*/
            for(j = 0, __local_mem = local_mem; j < 1024; j ++ , __local_mem +=16) { /*--- 1024 durchlauefe ---*/
                register int p = (int)__local_mem;
                register int dummy0 = 23;

                __asm__ __volatile__ ("  sw %0, 0(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0, 4(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0, 8(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,12(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,16(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,20(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,24(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,28(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,32(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,36(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,40(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,44(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,48(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,52(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,56(%1) \n" : :  "r" (dummy0), "r" (p) );
                __asm__ __volatile__ ("  sw %0,60(%1) \n" : :  "r" (dummy0), "r" (p) );
            }

            if(((unsigned long)mem & 0xE0000000UL) != 0xA0000000UL)
                dma_cache_wback_inv((unsigned long)mem, TOTAL_BLOCKS * 1024);

            tick_value_end = read_c0_count();
            if(tick_value_end > tick_value) {
                time_in_double_cpu_clocks += (tick_value_end - tick_value);
            } else {
                time_in_double_cpu_clocks += (tick_value_end + ((u32)0xFFFFFFFF - tick_value));
            }

            kb += TOTAL_BLOCKS;
        } while(time_in_double_cpu_clocks < MESS_LAENGE);
        if(irqsave) { local_irq_restore(flags); }
        printk("*");
    }

    printk("\n");
    return kb;
}


/*------------------------------------------------------------------------------------------*\
 * Mem-Test von Knut Dettmer (Lantiq-Mitarbeiter)
 *
 * KD: simple memory thruput test (MIPS cached memory address space). 
\*------------------------------------------------------------------------------------------*/
static inline signed long get_timer(long value) {
    return (signed long)(read_c0_count()) - value;
}

int do_dettmer_bench(char *argv_read, char* argv_mb, char *argv_mhz)
{
    register ulong data=0xdeadbeaf;
    volatile ulong	*p_addr;
    signed long start;
    ulong stop = 0;
    ulong	read, MB,freq;
    int 	i=1;
    int     rcode = 0;
    ulong mSeconds, thruput;



    // 1st parameter defines whether read or write should be tested (0-> read, 1->write)
    /*--- if (argc > 1) { ---*/
    read = (ulong)simple_strtoul(argv_read, NULL, 16);
    /*--- } else { ---*/
    /*--- read=0; ---*/
    /*--- } ---*/
    // 2nd parameter defines how many 500MBs should be read/written in hex
    /*--- if (argc > 2) { ---*/
    MB = (ulong)simple_strtoul(argv_mb, NULL, 16);
    MB = MB * 512;
    /*--- } else { ---*/
    /*--- MB = 4000; ---*/
    /*--- } ---*/
    //3rd parameters tells whether this is for 166.5 or 196.6MHz (0->166.5, 1->196.6)
    /*--- if (argc > 3) { ---*/
    freq = (ulong)simple_strtoul(argv_mhz, NULL, 16);
    /*--- } else { ---*/
    /*--- freq = 0; ---*/
    /*--- } ---*/
    printk(KERN_EMERG "Starting ddr ");
    if (read==0) printk(KERN_EMERG "write ");
    else printk(KERN_EMERG "read ");
    printk(KERN_EMERG "stress test for ");
    if (freq == 0) printk(KERN_EMERG "166.5 MHz ");
    else printk(KERN_EMERG "196.6 MHz ");
    printk(KERN_EMERG "ddr clock frequency\n");
    printk(KERN_EMERG "%liMB will be ",MB);
    if (read==0) printk(KERN_EMERG "written\n");
    else printk(KERN_EMERG "read\n");


    if (read==0) 
    {
        char *kmem = kmalloc(512, GFP_ATOMIC);
        uint32_t counter = 0;
        if(!kmem) {
            printk(KERN_EMERG "[%s:%d]error: kmalloc failed\n", __FUNCTION__, __LINE__);
            return -1;
        }
        // set start address
        // get starting time
        start = get_timer(0);
        //printk(KERN_EMERG "get_timer(0)=0x%8.8x\n", start);

        for (;;)
        {	// 128 writes in loop
            counter++;
            p_addr=(ulong*)kmem;

            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;
            *p_addr++=data;

            // reset address if 1MB was written
            if (counter >= 2048) {
                counter = 0;
                // decide here how many MBs should be written
                stop += (unsigned long)get_timer(start);

                if (i==MB)
                {	
                    // get end time
                    //printk(KERN_EMERG "get_timer(start)=0x%8.8x\n", stop);
                    printk(KERN_EMERG "%uMB written\n", i);
                    // calculate thruput
                    if (freq==0) mSeconds=stop/166500;
                    else mSeconds=stop/196699;
                    printk(KERN_EMERG "in %li mseconds\n", mSeconds);
                    thruput=i*1024/mSeconds;
                    printk(KERN_EMERG "==>> %liMB/s thruput\n", thruput);
                    return 1;
                }
                i++;
                start = get_timer(0);
            }
        }
        kfree(kmem);

    }else
    {
        //start read address
        p_addr=(ulong*)0x80100000;
        //get start time
        start = get_timer(0);

        for (;;)
        {	//128 reads in loop
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;
            data=*p_addr++;

            // reset address when 1 MB was read
            if (p_addr>=(ulong*)0x80200000) 
            {
                p_addr=(ulong*)0x80100000;
                // decide here how many MBs should be read
                stop += (unsigned long long)get_timer(start);
                if (i==MB)
                {
                    // get end time and calculate thruput
                    //printk(KERN_EMERG "get_timer(start)=0x%8.8x\n", stop);
                    printk(KERN_EMERG "%uMB read\n", i);
                    if (freq==0) mSeconds=stop/166500;
                    else mSeconds=stop/196600;
                    printk(KERN_EMERG "in %li mseconds\n", mSeconds);
                    thruput=i*1024/mSeconds;
                    printk(KERN_EMERG "==>> %liMB/s thruput\n", thruput);
                    return 1;
                }
                i++;
                start = get_timer(0);
            }
        }
    }
    return rcode;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void print_head(char *buf, off_t off, int *len, int loops, int wortbreite) {

    *len += sprintf(buf + off + *len, "\n");
    *len += sprintf(buf + off + *len, "\n");

    *len += sprintf(buf + off + *len, "AVM-RAM-Benchmark\n");
    *len += sprintf(buf + off + *len, "=============================================\n");
    *len += sprintf(buf + off + *len, "IRQs: off (alle Tests mit deaktivierten IRQs)\n");
    *len += sprintf(buf + off + *len, "CPU-Clock: %u\n", CPU_CLOCK);
    *len += sprintf(buf + off + *len, "RAM-Clock: %u (eff. Datentaktrate)\n", BUS_CLOCK);
    *len += sprintf(buf + off + *len, "BUS-Breite (Word=): %d Bit\n", wortbreite);
    *len += sprintf(buf + off + *len, "Measure-Time: %d * %d.%ds\n\n", loops, ZEIT_S, ZEIT_MS);

    *len += sprintf(buf + off + *len,     " -- Results --\n");
    *len += sprintf(buf + off + *len,     "=============================================================================\n");
    *len += sprintf(buf + off + *len,     " type             | total read | loops | DDR-Ticks | %2dBit   |\n", wortbreite);
    *len += sprintf(buf + off + *len,     "                  |    in kb   |       | /%2dBit    | Worte/s | kB/s\n", wortbreite);
    *len += sprintf(buf + off + *len,     "=============================================================================\n");
	udelay(100);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void print_read_pipe(char *buf, off_t off, int *len, int loops, int wortbreite, char *kmem) {
    unsigned long kb;

    kb = do_measure__read_pipe(kmem, 1, loops);
    {
        *len += sprintf(buf + off + *len, "read              | %7lu    | %1d     | %5lu.%03lu | %7lu | %6lu\n", kb,
                                                                                                                      loops,
                                                                                                                      WORTE_PRO_CLOCK_1(wortbreite), WORTE_PRO_CLOCK_10(wortbreite),
                                                                                                                      WORTE_PRO_SEC(wortbreite),
                                                                                                                      KB_PRO_SEC);
        *len += sprintf(buf + off + *len, "Pipeline-friendly |            |       |           |         |\n");
        *len += sprintf(buf + off + *len, "Lesen aus dem RAM mit optimaler Unterstuetzung der Pipline.  |\n");
        *len += sprintf(buf + off + *len, "D.h. der Code ist gewaehlt, dass die Pipeline nicht geleert  |\n");
        *len += sprintf(buf + off + *len, "werden muss und so keine Zeit verschwendet wird.             |\n");
        *len += sprintf(buf + off + *len, "-----------------------------------------------------------------------------\n");
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void print_read_extreme(char *buf, off_t off, int *len, int loops, int wortbreite, char *kmem) {
    unsigned long kb;

    //kb = do_measure__read_extreme(64*1024*1024, 1, loops);
    kb = do_measure__read_extreme(kmem, 1, loops);
    {
        *len += sprintf(buf + off + *len, "read              | %7lu    | %1d     | %5lu.%03lu | %7lu | %6lu\n", kb,
                                                                                                                      loops,
                                                                                                                      WORTE_PRO_CLOCK_1(wortbreite), WORTE_PRO_CLOCK_10(wortbreite),
                                                                                                                      WORTE_PRO_SEC(wortbreite),
                                                                                                                      KB_PRO_SEC);
        *len += sprintf(buf + off + *len, "extrema           |            |       |           |         |\n");
        *len += sprintf(buf + off + *len, "Die gelesenen Werte stehen im Speicher nicht hintereinander. |\n");
        *len += sprintf(buf + off + *len, "D.h. die CPU kann den Cache nicht nutzen.                    |\n");
        *len += sprintf(buf + off + *len, "-----------------------------------------------------------------------------\n");
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void print_readwrite(char *buf, off_t off, int *len, int loops, int wortbreite, char *kmem) {
    unsigned long kb;

    kb = do_measure__read_mixture(kmem, 1, loops);
    {
        *len += sprintf(buf + off + *len, "read/write        | %7lu    | %1d     | %5lu.%03lu | %7lu | %6lu\n", kb,
                                                                                                                      loops,
                                                                                                                      WORTE_PRO_CLOCK_1(wortbreite), WORTE_PRO_CLOCK_10(wortbreite),
                                                                                                                      WORTE_PRO_SEC(wortbreite),
                                                                                                                      KB_PRO_SEC);
        *len += sprintf(buf + off + *len, "                  |            |       |           |         |\n");
        *len += sprintf(buf + off + *len, "Immer schoen im Wechsel 1x Lesen und 1x Schreiben.           |\n");
        *len += sprintf(buf + off + *len, "-----------------------------------------------------------------------------\n");
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void print_write(char *buf, off_t off, int *len, int loops, int wortbreite, char *kmem) {
    unsigned long kb;

    kb = do_measure__write(kmem, 1, loops);
    {
        *len += sprintf(buf + off + *len, "write             | %7lu    | %1d     | %5lu.%03lu | %7lu | %6lu\n", kb,
                                                                                                                      loops,
                                                                                                                      WORTE_PRO_CLOCK_1(wortbreite), WORTE_PRO_CLOCK_10(wortbreite),
                                                                                                                      WORTE_PRO_SEC(wortbreite),
                                                                                                                      KB_PRO_SEC);
        *len += sprintf(buf + off + *len, "                  |            |       |           |         |\n");
        *len += sprintf(buf + off + *len, "Einfaches Schreiben.                                         |\n");
        *len += sprintf(buf + off + *len, "-----------------------------------------------------------------------------\n");
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int do_complete_membench(char *buf, char **start, off_t off, int count, int *eof, void *data) {
    int len = 0;
	int loops = 1;
	int wortbreite = WORTBREITE;
    char *kmem = kmalloc(TOTAL_BLOCKS * 1024, GFP_ATOMIC);

    if(kmem) {
        print_head(buf, off, &len, loops, wortbreite);

        print_read_pipe(buf, off, &len, loops, wortbreite, kmem);
        print_read_extreme(buf, off, &len, loops, wortbreite, kmem);
        print_readwrite(buf, off, &len, loops, wortbreite, kmem);
        print_write(buf, off, &len, loops, wortbreite, kmem);

        len += sprintf(buf + off + len, "\n");
        len += sprintf(buf + off + len, "\n");

        kfree(kmem);
    } else {
        len += sprintf(buf + off + len, "No memory for test\n");
    }

    *eof = 1;
    return len;
}

static int do_help(char *buf, char **start, off_t off, int count, int *eof, void *data) {
    int len = 0;

    len += sprintf(buf + off + len, "CP0-Config0: 0x%08x\n", read_c0_config());
    len += sprintf(buf + off + len, "\n");
    len += sprintf(buf + off + len, "\n");
    len += sprintf(buf + off + len, "AVM-RAM-Benchmark (HELP)\n");
    len += sprintf(buf + off + len, "=============================================\n");
    len += sprintf(buf + off + len, "cat /proc/avm/complete        -> Durchfuehrung aller Benchmarks\n");
    len += sprintf(buf + off + len, "cat /proc/avm/help            -> Anzeige dieser Hilfe\n");
    len += sprintf(buf + off + len, "\n");
    len += sprintf(buf + off + len, "cat /proc/avm/do_read_extreme -> Read Bench\n");
    len += sprintf(buf + off + len, "                                 Lese benchmark fuer nicht-lineares Lesen.\n");
    len += sprintf(buf + off + len, "cat /proc/avm/do_read_pipe    -> Read Bench\n");
    len += sprintf(buf + off + len, "                                 Pipeline orientierter Lese benchmark\n");
    len += sprintf(buf + off + len, "cat /proc/avm/do_read_write   -> Read/Schreib Bench\n");
    len += sprintf(buf + off + len, "cat /proc/avm/do_write        -> Schreib Bench\n");
    len += sprintf(buf + off + len, "\n");
    len += sprintf(buf + off + len, "cat /proc/avm/do_dettmer_read -> Lantiq Read Bench\n");
    len += sprintf(buf + off + len, "cat /proc/avm/do_dettmer_write-> Lantiq Schreib Bench\n");
    len += sprintf(buf + off + len, "\n");
#ifdef CONFIG_MACH_AR934x
    len += sprintf(buf + off + len, "cat /proc/avm/do_atheros_pctrace                    ->  performance counter log (on idle)\n");
    len += sprintf(buf + off + len, "cat /proc/avm/do_atheros__hogging                   ->  ddr hogging test\n");
#endif /*--- #ifdef CONFIG_MACH_AR934x ---*/
    len += sprintf(buf + off + len, "\n");

    *eof = 1;
    return len;
}

static int do_read_extreme(char *buf, char **start, off_t off, int count, int *eof, void *data) {
    int len = 0;
	int loops = 1;
    int wortbreite = WORTBREITE;

    char *kmem = kmalloc(TOTAL_BLOCKS * 1024, GFP_ATOMIC);

    if(kmem) {
        print_head(buf, off, &len, loops, wortbreite);

        print_read_extreme(buf, off, &len, loops, wortbreite, kmem);

        len += sprintf(buf + off + len, "\n");
        len += sprintf(buf + off + len, "\n");

        kfree(kmem);
    } else {
        len += sprintf(buf + off + len, "No memory for test\n");
    }

    *eof = 1;
    return len;
}

static int do_read_pipe(char *buf, char **start, off_t off, int count, int *eof, void *data) {
    int len = 0;
	int loops = 1;
    int wortbreite = WORTBREITE;

    char *kmem = kmalloc(TOTAL_BLOCKS * 1024, GFP_ATOMIC);

    if(kmem) {
        print_head(buf, off, &len, loops, wortbreite);

        print_read_pipe(buf, off, &len, loops, wortbreite, kmem);

        len += sprintf(buf + off + len, "\n");
        len += sprintf(buf + off + len, "\n");

        kfree(kmem);
    } else {
        len += sprintf(buf + off + len, "No memory for test\n");
    }

    *eof = 1;
    return len;
}

static int do_read_write(char *buf, char **start, off_t off, int count, int *eof, void *data) {
    int len = 0;
	int loops = 1;
    int wortbreite = WORTBREITE;

    char *kmem = kmalloc(TOTAL_BLOCKS * 1024, GFP_ATOMIC);

    if(kmem) {
        print_head(buf, off, &len, loops, wortbreite);

        print_readwrite(buf, off, &len, loops, wortbreite, kmem);

        len += sprintf(buf + off + len, "\n");
        len += sprintf(buf + off + len, "\n");

        kfree(kmem);
    } else {
        len += sprintf(buf + off + len, "No memory for test\n");
    }

    *eof = 1;
    return len;
}

static int do_write(char *buf, char **start, off_t off, int count, int *eof, void *data) {
    int len = 0;
	int loops = 1;
    int wortbreite = WORTBREITE;

    char *kmem = kmalloc(TOTAL_BLOCKS * 1024, GFP_ATOMIC);

    if(kmem) {
        print_head(buf, off, &len, loops, wortbreite);

        print_write(buf, off, &len, loops, wortbreite, kmem);

        len += sprintf(buf + off + len, "\n");
        len += sprintf(buf + off + len, "\n");

        kfree(kmem);
    } else {
        len += sprintf(buf + off + len, "No memory for test\n");
    }

    *eof = 1;
    return len;
}

static int do_dettmer_read(char *buf, char **start, off_t off, int count, int *eof, void *data) {
    do_dettmer_bench("1", "0x1", "0x1"); // 1 .. read | 0x1 .. 500MB Test | 0x1 .. 196.6MHz

    *eof = 1;
    return sprintf(buf + off, "Lantiq Read Benchmark. (Set LogLevel 0 to see the results..)\n");
}

static int do_dettmer_write(char *buf, char **start, off_t off, int count, int *eof, void *data) {
    do_dettmer_bench("0", "0x1", "0x1"); // 0 .. write | 0x1 .. 500MB Test | 0x1 .. 196.6MHz

    *eof = 1;
    return sprintf(buf + off, "Lantiq Write Benchmark. (Set LogLevel 0 to see the results..)\n");
}


/*------------------------------------------------------------------------------------------*\
 * ATHEROS
\*------------------------------------------------------------------------------------------*/
#ifdef CONFIG_MACH_AR934x

#define DDRMON_CTL_CLEAR_ALL_CNT            (1 << 0)
#define DDRMON_CTL_CLEAR_MAX_LATENCY_CNT    (1 << 1)
#define DDRMON_CTL_DISABLE_LATENCY_REFRESH  (1 << 2)
#define DDRMON_CTL_CLIENT_SEL_CPU           (0 << 8)
#define DDRMON_CTL_CLIENT_SEL_USB_MBOX      (3 << 8)
#define CLEAR_ALL (DDRMON_CTL_CLEAR_ALL_CNT | DDRMON_CTL_CLEAR_MAX_LATENCY_CNT | DDRMON_CTL_DISABLE_LATENCY_REFRESH)

// Optional
noinline int ddr_act_init(void)
{
    asm(
    " li $t0, 0xbd007f00\n"
    " li $t1, 0x500ddeed\n"
    " sw $t1, 0x0($t0)\n"
    "li $t0, 0x82100000\n"   /* DDR_START_ADDRESS */
    "   li $t1, 0x821a0000\n"   /* DDR_END_ADDRESS */
    "_init_seq:\n"
    "   sw $t0, 0x0($t0)\n"
    "   addiu $t0, $t0, 4\n"
    "   bne $t0, $t1, _init_seq\n"
    "   nop\n");
    return 0;
}
/*------------------------------------------------------------------------------------------*\
 * DDR CPU HOG
\*------------------------------------------------------------------------------------------*/
noinline void ddr_act(unsigned int start, unsigned int end)
{
    asm("STRT_DDR_TXNS:\n"
    "   li $t9, 0x100\n"
    "_outer_ddr_rw_loop:\n"
    "   ori $t0, %[ddr_start], 0\n"
    "   addiu $t8, %[ddr_start], 0x8000\n"
    "   li $t1, 0x8\n"  /* NO_CHNGES_WITHIN_CACHE_LINE */
    "_inner_ddr_rw_loop:\n"
    "   lw $t2, 0x0($t0)\n"
    "   lw $t5, 0x2000($t0)\n"
    "   lw $t6, 0x4000($t0)\n"
    "   lw $t7, 0x6000($t0)\n"
    "   addiu $t2, $t2, 1\n"
    "   sw $t2, 0x0($t8)\n"
    "   sw $t5, 0x2000($t8)\n"
    "   sw $t6, 0x4000($t8)\n"
    "   sw $t7, 0x6000($t8)\n"
    "   addiu $t0, $t0, 4\n"
    "   addiu $t8, $t8, 4\n"
    "   addiu $t1, $t1, -1\n"
    "   nop\n"
    "   bnez $t1, _inner_ddr_rw_loop\n"
    "   nop\n"
    "_changes_before_outer_loop:\n"
    "   addiu %[ddr_start], %[ddr_start], 32\n"     /* DDR_INCR_COUNT */
    "   bne %[ddr_start], %[ddr_end], _outer_ddr_rw_loop\n"
    "   nop\n"
    "   nop\n"
    : : [ddr_start] "r" (start), [ddr_end] "r" (end) );
}

/*------------------------------------------------------------------------------------------*\
 * ISR for timer
\*------------------------------------------------------------------------------------------*/
#define TEST_ELEMENTS   6
#define TEST_SIZE (1024 * TEST_ELEMENTS)
static volatile unsigned int ath_pctrace_mode = 3;
static unsigned int gcnt = 0;
volatile unsigned int soff = 0;
unsigned int test_buffer[TEST_SIZE];

irqreturn_t ath_timer_intr(int irq, void *dev_id)
{

#if 0
    ath_pctrace_mode++;
    if (ath_pctrace_mode > 7) {
        ath_pctrace_mode = 0;
    }
    ath_pctrace_mode = 3;
#endif

#if 0
    /*--- Performace count registers for DDR ---*/
    *(test_buffer + soff + 0) = ath_reg_rd(0xb80000ec);    /*--- ALL_GRANTS ---*/ 
    *(test_buffer + soff + 1) = ath_reg_rd(0xb80000f0);    /*--- ALL_DUR_L ---*/  
    /*--- *(test_buffer + soff + 2) = ath_reg_rd(0xb80000f4); ---*/
    *(test_buffer + soff + 2) = ath_reg_rd(0xb80000f8);    /*--- SEL_GRANTS ---*/ 
    *(test_buffer + soff + 3) = ath_reg_rd(0xb80000fc);    /*--- SEL_DUR_L ---*/  
    /*--- *(test_buffer + soff + 5) = ath_reg_rd(0xb8000100); ---*/
    *(test_buffer + soff + 4) = ath_reg_rd(0xb8000104);    /*--- MAX_LATENCY ---*/ 
    /*--- *(test_buffer + soff + 5) = gcnt | (ath_pctrace_mode << 16); ---*/
    *(test_buffer + soff + 5) = ath_pctrace_mode;
    /*--- *(test_buffer + soff + 7) = gcnt | (gcntinvalid << 28); ---*/
#else
    test_buffer[soff + 0] = ath_reg_rd(0xb80000ec);    /*--- ALL_GRANTS ---*/ 
    test_buffer[soff + 1] = ath_reg_rd(0xb80000f0);    /*--- ALL_DUR_L ---*/  
    test_buffer[soff + 2] = ath_reg_rd(0xb80000f8);    /*--- SEL_GRANTS ---*/ 
    test_buffer[soff + 3] = ath_reg_rd(0xb80000fc);    /*--- SEL_DUR_L ---*/  
    test_buffer[soff + 4] = ath_reg_rd(0xb8000104);    /*--- MAX_LATENCY ---*/ 
    /*--- test_buffer + soff + 5) = gcnt | (ath_pctrace_mode << 16); ---*/
    test_buffer[soff + 5] = ath_pctrace_mode;
#endif

    /*--- Reset Performance Counters ---*/
#if 0
    ath_reg_wr(0xb80000e8, DDRMON_CTL_CLIENT_SEL_USB_MBOX | CLEAR_ALL);
    ath_reg_wr(0xb80000e8, DDRMON_CTL_CLIENT_SEL_USB_MBOX);
#else
    ath_reg_wr(0xb80000e8, (ath_pctrace_mode << 8) | CLEAR_ALL);
    ath_reg_wr(0xb80000e8, (ath_pctrace_mode << 8));
#endif
    gcnt++;

    soff += TEST_ELEMENTS;

    if (soff > TEST_SIZE) {
        gcnt = 0;
        soff = 0;
    }

    return IRQ_HANDLED;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int do_atheros_pctrace_read(char *page, char **start, off_t off, int count, int *eof, void *data) {

    unsigned int index = soff, loop = 0;

    printk( "Atheros DMA Benchmark\n"
            "SELECTION = USB/MBOX(SLIC)\n\n"
            "ERROR COUNT   ALL_GRANTS ALL_DUR_L  SEL_GRANTS SEL_DUR_L  MAX_LATENCY\n"
            "      500us   0xB80000EC 0xB80000F0 0xB80000F8 0xB80000FC 0xB8000104 \n");

    /*--- printk( KERN_ERR "mode %d\n", ath_pctrace_mode); ---*/

    while (loop < TEST_SIZE) {
        printk( "0x%05x:  0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
                test_buffer[index+5], test_buffer[index+0], test_buffer[index+1], 
                test_buffer[index+2], test_buffer[index+3], test_buffer[index+4]);

        index += TEST_ELEMENTS;
        loop += TEST_ELEMENTS;
        if (index > TEST_SIZE) {
            index = 0;
        }
    }

    *eof = 1;
    return 0;

}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define PCTRACE_MAX_BUFFER 32
static int do_atheros_hogging(struct file *file, const char *buffer, unsigned long count, void *data) {
    char local_buffer[PCTRACE_MAX_BUFFER];
    unsigned char *memarea;
    unsigned int len;
    long cnt;

    memarea = kmalloc(0x10000 +(2 * PAGE_SIZE), GFP_ATOMIC);
    if(!memarea) {
        printk(KERN_ERR "Hogging test: Failed to reserve memory!\n");
        return -1;
    }

    if (count > PCTRACE_MAX_BUFFER)
        len = PCTRACE_MAX_BUFFER;
    else
        len = count;
	if(copy_from_user(local_buffer, buffer, len))
		return -EFAULT;
	local_buffer[len] = '\0';

    cnt = simple_strtol(local_buffer, NULL, 10);
    if(cnt < 1) {
        printk(KERN_ERR "Error: Invalid hogging test count %ld\n", cnt);
        return -1;
    }

    while(cnt) {
        ddr_act((unsigned int)memarea + PAGE_SIZE, 
                (unsigned int)memarea + PAGE_SIZE + 0x10000);
        /*--- if(cnt%1000 == 0) { ---*/
            /*--- schedule(); ---*/
        /*--- } ---*/
        cnt--;
    }

    kfree(memarea);
    return len;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int do_atheros_pctrace(struct file *file, const char *buffer, unsigned long count, void *data) {

    int len;
    char local_buffer[PCTRACE_MAX_BUFFER];

    if (count > PCTRACE_MAX_BUFFER)
        len = PCTRACE_MAX_BUFFER;
    else
        len = count;

	if(copy_from_user(local_buffer, buffer, len))
		return -EFAULT;

	local_buffer[len] = '\0';

    if ( ! strncmp(local_buffer, "start", sizeof("start") - 1)) {   /*--- init ---*/

        printk(KERN_ERR "%s start\n", __func__);
        /* Reset Performance Counters */
        ath_reg_wr(0xb80000e8, DDRMON_CTL_CLIENT_SEL_USB_MBOX | CLEAR_ALL);
        ath_reg_wr(0xb80000e8, DDRMON_CTL_CLIENT_SEL_USB_MBOX);

        ath_reg_wr(ATH_GENERAL_TMR2_RELOAD, 0x4e20);   	// 500 us
        //ath_reg_wr(ATH_GENERAL_TMR_RELOAD, 0x2625a00);	// 1s
        if(request_irq(ATH_MISC_IRQ_TIMER2, ath_timer_intr, IRQF_DISABLED, "ath_pctrace_timer", NULL)) {
            printk(KERN_ERR "[%s]: Failed to register general purpose timer interrupt\n", __FUNCTION__);
        }
        return len;
    } 

    if ( ! strncmp(local_buffer, "stop", sizeof("stop") - 1)) {   /*--- exit ---*/
        printk(KERN_ERR "%s stop\n", __func__);
        free_irq(ATH_MISC_IRQ_TIMER2, NULL);
        return len;
    }

    if ( ! strncmp(local_buffer, "mode", sizeof("mode") - 1)) {   /*--- exit ---*/
        ath_pctrace_mode = (unsigned int)(local_buffer[5] - '0') & 0xF;
        /*--- printk(KERN_ERR "%s mode -%s- \"%d\"\n", __func__, local_buffer, ath_pctrace_mode); ---*/
        if ((ath_pctrace_mode > 0) && (ath_pctrace_mode < 8)) {
            return len;
        } else {
            printk(KERN_ERR "%s: mode not set\n", __func__);
            ath_pctrace_mode = 0;
        }
    }

    printk(KERN_ERR "%s: use \"start\", \"stop\" or \"mode [0..7]\"\n", __func__);
    return len;

}
#endif /*--- #ifdef CONFIG_MACH_AR934x ---*/


static int performance_index(char *buf, char **start, off_t off, int count, int *eof, void *data) {
#define KB_VALUE_PRO_SEC(x)          ((x / loops) * 1000/(ZEIT_S * 1000 + ZEIT_MS))
    int len = 0;
	int loops = 1;

    char *kmem = kmalloc(TOTAL_BLOCKS * 1024, GFP_ATOMIC);

    if(kmem) {
        unsigned long kb_r_burst;
        unsigned long kb_w_burst;
        unsigned long kb_rw;
        unsigned long kb_r;

        kb_r_burst = do_measure__read_pipe(kmem, 1, loops);
        kb_w_burst = do_measure__write(kmem, 1, loops);
        kb_rw = do_measure__read_mixture(kmem, 1, loops);
        kb_r = do_measure__read_extreme(kmem, 1, loops);

        len += sprintf(buf + off + len, "Performance-Index: %lu\n",
                         KB_VALUE_PRO_SEC(kb_r_burst)/1000*10
                       + KB_VALUE_PRO_SEC(kb_w_burst)/1000*10
                       + KB_VALUE_PRO_SEC(kb_rw)/1000*1
                       + KB_VALUE_PRO_SEC(kb_r)/1000*1
                       );
        len += sprintf(buf + off + len, "CPU-Clock: %u MHz\n", CPU_CLOCK/(1000*1000));
        len += sprintf(buf + off + len, "RAM-Clock: %u MHz\n", BUS_CLOCK/(1000*1000));

        kfree(kmem);
    } else {
        len += sprintf(buf + off + len, "No memory for test\n");
    }

    *eof = 1;
    return len;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void early_membench(void) {
	static char buffer[1024];
	int eof;
	int len;
	printk( KERN_ERR "running membench\n");
	len = do_complete_membench(buffer, NULL, 0, 0, &eof, NULL);
	BUG_ON(len >= 1024);
	buffer[len] = '\0';
	printk( KERN_ERR "%s", buffer);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct proc_dir_entry *dir;

#ifdef CONFIG_MACH_AR934x
static struct proc_dir_entry *ath_pctrace;
#endif

int __init avm_membench_init(void) {

	dir = proc_mkdir("avm", NULL);

	printk("[%s]\n",__FUNCTION__);

    create_proc_read_entry("complete",         0, dir, do_complete_membench, NULL);
    create_proc_read_entry("help",             0, dir, do_help, NULL);
    create_proc_read_entry("do_read_extreme",  0, dir, do_read_extreme, NULL);
    create_proc_read_entry("do_read_pipe",     0, dir, do_read_pipe, NULL);
    create_proc_read_entry("do_read_write",    0, dir, do_read_write, NULL);
    create_proc_read_entry("do_write",         0, dir, do_write, NULL);
    create_proc_read_entry("do_dettmer_read",  0, dir, do_dettmer_read, NULL);
    create_proc_read_entry("do_dettmer_write", 0, dir, do_dettmer_write, NULL);
#ifdef CONFIG_MACH_AR934x
    ath_pctrace = create_proc_entry("do_atheros_hogging", 0222, dir);
    if (ath_pctrace) {
        ath_pctrace->write_proc = do_atheros_hogging;
    }
    ath_pctrace = create_proc_entry("do_atheros_pctrace", 0644, dir);
    if (ath_pctrace) {
        ath_pctrace->read_proc = do_atheros_pctrace_read;
        ath_pctrace->write_proc = do_atheros_pctrace;
    }
#endif /*--- #ifdef CONFIG_MACH_AR934x ---*/
    create_proc_read_entry("performance_index",0, dir, performance_index, NULL);
	return 0;
}

void avm_membench_exit(void) {
    remove_proc_entry("complete", dir);
    remove_proc_entry("help", dir);

    remove_proc_entry("do_read_extreme",  dir);
    remove_proc_entry("do_read_pipe",     dir);
    remove_proc_entry("do_read_write",    dir);
    remove_proc_entry("do_write",         dir);
    remove_proc_entry("do_dettmer_read",  dir);
    remove_proc_entry("do_dettmer_write", dir);
#ifdef CONFIG_MACH_AR934x
    remove_proc_entry("do_atheros_hogging", dir);
    remove_proc_entry("do_atheros_pctrace", dir);
#endif /*--- #ifdef CONFIG_MACH_AR934x ---*/
    remove_proc_entry("performance_index",dir);

    remove_proc_entry("avm", NULL);
}

module_init(avm_membench_init);
module_exit(avm_membench_exit)

