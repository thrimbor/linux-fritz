#ifndef _ATHEROS_GPIO_H
#define _ATHEROS_GPIO_H

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


#define GET_IRQ_FOR_GPIO(GPIO)  (ATH_GPIO_IRQ_BASE + (GPIO))
#define GET_GPIO_FOR_IRQ(IRQ)   ((IRQ) - ATH_GPIO_IRQ_BASE)

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _hw_gpio_direction {
    GPIO_OUTPUT_PIN = 0,
    GPIO_INPUT_PIN  = 1,
    GPIO_OUTPUT_INPUT_PIN = 2       /*--- QCA9556 MDIO_DATA ist IN- und OUTPUT ---*/
};
/*--------------------------------------------------------------------------------*\
 * falls das enum FUNCTION_PIN verwendet wird, so wird
 * die Funktion aus der hw-gpio-table (s.u.: gpio_func_hwxx) verwendet
 * Ausnahmen stellen
 * a) TDM_FS und TDM_CLK dar,  da diese auch im Mastermode betrieben werden koennen (-> zusaetzliche enums)
 * b) SPDIF_OUT und SPI_DAC_CS dar, da diese geshared sind
\*--------------------------------------------------------------------------------*/
enum _hw_gpio_function {
    GPIO_PIN      = 1,
    FUNCTION_PIN  = 0,
#ifdef CONFIG_MACH_AR724x
    FUNCTION2_PIN = 2,
    FUNCTION3_PIN = 3,
#elif defined(CONFIG_MACH_AR934x) || defined(CONFIG_MACH_QCA955x)
    /*--- mit dem Wasp koennen beliebige Funktionen auf beliebige Pins multiplexed werden ---*/
    FUNCTION_TDM_FS    = 2,
    FUNCTION_TDM_CLK   = 3,
    FUNCTION_SPDIF_OUT = 4,
#endif/*--- #elif defined(CONFIG_MACH_AR934x) ---*/
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


struct _gpio_function {
    int func;
    enum _hw_gpio_direction dir;
};

struct _gpio_func_table {
    unsigned int hwrev;
    struct _gpio_function *table;
    unsigned int size;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ath_avm_gpio_init(void);
int ath_avm_gpio_ctrl(unsigned int gpio_pin, enum _hw_gpio_function pin_mode, enum _hw_gpio_direction pin_dir);
int ath_avm_gpio_out_bit(unsigned int gpio_pin, int value);
int ath_avm_gpio_in_bit(unsigned int gpio_pin);
unsigned int ath_avm_gpio_in_value(void);
void ath_avm_gpio_set_bitmask(unsigned int mask, unsigned int value);



#ifdef GPIO_FUNCTION_TABLE
#include <atheros.h>
/*------------------------------------------------------------------------------------------*\
 * GPIO KONFIGURATIONEN NACH HW-Revision
 * TODO: 
 * - Wer ist noch GPIO_INPUT_PIN ?
 * Achtung: Für GPIO_IN_SPI_MISO und GPIO_IN_UART0_SIN nur GPIO-Pins < 18 erlaubt
\*------------------------------------------------------------------------------------------*/


#define IGNORE_FUNCTION  -1
#define NO_FUNCTION      -2

#ifdef CONFIG_MACH_AR934x
/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box ATH DB120
\*------------------------------------------------------------------------------------------*/
struct _gpio_function gpio_func_hw96[] = {
    { IGNORE_FUNCTION,     GPIO_OUTPUT_PIN },  /*--- GPIO  0:  JTCK                     ---*/
    { IGNORE_FUNCTION,     GPIO_INPUT_PIN  },  /*--- GPIO  1:  JTDI                     ---*/
    { IGNORE_FUNCTION,     GPIO_OUTPUT_PIN },  /*--- GPIO  2:  JTDO                     ---*/
    { IGNORE_FUNCTION,     GPIO_OUTPUT_PIN },  /*--- GPIO  3:  JTMS                     ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN },  /*--- GPIO  4:  S17_INTn                 ---*/
    { GPIO_OUT_SPI_CS_0,   GPIO_OUTPUT_PIN },  /*--- GPIO  5:  SPI_CS_L                 ---*/
    { GPIO_OUT_SPI_CLK,    GPIO_OUTPUT_PIN },  /*--- GPIO  6:  SPI_CLK                  ---*/
    { GPIO_OUT_SPI_MOSI,   GPIO_OUTPUT_PIN },  /*--- GPIO  7:  SPI_MOSI                 ---*/
    { GPIO_IN_SPI_MISO,    GPIO_INPUT_PIN  },  /*--- GPIO  8:  SPI_MISO /Pin15 von JP3  ---*/
    { GPIO_IN_UART0_SIN,   GPIO_INPUT_PIN  },  /*--- GPIO  9:  UART_SIN                 ---*/
    { GPIO_OUT_UART0_SOUT, GPIO_OUTPUT_PIN },  /*--- GPIO 10:  UART_SOUT                ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN },  /*--- GPIO 11:  USB_LED                  ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN },  /*--- GPIO 12:  AR9344_5G_WLAN_LED       ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN },  /*--- GPIO 13:  AR9344_2G_WLAN_LED       ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN },  /*--- GPIO 14:  RDY_STATUS_LED           ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN },  /*--- GPIO 15:  JUMP_START_LED           ---*/
    { NO_FUNCTION,         GPIO_INPUT_PIN  },  /*--- GPIO 16:  JUMP_ST_SW               ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN },  /*--- GPIO 17:  SWRST/WAKE_EP            ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN },  /*--- GPIO 18:  INTERNET_LED             ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN },  /*--- GPIO 19:  LED_LINK_1               ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN },  /*--- GPIO 20:  LED_LINK_2               ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN },  /*--- GPIO 21:  LED_LINK_3               ---*/
    { NO_FUNCTION,         GPIO_OUTPUT_PIN }   /*--- GPIO 22:  LED_LINK_4/WAKE_RC       ---*/
};

/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box 6810 LTE (ab C-Karten)
\*------------------------------------------------------------------------------------------*/
struct _gpio_function gpio_func_hw180[] = {
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  0:  JTCK            ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO  1:  JTDI            ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  2:  JTDO            ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  3:  JTMS            ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO  4:  WLAN_TASTER     ---*/
    { GPIO_OUT_SPI_CS_0,      GPIO_OUTPUT_PIN },  /*--- GPIO  5:  SPI_CS          ---*/
    { GPIO_OUT_SPI_CLK,       GPIO_OUTPUT_PIN },  /*--- GPIO  6:  SPI_CLK         ---*/
    { GPIO_OUT_SPI_MOSI,      GPIO_OUTPUT_PIN },  /*--- GPIO  7:  SPI_MOSI        ---*/
    { GPIO_IN_SPI_MISO,       GPIO_INPUT_PIN  },  /*--- GPIO  8:  SPI_MISO        ---*/
    { GPIO_IN_UART0_SIN,      GPIO_INPUT_PIN  },  /*--- GPIO  9:  UART_I_DECT     ---*/
    { GPIO_OUT_UART0_SOUT,    GPIO_OUTPUT_PIN },  /*--- GPIO 10:  UART_SOUT       ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 11:  /DECT_RSTN      ---*/
    { GPIO_IN_SLIC_PCM_FS,    GPIO_INPUT_PIN  },  /*--- GPIO 12:  TDM_FSC (slave) ---*/
    { GPIO_IN_I2S_MCLK,       GPIO_INPUT_PIN  },  /*--- GPIO 13:  TDM_DCL (slave) ---*/
    { GPIO_OUT_SLIC_DATA_OUT, GPIO_OUTPUT_PIN },  /*--- GPIO 14:  TDM_DO          ---*/
    { GPIO_IN_SLIC_DATA_IN,   GPIO_INPUT_PIN  },  /*--- GPIO 15:  TDM_DI          ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 16:  DECT_TASTER     ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 17:  /DECT_INT       ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 18:  LED1            ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 19:  LED2            ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 20:  LED3            ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 21:  LED4            ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN }   /*--- GPIO 22:  LED5            ---*/
};

/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box 6841 LTE (ab C-Karten)
\*------------------------------------------------------------------------------------------*/
struct _gpio_function gpio_func_hw184[] = {
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  0:  JTCK            ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO  1:  JTDI            ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  2:  JTDO            ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  3:  JTMS            ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO  4:  WLAN_TASTER     ---*/
    { GPIO_OUT_SPI_CS_0,      GPIO_OUTPUT_PIN },  /*--- GPIO  5:  SPI_CS          ---*/
    { GPIO_OUT_SPI_CLK,       GPIO_OUTPUT_PIN },  /*--- GPIO  6:  SPI_CLK         ---*/
    { GPIO_OUT_SPI_MOSI,      GPIO_OUTPUT_PIN },  /*--- GPIO  7:  SPI_MOSI        ---*/
    { GPIO_IN_SPI_MISO,       GPIO_INPUT_PIN  },  /*--- GPIO  8:  SPI_MISO        ---*/
    { GPIO_IN_UART0_SIN,      GPIO_INPUT_PIN  },  /*--- GPIO  9:  UART_I_DECT     ---*/
    { GPIO_OUT_UART0_SOUT,    GPIO_OUTPUT_PIN },  /*--- GPIO 10:  UART_SOUT       ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 11:  ETHER_INT       ---*/
    { GPIO_IN_SLIC_PCM_FS,    GPIO_INPUT_PIN  },  /*--- GPIO 12:  TDM_FSC (slave) ---*/
    { GPIO_IN_I2S_MCLK,       GPIO_INPUT_PIN  },  /*--- GPIO 13:  TDM_DCL (slave) ---*/
    { GPIO_OUT_SLIC_DATA_OUT, GPIO_OUTPUT_PIN },  /*--- GPIO 14:  TDM_DO          ---*/
    { GPIO_IN_SLIC_DATA_IN,   GPIO_INPUT_PIN  },  /*--- GPIO 15:  TDM_DI          ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 16:  DECT_TASTER     ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 17:                  ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 18:  /ETHER_RESET    ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 19:  /RESET_USB      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 20:  SDCLK           ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 21:  /DECT_RSTN      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN }   /*--- GPIO 22:  SDIN            ---*/
};

/*------------------------------------------------------------------------------------------*\
 * FRITZ!Powerline 546E
\*------------------------------------------------------------------------------------------*/
struct _gpio_function gpio_func_hw190[] = {
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  0:                      ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO  1:                      ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  2:                      ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  3:                      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO  4:  PB_Mode             ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  5:                      ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  6:                      ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  7:                      ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO  8:                      ---*/
    { GPIO_IN_UART0_SIN,      GPIO_INPUT_PIN  },  /*--- GPIO  9:  UART1_RX (Debug)    ---*/
    { GPIO_OUT_UART0_SOUT,    GPIO_OUTPUT_PIN },  /*--- GPIO 10:  UART1_TX (Debug)    ---*/
    { GPIO_OUT_UART1_RTS,     GPIO_OUTPUT_PIN },  /*--- GPIO 11:  UART2 RTS Flow control ---*/
    { GPIO_IN_UART1_CTS,      GPIO_INPUT_PIN  },  /*--- GPIO 12:  UART2 CTS Flow control ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 13:  LED_WLAN            ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 14:  POWER_TASTER        ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 15:  /PB_Reset           ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 16:  PLC_TASTER          ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 17:  LED_POWER           ---*/
    { GPIO_IN_UART1_RD,       GPIO_INPUT_PIN  },  /*--- GPIO 18:  UART2_RX (Prolific) ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 19:  LED_PLC             ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 20:  /RESET_PLC          ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 21:  WLAN_TASTER         ---*/
    { GPIO_OUT_UART1_TD,      GPIO_OUTPUT_PIN }   /*--- GPIO 22:  UART2_TX (Prolific) ---*/
    /*--- { GPIO_OUT_UART0_SOUT,      GPIO_OUTPUT_PIN } ---*/   /*--- GPIO 22:  UART2_TX (Prolific) ---*/
};

/*------------------------------------------------------------------------------------------*\
 * FRITZ!Repeater 310
\*------------------------------------------------------------------------------------------*/
struct _gpio_function gpio_func_hw194[] = {
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  0:  JTCK         ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO  1:  JTDI         ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  2:  JTDO         ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  3:  JTMS         ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO  4:               ---*/
    { GPIO_OUT_SPI_CS_0,      GPIO_OUTPUT_PIN },  /*--- GPIO  5:  SPI_CS       ---*/
    { GPIO_OUT_SPI_CLK,       GPIO_OUTPUT_PIN },  /*--- GPIO  6:  SPI_CLK      ---*/
    { GPIO_OUT_SPI_MOSI,      GPIO_OUTPUT_PIN },  /*--- GPIO  7:  SPI_MOSI     ---*/
    { GPIO_IN_SPI_MISO,       GPIO_INPUT_PIN  },  /*--- GPIO  8:  SPI_MISO     ---*/
    { GPIO_IN_UART0_SIN,      GPIO_INPUT_PIN  },  /*--- GPIO  9:  UART_RX      ---*/
    { GPIO_OUT_UART0_SOUT,    GPIO_OUTPUT_PIN },  /*--- GPIO 10:  UART_TX      ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO 11:               ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO 12:               ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 13:  FSI_LED0     ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 14:  FSI_LED1     ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 15:  FSI_LED2     ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 16:  FSI_LED3     ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 17:  FSI_LED4     ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO 18:               ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 19:  LED_WLAN     ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 20:  LED_POWER    ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO 21:               ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  }   /*--- GPIO 22:  WPS_TASTER   ---*/
};

/*------------------------------------------------------------------------------------------*\
 * FRITZ!Box 6842 LTE
\*------------------------------------------------------------------------------------------*/
struct _gpio_function gpio_func_hw195[] = {
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  0:  JTCK            ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO  1:  JTDI            ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  2:  JTDO            ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  3:  JTMS            ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO  4:  WLAN_TASTER     ---*/
    { GPIO_OUT_SPI_CS_0,      GPIO_OUTPUT_PIN },  /*--- GPIO  5:  SPI_CS          ---*/
    { GPIO_OUT_SPI_CLK,       GPIO_OUTPUT_PIN },  /*--- GPIO  6:  SPI_CLK         ---*/
    { GPIO_OUT_SPI_MOSI,      GPIO_OUTPUT_PIN },  /*--- GPIO  7:  SPI_MOSI        ---*/
    { GPIO_IN_SPI_MISO,       GPIO_INPUT_PIN  },  /*--- GPIO  8:  SPI_MISO        ---*/
    { GPIO_IN_UART0_SIN,      GPIO_INPUT_PIN  },  /*--- GPIO  9:  UART_I_DECT     ---*/
    { GPIO_OUT_UART0_SOUT,    GPIO_OUTPUT_PIN },  /*--- GPIO 10:  UART_SOUT       ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 11:  ETHER_INT       ---*/
    { GPIO_IN_SLIC_PCM_FS,    GPIO_INPUT_PIN  },  /*--- GPIO 12:  TDM_FSC (slave) ---*/
    { GPIO_IN_I2S_MCLK,       GPIO_INPUT_PIN  },  /*--- GPIO 13:  TDM_DCL (slave) ---*/
    { GPIO_OUT_SLIC_DATA_OUT, GPIO_OUTPUT_PIN },  /*--- GPIO 14:  TDM_DO          ---*/
    { GPIO_IN_SLIC_DATA_IN,   GPIO_INPUT_PIN  },  /*--- GPIO 15:  TDM_DI          ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 16:  DECT_TASTER     ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 17:                  ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 18:  LTE_PWR_EN      ---*/
    { GPIO_OUT_ATT_LED,       GPIO_OUTPUT_PIN },  /*--- GPIO 19:  XLNA_BIAS0      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 20:  SDCLK           ---*/
    { GPIO_OUT_PWR_LED,       GPIO_OUTPUT_PIN },  /*--- GPIO 21:  XLNA_BIAS1      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN }   /*--- GPIO 22:  SDIN            ---*/
};

/*------------------------------------------------------------------------------------------*\
 * AVM 6842 LTE (identisch HW195)
\*------------------------------------------------------------------------------------------*/
struct _gpio_function gpio_func_hw207[] = {
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  0:  JTCK            ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO  1:  JTDI            ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  2:  JTDO            ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  3:  JTMS            ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO  4:  WLAN_TASTER     ---*/
    { GPIO_OUT_SPI_CS_0,      GPIO_OUTPUT_PIN },  /*--- GPIO  5:  SPI_CS          ---*/
    { GPIO_OUT_SPI_CLK,       GPIO_OUTPUT_PIN },  /*--- GPIO  6:  SPI_CLK         ---*/
    { GPIO_OUT_SPI_MOSI,      GPIO_OUTPUT_PIN },  /*--- GPIO  7:  SPI_MOSI        ---*/
    { GPIO_IN_SPI_MISO,       GPIO_INPUT_PIN  },  /*--- GPIO  8:  SPI_MISO        ---*/
    { GPIO_IN_UART0_SIN,      GPIO_INPUT_PIN  },  /*--- GPIO  9:  UART_I_DECT     ---*/
    { GPIO_OUT_UART0_SOUT,    GPIO_OUTPUT_PIN },  /*--- GPIO 10:  UART_SOUT       ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 11:  ETHER_INT       ---*/
    { GPIO_IN_SLIC_PCM_FS,    GPIO_INPUT_PIN  },  /*--- GPIO 12:  TDM_FSC (slave) ---*/
    { GPIO_IN_I2S_MCLK,       GPIO_INPUT_PIN  },  /*--- GPIO 13:  TDM_DCL (slave) ---*/
    { GPIO_OUT_SLIC_DATA_OUT, GPIO_OUTPUT_PIN },  /*--- GPIO 14:  TDM_DO          ---*/
    { GPIO_IN_SLIC_DATA_IN,   GPIO_INPUT_PIN  },  /*--- GPIO 15:  TDM_DI          ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 16:  DECT_TASTER     ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 17:                  ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 18:  LTE_PWR_EN      ---*/
    { GPIO_OUT_ATT_LED,       GPIO_OUTPUT_PIN },  /*--- GPIO 19:  XLNA_BIAS0      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 20:  SDCLK           ---*/
    { GPIO_OUT_PWR_LED,       GPIO_OUTPUT_PIN },  /*--- GPIO 21:  XLNA_BIAS1      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN }   /*--- GPIO 22:  SDIN            ---*/
};

/*------------------------------------------------------------------------------------------*\
 * FRITZ!Powerline 540E
\*------------------------------------------------------------------------------------------*/
struct _gpio_function gpio_func_hw201[] = {
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  0:                      ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO  1:                      ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  2:                      ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  3:                      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO  4:  PB_Mode             ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  5:                      ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  6:                      ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN },  /*--- GPIO  7:                      ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN  },  /*--- GPIO  8:                      ---*/
    { GPIO_IN_UART0_SIN,      GPIO_INPUT_PIN  },  /*--- GPIO  9:  UART1_RX (Debug)    ---*/
    { GPIO_OUT_UART0_SOUT,    GPIO_OUTPUT_PIN },  /*--- GPIO 10:  UART1_TX (Debug)    ---*/
    { GPIO_OUT_UART1_RTS,     GPIO_OUTPUT_PIN },  /*--- GPIO 11:  UART2 RTS Flow control ---*/
    { GPIO_IN_UART1_CTS,      GPIO_INPUT_PIN  },  /*--- GPIO 12:  UART2 CTS Flow control ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 13:  LED_WLAN            ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 14:                      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 15:  /PB_Reset           ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 16:  PLC_TASTER          ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 17:                      ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 18:                      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 19:  LED_PLC             ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN },  /*--- GPIO 20:  /RESET_PLC          ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN  },  /*--- GPIO 21:  WLAN_TASTER         ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN }   /*--- GPIO 22:                      ---*/
    /*--- { GPIO_OUT_UART0_SOUT,      GPIO_OUTPUT_PIN } ---*/   /*--- GPIO 22:  UART2_TX (Prolific) ---*/
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _gpio_func_table gpio_func_tables[] = {
    {  96, gpio_func_hw96,  23 }, /*--- FRITZ!Box ATH DB120 ---*/
    { 180, gpio_func_hw180, 23 }, /*--- FRITZ!Box 6810 LTE (ab C-Karten) ---*/
    { 184, gpio_func_hw184, 23 }, /*--- FRITZ!Box 6841 LTE ---*/
    { 190, gpio_func_hw190, 23 }, /*--- FRITZ!Powerline 546E ---*/
    { 194, gpio_func_hw194, 23 }, /*--- FRITZ!Repeater 310 ---*/
    { 195, gpio_func_hw195, 23 }, /*--- FRITZ!Box 6842 LTE ---*/
    { 201, gpio_func_hw201, 23 }, /*--- FRITZ!Powerline 540E ---*/
    { 207, gpio_func_hw207, 23 }, /*--- AVM 6842 LTE ---*/
};
#endif /*--- #ifdef CONFIG_MACH_AR934x ---*/

#if defined(CONFIG_MACH_QCA955x) 
/*------------------------------------------------------------------------------------------*\
 * FRITZ!Repeater 450
\*------------------------------------------------------------------------------------------*/
#if 0   
/*--- aufheben für Repeater 460 ---*/
struct _gpio_function gpio_func_hw200[] = {
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  0:  JTCK          ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN        },  /*--- GPIO  1:  JTDI          ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  2:  JTDO          ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  3:  JTMS          ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO  4:  SHIFT_DATA    ---*/
    { GPIO_OUT_SPI_CS_0,      GPIO_OUTPUT_PIN       },  /*--- GPIO  5:  SPI_CS        ---*/
    { GPIO_OUT_SPI_CLK,       GPIO_OUTPUT_PIN       },  /*--- GPIO  6:  SPI_CLK       ---*/
    { GPIO_OUT_SPI_MOSI,      GPIO_OUTPUT_PIN       },  /*--- GPIO  7:  SPI_MOSI      ---*/
    { GPIO_IN_SPI_MISO,       GPIO_INPUT_PIN        },  /*--- GPIO  8:  SPI_MISO      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO  9:  SHIFT_CLK     ---*/
    { GPIO_OUT_UART0_SOUT,    GPIO_OUTPUT_PIN       },  /*--- GPIO 10:  UART_TX       ---*/
    { GPIO_OUT_SPI_CS_1,      GPIO_OUTPUT_PIN       },  /*--- GPIO 11:  SPI_CS_DAC    ---*/
    { GPIO_OUT_GE1_MII_MDC,   GPIO_OUTPUT_PIN       },  /*--- GPIO 12:  MDIO_CLK      ---*/
    { GPIO_IN_UART0_SIN,      GPIO_INPUT_PIN        },  /*--- GPIO 13:  UART_RX       ---*/
    { GPIO_OUT_I2S_MCK,       GPIO_OUTPUT_PIN       },  /*--- GPIO 14:  I2S_MCLK      ---*/
    { GPIO_OUT_I2S_CLK,       GPIO_OUTPUT_PIN       },  /*--- GPIO 15:  I2S_REF_CLK   ---*/
    { GPIO_OUT_I2S_WS,        GPIO_OUTPUT_PIN       },  /*--- GPIO 16:  I2S_WS_STEREO ---*/
    { GPIO_OUT_I2S_SD,        GPIO_OUTPUT_PIN       },  /*--- GPIO 17:  I2S_DATA      ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN        },  /*--- GPIO 18:  BUTTON        ---*/
    { GPIO_IN_MII_GE1_MDI|(GPIO_OUT_GE1_MII_MDO<<16),    GPIO_OUTPUT_INPUT_PIN },  /*--- GPIO 19:  MDIO_DATA     ---*/
};
#endif

struct _gpio_function gpio_func_hw200[] = {
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  0:  JTCK          ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN        },  /*--- GPIO  1:  JTDI          ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  2:  JTDO          ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  3:  JTMS          ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO  4:  SHIFT_DATA    ---*/
    { GPIO_OUT_SPI_CS_0,      GPIO_OUTPUT_PIN       },  /*--- GPIO  5:  SPI_CS        ---*/
    { GPIO_OUT_SPI_CLK,       GPIO_OUTPUT_PIN       },  /*--- GPIO  6:  SPI_CLK       ---*/
    { GPIO_OUT_SPI_MOSI,      GPIO_OUTPUT_PIN       },  /*--- GPIO  7:  SPI_MOSI      ---*/
    { GPIO_IN_SPI_MISO,       GPIO_INPUT_PIN        },  /*--- GPIO  8:  SPI_MISO      ---*/
    { GPIO_IN_UART0_SIN,      GPIO_INPUT_PIN        },  /*--- GPIO  9:  UART_RX       ---*/
    { GPIO_OUT_UART0_SOUT,    GPIO_OUTPUT_PIN       },  /*--- GPIO 10:  UART_TX       ---*/

    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO 11:  eth_reset     ---*/
    { GPIO_OUT_GE1_MII_MDC,   GPIO_OUTPUT_PIN       },  /*--- GPIO 12:  MDIO_CLK      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 13:  FSI_LED0      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 14:  LED_POWER     ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 15:  FSI_LED1      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 16:  FSI_LED2      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 17:  FSI_LED3      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 18:  FSI_LED4      ---*/
    { GPIO_IN_MII_GE1_MDI|(GPIO_OUT_GE1_MII_MDO<<16),    GPIO_OUTPUT_INPUT_PIN },  /*--- GPIO 19:  MDIO_DATA     ---*/
};

