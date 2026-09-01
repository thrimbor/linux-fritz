/*------------------------------------------------------------------------------------------*\
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
\*------------------------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/ioctl.h>

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <asm/mach-atheros/atheros.h>
#include "i2c-ath.h"

/*--- #define DEBUG_ATH_I2C ---*/
#if defined(DEBUG_ATH_I2C)
#define DBG_I2C(a...)      printk(a)
#else
#define DBG_I2C(a...)
#endif

/*--- #define ATH_I2C_USE_START_IRQ ---*/

int ath_i2c_10bit_slave = 0;
int ath_i2c_slave_index = 0;

module_param(ath_i2c_10bit_slave, int, S_IRUGO);
module_param(ath_i2c_slave_index, int, S_IRUGO);

/*------------------------------------------------------------------------------------------*\
 * Issue a soft reset to reset the state machine
\*------------------------------------------------------------------------------------------*/
static void ath_i2c_soft_reset( void ) {
	ath_reg_wr(ATH_I2C_SRESET, I2C_SRESET);
	while(ath_reg_rd(ATH_I2C_SRESET) & I2C_SRESET);
}

/*------------------------------------------------------------------------------------------*\
 * Enable I2C
\*------------------------------------------------------------------------------------------*/
static void ath_i2c_enable(void) {
    DBG_I2C("[%s]\n", __func__);
	ath_reg_wr(ATH_I2C_ENABLE, I2C_ENABLE);
}

/*------------------------------------------------------------------------------------------*\
 * Disable I2C
\*------------------------------------------------------------------------------------------*/
static void ath_i2c_disable(void) {
    DBG_I2C("[%s]\n", __func__);
	ath_reg_wr(ATH_I2C_ENABLE, I2C_DISABLE);
}

