/*------------------------------------------------------------------------------------------*\
 *   Copyright (C) 2006,...,2012 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*------------------------------------------------------------------------------------------*/

#ifndef _CPMAC_DEBUG_H_
#define _CPMAC_DEBUG_H_

/* Use the following debug settings */
#undef  CPMAC_DMA_RX_PRIOQUEUE_DEBUG
#undef  CPMAC_DMA_TX_PRIOQUEUE_DEBUG
#undef  CPMAC_SCHED_PRIOQUEUE_DEBUG
#undef  CPMAC_EXTERNAL_COMMAND_DEBUG /* Enable command to call test functions */
#define CPMAC_USE_ASSERT             /* Use assert or comment it out */

/* Use regular debug work item */
#undef  CPMAC_USE_DEBUG_WORK
#define CPMAC_DEBUG_INTERVAL (1 * HZ)

/* Use the following debug levels (these defines exist only in this file!) */
#define CPMAC_USE_DEBUG_LEVEL_SUPPORT   1u
#define CPMAC_USE_DEBUG_LEVEL_ERROR     1u
#define CPMAC_USE_DEBUG_LEVEL_WARNING   1u
/*--- #define CPMAC_USE_DEBUG_LEVEL_INFO      1u ---*/
/*--- #define CPMAC_USE_DEBUG_LEVEL_INFOTRACE 1u ---*/
/*--- #define CPMAC_USE_DEBUG_LEVEL_TRACE     1u ---*/
/*--- #define CPMAC_USE_DEBUG_LEVEL_TEST      1u ---*/
/*--- #define CPMAC_USE_DEBUG_LEVEL_DEBUG     1u ---*/

/* Debug level values */
#define CPMAC_DEBUG_LEVEL_SUPPORT   0x01u
#define CPMAC_DEBUG_LEVEL_ERROR     0x02u
#define CPMAC_DEBUG_LEVEL_WARNING   0x04u
#define CPMAC_DEBUG_LEVEL_INFO      0x08u
#define CPMAC_DEBUG_LEVEL_INFOTRACE 0x10u
#define CPMAC_DEBUG_LEVEL_TRACE     0x20u
#define CPMAC_DEBUG_LEVEL_TEST      0x40u
#define CPMAC_DEBUG_LEVEL_DEBUG     0x80u





/*------------------------------------------------------------------------------------------*\
 * Below this part the above definitions are realized and nothing should be changed there   *
\*------------------------------------------------------------------------------------------*/

/* My own definition of an assert */
#if defined(CPMAC_USE_ASSERT)
#define assert(i) do { \
                      if(unlikely(!(i))) { \
                          restore_printk(); \
                          printk(KERN_ERR "Assertion failed in %s: '"#i"' (at %s:%d)\n", __FUNCTION__, __FILE__, __LINE__); \
                          panic("assert failed!"); \
                      } \
                  } while(0)
#else /*--- #if defined(CPMAC_USE_ASSERT) ---*/
#define assert(exp)
#endif /*--- #else ---*/ /*--- #if defined(CPMAC_USE_ASSERT) ---*/

/* Assert, that the defines used to calculate the DEBUG_LEVEL value exist */
#if !defined(CPMAC_USE_DEBUG_LEVEL_SUPPORT)
#   define CPMAC_USE_DEBUG_LEVEL_SUPPORT 0
#endif
#if !defined(CPMAC_USE_DEBUG_LEVEL_ERROR)
#   define CPMAC_USE_DEBUG_LEVEL_ERROR 0
#endif
#if !defined(CPMAC_USE_DEBUG_LEVEL_WARNING)
#   define CPMAC_USE_DEBUG_LEVEL_WARNING 0
#endif
#if !defined(CPMAC_USE_DEBUG_LEVEL_INFO)
#   define CPMAC_USE_DEBUG_LEVEL_INFO 0
#endif
#if !defined(CPMAC_USE_DEBUG_LEVEL_INFOTRACE)
#   define CPMAC_USE_DEBUG_LEVEL_INFOTRACE 0
#endif
#if !defined(CPMAC_USE_DEBUG_LEVEL_TRACE)
#   define CPMAC_USE_DEBUG_LEVEL_TRACE 0
#endif
#if !defined(CPMAC_USE_DEBUG_LEVEL_TEST)
#   define CPMAC_USE_DEBUG_LEVEL_TEST 0
#endif
#if !defined(CPMAC_USE_DEBUG_LEVEL_DEBUG)
#   define CPMAC_USE_DEBUG_LEVEL_DEBUG 0
#endif