struct _gpio_function gpio_func_hw205[] = {
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  0:  JTCK          ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN        },  /*--- GPIO  1:  JTDI          ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  2:  JTDO          ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  3:  JTMS          ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO  4:  SHIFT_DATA    ---*/
    { GPIO_OUT_SPI_CS_0,      GPIO_OUTPUT_PIN       },  /*--- GPIO  5:  SPI_CS        ---*/
    { GPIO_OUT_SPI_CLK,       GPIO_OUTPUT_PIN       },  /*--- GPIO  6:  SPI_CLK       ---*/
    { GPIO_OUT_SPI_MOSI,      GPIO_OUTPUT_PIN       },  /*--- GPIO  7:  SPI_MOSI      ---*/
    { GPIO_IN_SPI_MISO,       GPIO_INPUT_PIN        },  /*--- GPIO  8:  SPI_MISO      ---*/
    { GPIO_IN_UART0_SIN,      GPIO_INPUT_PIN        },  /*--- GPIO  9:  UART_RX       ---*/
    { GPIO_OUT_UART0_SOUT,    GPIO_OUTPUT_PIN       },  /*--- GPIO 10:  UART_TX       ---*/

    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO 11:  eth_reset     ---*/
    { GPIO_OUT_GE1_MII_MDC,   GPIO_OUTPUT_PIN       },  /*--- GPIO 12:  MDIO_CLK      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 13:  FSI_LED0      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 14:  LED_POWER     ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 15:  FSI_LED1      ---*/
    { GPIO_IN_I2C_CLK | (GPIO_OUT_I2C_CLK << 16),        GPIO_OUTPUT_INPUT_PIN       },  /*--- GPIO 16:  I2C CLK      ---*/
    { GPIO_IN_I2C_DATA | (GPIO_OUT_I2C_DATA << 16),       GPIO_OUTPUT_INPUT_PIN       },  /*--- GPIO 17:  I2C DATA      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 18:  FSI_LED4      ---*/
    { GPIO_IN_MII_GE1_MDI|(GPIO_OUT_GE1_MII_MDO<<16),    GPIO_OUTPUT_INPUT_PIN },  /*--- GPIO 19:  MDIO_DATA     ---*/
};

