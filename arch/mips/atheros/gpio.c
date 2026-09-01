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
#include <asm/prom.h>
#include <linux/slab.h>
#include <linux/avm_hw_config.h>

#define GPIO_FUNCTION_TABLE
#include <atheros_gpio.h>

#include <atheros.h>



struct _hw_gpio_irqhandle *ath_avm_gpio_first_handle;
#if defined(CONFIG_MACH_AR934x) || defined(CONFIG_MACH_QCA955x)
static struct _gpio_function *current_gpio_table;
static unsigned int current_gpio_table_size;
#endif
unsigned int ath_gpio_int_both_edge;
unsigned int ath_gpio_int_active_low;
unsigned int ath_gpio_shiftreg_size = 0;
unsigned int ath_gpio_shiftreg_clk_pin = -1;
unsigned int ath_gpio_shiftreg_din_pin = -1;

void avmnet_gpio(unsigned int gpio, unsigned int on) __attribute__ ((weak));

void ath_avm_gpio_dump_registers(const char *prefix);

/*------------------------------------------------------------------------------------------*\
 * LOW LEVEL GPIO ACCESS FUNCTIONS
\*------------------------------------------------------------------------------------------*/


void ath_gpio_config_output(int gpio) {
    if(is_ar934x() || is_qca955x())
        ath_reg_rmw_clear(ATH_GPIO_OE, (1 << gpio));
    else
        ath_reg_rmw_set(ATH_GPIO_OE, (1 << gpio));
}

void ath_gpio_config_input(int gpio)
{
    if(is_ar934x() || is_qca955x())
        ath_reg_rmw_set(ATH_GPIO_OE, (1 << gpio));
    else
        ath_reg_rmw_clear(ATH_GPIO_OE, (1 << gpio));
}

#define gpio_ath_reg_wr(_phys, _val)  ((*(volatile ath_reg_t *)KSEG1ADDR(_phys)) = (_val))

static inline void ath_gpio_out_val(int gpio, int val) {
    if (val) {
        gpio_ath_reg_wr(ATH_GPIO_SET, (1 << gpio));
	} else {
        gpio_ath_reg_wr(ATH_GPIO_CLEAR, (1 << gpio));
    }
    wmb();
}

static inline int ath_gpio_in_val(int gpio) {
#if 0
    printk(KERN_ERR "[%s] \n\tgpio_val  =0x%08x\n"
                           "\tactive_low=0x%08x\n"
                           "\tboth_edge =0x%08x\n"
                           "\treturn_val=0x%08x (active %s)\n", __FUNCTION__,
            (1 << gpio) & ath_reg_rd(ATH_GPIO_IN), 
            ath_gpio_int_active_low,
            ath_gpio_int_both_edge,
            ((1 << gpio) & ~ath_gpio_int_active_low & ath_gpio_int_both_edge)      ?
                ((1 << gpio) & ath_reg_rd(ATH_GPIO_IN))                            :
                ((1 << gpio) & (ath_reg_rd(ATH_GPIO_IN) ^ ath_gpio_int_both_edge)),
            (ath_reg_rd(ATH_GPIO_INT_POLARITY) & (1 << gpio)) ? "high" : "low");
#endif
#ifdef CONFIG_MACH_QCA955x
    /*--- Bei Both-Edges-Interrupts den Active-High Wert bei Scorpion nicht invertieren ---*/
    if((1 << gpio) & ~ath_gpio_int_active_low & ath_gpio_int_both_edge) {
        return (1 << gpio) & ath_reg_rd(ATH_GPIO_IN);
    }
#endif
    return (1 << gpio) & (ath_reg_rd(ATH_GPIO_IN) ^ ath_gpio_int_both_edge);
}

static void ath_gpio_intr_enable(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
    /*--- printk(KERN_ERR "[%s] status=0x%x\n", __FUNCTION__, desc->status); ---*/
    ath_reg_rmw_set(ATH_GPIO_INT_MASK, (1 << (irq - ATH_GPIO_IRQ_BASE)));
	desc->status &= ~IRQ_MASKED;
}

static void ath_gpio_intr_unmask(unsigned int irq)
{
    ath_reg_rmw_set(ATH_GPIO_INT_MASK, (1 << (irq - ATH_GPIO_IRQ_BASE)));
}

static void ath_gpio_intr_mask(unsigned int irq)
{
    ath_reg_rmw_clear(ATH_GPIO_INT_MASK, (1 << (irq - ATH_GPIO_IRQ_BASE)));
}

static void ath_gpio_intr_disable(unsigned int irq)
{
    ath_reg_rmw_clear(ATH_GPIO_INT_MASK, (1 << (irq - ATH_GPIO_IRQ_BASE)));
}

static unsigned int ath_gpio_intr_startup(unsigned int irq)
{
    /*--- ath_reg_rmw_set(ATH_GPIO_INT_PENDING, (1 << (irq - ATH_GPIO_IRQ_BASE))); ---*/
    ath_reg_rmw_set(ATH_GPIO_INT_ENABLE, (1 << (irq - ATH_GPIO_IRQ_BASE)));
	ath_gpio_intr_enable(irq);
	return 0;
}

static void ath_gpio_intr_shutdown(unsigned int irq)
{
	ath_gpio_intr_disable(irq);
}

