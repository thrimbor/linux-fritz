/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/plat-nor.h>
#include <asm/io.h>
#include <asm/byteorder.h>

#include <ifx_types.h>
#include <ifx_regs.h>
#include <common_routines.h>

#include "ifxmips_mtd_nor.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define IFX_MTD_NOR_DATAWIDTH      2  /* 16 bit */

#define IFX_MTD_NOR_VER_MAJOR      1
#define IFX_MTD_NOR_VER_MID        0
#define IFX_MTD_NOR_VER_MINOR      4

#define IFX_MTD_NOR_BANK_NAME      "mtd-nor" /* cmd line bank name should be the same */
#define IFX_MTD_NOR_NAME_LEN       16

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define  ifx_enable_ebu()
#define  ifx_disable_ebu()
#define  MTD_NOR_PRINT(fmt, args...)     
/*--- #define  MTD_NOR_PRINT(fmt, args...)     printk("[%s]: " fmt, __func__, ##args) ---*/

/*--- #define  MTD_NOR_REGISTER_DUMP ---*/

/*------------------------------------------------------------------------------------------*\
 * private structure for each mtd platform ram device created
\*------------------------------------------------------------------------------------------*/
struct plat_nor_info {
	struct device		*dev;
	struct mtd_info		*mtd;
	struct map_info		 map;
	struct mtd_partition	*partitions;
	bool			free_partitions;
	struct resource		*area;
	struct physmap_flash_data *pdata;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static map_word plat_nor_mtd_map_read(struct map_info * map, unsigned long ofs) {
	map_word temp;

	ifx_enable_ebu();
	temp.x[0] = *(u16 *)(map->virt + ofs);
	ifx_disable_ebu();
	/*--- printk("[0x%08x + %ld] ==> %x\n", (unsigned int)map->virt, ofs, (u16)temp.x[0]); ---*/
	return temp;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void plat_nor_mtd_map_write(struct map_info *map, const map_word datum, unsigned long ofs) {
	/* WAR, kernel 2.4 and kernel 2.6 uses the hardcode address in probing */
	ifx_enable_ebu();
	*(u16 *)(map->virt + ofs) = (u16)datum.x[0];
	wmb();
	ifx_disable_ebu();
	/*--- printk("map_bankwidth_is_2 w16: [0x%08x + 0x%lx] <== %x\n", (unsigned int)map->virt, ofs, (u16)datum.x[0]); ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void plat_nor_mtd_map_cmd_write(struct map_info *map, const map_word datum, unsigned long ofs) {
	plat_nor_mtd_map_write(map, datum, ofs ^ 0x02);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static map_word plat_nor_mtd_map_cmd_read(struct map_info * map, unsigned long ofs) {
	return plat_nor_mtd_map_read(map, ofs ^0x02);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void plat_nor_mtd_map_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len) {

    unsigned char *src, *dest;
    
    src  = (unsigned char *)(from + map->virt);
    dest = (unsigned char *)to;

    ifx_enable_ebu();
    
    while (len--)
        *dest++ = *src++

    ifx_disable_ebu();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void plat_nor_mtd_map_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len) {
	unsigned short *dest;
	unsigned short *source;

	if((unsigned long)to & 0x01) {
		printk(KERN_ERR "[%s] unaligned write to 16 Bit device\n", __FUNCTION__);
		return;
	}

	to = (unsigned long) (to + map->virt);
    ifx_enable_ebu();

	if((unsigned long)from & 0x01) {
		unsigned char *p = (unsigned char *)from;
		unsigned short s;
		dest = (unsigned short *)to;
		while(len) {
			s  = 0;
#ifdef __BIG_ENDIAN
			s |= *p++ << 8;
			s |= *p++;
#endif 
#ifdef __LITTLE_ENDIAN
			s |= *p++;
			s |= *p++ << 8;
#endif
			if(len == 1) {
				*dest = s | 0xff00;
				break;
			}
			*dest++ = s;
			len -= 2;
		}
		ifx_disable_ebu();
		return;
	}
	dest = (unsigned short *)to;
	source = (unsigned short *)from;

	while(len) {
		if(len == 1) {
			*dest = *source | 0xFF00;
			break;
		}
		*dest++ = *source++;
		len -= 2;
	}
    ifx_disable_ebu();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void plat_nor_mtd_show_version(void) {
    char ver_buf[128] = {0};
    ifx_drv_ver(ver_buf, "MTD NOR", IFX_MTD_NOR_VER_MAJOR, IFX_MTD_NOR_VER_MID, IFX_MTD_NOR_VER_MINOR);
    printk(KERN_INFO "%s", ver_buf);
}

/*------------------------------------------------------------------------------------------*\
 * to_plat_nor_info()
 *
 * device private data to struct plat_nor_info conversion
\*------------------------------------------------------------------------------------------*/
static inline struct plat_nor_info *to_plat_nor_info(struct platform_device *dev) {
	return (struct plat_nor_info *)platform_get_drvdata(dev);
}

/*------------------------------------------------------------------------------------------*\
 * plat_nor_remove
 *
 * called to remove the device from the driver's control
\*------------------------------------------------------------------------------------------*/
static int plat_nor_remove(struct platform_device *pdev) {
	struct plat_nor_info *info = to_plat_nor_info(pdev);

	platform_set_drvdata(pdev, NULL);

	dev_dbg(&pdev->dev, "removing device\n");

	if (info == NULL)
		return 0;

	if (info->mtd) {
#ifdef CONFIG_MTD_PARTITIONS
		if (info->partitions) {
			del_mtd_partitions(info->mtd);
			if (info->free_partitions)
				kfree(info->partitions);
		}
#endif
		del_mtd_device(info->mtd);
		map_destroy(info->mtd);
	}

	/* release resources */

	if (info->area) {
		release_resource(info->area);
		kfree(info->area);
	}

#if defined(CONFIG_ARM)
	if (info->map.virt != NULL)
		iounmap(info->map.virt);
#endif /*--- #if defined(CONFIG_ARM) ---*/

	kfree(info);

	return 0;
}

/*------------------------------------------------------------------------------------------*\
 * plat_nor_probe
 *
 * called from device drive system when a device matching our
 * driver is found.
\*------------------------------------------------------------------------------------------*/
static int plat_nor_probe(struct platform_device *pdev) {
	struct physmap_flash_data *pdata;
	struct plat_nor_info *info;
	struct resource *res;
	int err = 0;

    printk(KERN_ERR "[%s]start probe\n", __FUNCTION__);
	/*--- dev_dbg(&pdev->dev, "probe entered\n"); ---*/

	if (pdev->dev.platform_data == NULL) {
		dev_err(&pdev->dev, "no platform data supplied\n");
        printk(KERN_ERR "[%s/%s] no platform data\n", __FUNCTION__, pdev->name);
		err = -ENOENT;
		goto exit_error;
	}

	pdata = pdev->dev.platform_data;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&pdev->dev, "no memory for flash info\n");
        printk(KERN_ERR "[%s/%s] no memory \n", __FUNCTION__, pdev->name);
		err = -ENOMEM;
		goto exit_error;
	}