struct _gpio_function gpio_func_hw206[] = {
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  0:  JTCK          ---*/
    { IGNORE_FUNCTION,        GPIO_INPUT_PIN        },  /*--- GPIO  1:  JTDI          ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  2:  JTDO          ---*/
    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO  3:  JTMS          ---*/
    { NO_FUNCTION,            GPIO_INPUT_PIN        },  /*--- GPIO  4:  SHIFT_DATA    ---*/
    { GPIO_OUT_SPI_CS_0,      GPIO_OUTPUT_PIN       },  /*--- GPIO  5:  SPI_CS        ---*/
    { GPIO_OUT_SPI_CLK,       GPIO_OUTPUT_PIN       },  /*--- GPIO  6:  SPI_CLK       ---*/
    { GPIO_OUT_SPI_MOSI,      GPIO_OUTPUT_PIN       },  /*--- GPIO  7:  SPI_MOSI      ---*/
    { GPIO_IN_SPI_MISO,       GPIO_INPUT_PIN        },  /*--- GPIO  8:  SPI_MISO      ---*/
    { GPIO_IN_UART0_SIN,      GPIO_INPUT_PIN        },  /*--- GPIO  9:  UART_RX       ---*/
    { GPIO_OUT_UART0_SOUT,    GPIO_OUTPUT_PIN       },  /*--- GPIO 10:  UART_TX       ---*/

