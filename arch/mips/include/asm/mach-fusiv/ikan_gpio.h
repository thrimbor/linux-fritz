/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifndef _ikan_gpio_h
#define _ikan_gpio_h

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <hw_gpio.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ikan_gpio_init(void);
int ikan_gpio_ctrl(unsigned int gpio_pin, enum _hw_gpio_function pin_mode, enum _hw_gpio_direction pin_dir);

struct _hw_gpio_irqhandle *ikan_gpio_request_irq(unsigned int mask, enum _hw_gpio_polarity mode, 
                                                 enum _hw_gpio_sensitivity edge, int (*handle_func)(unsigned int));
void ikan_gpio_set_delayed_irq_mode(struct _hw_gpio_irqhandle *handle, int delayed);
int ikan_gpio_enable_irq(struct _hw_gpio_irqhandle *handle);
int ikan_gpio_disable_irq(struct _hw_gpio_irqhandle *handle);
int ikan_read_irq_gpio(struct _hw_gpio_irqhandle *handle, unsigned int gpio_pin, unsigned int mask);

int ikan_gpio_out_bit(unsigned int gpio_pin, int value);
int ikan_gpio_in_bit(unsigned int gpio_pin);
unsigned int ikan_gpio_in_value(void);
void ikan_gpio_set_bitmask(unsigned int mask, unsigned int value);





#endif /*--- #ifndef _ikan_gpio_h ---*/