	platform_set_drvdata(pdev, info);

	info->dev = &pdev->dev;
	info->pdata = pdata;

	/* get the resource for the memory mapping */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    info->area = res;

	if (res == NULL) {
		dev_err(&pdev->dev, "no memory resource specified\n");
		err = -ENOENT;
        printk(KERN_ERR "[%s/%s] no memory resource specified\n", __FUNCTION__, pdev->name);
		goto exit_free;
	}

	dev_dbg(&pdev->dev, "got platform resource %p (0x%llx)\n", res, (unsigned long long)res->start);

	/* setup map parameters */
    /*--- printk(KERN_ERR "[%s/%s] setup map-functions\n", __FUNCTION__, pdev->name); ---*/

	info->map.phys      = res->start;
	info->map.size      = (res->end - res->start);
    /*--- printk(KERN_ERR "[%s/%s] size %d\n", __FUNCTION__, pdev->name, info->map.size); ---*/
    /*--- printk(KERN_ERR "[%s/%s] bankwidth %d\n", __FUNCTION__, pdev->name, pdata->width); ---*/
	info->map.bankwidth = (int)(pdata->width);

	/* register our usage of the memory area */
    /*--- printk(KERN_ERR "[%s/%s] request memory region start=0x%x size=0x%lx\n", __FUNCTION__, pdev->name, res->start, info->map.size); ---*/

	info->area = request_mem_region(res->start, info->map.size, pdev->name);
	if (info->area == NULL) {
		dev_err(&pdev->dev, "failed to request memory region\n");
        printk(KERN_ERR "[%s/%s] failed to request memory region\n", __FUNCTION__, pdev->name);
		err = -EIO;
		goto exit_free;
	}

	/* remap the memory area */
    /*--- printk(KERN_ERR "[%s/%s] ioremap_nocache(...)\n", __FUNCTION__, pdev->name); ---*/

#if defined(CONFIG_ARM)
	info->map.virt = ioremap_nocache(res->start, info->map.size);
	dev_dbg(&pdev->dev, "virt %p, %lu bytes\n", info->map.virt, info->map.size);


