#ifndef _consts_h_
#define _consts_h_

#include <linux/version.h>

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#define MAX_CAPI_MESSAGE_SIZE               2048U
#define MAX_CONTROLLERS                     10
/*--- #define NEED_PAGE_LOCK ---*/

/*--- #define CAPI_OSLIB_USE_LOCAL_BUFFERS ---*/        /* copy_from_user und copy_to_user fuer messages verwenden */

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
/*--- hier gibt es leider keine Realtime-Workqueues: Kernelthread mit hoher Prio besser als Workqueue ! ---*/
#define USE_THREAD
#else
/*--- Realtime-Workqueues sind deutlich performanter als Kernelthreads! ---*/
#define USE_WORKQUEUES
#endif
/*--- #define USE_TASKLETS ---*/


#endif /*--- #ifndef _consts_h_ ---*/
#include <linux/pcmlink_ul.h>
