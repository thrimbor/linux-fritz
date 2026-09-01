/*
 * This file contains glue for Atheros ar7240 spi flash interface
 * Primitives are ar7240_spi_*
 * mtd flash implements are ar7240_flash_*
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/plat_ar7240_flash.h>
#include <asm/delay.h>
#include <asm/cacheflush.h>
#include <asm/r4kcache.h>
#include <asm/io.h>
#include <asm/div64.h>
#ifdef CONFIG_MACH_AR7240
#include "ar7240.h"
#else
#include "atheros.h"
#endif
#include "ar7240_flash.h"
#define MTD_AVM_PARTITION_PARSER

#define AR7240_FLASH_SIZE_2MB          (2*1024*1024)
#define AR7240_FLASH_SIZE_4MB          (4*1024*1024)
#define AR7240_FLASH_SIZE_8MB          (8*1024*1024)
#define AR7240_FLASH_SIZE_16MB          (16*1024*1024)
#define AR7240_FLASH_SECTOR_SIZE_64KB  (64*1024)
#define AR7240_FLASH_PG_SIZE_256B       256
#ifdef CONFIG_MACH_AR7240
#define AR7240_FLASH_NAME               "ar7240-nor"
#else
#define AR7240_FLASH_NAME               "ath-nor"
#endif

#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
static spinlock_t nmi_lock = SPIN_LOCK_UNLOCKED;

/*--- #define FLASH_ACCESS_STATISTIC ---*/
#if defined(FLASH_ACCESS_STATISTIC)
struct _generic_stat {
    unsigned long long avg;
    unsigned long cnt; 
    unsigned long min;
    unsigned long max;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void init_generic_stat(struct _generic_stat *pgstat) {
    pgstat->min = ULONG_MAX;
    pgstat->max = 0;
    pgstat->cnt = 0;
    pgstat->avg = 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void generic_stat(struct _generic_stat *pgstat, signed long val) {
    if(pgstat->cnt == 0) {
        init_generic_stat(pgstat);
    }
    if(val > pgstat->max) pgstat->max = val;
    if(val < pgstat->min) pgstat->min = val;
    pgstat->avg += (unsigned long long) val;
    pgstat->cnt++;
}
#define CYCLE_MHZ  280
#define CLK_TO_USEC(a) ((a) / CYCLE_MHZ)
/*--------------------------------------------------------------------------------*\
 * timebase: 0 unveraendert, 1: clk ->  usec
 * reset: Statistik ruecksetzen
\*--------------------------------------------------------------------------------*/
static void display_generic_stat(char *prefix, struct _generic_stat *pgstat, unsigned int timebase, unsigned int reset) {
    struct _generic_stat gstat;
    unsigned long flags, tmp;
    signed long cnt = pgstat->cnt;
    spin_lock_irqsave(&nmi_lock, flags);
    if(cnt == 0) {
        spin_unlock_irqrestore(&nmi_lock, flags);
        return;
    }
    memcpy(&gstat,pgstat, sizeof(gstat));
    if(reset) {
        pgstat->cnt = 0;
    }
    spin_unlock_irqrestore(&nmi_lock, flags);
    tmp = cnt;
    while(gstat.avg > 0xFFFFFFFFLU) {
        gstat.avg  >>= 1;
        tmp        >>= 1;
    }
    switch(timebase) {
        case 0: 
            printk(KERN_ERR "%s[%lu] min=%lu max=%lu avg=%lu\n", prefix, cnt, gstat.min, gstat.max, tmp ? (long)gstat.avg / tmp : 0);
            break;
        case 1: 
            printk(KERN_ERR "%s[%lu] min=%lu max=%lu avg=%lu usec\n", prefix, cnt, CLK_TO_USEC(gstat.min), CLK_TO_USEC(gstat.max), tmp ? CLK_TO_USEC((long)gstat.avg / tmp) : 0);
            break;
    }
}
static struct _generic_stat erase_stat, write_stat, read_stat;
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void display_periodical_statistic(void) {
    static unsigned int last;
    if((jiffies - last) >  10 * HZ) {   
        last = jiffies;
        /*--- display_generic_stat("flash:read", &read_stat, 1, 1); ---*/
        display_generic_stat("flash:write", &write_stat, 1, 1);
        display_generic_stat("flash:erase", &erase_stat, 1, 1);
    }
}
#endif/*--- #if defined(FLASH_ACCESS_STATISTIC) ---*/

#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
/*------------------------------------------------------------------------------------------*\
 * Prototypes
\*------------------------------------------------------------------------------------------*/
static int __init ar7240_flash_init (void);
static void __exit ar7240_flash_exit(void);
static int __devinit ar7240_flash_probe(struct platform_device*); 
static int ar7240_flash_remove(struct platform_device *dev);

static void ar7240_spi_write_enable(void);
static void ar7240_spi_poll(unsigned int locked);
static void ar7240_spi_write_page(uint32_t addr, uint8_t *data, int len);
static void ar7240_spi_sector_erase(uint32_t addr);

/*------------------------------------------------------------------------------------------*\
 * DATA
\*------------------------------------------------------------------------------------------*/

/* this is passed in as a boot parameter by bootloader */
/*--- extern int __ath_flash_size; //TODO aus pdata uebernehmen ---*/

static struct platform_driver ar7240_flash_driver = {
    .probe  = ar7240_flash_probe,
    .remove = ar7240_flash_remove,
    .driver = {
                      .name = AR7240_FLASH_NAME,              
                  },
};
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

typedef struct ar7240_flash_geom {
    uint32_t     size;
    uint32_t     sector_size;
    uint32_t     nsectors;
    uint32_t     pgsize;
} ar7240_flash_geom_t; 

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
ar7240_flash_geom_t flash_geom_tbl[AR7240_FLASH_MAX_BANKS] = {
	{
		.size           =   AR7240_FLASH_SIZE_16MB,
		.sector_size    =   AR7240_FLASH_SECTOR_SIZE_64KB, /* TODO stimmt das?! HBL: In dem Atheros-Patch 9.6.0.16 war der Wert genauso. */
		.pgsize         =   AR7240_FLASH_PG_SIZE_256B
	}
};

static DECLARE_MUTEX(ar7240_flash_sem);

/*------------------------------------------------------------------------------------------*\
 * Locking API
\*------------------------------------------------------------------------------------------*/
void ar7240_flash_spi_down(void) {
  down(&ar7240_flash_sem);
}

void ar7240_flash_spi_up(void) {
  up(&ar7240_flash_sem);
}

/*------------------------------------------------------------------------------------------*\
 * MTD / Platform-Device-Driver Functions
\*------------------------------------------------------------------------------------------*/

static int ar7240_flash_erase(struct mtd_info *mtd,struct erase_info *instr) {
    int nsect, s_curr, s_last;
    uint64_t  res;

    if (instr->addr + instr->len > mtd->size) return (-EINVAL);

    ar7240_flash_spi_down();

    res = instr->len;
    do_div(res, mtd->erasesize);
    nsect = res;

    if (((uint32_t)instr->len) % mtd->erasesize)
        nsect ++;

    res = instr->addr;
    do_div(res,mtd->erasesize);
    s_curr = res;

    s_last  = s_curr + nsect;

    do {
        ar7240_spi_sector_erase(s_curr * AR7240_SPI_SECTOR_SIZE);
    } while (++s_curr < s_last);
#if !defined(CONFIG_NMI_ARBITER_WORKAROUND)
    ar7240_spi_done();
#endif/*--- #if !defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/

    ar7240_flash_spi_up();

    if (instr->callback) {
	instr->state |= MTD_ERASE_DONE;
        instr->callback (instr);
    }
#if defined(FLASH_ACCESS_STATISTIC)
    display_periodical_statistic();
#endif/*--- #if defined(FLASH_ACCESS_STATISTIC) ---*/

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _nmi_vector_gap {
    unsigned int in_use;
    unsigned long long start;
    unsigned long long gap_size;
    unsigned long long end;
};

struct _nmi_vector_gap nmi_vector_gap;

void set_nmi_vetor_gap(unsigned int start, unsigned int firmware_size, unsigned int gap_size) {
    nmi_vector_gap.start     = (unsigned long long)start & ((16ULL << 20) - 1ULL);   /*--- funktioniert bis zur Flashgröße von 16 MByte ---*/
    nmi_vector_gap.gap_size  = gap_size;
    nmi_vector_gap.end       = (unsigned long long)firmware_size; 
    nmi_vector_gap.in_use    = 1;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void local_memcpy(uint8_t *to, uint8_t *from, size_t len) {
    uint32_t addr = (uint32_t)from | 0xbf000000;
    flush_icache_range((uint32_t)to, (uint32_t)to + len);
    memcpy(to, (void *)addr, len);
}

static int ar7240_flash_read(struct mtd_info *mtd, loff_t from, size_t len, size_t *retlen ,u_char *buf) {
#if defined(FLASH_ACCESS_STATISTIC)
    /*--- unsigned int cycles = get_cycles(); ---*/
#endif/*--- #if defined(FLASH_ACCESS_STATISTIC) ---*/
    uint32_t addr = from;

    if (!len) return (0);
    if (from + len > mtd->size) {
        printk("%s:error: from=%llx len=%d\n", __func__, from, len);
        return (-EINVAL);
    }

    ar7240_flash_spi_down();

    /*--- memcpy(buf, (addr), len); ---*/
    /*--- wir sind jenseites/höher als der nmi vector gap aber unterhalb des JFFS ---*/
    if (nmi_vector_gap.in_use && (addr > nmi_vector_gap.start) && (addr < nmi_vector_gap.end)) {
        /*--- printk(KERN_ERR "[%s] addr 0x%x (groesser gap start, kleiner end: %llx)\n", __FUNCTION__, addr, nmi_vector_gap.end); ---*/
	    local_memcpy(buf, (uint8_t *)((unsigned long)addr + (unsigned long)nmi_vector_gap.gap_size), len);

    /*--- wir sind unterhalb des nmi vector gaps, die länge kann aber hineinreichen ---*/
    } else if (nmi_vector_gap.in_use && (addr <= nmi_vector_gap.start)) {

        /*--- eine Aufteilung ist nötig ---*/
        if(addr + len > nmi_vector_gap.start) {
            unsigned int len_part = nmi_vector_gap.start - addr;
            /*--- printk(KERN_ERR "[%s] addr 0x%x (kleiner gap start, aufteilung noetig, part 0x%x und 0x%x)\n", ---*/ 
                    /*--- __FUNCTION__, addr, len_part, len - len_part); ---*/
            if(len_part)
	            local_memcpy(buf, (uint8_t *)addr, len_part);
	        local_memcpy(buf + len_part, (uint8_t *)addr + nmi_vector_gap.gap_size + len_part, len - len_part);

        /*--- eine Aufteilung ist nicht nötig ---*/
        } else {
            /*--- printk(KERN_ERR "[%s] addr 0x%x (kleiner gap start)\n", __FUNCTION__, addr); ---*/
	        local_memcpy(buf, (uint8_t *)addr, len);
        }
    } else {
        /*--- printk(KERN_ERR "[%s] addr 0x%x (groesser end 0x%llx)\n", __FUNCTION__, addr, nmi_vector_gap.end); ---*/
	    local_memcpy(buf, (uint8_t *)addr, len);
    }
    *retlen = len;

    ar7240_flash_spi_up();

#if defined(FLASH_ACCESS_STATISTIC)
    /*--- generic_stat(&read_stat, get_cycles() - cycles); ---*/
#endif/*--- #if defined(FLASH_ACCESS_STATISTIC) ---*/
    
    return 0;
}

#if defined(ATH_SST_FLASH)
static int
ar7240_flash_write(struct mtd_info *mtd, loff_t dst, size_t len,
		   size_t * retlen, const u_char * src)
{
	uint32_t val;

	//printk("write len: %lu dst: 0x%x src: %p\n", len, dst, src);
	*retlen = len;

	for (; len; len--, dst++, src++) {
		ar7240_spi_write_enable();	// dont move this above 'for'
		ar7240_spi_bit_banger(ATH_SPI_CMD_PAGE_PROG);
		ar7240_spi_send_addr(dst);

		val = *src & 0xff;
		ar7240_spi_bit_banger(val);

		ar7240_spi_go();
		ar7240_spi_poll(0);
	}
	/*
	 * Disable the Function Select
	 * Without this we can't re-read the written data
	 */
	ath_reg_wr(ATH_SPI_FS, 0);

	if (len) {
		*retlen -= len;
		return -EIO;
	}
	return 0;
}
#else
static int ar7240_flash_write (struct mtd_info *mtd, loff_t to, size_t len, size_t *retlen, const u_char *buf) {
    int total = 0, len_this_lp, bytes_this_page;
    uint32_t addr = 0;
    u_char *mem;

    ar7240_flash_spi_down();

    /*--- printk("spi: SPI_CONTROL %08x: %08x divider=%d+1\n", ATH_SPI_CLOCK, ath_reg_rd(ATH_SPI_CLOCK), (ath_reg_rd(ATH_SPI_CLOCK) & ~(0x1 << 5))); ---*/


    while(total < len) {

        mem              = (u_char *)(buf + total);
        addr             = to  + total;
        bytes_this_page  = AR7240_SPI_PAGE_SIZE - (addr % AR7240_SPI_PAGE_SIZE);
        len_this_lp      = min(((int)len - total), bytes_this_page);

        ar7240_spi_write_page(addr, mem, len_this_lp);
        total += len_this_lp;
    }
#if !defined(CONFIG_NMI_ARBITER_WORKAROUND)
    ar7240_spi_done();
#endif/*--- #if !defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/

    ar7240_flash_spi_up();
#if defined(FLASH_ACCESS_STATISTIC)
    display_periodical_statistic();
#endif/*--- #if defined(FLASH_ACCESS_STATISTIC) ---*/

    *retlen = len;
    return 0;
}
#endif



static int __devinit ar7240_flash_probe(struct platform_device* pdev) {
    struct mtd_partition *mtd_parts = NULL; /* part-pointer wird von jffs2-parser gefuellt */
   	int  np;
    ar7240_flash_geom_t *geom;
    struct mtd_info *mtd;
    uint8_t index;
    struct ar7240_flash_data* pdata = pdev->dev.platform_data;

    if (!pdata) {
        printk(KERN_ERR "Platform data invalid");
        return -EINVAL;
    }

    init_MUTEX(&ar7240_flash_sem);

    ath_reg_wr_nf(ATH_SPI_CLOCK, 0x43);

	index = 0;
	geom  = &flash_geom_tbl[index];

	/* set flash size to value from bootloader if it passed valid value */ 
	/* otherwise use the default 4MB.                                   */
	if (pdata->flash_size >= 4 && pdata->flash_size <= 16) 
		geom->size = pdata->flash_size * 1024 * 1024;

	mtd = kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
	if (!mtd) {
		printk("Cant allocate mtd stuff\n");
		return -1;
	}
	memset(mtd, 0, sizeof(struct mtd_info));

	mtd->name               =   AR7240_FLASH_NAME;
	mtd->type               =   MTD_NORFLASH;
	mtd->flags              =   MTD_CAP_NORFLASH;
	mtd->size               =   geom->size;
	mtd->erasesize          =   geom->sector_size;
	mtd->numeraseregions    =   0;
	mtd->eraseregions       =   NULL;
	mtd->owner              =   THIS_MODULE;
	mtd->erase              =   ar7240_flash_erase;
	mtd->read               =   ar7240_flash_read;
	mtd->write              =   ar7240_flash_write;
	mtd->writesize          =   1;

	np = parse_mtd_partitions(mtd, pdata->probes, &mtd_parts, 0);

#ifdef MTD_AVM_PARTITION_PARSER
	if (np > 0) {
		/*-----------------------------------------------------------------*\
		 * Partitionenen gefunen, add_mtd_partions traegt diese als
		 * mtds in 'mtd_table' ein
		\*-----------------------------------------------------------------*/
		add_mtd_partitions(mtd, mtd_parts, np);
		return 0;

	} else {

		printk("[%s] No partitions found\n", __FUNCTION__);
	}
#endif

	/*-----------------------------------------------------------------*\
	 * keine Partitionenen gefunen, add_mtd_device 
	 * traegt das gesamte mtd 'mtd_table' ein
	\*-----------------------------------------------------------------*/
	add_mtd_device(mtd);
   	return 0;
}



static int ar7240_flash_remove(struct platform_device *dev) {
#if 0
    if(dev != NULL) {
        {
            struct avmnand__class* this = (struct avmnand__class*)platform_get_drvdata(dev);

            if(this != NULL) {
                avmnand_c__dtor(&this->data_obj);

#ifdef CONFIG_MTD_PARTITIONS
                /* Deregister partitions */
                del_mtd_partitions(&this->mtd_info);
#endif
                /* Deregister the device */
                del_mtd_device(&this->mtd_info);
            }
        }

        /*------------------------------------------------------------------------------------------*\
        \*------------------------------------------------------------------------------------------*/
        {
            struct resource *res;
            res = platform_get_resource(dev, IORESOURCE_MEM, 1);
            if (res) {
                release_mem_region(res->start, res->end - res->start + 1);
            }

            res = platform_get_resource(dev, IORESOURCE_MEM, 0);
            release_mem_region(res->start, res->end - res->start + 1);

            kfree((struct avmnand__class*)platform_get_drvdata(dev));
        }
        return 1;
    }

    return -1;
#endif
	return 1;

}


/*------------------------------------------------------------------------------------------*\
 * Primitives to implement flash operations (spi functions)
\*------------------------------------------------------------------------------------------*/

static void ar7240_spi_write_enable()  {
    do {
        ath_reg_wr_nf(ATH_SPI_FS, 1);
    } while(ath_reg_rd(ATH_SPI_FS) != 1);
    ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);     
    ar7240_spi_bit_banger(ATH_SPI_CMD_WREN);             
    ar7240_spi_go();
}
static void ar7240_spi_poll(unsigned int locked) {
    int rd;                                                 
    /*--------------------------------------------------------------------------------*\
     * ausgemessen: 5 Durchlaeufe pro 10 usec - es werden mindestens 500 us benoetigt
    \*--------------------------------------------------------------------------------*/
    do {
        ath_reg_wr_nf(ATH_SPI_WRITE, ATH_SPI_CS_DIS);     
        ar7240_spi_bit_banger(ATH_SPI_CMD_RD_STATUS);        
        ar7240_spi_delay_8();
        rd = (ath_reg_rd(ATH_SPI_RD_STATUS) & 1);               
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
        if(locked == 0) {
            unsigned long flags;
            /*--------------------------------------------------------------------------------*\
             * wegen NMI-Workaround
             * wir sind im Erase-Modus: geschuetzt warten, um CPU-Zugriff zu entspannen
             * da kein Preempt kommen aktuell sowieso nur Sw-Irqs/Irqs ran
            \*--------------------------------------------------------------------------------*/
            spin_lock_irqsave(&nmi_lock, flags);
            udelay(100);
            spin_unlock_irqrestore(&nmi_lock, flags);
        }
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
    }while(rd);
}

static void ar7240_spi_write_page(uint32_t addr, uint8_t *data, int len) {
    int i;
    uint8_t ch;
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
    unsigned long flags;
    unsigned long cycles __attribute__((unused));
    spin_lock_irqsave(&nmi_lock, flags);
    cycles = get_cycles();
    ath_workaround_nmi_suspendresume(0x1 | 0x2);
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/

    ar7240_spi_write_enable();
    ar7240_spi_bit_banger(ATH_SPI_CMD_PAGE_PROG);
    ar7240_spi_send_addr(addr);

    for(i = 0; i < len; i++) {
        ch = *(data + i);
        ar7240_spi_bit_banger(ch);
    }

    ar7240_spi_go();
    ar7240_spi_poll(1);

#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
    ar7240_spi_done();

    dma_cache_wback_inv((unsigned int)data, len);
    ath_workaround_nmi_suspendresume(0x0 | 0x2);
#if defined(FLASH_ACCESS_STATISTIC)
    generic_stat(&write_stat, get_cycles() - cycles);
#endif/*--- #if defined(FLASH_ACCESS_STATISTIC) ---*/
    spin_unlock_irqrestore(&nmi_lock, flags);
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
}

/*--------------------------------------------------------------------------------*\
 * flash-erase dauert bis zu 400 ms -> kein irq-save !!!!!
\*--------------------------------------------------------------------------------*/
static void ar7240_spi_sector_erase(uint32_t addr) {
#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
    unsigned long cycles __attribute__((unused));
    cycles = get_cycles();
    ath_workaround_nmi_suspendresume(0x1 | 0x4);
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
	ar7240_spi_write_enable();
	ar7240_spi_bit_banger(ATH_SPI_CMD_SECTOR_ERASE);
	ar7240_spi_send_addr(addr);
	ar7240_spi_go();
#if 0
	/*
	 * Do not touch the GPIO's unnecessarily. Might conflict
	 * with customer's settings.
	 */
	display(0x7d);
#endif
	ar7240_spi_poll(0);

#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
    ar7240_spi_done();
    ath_workaround_nmi_suspendresume(0x0 | 0x4);
#if defined(FLASH_ACCESS_STATISTIC)
    generic_stat(&erase_stat, get_cycles() - cycles);
#endif/*--- #if defined(FLASH_ACCESS_STATISTIC) ---*/
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/

}

/*------------------------------------------------------------------------------------------*\
 * Module Functions
\*------------------------------------------------------------------------------------------*/
static int __init ar7240_flash_init (void) {
    printk(KERN_INFO "Registering AR7240-flash-driver...\n");

#if defined(FLASH_ACCESS_STATISTIC)
    init_generic_stat(&erase_stat);
    init_generic_stat(&write_stat);
    init_generic_stat(&read_stat);
#endif/*--- #if defined(FLASH_ACCESS_STATISTIC) ---*/

    return platform_driver_register(&ar7240_flash_driver);
}

static void __exit ar7240_flash_exit(void) {
    platform_driver_unregister(&ar7240_flash_driver);
}
module_init(ar7240_flash_init);
module_exit(ar7240_flash_exit);