/*------------------------------------------------------------------------------------------*\
 * Check if I2C bus is free to start a transaction
\*------------------------------------------------------------------------------------------*/
unsigned long I2C_BusBusy(void) {
	unsigned char   reg_com;
	unsigned long   rc = _I2C_OK;                                          /* Bus-IDLE */

	/* I2C bus status check */
	reg_com = ath_reg_rd(ATH_I2C_STATUS);
	if ( (reg_com & ATH_I2C_STAT_ACTIVITY) != 0 ) {
		rc = -_I2C_ERR_BUSY;                                               /* Bus-BUSY */
	}
	return rc;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int xfer_read(struct i2c_adapter *adap, unsigned char *buf, int length) {

    ath_i2c_dev_t *ath_i2c_device = adap->algo_data;
	int i;

    ath_i2c_device->buffer = buf;
    ath_i2c_device->bufferlen = length;
    ath_i2c_device->rx_len  = 0;

    if (length < 64) {
		/*--- ath_reg_wr(ATH_I2C_OFF_RX_TL, length - 1); ---*/  /*--- read data on RX_FULL-IRQ ---*/
		ath_reg_wr(ATH_I2C_OFF_RX_TL, length);  /*--- read data on STOP-IRQ ---*/ 
    }

	while((ath_reg_rd((ATH_I2C_STATUS)) & ATH_I2C_STAT_ACTIVITY));

	/*--- Write Read command 'n' number of times. ---*/
	for ( i = 0; i < length; i++ ) {
		ath_reg_wr(ATH_I2C_OFF_DATA_CMD, ATH_I2C_CMD_READ);
	}

	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int xfer_write(struct i2c_adapter *adap, unsigned char *buf, int length) {

	int i;

	while((ath_reg_rd((ATH_I2C_STATUS)) & ATH_I2C_STAT_ACTIVITY));

	for (i = 0; i < length; i++) {
		ath_reg_wr((ATH_I2C_OFF_DATA_CMD),  buf[i]);
		while(!(ath_reg_rd((ATH_I2C_STATUS)) & ATH_I2C_STAT_TFNF));
	}

    return 0;
}

/*------------------------------------------------------------------------------------------*\
 * Generic i2c master transfer entrypoint.
\*------------------------------------------------------------------------------------------*/
static int ath_xfer(struct i2c_adapter *adap, struct i2c_msg *pmsg, int num) {

    ath_i2c_dev_t *ath_i2c_device = adap->algo_data;
	int i;

	dev_dbg(&adap->dev, "ath_xfer: processing %d messages:\n", num);
	DBG_I2C("ath_xfer: processing %d messages:\n", num);

	/* Initialization check and Bus-Busy check */
	if ( _I2C_OK != I2C_BusBusy()) { /* Checks if I2C bus is busy */
		return -EBUSY;
	}

	for (i = 0; i < num; i++) {
		dev_dbg(&adap->dev, " #%d: %sing %d byte%s %s 0x%02x\n", i,
			pmsg->flags & I2C_M_RD ? "read" : "writ",
			pmsg->len, pmsg->len > 1 ? "s" : "",
			pmsg->flags & I2C_M_RD ? "from" : "to",	pmsg->addr);

		DBG_I2C(" #%d: %sing %d byte%s %s 0x%02x\n", i,
			pmsg->flags & I2C_M_RD ? "read" : "writ",
			pmsg->len, pmsg->len > 1 ? "s" : "",
			pmsg->flags & I2C_M_RD ? "from" : "to",	pmsg->addr);

        if (pmsg->len > 0) {
		    init_completion(&ath_i2c_device->wait);

            ath_i2c_disable();                              /*--- Disable I2C ---*/

            if (pmsg->flags & I2C_FUNC_10BIT_ADDR)
		        ath_reg_rmw_set(ATH_I2C_OFF_I2CON, ATH_I2C_CON_10BITADDR_MASTER);
            else
		        ath_reg_rmw_clear(ATH_I2C_OFF_I2CON, ATH_I2C_CON_10BITADDR_MASTER);

            ath_reg_wr(ATH_I2C_OFF_TAR, pmsg->addr);        /*--- Write Slave addr, avaiable if i2c disabled ---*/

	        ath_i2c_enable();

			if (pmsg->flags & I2C_M_RD)
				xfer_read(adap, pmsg->buf, pmsg->len);
			else
				xfer_write(adap, pmsg->buf, pmsg->len);

		    wait_for_completion(&ath_i2c_device->wait);     /*--- Wait until transfer is finished ---*/

            if (ath_i2c_device->error) 
                return ath_i2c_device->error;

            DBG_I2C("{%s} rx_len %d\n", __func__, ath_i2c_device->rx_len);
        }

		dev_dbg(&adap->dev, "transfer complete\n");
		DBG_I2C("transfer complete\n");
		pmsg++;		/* next message */
	}
	return i;
}

/*------------------------------------------------------------------------------------------*\
 * ISR to handle I2C interrupts
\*------------------------------------------------------------------------------------------*/
static irqreturn_t ath_i2c_intr(int irq __attribute__((unused)), void *dev) {

    ath_i2c_dev_t *ath_i2c_device = (ath_i2c_dev_t *)dev;

    unsigned int int_status = ath_reg_rd(ATH_I2C_OFF_RAW_INTR_STAT);	

    DBG_I2C("{%s} int_status 0x%x\n", __func__, int_status);

#if defined(ATH_I2C_USE_START_IRQ)
    if (likely(int_status & ATH_I2C_INTR_MASK_START)) {
        ath_reg_rd(ATH_I2C_CLR_START_DET);
    }
#endif

    if (likely(int_status & ATH_I2C_INTR_MASK_RXFULL)) {
        /* Read data cmd register and get the data */
        DBG_I2C("{%s} ATH_I2C_COND_MASK_RXFULL fifo %d\n", __func__, ath_reg_rd(ATH_I2C_RXFLR));
        while (ath_reg_rd(ATH_I2C_RXFLR)) {
            ath_i2c_device->buffer[ath_i2c_device->rx_len] = ath_reg_rd(ATH_I2C_OFF_DATA_CMD) & ATH_I2C_DAT_MASK;
            ath_i2c_device->rx_len++;
            if (ath_i2c_device->rx_len > ath_i2c_device->bufferlen) {
                ath_i2c_device->error = _I2C_ERR_OVER;
                break;
            }
        }
    }	

    /* Slave Tx */
    if (unlikely(int_status & ATH_I2C_INTR_MASK_RDREQ)) {
        /* A RD REQ was received before receiving the offset bytes */
        if (ath_i2c_device->rx_len < MAX_ADDRESS_OFFSET_BYTES) {
            DBG_I2C("No proper offset received \n");
        }

        /* Clear red request interrupt */
        ath_reg_rd(ATH_I2C_OFF_CLR_RD_REQ);
    }

    /* Rx Done received for Slave Tx mode after the last byte is
     * received by the master */
    if (unlikely(int_status & ATH_I2C_INTR_MASK_RXDONE)) {
        DBG_I2C("Rx Done \n");
        /* Clear Rx Done interrupt */
        ath_reg_rd(ATH_I2C_OFF_CLR_RX_DONE);
    }

    if (likely(int_status & ATH_I2C_INTR_MASK_STOP)) {

        DBG_I2C("{%s} ATH_I2C_COND_STOP RX-Fifo 0x%x\n", __func__, ath_reg_rd(ATH_I2C_RXFLR));
        /*--- while(ath_reg_rd(ATH_I2C_STATUS) & ATH_I2C_STAT_RFNE) { ---*/
        while (ath_reg_rd(ATH_I2C_RXFLR)) {
            DBG_I2C("[%s] copy Data on stop condition ATH_I2C_STATUS 0x%x\n", __func__, ath_reg_rd(ATH_I2C_STATUS));
            ath_i2c_device->buffer[ath_i2c_device->rx_len] = ath_reg_rd(ATH_I2C_OFF_DATA_CMD) & ATH_I2C_DAT_MASK;
            ath_i2c_device->rx_len++;
            if (ath_i2c_device->rx_len > ath_i2c_device->bufferlen) {
                ath_i2c_device->error = _I2C_ERR_OVER;
                break;
            }
        }

        ath_reg_rd(ATH_I2C_CLR_STOP_DET);	
		complete(&ath_i2c_device->wait);
    }
    return IRQ_HANDLED;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ath_i2c_init_reg(void) {

#define ATH_I2C_DEF_CON (ATH_I2C_CON_MASTER_MODE | ATH_I2C_CON_SPEED_SS | ATH_I2C_CON_RESTART_EN)

	unsigned int base_slave_addr = I2C_BASE_SLAVE_ADDR;

	/* Issue a Soft reset */
	ath_i2c_soft_reset();

	/* Disable I2C controller to initialize registers */
	ath_i2c_disable();

	/*--- Set Default Parameters for Control and Clock Clock 40 MHZ, Slow Speed ---*/
	ath_reg_wr((ATH_I2C_OFF_I2CON), ATH_I2C_DEF_CON);	
	ath_reg_wr((ATH_I2C_OFF_SS_SCL_HCNT), ATH_I2C_SS_SCL_HCNT_40);
	ath_reg_wr((ATH_I2C_OFF_SS_SCL_LCNT), ATH_I2C_SS_SCL_LCNT_40);

	/* 7 bit slave/10 bit slave */	
	if (ath_i2c_10bit_slave) {
		ath_reg_rmw_set(ATH_I2C_OFF_I2CON, ATH_I2C_CON_IC_10BITADDR_SLAVE);
		base_slave_addr = I2C_10BIT_BASE_ADDR;
	}

	/* Write our OWN slave address from a predefined list based on modparam. As of 
	 * now, 2 slaves are supported */
	ath_reg_wr(ATH_I2C_OFF_SAR, base_slave_addr + ath_i2c_slave_index);

	/* Set Rx Threshold so that the slave can respond when there is
	 * incoming data from the master */

	/* Triggers an Rx FULL interrupt when there is 1 data in the Receive buffer */
	ath_reg_wr(ATH_I2C_OFF_RX_TL, 0x0);

	/* Disable all interrupts initially */
	ath_reg_wr(ATH_I2C_OFF_INTR_MASK, 0x0);

	/* Enable only interrupts which are required */
	ath_reg_wr(ATH_I2C_OFF_INTR_MASK, 
                            ATH_I2C_INTR_MASK_RXFULL | 
                            ATH_I2C_INTR_MASK_RDREQ | 
					        ATH_I2C_INTR_MASK_RXDONE | 
#if defined(ATH_I2C_USE_START_IRQ)
                            ATH_I2C_INTR_MASK_START | 
#endif
                            ATH_I2C_INTR_MASK_STOP
                        ); 

	ath_i2c_enable();
}

/*------------------------------------------------------------------------------------------*\
 * Return list of supported functionality.
\*------------------------------------------------------------------------------------------*/
static u32 ath_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static struct i2c_algorithm ath_algorithm = {
	.master_xfer	= ath_xfer,
	.functionality	= ath_func,
};

/*------------------------------------------------------------------------------------------*\
 * Main initialization routine.
\*------------------------------------------------------------------------------------------*/
static int __devinit ath_i2c_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

    ath_i2c_dev_t *ath_i2c_device;

    DBG_I2C(KERN_ERR "{%s}\n", __func__);


    /* Initialize dev structure */
    ath_i2c_device = kzalloc(sizeof(struct ath_i2c_dev), GFP_KERNEL);
    if (ath_i2c_device == NULL) {
		dev_err(&pdev->dev, "can't allocate Device\n");
		return -ENOMEM;
    }

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
        ret = -ENODEV;
        goto fail0;
    }

	ath_i2c_device->ioarea = request_mem_region(res->start, resource_size(res), pdev->name);
    if (!ath_i2c_device->ioarea) {
		ret = -EBUSY;
        goto fail0;
    }

	ath_i2c_device->iobase = ioremap(res->start, resource_size(res));
	if (!ath_i2c_device->iobase) {
		ret = -ENODEV;
		goto fail1;
	}

	snprintf(ath_i2c_device->adapter.name, sizeof(ath_i2c_device->adapter.name), "ath-i2c");
	ath_i2c_device->adapter.nr = pdev->id;
	ath_i2c_device->adapter.algo = &ath_algorithm;
	ath_i2c_device->adapter.class = I2C_CLASS_HWMON | I2C_CLASS_TV_DIGITAL;
	ath_i2c_device->adapter.retries = 3;
	ath_i2c_device->adapter.algo_data = ath_i2c_device;
	ath_i2c_device->adapter.dev.parent = &pdev->dev;
	/* adapter->id == 0 ... only one TWI controller for now */

	ath_i2c_device->irq      = platform_get_irq(pdev, 0);
	ath_i2c_device->rx_len   = 0;

	platform_set_drvdata(pdev, ath_i2c_device);

	/* Establish ISR would take care of enabling the interrupt */
	if (request_irq(ath_i2c_device->irq, (void *)ath_i2c_intr, IRQF_DISABLED, "ath-i2c", ath_i2c_device)) {
		dev_err(&pdev->dev, "cannot get irq %d\n", ath_i2c_device->irq);
		printk(KERN_INFO "<i2c: can't get assigned irq %d>\n", ath_i2c_device->irq);
        ret = -EBUSY;
        goto fail2;
	}

	ath_i2c_init_reg();     /* Initialize GPIO and default configuration */

	ret = i2c_add_numbered_adapter(&ath_i2c_device->adapter);
	if (ret) {
		dev_err(&pdev->dev, "Adapter %s registration failed\n", ath_i2c_device->adapter.name);
		goto fail3;
	}

	dev_info(&pdev->dev, "ATH i2c bus driver.\n");
	return 0;

fail3:
	free_irq(ath_i2c_device->irq, NULL);
fail2:
    iounmap(ath_i2c_device->iobase);
	platform_set_drvdata(pdev, NULL);
fail1:
    release_resource(ath_i2c_device->ioarea);
    kfree(ath_i2c_device->ioarea);
fail0:
    kfree(ath_i2c_device);

	return ret;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __devexit ath_i2c_remove(struct platform_device *pdev)
{
	struct i2c_adapter *adapter = platform_get_drvdata(pdev);
    ath_i2c_dev_t *ath_i2c_device = adapter->algo_data;
	int ret;


	ret = i2c_del_adapter(adapter);

	free_irq(ath_i2c_device->irq, NULL);
    iounmap(ath_i2c_device->iobase);

    release_resource(ath_i2c_device->ioarea);
    kfree(ath_i2c_device->ioarea);
    kfree(ath_i2c_device);

	platform_set_drvdata(pdev, NULL);

	return ret;
}

MODULE_ALIAS("platform:ath-i2c");

static struct platform_driver ath_i2c_driver = {
	.probe		= ath_i2c_probe,
	.remove		= __devexit_p(ath_i2c_remove),
	.driver		= {
		.name	= "ath-i2c",
		.owner	= THIS_MODULE,
	},
};

static int __init ath_i2c_init(void)
{
	return platform_driver_register(&ath_i2c_driver);
}

static void __exit ath_i2c_exit(void)
{
	platform_driver_unregister(&ath_i2c_driver);
}

module_init(ath_i2c_init);
module_exit(ath_i2c_exit);

MODULE_AUTHOR("AVM (c) 2013");
MODULE_DESCRIPTION("I2C driver for Atheros QCA955x");
MODULE_LICENSE("Dual BSD/GPL");
