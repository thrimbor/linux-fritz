/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/kernel_stat.h>
#include <linux/delay.h>

#include <ur8.h>
#include <asm/mach_avm.h>
#include <hw_reset.h>

static struct _ur8_gpio_reset_bits {
    unsigned long long Bit_Mask;
    unsigned long long Polarity_Mask;
} ur8_gpio_reset_bits;

static spinlock_t ur8_reset_spinlock;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_reset_init(void) {
    spin_lock_init(&ur8_reset_spinlock);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_put_device_into_reset(unsigned int Bit) {
    struct _hw_reset *RESET = (struct _hw_reset *)UR8_RESET_BASE;
    if(Bit < UR8_RESET_START_GPIO) {
        unsigned long flags;
        spin_lock_irqsave(&ur8_reset_spinlock, flags);
        RESET->non_reset.Reg &= ~(1 << Bit);
        spin_unlock_irqrestore(&ur8_reset_spinlock, flags);
        return 0;
    }
    if(Bit < UR8_RESET_END_GPIO) {
        Bit -= UR8_RESET_START_GPIO;
        ur8_gpio_out_bit(Bit, (ur8_gpio_reset_bits.Polarity_Mask & (1 << Bit)) ? 1 : 0);
        return 0;
    }
#if defined(CONFIG_VLYNQ_SUPPORT)
    if(Bit < UR8_RESET_END_VIRTUAL) {
        return 0;
    }
#endif /*--- #if defined(CONFIG_VLYNQ_SUPPORT) ---*/
    return 1;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_take_device_out_of_reset(unsigned int Bit) {
    struct _hw_reset *RESET = (struct _hw_reset *)UR8_RESET_BASE;
    if(Bit < UR8_RESET_START_GPIO) {
        unsigned long flags;
        spin_lock_irqsave(&ur8_reset_spinlock, flags);
        RESET->non_reset.Reg |= (1 << Bit);
        spin_unlock_irqrestore(&ur8_reset_spinlock, flags);
        return 0;
    }
    if(Bit < UR8_RESET_END_GPIO) {
        Bit -= UR8_RESET_START_GPIO;
        ur8_gpio_out_bit(Bit, (ur8_gpio_reset_bits.Polarity_Mask & (1 << Bit)) ? 0 : 1);
        return 0;
    }
#if defined(CONFIG_VLYNQ_SUPPORT)
    if(Bit < UR8_RESET_END_VIRTUAL) {
        return 0;
    }
#endif /*--- #if defined(CONFIG_VLYNQ_SUPPORT) ---*/
    return 1;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_reset_device(unsigned int Bit, unsigned int msec_delay) {
    unsigned int ret;

    ret = ur8_put_device_into_reset(Bit);
    if(ret) return ret;
    if(msec_delay)
        mdelay(msec_delay);
    ret = ur8_take_device_out_of_reset(Bit);
    if(ret) return ret;
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_register_reset_gpio(unsigned int gpio_pin, t_reset_gpio_polarity Polarity) {

    if(gpio_pin > 44)
        return (unsigned int)-1;

    if(ur8_gpio_reset_bits.Bit_Mask & (1 << gpio_pin)) 
        return (unsigned int)-1;

    ur8_gpio_reset_bits.Bit_Mask      |= (1 << gpio_pin);
    if(Polarity)
        ur8_gpio_reset_bits.Polarity_Mask |=  (1 << gpio_pin);
    else
        ur8_gpio_reset_bits.Polarity_Mask &= ~(1 << gpio_pin);
        
    ur8_gpio_ctrl(gpio_pin, GPIO_PIN, GPIO_OUTPUT_PIN);

    return gpio_pin + UR8_RESET_START_GPIO;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ur8_release_reset_gpio(unsigned int Bit) {
    unsigned int gpio_pin = Bit - UR8_RESET_START_GPIO;
    if(gpio_pin > 44)
        return (unsigned int)-1;
    if(!(ur8_gpio_reset_bits.Bit_Mask & (1 << gpio_pin)))
        return (unsigned int)-1;
    ur8_gpio_reset_bits.Bit_Mask &= ~(1 << gpio_pin);
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ur8_reset_status(void) {
    struct _hw_reset *RESET = (struct _hw_reset *)UR8_RESET_BASE;
    
    return (unsigned int)RESET->reset_status.Bits.cause;
}

EXPORT_SYMBOL(ur8_put_device_into_reset);
EXPORT_SYMBOL(ur8_take_device_out_of_reset);
EXPORT_SYMBOL(ur8_reset_device);
EXPORT_SYMBOL(ur8_register_reset_gpio);
EXPORT_SYMBOL(ur8_release_reset_gpio);
EXPORT_SYMBOL(ur8_reset_status);

