#ifndef _AR7240_GPIO_H
#define _AR7240_GPIO_H

#define GPIO_BITS               32

#define GPIO_BIT_0               0
#define GPIO_BIT_1               1
#define GPIO_BIT_2               2
#define GPIO_BIT_3               3
#define GPIO_BIT_4               4
#define GPIO_BIT_5               5
#define GPIO_BIT_6               6
#define GPIO_BIT_7               7
#define GPIO_BIT_8               8
#define GPIO_BIT_9               9
#define GPIO_BIT_10             10
#define GPIO_BIT_11             11
#define GPIO_BIT_12             12
#define GPIO_BIT_13             13
#define GPIO_BIT_14             14
#define GPIO_BIT_15             15
#define GPIO_BIT_16             16
#define GPIO_BIT_17             17
#define GPIO_BIT_18             18
#define GPIO_BIT_19             19
#define GPIO_BIT_20             20
#define GPIO_BIT_21             21
#define GPIO_BIT_22             22
#define GPIO_BIT_23             23
#define GPIO_BIT_24             24
#define GPIO_BIT_25             25
#define GPIO_BIT_26             26
#define GPIO_BIT_27             27
#define GPIO_BIT_28             28
#define GPIO_BIT_29             29
#define GPIO_BIT_30             30
#define GPIO_BIT_31             31


#define GPIO_MASK_0             (1 << 0)
#define GPIO_MASK_1             (1 << 1 )
#define GPIO_MASK_2             (1 << 2 )
#define GPIO_MASK_3             (1 << 3 )
#define GPIO_MASK_4             (1 << 4 )
#define GPIO_MASK_5             (1 << 5 )
#define GPIO_MASK_6             (1 << 6 )
#define GPIO_MASK_7             (1 << 7 )
#define GPIO_MASK_8             (1 << 8 )
#define GPIO_MASK_9             (1 << 9 )
#define GPIO_MASK_10            (1 << 10)
#define GPIO_MASK_11            (1 << 11)
#define GPIO_MASK_12            (1 << 12)
#define GPIO_MASK_13            (1 << 13)
#define GPIO_MASK_14            (1 << 14)
#define GPIO_MASK_15            (1 << 15)
#define GPIO_MASK_16            (1 << 16)
#define GPIO_MASK_17            (1 << 17)
#define GPIO_MASK_18            (1 << 18)
#define GPIO_MASK_19            (1 << 19)
#define GPIO_MASK_20            (1 << 20)
#define GPIO_MASK_21            (1 << 21)
#define GPIO_MASK_22            (1 << 22)
#define GPIO_MASK_23            (1 << 23)
#define GPIO_MASK_24            (1 << 24)
#define GPIO_MASK_25            (1 << 25)
#define GPIO_MASK_26            (1 << 26)
#define GPIO_MASK_27            (1 << 27)
#define GPIO_MASK_28            (1 << 28)
#define GPIO_MASK_29            (1 << 29)
#define GPIO_MASK_30            (1 << 30)
#define GPIO_MASK_31            (1 << 31)


#define GET_IRQ_FOR_GPIO(GPIO)  (AR7240_GPIO_IRQ_BASE + (GPIO))
#define GET_GPIO_FOR_IRQ(IRQ)   ((IRQ) - AR7240_GPIO_IRQ_BASE)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _hw_gpio_direction {
    GPIO_OUTPUT_PIN = 0,
    GPIO_INPUT_PIN  = 1
};

enum _hw_gpio_function {
    GPIO_PIN      = 1,
    FUNCTION_PIN  = 0,
    FUNCTION2_PIN = 2,
    FUNCTION3_PIN = 3
};

enum _hw_gpio_polarity {
    GPIO_ACTIVE_HIGH = 0,
    GPIO_ACTIVE_LOW  = 1
};

enum _hw_gpio_sensitivity {
    GPIO_LEVEL_SENSITIVE       = 0,
    GPIO_EDGE_SENSITIVE        = 1,
    GPIO_BOTH_EDGES_SENSITIVE  = 2
};


typedef enum _hw_gpio_direction     GPIO_DIR;
typedef enum _hw_gpio_function      GPIO_MODE;
typedef enum _hw_gpio_polarity      GPIO_POLAR;
typedef enum _hw_gpio_sensitivity   GPIO_SENSE;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ar7240_avm_gpio_init(void);
int ar7240_avm_gpio_ctrl(unsigned int gpio_pin, enum _hw_gpio_function pin_mode, enum _hw_gpio_direction pin_dir);
int ar7240_avm_gpio_out_bit(unsigned int gpio_pin, int value);
int ar7240_avm_gpio_in_bit(unsigned int gpio_pin);
unsigned int ar7240_avm_gpio_in_value(void);
void ar7240_avm_gpio_set_bitmask(unsigned int mask, unsigned int value);

#endif
