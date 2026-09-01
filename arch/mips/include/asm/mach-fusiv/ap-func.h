#ifndef _ASM_MACH_FUSIV_AP_FUNC_H_
#define _ASM_MACH_FUSIV_AP_FUNC_H_


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <netpro/apload.h>
#include <fusiv_types.h>
#include <linux/netdevice.h>
#include <linux/atmdev.h>

struct sk_buff;


/*------------------------------------------------------------------------------------------*\
 * aus ap2apbridge.c
\*------------------------------------------------------------------------------------------*/
extern void ap2apFlowProcess(struct sk_buff *skb, struct net_device *txdev);

/*------------------------------------------------------------------------------------------*\
 * aus apdmem.c
\*------------------------------------------------------------------------------------------*/
simResult_t pmemLoadVx180AATMAP( unsigned long ulAPBaseAddr, 
                                 unsigned char *pProgram, 
                                 unsigned short length);

simResult_t dmemLoadData( unsigned char *pData, unsigned long addr, unsigned short length );

simResult_t dmemZeroData(unsigned long addr, unsigned short length);

/*------------------------------------------------------------------------------------------*\
 * aus apload.c
\*------------------------------------------------------------------------------------------*/
extern unsigned long getAPBaseAddress(unsigned char apId);
extern void  adjustBMAPCode(void);

/*------------------------------------------------------------------------------------------*\
 * appmem.c
\*------------------------------------------------------------------------------------------*/
simResult_t pmemLoad( unsigned long ulAPBaseAddr, unsigned char *pProgram, unsigned short length);

/*------------------------------------------------------------------------------------------*\
 * syscall.c
\*------------------------------------------------------------------------------------------*/
void doDataCacheFlush(void *startAddr, UINT32 length);
INT32 tskSleep(UINT32 ulTimeOut);

/*------------------------------------------------------------------------------------------*\
 * hostap.c
\*------------------------------------------------------------------------------------------*/
void  *mallocSlaveVx180aMem( unsigned long size);
void   *getCluster2(void);
void rdVx180AAtmApIntrptStatusReg( unsigned char apId, unsigned short *val);
simResult_t  apGetStatistics( unsigned char  apId, char *ptr2UploadArea );

/*------------------------------------------------------------------------------------------*\
 * hostroute_syms.c
\*------------------------------------------------------------------------------------------*/
simResult_t dmemVx180AAtmStructExchange(unsigned char apId, 
                                               void *pMemBlock,
                                               unsigned short eventType, 
                                               unsigned short usTimeOut);

simResult_t dmemStructExchange( unsigned char apId, void *pMemBlock,
                                        unsigned short eventType, unsigned short usTimeOut);

/*------------------------------------------------------------------------------------------*\
 * hostconfig.c
\*------------------------------------------------------------------------------------------*/
unsigned long apDymamicReconfig(unsigned char  apId);

simResult_t  apChangeFeature( unsigned char  apId,
	                      unsigned char  feature,
                              unsigned char  mode);
unsigned long apPortInit(unsigned char  apId);

/*------------------------------------------------------------------------------------------*\
 * idmaboot.c:
\*------------------------------------------------------------------------------------------*/
void idmaThread(void);


/*------------------------------------------------------------------------------------------*\
 * initlib.c
\*------------------------------------------------------------------------------------------*/
unsigned char* ap_get_cluster(struct sk_buff *skbuff, int size);
extern unsigned char* (*ap_get_cluster_ptr)(struct sk_buff *skbuff, int size);
void putCluster(void *ulPtr);
extern void   (*putCluster_ptr)(void *ulPtr);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void enableApIntrpt( unsigned char apId, unsigned short val);
void clearApEventOutReg( unsigned char apId, unsigned short val);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern char get_apid_by_name(char *name);

/*------------------------------------------------------------------------------------------*\
 *ap2apmulticastrt.c
\*------------------------------------------------------------------------------------------*/
extern int ap2apMcastRtFillWlanIfs(void* apMcastRft, struct net_device** wlanDevArray, unsigned char *nonApIf);

/*------------------------------------------------------------------------------------------*\
 * ap2aproute.c
\*------------------------------------------------------------------------------------------*/
void ap2apRouteFlowDelete (void *conntrack_entry);

/*------------------------------------------------------------------------------------------*\
 * atmapdriver.c
\*------------------------------------------------------------------------------------------*/
int adiatm_module_init(void);
void adiatm_module_cleanup(struct atm_dev* atm_dev);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(NEED_IPMR_CACHE)
#include <linux/spinlock.h>
#include <linux/mroute.h>
extern struct vif_device vif_table[];
extern int (*ap2apMcastRtPortForwarded_ptr)(struct sk_buff* , int);
extern rwlock_t mrt_lock;
#endif /*--- #if defined(NEED_IPMR_CACHE) ---*/

extern struct mfc_cache *ipmr_cache_find(__be32 origin, __be32 mcastgrp);

/*------------------------------------------------------------------------------------------*\
 * aus atmapif.c
\*------------------------------------------------------------------------------------------*/
unsigned long       apATMUserConfig(unsigned char apId);

/*------------------------------------------------------------------------------------------*\
 * aus: timers.c
\*------------------------------------------------------------------------------------------*/
INT32 aditimerStart(INT32 lTimeOut, VOID (*handler)(VOID *), VOID *arg);



#endif /*--- #ifndef _ASM_MACH_FUSIV_AP_FUNC_H_ ---*/
