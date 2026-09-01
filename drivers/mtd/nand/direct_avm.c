/*
 * drivers/mtd/nand/direct_avm.c
 *
 * Updated, and converted to generic GPIO based driver by Russell King.
 * Updated, and converted to generic adresspamming based driver by Martin Pommerenke
 *
 * Written by Martin Pommerenke <m.pommerenke@avm.de>
 *   Based on Ben Dooks <ben@simtec.co.uk>
 *   Based on 2.4 version by Mark Whittaker
 *
 * © 2004 Simtec Electronics
 * © 2009 AVM Berlin
 *
 * Device driver for NAND connected via GPIO and Addressmapping
 *
 * modified vor special AVM NAND interface
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/semaphore.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand-direct-avm.h>
#include <asm/mach_avm.h>
#include <linux/timer.h>
#include <linux/delay.h>

#if defined(CONFIG_MTD_NAND_DIRECT_AVM_GPIO)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
/*--- #define DEBUG_DIRECT_NAND ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifdef DEBUG_DIRECT_NAND
#define DBG_ERR(...)      printk(KERN_ERR __VA_ARGS__)
#define DBG_WARN(...)     printk(KERN_WARNING __VA_ARGS__)
#define DBG_DEBUG(...)    printk(KERN_DEBUG __VA_ARGS__)
/*--- #define DBG_DEBUG(...)    printk(KERN_WARNING __VA_ARGS__) ---*/
#define DBG_TRACE(...)    printk(KERN_DEBUG __VA_ARGS__)
#define DBG_PRINTK(...)   printk(__VA_ARGS__)
#else
#define DBG_ERR(...)      printk(KERN_ERR __VA_ARGS__)
#define DBG_WARN(...)
#define DBG_DEBUG(...)
#define DBG_TRACE(...)
#define DBG_PRINTK(...) 
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define DIRECT_NAND_USE_READY_SEMA

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define DIRECT_NAND_MAX_GPIO_RESOURCES      7
static struct resource direct_nand_gpioressource[DIRECT_NAND_MAX_GPIO_RESOURCES];
#if defined(DIRECT_NAND_USE_READY_SEMA)
static struct semaphore direct_nand_ready_sema;
static struct _hw_gpio_irqhandle *direct_nand_ready_irq_handle[2];
static struct timer_list direct_nand_ready_timer;
#endif /*--- #if defined(DIRECT_NAND_USE_READY_SEMA) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if 0
static struct nand_ecclayout avm_nand_oob_64_hwemulation_8bit = {
	.eccbytes = 12,
	.eccpos = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
	.oobfree = {
		{.offset = 13,
		 .length = 51}}
};

static struct nand_ecclayout avm_nand_oob_128_hwemulation_8bit = {
    .eccbytes = 24,
	.eccpos = {  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 
                65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76 },
    .oobfree = { 
        { .offset = 77, 
          .length = 51}}
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static uint8_t bbt_pattern[] = { 'A', 'V', 'M', ' ' };
static uint8_t mirror_pattern[] = { ' ', 'M', 'V', 'A' };

static struct nand_bbt_descr direct_avm_nand_bbt_main_descr = {
	.options = NAND_BBT_SEARCH | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_8BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP | NAND_BBT_SCANALLPAGES | NAND_BBT_SCANEMPTY,
	.offs = 8,
	.len = sizeof(bbt_pattern),
	.veroffs = 8 + sizeof(mirror_pattern),
	.maxblocks = 4,
	.pattern = bbt_pattern
};

static struct nand_bbt_descr direct_avm_nand_bbt_mirror_descr = {
	.options = NAND_BBT_SEARCH | NAND_BBT_CREATE | NAND_BBT_WRITE
		| NAND_BBT_8BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP | NAND_BBT_SCANALLPAGES | NAND_BBT_SCANEMPTY,
	.offs = 8,
	.len = sizeof(mirror_pattern),
	.veroffs = 8 + sizeof(mirror_pattern),
	.maxblocks = 4,
	.pattern = mirror_pattern
};
#endif /*-- #if 0 --*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct direct_avm_nand {
	void __iomem		*io_ctrl;
	struct mtd_info		mtd_info;
	struct nand_chip	nand_chip;
	struct direct_avm_nand_platdata plat;
    unsigned int ctrl_addr;
};

