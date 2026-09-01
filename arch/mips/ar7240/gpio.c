#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

//#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/resource.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <asm/types.h>
#include <asm/irq.h>
#include <asm/delay.h>
#include <asm/system.h>
#include <linux/slab.h>

#include <ar7240_gpio.h>
#include <ar7240.h>


void ar7240_avm_gpio_dump_registers(void);

struct _hw_gpio_irqhandle *ar7240_avm_gpio_first_handle;

unsigned int ar7240_gpio_int_both_edge;


/*------------------------------------------------------------------------------------------*\
 * LOW LEVEL GPIO ACCESS FUNCTIONS
\*------------------------------------------------------------------------------------------*/


void ar7240_gpio_config_output(int gpio) {
#ifdef CONFIG_WASP_SUPPORT
    ar7240_reg_rmw_clear(AR7240_GPIO_OE, (1 << gpio));
#else
    ar7240_reg_rmw_set(AR7240_GPIO_OE, (1 << gpio));
#endif
}

void ar7240_gpio_config_input(int gpio)
{
#ifdef CONFIG_WASP_SUPPORT
    ar7240_reg_rmw_set(AR7240_GPIO_OE, (1 << gpio));
#else
    ar7240_reg_rmw_clear(AR7240_GPIO_OE, (1 << gpio));
#endif
}

void ar7240_gpio_out_val(int gpio, int val) {
    if (val & 0x1) {
        ar7240_reg_rmw_set(AR7240_GPIO_OUT, (1 << gpio));
    }
    else {
        ar7240_reg_rmw_clear(AR7240_GPIO_OUT, (1 << gpio));
    }
}

int ar7240_gpio_in_val(int gpio) {
    /*--- printk(KERN_ERR "[%s] gpio=0x%x both_edge=0x%x => val=0x%x (active %s)\n", __FUNCTION__, ---*/
            /*--- (1 << gpio) & ar7240_reg_rd(AR7240_GPIO_IN), ar7240_gpio_int_both_edge, ---*/
            /*--- (1 << gpio) & (ar7240_reg_rd(AR7240_GPIO_IN) ^ ar7240_gpio_int_both_edge), ---*/
            /*--- (ar7240_reg_rd(AR7240_GPIO_INT_POLARITY) & (1 << gpio)) ? "high" : "low"); ---*/
    return (1 << gpio) & (ar7240_reg_rd(AR7240_GPIO_IN) ^ ar7240_gpio_int_both_edge);
}

static void ar7240_gpio_intr_enable(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
    /*--- printk(KERN_ERR "[%s] status=0x%x\n", __FUNCTION__, desc->status); ---*/
    ar7240_reg_rmw_set(AR7240_GPIO_INT_MASK, 
                      (1 << (irq - AR7240_GPIO_IRQ_BASE)));
	desc->status &= ~IRQ_MASKED;
}

static void ar7240_gpio_intr_unmask(unsigned int irq)
{
    ar7240_reg_rmw_set(AR7240_GPIO_INT_MASK, 
                      (1 << (irq - AR7240_GPIO_IRQ_BASE)));
}

static void ar7240_gpio_intr_mask(unsigned int irq)
{
    ar7240_reg_rmw_clear(AR7240_GPIO_INT_MASK, (1 << (irq - AR7240_GPIO_IRQ_BASE)));
}

static void ar7240_gpio_intr_disable(unsigned int irq)
{
    ar7240_reg_rmw_clear(AR7240_GPIO_INT_MASK, 
                        (1 << (irq - AR7240_GPIO_IRQ_BASE)));
}

static unsigned int ar7240_gpio_intr_startup(unsigned int irq)
{
    /*--- ar7240_reg_rmw_set(AR7240_GPIO_INT_PENDING, (1 << (irq - AR7240_GPIO_IRQ_BASE))); ---*/
    ar7240_reg_rmw_set(AR7240_GPIO_INT_ENABLE, (1 << (irq - AR7240_GPIO_IRQ_BASE)));
	ar7240_gpio_intr_enable(irq);
	return 0;
}

static void ar7240_gpio_intr_shutdown(unsigned int irq)
{
	ar7240_gpio_intr_disable(irq);
}

static void ar7240_gpio_intr_ack(unsigned int irq)
{
    unsigned int gpio_bit = (1 << (irq - AR7240_GPIO_IRQ_BASE));

    if(unlikely(ar7240_gpio_int_both_edge & gpio_bit)) {
    	ar7240_reg_wr(AR7240_GPIO_INT_POLARITY, ar7240_reg_rd(AR7240_GPIO_INT_POLARITY) ^ gpio_bit);
	    ar7240_reg_rd(AR7240_GPIO_INT_POLARITY);
    }
}