    { IGNORE_FUNCTION,        GPIO_OUTPUT_PIN       },  /*--- GPIO 11:  eth_reset     ---*/
    { GPIO_OUT_GE1_MII_MDC,   GPIO_OUTPUT_PIN       },  /*--- GPIO 12:  MDIO_CLK      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 13:  LED_POWER     ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 14:  SHIFT_DATA    ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 15:  SHIFT_CLK     ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 16:  NC            ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 17:  PERE_RST      ---*/
    { NO_FUNCTION,            GPIO_OUTPUT_PIN       },  /*--- GPIO 18:  PCIE_RST      ---*/
    { GPIO_IN_MII_GE1_MDI|(GPIO_OUT_GE1_MII_MDO<<16),    GPIO_OUTPUT_INPUT_PIN },  /*--- GPIO 19:  MDIO_DATA     ---*/
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _gpio_func_table gpio_func_tables[] = {
    { 200, gpio_func_hw200, 20 }, /*--- FRITZ!Repeater 450 ---*/
    { 205, gpio_func_hw205, 20 }, /*--- FRITZ!Repeater DVBC ---*/
    { 206, gpio_func_hw206, 20 }, /*--- FRITZ!Repeater 11AC ---*/
};
#endif /*--- #if defined(CONFIG_MACH_QCA955x) ---*/ 

#endif /*--- #ifdef GPIO_FUNCTION_TABLE ---*/


#endif
