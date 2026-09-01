#define CONFIG_MIPS_IKAN
/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>

#include <asm/mach_avm.h>
#include <asm/mach-fusiv/vx180.h>
#include <asm/mach-fusiv/hw_gpio.h>

/*--- #define AR7GPIO_DEBUG ---*/
#undef AR7GPIO_DEBUG

#if defined(AR7GPIO_DEBUG)
#define DBG(...)  printk(KERN_INFO __VA_ARGS__)
#else /*--- #if defined(AR7GPIO_DEBUG) ---*/
#define DBG(...)  
#endif /*--- #else ---*/ /*--- #if defined(AR7GPIO_DEBUG) ---*/

#if defined (CONFIG_MIPS_IKAN)

#define GPIO_IRQ1_REGISTERED    1 /*--- für GPIOs  0..15 ---*/
#define GPIO_IRQ2_REGISTERED    2 /*--- für GPIOs 16..31 ---*/
#define GPIO_IRQ1_ENABLED       4 /*--- für GPIOs  0..15 ---*/
#define GPIO_IRQ2_ENABLED       8 /*--- für GPIOs 16..31 ---*/

/*--- #define GPIO_IRQEN(PIN, FLAG) VX180_GPIO_IRQEN(PIN, FLAG) ---*/
#define GPIO_IRQEN(PIN, FLAG) { \
            VX180_GPIO_IRQEN(PIN, FLAG); \
            if(FLAG) ikan_gpio_irq_global_mask |= (1 << PIN);  \
            else     ikan_gpio_irq_global_mask &= ~(1 << PIN); \
        }

int ikan_gpio_irq_flags;
int ikan_gpio_irq_global_mask;
static spinlock_t ikan_gpio_spinlock;
struct _hw_gpio_irqhandle *ikan_gpio_first_handle;