static void ar7240_gpio_intr_end(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
	    ar7240_gpio_intr_enable(irq);
}

static int ar7240_gpio_intr_set_affinity(unsigned int irq, const struct cpumask *mask)
{
	/* 
     * Only 1 CPU; ignore affinity request
     */
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ar7240_gpio_intr_set_type(unsigned int irq, unsigned int flow_type)
{
    unsigned int gpio_bit;
    struct irq_desc *desc = &irq_desc[irq];

    gpio_bit = 1 << (irq - AR7240_GPIO_IRQ_BASE);
    /*--- printk(KERN_ERR "[%s]: irq %d trigger type %d\n", __func__, irq, flow_type); ---*/

    switch (flow_type) {
        case IRQF_TRIGGER_NONE:
            printk(KERN_ERR "[%s %d]: Assuming IRQ%d level triggered are already configured!\n", __func__, __LINE__, irq);
            ar7240_gpio_int_both_edge &= ~gpio_bit;
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_level_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_RISING:
            /* edge triggered & active high */
            ar7240_gpio_int_both_edge &= ~gpio_bit;
            ar7240_reg_rmw_clear(AR7240_GPIO_INT_TYPE, gpio_bit);
            ar7240_reg_rmw_set (AR7240_GPIO_INT_POLARITY, gpio_bit);
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_edge_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_FALLING:
            /* edge triggered & active low */
            ar7240_gpio_int_both_edge &= ~gpio_bit;
            ar7240_reg_rmw_clear(AR7240_GPIO_INT_TYPE, gpio_bit);
            ar7240_reg_rmw_clear(AR7240_GPIO_INT_POLARITY, gpio_bit);
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_edge_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_HIGH:
            /* both edge triggered & active high (emuliert, initial wie IRQF_TRIGGER_RISING) */
            ar7240_gpio_int_both_edge |= gpio_bit;
            printk(KERN_ERR "[%s] gpio_bit=9x%x both_edge=0x%x", __FUNCTION__, gpio_bit, ar7240_gpio_int_both_edge);
            ar7240_reg_rmw_clear(AR7240_GPIO_INT_TYPE, gpio_bit);
            ar7240_reg_rmw_set (AR7240_GPIO_INT_POLARITY, gpio_bit);
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_edge_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_LOW:
            /* both edge triggered & active high (emuliert, initial wie IRQF_TRIGGER_FALLING) */
            ar7240_gpio_int_both_edge |= gpio_bit;
            ar7240_reg_rmw_clear(AR7240_GPIO_INT_TYPE, gpio_bit);
            ar7240_reg_rmw_clear (AR7240_GPIO_INT_POLARITY, gpio_bit);
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_edge_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_HIGH:
            /* level sensitive & active high */
            ar7240_gpio_int_both_edge &= ~gpio_bit;
            ar7240_reg_rmw_set(AR7240_GPIO_INT_TYPE, gpio_bit);
            ar7240_reg_rmw_set (AR7240_GPIO_INT_POLARITY, gpio_bit);
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_level_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_LOW:
            /* level sensitive & active low */
            ar7240_gpio_int_both_edge &= ~gpio_bit;
            ar7240_reg_rmw_set(AR7240_GPIO_INT_TYPE, gpio_bit);
            ar7240_reg_rmw_clear(AR7240_GPIO_INT_POLARITY, gpio_bit);
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_level_irq);
	        spin_lock(&desc->lock);
            break;
        default:
            printk(KERN_ERR "[%s %d]: Invalid irq %d trigger type %d\n", __func__, __LINE__, irq, flow_type);
            break;
    }

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct irq_chip /* hw_interrupt_type */ ar7240_gpio_intr_controller = {
	.name		    = "AR7240 GPIO",
	.set_type       = ar7240_gpio_intr_set_type,
	.startup	    = ar7240_gpio_intr_startup,
	.shutdown	    = ar7240_gpio_intr_shutdown,
	.enable		    = ar7240_gpio_intr_enable,
	.disable	    = ar7240_gpio_intr_disable,
    .mask           = ar7240_gpio_intr_mask,
    .unmask         = ar7240_gpio_intr_unmask,
    .ack            = ar7240_gpio_intr_ack,
	.end		    = ar7240_gpio_intr_end,
	.eoi		    = ar7240_gpio_intr_end,
	.set_affinity   = ar7240_gpio_intr_set_affinity,
};

void ar7240_gpio_irq_init(int irq_base)
{
	int i;
    ar7240_gpio_int_both_edge = 0;
	for (i = irq_base; i < irq_base + AR7240_GPIO_IRQ_COUNT; i++) {
		irq_desc[i].status  = IRQ_DISABLED;
		irq_desc[i].action  = NULL;
		irq_desc[i].depth   = 1;
		//irq_desc[i].chip = &ar7240_gpio_intr_controller;
		set_irq_chip_and_handler(i, &ar7240_gpio_intr_controller,
					 handle_level_irq);
	}
    printk(KERN_ERR "[%s] done\n", __FUNCTION__);
}



/*------------------------------------------------------------------------------------------*\
 * AVM GPIO API
\*------------------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7240_avm_gpio_dump_registers(void) {
    printk( KERN_ERR "[%s] GPIO register dump:\n", __FUNCTION__);
    printk( KERN_ERR "    AR7240_GPIO_OE           = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_OE));
    printk( KERN_ERR "    AR7240_GPIO_IN           = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_IN));
    printk( KERN_ERR "    AR7240_GPIO_OUT          = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_OUT));
    printk( KERN_ERR "    AR7240_GPIO_SET          = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_SET));
    printk( KERN_ERR "    AR7240_GPIO_CLEAR        = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_CLEAR));
    printk( KERN_ERR "    AR7240_GPIO_INT_ENABLE   = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_INT_ENABLE));
    printk( KERN_ERR "    AR7240_GPIO_INT_TYPE     = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_INT_TYPE));
    printk( KERN_ERR "    AR7240_GPIO_INT_POLARITY = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_INT_POLARITY));
    printk( KERN_ERR "    AR7240_GPIO_INT_PENDING  = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_INT_PENDING));
    printk( KERN_ERR "    AR7240_GPIO_INT_MASK     = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_INT_MASK));
    printk( KERN_ERR "    AR7240_GPIO_FUNCTIONS    = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_FUNCTIONS));
#ifndef CONFIG_WASP_SUPPORT
    printk( KERN_ERR "    AR7240_GPIO_FUNCTION_2   = 0x%x\n", ar7240_reg_rd(AR7240_GPIO_FUNCTION_2));
#endif /*--- #ifndef CONFIG_WASP_SUPPORT ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ar7240_gpio_setup_pin_mode(unsigned int gpio_pin, enum _hw_gpio_function pin_mode ){
	unsigned int function_register = 0;
	unsigned int function_register2 = 0;
	unsigned int bits = 0;
	unsigned int bits2 = 0;
	switch (gpio_pin) {
		case 0:
			/* SPI Chip select on GPIO_0 [Bit 13] */
			bits |= AR7240_GPIO_FUNCTION_SPI_CS_0_EN; 
			function_register = AR7240_GPIO_FUNCTIONS;
			/* Enables I2S_WS on GPIO_0 [FUNC_2 Bit 4] */
			bits2 |= AR7240_GPIO_FUNCTION_2_EN_I2WS_ON_0;
			function_register2 = AR7240_GPIO_FUNCTIONS;
			break;
		case 1:
			/* SPI Chip select on GPIO_1 [Bit 14] */
			bits |= AR7240_GPIO_FUNCTION_SPI_CS_1_EN;
			function_register = AR7240_GPIO_FUNCTIONS;
			/* Enables I2S_CK Out on GPIO_1 [FUNC_2 Bit 3] */
			bits2 |= AR7240_GPIO_FUNCTION_2_EN_I2SCK_ON_1;
			function_register2 = AR7240_GPIO_FUNCTIONS;
			break;
		case 2:
		case 3:
		case 4:
		case 5:
			/* SPI Chip select on GPIO_2 - GPIO_5 [Bit 18] */
			bits |= AR7240_GPIO_FUNCTION_SPI_EN;
			function_register = AR7240_GPIO_FUNCTIONS;
			break;
		case 6:
		case 7:
		case 8:
		/*--- case 11: ---*/
		/*--- case 12: ---*/
			/* I2S on GPIO_6-8 and 11-12 [Bit 26] */
			bits |= AR7240_GPIO_FUNCTION_I2S0_EN;
			function_register = AR7240_GPIO_FUNCTIONS;
			break;
		case 9:
		case 10:
			/* Enables UART on GPIO_11 and GPIO_12 [Bit 1] */
			bits |= AR7240_GPIO_FUNCTION_UART_EN;
			function_register = AR7240_GPIO_FUNCTIONS;
			break;
		case 11:
            if( pin_mode == GPIO_PIN ) {
			    /* Master Audio CLK_MCK on GPIO_11 (only if Bit 26 is set) [Bit 27] - clear only! */
			    bits |= AR7240_GPIO_FUNCTION_I2S_MCKEN; 
			    /* Disable undocumented Function [Bit 20] (otherwise GPIO mode won't work) */
			    bits |= AR7240_GPIO_FUNCTION_CLK_OBS6_ENABLE;
            }
		case 12:
			/* Enables UART RTS/CTS on GPIO_11 and GPIO_12 [Bit 2] - default */
			bits |= AR7240_GPIO_FUNCTION_UART_RTS_CTS_EN;
			/* I2S on GPIO_6-8 and 11-12 [Bit 26] - clear only! */
            if( pin_mode == GPIO_PIN )
			    bits |= AR7240_GPIO_FUNCTION_I2S0_EN; 
			function_register = AR7240_GPIO_FUNCTIONS;
            if(gpio_pin == 12) {
                /* Enables I2S_SD on GPIO_12 [FUNC_2 Bit 5] */
                bits2 |= AR7240_GPIO_FUNCTION_2_EN_I2WS_ON_0;
                function_register2 = AR7240_GPIO_FUNCTIONS;
            }
			break;
		case 13:
			/* Enables Ethernet LED on GPIO_13 [Bit 13] */
			bits |= AR7240_GPIO_FUNCTION_ETH_SWITCH_LED0_EN;
            if( pin_mode == GPIO_PIN )
			    /* Disable undocumented Function [Bit 8] (otherwise GPIO mode won't work) */
			    bits |= AR7240_GPIO_FUNCTION_CLK_OBS1_ENABLE;
            if( pin_mode == GPIO_PIN )
			    /* Disable TCK as SPDIF Serial out on GPIO_13 [Bit 30] - clear only! */
			    bits |= AR7240_GPIO_FUNCTION_SPDIF_EN; 
			function_register = AR7240_GPIO_FUNCTIONS;
			break;
#ifndef CONFIG_WASP_SUPPORT
		case 14:
		case 15:
		case 16:
			/* Enables I2C LED on GPIO_14 - 16 [FUNC_2 Bit 1] */
			bits2 |= AR7240_GPIO_FUNCTION_2_I2S_ON_LED;
            function_register2 = AR7240_GPIO_FUNCTION_2;
			break;
#endif /*--- #ifndef CONFIG_WASP_SUPPORT ---*/
		default:
			return -1;
	}
	if ( pin_mode == GPIO_PIN ) {
        if(function_register)
    		ar7240_reg_rmw_clear(function_register, bits);
        if(function_register2)
		    ar7240_reg_rmw_clear(function_register2, bits2);
	} else {
        if(function_register)
    		ar7240_reg_rmw_set(function_register, bits);
        if(function_register2)
		    ar7240_reg_rmw_set(function_register2, bits2);
	}

	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ar7240_avm_gpio_ctrl(unsigned int gpio_pin, enum _hw_gpio_function pin_mode, enum _hw_gpio_direction pin_dir)
{
    printk("[%s] gpio=%u as %s direction=%s\n", __FUNCTION__, gpio_pin, 
            pin_mode == GPIO_PIN       ? "gpio"  : "function",
            pin_dir  == GPIO_INPUT_PIN ? "input" : "output");

	ar7240_gpio_setup_pin_mode(gpio_pin, pin_mode);

    if(pin_mode == GPIO_PIN) {
        if (pin_dir == GPIO_INPUT_PIN)
			ar7240_gpio_config_input( gpio_pin );
        else
			ar7240_gpio_config_output( gpio_pin );
    } 
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ar7240_avm_gpio_out_bit(unsigned int gpio_pin, int value)
{	
	if (gpio_pin > AR7240_GPIO_IRQ_COUNT) {
		return - 1;
	} 

	ar7240_gpio_out_val(gpio_pin, value);
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ar7240_avm_gpio_in_bit(unsigned int gpio_pin){
	/* wird von sammel-treiber verwendet */
	return ar7240_gpio_in_val( gpio_pin ) ? 1 : 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ar7240_avm_gpio_in_value(void){
    return ar7240_reg_rd(AR7240_GPIO_IN);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ar7240_avm_gpio_set_bitmask(unsigned int mask, unsigned int value){
	/* wird von sammel-treiber verwendet */
	ar7240_reg_rmw_clear(AR7240_GPIO_OUT, mask);
	ar7240_reg_rmw_set(AR7240_GPIO_OUT, (value & mask)); 
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ar7240_avm_gpio_init(void){
    printk("[%s]\n", __FUNCTION__);
    /*--- ar7240_avm_gpio_dump_registers(); ---*/
    ar7240_avm_gpio_first_handle = NULL;

    return 0;
}
arch_initcall(ar7240_avm_gpio_init);


EXPORT_SYMBOL(ar7240_avm_gpio_init);
EXPORT_SYMBOL(ar7240_avm_gpio_ctrl);
EXPORT_SYMBOL(ar7240_avm_gpio_out_bit);
EXPORT_SYMBOL(ar7240_avm_gpio_in_bit);
EXPORT_SYMBOL(ar7240_avm_gpio_in_value);
EXPORT_SYMBOL(ar7240_avm_gpio_set_bitmask);