static void ath_gpio_intr_ack(unsigned int irq)
{
    unsigned int gpio_bit = (1 << (irq - ATH_GPIO_IRQ_BASE));

    if(unlikely(ath_gpio_int_both_edge & gpio_bit)) {
    	ath_reg_wr(ATH_GPIO_INT_POLARITY, ath_reg_rd(ATH_GPIO_INT_POLARITY) ^ gpio_bit);
	    ath_reg_rd(ATH_GPIO_INT_POLARITY);
    }
}

static void ath_gpio_intr_end(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
	    ath_gpio_intr_enable(irq);
}

static int ath_gpio_intr_set_affinity(unsigned int irq, const struct cpumask *mask)
{
	/* 
     * Only 1 CPU; ignore affinity request
     */
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ath_gpio_intr_set_type(unsigned int irq, unsigned int flow_type)
{
    unsigned int gpio_bit;
    struct irq_desc *desc = &irq_desc[irq];

    gpio_bit = 1 << (irq - ATH_GPIO_IRQ_BASE);
    /*--- printk(KERN_ERR "[%s]: irq %d trigger type %d\n", __func__, irq, flow_type); ---*/

    switch (flow_type) {
        case IRQF_TRIGGER_NONE:
            printk(KERN_ERR "[%s %d]: Assuming IRQ%d level triggered are already configured!\n", __func__, __LINE__, irq);
            ath_gpio_int_both_edge &= ~gpio_bit;
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_level_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_RISING:
            /* edge triggered & active high */
            ath_gpio_int_both_edge &= ~gpio_bit;
            ath_reg_rmw_clear(ATH_GPIO_INT_TYPE, gpio_bit);
            ath_reg_rmw_set (ATH_GPIO_INT_POLARITY, gpio_bit);
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_edge_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_FALLING:
            /* edge triggered & active low */
            ath_gpio_int_both_edge &= ~gpio_bit;
            ath_reg_rmw_clear(ATH_GPIO_INT_TYPE, gpio_bit);
            ath_reg_rmw_clear(ATH_GPIO_INT_POLARITY, gpio_bit);
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_edge_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_HIGH:
            /* both edge triggered & active high (emuliert, initial wie IRQF_TRIGGER_RISING) */
            ath_gpio_int_both_edge |= gpio_bit;
            ath_gpio_int_active_low &= ~gpio_bit;
            /*--- printk(KERN_ERR "[%s] IRQF_TRIGGER_HIGH gpio_bit=0x%x both_edge=0x%x", __FUNCTION__, gpio_bit, ath_gpio_int_both_edge); ---*/
            ath_reg_rmw_clear(ATH_GPIO_INT_TYPE, gpio_bit);
            ath_reg_rmw_set (ATH_GPIO_INT_POLARITY, gpio_bit);
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_edge_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_LOW:
            /* both edge triggered & active high (emuliert, initial wie IRQF_TRIGGER_FALLING) */
            ath_gpio_int_both_edge |= gpio_bit;
            ath_gpio_int_active_low |= gpio_bit;
            /*--- printk(KERN_ERR "[%s] IRQF_TRIGGER_LOW gpio_bit=0x%x both_edge=0x%x", __FUNCTION__, gpio_bit, ath_gpio_int_both_edge); ---*/
            ath_reg_rmw_clear(ATH_GPIO_INT_TYPE, gpio_bit);
            ath_reg_rmw_clear (ATH_GPIO_INT_POLARITY, gpio_bit);
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_edge_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_HIGH:
            /* level sensitive & active high */
            ath_gpio_int_both_edge &= ~gpio_bit;
            ath_gpio_int_active_low &= ~gpio_bit;
            ath_reg_rmw_set(ATH_GPIO_INT_TYPE, gpio_bit);
            ath_reg_rmw_set (ATH_GPIO_INT_POLARITY, gpio_bit);
	        spin_unlock(&desc->lock);  /*--- wir dürfen den Lock nicht behalten sonst haben wir einen deadlock ---*/
            set_irq_handler(irq, handle_level_irq);
	        spin_lock(&desc->lock);
            break;
        case IRQF_TRIGGER_LOW:
            /* level sensitive & active low */
            ath_gpio_int_both_edge &= ~gpio_bit;
            ath_gpio_int_active_low |= gpio_bit;
            ath_reg_rmw_set(ATH_GPIO_INT_TYPE, gpio_bit);
            ath_reg_rmw_clear(ATH_GPIO_INT_POLARITY, gpio_bit);
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
struct irq_chip /* hw_interrupt_type */ ath_gpio_intr_controller = {
	.name		    = "AR7240 GPIO",
	.set_type       = ath_gpio_intr_set_type,
	.startup	    = ath_gpio_intr_startup,
	.shutdown	    = ath_gpio_intr_shutdown,
	.enable		    = ath_gpio_intr_enable,
	.disable	    = ath_gpio_intr_disable,
    .mask           = ath_gpio_intr_mask,
    .unmask         = ath_gpio_intr_unmask,
    .ack            = ath_gpio_intr_ack,
	.end		    = ath_gpio_intr_end,
	.eoi		    = ath_gpio_intr_end,
	.set_affinity   = ath_gpio_intr_set_affinity,
};

void ath_gpio_irq_init(int irq_base)
{
	int i;
    ath_gpio_int_both_edge = 0;
    ath_gpio_int_active_low = 0;
	for (i = irq_base; i < irq_base + ATH_GPIO_IRQ_COUNT; i++) {
		irq_desc[i].status  = IRQ_DISABLED;
		irq_desc[i].action  = NULL;
		irq_desc[i].depth   = 1;
		//irq_desc[i].chip = &ath_gpio_intr_controller;
		set_irq_chip_and_handler(i, &ath_gpio_intr_controller,
					 handle_level_irq);
	}
    printk(KERN_ERR "[%s] done\n", __FUNCTION__);
}



/*------------------------------------------------------------------------------------------*\
 * AVM GPIO API
\*------------------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ath_avm_gpio_dump_registers(const char *prefix) {
    printk( KERN_ERR "[%s] GPIO register dump:\n", prefix ? prefix : "");
    printk( KERN_ERR "    ATH_GPIO_OE(%08x)               = 0x%08x\n", KSEG1ADDR(ATH_GPIO_OE),  ath_reg_rd(ATH_GPIO_OE));
    printk( KERN_ERR "    ATH_GPIO_IN(%08x)               = 0x%08x\n", KSEG1ADDR(ATH_GPIO_IN),  ath_reg_rd(ATH_GPIO_IN));
    printk( KERN_ERR "    ATH_GPIO_OUT(%08x)              = 0x%08x\n", KSEG1ADDR(ATH_GPIO_OUT), ath_reg_rd(ATH_GPIO_OUT));
    printk( KERN_ERR "    ATH_GPIO_INT_ENABLE(%08x)       = 0x%08x\n", KSEG1ADDR(ATH_GPIO_INT_ENABLE), ath_reg_rd(ATH_GPIO_INT_ENABLE));
    printk( KERN_ERR "    ATH_GPIO_INT_TYPE(%08x)         = 0x%08x\n", KSEG1ADDR(ATH_GPIO_INT_TYPE), ath_reg_rd(ATH_GPIO_INT_TYPE));
    printk( KERN_ERR "    ATH_GPIO_INT_POLARITY(%08x)     = 0x%08x\n", KSEG1ADDR(ATH_GPIO_INT_POLARITY), ath_reg_rd(ATH_GPIO_INT_POLARITY));
    printk( KERN_ERR "    ATH_GPIO_INT_PENDING(%08x)      = 0x%08x\n", KSEG1ADDR(ATH_GPIO_INT_PENDING), ath_reg_rd(ATH_GPIO_INT_PENDING));
    printk( KERN_ERR "    ATH_GPIO_INT_MASK(%08x)         = 0x%08x\n", KSEG1ADDR(ATH_GPIO_INT_MASK), ath_reg_rd(ATH_GPIO_INT_MASK));
#ifdef CONFIG_MACH_AR724x
    printk( KERN_ERR "    ATH_GPIO_FUNCTIONS(%08x)        = 0x%08x\n", KSEG1ADDR(ATH_GPIO_FUNCTIONS), ath_reg_rd(ATH_GPIO_FUNCTIONS));
    printk( KERN_ERR "    ATH_GPIO_FUNCTION_2             = 0x%08x\n", ath_reg_rd(ATH_GPIO_FUNCTION_2));
#endif /*--- #ifdef CONFIG_MACH_AR724x ---*/
#if defined(CONFIG_MACH_AR934x) || defined(CONFIG_MACH_QCA955x)
    printk( KERN_ERR "    ATH_GPIO_IN_ETH_SWITCH_LED(%08x)= 0x%08x\n",              KSEG1ADDR(ATH_GPIO_IN_ETH_SWITCH_LED), ath_reg_rd(ATH_GPIO_IN_ETH_SWITCH_LED));
    printk( KERN_ERR "    ATH_GPIO_OUT_FUNCTION0(%08x)    = 0x%08x (gpio 0-3)\n",   KSEG1ADDR(ATH_GPIO_OUT_FUNCTION0), ath_reg_rd(ATH_GPIO_OUT_FUNCTION0));
    printk( KERN_ERR "    ATH_GPIO_OUT_FUNCTION1(%08x)    = 0x%08x (gpio 4-7)\n",   KSEG1ADDR(ATH_GPIO_OUT_FUNCTION1), ath_reg_rd(ATH_GPIO_OUT_FUNCTION1));
    printk( KERN_ERR "    ATH_GPIO_OUT_FUNCTION2(%08x)    = 0x%08x (gpio 8-11)\n",  KSEG1ADDR(ATH_GPIO_OUT_FUNCTION2), ath_reg_rd(ATH_GPIO_OUT_FUNCTION2));
    printk( KERN_ERR "    ATH_GPIO_OUT_FUNCTION3(%08x)    = 0x%08x (gpio 12-15)\n", KSEG1ADDR(ATH_GPIO_OUT_FUNCTION3), ath_reg_rd(ATH_GPIO_OUT_FUNCTION3));
    printk( KERN_ERR "    ATH_GPIO_OUT_FUNCTION4(%08x)    = 0x%08x (gpio 16-19)\n", KSEG1ADDR(ATH_GPIO_OUT_FUNCTION4), ath_reg_rd(ATH_GPIO_OUT_FUNCTION4));
    printk( KERN_ERR "    ATH_GPIO_OUT_FUNCTION5(%08x)    = 0x%08x (gpio 20-23)\n", KSEG1ADDR(ATH_GPIO_OUT_FUNCTION5), ath_reg_rd(ATH_GPIO_OUT_FUNCTION5));
    printk( KERN_ERR "    ATH_GPIO_IN_ENABLE0(%08x)       = 0x%08x (UART0/SPI)\n",  KSEG1ADDR(ATH_GPIO_IN_ENABLE0), ath_reg_rd(ATH_GPIO_IN_ENABLE0));
    printk( KERN_ERR "    ATH_GPIO_IN_ENABLE1(%08x)       = 0x%08x (I2S)\n",        KSEG1ADDR(ATH_GPIO_IN_ENABLE1), ath_reg_rd(ATH_GPIO_IN_ENABLE1));
    printk( KERN_ERR "    ATH_GPIO_IN_ENABLE2(%08x)       = 0x%08x (ETHRX)\n",      KSEG1ADDR(ATH_GPIO_IN_ENABLE2), ath_reg_rd(ATH_GPIO_IN_ENABLE2));
    printk( KERN_ERR "    ATH_GPIO_IN_ENABLE3(%08x)       = 0x%08x (MDIO)\n",       KSEG1ADDR(ATH_GPIO_IN_ENABLE3), ath_reg_rd(ATH_GPIO_IN_ENABLE3));
    printk( KERN_ERR "    ATH_GPIO_IN_ENABLE4(%08x)       = 0x%08x (SLIC)\n",       KSEG1ADDR(ATH_GPIO_IN_ENABLE4), ath_reg_rd(ATH_GPIO_IN_ENABLE4));
    printk( KERN_ERR "    ATH_GPIO_IN_ENABLE5(%08x)       = 0x%08x (I2C)\n",        KSEG1ADDR(ATH_GPIO_IN_ENABLE5), ath_reg_rd(ATH_GPIO_IN_ENABLE5));
    printk( KERN_ERR "    ATH_GPIO_IN_ENABLE6(%08x)       = 0x%08x (?)\n",          KSEG1ADDR(ATH_GPIO_IN_ENABLE6), ath_reg_rd(ATH_GPIO_IN_ENABLE6));
    printk( KERN_ERR "    ATH_GPIO_IN_ENABLE7(%08x)       = 0x%08x (?)\n",          KSEG1ADDR(ATH_GPIO_IN_ENABLE7), ath_reg_rd(ATH_GPIO_IN_ENABLE7));
    printk( KERN_ERR "    ATH_GPIO_IN_ENABLE8(%08x)       = 0x%08x (?)\n",          KSEG1ADDR(ATH_GPIO_IN_ENABLE8), ath_reg_rd(ATH_GPIO_IN_ENABLE8));
    printk( KERN_ERR "    ATH_GPIO_IN_ENABLE9(%08x)       = 0x%08x (UART1)\n",      KSEG1ADDR(ATH_GPIO_IN_ENABLE9), ath_reg_rd(ATH_GPIO_IN_ENABLE9));
    printk( KERN_ERR "    ATH_GPIO_FUNCTIONS(%08x)        = 0x%08x\n",              KSEG1ADDR(ATH_GPIO_FUNCTIONS), ath_reg_rd(ATH_GPIO_FUNCTIONS));
#endif/*--- #ifdef CONFIG_MACH_AR934x ---*/
}
EXPORT_SYMBOL(ath_avm_gpio_dump_registers);

#if defined(CONFIG_MACH_AR934x) || defined(CONFIG_MACH_QCA955x)
/*--------------------------------------------------------------------------------*\
 * mbahr: Doppelbelegung erlaubt ?
 * was sind ATH_GPIO_IN_ENABLE5 - ATH_GPIO_IN_ENABLE8 ?? 
\*--------------------------------------------------------------------------------*/
static void ath_infunction_clear(unsigned int gpio_pin) {
    unsigned int inaddr, i;
    for(inaddr = ATH_GPIO_IN_ENABLE0; inaddr <= ATH_GPIO_IN_ENABLE9; inaddr += sizeof(unsigned int)) {
        unsigned int funcin_val;
        if((inaddr >= ATH_GPIO_IN_ENABLE4) && (inaddr != ATH_GPIO_IN_ENABLE9)) {
            /*--- undokumentiert ?  ---*/
            continue;
        }
        funcin_val  = ath_reg_rd(inaddr);
        for(i = 0; i < 32; i += 8) {
            if(((funcin_val >> i) & 0xFF) == gpio_pin) {
                /*--- printk(KERN_ERR "warning[%d]: %08x %08x contain gpio_pin %d\n", i, inaddr, funcin_val, gpio_pin); ---*/
                funcin_val = (funcin_val & ~(0xFF << i));
                ath_reg_wr(inaddr, funcin_val);
            }
        }
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __gpio_infunction_set(unsigned int gpio_pin, int func) 
{
    unsigned int val;
#if 0
    if(((func == GPIO_IN_SPI_MISO) || (func == GPIO_IN_UART0_SIN)) && (gpio_pin > 17)) {
        printk("[%s] error: invalid GPIO %d for input function %d (only gpio pin < 18 allowed)\n",__FUNCTION__, gpio_pin, func);
        return;
    }
#endif
    ath_infunction_clear(gpio_pin);
    if(func == NO_FUNCTION) {
        /*--- printk("[%s] Clear input function register for gpio %d\n", __FUNCTION__, gpio_pin); ---*/
        return;
    }
    val = ath_reg_rd(GPIO_IN_ENABLE_REG(func));
    val = GPIO_IN_ENABLE_SET(val, func, gpio_pin);
    ath_reg_wr(GPIO_IN_ENABLE_REG(func), val);
    /*--- printk("[%s] Set input function register for gpio %u to func %d (reg=0x%x, val=0x%x)\n", __FUNCTION__, ---*/ 
            /*--- gpio_pin, func, GPIO_IN_ENABLE_REG(func), val); ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __gpio_outfunction_set(unsigned int gpio_pin, int func) 
{
    unsigned int val;
    if(func == NO_FUNCTION)
        func = 0;
    val = ath_reg_rd(GPIO_OUT_FUNC_REG(gpio_pin));
    val = GPIO_OUT_FUNC_SET(val, func, gpio_pin);
    ath_reg_wr(GPIO_OUT_FUNC_REG(gpio_pin), val);
    /*--- printk("[%s] Set output function register for gpio %u to func %d (reg=0x%x, val=0x%x)\n", __FUNCTION__, ---*/ 
           /*--- gpio_pin, func, GPIO_OUT_FUNC_REG(gpio_pin), val); ---*/
}

static inline void ath_avm_gpio_infunction_set(unsigned int gpio_pin, int func) 
{
    __gpio_outfunction_set(gpio_pin, NO_FUNCTION);
    __gpio_infunction_set(gpio_pin, func);
}
static inline void ath_avm_gpio_outfunction_set(unsigned int gpio_pin, int func) 
{
    __gpio_infunction_set(gpio_pin, NO_FUNCTION);
    __gpio_outfunction_set(gpio_pin, func);
}
static inline void ath_avm_gpio_inoutfunction_set(unsigned int gpio_pin, int func) 
{
    __gpio_infunction_set(gpio_pin, func & 0xFFFF);
    __gpio_outfunction_set(gpio_pin, func >> 16);
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ath_avm_gpio_init_functions(void)
{
    unsigned int hwrev = 0, pin, i;
    char *s, *p;

    s = prom_getenv("HWRevision");
    if (s) {
		hwrev = simple_strtoul(s, &p, 10);
	} else {
	  printk("[%s] error: no HWRevision detected in environment variables\n",__FUNCTION__);
	  BUG_ON(1);
	}

    for(i = 0; i < sizeof(gpio_func_tables)/sizeof(struct _gpio_func_table); i++) {
        if(gpio_func_tables[i].hwrev == hwrev) {
            current_gpio_table      = gpio_func_tables[i].table;
            current_gpio_table_size = gpio_func_tables[i].size; 
            break;
        }

    }
    if(i == sizeof(gpio_func_tables)/sizeof(struct _gpio_func_table)) {
        /*--- Kein Tabelleneintrag für HWRevsion ---*/
        printk(KERN_ERR "[%s]<ERROR: KEIN EINTRAG gefunden>\n", __func__);
        return;
    }
#if 0
    for(inaddr = ATH_GPIO_IN_ENABLE0; inaddr <= ATH_GPIO_IN_ENABLE9; inaddr += sizeof(unsigned int)) {
        if((inaddr >= ATH_GPIO_IN_ENABLE4) && (inaddr != ATH_GPIO_IN_ENABLE9)) {
            /*--- undokumentiert ?  ---*/
            continue;
        }
        printk("[%s] ATH_GPIO_IN_ENABLE: 0x%x => 0x%x\n", __FUNCTION__, inaddr, ath_reg_rd(inaddr));
        ath_reg_wr(inaddr, 0);
    }
	printk("[%s] Setting gpio functions for HWRevision %d\n", __FUNCTION__, hwrev);
#endif
    for(pin = 0; pin < current_gpio_table_size; pin++) {
        if(current_gpio_table[pin].func == IGNORE_FUNCTION)
            continue;

        if(pin < 4)
            ath_reg_rmw_set(ATH_GPIO_FUNCTIONS, ATH_GPIO_FUNCTION_JTAG_DISABLE);

        switch (current_gpio_table[pin].dir) {
            case GPIO_INPUT_PIN:
                ath_avm_gpio_infunction_set(pin, current_gpio_table[pin].func);
                ath_gpio_config_input(pin);
                break;
            case GPIO_OUTPUT_PIN:
                ath_avm_gpio_outfunction_set(pin, current_gpio_table[pin].func);
                ath_gpio_config_output(pin);
                break;
            case GPIO_OUTPUT_INPUT_PIN:
                ath_avm_gpio_inoutfunction_set(pin, current_gpio_table[pin].func);
                ath_gpio_config_output(pin);
                break;
        }
    }
}
#endif  /*--- #if defined(CONFIG_MACH_AR934x) || defined(CONFIG_MACH_QCA955x) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ath_gpio_shift_register_load(unsigned int mask, unsigned int values) {
    unsigned int i, clk, din, dataout;
    static unsigned int ath_gpio_shiftreg_value = 0;

    clk = 1 << ath_gpio_shiftreg_clk_pin;
    din = 1 << ath_gpio_shiftreg_din_pin;

#if 0
    printk(KERN_ERR "[%s] current=0x%x mask=0x%x values=0x%x new=0x%x\n",__FUNCTION__, 
            ath_gpio_shiftreg_value, mask, values,  (ath_gpio_shiftreg_value & ~mask) | (mask & values));
#endif

    ath_gpio_shiftreg_value &= ~mask;
    ath_gpio_shiftreg_value |= mask & values;
    ath_gpio_shiftreg_value |= 1 << (ath_gpio_shiftreg_size - 1);     /*--- muss zum Reset gesetzt sein ---*/

    for(i = ath_gpio_shiftreg_size; i; i-- ) {
#if 0
        printk(KERN_ERR "[%s] current=0x%x & 0x%x => 0x%x\n", __FUNCTION__, 
               ath_gpio_shiftreg_value, (1 << (i - 1)), (ath_gpio_shiftreg_value & (1 << (i - 1))) ? 1 : 0);
#endif
        dataout = (ath_gpio_shiftreg_value & (1 << (i - 1))) ? 1 : 0;

        gpio_ath_reg_wr(ATH_GPIO_CLEAR, clk);

        if (dataout)
            gpio_ath_reg_wr(ATH_GPIO_SET, din);
        else
            gpio_ath_reg_wr(ATH_GPIO_CLEAR, din);
        gpio_ath_reg_wr(ATH_GPIO_SET, clk);
        wmb();
    }

    gpio_ath_reg_wr(ATH_GPIO_CLEAR, clk|din);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ath_gpio_setup_pin_mode(unsigned int gpio_pin, enum _hw_gpio_function pin_mode, enum _hw_gpio_direction pin_dir)
{
#if defined(CONFIG_MACH_AR724x)
    unsigned int bits = 0;
    unsigned int bits2 = 0;
#elif defined(CONFIG_MACH_AR934x) || defined(CONFIG_MACH_QCA955x)
    unsigned int function = NO_FUNCTION;
#endif

#if defined(CONFIG_MACH_AR724x)
	switch (gpio_pin) {
		case 0:
			/* SPI Chip select on GPIO_0 [Bit 13] */
			bits |= ATH_GPIO_FUNCTION_SPI_CS_0_EN; 
			/* Enables I2S_WS on GPIO_0 [FUNC_2 Bit 4] */
			bits2 |= ATH_GPIO_FUNCTION_2_EN_I2WS_ON_0;
			break;
		case 1:
			/* SPI Chip select on GPIO_1 [Bit 14] */
			bits |= ATH_GPIO_FUNCTION_SPI_CS_1_EN;
			/* Enables I2S_CK Out on GPIO_1 [FUNC_2 Bit 3] */
			bits2 |= ATH_GPIO_FUNCTION_2_EN_I2SCK_ON_1;
			break;
		case 2:
		case 3:
		case 4:
		case 5:
			/* SPI Chip select on GPIO_2 - GPIO_5 [Bit 18] */
			bits |= ATH_GPIO_FUNCTION_SPI_EN;
			break;
		case 6:
		case 7:
		case 8:
		/*--- case 11: ---*/
		/*--- case 12: ---*/
			/* I2S on GPIO_6-8 and 11-12 [Bit 26] */
			bits |= ATH_GPIO_FUNCTION_I2S0_EN;
			break;
		case 9:
		case 10:
			/* Enables UART on GPIO_11 and GPIO_12 [Bit 1] */
			bits |= ATH_GPIO_FUNCTION_UART_EN;
			break;
		case 11:
            if( pin_mode == GPIO_PIN ) {
			    /* Master Audio CLK_MCK on GPIO_11 (only if Bit 26 is set) [Bit 27] - clear only! */
			    bits |= ATH_GPIO_FUNCTION_I2S_MCKEN; 
			    /* Disable undocumented Function [Bit 20] (otherwise GPIO mode won't work) */
			    bits |= ATH_GPIO_FUNCTION_CLK_OBS6_ENABLE;
            }
		case 12:
			/* Enables UART RTS/CTS on GPIO_11 and GPIO_12 [Bit 2] - default */
			bits |= ATH_GPIO_FUNCTION_UART_RTS_CTS_EN;
			/* I2S on GPIO_6-8 and 11-12 [Bit 26] - clear only! */
            if( pin_mode == GPIO_PIN )
			    bits |= ATH_GPIO_FUNCTION_I2S0_EN; 
            if(gpio_pin == 12) {
                /* Enables I2S_SD on GPIO_12 [FUNC_2 Bit 5] */
                bits2 |= ATH_GPIO_FUNCTION_2_EN_I2WS_ON_0;
            }
			break;
		case 13:
			/* Enables Ethernet LED on GPIO_13 [Bit 13] */
			bits |= ATH_GPIO_FUNCTION_ETH_SWITCH_LED0_EN;
            if( pin_mode == GPIO_PIN )
			    /* Disable undocumented Function [Bit 8] (otherwise GPIO mode won't work) */
			    bits |= ATH_GPIO_FUNCTION_CLK_OBS1_ENABLE;
            if( pin_mode == GPIO_PIN )
			    /* Disable TCK as SPDIF Serial out on GPIO_13 [Bit 30] - clear only! */
			    bits |= ATH_GPIO_FUNCTION_SPDIF_EN; 
			break;
		case 14:
		case 15:
		case 16:
			/* Enables I2C LED on GPIO_14 - 16 [FUNC_2 Bit 1] */
            bits2 |= ATH_GPIO_FUNCTION_2_I2S_ON_LED;
			break;
		default:
			return -1;
	}
	if ( pin_mode == GPIO_PIN ) {
        if(bits)
    		ath_reg_rmw_clear(ATH_GPIO_FUNCTIONS, bits);
        if(bits2)
		    ath_reg_rmw_clear(ATH_GPIO_FUNCTION_2, bits2);
	} else {
        if(bits)
    		ath_reg_rmw_set(ATH_GPIO_FUNCTIONS, bits);
        if(bits2)
		    ath_reg_rmw_set(ATH_GPIO_FUNCTION_2, bits2);
	}
#elif defined(CONFIG_MACH_AR934x) || defined(CONFIG_MACH_QCA955x)
    /*--- die rein generische Loesung:  ---*/
    switch(pin_mode) {
        /*--- Sonderfaelle: ---*/
        case FUNCTION_TDM_FS:  
            function = (pin_dir == GPIO_INPUT_PIN) ? GPIO_IN_SLIC_PCM_FS : GPIO_OUT_SLIC_PCM_FS;
            break;
        case FUNCTION_TDM_CLK: 
             /*--- IN: SLIC_PCM_DCL (Doppelfunktion mit I2S_MCLK) ---*/
             function = (pin_dir == GPIO_INPUT_PIN) ? GPIO_IN_I2S_MCLK : GPIO_OUT_SLIC_PCM_CLK; 
             break;
        case FUNCTION_SPDIF_OUT: 
             function = GPIO_OUT_SPDIF_OUT;
             /*--- function = GPIO_OUT_SPDIF_OUT_23; ---*/
            /*--- printk(KERN_ERR"%s: pin=%d pin_mode=%d function=%x\n", __func__,  gpio_pin, pin_mode, function); ---*/
             ath_reg_rmw_set(ATH_GPIO_FUNCTIONS, ATH_GPIO_FUNCTION_SPDIF_EN | ATH_GPIO_FUNCTION_STEREO_EN);
             /*--- printk(KERN_ERR "%s: pin=%d function=%d - > %x \n", __func__, gpio_pin, function, ath_reg_rd(ATH_GPIO_FUNCTIONS)); ---*/
             break;
        /*--- Funktion aus der hw-gpio-table  ---*/
        case FUNCTION_PIN: 
            if((current_gpio_table_size > gpio_pin) &&
               (current_gpio_table[gpio_pin].func != IGNORE_FUNCTION)) {
                function =  current_gpio_table[gpio_pin].func;
            }
            if(function == NO_FUNCTION) {
                printk(KERN_ERR "%s: error: no function-mode in gpio_table (%d)\n", __func__, gpio_pin);
            }
            /*--- printk(KERN_ERR "%s: pin=%d function=%d\n", __func__, gpio_pin, function); ---*/
            /*--- Sonderfall I2S: AudioClock enablen ---*/
            switch(function) {
                case GPIO_OUT_I2S_MCK:
                    ath_reg_rmw_set(ATH_GPIO_FUNCTIONS, ATH_GPIO_FUNCTION_I2S_MCKEN);
/*--- printk(KERN_ERR "%s: pin=%d function=%d - > %x \n", __func__, gpio_pin, function, ath_reg_rd(ATH_GPIO_FUNCTIONS)); ---*/
                    break;
                case GPIO_OUT_I2S_CLK:
                    ath_reg_rmw_set(ATH_GPIO_FUNCTIONS, ATH_GPIO_FUNCTION_I2S_REFCLKEN);
/*--- printk(KERN_ERR "%s: pin=%d function=%d - > %x \n", __func__, gpio_pin, function, ath_reg_rd(ATH_GPIO_FUNCTIONS)); ---*/
                    break;
                case GPIO_OUT_I2S_SD:
                    ath_reg_rmw_set(ATH_GPIO_FUNCTIONS, ATH_GPIO_FUNCTION_I2S0_EN);
/*--- printk(KERN_ERR "%s: pin=%d function=%d - > %x \n", __func__, gpio_pin, function, ath_reg_rd(ATH_GPIO_FUNCTIONS)); ---*/
                    break;
                case GPIO_OUT_I2S_WS:
                default:
                    break;
            }
            break;
        case GPIO_PIN: break;
        default:
            printk(KERN_ERR "%s: error: no configuration exist for pin_mode=%d\n", __func__, pin_mode);
            break;
    }
    if(pin_dir == GPIO_INPUT_PIN) {
        ath_avm_gpio_infunction_set(gpio_pin, function);
        ath_gpio_config_input(gpio_pin);
    } else {
        ath_avm_gpio_outfunction_set(gpio_pin, function);
        ath_gpio_config_output(gpio_pin);
    }
#endif /*--- #ifdef CONFIG_MACH_AR724x ---*/

	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ath_avm_gpio_ctrl(unsigned int gpio_pin, enum _hw_gpio_function pin_mode, enum _hw_gpio_direction pin_dir)
{
	if (gpio_pin > ATH_GPIO_IRQ_COUNT) {
        if ((gpio_pin >= 100) && (gpio_pin < 100 + ath_gpio_shiftreg_size)) {
            return  (pin_dir == GPIO_INPUT_PIN) ? -1 : 0; // input on shift register not supported
        }
        if ((gpio_pin >= 200) && (gpio_pin < 300)) {
            return  (pin_dir == GPIO_INPUT_PIN) ? -1 : 0; // input on lanphy-gpio not supported
        }
        return -1;
    }
#if 0
    printk("[%s] gpio=%u as %s(%d) direction=%s\n", __FUNCTION__, gpio_pin, 
            pin_mode == GPIO_PIN       ? "gpio"  : "function", pin_mode,
            pin_dir  == GPIO_INPUT_PIN ? "input" : "output");
#endif
	ath_gpio_setup_pin_mode(gpio_pin, pin_mode, pin_dir);

    if(pin_mode == GPIO_PIN) {
        if (pin_dir == GPIO_INPUT_PIN)
			ath_gpio_config_input( gpio_pin );
        else
			ath_gpio_config_output( gpio_pin );
    }
    /*--- ath_avm_gpio_dump_registers(__func__); ---*/
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ath_avm_gpio_out_bit(unsigned int gpio_pin, int value) {	

    /*--- printk(KERN_ERR "{%s} gpio_pin %d value 0x%x\n", __func__, gpio_pin, value); ---*/

    if ((gpio_pin >= 100) && (gpio_pin < 200)) {
        if (gpio_pin >= 100 + ath_gpio_shiftreg_size) {
            /*--- printk(KERN_ERR "{%s} error: invalid shift gpio %d\n", __func__, gpio_pin); ---*/
            return -1;
        }

        if((gpio_pin >= 100) && ath_gpio_shiftreg_size) {
            /*--- printk(KERN_ERR "{%s} avmnet_gpio %d value 0x%x\n", __func__, gpio_pin, value); ---*/
            gpio_pin -= 100;
            ath_gpio_shift_register_load(1 << gpio_pin, (value ? 1 : 0) << gpio_pin);
            return 0;
        }
    }
    if ((gpio_pin >= 200) && (gpio_pin < 300)) {
        /*--- printk(KERN_ERR "{%s} avmnet_gpio %d value 0x%x\n", __func__, gpio_pin, value); ---*/
        avmnet_gpio(gpio_pin - 200, value);
    }

	if (gpio_pin > ATH_GPIO_IRQ_COUNT) {
		return -1;
	}

	ath_gpio_out_val(gpio_pin, value);
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ath_avm_gpio_in_bit(unsigned int gpio_pin){
	/* wird von sammel-treiber verwendet */
	if (gpio_pin > ATH_GPIO_IRQ_COUNT) {
        return -1; // Invalid pin or shift register (input not supported)
    }
	return ath_gpio_in_val( gpio_pin ) ? 1 : 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ath_avm_gpio_in_value(void){
    return ath_reg_rd(ATH_GPIO_IN);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ath_avm_gpio_set_bitmask(unsigned int mask, unsigned int value){
	/* wird von sammel-treiber verwendet */
	ath_reg_rmw_clear(ATH_GPIO_OUT, mask);
	ath_reg_rmw_set(ATH_GPIO_OUT, (value & mask)); 
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ath_avm_gpio_init(void){

    int gpio;

    printk("[%s]\n", __FUNCTION__);
    /*--- ath_avm_gpio_dump_registers(__func__); ---*/
    ath_avm_gpio_first_handle = NULL;
#if defined(CONFIG_MACH_AR934x) || defined(CONFIG_MACH_QCA955x)
    ath_avm_gpio_init_functions();

    avm_get_hw_config(AVM_HW_CONFIG_VERSION, "gpio_avm_shift_clk", &ath_gpio_shiftreg_clk_pin, NULL);
    avm_get_hw_config(AVM_HW_CONFIG_VERSION, "gpio_avm_shift_din", &ath_gpio_shiftreg_din_pin, NULL);
    avm_get_hw_config(AVM_HW_CONFIG_VERSION, "shift_register_size", &ath_gpio_shiftreg_size, NULL);

    if(avm_get_hw_config(AVM_HW_CONFIG_VERSION, "gpio_avm_spi_cs_dis_dac", &gpio, NULL) == 0) {
        printk("disable spi_cs_dac\n");
        ath_avm_gpio_out_bit(gpio, 0); /*--- disable SPI_CS1-Access (or'd with SPDIF_OUT) ---*/
    }

    if(avm_get_hw_config(AVM_HW_CONFIG_VERSION, "gpio_avm_peregrine_reset", &gpio, NULL) == 0) {
        printk("Peregrine out of reset\n");
        ath_avm_gpio_out_bit(gpio, 1); /*--- disable RESET ---*/
    }

#endif
    return 0;
}
arch_initcall(ath_avm_gpio_init);


EXPORT_SYMBOL(ath_avm_gpio_init);
EXPORT_SYMBOL(ath_avm_gpio_ctrl);
EXPORT_SYMBOL(ath_avm_gpio_out_bit);
EXPORT_SYMBOL(ath_avm_gpio_in_bit);
EXPORT_SYMBOL(ath_avm_gpio_in_value);
EXPORT_SYMBOL(ath_avm_gpio_set_bitmask);

