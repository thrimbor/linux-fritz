
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/prom.h>

#define DEBUG_IFX_DCDC(x)   prom_printf(x)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern void prom_printf(const char * fmt, ...);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int __init final_setup_dcdc(void) {
    DEBUG_IFX_DCDC("[final_setup_dcdc] start\n");
    /*--- 1.  Set DUTY_MAX_SAT=90 and  DUTY_MIN_SAT= 70 ---*/
    *((volatile unsigned char *)0xBF106A17) = 0x5A;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBF106A17) = 0x5A;\n");

    *((volatile unsigned char *)0xBF106A18) = 0x46;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBF106A18) = 0x46;\n");

    /*--- 2.  Set FREEZE_PID= 1 (register “CONF_TEST_DIG”) ---*/	
    *((volatile unsigned char *)0xBF106A11) = 0x13;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBF106A11) = 0x13;\n");

    /*--- 3.  Program the “Low gain coefficients” (6 registers of 8 bits) ---*/
    *((volatile unsigned char *)0xBf106a00) = 0;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBf106a00) = 0;\n");

    *((volatile unsigned char *)0xBf106a01) = 0;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBf106a01) = 0;\n");

    *((volatile unsigned char *)0xBf106a02) = 0xFF;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBf106a02) = 0xFF;\n");

    *((volatile unsigned char *)0xBf106a03) = 0xE6;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBf106a03) = 0xE6;\n");

    *((volatile unsigned char *)0xBf106a04) = 0;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBf106a04) = 0;\n");

    *((volatile unsigned char *)0xBf106a05) = 0x1B;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBf106a05) = 0x1B;\n");

    *((volatile unsigned char *)0xBf106a15) = 0x8b; 
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBf106a15) = 0x8b;\n");

    /*--- 4.  Set DUTY_MAX_SAT=128 and DUTY_MIN_SAT=40 ---*/	
    *((volatile unsigned char *)0xBf106a17) = 0x80;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBf106a17) = 0x80;\n");

    *((volatile unsigned char *)0xBf106a18) = 0x28;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBf106a18) = 0x28;\n");

    *((volatile unsigned char *)0xBf106a06) = 0x52;
    DEBUG_IFX_DCDC("*((volatile unsigned char *)0xBf106a06) = 0x52;\n");

    /*--- 5.  Set FREEZE_PID=0 ---*/	
    *((volatile unsigned char *)0xBF106A11) = 0x03;
    DEBUG_IFX_DCDC("[final_setup_dcdc] done\n");

#if 0
    {
        unsigned char value = 
            /*--- (1 << 6) | ---*/  /*--- increase current load for internal regulator ---*/
            /*--- (1 << 5) | ---*/  /*--- reduce bais current for voltage regulator ---*/
            /*--- (0 << 3) | ---*/  /*--- 0,930 Volt ---*/
            (1 << 3) |  /*--- 1,000 Volt ---*/
            /*--- (2 << 3) | ---*/  /*--- 1,050 Volt ---*/
            /*--- (3 << 3) | ---*/  /*--- 1,175 Volt ---*/
            (0 << 0) |  /*--- +- 0% ---*/
            /*--- (1 << 0) | ---*/  /*--- - 1,25% ---*/
            /*--- (2 << 0) | ---*/  /*--- - 2,50% ---*/
            /*--- (3 << 0) | ---*/  /*--- - 3,75% ---*/
            /*--- (4 << 0) | ---*/  /*--- + 1,25% ---*/
            /*--- (5 << 0) | ---*/  /*--- + 2,50% ---*/
            /*--- (6 << 0) | ---*/  /*--- + 3,75% ---*/
            /*--- (7 << 0) | ---*/  /*--- + 5,00% ---*/
            0;
        *((volatile unsigned char *)0xBF106A0a) = value;
        value = 0x7F; /*--- 1,00 Volt ---*/
        /*--- value = 0x86; ---*/ /*--- 1,05 Volt ---*/
        /*--- value = 0x8C; ---*/ /*--- 1,10 Volt ---*/
        *((volatile unsigned char *)0xBF106A0b) = value;
        prom_printf("[adjust core voltage done] done\n");
    }
#endif

    return 0;
}

core_initcall(final_setup_dcdc);
