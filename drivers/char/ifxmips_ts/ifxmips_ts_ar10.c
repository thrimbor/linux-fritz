/******************************************************************************
**
** FILE NAME    : ifxmips_ts_ar10.c
** PROJECT      : UEIP
** MODULES      : Thermal Sensor
**
** DATE         : 20 Nov 2012
** AUTHOR       : Xu Liang, AVM GmbH <fritzbox_info@avm.de>
** DESCRIPTION  : Thermal Sensor driver AR10 source file
** COPYRIGHT    :       Copyright (c) 2006
**                      Infineon Technologies AG
**                      Am Campeon 1-12, 85579 Neubiberg, Germany
**
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
** HISTORY
** $Date          $Author         $Comment
** Nov 20, 2012   AVM GmbH        Adaption for AR10
** Aug 16, 2011   Xu Liang        Init Version
*******************************************************************************/



/*
 * ####################################
 *              Version No.
 * ####################################
 */

#define VER_MAJOR                   1
#define VER_MID                     0
#define VER_MINOR                   3



/*
 * ####################################
 *              Head File
 * ####################################
 */

/*
 *  Common Head File
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>

/*
 *  Chip Specific Head File
 */
#include <ifx_types.h>
#include <ifx_regs.h>
#include <common_routines.h>
#include "ifxmips_ts.h"



/*
 * ####################################
 *              Definition
 * ####################################
 */

#ifdef CONFIG_AR10
#define IFX_TS_CFG1                  KSEG1ADDR(0x1F106F00)
#define IFX_TS_CFG2                  KSEG1ADDR(0x1F106F04)
#define IFX_TS_S1                    KSEG1ADDR(0x1F106F08)
#define IFX_TS_S2                    KSEG1ADDR(0x1F106F0C)
#define IFX_TS_S3                    KSEG1ADDR(0x1F106F10)

#define IFX_PMU_ANALOG_SR            KSEG1ADDR(0x1F102040)
#define IFX_PMU_ANALOGCR_A           KSEG1ADDR(0x1F102044)
#define IFX_PMU_ANALOGCR_B           KSEG1ADDR(0x1F102048)

#define IFX_TS_INT                  INT_NUM_IM0_IRL19

#else /*--- #ifdef CONFIG_AR10 ---*/
#define IFX_TS_REG                  KSEG1ADDR(0x1F103040)
#define IFX_TS_INT                  INT_NUM_IM3_IRL19
#endif



/*
 * ####################################
 *             Declaration
 * ####################################
 */

/*
 *  File Operations
 */
static int ts_open(struct inode *, struct file *);
static int ts_release(struct inode *, struct file *);
static int ts_ioctl(struct inode *, struct file *, unsigned int, unsigned long);

/*
 *  Interrupt Handler
 */
static irqreturn_t ts_irq_handler(int, void *);

/*
 *  Proc File Functions
 */
static INLINE void proc_file_create(void);
static INLINE void proc_file_delete(void);
static int proc_read_version(char *, char **, off_t, int, int *, void *);
static int proc_read_temp(char *, char **, off_t, int, int *, void *);

/*
 *  Init Help Functions
 */
static INLINE int ifx_ts_version(char *);
static INLINE int print_temp(char *);



/*
 * ####################################
 *            Local Variable
 * ####################################
 */

//static int g_temp_read_enable = 0;

static int g_ts_major;
static struct file_operations g_ts_fops = {
    .owner      = THIS_MODULE,
    .open       = ts_open,
    .release    = ts_release,
    .ioctl      = ts_ioctl,
};

static unsigned int g_dbg_enable = DBG_ENABLE_MASK_ERR;

static struct proc_dir_entry* g_proc_dir = NULL;



