/*------------------------------------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------------------------------------*/
#ifndef _hw_gpio_h_
#define _hw_gpio_h_

enum _hw_gpio_direction {
    GPIO_OUTPUT_PIN = 0,
    GPIO_INPUT_PIN  = 1
};

enum _hw_gpio_function {
    GPIO_PIN     = 1,
    FUNCTION_PIN = 0
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

struct _hw_gpio_irqhandle {
    int enabled;
    unsigned long long mask;
    unsigned int is_edge, is_both_edges, is_active_low;
    int (*func)(unsigned int mask);
    struct _hw_gpio_irqhandle *next;
};

#endif /*--- #ifndef _hw_gpio_h_ ---*/