	if (info->map.virt == NULL) {
		dev_err(&pdev->dev, "failed to ioremap() region\n");
		err = -EIO;
        printk(KERN_ERR "[%s/%s] failed to ioremap() region\n", __FUNCTION__, pdev->name);
		goto exit_free;
	}
#else /*--- #if defined(CONFIG_ARM) ---*/
    info->map.virt = (resource_size_t *)KSEG1ADDR(res->start);
    /*--- Adressbereich freischalten ---*/
    {
        unsigned int mask =  fls(info->map.size);
        unsigned int base = CPHYSADDR(res->start);
        unsigned int reg;
        mask -= 13;
        mask  = ~mask & 0xF;
        reg   = 
            (base & (((1 << 20) - 1) << 12)) |
            ((mask & ((1 <<  4) - 1)) <<  4) |
            1;  /*--- enable ---*/

        *IFX_EBU_ADDSEL0 = reg;
        /*--- printk(KERN_ERR "[%s/%s] EBU_ADDR_SEL_0: 0x%08x <= 0x%08x\n", __FUNCTION__, pdev->name, *IFX_EBU_ADDSEL0, reg); ---*/
    }
#endif /*--- #else ---*/ /*--- #if defined(CONFIG_ARM) ---*/
    /*--- printk(KERN_ERR "[%s/%s] virtual start=0x%x\n", __FUNCTION__, pdev->name, info->map.virt); ---*/

	/*--- simple_map_init(&info->map); ---*/
    info->map.name = IFX_MTD_NOR_BANK_NAME;
    info->map.bankwidth  = IFX_MTD_NOR_DATAWIDTH;
#if defined(CONFIG_MTD_COMPLEX_MAPPINGS)
    info->map.read       = plat_nor_mtd_map_read;
    info->map.cmd_read   = plat_nor_mtd_map_cmd_read;
    info->map.copy_from  = plat_nor_mtd_map_copy_from;
    info->map.write      = plat_nor_mtd_map_write;
    info->map.cmd_write  = plat_nor_mtd_map_cmd_write;
    info->map.copy_to    = plat_nor_mtd_map_copy_to;
#endif /*--- #if defined(CONFIG_MTD_COMPLEX_MAPPINGS) ---*/
    /*--- info->map.map_priv_1 = (unsigned long)info->map.virt; ---*/

    /* Make sure probing works */
	dev_dbg(&pdev->dev, "initialised map, probing for mtd\n");

	/* probe for the right mtd map driver supplied by the platform_data struct */
    /*--- printk(KERN_ERR "[%s/%s] start probing\n", __FUNCTION__, pdev->name); ---*/
    info->mtd = do_map_probe("cfi_probe", &info->map);

	if (info->mtd == NULL) {
		dev_err(&pdev->dev, "failed to probe for map_ram\n");
		err = -ENOMEM;
		goto exit_free;
	}

	info->mtd->owner = THIS_MODULE;

	/* check to see if there are any available partitions, or wether
	 * to add this device whole */

#ifdef CONFIG_MTD_PARTITIONS
	if (!pdata->nr_parts || pdata->probes) {
		/* try to probe using the supplied probe type */
    /*--- printk(KERN_ERR "[%s/%s] creating partitions\n", __FUNCTION__, pdev->name); ---*/
		if (pdata->probes) {
    /*--- printk(KERN_ERR "[%s/%s] parsing partitions\n", __FUNCTION__, pdev->name); ---*/
			err = parse_mtd_partitions(info->mtd, pdata->probes, &info->partitions, 0);
			info->free_partitions = 1;
			if (err > 0) {
    /*--- printk(KERN_ERR "[%s/%s] %d partitions found\n", __FUNCTION__, pdev->name, err); ---*/
				err = add_mtd_partitions(info->mtd, info->partitions, err);
            }
		}
	} else {
	    /* use the static mapping */
		err = add_mtd_partitions(info->mtd, pdata->parts, pdata->nr_parts);
    }
#endif /* CONFIG_MTD_PARTITIONS */

	if (add_mtd_device(info->mtd)) {
		dev_err(&pdev->dev, "add_mtd_device() failed\n");
		err = -ENOMEM;
	}

	if (!err) {
		dev_info(&pdev->dev, "registered mtd device\n");
    }