irqreturn_t ikan_gpio_manage_irq(int irq, void *dev_id);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ikan_gpio_dump_registers(void) {
    union __vx180_gpio_flag        *pflag; 
    union __vx180_gpio_dir         *pdir;
    union __vx180_gpio_irq_enable  *pirq; 
    union __vx180_gpio_polar       *ppolar;
    union __vx180_gpio_sensitivity *psens;
    union __vx180_gpio_both_edges  *pboth;
    int i;

    for(i = 0; i < 2; i++) {
        pflag   = (union __vx180_gpio_flag *)VX180_GPIO_FLAG_BASE(i);
        pdir    = (union __vx180_gpio_dir *)VX180_GPIO_DIR_BASE(i);
        pirq    = (union __vx180_gpio_irq_enable *)VX180_GPIO_IRQ_ENABLE_BASE(i);
        ppolar  = (union __vx180_gpio_polar *)VX180_GPIO_POLAR_BASE(i);
        psens   = (union __vx180_gpio_sensitivity *)VX180_GPIO_SENSITIVITY_BASE(i);
        pboth   = (union __vx180_gpio_both_edges *)VX180_GPIO_BOTH_EDGES_BASE(i);
    
        printk(KERN_ERR "GPIO register configuration for %s\n", (i ? "16..31" : "0..15"));
        printk(KERN_ERR "   flag(%d)        = 0x%08x\n", i, pflag->Reg);
        printk(KERN_ERR "   dir(%d)         = 0x%08x\n", i, pdir->Reg);
        printk(KERN_ERR "   irq_enable (%d) = 0x%08x\n", i, pirq->Reg);  
        printk(KERN_ERR "   polar(%d)       = 0x%08x\n", i, ppolar->Reg);
        printk(KERN_ERR "   sensitivity(%d) = 0x%08x\n", i, psens->Reg); 
        printk(KERN_ERR "   both_edges(%d)  = 0x%08x\n", i, pboth->Reg); 
    }

#if 0
    printk(KERN_ERR "PCI Status Register 1 0x%08x\n", *((volatile unsigned int *)0xb9050190));
    printk(KERN_ERR "PCI Status Register 2 0x%08x\n", *((volatile unsigned int *)0xb9050194));

    printk(KERN_ERR "IPC to CPE 1a: 0x%08x\n", *((volatile unsigned int *)0xb9050130));
    printk(KERN_ERR "IPC to CPE 1b: 0x%08x\n", *((volatile unsigned int *)0xb9050134));
    printk(KERN_ERR "IPC to CPE 2a: 0x%08x\n", *((volatile unsigned int *)0xb9050138));
    printk(KERN_ERR "IPC to CPE 2b: 0x%08x\n", *((volatile unsigned int *)0xb905013C));
    printk(KERN_ERR "IPC to CPE 3a: 0x%08x\n", *((volatile unsigned int *)0xb9050140));
    printk(KERN_ERR "IPC to CPE 3b: 0x%08x\n", *((volatile unsigned int *)0xb9050144));
    printk(KERN_ERR "IPC to CPE 4a: 0x%08x\n", *((volatile unsigned int *)0xb9050148));
    printk(KERN_ERR "IPC to CPE 4b: 0x%08x\n", *((volatile unsigned int *)0xb905014C));
    printk(KERN_ERR "IPC to CPE 5a: 0x%08x\n", *((volatile unsigned int *)0xb9050150));
    printk(KERN_ERR "IPC to CPE 5b: 0x%08x\n", *((volatile unsigned int *)0xb9050154));
    printk(KERN_ERR "IPC to CPE 6a: 0x%08x\n", *((volatile unsigned int *)0xb9050158));
    printk(KERN_ERR "IPC to CPE 6b: 0x%08x\n", *((volatile unsigned int *)0xb905015C));

    printk(KERN_ERR "IPC to  AP 1a: 0x%08x\n", *((volatile unsigned int *)0xb9050168));
    printk(KERN_ERR "IPC to  AP 1b: 0x%08x\n", *((volatile unsigned int *)0xb905016C));
    printk(KERN_ERR "IPC to  AP 2a: 0x%08x\n", *((volatile unsigned int *)0xb9050170));
    printk(KERN_ERR "IPC to  AP 2b: 0x%08x\n", *((volatile unsigned int *)0xb9050174));
    printk(KERN_ERR "IPC to  AP 3a: 0x%08x\n", *((volatile unsigned int *)0xb9050178));
    printk(KERN_ERR "IPC to  AP 3b: 0x%08x\n", *((volatile unsigned int *)0xb905017C));
    printk(KERN_ERR "IPC to  AP 4a: 0x%08x\n", *((volatile unsigned int *)0xb9050180));
    printk(KERN_ERR "IPC to  AP 4b: 0x%08x\n", *((volatile unsigned int *)0xb9050184));
    printk(KERN_ERR "IPC to  AP 5a: 0x%08x\n", *((volatile unsigned int *)0xb9050188));
    printk(KERN_ERR "IPC to  AP 5b: 0x%08x\n", *((volatile unsigned int *)0xb905018C));

    printk(KERN_ERR "PCI Control 2: 0x%08x\n", *((volatile unsigned int *)0xb9170038));
    printk(KERN_ERR "IPC to PCI 1a: 0x%08x\n", *((volatile unsigned int *)0xb9050198));
    printk(KERN_ERR "IPC to PCI 1b: 0x%08x\n", *((volatile unsigned int *)0xb905019C));
#endif
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void reset_registers(void) {
    union __vx180_gpio_flag        *pflag; 
    union __vx180_gpio_dir         *pdir;
    union __vx180_gpio_irq_enable  *pirq; 
    union __vx180_gpio_polar       *ppolar;
    union __vx180_gpio_sensitivity *psens;
    union __vx180_gpio_both_edges  *pboth;
    int i;

    for(i = 0; i < 2; i++) {
        pflag   = (union __vx180_gpio_flag *)VX180_GPIO_FLAG_BASE(i);
        pdir    = (union __vx180_gpio_dir *)VX180_GPIO_DIR_BASE(i);
        pirq    = (union __vx180_gpio_irq_enable *)VX180_GPIO_IRQ_ENABLE_BASE(i);
        ppolar  = (union __vx180_gpio_polar *)VX180_GPIO_POLAR_BASE(i);
        psens   = (union __vx180_gpio_sensitivity *)VX180_GPIO_SENSITIVITY_BASE(i);
        pboth   = (union __vx180_gpio_both_edges *)VX180_GPIO_BOTH_EDGES_BASE(i);

        pflag->outputBits.flag_clr = 0xFFFF;
        pirq->outputBits.flag_clr  = 0xFFFF;    
        pdir->Bits.dir             = 0x0000;
        ppolar->Bits.polar         = 0x0000;
        psens->Bits.sensitivity    = 0x0000; 
        pboth->Bits.both_edges     = 0x0000;
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int __init ikan_gpio_init(void) {
    printk("[ikan_gpio_init]\n");
    spin_lock_init(&ikan_gpio_spinlock);

    ikan_gpio_irq_flags       = 0;
    ikan_gpio_irq_global_mask = 0;
    ikan_gpio_first_handle    = NULL;
    reset_registers();
    /*--- ikan_gpio_dump_registers(); ---*/
    return 0;
}

arch_initcall(ikan_gpio_init);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ikan_gpio_ctrl(unsigned int gpio_pin, enum _hw_gpio_function pin_mode, enum _hw_gpio_direction pin_dir) {
    unsigned long flags;
    unsigned int irq_restore;
    union __vx180_gpio_irq_enable *pirq;
    pirq = (union __vx180_gpio_irq_enable *)VX180_GPIO_IRQ_ENABLE_BASE(gpio_pin < 16 ? 0 : 1);

    /*--- printk("[ikan_gpio_ctrl] gpio=%u as %s direction=%s\n", gpio_pin, ---*/ 
            /*--- (pin_mode == GPIO_PIN) ? "gpio" : "function", ---*/
            /*--- (pin_dir == GPIO_INPUT_PIN) ? "input" : "output"); ---*/

    if(gpio_pin > 31)
        return(-1);

    spin_lock_irqsave(&ikan_gpio_spinlock, flags);
    irq_restore = pirq->outputBits.flag_set & (1 << (gpio_pin % 16));
    if(irq_restore)
        GPIO_IRQEN(gpio_pin, 0); /*--- Interrupt maskieren (unerwünschte Interrupts unterbinden) ---*/
    if(pin_mode == GPIO_PIN) {
        switch(gpio_pin) {
            case 0 ... 2:  /*--- MISC_CTL_REGISTER ---*/
                if(*((volatile unsigned int *)0xB9000088) & (1 << (gpio_pin + 24))) {
                    printk(KERN_ERR "[GPIO] pin %d is configured as funktion pin, not avileable as gpio\n", gpio_pin);
                    return -EFAULT;
                }
                break;
            case 3 ... 4:  /*--- MODE_CTL_REGISTER ---*/
                if(*((volatile unsigned int *)0xB90000B0) & (1 << 5)) {
                    printk(KERN_ERR "[GPIO] pin %d is configured as funktion pin, not avileable as gpio\n", gpio_pin);
                    return -EFAULT;
                }
                break;
            case 7:  /*--- MODE_CTL_REGISTER ---*/
                if(*((volatile unsigned int *)0xB90000B0) & (1 << 4)) {
                    printk(KERN_ERR "[GPIO] pin %d is configured as funktion pin, not avileable as gpio\n", gpio_pin);
                    return -EFAULT;
                }
                /*--- kein break; ---*/
            case 8 ... 10:  /*--- MISC_CTL_REGISTER ---*/
                if(*((volatile unsigned int *)0xB9000088) & (1 << (gpio_pin - 3))) {
                    printk(KERN_ERR "[GPIO] pin %d is configured as funktion pin, not avileable as gpio\n", gpio_pin);
                    return -EFAULT;
                }
                break;
            case 11 ... 12:  /*--- MISC_CTL_REGISTER ---*/
                if(*((volatile unsigned int *)0xB9000088) & (1 << (gpio_pin + 16))) {
                    printk(KERN_ERR "[GPIO] pin %d is configured as funktion pin, not avileable as gpio\n", gpio_pin);
                    return -EFAULT;
                }
                break;
            case 16 ... 19:  /*--- PAD_PIO_MISC_CTL_REGISTER ---*/
                if(*((volatile unsigned int *)0xB90000D4) & (1 << (gpio_pin - 12))) {
                    printk(KERN_ERR "[GPIO] pin %d is configured as funktion pin, not avileable as gpio\n", gpio_pin);
                    return -EFAULT;
                }
                break;
            default:
                break;
        }
        if (pin_dir == GPIO_INPUT_PIN)
            VX180_GPIO_SETDIR(gpio_pin, VX180_GPIO_AS_INPUT);
        else
            VX180_GPIO_SETDIR(gpio_pin, VX180_GPIO_AS_OUTPUT);
    }
    if(irq_restore)
        GPIO_IRQEN(gpio_pin, 1); /*--- u.U. deaktivierten Interrupt wieder aktivieren ---*/
    spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);
    return (0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _hw_gpio_irqhandle *ikan_gpio_enqueue_handle(unsigned int mask, int (*handle_func)(unsigned int)) {
    struct _hw_gpio_irqhandle *handle;


    handle = kmalloc(sizeof(struct _hw_gpio_irqhandle), GFP_ATOMIC);
    if(handle) {
        memset(handle, 0, sizeof(struct _hw_gpio_irqhandle));
        handle->enabled = 1;
        handle->mask    = mask;
        handle->func    = handle_func;
        /*--- Neues Handle vorn in die Liste einhängen: ---*/
        handle->next    = ikan_gpio_first_handle;
        ikan_gpio_first_handle = handle;
    }

    return handle;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ikan_read_irq_gpio(struct _hw_gpio_irqhandle *handle, unsigned int gpio_pin, unsigned int mask) {
    int value;
    if(gpio_pin > 31) {
        printk("[GPIO] invallid gpio pin %d\n", gpio_pin);
        return(-1);
    }
    if(!handle->delayed_irq)
        ikan_gpio_disable_irq(handle);

    if(handle->is_both_edges)
        VX180_GPIO_BOTHEDGES(gpio_pin, 0);
    if(handle->is_edge)
        VX180_GPIO_SENSITIVE(gpio_pin, 0);

    /*--- printk("gpio(%d)=0x%x/%x", gpio_pin, pflag->outputBits.flag_set, pflag->outputBits.flag_clr); ---*/

    value = VX180_GPIO_INPUT(gpio_pin);

    if(handle->is_both_edges)
            VX180_GPIO_BOTHEDGES(gpio_pin, 1);
    if(handle->is_edge)
        VX180_GPIO_SENSITIVE(gpio_pin, 1);

    ikan_gpio_set_bitmask(mask, 0);  /*--- Interrupt Acknowledge ---*/
    ikan_gpio_enable_irq(handle);
    return value;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
irqreturn_t ikan_gpio_manage_irq(int irq, void *dev_id) {
    static unsigned int value_mask;
    static struct _hw_gpio_irqhandle *handle;

    value_mask = ikan_gpio_in_value();
    if(irq == INT_GPIO)
        value_mask &= 0x0000FFFF; /*--- Interrupt für GPIOs 0..15 ---*/
    else
        value_mask &= 0xFFFF0000; /*--- Interrupt für GPIOs 16..31 ---*/

    for(handle = ikan_gpio_first_handle; handle; handle = handle->next) {
        if( handle->enabled && (value_mask & handle->mask) ) {
            /*--- union __vx180_gpio_flag *pflag; ---*/
            unsigned int bit;
            bit = ffs(value_mask & handle->mask) - 1;

#if 0
            ikan_gpio_disable_irq(handle);
#endif

            /*--- if(bit > 15) { ---*/
                /*--- pflag = (union __vx180_gpio_flag *)VX180_GPIO_FLAG_BASE(1); ---*/
            /*--- } else { ---*/
                /*--- pflag = (union __vx180_gpio_flag *)VX180_GPIO_FLAG_BASE(0); ---*/
            /*--- } ---*/
#if 1
            ikan_gpio_set_bitmask(value_mask & handle->mask, 0);  /*--- Interrupt Acknowledge ---*/
#endif
#if 1
            if(!handle->delayed_irq) {
                if(handle->is_both_edges)
                    VX180_GPIO_BOTHEDGES(bit, 0);
                if(handle->is_edge)
                    VX180_GPIO_SENSITIVE(bit, 0);
            }
#endif
            /*--- printk("gpio(%d)=0x%x/%x", bit, pflag->outputBits.flag_set, pflag->outputBits.flag_clr); ---*/

            /*--- pflag->outputBits.flag_clr = (unsigned short) --*/
            (*handle->func)(value_mask & handle->mask);

            if(!handle->delayed_irq) {
#if 1
                if(handle->is_both_edges)
                        VX180_GPIO_BOTHEDGES(bit, 1);
                if(handle->is_edge)
                        VX180_GPIO_SENSITIVE(bit, 1);
#endif

                ikan_gpio_set_bitmask(value_mask & handle->mask, 0);  /*--- Interrupt Acknowledge ---*/
#if 0
                ikan_gpio_enable_irq(handle);
#endif
            }

        }
    }

    if((*(unsigned short *)VX180_GPIO_INTREN_NEW_BASE & 0x01) && (*(unsigned short *)VX180_GPIO_FLAG_NEW_BASE & 0x01)) {
        printk(KERN_ERR "[ikan_gpio_manage_irq] PCI CLKRUN INTERRRUPT\n");
    }

    return IRQ_HANDLED;    
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _hw_gpio_irqhandle *ikan_gpio_request_irq(unsigned int mask, enum _hw_gpio_polarity mode, 
                    enum _hw_gpio_sensitivity edge, int (*handle_func)(unsigned int)) {
    unsigned long flags;
    int gpio_pin;
    struct _hw_gpio_irqhandle *handle;

    DBG( "[ikan_gpio_request_irq] gpio_mask=0x%X, polarity=%s, sensitivity=%s\n", mask, 
            mode ? "active_low" : "active_high", 
            (edge == 2) ? "both_edges" : ((edge == 1) ? "edge" : "level"));

    /*--- GPIO 0..15 => Interrupt INT_GPIO registrieren: ---*/
    if((mask & 0xFFFF) && !(ikan_gpio_irq_flags & GPIO_IRQ1_REGISTERED)) {
        /*--- printk(KERN_ERR "[gpio] init base irq for GPIO_IRQ1\n"); ---*/
        /*--- Interrupts für GPIOs 0..15 deaktivieren: ---*/
        for(gpio_pin = 0; gpio_pin < 16; gpio_pin++) {
                spin_lock_irqsave(&ikan_gpio_spinlock, flags);
                GPIO_IRQEN(gpio_pin, 0);
                spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);
        }
        if( request_irq(INT_GPIO, ikan_gpio_manage_irq, IRQF_DISABLED, "gpio 0..15", NULL) == 0 ) {
            ikan_gpio_irq_flags |= GPIO_IRQ1_REGISTERED;
            /*--- ikan_gpio_irq_flags |= GPIO_IRQ1_ENABLED; ---*/
            disable_irq(INT_GPIO);
        }
        else {
            printk(KERN_ERR "[ikan_gpio_request_irq] Error: Failed to register interrupt INT_GPIO\n");
            return NULL;
        }
    }
    /*--- GPIO 16..31 => Interrupt INT_GPIO2 registrieren: ---*/
    if((mask >> 16) && !(ikan_gpio_irq_flags & GPIO_IRQ2_REGISTERED))  {
        /*--- printk(KERN_ERR "[gpio] init base irq for GPIO_IRQ2\n"); ---*/
        /*--- Interrupts für GPIOs 16..31 deaktivieren: ---*/
        for(gpio_pin = 16; gpio_pin < 32; gpio_pin++) {
                spin_lock_irqsave(&ikan_gpio_spinlock, flags);
                GPIO_IRQEN(gpio_pin, 0);
                spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);
        }
        if( request_irq(INT_GPIO2, ikan_gpio_manage_irq, IRQF_DISABLED, "gpio 16..31", NULL) == 0 ) {
            ikan_gpio_irq_flags |= GPIO_IRQ2_REGISTERED;
            /*--- ikan_gpio_irq_flags |= GPIO_IRQ2_ENABLED; ---*/
            disable_irq(INT_GPIO2);
        }
        else {
            printk(KERN_ERR "[ikan_gpio_request_irq] Error: Failed to register interrupt INT_GPIO2\n");
            return NULL;
        }
    }

    /*--- Interrupt handle in Liste einfügen: ---*/
    handle = ikan_gpio_enqueue_handle(mask, handle_func);
    if(!handle)
        return NULL;

    /*--- GPIOs konfigurieren: ---*/
    for(gpio_pin = 0; gpio_pin < GPIO_BITS; gpio_pin++) {
        if(mask & (1 << gpio_pin) ) {
            spin_lock_irqsave(&ikan_gpio_spinlock, flags);
            printk(KERN_ERR "[gpio %d]: VX180_GPIO_POLAR: %s\n", gpio_pin, mode == 1 ? "active low" : "active high");
            VX180_GPIO_POLAR(gpio_pin, mode);
            handle->is_active_low = mode;
            switch(edge) {
                case GPIO_LEVEL_SENSITIVE:
                    printk(KERN_ERR "[gpio %d]: GPIO_LEVEL_SENSITIVE\n", gpio_pin);
                    VX180_GPIO_SENSITIVE(gpio_pin, 0);
                    VX180_GPIO_BOTHEDGES(gpio_pin, 0);
                    handle->is_edge = 0;
                    handle->is_both_edges = 0;
                    break;
                case GPIO_EDGE_SENSITIVE:
                    printk(KERN_ERR "[gpio %d]: GPIO_EDGE_SENSITIVE\n", gpio_pin);
                    VX180_GPIO_SENSITIVE(gpio_pin, 1);
                    VX180_GPIO_BOTHEDGES(gpio_pin, 0);
                    handle->is_edge = 1;
                    handle->is_both_edges = 0;
                    break;
                case GPIO_BOTH_EDGES_SENSITIVE:
                    printk(KERN_ERR "[gpio %d]: GPIO_BOTH_EDGES_SENSITIVE\n", gpio_pin);
                    VX180_GPIO_SENSITIVE(gpio_pin, 1);
                    VX180_GPIO_BOTHEDGES(gpio_pin, 1);
                    handle->is_edge = 1;
                    handle->is_both_edges = 1;
                    break;
            }
            spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);
        }
    }
    return handle;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ikan_gpio_set_delayed_irq_mode(struct _hw_gpio_irqhandle *handle, int delayed) {
    /*--- DBG( "[ikan_gpio_set_delayed_irq_mode] IRQ delay mode %s\n", delayed ? "set" : "cleared"); ---*/
    handle->delayed_irq = delayed;
}

static inline void disable_irq_save(int irq, unsigned int *p_gpio_irq_flags, unsigned int gpio_irq_bit) {
    unsigned long flags;
    int do_irq_disable = 0;
    spin_lock_irqsave(&ikan_gpio_spinlock, flags);
    if(*p_gpio_irq_flags & gpio_irq_bit) {
        *p_gpio_irq_flags &= ~gpio_irq_bit;
        do_irq_disable = 1;
    }
    spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);
    if(do_irq_disable) {
        disable_irq(irq);
        *p_gpio_irq_flags &= ~gpio_irq_bit; /*--- => falls in der Zwischenzeit noch ein Interrupt kam ---*/
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ikan_gpio_enable_irq(struct _hw_gpio_irqhandle *handle) {
    unsigned long flags;
    unsigned int old_mask;
    int gpio_pin;
    
    if(!handle || !handle->func)
        return (-1);

    DBG("[gpio_enable_irq] gpio_mask=0x%X => currently %s\n", 
           handle->mask, handle->enabled ? "enabled" : "disabled" );

#if 0
    if(handle->mask & 0x0000FFFF) {
        disable_irq_save(INT_GPIO, &ikan_gpio_irq_flags, GPIO_IRQ1_ENABLED);
        DBG( "[gpio_enable_irq] temporarily disable base irq for GPIO_IRQ1\n");
    }
    if(handle->mask & 0xFFFF0000) {
        disable_irq_save(INT_GPIO2, &ikan_gpio_irq_flags, GPIO_IRQ2_ENABLED);
        DBG( "[gpio_enable_irq] temporarily disable base irq for GPIO_IRQ2\n");
    }

    /*--- GPIO-Interrupts entsprechend der Maske aktivieren: ---*/
    for(gpio_pin = 0; gpio_pin < GPIO_BITS; gpio_pin++) {
        if(handle->mask & (1 << gpio_pin) ) {
            spin_lock_irqsave(&ikan_gpio_spinlock, flags);
            handle->enabled = 1;
            GPIO_IRQEN(gpio_pin, 1);
            spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);
        }
    }

    if((handle->mask & 0x0000FFFF) && (ikan_gpio_irq_global_mask & 0x0000FFFF)) {
        DBG( "[gpio_enable_irq] enable base irq for GPIO_IRQ1\n");
        ikan_gpio_irq_flags |= GPIO_IRQ1_ENABLED;
        enable_irq(INT_GPIO);
    }
    if((handle->mask & 0xFFFF0000) && (ikan_gpio_irq_global_mask & 0xFFFF0000)) {
        DBG( "[gpio_enable_irq] enable base irq for GPIO_IRQ2\n");
        ikan_gpio_irq_flags |= GPIO_IRQ2_ENABLED;
        enable_irq(INT_GPIO2);
    }
#else
    spin_lock_irqsave(&ikan_gpio_spinlock, flags);

    old_mask = ikan_gpio_irq_global_mask;

    /*--- GPIO-Interrupts entsprechend der Maske aktivieren: ---*/
    for(gpio_pin = 0; gpio_pin < GPIO_BITS; gpio_pin++) {
        if(handle->mask & (1 << gpio_pin) ) {
            handle->enabled = 1;
            GPIO_IRQEN(gpio_pin, 1);
        }
    }

    if(!(old_mask & 0x0000FFFF) && (ikan_gpio_irq_global_mask & 0x0000FFFF)) {
        DBG( "[gpio_enable_irq] enable base irq for GPIO_IRQ1\n");
        enable_irq(INT_GPIO);
    }
    if(!(old_mask & 0xFFFF0000) && (ikan_gpio_irq_global_mask & 0xFFFF0000)) {
        DBG( "[gpio_enable_irq] enable base irq for GPIO_IRQ2\n");
        enable_irq(INT_GPIO2);
    }

    spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);
#endif

    return (0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ikan_gpio_disable_irq(struct _hw_gpio_irqhandle *handle) {
    unsigned long flags;
    unsigned int old_mask;
    int gpio_pin;

    if(!handle)
        return (-1);

    DBG("[gpio_disable_irq] gpio_mask=0x%X => currently %s\n", 
           handle->mask, handle->enabled ? "enabled" : "disabled" );

#if 0
    if(handle->mask & 0x0000FFFF) {
        disable_irq_save(INT_GPIO, &ikan_gpio_irq_flags, GPIO_IRQ1_ENABLED);
        DBG( "[gpio_disable_irq] disable base irq for GPIO_IRQ1\n");
    }
    if(handle->mask & 0xFFFF0000) {
        disable_irq_save(INT_GPIO2, &ikan_gpio_irq_flags, GPIO_IRQ2_ENABLED);
        DBG( "[gpio_disable_irq] disable base irq for GPIO_IRQ2\n");
    }

    /*--- GPIO-Interrupts entsprechend der Maske deaktivieren: ---*/
    for(gpio_pin = 0; gpio_pin < GPIO_BITS; gpio_pin++) {
        if(handle->mask & (1 << gpio_pin) ) {
            spin_lock_irqsave(&ikan_gpio_spinlock, flags);
            handle->enabled = 0;
            ikan_gpio_irq_global_mask &= ~(1 << gpio_pin);
            GPIO_IRQEN(gpio_pin, 0);
            spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);
        }
    }

    if((handle->mask & 0x0000FFFF) && (ikan_gpio_irq_global_mask & 0x0000FFFF)) {
        DBG( "[gpio_disable_irq] re-enable base irq for GPIO_IRQ1\n");
        ikan_gpio_irq_flags |= GPIO_IRQ1_ENABLED;
        enable_irq(INT_GPIO);
    }
    if((handle->mask & 0xFFFF0000) && (ikan_gpio_irq_global_mask & 0xFFFF0000)) {
        DBG( "[gpio_disable_irq] re-enable base irq for GPIO_IRQ2\n");
        ikan_gpio_irq_flags |= GPIO_IRQ2_ENABLED;
        enable_irq(INT_GPIO2);
    }
#else

    spin_lock_irqsave(&ikan_gpio_spinlock, flags);

    old_mask = ikan_gpio_irq_global_mask;

    /*--- GPIO-Interrupts entsprechend der Maske deaktivieren: ---*/
    for(gpio_pin = 0; gpio_pin < GPIO_BITS; gpio_pin++) {
        if(handle->mask & (1 << gpio_pin) ) {
            handle->enabled = 0;
            GPIO_IRQEN(gpio_pin, 0);
        }
    }

    if ((old_mask & 0x0000FFFF) && !(ikan_gpio_irq_global_mask & 0x0000FFFF))
        disable_irq(INT_GPIO);

    if ((old_mask & 0xFFFF0000) && !(ikan_gpio_irq_global_mask & 0xFFFF0000))
        disable_irq(INT_GPIO2);

    spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);

#endif
    return (0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ikan_gpio_out_bit(unsigned int gpio_pin, int value) {
    unsigned long flags;

    /*--- printk("[ikan_gpio_out_bit] gpio=%u value=%u\n", gpio_pin, value); ---*/

    if(gpio_pin > 31) {
        printk("[GPIO] invallid gpio pin %d\n", gpio_pin);
        return(-1);
    }


    spin_lock_irqsave(&ikan_gpio_spinlock, flags);
    VX180_GPIO_OUTPUT(gpio_pin, value);
    spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);

    return(0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ikan_gpio_in_bit(unsigned int gpio_pin) {

    /*--- printk("[ikan_gpio_in_bit] gpio=%u\n", gpio_pin); ---*/
    if(gpio_pin > 31) {
        printk("[GPIO] invallid gpio pin %d\n", gpio_pin);
        return(-1);
    }

    return VX180_GPIO_INPUT(gpio_pin);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ikan_gpio_in_value(void) {
    unsigned long flags;
    unsigned int value;
    union __vx180_gpio_flag *pflag1 = (union __vx180_gpio_flag *)VX180_GPIO_FLAG_BASE(0);
    union __vx180_gpio_flag *pflag2 = (union __vx180_gpio_flag *)VX180_GPIO_FLAG_BASE(1);

    /*--- printk("[gpio] FLAG_BASE  0x%x (32 Bit)\n", *(volatile unsigned int *)VX180_GPIO_FLAG_BASE(0)); ---*/
    /*--- printk("[gpio] FLAG_BASE  0x%x (16 Bit)\n", (unsigned int)*(volatile short int *)VX180_GPIO_FLAG_BASE(0)); ---*/
    /*--- printk("[gpio] FLAG_BASE2 0x%x (32 Bit)\n", *(volatile unsigned int *)VX180_GPIO_FLAG_BASE(1)); ---*/
    /*--- printk("[gpio] FLAG_BASE2 0x%x (16 Bit)\n", (unsigned int)*(volatile short int *)VX180_GPIO_FLAG_BASE(1)); ---*/

    /*--- printk("[ikan_gpio_in_value]\n"); ---*/
    spin_lock_irqsave(&ikan_gpio_spinlock, flags);
    value = (pflag2->Bits.flag << 16) | pflag1->Bits.flag;
    /*--- printk(KERN_ERR "[ikan_gpio_in_value]: 0x%08X\n", value); ---*/
    spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);
    return value;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ikan_gpio_set_bitmask(unsigned int mask, unsigned int value) {
    unsigned long flags;
    union __vx180_gpio_flag *pflag1 = (union __vx180_gpio_flag *)VX180_GPIO_FLAG_BASE(0);
    union __vx180_gpio_flag *pflag2 = (union __vx180_gpio_flag *)VX180_GPIO_FLAG_BASE(1);
    /*--- unsigned short *pflag1_clr = (unsigned short *)VX180_GPIO_FLAG_BASE(0); ---*/
    /*--- unsigned short *pflag2_clr = (unsigned short *)VX180_GPIO_FLAG_BASE(1); ---*/
    /*--- unsigned short *pflag1_set = (unsigned short *)VX180_GPIO_FLAG_BASE(0) + 1; ---*/
    /*--- unsigned short *pflag2_set = (unsigned short *)VX180_GPIO_FLAG_BASE(1) + 1; ---*/


    /*--- printk(KERN_ERR "[ikan_gpio_set_bitmask] mask=0x%X value=0x%X\n", mask, value); ---*/
    spin_lock_irqsave(&ikan_gpio_spinlock, flags);

    /*--- *pflag1_set = ( value & mask) & 0xFFFF; ---*/
    /*--- *pflag1_clr = (~value & mask) & 0xFFFF; ---*/
    /*--- value >>= 16; ---*/
    /*--- mask  >>= 16; ---*/
    /*--- *pflag2_set = ( value & mask) & 0xFFFF; ---*/
    /*--- *pflag2_clr = (~value & mask) & 0xFFFF; ---*/

    pflag1->outputBits.flag_set = ( value & mask) & 0xFFFF;
    pflag1->outputBits.flag_clr = (~value & mask) & 0xFFFF;
    pflag2->outputBits.flag_set = (( value & mask) >> 16) & 0xFFFF;
    pflag2->outputBits.flag_clr = ((~value & mask) >> 16) & 0xFFFF;
    spin_unlock_irqrestore(&ikan_gpio_spinlock, flags);
}

EXPORT_SYMBOL(ikan_gpio_ctrl);
EXPORT_SYMBOL(ikan_gpio_request_irq);
EXPORT_SYMBOL(ikan_gpio_set_delayed_irq_mode);
EXPORT_SYMBOL(ikan_gpio_enable_irq);
EXPORT_SYMBOL(ikan_gpio_disable_irq);
EXPORT_SYMBOL(ikan_read_irq_gpio);
EXPORT_SYMBOL(ikan_gpio_out_bit);
EXPORT_SYMBOL(ikan_gpio_in_bit);
EXPORT_SYMBOL(ikan_gpio_in_value);
EXPORT_SYMBOL(ikan_gpio_set_bitmask);

#endif /*--- #if defined (CONFIG_MIPS_IKAN) ---*/

