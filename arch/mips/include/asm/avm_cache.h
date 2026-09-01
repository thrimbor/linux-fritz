#ifndef _AVM_CACHE_H_
#define _AVM_CACHE_H_

#include <asm/prom.h>
#include <asm/pgtable.h>

struct _avm_cache_setting {
    unsigned int hwrev;
    unsigned int mode;
};

/*------------------------------------------------------------------------------------------*\
 * defines in include/asm/mipsregs.h:
 * #define CONF_CM_CACHABLE_NO_WA		0  => write through
 * #define CONF_CM_CACHABLE_NONCOHERENT	3  => write back allocate
\*------------------------------------------------------------------------------------------*/
struct _avm_cache_setting avm_cache_settings[] = {
/*--- UR8 ---*/
    { 135, CONF_CM_CACHABLE_NO_WA },       /*--- Speedport W 920V ---*/
    { 136, CONF_CM_CACHABLE_NO_WA },       /*--- Speedport W 503V ---*/
    { 137, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box WLAN 3270 ---*/
    { 138, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!WLAN Repeater N/G ---*/
    { 139, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7270 v2 16MB ---*/
    { 141, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7270 Annex A ---*/
    { 143, CONF_CM_CACHABLE_NO_WA },       /*--- Speedport W 101 Bridge ---*/
    { 144, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7240 ---*/
    { 145, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7270 v3 ---*/
    { 146, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7570 vDSL ---*/
    { 147, CONF_CM_CACHABLE_NO_WA },       /*--- DSL-EasyBox A802 ---*/
    { 148, CONF_CM_CACHABLE_NO_WA },       /*--- DSL-EasyBox A602 ---*/
    { 153, CONF_CM_CACHABLE_NO_WA },       /*--- Alice IAD 7570 vDSL ---*/
    { 154, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7212 ---*/
    { 160, CONF_CM_CACHABLE_NO_WA },       /*--- Speedport W 504V ---*/
    { 164, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 504avm ---*/
    { 167, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7270 v4 ---*/
    { 168, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box WLAN 3270 v3 ---*/
    { 170, CONF_CM_CACHABLE_NO_WA },       /*--- Speedport W 503V AVM ---*/
    { 197, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box WLAN 3270 v3 Edition Italia ---*/
/*--- IKS ---*/
    { 150, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Ikanos ---*/
    { 152, CONF_CM_CACHABLE_NONCOHERENT }, /*--- Speedport W 722V ---*/
    { 156, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Fon WLAN 7390 ---*/
    { 165, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Fon WLAN 7541 vDSL ---*/
    { 171, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Fon WLAN 7340 ---*/
/*--- AR9 ---*/
    { 161, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Bene ---*/
    { 172, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7320 ---*/
    { 178, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7313 ---*/
    { 179, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7330 ---*/
    { 188, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7330 SL ---*/
    { 189, CONF_CM_CACHABLE_NO_WA },       /*--- FRITZ!Box Fon WLAN 7312 ---*/
/*--- VR9 ---*/
    { 175, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box WLAN 3370 ---*/
    { 177, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box 6840 LTE ---*/
    { 181, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Fon WLAN 7360 SL ---*/
    { 183, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Fon WLAN 7360 ---*/
    { 185, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Fon WLAN 7391 ---*/
    { 193, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Fon WLAN 3390 ---*/
    { 196, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Fon WLAN 7360v2 ---*/
    { 202, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Fon WLAN 7362 ---*/
    { 203, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Fon WLAN 7362 SL ---*/
/*--- AR10 ---*/
    { 198, CONF_CM_CACHABLE_NONCOHERENT }, /*--- Italo-R ---*/
    { 192, CONF_CM_CACHABLE_NONCOHERENT }, /*--- Italo ---*/
/*--- VIRIAN ---*/
    { 173, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!WLAN Repeater 300E ---*/
/*--- WASP ---*/
    { 180, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box 6810 LTE ---*/
    { 184, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box 6841 LTE ---*/
    { 190, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Powerline 546E ---*/
    { 194, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Repeater 310 ---*/
    { 195, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box 6842 LTE ---*/
    { 96,  CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box ATH DB120 ---*/
    { 201, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box Powerline 546E ---*/
/*--- Scorpian ---*/
    { 200, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Repeater 450 ---*/
    { 205, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Repeater DVB-C ---*/
    { 206, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Repeater 11ac ---*/
    { 207, CONF_CM_CACHABLE_NONCOHERENT }, /*--- FRITZ!Box AVM 6842 LTE ---*/
};


/*------------------------------------------------------------------------------------------*\
 * Configure Cache Coherency policy for KSEG0 depending on HWRevision
\*------------------------------------------------------------------------------------------*/
static inline void avm_cache_set_coherency(void)
{
    char *s, *p;
    unsigned int i, hwrev = 0;

    s = prom_getenv("HWRevision");
    if (s) {
		hwrev = simple_strtoul(s, &p, 10);
	} else {
	  printk("[%s] error: no HWRevision detected in environment variables\n",__FUNCTION__);
	  BUG_ON(1);
	}

    for(i= 0; i < sizeof(avm_cache_settings)/sizeof(struct _avm_cache_setting); i++) {
        if(hwrev == avm_cache_settings[i].hwrev) {
            printk("[%s]: setting cache coherency for HWRevision=%d to %s \n", __FUNCTION__, hwrev,
                         avm_cache_settings[i].mode == CONF_CM_CACHABLE_NO_WA ? "write through" : "write back allocate");
	        _page_cachable_default = avm_cache_settings[i].mode << _CACHE_SHIFT;
            change_c0_config(CONF_CM_CMASK, avm_cache_settings[i].mode);
            return;
        }
    }

    panic("[%s]: Cache coherency not set for HWRevision %d in avm_cache.h\n", __FUNCTION__, hwrev);
}


#endif /*--- #ifndef _AVM_CACHE_H_ ---*/