#define CPMAC_DEBUG_LEVEL (  0 \
                           | (CPMAC_USE_DEBUG_LEVEL_SUPPORT   * CPMAC_DEBUG_LEVEL_SUPPORT  ) \
                           | (CPMAC_USE_DEBUG_LEVEL_ERROR     * CPMAC_DEBUG_LEVEL_ERROR    ) \
                           | (CPMAC_USE_DEBUG_LEVEL_WARNING   * CPMAC_DEBUG_LEVEL_WARNING  ) \
                           | (CPMAC_USE_DEBUG_LEVEL_INFO      * CPMAC_DEBUG_LEVEL_INFO     ) \
                           | (CPMAC_USE_DEBUG_LEVEL_INFOTRACE * CPMAC_DEBUG_LEVEL_INFOTRACE) \
                           | (CPMAC_USE_DEBUG_LEVEL_TRACE     * CPMAC_DEBUG_LEVEL_TRACE    ) \
                           | (CPMAC_USE_DEBUG_LEVEL_TEST      * CPMAC_DEBUG_LEVEL_TEST     ) \
                           | (CPMAC_USE_DEBUG_LEVEL_DEBUG     * CPMAC_DEBUG_LEVEL_DEBUG    ) \
                          )

/* Clear perhaps existing external defines */
#if defined(DEB_SUPPORT)
#  undef DEB_SUPPORT
#endif
#if defined(DEB_ERR)
#  undef DEB_ERR
#endif
#if defined(DEB_WARN)
#  undef DEB_WARN
#endif
#if defined(DEB_INFO)
#  undef DEB_INFO
#endif
#if defined(DEB_INFOTRC)
#  undef DEB_INFOTRC
#endif
#if defined(DEB_TRC)
#  undef DEB_TRC
#endif
#if defined(DEB_TEST)
#  undef DEB_TEST
#endif
#if defined(DEB_DEBUG)
#  undef DEB_DEBUG
#endif

/* Now define the debug routines according to the above information */
#if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_SUPPORT
#  define DEB_SUPPORT(a...) printk(KERN_ERR "[cpmac] " a);
#endif
#if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_ERROR
#  define DEB_ERR(a...) printk(KERN_ERR "[cpmac] " a);
#endif
#if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_WARNING
#  define DEB_WARN(a...) printk(KERN_ERR "[cpmac] " a);
#endif
#if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_INFO
#  define DEB_INFO(a...) printk(KERN_ERR "[cpmac] " a);
#endif
#if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_INFOTRACE
#  define DEB_INFOTRC(a...) printk(KERN_ERR "[cpmac] " a);
#endif
#if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_TRACE
#  define DEB_TRC(a...) printk(KERN_ERR "[cpmac] " a);
#endif
#if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_TEST
#  define DEB_TEST(a...) printk(KERN_ERR "[cpmac] [test] " a);
#endif
#if CPMAC_DEBUG_LEVEL & CPMAC_DEBUG_LEVEL_DEBUG
#  define DEB_DEBUG(a...) printk(KERN_ERR "[cpmac] " a);
#endif

/* Now define all unused error levels as empty */
#if !defined(DEB_SUPPORT)
#  define DEB_SUPPORT(a...)
#endif
#if !defined(DEB_ERR)
#  define DEB_ERR(a...)
#endif
#if !defined(DEB_WARN)
#  define DEB_WARN(a...)
#endif
#if !defined(DEB_INFO)
#  define DEB_INFO(a...)
#endif
#if !defined(DEB_INFOTRC)
#  define DEB_INFOTRC(a...)
#endif
#if !defined(DEB_TRC)
#  define DEB_TRC(a...)
#endif
#if !defined(DEB_TEST)
#  define DEB_TEST(a...)
#endif
#if !defined(DEB_DEBUG)
#  define DEB_DEBUG(a...)
#endif

#endif /*--- #ifndef _CPMAC_DEBUG_H_ ---*/