#define direct_avm_nand_getpriv(x) container_of(x, struct direct_avm_nand, mtd_info)

#define value_is_valid(x)      (((signed int)(x) == -1) ? 0 : 1)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static __inline void direct_avm_nand_write_protect(struct direct_avm_nand *direct_avm_nand, unsigned int yes) {
    DBG_DEBUG("[DIRECT_NAND] %s write protect ", yes ? "set" : "reset");
    if(yes) {
        DBG_PRINTK("NWP == 0 ");
        avm_gpio_out_bit(direct_avm_nand->plat.gpio_nwp, 0);
    } else {
        DBG_PRINTK("NWP == 1 ");
        avm_gpio_out_bit(direct_avm_nand->plat.gpio_nwp, 1);
    }
    DBG_PRINTK("\n");
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static __inline void direct_avm_nand_chip_select(struct direct_avm_nand *direct_avm_nand, unsigned int yes) {
    DBG_DEBUG("[DIRECT_NAND] %s chip select \n", yes ? "set" : "reset");
    if(yes) {
        avm_gpio_out_bit(AVM_NAND_NCE(direct_avm_nand), 0);
    } else {
        avm_gpio_out_bit(AVM_NAND_NCE(direct_avm_nand), 1);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static __inline void direct_avm_nand_command_latch_enable(struct direct_avm_nand *direct_avm_nand, unsigned int yes) {
    DBG_DEBUG("[DIRECT_NAND] %s command latch enable\n", yes ? "set" : "reset");
    if(yes) {
        avm_gpio_out_bit(direct_avm_nand->plat.gpio_cle, 1);
    } else {
        avm_gpio_out_bit(direct_avm_nand->plat.gpio_cle, 0);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static __inline void direct_avm_nand_address_latch_enable(struct direct_avm_nand *direct_avm_nand, unsigned int yes) {
    DBG_DEBUG("[DIRECT_NAND] %s address latch enable\n", yes ? "set" : "reset");
    if(yes) {
        avm_gpio_out_bit(direct_avm_nand->plat.gpio_ale, 1);
    } else {
        avm_gpio_out_bit(direct_avm_nand->plat.gpio_ale, 0);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if 0
void signal_led(int _signal_led, int on) {
    int index;
    static int once = 1;
    if(once) {
        once = 0;
        ikan_gpio_ctrl(26, GPIO_PIN, GPIO_OUTPUT_PIN);
        ikan_gpio_ctrl(28, GPIO_PIN, GPIO_OUTPUT_PIN);
    }
    switch(_signal_led) {
        case 1:
            index = 26;
            break;
        case 0:
            index = 28;
            break;
        default:
            return;
    }
    /*--- printk("[%d] %s\n", index, on ? "high" : "low"); ---*/
    avm_gpio_out_bit(index, on);
}
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int last_cmd = 0xAA;

static void direct_avm_nand_cmd_ctrl(struct mtd_info *mtd, int cmd, unsigned int ctrl) {
	struct direct_avm_nand *direct_avm_nand = direct_avm_nand_getpriv(mtd);
    void dummy_read(void) {
        volatile unsigned char *p_dummy = (volatile unsigned char *)0xBF000000;
        volatile unsigned char dummy;
        dummy = *p_dummy;
    }

	if (ctrl & NAND_CTRL_CHANGE) {
        DBG_TRACE("[DIRECT_NAND] CHANGE: 0x%x ", ctrl);
        dummy_read();
        if(ctrl & NAND_NCE) {
            DBG_TRACE("NCE == 1 ");
            avm_gpio_out_bit(AVM_NAND_NCE(direct_avm_nand), 0);
        } else {
            DBG_TRACE("NCE == 0 ");
            avm_gpio_out_bit(AVM_NAND_NCE(direct_avm_nand), 1);
        }
        if(ctrl & NAND_CLE) {
            DBG_TRACE("CLE == 1 ");
            avm_gpio_out_bit(direct_avm_nand->plat.gpio_cle, 1);
        } else {
            DBG_TRACE("CLE == 0 ");
            avm_gpio_out_bit(direct_avm_nand->plat.gpio_cle, 0);
        }
        if(ctrl & NAND_ALE) {
            DBG_TRACE("ALE == 1 ");
            avm_gpio_out_bit(direct_avm_nand->plat.gpio_ale, 1);
        } else {
            DBG_TRACE("ALE == 0 ");
            avm_gpio_out_bit(direct_avm_nand->plat.gpio_ale, 0);
        }
        dummy_read();
        DBG_PRINTK(" \n");
	}
	if (cmd == NAND_CMD_NONE) {
        DBG_TRACE("[DIRECT_NAND] CMD NONE\n");
		return;
    }

    DBG_TRACE("[DIRECT_NAND] write 1 byte 0x%x to 0x%x\n", cmd & 0xFF, (unsigned int)direct_avm_nand->nand_chip.IO_ADDR_W);
    /*--- printk(KERN_ERR "[DIRECT_NAND] write 1 byte 0x%x to 0x%x\n", cmd & 0xFF, (unsigned int)direct_avm_nand->nand_chip.IO_ADDR_W); ---*/
	/*--- writeb(cmd, direct_avm_nand->nand_chip.IO_ADDR_W); ---*/
	/*--- *((volatile unsigned short * )direct_avm_nand->nand_chip.IO_ADDR_W) = ((cmd & 0xFF) << 8) | (cmd & 0xFF); ---*/
	*((volatile unsigned short * )direct_avm_nand->nand_chip.IO_ADDR_W) = (cmd & 0xFF);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void direct_avm_nand_writebuf(struct mtd_info *mtd, const u_char *buf, int len) {
	struct nand_chip *this = mtd->priv;
    DBG_DEBUG("[DIRECT_NAND] write %d bytes to 0x%x\n", len, (unsigned int)this->IO_ADDR_W);
    /*--- DBG_ERR("[DIRECT_NAND] write %d bytes to 0x%x\n", len, (unsigned int)this->IO_ADDR_W); ---*/
    while(len--) {
	    *((volatile unsigned short * )this->IO_ADDR_W) = (*buf++ & 0xFF);
    }
	/*--- writesb(this->IO_ADDR_W, buf, len); ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned char direct_avm_nand_read(struct mtd_info *mtd) {
	struct nand_chip *this = mtd->priv;
	unsigned short ch = *((volatile unsigned short *)this->IO_ADDR_R);
    /*--- DBG_ERR("[DIRECT_NAND] read 1 byte 0x%x: 0x%x\n", (unsigned int)this->IO_ADDR_R, ch); ---*/
    DBG_DEBUG("[DIRECT_NAND] read 1 byte 0x%x: 0x%x\n", (unsigned int)this->IO_ADDR_R, ch);
    return ch;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void direct_avm_nand_readbuf(struct mtd_info *mtd, u_char *buf, int len) {
	struct nand_chip *this = mtd->priv;
    /*--- int dump = 0, all_ff = 1; ---*/
    DBG_TRACE("[DIRECT_NAND] read %d bytes from 0x%x\n", len, (unsigned int)this->IO_ADDR_W);
    /*--- DBG_ERR("[DIRECT_NAND] read %d bytes from 0x%x\n", len, (unsigned int)this->IO_ADDR_W); ---*/
	/*--- readsb(this->IO_ADDR_R, buf, len); ---*/
    /*--- if(len == 218) ---*/ 
        /*--- dump = len; ---*/
    while(len--) {
	    *buf++ = (unsigned char)*((volatile unsigned short * )this->IO_ADDR_R);
    }
    /*--- if(dump && !all_ff) ---*/
        /*--- printk(KERN_ERR "[NAND] 218 bytes: % *B\n", dump, buf - dump); ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int direct_avm_nand_verifybuf(struct mtd_info *mtd, const u_char *buf, int len) {
	struct nand_chip *this = mtd->priv;
	unsigned char read, *p = (unsigned char *) buf;
	int i, err = 0;

    DBG_DEBUG("[DIRECT_NAND] verify %d bytes\n", len);
    /*--- DBG_ERR("[DIRECT_NAND] verify %d bytes\n", len); ---*/

	for (i = 0; i < len; i++) {
	    read = (unsigned char)*((volatile unsigned short * )this->IO_ADDR_R);
		/*--- read = readb(this->IO_ADDR_R); ---*/
		if (read != p[i]) {
			DBG_WARN("%s: err at %d (read %04x vs %04x)\n", __func__, i, read, p[i]);
			err = -EFAULT;
		}
	}
	return err;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void direct_avm_nand_devready_timer(unsigned long date) {
    const char *text;
    if(date)
        text = (const char *)date;
    else
        text = "unbekannt";

    /*--- printk(KERN_ERR "[%s] up (%s) cmd=0x%x\n", __FUNCTION__, text, last_cmd); ---*/
    del_timer(&direct_nand_ready_timer);
/*--- signal_led(0,0); ---*/
    up(&direct_nand_ready_sema);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _value_queue {
    int values[0x100];
    int read, write;
} value_queue[2];
unsigned int ready_gpios[2];


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int direct_avm_nand_devready_interrupt(unsigned int mask, unsigned int index) {
    int ret;
    del_timer(&direct_nand_ready_timer);
	ret = avm_gpio_in_bit(ready_gpios[index]);
    /*--- printk(KERN_ERR "[%s/%d] up (bit %d=%x)\n", __FUNCTION__, index, ready_gpios[index], ret); ---*/
    value_queue[index].values[value_queue[index].write++] = ret;
    value_queue[index].write &= 0xFF;
    if(value_queue[index].write == value_queue[index].read) {
        printk(KERN_ERR "[%s/%d] queue overflow\n", __FUNCTION__, index);
    }
/*--- signal_led(1,0); ---*/
    up(&direct_nand_ready_sema);
    return mask;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int direct_avm_nand_devready_interrupt_0(unsigned int mask) {
    return direct_avm_nand_devready_interrupt(mask, 0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int direct_avm_nand_devready_interrupt_1(unsigned int mask) {
    return direct_avm_nand_devready_interrupt(mask, 1);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int direct_avm_nand_devready(struct mtd_info *mtd) {
	struct direct_avm_nand *direct_avm_nand = direct_avm_nand_getpriv(mtd);
    int ret;
#if defined(DIRECT_NAND_USE_READY_SEMA)
    unsigned int index = direct_avm_nand->plat.current_chip;

    //==============================
    // [hbl]
    // Timer starten, der nach 2 jiffies ablaeuft und die Semaphore freigibt.
    // Nortig ist dies z.B. bei der 7340, die keinen NAND hat und somit auch 
    // keiner auf Anfragen reagiert und den Ready-Interrupt ausloest.
    direct_nand_ready_timer.expires = jiffies + 2;
    add_timer(&direct_nand_ready_timer);
    //==============================

    //==============================
    // [hbl]
    // Nie wieder ein 'down_interruptible' mehr, weil:
    // Wenn der Ablauf abgebrochen wird, kann so sichergestellt werden, dass die Operation konsistent zu Ende gebracht wird.
    //  -Altes Verhalten:
    //    .das Warten auf die Semaphore wurde unterbrochen, ein Fehlerwert wird zurueckgeliefert
    //     (und evtl. schlimmer sogar z.B. eine noch nicht bereitgestellte Page gelesen)
    //    .das Yaffs reagierte sehr empfindlich darauf und warf massenhaft Fehler
    //    -> Dateisystem war nicht mehr konsistent
    //  -Neues Verhalten
    //    .Die anliegende Op wird zu Ende gebracht und das Yaffs kann richtig auf das sig abort reagieren
    down(&direct_nand_ready_sema);
    //==============================

    //==============================
    // [hbl]
    // Timer wieder stoppen, sonst laeuft der irgendwann ab nud loest ungewollt aus.
    del_timer(&direct_nand_ready_timer);
    //==============================

    if(value_queue[index].write != value_queue[index].read) {
        ret = value_queue[index].values[value_queue[index].read++];
        value_queue[index].read &= 0xFF;
    } else {
        ret = 1;
    }

    if(ret == 0) {
        printk(KERN_ERR "[%s] internal communication error, ready still %d (retry)\n", __FUNCTION__, ret);
        del_timer(&direct_nand_ready_timer);
        direct_nand_ready_timer.expires = jiffies + 2;
        add_timer(&direct_nand_ready_timer);
    /*--- } else { ---*/
        /*--- ikan_gpio_disable_irq(direct_nand_ready_irq_handle[index]); ---*/
        /*--- printk(KERN_ERR "[%s] rdy=%d\n", __FUNCTION__, ret); ---*/
    }
    return ret;
#else /*--- #if defined(DIRECT_NAND_USE_READY_SEMA) ---*/
    ret = avm_gpio_in_bit(AVM_NAND_RDY(direct_avm_nand));
    DBG_DEBUG("[DIRECT_NAND] device %sready\n", ret ? "" : "not ");
#endif /*--- #else ---*/ /*--- #if defined(DIRECT_NAND_USE_READY_SEMA) ---*/

    return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __exit direct_avm_nand_remove(struct platform_device *dev) {
	struct direct_avm_nand *direct_avm_nand = platform_get_drvdata(dev);
	struct resource *res;

	nand_release(&direct_avm_nand->mtd_info);
    direct_avm_nand_chip_select(direct_avm_nand, 0);
    direct_avm_nand_write_protect(direct_avm_nand, 1);

	res = platform_get_resource(dev, IORESOURCE_MEM, 1);
	/*--- iounmap(direct_avm_nand->io_ctrl); ---*/
	if (res)
		release_mem_region(res->start, res->end - res->start + 1);

	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, res->end - res->start + 1);
    
	release_resource(&direct_nand_gpioressource[0]);

	release_resource(&direct_nand_gpioressource[1]);
	release_resource(&direct_nand_gpioressource[2]);
	release_resource(&direct_nand_gpioressource[3]);
	release_resource(&direct_nand_gpioressource[4]);
	release_resource(&direct_nand_gpioressource[5]);
	release_resource(&direct_nand_gpioressource[6]);

	kfree(direct_avm_nand);

	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void __iomem *request_and_remap(struct resource *res, size_t size __attribute__((unused)), const char *name, int *err) {
	void __iomem *ptr;

	if (!request_mem_region(res->start, res->end - res->start + 1, name)) {
		DBG_ERR("[direct_avm_nand_probe] unable to request Memory within region 0x%x - 0x%x\n", 
                res->start, res->end);
		*err = -EBUSY;
		return NULL;
	}

    ptr = (void __iomem *)res->start;

#if 0
	ptr = ioremap(res->start, size);
	if (!ptr) {
		release_mem_region(res->start, res->end - res->start + 1);
		*err = -ENOMEM;
	}
#endif
	return ptr;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void direct_avm_nand_select_chip(struct mtd_info *mtd, int chip_nr) {
	struct direct_avm_nand *direct_avm_nand = direct_avm_nand_getpriv(mtd);
	struct nand_chip *chip = mtd->priv;

    switch (chip_nr) {
        case -1:
            chip->cmd_ctrl(mtd, NAND_CMD_NONE, 0 | NAND_CTRL_CHANGE);
            direct_avm_nand->plat.current_chip = -1;
            break;
        case 0:
            direct_avm_nand->plat.current_chip = 0;
            break;
        case 1:
            direct_avm_nand->plat.current_chip = 1;
            break;
        default:
            BUG();
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __devinit direct_avm_nand_probe(struct platform_device *dev)
{
	struct direct_avm_nand *direct_avm_nand;
	struct nand_chip *this;
	int ret = 0;

    DBG_ERR("[direct_avm_nand_probe] error output\n");
    DBG_WARN("[direct_avm_nand_probe] warning output\n");
    DBG_DEBUG("[direct_avm_nand_probe] debug output\n");

	if (!dev->dev.platform_data)
		return -EINVAL;

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    DBG_DEBUG("[direct_avm_nand_probe] got resource values 0 (data_addr %x)\n", dev->resource[0].start);

	direct_avm_nand = kmalloc(sizeof(struct direct_avm_nand), GFP_KERNEL);
	if (direct_avm_nand == NULL) {
		DBG_ERR("[direct_avm_nand_probe] failed to create NAND MTD\n");
		return -ENOMEM;
	}
    memset(direct_avm_nand, 0, sizeof(struct direct_avm_nand));

	this = &direct_avm_nand->nand_chip;
	this->IO_ADDR_R = (void __iomem*)KSEG1ADDR(request_and_remap(&dev->resource[0], 2, "NAND Data", &ret));
	if (!this->IO_ADDR_R) {
		DBG_ERR("[direct_avm_nand_probe] unable to map NAND Data\n");
		goto err_map;
	}

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
	memcpy(&direct_avm_nand->plat, dev->dev.platform_data, sizeof(direct_avm_nand->plat));
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    direct_nand_gpioressource[0].start = direct_avm_nand->plat.gpio_rdy0;
    direct_nand_gpioressource[0].end   = direct_avm_nand->plat.gpio_rdy0;
    direct_nand_gpioressource[0].name  = "NAND Ready";
    direct_nand_gpioressource[0].flags = IORESOURCE_IO;
	if(request_resource(&gpio_resource, &direct_nand_gpioressource[0])) {
        DBG_ERR("[direct_avm_nand_probe] unable to get GPIO resource\n");
        goto err_gpio;
    }
    ikan_gpio_ctrl(direct_avm_nand->plat.gpio_rdy0, GPIO_PIN, GPIO_INPUT_PIN);

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    direct_nand_gpioressource[1].start = direct_avm_nand->plat.gpio_rdy1;
    direct_nand_gpioressource[1].end   = direct_avm_nand->plat.gpio_rdy1;
    direct_nand_gpioressource[1].name  = "NAND Ready";
    direct_nand_gpioressource[1].flags = IORESOURCE_IO;
	if(request_resource(&gpio_resource, &direct_nand_gpioressource[1])) {
        DBG_ERR("[direct_avm_nand_probe] unable to get GPIO resource\n");
        goto err_gpio2;
    }
    ikan_gpio_ctrl(direct_avm_nand->plat.gpio_rdy1, GPIO_PIN, GPIO_INPUT_PIN);

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    direct_nand_gpioressource[2].start = direct_avm_nand->plat.gpio_nce0;
    direct_nand_gpioressource[2].end   = direct_avm_nand->plat.gpio_nce0;
    direct_nand_gpioressource[2].name  = "NAND /CE0";
    direct_nand_gpioressource[2].flags = IORESOURCE_IO;
	if(request_resource(&gpio_resource, &direct_nand_gpioressource[2])) {
        DBG_ERR("[direct_avm_nand_probe] unable to get GPIO resource\n");
        goto err_gpio3;
    }
    ikan_gpio_ctrl(direct_avm_nand->plat.gpio_nce0, GPIO_PIN, GPIO_OUTPUT_PIN);

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    direct_nand_gpioressource[3].start = direct_avm_nand->plat.gpio_nce1;
    direct_nand_gpioressource[3].end   = direct_avm_nand->plat.gpio_nce1;
    direct_nand_gpioressource[3].name  = "NAND /CE1";
    direct_nand_gpioressource[3].flags = IORESOURCE_IO;
	if(request_resource(&gpio_resource, &direct_nand_gpioressource[3])) {
        DBG_ERR("[direct_avm_nand_probe] unable to get GPIO resource\n");
        goto err_gpio4;
    }
    ikan_gpio_ctrl(direct_avm_nand->plat.gpio_nce1, GPIO_PIN, GPIO_OUTPUT_PIN);

    direct_nand_gpioressource[4].start = direct_avm_nand->plat.gpio_nwp;
    direct_nand_gpioressource[4].end   = direct_avm_nand->plat.gpio_nwp;
    direct_nand_gpioressource[4].name  = "NAND /WP";
    direct_nand_gpioressource[4].flags = IORESOURCE_IO;
	if(request_resource(&gpio_resource, &direct_nand_gpioressource[4])) {
        DBG_ERR("[direct_avm_nand_probe] unable to get GPIO resource\n");
        goto err_gpio5;
    }
    ikan_gpio_ctrl(direct_avm_nand->plat.gpio_nwp, GPIO_PIN, GPIO_OUTPUT_PIN);

    direct_nand_gpioressource[5].start = direct_avm_nand->plat.gpio_ale;
    direct_nand_gpioressource[5].end   = direct_avm_nand->plat.gpio_ale;
    direct_nand_gpioressource[5].name  = "NAND ALE";
    direct_nand_gpioressource[5].flags = IORESOURCE_IO;
	if(request_resource(&gpio_resource, &direct_nand_gpioressource[5])) {
        DBG_ERR("[direct_avm_nand_probe] unable to get GPIO resource\n");
        goto err_gpio6;
    }
    ikan_gpio_ctrl(direct_avm_nand->plat.gpio_ale, GPIO_PIN, GPIO_OUTPUT_PIN);

    direct_nand_gpioressource[6].start = direct_avm_nand->plat.gpio_cle;
    direct_nand_gpioressource[6].end   = direct_avm_nand->plat.gpio_cle;
    direct_nand_gpioressource[6].name  = "NAND CLE";
    direct_nand_gpioressource[6].flags = IORESOURCE_IO;
	if(request_resource(&gpio_resource, &direct_nand_gpioressource[6])) {
        DBG_ERR("[direct_avm_nand_probe] unable to get GPIO resource\n");
        goto err_gpio7;
    }
    ikan_gpio_ctrl(direct_avm_nand->plat.gpio_cle, GPIO_PIN, GPIO_OUTPUT_PIN);

    DBG_ERR("[NAND] direct NAND: GPIO usage: %d: /CE0 %d: /CE1: %d /WP %d: CLE %d: ALE (0x%x)\n", 
        direct_avm_nand->plat.gpio_nce0,
        direct_avm_nand->plat.gpio_nce1,
        direct_avm_nand->plat.gpio_nwp,
        direct_avm_nand->plat.gpio_cle,
        direct_avm_nand->plat.gpio_ale,
        *((volatile unsigned int *)0xB90000D4));

	this->IO_ADDR_W   = this->IO_ADDR_R;
	this->ecc.mode    = NAND_ECC_SOFT;

	this->options     = direct_avm_nand->plat.options;
	this->chip_delay  = direct_avm_nand->plat.chip_delay;
	this->select_chip = direct_avm_nand_select_chip;

	/*--- this->bbt_td = &direct_avm_nand_bbt_main_descr; ---*/
	/*--- this->bbt_md = &direct_avm_nand_bbt_mirror_descr; ---*/

	/* install our routines */
	this->cmd_ctrl   = direct_avm_nand_cmd_ctrl;
	this->dev_ready  = direct_avm_nand_devready;

	if (this->options & NAND_BUSWIDTH_16) {
        printk("[direct_avm_nand_probe] NAND_BUSWIDTH_16\n");
#if 0
		this->read_buf   = direct_avm_nand_readbuf16;
		this->write_buf  = direct_avm_nand_writebuf16;
		this->verify_buf = direct_avm_nand_verifybuf16;
#endif
	} else {
		this->read_buf   = direct_avm_nand_readbuf;
		this->write_buf  = direct_avm_nand_writebuf;
		this->verify_buf = direct_avm_nand_verifybuf;
	}

    this->read_byte      = direct_avm_nand_read;

	/* set the mtd private data for the nand driver */
	direct_avm_nand->mtd_info.priv  = this;
	direct_avm_nand->mtd_info.owner = THIS_MODULE;

#if defined(DIRECT_NAND_USE_READY_SEMA)
    ready_gpios[0] = direct_avm_nand->plat.gpio_rdy0;
    ready_gpios[1] = direct_avm_nand->plat.gpio_rdy1;
    setup_timer(&direct_nand_ready_timer, direct_avm_nand_devready_timer, 0UL);
    printk(KERN_ERR "[%s] irq init\n", __FUNCTION__);
    direct_nand_ready_irq_handle[0] = ikan_gpio_request_irq(1 << direct_avm_nand->plat.gpio_rdy0, 0, GPIO_EDGE_SENSITIVE, direct_avm_nand_devready_interrupt_0);
    direct_nand_ready_irq_handle[1] = ikan_gpio_request_irq(1 << direct_avm_nand->plat.gpio_rdy1, 0, GPIO_EDGE_SENSITIVE, direct_avm_nand_devready_interrupt_1);
    ikan_gpio_enable_irq(direct_nand_ready_irq_handle[0]);
    ikan_gpio_enable_irq(direct_nand_ready_irq_handle[1]);

#endif /*--- #if defined(DIRECT_NAND_USE_READY_SEMA) ---*/

    DBG_DEBUG("[direct_avm_nand_probe] start nand_scan ...\n");
    direct_avm_nand_write_protect(direct_avm_nand, 0);
	if (nand_scan(&direct_avm_nand->mtd_info, 1)) {
		DBG_ERR("[direct_avm_nand_probe] no nand chips found?\n");
		ret = -ENXIO;
		goto err_wp;
	}
    DBG_DEBUG("[direct_avm_nand_probe] adjust_parts ...\n");

#if 0
    if (direct_avm_nand->mtd_info.oobsize == 128)
        this->ecc.layout = &avm_nand_oob_128_hwemulation_8bit;
    else
        this->ecc.layout = &avm_nand_oob_64_hwemulation_8bit;
#endif

	if (direct_avm_nand->plat.adjust_parts)
		direct_avm_nand->plat.adjust_parts(&direct_avm_nand->plat, direct_avm_nand->mtd_info.size);

    DBG_DEBUG("[direct_avm_nand_probe] add_mtd_partitions %d partitions ...\n",  direct_avm_nand->plat.num_parts);

	add_mtd_partitions(&direct_avm_nand->mtd_info, direct_avm_nand->plat.parts, direct_avm_nand->plat.num_parts);

    DBG_DEBUG("[direct_avm_nand_probe] platform_set_drvdata ...\n");
	platform_set_drvdata(dev, direct_avm_nand);
    DBG_DEBUG("[direct_avm_nand_probe] success !\n");
	return 0;

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
err_wp:
    direct_avm_nand_write_protect(direct_avm_nand, 1);

err_gpio7: release_resource(&direct_nand_gpioressource[5]);
err_gpio6: release_resource(&direct_nand_gpioressource[4]);
err_gpio5: release_resource(&direct_nand_gpioressource[3]);
err_gpio4: release_resource(&direct_nand_gpioressource[2]);
err_gpio3: release_resource(&direct_nand_gpioressource[1]);
err_gpio2: release_resource(&direct_nand_gpioressource[0]);
err_gpio:
	/*--- iounmap(direct_avm_nand->io_ctrl); ---*/

/*--- err_ctrl: ---*/
	/*--- iounmap(direct_avm_nand->nand_chip.IO_ADDR_R); ---*/
	release_mem_region(dev->resource[0].start, dev->resource[0].end - dev->resource[0].start + 1);
err_map:
	kfree(direct_avm_nand);
	return ret;
}

static struct platform_driver direct_avm_nand_driver = {
	.probe		= direct_avm_nand_probe,
	.remove		= direct_avm_nand_remove,
	.driver		= {
		.name	= "direct-avm-nand",
	},
};

static int __init direct_avm_nand_init(void) {
	printk(KERN_INFO "AVM Direct NAND driver, © 2008 AVM Berlin\n");
#if defined(DIRECT_NAND_USE_READY_SEMA)
    sema_init(&direct_nand_ready_sema, 0);
#endif /*--- #if defined(DIRECT_NAND_USE_READY_SEMA) ---*/
	return platform_driver_register(&direct_avm_nand_driver);
}

static void __exit direct_avm_nand_exit(void) {
	platform_driver_unregister(&direct_avm_nand_driver);
}

module_init(direct_avm_nand_init);
module_exit(direct_avm_nand_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AVM Direct NAND Driver");
#endif /*--- #if defined(CONFIG_MTD_NAND_DIRECT_AVM_GPIO) ---*/
