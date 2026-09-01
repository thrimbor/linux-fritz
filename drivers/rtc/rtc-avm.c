/*
 * An RTC avm device/driver
 * Copyright (C) 2005 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/rtc.h>
#include <linux/rtc-avm.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>

/*--- #define RTC_AVM_DEBUG           1 ---*/
/*--- #define RTC_AVM_DEVICE_COUNT    2 ---*/

static struct platform_device *rtc_avm0 = NULL, *rtc_avm1 __attribute__((unused)) = NULL;

#define RTC_AVM_AIE         0x01
#define RTC_AVM_UIE         0x02
#define RTC_AVM_PIE         0x04
#define RTC_AVM_WIE         0x08

volatile unsigned long rtc_avm_time = 0;
volatile unsigned long rtc_avm_time_msec = 0;
volatile unsigned long long rtc_avm_flags = 0;
volatile unsigned long avm_irq_rate = 0;
spinlock_t avm_rtc_irq_lock;
struct class *rtc_avm_osclass;

struct timer_list avm_rtc_timer;
unsigned int avm_rtc_timer_inuse = 0;

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void avm_rtc_timer_function(unsigned long context);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void *avm_rtc_register_external_interrupt(char *Name) {
#ifdef RTC_AVM_DEBUG
    printk(KERN_ERR "[avm-rtc]: avm_rtc_register_external_interrupt(%s)\n", Name);
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
    del_timer(&avm_rtc_timer);
    avm_rtc_timer_inuse = 0;
    avm_irq_rate = 0;
    return (void *)rtc_avm0;
}
EXPORT_SYMBOL(avm_rtc_register_external_interrupt);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int avm_rtc_external_interrupt(void *handle, unsigned int interval, unsigned int failed) {
    struct platform_device *plat_dev = (struct platform_device *)handle;
	struct rtc_device *rtc = platform_get_drvdata(plat_dev);
    static unsigned long long last_local_rtc_avm_time = 0ULL;
    static unsigned long last_timer_update = 0;
    unsigned long long local_rtc_avm_time;

    if(failed && ((unsigned int)jiffies < 5)) {
        failed = 0;
    }
    rtc_avm_time_msec += (unsigned long long)(interval * (failed + 1));
    if(rtc_avm_time_msec > 1000) {
        rtc_avm_time_msec -= 1000;
        rtc_avm_time++;
    }

#ifdef RTC_AVM_DEBUG
    if(failed) {
        printk(KERN_ERR "[avm-rtc]: %d more failed interrupts\n", failed);
    }
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/

    if(rtc_avm_flags & RTC_AVM_UIE) {  /*--- jede Sekunde ein Interrupt ---*/
        if(last_timer_update != rtc_avm_time) {
            rtc_update_irq(rtc, 1, RTC_IRQF | RTC_UF);
            /*--- printk("|"); ---*/
        }
    }
    if(rtc_avm_flags & RTC_AVM_PIE) {  /*--- einstellbar periodisch interrupts ---*/
        local_rtc_avm_time = (unsigned long long)rtc_avm_time * 1000ULL + (unsigned long long)rtc_avm_time_msec;

        if(last_local_rtc_avm_time == 0ULL) {
            printk(KERN_ERR "[avm-rtc]: first call\n");
            last_local_rtc_avm_time = local_rtc_avm_time;
        } else {
            if(avm_irq_rate) {
                while(local_rtc_avm_time >= last_local_rtc_avm_time + (unsigned long long)avm_irq_rate) {
                    last_local_rtc_avm_time += (unsigned long long)avm_irq_rate;
                    if(rtc_avm_flags & RTC_AVM_PIE) {  /*--- einstellbar periodisch interrupts ---*/
                        rtc_update_irq(rtc, 1, RTC_IRQF | RTC_PF);
                        /*--- printk("|"); ---*/
                    }
                }
            }
        }
    } else {
        last_local_rtc_avm_time = 0ULL;
    }
    last_timer_update = rtc_avm_time;
    return 0;
}

