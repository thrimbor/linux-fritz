/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>

#include <asm/mach_avm.h>
#include <ur8.h>
#include <hw_gpio.h>
#include <hw_reset.h>
#include <ur8_gpio.h>

#define AR7GPIO_DEBUG

#if defined(AR7GPIO_DEBUG)
#define DBG(...)  printk(KERN_INFO __VA_ARGS__)
#else /*--- #if defined(AR7GPIO_DEBUG) ---*/
#define DBG(...)  
#endif /*--- #else ---*/ /*--- #if defined(AR7GPIO_DEBUG) ---*/

#if defined (CONFIG_MIPS_UR8)
extern union _hw_non_reset *UR8_hw_non_reset;
extern struct _hw_gpio *UR8_hw_gpio;
static spinlock_t ur8_gpio_spinlock;
static const char *ur8_gpio_list[UR8_GPIO_MAX];

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ur8_gpio_init(void) {

    /*--- printk("[ur8_gpio_init]\n"); ---*/
    UR8_hw_non_reset->Bits.gpio_unreset = 1;
    spin_lock_init(&ur8_gpio_spinlock);
    ur8_take_device_out_of_reset(GPIO_RESET_BIT);
}

/*------------------------------------------------------------------------------------------*\
 * KNOWN BUG: ur8_gpio_init wird nie aufgerufen => scheint aber trotzdem zu funktionieren:
 * FIX:
\*------------------------------------------------------------------------------------------*/
/*--- arch_initcall(ur8_gpio_init); ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_gpio_ctrl(unsigned int gpio_pin, enum _hw_gpio_function pin_mode, enum _hw_gpio_direction pin_dir) {
    unsigned long flags;
    union _hw_gpio_bits *p_gpio_dir;
    union _hw_gpio_bits *p_gpio_enable;

#if 0
    printk("[ur8_gpio_ctrl] gpio=%u as %s direction=%s\n", gpio_pin, 
            pin_mode == GPIO_PIN ? "gpio" : "function",
            pin_dir == GPIO_INPUT_PIN ? "input" : "output");
#endif

    if(gpio_pin > 44)
        return(-1);

    if(gpio_pin > 31) {
        p_gpio_dir = (union _hw_gpio_bits *)&UR8_hw_gpio->Direction2;
        p_gpio_enable = (union _hw_gpio_bits *)&UR8_hw_gpio->Enable2;
        gpio_pin -= 32;
    } else {
        p_gpio_dir = &UR8_hw_gpio->Direction;
        p_gpio_enable = &UR8_hw_gpio->Enable;
    }

    spin_lock_irqsave(&ur8_gpio_spinlock, flags);

    if(pin_mode == GPIO_PIN) {
        p_gpio_enable->Register |= (1 << gpio_pin);
        if (pin_dir == GPIO_INPUT_PIN)
            p_gpio_dir->Register |= (1 << gpio_pin);
        else
            p_gpio_dir->Register &= ~(1 << gpio_pin);

    } else { /* FUNCTIONAL PIN */
        p_gpio_enable->Register &= ~(1 << gpio_pin);
    }

    spin_unlock_irqrestore(&ur8_gpio_spinlock, flags);
    return (0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_gpio_out_bit(unsigned int gpio_pin, int value) {
    unsigned long flags;
    union _hw_gpio_bits *p_gpio_out;

    /*--- printk("[ur8_gpio_out_bit] gpio=%u value=%u\n", gpio_pin, value); ---*/

    if(gpio_pin > 44)
        return(-1);

    if(gpio_pin > 31) {
        p_gpio_out = (union _hw_gpio_bits *)&UR8_hw_gpio->OutputData2;
        gpio_pin -= 32;
    } else {
        p_gpio_out = &UR8_hw_gpio->OutputData;
    }

    spin_lock_irqsave(&ur8_gpio_spinlock, flags);
    if(value == 1)
        p_gpio_out->Register |= 1 << gpio_pin;
    else
        p_gpio_out->Register &= ~(1 << gpio_pin);
    spin_unlock_irqrestore(&ur8_gpio_spinlock, flags);

    return(0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_gpio_in_bit(unsigned int gpio_pin) {

    /*--- printk("[ur8_gpio_in_bit] gpio=%u\n", gpio_pin); ---*/
    if(gpio_pin >= 44)
        return(-1);
    
    if(gpio_pin > 31) {
        gpio_pin -= 32;
        return (UR8_hw_gpio->InputData2.Register & (1 << gpio_pin));
    } else {
        return (UR8_hw_gpio->InputData.Register & (1 << gpio_pin));
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ur8_gpio_in_value(void) {
    /*--- printk("[ur8_gpio_in_value]\n"); ---*/
    return UR8_hw_gpio->InputData.Register;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ur8_gpio_set_bitmask(unsigned int mask, unsigned int value) {
    unsigned long flags;
    /*--- printk("[ur8_gpio_set_bitmask] mask=0x%x value=0x%x\n", mask, value); ---*/
    spin_lock_irqsave(&ur8_gpio_spinlock, flags);
    UR8_hw_gpio->OutputData.Register &=  ~mask;
    UR8_hw_gpio->OutputData.Register |=  value & mask;
    spin_unlock_irqrestore(&ur8_gpio_spinlock, flags);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_gpio_request(unsigned gpio, const char *label)
{
	if (gpio >= UR8_GPIO_MAX)
		return -EINVAL;

	if (ur8_gpio_list[gpio])
		return -EBUSY;

	if (label)
		ur8_gpio_list[gpio] = label;
	else
		ur8_gpio_list[gpio] = "busy";

	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ur8_gpio_free(unsigned gpio)
{
	BUG_ON(!ur8_gpio_list[gpio]);
	ur8_gpio_list[gpio] = NULL;
}

EXPORT_SYMBOL(ur8_gpio_ctrl);
EXPORT_SYMBOL(ur8_gpio_out_bit);
EXPORT_SYMBOL(ur8_gpio_in_bit);
EXPORT_SYMBOL(ur8_gpio_in_value);
EXPORT_SYMBOL(ur8_gpio_set_bitmask);
EXPORT_SYMBOL(ur8_gpio_request);
EXPORT_SYMBOL(ur8_gpio_free);

#endif /*--- #if defined (CONFIG_MIPS_UR8) ---*/
