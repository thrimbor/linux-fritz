#ifndef _ikan_wdt_h_
#define _ikan_wdt_h_

#if !defined(CONFIG_FUSIV_VX185)
#define FUSIV_WDT_CTRL          ((volatile unsigned int *)0xB9000008)
#define FUSIV_WDT_CTRL_DISABLE  (1 << 0)
#define FUSIV_WDT_CTRL_PRESCALE (1 << 1)
#define FUSIV_WDT_CTRL_SERVICE  (0xA << 8)
#define FUSIV_WDT_CTRL_RELOAD(x) ((x) << 16)

#define FUSIV_WDT_STAT          ((volatile unsigned int *)0xB900000C)
#define FUSIV_WDT_STAT_PREWARN  (1 << 0)
#define FUSIV_WDT_STAT_DISABLED (1 << 1)
#define FUSIV_WDT_STAT_ERROR    (1 << 2)

#endif/*--- #if !defined(CONFIG_FUSIV_VX185) ---*/

#endif /*--- #ifndef _ikan_wdt_h_ ---*/