EXPORT_SYMBOL(avm_rtc_external_interrupt);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
int avm_rtc_release_external_interrupt(void *handle) {
#ifdef RTC_AVM_DEBUG
    printk(KERN_ERR "[avm-rtc]: avm_rtc_release_external_interrupt(%p)\n", handle);
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
    setup_timer(&avm_rtc_timer, avm_rtc_timer_function, (unsigned long)rtc_avm0);
    avm_rtc_timer.expires = jiffies + 1;
    avm_irq_rate = 0;
    add_timer(&avm_rtc_timer);
    avm_rtc_timer_inuse = 1;
    return 0;
}
EXPORT_SYMBOL(avm_rtc_release_external_interrupt);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void avm_rtc_timer_function(unsigned long context) {
    avm_rtc_external_interrupt((void *)context, 1000 / HZ, 0);
    avm_rtc_timer.expires++;  /*--- das nächste mal wieder ---*/
    add_timer(&avm_rtc_timer);
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
#ifdef AVM_RTC_ALARM
static int avm_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm) {
	return -ENODEV;
}
#endif /*--- #ifdef AVM_RTC_ALARM ---*/

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
#ifdef AVM_RTC_ALARM
static int avm_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm) {
	return -ENODEV;
}
#endif /*--- #ifdef AVM_RTC_ALARM ---*/

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avm_rtc_read_time(struct device *dev, struct rtc_time *tm) {
    unsigned long long time;
	unsigned long flags __attribute__((unused));
    time = rtc_avm_time;

	rtc_time_to_tm((unsigned int)(time), tm);
#ifdef RTC_AVM_DEBUG
    printk(KERN_ERR "[avm-rtc]: avm_rtc_read_time (%s) %02d:%02d:%02d:  %d.%d.%d\n",
            dev->driver->name ?  dev->driver->name : "no driver",
            tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_mday, tm->tm_mon, tm->tm_year + 1900);
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
	return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avm_rtc_set_time(struct device *dev, struct rtc_time *tm) {
    unsigned long time;
	unsigned long flags;
#ifdef RTC_AVM_DEBUG
    printk(KERN_ERR "[avm-rtc]: avm_rtc_set_time (%s) %02d:%02d:%02d:  %d.%d.%d\n",
            dev->driver->name ?  dev->driver->name : "no driver",
            tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_mday, tm->tm_mon, tm->tm_year + 1900);
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
    rtc_tm_to_time(tm, &time);
	spin_lock_irqsave(&avm_rtc_irq_lock, flags);
    rtc_avm_time = (unsigned long long)time;
	spin_unlock_irqrestore(&avm_rtc_irq_lock, flags);
	return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avm_rtc_set_mmss(struct device *dev, unsigned long secs)__attribute__((unused));
static int avm_rtc_set_mmss(struct device *dev, unsigned long secs) {
#ifdef RTC_AVM_DEBUG
    printk(KERN_ERR "[avm-rtc]: avm_rtc_set_mmss\n");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
	return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avm_rtc_proc(struct device *dev, struct seq_file *seq) {
	struct platform_device *plat_dev = to_platform_device(dev);

#ifdef RTC_AVM_DEBUG
    printk(KERN_ERR "[avm-rtc]: avm_rtc_proc\n");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/

	seq_printf(seq, "avm\t\t: yes\n");
	seq_printf(seq, "id\t\t: %d\n", plat_dev->id);

	return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avm_rtc_ioctl(struct device *dev, unsigned int cmd, unsigned long arg) {
	void __user *uarg = (void __user *) arg;
	/* We do support interrupts, they're generated
	 * using the sysfs interface.
	 */
#ifdef RTC_AVM_DEBUG
    printk(KERN_ERR "[avm-rtc]: avm_rtc_ioctl, cmd=0x%x arg=0x%lx\n", cmd, arg);
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/

	switch (cmd) {
        case RTC_PIE_ON:  /*--- update interrupt enable ---*/
#ifdef RTC_AVM_DEBUG
            printk(KERN_ERR "[avm-rtc] enable PI (update interrupt)\n");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
            rtc_avm_flags |= RTC_AVM_PIE;
            break;
        case RTC_PIE_OFF:
#ifdef RTC_AVM_DEBUG
            printk(KERN_ERR "[avm-rtc] disable PI (update interrupt)\n");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
            rtc_avm_flags &= ~RTC_AVM_PIE;
            break;
        case RTC_UIE_ON:  /*--- update interrupt enable ---*/
#ifdef RTC_AVM_DEBUG
            printk(KERN_ERR "[avm-rtc] enable UI (update interrupt)\n");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
            rtc_avm_flags |= RTC_AVM_UIE;
            break;
        case RTC_UIE_OFF:
#ifdef RTC_AVM_DEBUG
            printk(KERN_ERR "[avm-rtc] disable UI (update interrupt)\n");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
            rtc_avm_flags &= ~RTC_AVM_UIE;
            break;
        /*--- case RTC_AIE_ON: ---*/ /*--- alarm interrupt enable ---*/
        /*--- case RTC_AIE_OFF: ---*/

#ifdef AVM_RTC_ALARM
        /*--- case RTC_ALM_SET: ---*/	/*--- _IOW('p', 0x07, struct rtc_time) ---*/ /* Set alarm time  */
        /*--- case RTC_ALM_READ: ---*/	/*--- _IOR('p', 0x08, struct rtc_time) ---*/ /* Read alarm time */
#endif /*--- #ifdef AVM_RTC_ALARM ---*/

        case RTC_SET_TIME:	/*--- _IOW('p', 0x0a, struct rtc_time) ---*/ /* Set RTC time    */
            {
                struct rtc_time tm;
#ifdef RTC_AVM_DEBUG
                printk(KERN_ERR "[avm-rtc] set time\n");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/

                if (copy_from_user(&tm, uarg, sizeof(tm)))
                    return -EFAULT;
                avm_rtc_set_time(dev, &tm);
            }
            break;
        case RTC_RD_TIME:	/*--- _IOR('p', 0x09, struct rtc_time) ---*/ /* Read RTC time   */
            {
                struct rtc_time tm;
#ifdef RTC_AVM_DEBUG
                printk(KERN_ERR "[avm-rtc] read time\n");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
                avm_rtc_read_time(dev, &tm);
                if (copy_to_user(uarg, &tm, sizeof(tm)))
                    return -EFAULT;
            }
            break;

        case RTC_IRQP_READ:
            {
                if (copy_to_user(uarg, (void const*)&avm_irq_rate, sizeof(avm_irq_rate)))
                    return -EFAULT;
#ifdef RTC_AVM_DEBUG
                printk(KERN_ERR "[avm-rtc] read irq poll rate\n");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
            }
            break;

        case RTC_IRQP_SET:
            {
#ifdef RTC_AVM_DEBUG
                printk(KERN_ERR "[avm-rtc] set irq poll rate\n");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
                if (copy_to_user((void*)&avm_irq_rate, uarg, sizeof(avm_irq_rate)))
                    return -EFAULT;
            }
            break;

        /*--- case RTC_EPOCH_READ: ---*/	/*--- _IOR('p', 0x0d, unsigned long) ---*/	 /* Read epoch      */
        /*--- case RTC_EPOCH_SET: ---*/	/*--- _IOW('p', 0x0e, unsigned long) ---*/	 /* Set epoch       */

        /*--- case RTC_WKALM_SET: ---*/	/*--- _IOW('p', 0x0f, struct rtc_wkalrm) ---*//* Set wakeup alarm*/
        /*--- case RTC_WKALM_RD: ---*/	/*--- _IOR('p', 0x10, struct rtc_wkalrm) ---*//* Get wakeup alarm*/

        /*--- case RTC_PLL_GET: ---*/	/*--- _IOR('p', 0x11, struct rtc_pll_info) ---*/  /* Get PLL correction */
        /*--- case RTC_PLL_SET: ---*/	/*--- _IOW('p', 0x12, struct rtc_pll_info) ---*/  /* Set PLL correction */

        default:
            printk(KERN_ERR "[avm-rtc] unsupported command 0x%x\n", cmd);
            return -ENOIOCTLCMD;
	}
    return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static const struct rtc_class_ops avm_rtc_ops = {
	.proc = avm_rtc_proc,
	.read_time = avm_rtc_read_time,
	.set_time = avm_rtc_set_time,
#ifdef AVM_RTC_ALARM
	.read_alarm = avm_rtc_read_alarm,
	.set_alarm = avm_rtc_set_alarm,
#endif /*--- #ifdef AVM_RTC_ALARM ---*/
	/*--- .set_mmss = avm_rtc_set_mmss, ---*/
	.ioctl = avm_rtc_ioctl,
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static ssize_t avm_irq_show(struct device *dev, struct device_attribute *attr, char *buf) {
#ifdef RTC_AVM_DEBUG
    printk(KERN_ERR "[avm-rtc]: avm_irq_show\n");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/
	return sprintf(buf, "%d\n", 42);
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static ssize_t avm_irq_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	int retval;
	struct platform_device *plat_dev = to_platform_device(dev);
	struct rtc_device *rtc = platform_get_drvdata(plat_dev);

#ifdef RTC_AVM_DEBUG
    printk(KERN_ERR "[avm-rtc]: avm_irq_store (%s)\n",
            dev->driver->name ?  dev->driver->name : "no driver");
#endif /*--- #ifdef RTC_AVM_DEBUG ---*/

	retval = count;
	if (strncmp(buf, "tick", 4) == 0)
		rtc_update_irq(rtc, 1, RTC_PF | RTC_IRQF);
#ifdef AVM_RTC_ALARM
	else if (strncmp(buf, "alarm", 5) == 0)
		rtc_update_irq(rtc, 1, RTC_AF | RTC_IRQF);
#endif /*--- #ifdef AVM_RTC_ALARM ---*/
	else if (strncmp(buf, "update", 6) == 0)
		rtc_update_irq(rtc, 1, RTC_UF | RTC_IRQF);
	else
		retval = -EINVAL;

	return retval;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static DEVICE_ATTR(irq, S_IRUGO | S_IWUSR, avm_irq_show, avm_irq_store);

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int avm_rtc_probe(struct platform_device *plat_dev) {
	int err;
	struct rtc_device *rtc = rtc_device_register("avm", &plat_dev->dev,
						&avm_rtc_ops, THIS_MODULE);

    printk(KERN_ERR "[avm-rtc]: avm_rtc_probe: register: ret=0x%p\n", rtc);

	if (IS_ERR(rtc)) {
		err = PTR_ERR(rtc);
		return err;
	}
	err = device_create_file(&plat_dev->dev, &dev_attr_irq);
    if(err) {
        printk(KERN_ERR "[avm-rtc]: device_create_file returned the following error: %d\n", err);
    }

	platform_set_drvdata(plat_dev, rtc);

    printk(KERN_ERR "[avm-rtc]: avm_rtc_probe: success\n");
	return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int __devexit avm_rtc_remove(struct platform_device *plat_dev) {
	struct rtc_device *rtc = platform_get_drvdata(plat_dev);

    if(avm_rtc_timer_inuse) {
        avm_rtc_timer_inuse = 0;
        del_timer(&avm_rtc_timer);
    }
	rtc_device_unregister(rtc);
	device_remove_file(&plat_dev->dev, &dev_attr_irq);

    printk(KERN_ERR "[avm-rtc]: avm_rtc_remove: success\n");

	return 0;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static struct platform_driver avm_driver = {
	.probe	= avm_rtc_probe,
	.remove = __devexit_p(avm_rtc_remove),
	.driver = {
		.name = "rtc-avm",
		.owner = THIS_MODULE,
	},
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int __init avm_rtc_init(void)
{
	int err;

	spin_lock_init(&avm_rtc_irq_lock);

	if ((err = platform_driver_register(&avm_driver)))
		return err;

	if ((rtc_avm0 = platform_device_alloc("rtc-avm", 0)) == NULL) {
		err = -ENOMEM;
		goto exit_driver_unregister;
	}

#if RTC_AVM_DEVICE_COUNT > 1
	if ((rtc_avm1 = platform_device_alloc("rtc-avm", 1)) == NULL) {
		err = -ENOMEM;
		goto exit_free_avm0;
	}
#endif /*--- #if RTC_AVM_DEVICE_COUNT > 1 ---*/

	if ((err = platform_device_add(rtc_avm0)))
		goto exit_free_avm1;

#if RTC_AVM_DEVICE_COUNT > 1
	if ((err = platform_device_add(rtc_avm1)))
		goto exit_device_unregister;
#endif /*--- #if RTC_AVM_DEVICE_COUNT > 1 ---*/

    /*--- Geraetedatei anlegen: ---*/
    rtc_avm_osclass = class_create(THIS_MODULE, "rtc-avm");
    device_create(rtc_avm_osclass, NULL, rtc_avm0->dev.devt, NULL, "%s%d", "rtc-avm", 0);
#if RTC_AVM_DEVICE_COUNT > 1
    device_create(rtc_avm_osclass, NULL, rtc_avm1->dev.devt, NULL, "%s%d", "rtc-avm", 1);
#endif /*--- #if RTC_AVM_DEVICE_COUNT > 1 ---*/

    avm_rtc_release_external_interrupt(NULL);

	return 0;

#if RTC_AVM_DEVICE_COUNT > 1
exit_device_unregister:
#endif /*--- #if RTC_AVM_DEVICE_COUNT > 1 ---*/
	platform_device_unregister(rtc_avm0);

exit_free_avm1:
#if RTC_AVM_DEVICE_COUNT > 1
	platform_device_put(rtc_avm1);
#endif /*--- #if RTC_AVM_DEVICE_COUNT > 1 ---*/

#if RTC_AVM_DEVICE_COUNT > 1
exit_free_avm0:
#endif /*--- #if RTC_AVM_DEVICE_COUNT > 1 ---*/
	platform_device_put(rtc_avm0);

exit_driver_unregister:
	platform_driver_unregister(&avm_driver);
	return err;
}

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void __exit avm_rtc_exit(void)
{
	platform_device_unregister(rtc_avm0);
#if RTC_AVM_DEVICE_COUNT > 1
	platform_device_unregister(rtc_avm1);
    device_destroy(rtc_avm_osclass, rtc_avm1->dev.devt);
#endif /*--- #if RTC_AVM_DEVICE_COUNT > 1 ---*/
    device_destroy(rtc_avm_osclass, rtc_avm0->dev.devt);
    class_destroy(rtc_avm_osclass);
	platform_driver_unregister(&avm_driver);
}

/*--- MODULE_AUTHOR("Alessandro Zummo <a.zummo@towertech.it>"); ---*/
MODULE_DESCRIPTION("RTC avm driver/device");
MODULE_LICENSE("GPL");

module_init(avm_rtc_init);
module_exit(avm_rtc_exit);