	return err;

exit_free:
	plat_nor_remove(pdev);
exit_error:
	return err;
}

/*------------------------------------------------------------------------------------------*\
 * device driver info
 * work with hotplug and coldplug
\*------------------------------------------------------------------------------------------*/
MODULE_ALIAS("platform:mtd-nor");

static struct platform_driver plat_nor_driver = {
	.probe		= plat_nor_probe,
	.remove		= plat_nor_remove,
	.driver		= {
		.name	= "mtd-nor",
		.owner	= THIS_MODULE,
	},
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(MTD_NOR_REGISTER_DUMP)
static void local_register_dump(void) {
    volatile unsigned char *ebu = (volatile unsigned char *)0xbe105300;
    printk("EBU: Register dump: ");
    for(i = 0 ; i < 0xC8 ; i += 4) {
        if((i % 16) == 0)
            printk("\n0x%08x: ", (unsigned int)ebu + i);
        printk("0x%08x ",  *((volatile unsigned int *)(ebu + i)));
    }
    printk("\n");
}
#endif

/*------------------------------------------------------------------------------------------*\
 * module init/exit
\*------------------------------------------------------------------------------------------*/
static int __init plat_nor_init(void) {
    int reg;

    printk(KERN_INFO "Platform NOR MTD, (c) AVM 2010\n");

#if defined(MTD_NOR_REGISTER_DUMP)
    local_register_dump();
#endif

    /* Configure EBU */
    reg = SM(IFX_EBU_BUSCON0_XDM16, IFX_EBU_BUSCON0_XDM) |
#if 0
        IFX_EBU_BUSCON0_ADSWP |  /*--- Hardware-Swapping muss in diesem Treiber ausgeschaltet sein, da die Kommando-Adressen explitzit im 2.Bit verXORt werden ---*/
#endif
        SM(IFX_EBU_BUSCON0_ALEC3, IFX_EBU_BUSCON0_ALEC) |
        SM(IFX_EBU_BUSCON0_BCGEN_INTEL,IFX_EBU_BUSCON0_BCGEN) |
        SM(IFX_EBU_BUSCON0_WAITWRC7, IFX_EBU_BUSCON0_WAITWRC) |
        SM(IFX_EBU_BUSCON0_WAITRDC3, IFX_EBU_BUSCON0_WAITRDC) |
        SM(IFX_EBU_BUSCON0_HOLDC3, IFX_EBU_BUSCON0_HOLDC) |
        SM(IFX_EBU_BUSCON0_RECOVC3, IFX_EBU_BUSCON0_RECOVC);

    /* XXX, VR9 support */     
#if defined(CONFIG_MTD_IFX_LESS_WAIT_CYCLE) && 0
#if defined(CONFIG_AR9)
    /* 393/196MHz */    
    if ((MS(IFX_REG_R32(IFX_CGU_SYS), IFX_CGU_SYS_SEL) == IFX_CGU_SYS_SEL_393)) {
        reg = SM(IFX_EBU_BUSCON0_XDM16, IFX_EBU_BUSCON0_XDM) |
#if 0
            IFX_EBU_BUSCON0_ADSWP | /*--- Hardware-Swapping muss in diesem Treiber ausgeschaltet sein, da die Kommando-Adressen explitzit im 2.Bit verXORt werden ---*/
#endif
            SM(IFX_EBU_BUSCON0_ALEC3, IFX_EBU_BUSCON0_ALEC) |
            SM(IFX_EBU_BUSCON0_BCGEN_INTEL,IFX_EBU_BUSCON0_BCGEN) |
            SM(IFX_EBU_BUSCON0_WAITWRC5, IFX_EBU_BUSCON0_WAITWRC) |
            SM(IFX_EBU_BUSCON0_WAITRDC2, IFX_EBU_BUSCON0_WAITRDC) |
            SM(IFX_EBU_BUSCON0_HOLDC2, IFX_EBU_BUSCON0_HOLDC) |
            SM(IFX_EBU_BUSCON0_RECOVC2, IFX_EBU_BUSCON0_RECOVC) |
            SM(IFX_EBU_BUSCON0_CMULT8, IFX_EBU_BUSCON0_CMULT);
    }
    /* 333/196MHz */
    else {
        reg |= SM(IFX_EBU_BUSCON0_CMULT4, IFX_EBU_BUSCON0_CMULT);
    }
#elif defined (CONFIG_VR9)
    reg |= SM(IFX_EBU_BUSCON0_CMULT4, IFX_EBU_BUSCON0_CMULT);
#endif /* CONFIG_AR9 */        
#else
    reg |= SM(IFX_EBU_BUSCON0_CMULT16, IFX_EBU_BUSCON0_CMULT);  
#endif /* defined(CONFIG_MTD_IFX_LESS_WAIT_CYCLE) */     

    IFX_REG_W32(reg, IFX_EBU_BUSCON0);
    /*--- printk(KERN_INFO "init done\n"); ---*/

#if defined(MTD_NOR_REGISTER_DUMP)
    local_register_dump();
#endif

    return platform_driver_register(&plat_nor_driver);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void __exit plat_nor_exit(void) {
	platform_driver_unregister(&plat_nor_driver);
}

module_init(plat_nor_init);
module_exit(plat_nor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_DESCRIPTION("MTD platform NOR map driver");