/*
 * ####################################
 *            Local Function
 * ####################################
 */

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ts_open(struct inode *inode, struct file *filep)
{
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ts_release(struct inode *inode, struct file *filep)
{
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ts_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    int retval = 0;

    printk(KERN_ERR "IOCTL: Undefined IOCTL call!\n");
    retval = -EACCES;

    return retval;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static irqreturn_t ts_irq_handler(int irq, void *dev_id)
{
    char str[16];

    print_temp(str);
    err("Chip is over heated or 2.5 V error (%s)!", str);
    return IRQ_HANDLED;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static INLINE void proc_file_create(void)
{
    g_proc_dir = proc_mkdir("driver/ifx_ts", NULL);

    create_proc_read_entry("version",
                            0,
                            g_proc_dir,
                            proc_read_version,
                            NULL);

    create_proc_read_entry("temp",
                            0,
                            g_proc_dir,
                            proc_read_temp,
                            NULL);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static INLINE void proc_file_delete(void)
{
    remove_proc_entry("temp", g_proc_dir);
    remove_proc_entry("version", g_proc_dir);
    remove_proc_entry("driver/ifx_ts", NULL);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int proc_read_version(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;

    len += ifx_ts_version(buf + len);
    len += sprintf(buf + len, "build: %s %s\n", __DATE__, __TIME__);
    len += sprintf(buf + len, "major.minor: %d.0\n", g_ts_major);

    *eof = 1;

    return len - off;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int proc_read_temp(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
    int len = print_temp(buf);

    *eof = 1;

    return len - off;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static INLINE int ifx_ts_version(char *buf)
{
    return ifx_drv_ver(buf, "Thermal Sensor", VER_MAJOR, VER_MID, VER_MINOR);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static INLINE int print_temp(char *buf)
{
    int len = 0;
    int temp_value = 0;
    int fraction;

    if ( ifx_ts_get_temp(&temp_value) == 0 ) {
        if ( temp_value < 0 ) {
            len += sprintf(buf + len, "=");
            temp_value = -temp_value;
        }
        len += sprintf(buf + len, "%d", temp_value / 10);
        if ( (fraction = temp_value % 10) != 0 )
            len += sprintf(buf + len, ".%d", fraction);
        len += sprintf(buf + len, "\n");
    }
    else
        len += sprintf(buf + len, "Failed in reading temperature info!\n");

    return len;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if 0
static void dump_regs(const char *prefix) {
    printk(KERN_ERR "%s IFX_TS_CFG1        0x%x\n",                          prefix, IFX_REG_R32(IFX_TS_CFG1));
    printk(KERN_ERR "%s IFX_TS_CFG2        0x%x\n",                          prefix, IFX_REG_R32(IFX_TS_CFG2));
    printk(KERN_ERR "%s IFX_TS_S1          0x%x\n",                          prefix, IFX_REG_R32(IFX_TS_S1));
    printk(KERN_ERR "%s IFX_TS_S2          0x%x\n",                          prefix, IFX_REG_R32(IFX_TS_S2));
    printk(KERN_ERR "%s IFX_TS_S3          0x%x\n",                          prefix, IFX_REG_R32(IFX_TS_S3));
    printk(KERN_ERR "%s IFX_RST_STAT2      0x%x (TEMP: Bit 11 on=reset)\n",  prefix, IFX_REG_R32(IFX_RCU_RST_STAT2));
    printk(KERN_ERR "%s IFX_PMU_ANALOG_SR  0x%x (TEMP: Bit 17 on=enable)\n", prefix, IFX_REG_R32(IFX_PMU_ANALOG_SR));

}
#endif

/*
 * ####################################
 *           Global Function
 * ####################################
 */

/*------------------------------------------------------------------------------------------*\
 * returns temperature in p_temp with 0.1°C accuracy
\*------------------------------------------------------------------------------------------*/
int ifx_ts_get_temp(int *p_temp)
{
    unsigned int temp_value;

    if ( p_temp == NULL )
        return -EINVAL;

#ifdef CONFIG_AR10
    //  value x 5 = Celsius Degree x 10
    temp_value  = IFX_REG_R32(IFX_TS_S1) & 0xFF;
    temp_value |= (IFX_REG_R32(IFX_TS_S2) & 0x1) << 8;
    if(IFX_REG_R32(IFX_TS_S2) & 0x80)
        temp_value *= -1;
    *p_temp = (int)(temp_value * 5); // pretend 0.1 °C accuracy

#else
    //  value x 5 = (Celsius Degree + 38) x 10
    *p_temp = (int)(temp_value * 5 - 380); // pretend 0.1 °C accuracy
#endif

    return 0;
}
EXPORT_SYMBOL(ifx_ts_get_temp);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ifx_ts_config(void) 
{
    // Temperatur-Sensor aus dem Reset holen und Power up:
    IFX_REG_W32_MASK(1 << 12, 0, IFX_RCU_RST_REQ2);
    IFX_REG_W32_MASK(0, 1 << 17, IFX_PMU_ANALOGCR_A);

    // Default-Konfiguration setzen:
    IFX_REG_W32(0, IFX_TS_CFG1);
    IFX_REG_W32(0, IFX_TS_CFG2);

    return 0;
}

/*
 * ####################################
 *           Init/Cleanup API
 * ####################################
 */

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __devinit ts_init(void)
{
    int ret;

    err("%s\n", __FUNCTION__);

#ifndef CONFIG_AR10
    ifx_chipid_t chipid = {0};
    ifx_get_chipid(&chipid);
    if ( chipid.family_id != IFX_FAMILY_xRX200
        || chipid.family_ver != IFX_FAMILY_xRX200_A2x ) {
        err("Only xRX200 A2x chip is supported, family_id = %d, family_ver = %d\n",
            chipid.family_id, chipid.family_ver);
        return -EIO;
    }
#endif

    ret = register_chrdev(IFX_TS_MAJOR, "ifx_ts", &g_ts_fops);
#if IFX_TS_MAJOR == 0
    g_ts_major = ret;
#else
    g_ts_major = IFX_TS_MAJOR;
#endif
    if ( ret < 0 ) {
        err("Can not register thermal sensor device - %d", ret);
        return ret;
    }

#ifdef CONFIG_AR10
    ifx_ts_config();
#else
    IFX_REG_W32_MASK(0, 0x00080000, IFX_TS_REG);    //  turn on Thermal Sensor
#endif

    ret = request_irq(IFX_TS_INT, ts_irq_handler, IRQF_DISABLED, "ts_isr", NULL);
    if ( ret ) {
        err("Can not get IRQ - %d", ret);
        unregister_chrdev(g_ts_major, "ifx_ts");
        return ret;
    }

    proc_file_create();

    return 0;
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void __exit ts_exit(void)
{
    proc_file_delete();

    free_irq(IFX_TS_INT, NULL);

    /*--- IFX_REG_W32_MASK(0x00080000, 0, IFX_TS_REG);    //  turn off Thermal Sensor ---*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32)
    if ( unregister_chrdev(g_ts_major, "ifx_ts") ) {
        err("Can not unregister TS device (major %d)!", g_ts_major);
    }
#else
    unregister_chrdev(g_ts_major, "ifx_ts");
#endif
}

module_init(ts_init);
module_exit(ts_exit);

MODULE_AUTHOR("Xu Liang");
MODULE_LICENSE("GPL");
