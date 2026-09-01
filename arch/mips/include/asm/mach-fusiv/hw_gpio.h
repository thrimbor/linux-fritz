/*------------------------------------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------------------------------------*/
#ifndef _hw_gpio_h_
#define _hw_gpio_h_

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
/* GPIO registers */
/*--- #define GPIO_R_IN           (*(volatile unsigned int *)(UR8_GPIO_BASE+0x0)) ---*/
/*--- #define GPIO_R_OUT   		(*(volatile unsigned int *)(UR8_GPIO_BASE+0x4)) ---*/
/*--- #define GPIO_R_DIR   		(*(volatile unsigned int *)(UR8_GPIO_BASE+0x8)) ---*/     /* 0=o/p, 1=i/p */
/*--- #define GPIO_R_EN    		(*(volatile unsigned int *)(UR8_GPIO_BASE+0xc)) ---*/     /* 0=GPIO Mux 1=GPIO */
/*--- #define GPIO_VERSION        (*(volatile unsigned int *)(UR8_GPIO_BASE+0x14)) ---*/

/*--- #define UR8_CVR (*(volatile unsigned int *)(UR8_GPIO_BASE+0x14)) ---*/ /* see _chip_version */


#define GPIO_BITS               32

#define GPIO_BIT_SPI_ERR        0   /*--- SPI Master mode input  ---*/
#define GPIO_BIT_SPI_CS1_OUT    1   /*--- SPI Master mode ---*/
#define GPIO_BIT_SPI_CS2_OUT    2   /*--- SPI Master mode ---*/
#define GPIO_BIT_UART2_TXD      3 
#define GPIO_BIT_UART2_RXD      4 
#define GPIO_BIT_UART2_CTS      5 
#define GPIO_BIT_UART2_RTS      6 
#define GPIO_BIT_DSP1_PF2       7   /*--- NOTE: GPIO 7 to GPIO 10 are connected to PF flags. ---*/
#define GPIO_BIT_DSP1_PF3       8 
#define GPIO_BIT_DSP2_PF2       9 
#define GPIO_BIT_DSP2_PF3       10
#define GPIO_BIT_SPI_CS2        11
#define GPIO_BIT_SPI_CS3        12
#define GPIO_BIT_DYING_GASP     13  /*--- NOTE: GPIO 13 is reserved for Dying Gasp function. ---*/
#define GPIO_BIT_14             14
#define GPIO_BIT_15             15  /* Ethernet switch */
#define GPIO_BIT_16             16  /*--- / VDSL PHY GPIO 0 ---*/
#define GPIO_BIT_17             17  /*--- / VDSL PHY GPIO 1 ---*/
#define GPIO_BIT_18             18  /*--- / VDSL PHY GPIO 2 ---*/
#define GPIO_BIT_19             19  /*--- / VDSL PHY GPIO 3 ---*/
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


#define GPIO_MASK_SPI_ERR       (1 << 0)
#define GPIO_MASK_SPI_CS1_OUT   (1 << 1 )
#define GPIO_MASK_SPI_CS2_OUT   (1 << 2 )
#define GPIO_MASK_UART2_TXD     (1 << 3 )
#define GPIO_MASK_UART2_RXD     (1 << 4 )
#define GPIO_MASK_UART2_CTS     (1 << 5 )
#define GPIO_MASK_UART2_RTS     (1 << 6 )
#define GPIO_MASK_DSP1_PF2      (1 << 7 )
#define GPIO_MASK_DSP1_PF3      (1 << 8 )
#define GPIO_MASK_DSP2_PF2      (1 << 9 )
#define GPIO_MASK_DSP2_PF3      (1 << 10)
#define GPIO_MASK_SPI_CS2       (1 << 11)
#define GPIO_MASK_SPI_CS3       (1 << 12)
#define GPIO_MASK_DYING_GASP    (1 << 13)
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
    unsigned int mask;
    unsigned int is_edge, is_both_edges, is_active_low;
    int delayed_irq;
    int (*func)(unsigned int mask);
    struct _hw_gpio_irqhandle *next;
};

#endif /*--- #ifndef _hw_gpio_h_ ---*/

