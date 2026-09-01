/******************************************************************************
**
** FILE NAME    : ifxmips_dma_core.c
** PROJECT      : IFX UEIP
** MODULES      : Central DMA
**
** DATE         : 03 June 2009
** AUTHOR       : Reddy Mallikarjuna
** DESCRIPTION  : IFX Cross-Platform Central DMA driver
** COPYRIGHT    :       Copyright (c) 2009
**                      Infineon Technologies AG
**                      Am Campeon 1-12, 85579 Neubiberg, Germany
**
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
** HISTORY
** $Date                $Author                 $Comment
** 03 June 2009         Reddy Mallikarjuna      Initial release
*******************************************************************************/

/*!
  \file ifxmips_dma_core.c
  \ingroup IFX_DMA_CORE
  \brief DMA driver main source file.
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <linux/version.h>
#include <linux/list.h>
#include <linux/skbuff.h>

#include <ifx_types.h>
#include <ifx_regs.h>
#include <common_routines.h>
#include <ifx_pmu.h>
#include <ifx_dma_core.h>
#include "ifxmips_dma_reg.h"
#if defined(CONFIG_DANUBE)
#include "ifxmips_dma_danube.h"
#elif defined(CONFIG_AMAZON_SE)
#include "ifxmips_dma_amazon_se.h"
#elif defined(CONFIG_AR9)
#include "ifxmips_dma_ar9.h"
#elif defined(CONFIG_VR9)
#include "ifxmips_dma_vr9.h"
#elif defined(CONFIG_AR10)
#include "ifxmips_dma_ar10.h"
#else
#error Platform is not specified!
#endif
#include <asm/avm_busmaster_handler.h>

/* ============================= */
/* Local Macros & Definitions    */
/* ============================= */
/** Default channel budget */
#define DMA_INT_BUDGET                  20
#define DMA_MAJOR 						250
#define IFX_DMA_DRV_MODULE_NAME         "ifxmips_dma_core"
#define IFX_DRV_MODULE_VERSION          "1.0.17"
#define DEBUG_IFX_DMA                   0

static char version[] __devinitdata =
        IFX_DMA_DRV_MODULE_NAME ".c:v" IFX_DRV_MODULE_VERSION " \n";

static struct proc_dir_entry*           g_dma_dir;
static u64*                             g_desc_list;
static volatile u32 g_dma_int_status 	= 0;
static volatile int g_dma_in_process 	= 0;
_dma_chan_map* chan_map 				= ifx_default_dma_map;
static u64 *g_desc_list_backup 			= NULL;
static _dma_device_info  				dma_devs[MAX_DMA_DEVICE_NUM];
static _dma_channel_info 				dma_chan[MAX_DMA_CHANNEL_NUM];
static char dma_proc_read_buff[1024];
static int min_chan_dump = 0;
static int max_chan_dump = MAX_DMA_CHANNEL_NUM;

static unsigned int global_channel_complete_irq_counter = 0;
static unsigned int global_channel_full_irq_counter = 0;

#ifdef CONFIG_NAPI_ENABLED
static volatile u32 g_dma_poll_int_status = 0;

static volatile unsigned int g_dma_fix_counter = 0;
static volatile unsigned int g_dma_parachute_counter = 0;

/*--- #define DEBUG_REGISTERS ---*/
#ifdef DEBUG_REGISTERS
void dma_register_dump(void);
#endif
#endif /* CONFIG_NAPI_ENABLED */

struct list_head parachute_head;
static void do_dma_tasklet(unsigned long);
DECLARE_TASKLET(dma_tasklet,do_dma_tasklet,0);


_dma_channel_info dummy_chan;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(DEBUG_IFX_DMA) && DEBUG_IFX_DMA 

/*----------------------------------------------------------------*\
 * setup dma_verbose Module-Parameter: kann gesetzt werden in:
 * /sys/module/ifxmips_dma_core/parameters/dma_verbose
\*----------------------------------------------------------------*/
int dma_verbose = 0;
module_param(dma_verbose, int, S_IRUSR | S_IWUSR);
#define dbg_verbose(format, arg...)  do { if ( unlikely( dma_verbose ) ) printk(KERN_INFO "[%s]: " format "\n", __FUNCTION__, ##arg); } while ( 0 )

#else /*--- #if defined(DEBUG_IFX_DMA) ---*/

#define dbg_verbose(format, arg...)

#endif /*--- #if defined(DEBUG_IFX_DMA) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

struct timer_list dma_fix_timer[MAX_DMA_CHANNEL_NUM];
static void do_dma_fix(unsigned long chan_no)
{
    unsigned int tmp, tmp2, cis;

//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);
    if(!(g_dma_poll_int_status & (1 << chan_no))){
        tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
        cis = IFX_REG_R32(IFX_DMA_CIS_REG);

        if(cis & 0xe){
            IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNICR_REG) | (1 << chan_no)), IFX_DMA_IRNICR_REG);
            ++g_dma_fix_counter;
            tmp2 = IFX_REG_R32(IFX_DMA_CS_REG);
            dbg_verbose( "sending pseudo-IRQ for channel %d", chan_no);
            if(tmp2 != chan_no){
                dbg_verbose( " ***!!!*** IFX_DMA_CS_REG changed to %d ***!!!***", tmp2);
            }
        }

        IFX_REG_W32(tmp, IFX_DMA_CS_REG);
    }
//    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
}

#if 0 && defined(CONFIG_VR9) || defined(CONFIG_AR10)
extern struct dma_device_info* g_dma_device_ppe;
struct timer_list dma_parachute_timer;
static void do_dma_parachute_timer(unsigned long data __attribute__((unused))) {
    int i, chan_no, cis, tmp;
    struct dma_device_info* dma_dev;

//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    list_for_each_entry(dma_dev, &parachute_head, parachute_list){
        if(dma_dev != NULL){
            tmp = IFX_REG_R32(IFX_DMA_CS_REG);

            for (i = 0; i < dma_dev->max_rx_chan_num; i++) {
                chan_no = dma_dev->rx_chan[i] - dma_chan;

                if (dma_dev->rx_chan[i]->no_cpu_data
                    || dma_dev->rx_chan[i]->control == IFX_DMA_CH_OFF
                    || (g_dma_poll_int_status & (1 << chan_no)))
                {
                    continue;
                }

                IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
                cis = IFX_REG_R32(IFX_DMA_CIS_REG);

                if(cis & 0xe){
                    IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNICR_REG) | (1 << chan_no)), IFX_DMA_IRNICR_REG);
                    ++g_dma_parachute_counter;
                    dbg_verbose( "sending pseudo-IRQ for channel %d", chan_no);
                }
            }


            IFX_REG_W32(tmp, IFX_DMA_CS_REG);
        }
    }
    mod_timer(&dma_parachute_timer, jiffies + HZ);

//    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
}
#endif

/*------------------------------------------------------------------------------------------*\
 * AVM
 * Diese Funktionen werden genutzt, um den DMA-Controller waehrend des Umschaltens des Taktes
 * zu unterbrechen, un danach wieder fortlaufen zu lassen.
\*------------------------------------------------------------------------------------------*/
#define AVM_DMA_BUSMASTER_NAME      "AVM_DMA_BUSMASTER"
#define AVM_DMA_DBG_ERROR(...)      printk(KERN_ERR "[%s:%u]: ", __FUNCTION__, __LINE__); printk(__VA_ARGS__); printk("\n");
       int  avm_dma_busmaster_handler_callback(enum _avm_busmaster_cmd cmd);
static void avm_dma_busmaster_handler_init    (void);
static void avm_dma_busmaster_handler_exit    (void);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_dma_busmaster_handler_callback(enum _avm_busmaster_cmd cmd) {
#ifdef CONFIG_AR9
    #define NO_OF_DMA       20
#else /*--- #ifdef CONFIG_AR9 ---*/
    #define NO_OF_DMA       28
#endif /*--- #else ---*/ /*--- #ifdef CONFIG_AR9 ---*/
    static uint32_t dma_on_flags = 0;
    static int64_t cp0_count;
    int i;

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    switch(cmd) {
        case avm_busmaster_prepare_stop:
            if(dma_on_flags != 0) {
                AVM_DMA_DBG_ERROR("Cmd 'prepare_stop' called, but DMAs are already stopped. So no DAM is running, and we can't stop them.");
                goto err;
            }

            for(i = 0; i < 20; i++) {
                IFX_REG_W32(i, IFX_DMA_CS_REG);

                if((IFX_REG_R32(IFX_DMA_CCTRL_REG) & DMA_CCTRL_ON)) {
                    dma_on_flags |= 1 << i;

                    /*--- AVM_DMA_DBG_ERROR("chan_no:%d, cctrl:%u\n", i, IFX_REG_R32(IFX_DMA_CCTRL_REG)); ---*/
                    IFX_REG_W32((IFX_REG_R32(IFX_DMA_CCTRL_REG) & ~DMA_CCTRL_ON), IFX_DMA_CCTRL_REG);
                    /*--- AVM_DMA_DBG_ERROR("chan_no:%d, cctrl:%u\n", i, IFX_REG_R32(IFX_DMA_CCTRL_REG)); ---*/
                }
            }

            cp0_count = read_c0_count();
            break;
        case avm_busmaster_stop:
            /*--- AVM_DMA_DBG_ERROR("cp0_count: %d, read_c0_count: %d", (int)cp0_count, (int)read_c0_count()); ---*/
            while((read_c0_count() - cp0_count) < 100); // Counter zaehlt mit halber Taktfrequ. -> min. 200 CPU-Takte warten
            /*--- AVM_DMA_DBG_ERROR("cp0_count: %d, read_c0_count: %d", (int)cp0_count, (int)read_c0_count()); ---*/
            break;
        case avm_busmaster_run:
            for(i = 0; i < 20; i++) {
                if(dma_on_flags & (1 << i)) {
                    IFX_REG_W32(i, IFX_DMA_CS_REG);

                    /*--- AVM_DMA_DBG_ERROR("chan_no:%d, cctrl:%u\n", i, IFX_REG_R32(IFX_DMA_CCTRL_REG)); ---*/
                    IFX_REG_W32((IFX_REG_R32(IFX_DMA_CCTRL_REG) | DMA_CCTRL_ON), IFX_DMA_CCTRL_REG);
                    /*--- AVM_DMA_DBG_ERROR("chan_no:%d, cctrl:%u\n", i, IFX_REG_R32(IFX_DMA_CCTRL_REG)); ---*/
                }
            }
            dma_on_flags = 0;
            break;
        default:
            AVM_DMA_DBG_ERROR("Unknown command: %u\n", cmd);
            goto err;
    }

    return 0;
err:
    return -EFAULT;
#undef NO_OF_DMA
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avm_dma_busmaster_handler_init(void) {
    avm_register_busmaster(AVM_DMA_BUSMASTER_NAME, avm_dma_busmaster_handler_callback);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void avm_dma_busmaster_handler_exit(void) {
    avm_release_busmaster(AVM_DMA_BUSMASTER_NAME);
}

#undef AVM_DMA_DBG_ERROR
/*------------------------------------------------------------------------------------------*\
 * Set the channel number
\*------------------------------------------------------------------------------------------*/
static inline void select_chan(int chan_no) {
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
}

/*------------------------------------------------------------------------------------------*\
 * Select the channel ON
\*------------------------------------------------------------------------------------------*/
static void enable_chan(int chan_no) {
    unsigned int tmp;

    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    tmp = IFX_REG_R32(IFX_DMA_CS_REG);
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
    IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) | DMA_CCTRL_ON), IFX_DMA_CCTRL_REG);
    IFX_REG_W32(tmp, IFX_DMA_CS_REG);

    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
}

/*------------------------------------------------------------------------------------------*\
 * Select the channel OFF
\*------------------------------------------------------------------------------------------*/
static void disable_chan(int chan_no) {
    unsigned int tmp;

    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    tmp = IFX_REG_R32(IFX_DMA_CS_REG);
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
    IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) & ~DMA_CCTRL_ON), IFX_DMA_CCTRL_REG);
    IFX_REG_W32(tmp, IFX_DMA_CS_REG);

    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
}

/*------------------------------------------------------------------------------------------*\
 * Select the channel OFF
\*------------------------------------------------------------------------------------------*/
static int is_enabled_chan(int chan_no) {
    unsigned int tmp;
    int is_enabled;

    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    tmp = IFX_REG_R32(IFX_DMA_CS_REG);
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
    is_enabled = (IFX_REG_R32(IFX_DMA_CCTRL_REG) & DMA_CCTRL_ON)?1:0;
    IFX_REG_W32(tmp, IFX_DMA_CS_REG);

    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
    return is_enabled;
}

/*------------------------------------------------------------------------------------------*\
 * This function handle in client driver.
 * if the client driver is not register with this function, then this function will invoke.
\*------------------------------------------------------------------------------------------*/
static unsigned char* common_buffer_alloc(int len, int* byte_offset,void** opt) {
    unsigned char *buffer = (unsigned char*) kmalloc (len * sizeof (unsigned char), GFP_ATOMIC);
    *byte_offset = 0;
    return buffer;
}

/*------------------------------------------------------------------------------------------*\
 * This function handle in the client device driver. 
 * if the client driver is not register with this function, then this function will invoke.
\*------------------------------------------------------------------------------------------*/
static int common_buffer_free(unsigned char* dataptr,void* opt) {
    if(dataptr) {
        kfree(dataptr);
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
 * Enable the DMA channel interrupt
 * Caller must hold spin lock!
\*------------------------------------------------------------------------------------------*/
static void enable_ch_irq(_dma_channel_info* pCh) {
    int chan_no = (int)(pCh - dma_chan);
    unsigned int tmp, val = 0;
    dbg_verbose( "chan: %d\n", chan_no );
    if(pCh->dir == IFX_DMA_RX_CH) {
#ifdef CONFIG_DMA_HW_POLL_DISABLED
        val |= DMA_CIE_DUR;
#else
        val |= DMA_CIE_DESCPT;
#endif /* CONFIG_DMA_HW_POLL_DISABLED */
    }
    val |= DMA_CIE_EOP;

    //    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);
    tmp = IFX_REG_R32(IFX_DMA_CS_REG);
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
    IFX_REG_W32(val, IFX_DMA_CIE_REG);
    IFX_REG_W32(tmp, IFX_DMA_CS_REG);

    IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);

//    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
}

/*------------------------------------------------------------------------------------------*\
 * Disable the DMA channel Interrupt
 * Caller must hold spin lock!
\*------------------------------------------------------------------------------------------*/
static void disable_ch_irq(_dma_channel_info* pCh) {
    unsigned int tmp, chan_no = (unsigned int)(pCh - dma_chan);
    dbg_verbose( "chan: %d\n", chan_no );

//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    g_dma_int_status &= ~(1 << chan_no);

    tmp = IFX_REG_R32(IFX_DMA_CS_REG);
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
    IFX_REG_W32(DMA_CIE_DISABLE_ALL, IFX_DMA_CIE_REG);
    IFX_REG_W32(tmp, IFX_DMA_CS_REG);

    IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNEN_REG) & ~(1 << chan_no)), IFX_DMA_IRNEN_REG);
   /* IFX_REG_W32((1 << chan_no), IFX_DMA_IRNCR_REG); */

//    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
}

/*------------------------------------------------------------------------------------------*\
 *  Set the DMA channel ON
\*------------------------------------------------------------------------------------------*/
static void open_chan(_dma_channel_info* pCh) {
    int chan_no = (int)(pCh - dma_chan);
    dbg_verbose( "chan: %d\n", chan_no );

//    DMA_IRQ_LOCK(pCh);
//    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    enable_chan(chan_no);
    if(pCh->dir == IFX_DMA_RX_CH && !pCh->no_cpu_data){
        enable_ch_irq(pCh);
    }

//    DMA_IRQ_UNLOCK(pCh);
//    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
}

/*------------------------------------------------------------------------------------------*\
 *  Set the DMA channel OFF
\*------------------------------------------------------------------------------------------*/
static void close_chan(_dma_channel_info* pCh) {
	int j;
    int chan_no = (int)(pCh - dma_chan);
    unsigned int tmp;
    unsigned int cctrl_reg_val;
    unsigned long __ilockflags;                     \

    dbg_verbose( "chan: %d\n", chan_no );

    // we need this lock to protect us against
    // shutting down while only some fragments have been sent so far
    spin_lock_irqsave(&(pCh->irq_lock), __ilockflags);

//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);


    tmp = IFX_REG_R32(IFX_DMA_CS_REG);
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
    IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) & ~DMA_CCTRL_ON), IFX_DMA_CCTRL_REG);
    // wait until channel off complete
    for ( j = 10000; (IFX_REG_R32(IFX_DMA_CCTRL_REG) & DMA_CCTRL_ON) && j > 0; j-- );

    cctrl_reg_val = IFX_REG_R32(IFX_DMA_CCTRL_REG);

    IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

    disable_ch_irq(pCh);


    if ( j == 0 ){
        printk( KERN_ERR "chan_no %d: can not be turned off, IFX_DMA_CCTRL_REG %08x\n", chan_no, cctrl_reg_val);
        dump_stack();
    }

    spin_unlock_irqrestore(&(pCh->irq_lock), __ilockflags);
}

/*------------------------------------------------------------------------------------------*\
 * Reset the dma channel
\*------------------------------------------------------------------------------------------*/
static void reset_chan(_dma_channel_info* pCh) {
    u32 cs_val;
    int chan_no = (int)(pCh - dma_chan);
    int i;

//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    g_dma_int_status &= ~(1 << chan_no);
    cs_val = IFX_REG_R32(IFX_DMA_CS_REG);
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
    IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) | DMA_CCTRL_RST), IFX_DMA_CCTRL_REG);

    while ((IFX_REG_R32(IFX_DMA_CCTRL_REG) & DMA_CCTRL_RST)) {
        ;
    }

    IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) & ~DMA_CCTRL_ON), IFX_DMA_CCTRL_REG);
    IFX_REG_W32( ( (u32)CPHYSADDR(pCh->desc_base)), IFX_DMA_CDBA_REG);
    IFX_REG_W32( pCh->desc_len  , IFX_DMA_CDLEN_REG);
    pCh->curr_desc = 0;
    pCh->prev_desc = 0;
    IFX_REG_W32(cs_val, IFX_DMA_CS_REG);


    /* clear descriptor memory, so that OWN bit is set to CPU */
    memset((u32*)pCh->desc_base, 0, pCh->desc_len * sizeof (struct tx_desc));
    //memset((u32*)pCh->desc_base, 0, pCh->desc_len * sizeof (struct tx_desc));
    if ( pCh->dir == IFX_DMA_TX_CH ) {
        for ( i = 0; i < pCh->desc_len; i++ )
            ((struct tx_desc *)pCh->desc_base)[i].status.field.OWN = IFX_CPU_OWN;
    }
    else {
        for ( i = 0; i < pCh->desc_len; i++ ) {
            ((struct rx_desc *)pCh->desc_base)[i].status.field.OWN = IFX_CPU_OWN;
            ((struct rx_desc *)pCh->desc_base)[i].status.field.C = 1;
        }
    }
    /* TODO: Reset DMA descriptors (free buffer, reset owner bit, allocate new buffer) */

    //    DMA_IRQ_UNLOCK(pCh);
        DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
}

/*------------------------------------------------------------------------------------------*\
 * Handle the Rx channel interrupt scheduler
\*------------------------------------------------------------------------------------------*/
static void rx_chan_intr_handler(int chan_no) {
    unsigned int tmp, cis;

    _dma_device_info* pDev = (_dma_device_info*)dma_chan[chan_no].dma_dev;
    _dma_channel_info* pCh = &dma_chan[chan_no];
    struct rx_desc* rx_desc_p;

    /*
     * just acknowledge all IRQs if channel is no_cpu_data
     */
    if(unlikely(pCh->no_cpu_data)){
        pCh->weight--;

        DMA_IRQ_SPIN_LOCK(&dummy_chan);

        tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32(chan_no, IFX_DMA_CS_REG);

        cis = IFX_REG_R32(IFX_DMA_CIS_REG);
        IFX_REG_W32((cis & DMA_CIS_ALL), IFX_DMA_CIS_REG);
        IFX_REG_W32(tmp, IFX_DMA_CS_REG);

        g_dma_int_status &= ~(1 << chan_no);
        IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);

        DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

        return;
    }

    /* Handle command complete interrupt */
    rx_desc_p = (struct rx_desc*)pCh->desc_base + pCh->curr_desc;
    if (rx_desc_p->status.field.OWN == IFX_CPU_OWN  && rx_desc_p->status.field.C) {
        /* Everything is correct, then we inform the upper layer */
        if (pDev->intr_handler) {
            pDev->intr_handler(pDev, RCV_INT, pCh->rel_chan_no);
        }
        pCh->weight--;
        /* Clear interrupt status bits once we send out the pseudo interrupt to client driver */

//        DMA_IRQ_LOCK(pCh);
        DMA_IRQ_SPIN_LOCK(&dummy_chan);

        tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
        cis = IFX_REG_R32(IFX_DMA_CIS_REG);
        IFX_REG_W32( (cis | DMA_CIS_ALL), IFX_DMA_CIS_REG);
        IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//        DMA_IRQ_UNLOCK(pCh);
        DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
    } else {
        /* Read channel interrupt status bit to determine if we should wait or move forward */
//        DMA_IRQ_LOCK(pCh);
        DMA_IRQ_SPIN_LOCK(&dummy_chan);

        /* Clear the DMA Node status register bit to remove the dummy interrupts */
        IFX_REG_W32((1 << chan_no), IFX_DMA_IRNCR_REG);
        tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
        cis = IFX_REG_R32(IFX_DMA_CIS_REG);
        /*
         * In most cases, interrupt bit has been cleared in the first branch, it will
         * hit the following. However, in some corner cases, the above branch doesn't happen,
         * so it indicates that interrupt is received actually, but OWN bit and
         * Complete bit not update yet, so go back to DMA tasklet and let DMA tasklet
         * wait <polling>. This is the most efficient way.
         * NB, only EOP and Descript Complete interrupt enabled in Channel Interrupt Enable Register.
         */
        if ((cis & IFX_REG_R32(IFX_DMA_CIE_REG)) == 0) {
            if (rx_desc_p->status.field.OWN != IFX_CPU_OWN) {
                g_dma_int_status &= ~(1 << chan_no);
            }
            /*
             * Enable this channel interrupt again after processing all packets available
             * Placing this line at the last line will avoid interrupt coming again
             * in heavy load to great extent
             */
            IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);
        }
        IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//        DMA_IRQ_UNLOCK(pCh);
        DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
    }
#ifdef CONFIG_DMA_HW_POLL_DISABLED
    /* remember that we ran out of descriptors, don't trigger polling now */
    if (cis & DMA_CIS_DUR)
        pCh->dur = 1;
#endif /* CONFIG_DMA_HW_POLL_DISABLED */
}

/*------------------------------------------------------------------------------------------*\
 * Handle the Tx channel interrupt scheduler
\*------------------------------------------------------------------------------------------*/
static void tx_chan_intr_handler(int chan_no) {
    unsigned int tmp, cis;
    _dma_device_info* pDev = (_dma_device_info*)dma_chan[chan_no].dma_dev;
    _dma_channel_info* pCh = &dma_chan[chan_no];
    struct tx_desc* tx_desc_p;

    int exec_irq = 0;
    if ( pCh->scatter_gather_channel ) {
        // we don't wanna know if the next buffer is free
        // we need to know if MAX_SKB_FRAGS are free
        int scatter_gather_range_end = ( pCh->curr_desc + MAX_SKB_FRAGS - 1 ) % (pCh->desc_len);
        tx_desc_p = (struct tx_desc*)pCh->desc_base + scatter_gather_range_end;

    } else {
        tx_desc_p = (struct tx_desc*)pCh->desc_base + pCh->curr_desc;
    }

//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    tmp = IFX_REG_R32(IFX_DMA_CS_REG);
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
    cis = IFX_REG_R32(IFX_DMA_CIS_REG);
    IFX_REG_W32( (cis | DMA_CIS_ALL), IFX_DMA_CIS_REG);
    IFX_REG_W32(tmp, IFX_DMA_CS_REG);

    /*  DMA Descriptor update by Hardware is not sync with DMA interrupt (EOP/Complete).
    To ensure descriptor is available before sending psudo interrupt to the client drivers.*/
    if(!(tx_desc_p->status.field.OWN == IFX_DMA_OWN)) {
       if ((cis & DMA_CIS_EOP) && pDev->intr_handler) {
           g_dma_int_status &= ~(1 << chan_no);
           exec_irq = 1;
       }
    }

//    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
    if(exec_irq){
        pDev->intr_handler(pDev, TRANSMIT_CPT_INT, pCh->rel_chan_no);
        pCh->channel_complete_irq_counter++;
        global_channel_complete_irq_counter++;

        pCh->channel_hangs_since = 0;
    }

}
/* Handle the Rx channel interrupt handler*/
void rx_chan_handler(int chan_no)
{
    unsigned int tmp ;
    _dma_device_info* pDev = (_dma_device_info*)dma_chan[chan_no].dma_dev;
    _dma_channel_info* pCh = &dma_chan[chan_no];
    struct rx_desc* rx_desc_p;

    /* Handle command complete interrupt */
    rx_desc_p = (struct rx_desc*)pCh->desc_base + pCh->prev_desc;
    if (rx_desc_p->status.field.OWN == IFX_CPU_OWN  && rx_desc_p->status.field.C) {
        /* Everything is correct, then we inform the upper layer */
        if (pDev->intr_handler) {
            pDev->intr_handler(pDev, RCV_INT, pCh->rel_chan_no);
        }
        /* Clear interrupt status bits once we sendout the psuedo interrupt to client driver */
//        DMA_IRQ_LOCK(pCh);
        DMA_IRQ_SPIN_LOCK(&dummy_chan);

        tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
        IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CIS_REG) | DMA_CIS_ALL), IFX_DMA_CIS_REG);
        IFX_REG_W32(tmp, IFX_DMA_CS_REG);
        /*
            * Enable this channel interrupt again after processing all packets available
            * Placing this line at the last line will avoid interrupt coming again
            * in heavy load to great extent
        */
        IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);

//        DMA_IRQ_UNLOCK(pCh);
        DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

        pCh->prev_desc = (pCh->prev_desc + 1) % (pCh->desc_len);
    } else {
        /*
        unsigned int tmp1; */
//        DMA_IRQ_LOCK(pCh);
        DMA_IRQ_SPIN_LOCK(&dummy_chan);

        /*tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
        tmp1 = IFX_REG_R32(IFX_DMA_CIS_REG);
        IFX_REG_W32(tmp, IFX_DMA_CS_REG); */
        /*
            * Enable this channel interrupt again after processing all packets available
            * Placing this line at the last line will avoid interrupt coming again
            * in heavy load to great extent
        */
        IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);

//        DMA_IRQ_UNLOCK(pCh);
        DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

        /*printk(KERN_ERR "%s: should not be here (dma cis: 0x%8x)\n", __func__,tmp1); */
    }
}

/*------------------------------------------------------------------------------------------*\
 * Trigger when taklet schedule calls
\*------------------------------------------------------------------------------------------*/
static void do_dma_tasklet(unsigned long unused) {
    int i, chan_no=0, budget = DMA_INT_BUDGET, weight = 0;

    while (g_dma_int_status) {
        if (budget-- < 0) {
            tasklet_hi_schedule(&dma_tasklet);
            return;
        }
        chan_no = -1;
        weight = 0;
        /* WFQ algorithm to select the channel */
        for (i = 0; i < MAX_DMA_CHANNEL_NUM; i++) {
            if (unlikely((g_dma_int_status & (1 << i)) && dma_chan[i].weight > 0)) {
                if(dma_chan[i].weight > weight) {
                    chan_no = i;
                    weight = dma_chan[chan_no].weight;
                }
            }
        }
        if (chan_no >= 0) {
            if (chan_map[chan_no].dir == IFX_DMA_RX_CH) {
                rx_chan_intr_handler(chan_no);
            } else {
                tx_chan_intr_handler(chan_no);
            }
        } else {
            /* Reset the default weight vallue for all channels */
            for(i = 0; i < MAX_DMA_CHANNEL_NUM; i++) {
                dma_chan[i].weight = dma_chan[i].default_weight;
            }
        }
    }
    /*
     * Sanity check, check if there is new packet coming during this small gap
     * However, at the same time, the following may cause interrupts is coming again
     * on the same channel, because of rescheduling.
     */
//    DMA_IRQ_LOCK(chan_no);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    g_dma_in_process = 0;
    if (g_dma_int_status) {
        g_dma_in_process = 1;
        tasklet_hi_schedule(&dma_tasklet);
    }

//    DMA_IRQ_UNLOCK(chan_no);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
}

/*------------------------------------------------------------------------------------------*\
 * This function works as the following,
 * 1) Valid check
 * 2) Disable interrupt on this DMA channel, so that no further interrupts are coming
 *    when DMA tasklet is running. However, it still allows other DMA channel interrupt
 *    to come in.
 * 3) Record interrupt on this DMA channel, and dispatch it to DMA tasklet later
 * 4) If NAPI is enabled, submit to polling process instead of DMA tasklet.
 * 5) Check if DMA tasklet is running, if not, invoke it so that context switch is minimized
\*------------------------------------------------------------------------------------------*/
static irqreturn_t dma_interrupt(int irq, void *dev_id) {
    int chan_no;
    _dma_channel_info* pCh;
    pCh = (_dma_channel_info*)dev_id;
    chan_no = (int )(pCh - dma_chan);

    if(dev_id == NULL) {
    	printk(KERN_ERR "[%s] error: irq =%d\n", __func__, irq);
        return IRQ_HANDLED;
    }

    if (chan_no < 0 ||chan_no > (MAX_DMA_CHANNEL_NUM - 1)) {
        printk(KERN_ERR "%s: dma_interrupt irq=%d chan_no=%d\n", __func__,irq, chan_no);
        return IRQ_NONE;
    }

    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    /*
     * Disable interrupt on this channel, later tasklet will enable it after
     * processing all available packets on this channel.
     */
    IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNEN_REG) & ~(1 << chan_no)), IFX_DMA_IRNEN_REG);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

    if(pCh->dma_dev == NULL) {
        return IRQ_HANDLED;
    }

#ifdef CONFIG_DMA_HW_POLL_DISABLED
#error CONFIG_DMA_HW_POLL_DISABLED funktioniert bisher nicht mit dem PPA -> es kommt zum Dauer-DUR-interrupt
    if (pCh->no_cpu_data) {
        unsigned int tmp, cis;

        DMA_IRQ_SPIN_LOCK(&dummy_chan);

        tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
        cis = IFX_REG_R32(IFX_DMA_CIS_REG);
        // if (cis & DMA_CIS_DUR){
        if (cis){
        	// printk(KERN_ERR "[%s] DUR Interrupt {CIS=%#x} on DMA-Chan:%d \n", __func__, cis, chan_no);
        	//  IFX_REG_W32(DMA_CIS_DUR, IFX_DMA_CIS_REG);
        	IFX_REG_W32(cis | DMA_CIS_ALL, IFX_DMA_CIS_REG); //TODO hack: clear all irqs
        	IFX_REG_W32((1 << chan_no), IFX_DMA_IRNCR_REG);
        }
        IFX_REG_W32(tmp, IFX_DMA_CS_REG);

        // DUR_IRQ_ONLYL reenable it here -> this is normally done in the tasklet
        IFX_REG_W32( (IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);

        DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

        return IRQ_HANDLED;
    }
#endif

    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    /* Record this channel interrupt for tasklet */
    g_dma_int_status |= (1 << chan_no);

    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);


// TKL: mit asynchronem SKB-Handling in AVMNET wieder auf Tasklet umgestellt
#if 0 && defined(CONFIG_NAPI_ENABLED)
    if (pCh->dir == IFX_DMA_RX_CH) {
        _dma_device_info* pDev = (_dma_device_info*)pCh->dma_dev;
        if (pDev->activate_poll) {
            /* Handled by polling rather than tasklet */

            DMA_IRQ_SPIN_LOCK(&dummy_chan);

            g_dma_int_status &= ~(1 << chan_no);
            g_dma_poll_int_status |= (1 << chan_no);

            DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

            pDev->activate_poll(pDev);
            return IRQ_HANDLED;
        }
    }
#else
#endif /* CONFIG_NAPI_ENABLED */

    /* if not in process, then invoke the tasklet */
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    if (!g_dma_in_process) {
        g_dma_in_process = 1;
        tasklet_hi_schedule(&dma_tasklet);
    }

    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

    return IRQ_HANDLED;
}

/*------------------------------------------------------------------------------------------*\
 * This function should call before the dma_device_register
 * It is reserved the dma device port if the port is supported and send the dma device info pointer
 * \param    dev_name --> The supported port device name.
 * \return on success send dma devcie pointer else NULL
\*------------------------------------------------------------------------------------------*/
_dma_device_info* dma_device_reserve(IFX_char_t* dev_name) {
    int i;

    for (i = 0; i < MAX_DMA_DEVICE_NUM; i++) {
        if (strcmp(dev_name, dma_devs[i].device_name) == 0) {
            if(dma_devs[i].port_reserved) {
                return NULL;
            }
            dma_devs[i].port_reserved = IFX_TRUE;
#ifdef CONFIG_NAPI_ENABLED
            dma_devs[i].activate_poll   = NULL;
            dma_devs[i].inactivate_poll = NULL;
#endif /* CONFIG_NAPI_ENABLED */
            return &dma_devs[i];
        }
    }
    return NULL;
}
EXPORT_SYMBOL(dma_device_reserve);

/*------------------------------------------------------------------------------------------*\
  \fn int dma_device_release(_dma_device_info* dev)
  \ingroup  IFX_DMA_DRV_API
  \brief Unreseve the dma device port

  This function will called after the dma_device_unregister
  This fucnction unreseve the port.

  \param    dev --> pointer to DMA device structure
  \return IFX_SUCCESS
\*------------------------------------------------------------------------------------------*/
int dma_device_release(_dma_device_info* dev) {
    int i;
    for (i = 0; i < MAX_DMA_DEVICE_NUM; i++) {
        if (strcmp(dma_devs[i].device_name, dev->device_name) == 0) {
            if(dev->port_reserved) {
                dev->port_reserved = IFX_FALSE;
                return IFX_SUCCESS;
            }
        }
    }
    printk(KERN_ERR "%s: Device Port released failed: %s\n", __func__,dev->device_name);
    return IFX_ERROR;
}
EXPORT_SYMBOL(dma_device_release);

/*------------------------------------------------------------------------------------------*\
  \fn int dma_device_register(_dma_device_info* dev)
  \ingroup  IFX_DMA_DRV_API
  \brief Register with DMA device driver.

  This function registers with dma device driver to handle dma functionality.
  Should provide the configuration info during the register with dma device.
  if not provide channel/port configuration info, then take default values.
  This function should call after dma_device_reserve function.
  This function configure the Tx/Rx channel info as well as device port info

  \param    dev --> pointer to DMA device structure
  \return IFX_SUCCESS/IFX_ERROR
\*------------------------------------------------------------------------------------------*/
int dma_device_register(_dma_device_info* dev) {
    int result = IFX_SUCCESS, i ,j, chan_no=0, byte_offset = 0, txbl,rxbl;
    unsigned int reg_val, tmp;
    unsigned char *buffer;
    _dma_device_info*  pDev;
    _dma_channel_info* pCh;
    struct rx_desc* rx_desc_p;
    struct tx_desc* tx_desc_p;
    int port_no = dev->port_num;

    if (port_no < 0 || port_no > MAX_DMA_DEVICE_NUM)
        printk(KERN_ERR "%s: Wrong port number(%d)!!!\n", __func__,port_no);
    /* burst length Configration */
    switch(dev->tx_burst_len) {
        case 8:
            txbl = IFX_DMA_BURSTL_8;
            break;
        case 4:
            txbl = IFX_DMA_BURSTL_4;
            break;
        default:
            txbl = IFX_DMA_PORT_DEFAULT_TX_BURST_LEN;
    }
    switch(dev->rx_burst_len) {
        case 8:
            rxbl = IFX_DMA_BURSTL_8;
            break;
        case 4:
            rxbl = IFX_DMA_BURSTL_4;
            break;
        default:
            rxbl = IFX_DMA_PORT_DEFAULT_RX_BURST_LEN;
    }

//    DMA_IRQ_LOCK(dev);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    IFX_REG_W32(port_no, IFX_DMA_PS_REG);
    reg_val = DMA_PCTRL_TXWGT_SET_VAL(dev->port_tx_weight)   \
            | DMA_PCTRL_TXENDI_SET_VAL(dev->tx_endianness_mode) \
            | DMA_PCTRL_RXENDI_SET_VAL(dev->rx_endianness_mode) \
            | DMA_PCTRL_PDEN_SET_VAL(dev->port_packet_drop_enable) \
            | DMA_PCTRL_TXBL_SET_VAL(txbl) \
            | DMA_PCTRL_RXBL_SET_VAL(rxbl);
    if ( dev->mem_port_control) {
        reg_val |= DMA_PCTRL_GPC;
    } else {
        reg_val &= ~DMA_PCTRL_GPC;
    }

/*	dbg_verbose("PCTRL: 0x%08x ****************\n", reg_val); */
    IFX_REG_W32(reg_val, IFX_DMA_PCTRL_REG);

    // dbg_verbose("CLEAR IRNEN\n");
    // IFX_REG_W32(0, IFX_DMA_IRNEN_REG);

//    DMA_IRQ_UNLOCK(dev);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

    /* Tx channel register */
    for (i = 0;i < dev->max_tx_chan_num; i++) {
        pCh = dev->tx_chan[i];
        chan_no = (int)(pCh - dma_chan);
        if (pCh->control == IFX_DMA_CH_ON) {
            /** Number of descriptor length should be less than the allocated
            descriptors during the module init */
            if (pCh->desc_len > IFX_DMA_DESCRIPTOR_OFFSET) {
                printk("%s Tx Channel %d descriptor length %d out of range <1~%d>\n",   \
                    __func__, chan_no, pCh->desc_len, IFX_DMA_DESCRIPTOR_OFFSET);
                result = IFX_ERROR;
                goto done;
            }
            for (j = 0; j < pCh->desc_len; j++) {
                tx_desc_p = (struct tx_desc*)pCh->desc_base + j;
                memset(tx_desc_p, 0, sizeof(struct tx_desc));
            }

//            DMA_IRQ_LOCK(pCh);
            DMA_IRQ_SPIN_LOCK(&dummy_chan);

            tmp = IFX_REG_R32(IFX_DMA_CS_REG);
            IFX_REG_W32(chan_no, IFX_DMA_CS_REG);

            /* check if the descriptor base is changed */
            if (IFX_REG_R32(IFX_DMA_CDBA_REG) != (u32)CPHYSADDR(pCh->desc_base)) {
                IFX_REG_W32( ( (u32)CPHYSADDR(pCh->desc_base)), IFX_DMA_CDBA_REG);
            }
            /* Check if the descriptor length is changed */
            if (IFX_REG_R32(IFX_DMA_CDLEN_REG) != pCh->desc_len) {
                IFX_REG_W32( ( (pCh->desc_len)), IFX_DMA_CDLEN_REG);
            }
            IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) & ~(DMA_CCTRL_ON)), IFX_DMA_CCTRL_REG);
            udelay(20);
            IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) | (DMA_CCTRL_RST)), IFX_DMA_CCTRL_REG);
            while ((IFX_REG_R32(IFX_DMA_CCTRL_REG) & DMA_CCTRL_RST)) {
                ;
            }

            IFX_REG_W32((1 << chan_no), IFX_DMA_IRNCR_REG);
            IFX_REG_W32( (IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);
            reg_val = IFX_REG_R32(IFX_DMA_CCTRL_REG);
            reg_val &= ~DMA_CCTRL_TXWGT_MASK;
            reg_val |= DMA_CCTRL_TXWGT_VAL(pCh->tx_channel_weight);
            if ( pCh->channel_packet_drop_enable ) {
                reg_val |= DMA_CCTRL_PDEN;
            }
            else {
                reg_val &= ~DMA_CCTRL_PDEN;
            }
            if ( pCh->class_value ) {
                reg_val &= ~DMA_CCTRL_CLASS_MASK;
                reg_val |= DMA_CCTRL_CLASS_VAL_SET(pCh->class_value);
            }
            if (pCh->peri_to_peri) {
                reg_val |= DMA_CCTRL_P2PCPY;
                if ( pCh->global_buffer_len ){
                    IFX_REG_W32( pCh->global_buffer_len, IFX_DMA_CGBL_REG);
                }
            } else {
                reg_val &= ~DMA_CCTRL_P2PCPY;
            }
            if ( pCh->loopback_enable ) {
            	// loopback setup in Tx-Channel
                reg_val |= DMA_CCTRL_LBEN;
                if ( pCh->loopback_channel_number) {
                    reg_val &= ~DMA_CCTRL_LBCHNR_MASK;
                    reg_val |= DMA_CCTRL_LBCHNR_SET(pCh->loopback_channel_number);
                }
            } else {
            	// Reset Loppback
                reg_val &= ~DMA_CCTRL_LBEN;
                reg_val &= ~DMA_CCTRL_LBCHNR_MASK;
            }
            IFX_REG_W32( reg_val, IFX_DMA_CCTRL_REG);
            if (pCh->req_irq_to_free == dma_chan[chan_no].irq) {
                //free_irq(dma_chan[chan_no].irq, (void*)&dma_chan[chan_no]);
                dbg_verbose( "dont enable irq (req_irq_to_free): %d\n", dma_chan[chan_no].irq );
            }
            else{
                enable_irq(dma_chan[chan_no].irq);
                // IFX_REG_W32( (IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);
                dbg_verbose( "enable irq: %d\n", dma_chan[chan_no].irq );
			}

            IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//            DMA_IRQ_UNLOCK(pCh);
            DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

        } else {
                dbg_verbose( "dont enable irq (!IFX_DMA_CH_ON): %d\n", dma_chan[chan_no].irq );
        }
    }
    /* RX channel register */
    for (i = 0; i < dev->max_rx_chan_num; i++) {
        pCh = dev->rx_chan[i];
        chan_no = (int)(pCh - dma_chan);
        if (pCh->control == IFX_DMA_CH_ON) {
            /** Number of descriptor length should be less than the allocated
            descriptors during the module init */
            if (pCh->desc_len > IFX_DMA_DESCRIPTOR_OFFSET) {
                printk("%s Rx Channel %d descriptor length %d out of range <1~%d>\n",   \
                    __func__, chan_no, pCh->desc_len, IFX_DMA_DESCRIPTOR_OFFSET);
                result = IFX_ERROR;
                goto done;
            }
            for (j = 0; j < pCh->desc_len; j++) {
                rx_desc_p = (struct rx_desc*)pCh->desc_base + j;
                pDev = (_dma_device_info*)(pCh->dma_dev);
                buffer = pDev->buffer_alloc(pCh->packet_size, &byte_offset,(void*)&(pCh->opt[j]));
                if (!buffer) {
                    break;
                }

#ifndef CONFIG_MIPS_UNCACHED
                dma_cache_wback_inv((unsigned long)buffer, pCh->packet_size);
#endif /* CONFIG_MIPS_UNCACHED */
                rx_desc_p->Data_Pointer = (u32)CPHYSADDR((u32)buffer);
                rx_desc_p->status.word  = 0;
                rx_desc_p->status.field.byte_offset = byte_offset;
                rx_desc_p->status.field.OWN = IFX_DMA_OWN;
                rx_desc_p->status.field.data_length = pCh->packet_size;
            }

//            DMA_IRQ_LOCK(pCh);
            DMA_IRQ_SPIN_LOCK(&dummy_chan);

            tmp = IFX_REG_R32(IFX_DMA_CS_REG);

            IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
            /* Check if the descriptor base is changed */
            if (IFX_REG_R32(IFX_DMA_CDBA_REG) != (u32)CPHYSADDR(pCh->desc_base)) {
                IFX_REG_W32( ( (u32)CPHYSADDR(pCh->desc_base)), IFX_DMA_CDBA_REG);
            }
            /* Check if the descriptor length is changed */
            if (IFX_REG_R32(IFX_DMA_CDLEN_REG) != pCh->desc_len) {
                IFX_REG_W32( ( (pCh->desc_len)), IFX_DMA_CDLEN_REG);
            }
            IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) & ~(DMA_CCTRL_ON)), IFX_DMA_CCTRL_REG);
            udelay(20);
            IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) | (DMA_CCTRL_RST)), IFX_DMA_CCTRL_REG);
            while ((IFX_REG_R32(IFX_DMA_CCTRL_REG) & DMA_CCTRL_RST)) {
                ;
            }

            /* XXX, should enable all interrupts? */
            IFX_REG_W32(DMA_CIE_DEFAULT, IFX_DMA_CIE_REG);
            IFX_REG_W32((1 << chan_no), IFX_DMA_IRNCR_REG);
            IFX_REG_W32( (IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);
            reg_val = IFX_REG_R32(IFX_DMA_CCTRL_REG);
            /*
            reg_val &= ~DMA_CCTRL_TXWGT_MASK;
            reg_val |= DMA_CCTRL_TXWGT_VAL(pCh->tx_channel_weight);
            */
            if ( pCh->channel_packet_drop_enable ){
                reg_val |= DMA_CCTRL_PDEN;
            } else{
                reg_val &= ~DMA_CCTRL_PDEN;
            }
            if ( pCh->class_value ) {
                reg_val &= ~DMA_CCTRL_CLASS_MASK;
                reg_val |= DMA_CCTRL_CLASS_VAL_SET(pCh->class_value);
            }
            if ( pCh->loopback_enable ) {
            	// loopback setup in Rx-Channel
                reg_val |= DMA_CCTRL_LBEN;
                if ( pCh->loopback_channel_number) {
                    reg_val &= ~DMA_CCTRL_LBCHNR_MASK;
                    reg_val |= DMA_CCTRL_LBCHNR_SET(pCh->loopback_channel_number);
                }
            }
            else {
                reg_val &= ~DMA_CCTRL_LBEN;
                reg_val &= ~DMA_CCTRL_LBCHNR_MASK;
			}
            IFX_REG_W32( reg_val, IFX_DMA_CCTRL_REG);
            if (pCh->req_irq_to_free == dma_chan[chan_no].irq) {
                //free_irq(dma_chan[chan_no].irq, (void*)&dma_chan[chan_no]);
                dbg_verbose( "dont enable irq (req_irq_to_free): %d\n", dma_chan[chan_no].irq );
            }
            else {
                enable_irq( dma_chan[chan_no].irq );
                // IFX_REG_W32( (IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);
                dbg_verbose( "enable irq: %d\n", dma_chan[chan_no].irq );
			}

            IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//            DMA_IRQ_UNLOCK(pCh);
            DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
        } else {
        		chan_no = (int)(pCh - dma_chan);
                dbg_verbose( "dont enable irq (!IFX_DMA_CH_ON): %d\n", dma_chan[chan_no].irq );
        }
    }

//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    INIT_LIST_HEAD(&(dev->parachute_list));
    list_add_tail(&(dev->parachute_list), &parachute_head);

    //    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

done:
    return result;
}
EXPORT_SYMBOL(dma_device_register);

/*------------------------------------------------------------------------------------------*\
 * This function unregisters with dma core driver. Once it unregisters there is no
 * DMA handling with client driver.
\*------------------------------------------------------------------------------------------*/
int dma_device_unregister(_dma_device_info* dev) {
    int result = IFX_SUCCESS, i, j, chan_no;
    unsigned int reg_val, tmp;
    _dma_channel_info* pCh;
    struct rx_desc* rx_desc_p;
    struct tx_desc* tx_desc_p;
    int port_no = dev->port_num;
    unsigned int cctrl_reg_val;

//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    list_del(&(dev->parachute_list));

//    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

    if (port_no < 0 || port_no > MAX_DMA_DEVICE_NUM)
        printk(KERN_ERR "%s: Wrong port number(%d)!!!\n", __func__,port_no);
    for (i = 0; i < dev->max_rx_chan_num; i++) {
        pCh = dev->rx_chan[i];
        if (pCh->control == IFX_DMA_CH_ON) {
            chan_no = (int)(dev->rx_chan[i] - dma_chan);

//            DMA_IRQ_LOCK(pCh);
            DMA_IRQ_SPIN_LOCK(&dummy_chan);

            g_dma_int_status  &= ~(1<<chan_no);
            pCh->curr_desc = 0;
            pCh->prev_desc = 0;
            pCh->control = IFX_DMA_CH_OFF;

            tmp = IFX_REG_R32(IFX_DMA_CS_REG);

            IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
            if (IFX_REG_R32(IFX_DMA_CS_REG) != chan_no) {
                printk(__FILE__ ":%d:%s: *DMA_CS (%d) != chan_no (%d)\n",
                    __LINE__, __func__, IFX_REG_R32(IFX_DMA_CS_REG), chan_no);
            }
            /* XXX, should disable all interrupts? */
            IFX_REG_W32(0, IFX_DMA_CIE_REG);
            /* Disable this channel interrupt */
            IFX_REG_W32( (IFX_REG_R32(IFX_DMA_IRNEN_REG) & ~(1 << chan_no)), IFX_DMA_IRNEN_REG);
            IFX_REG_W32((1 << chan_no), IFX_DMA_IRNCR_REG);
            IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) & ~(DMA_CCTRL_ON)), IFX_DMA_CCTRL_REG);
            for ( j = 10000; (IFX_REG_R32(IFX_DMA_CCTRL_REG) & DMA_CCTRL_ON) && j > 0; j-- );
            cctrl_reg_val = IFX_REG_R32(IFX_DMA_CCTRL_REG);
            IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//            DMA_IRQ_UNLOCK(pCh);
            DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

            if ( j == 0 )
                printk(KERN_ERR "Port %d RX CH %d: can not be turned off, IFX_DMA_CCTRL_REG %08x\n", port_no, i, cctrl_reg_val);
            dbg_verbose("free rx chan %d, chan_no=%d, loopback=%d, peri_2_peri=%d, no_cpu_data=%d \n",i, chan_no, pCh->loopback_enable, pCh->peri_to_peri, pCh->no_cpu_data );
            for (j = 0; j < pCh->desc_len; j++) {
                rx_desc_p = (struct rx_desc*)pCh->desc_base + j;
                /*--- added shutdown support for 'no_cpu_data rx chan' w.o loopback_enable ( e.g. PPA->DSL-Shared-Buffer ) ---*/
                if( pCh->loopback_enable || pCh->no_cpu_data || \
                    (rx_desc_p->status.field.OWN == IFX_CPU_OWN && rx_desc_p->status.field.C)||\
                    (rx_desc_p->status.field.OWN == IFX_DMA_OWN && rx_desc_p->status.field.data_length > 0)) {
                    dev->buffer_free((u8*)__va(rx_desc_p->Data_Pointer), (void*)pCh->opt[j]);
                    pCh->opt[j] = NULL;
                } else {
                    dbg_verbose("dont free rx chan[%d] descr %d own=%s, c=%d, data_length=%d  \n", chan_no, j,
                            (rx_desc_p->status.field.OWN == IFX_CPU_OWN)?"CPU":"DMA",
                            rx_desc_p->status.field.C,
                            rx_desc_p->status.field.data_length );
                }
            }
            if (pCh->req_irq_to_free != dma_chan[chan_no].irq)
                disable_irq(dma_chan[chan_no].irq);
            pCh->desc_base = (u32)g_desc_list + chan_no * IFX_DMA_DESCRIPTOR_OFFSET * 8;
            pCh->desc_len  = IFX_DMA_DESCRIPTOR_OFFSET;

            pCh->channel_packet_drop_enable = IFX_DMA_DEF_CHAN_BASED_PKT_DROP_EN;
            pCh->peri_to_peri = 0;
            pCh->loopback_enable = 0;
            pCh->req_irq_to_free = -1;
            pCh->no_cpu_data = 0;
        }
    }
    for (i = 0; i < dev->max_tx_chan_num; i++) {
        // dbg_verbose("loop tx chan=%d\n", i);
        pCh = dev->tx_chan[i];
        if (pCh->control == IFX_DMA_CH_ON) {
            chan_no = (int)(dev->tx_chan[i] - dma_chan);

//            DMA_IRQ_LOCK(pCh);
            DMA_IRQ_SPIN_LOCK(&dummy_chan);

            tmp = IFX_REG_R32(IFX_DMA_CS_REG);

            IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
            pCh->curr_desc = 0;
            pCh->prev_desc = 0;
            pCh->control = IFX_DMA_CH_OFF;

            /* XXX, Should dislabe all interrupts here */
            IFX_REG_W32(0, IFX_DMA_CIE_REG);

            /* Disable this channel interrupt */
            IFX_REG_W32( (IFX_REG_R32(IFX_DMA_IRNEN_REG) & ~(1 << chan_no)), IFX_DMA_IRNEN_REG);
            IFX_REG_W32((1 << chan_no), IFX_DMA_IRNCR_REG);
            IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) & ~(DMA_CCTRL_ON)), IFX_DMA_CCTRL_REG);
            for ( j = 10000; (IFX_REG_R32(IFX_DMA_CCTRL_REG) & DMA_CCTRL_ON) && j > 0; j-- );

            cctrl_reg_val = IFX_REG_R32(IFX_DMA_CCTRL_REG);
            IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//            DMA_IRQ_UNLOCK(pCh);
            DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

            dbg_verbose("free tx chan %d, chan_no=%d, loopback=%d, peri_2_peri=%d, no_cpu_data=%d \n",i, chan_no, pCh->loopback_enable, pCh->peri_to_peri, pCh->no_cpu_data );
            if ( j == 0 ) {
                printk( KERN_ERR "chan_no %d: can not be turned off, IFX_DMA_CCTRL_REG %08x\n", chan_no, cctrl_reg_val);
                dump_stack();
            }
            for (j = 0; j < pCh->desc_len; j++) {
                if ( pCh->peri_to_peri || pCh->no_cpu_data ) // no_cpu_data tx-channels cannot be freed here
                    break;

                tx_desc_p = (struct tx_desc*)pCh->desc_base + j;
                if((tx_desc_p->status.field.OWN == IFX_CPU_OWN && tx_desc_p->status.field.C)||\
                    (tx_desc_p->status.field.OWN == IFX_DMA_OWN && tx_desc_p->status.field.data_length > 0)) {
                    // dbg_verbose("free tx chan=%d, opt=%d, no_cpu_data=%d \n", i, j, pCh->no_cpu_data);
                    dev->buffer_free((u8*)__va(tx_desc_p->Data_Pointer), (void*)pCh->opt[j]);
                    pCh->opt[j] = NULL;
                    dbg_verbose("free tx chan[%d] descr %d own=%s, c=%d, data_length=%d  \n", chan_no, j,
                            (tx_desc_p->status.field.OWN == IFX_CPU_OWN)?"CPU":"DMA",
                            tx_desc_p->status.field.C,
                            tx_desc_p->status.field.data_length );
                }
#if 0
                else {
                    printk(KERN_ERR "dont free tx chan[%d] descr %d own=%s, c=%d, data_length=%d  \n", chan_no, j,
                            (tx_desc_p->status.field.OWN == IFX_CPU_OWN)?"CPU":"DMA",
                            tx_desc_p->status.field.C,
                            tx_desc_p->status.field.data_length );
                }
#endif
                tx_desc_p->status.field.OWN = IFX_CPU_OWN;
                memset(tx_desc_p,0,sizeof(struct tx_desc));
            }
            if (pCh->req_irq_to_free != dma_chan[chan_no].irq){
				disable_irq(dma_chan[chan_no].irq);
				// disable_ch_irq(pCh);
				dbg_verbose( "[%d] disable irq: %d\n", __LINE__, dma_chan[chan_no].irq );
            } else {
            	dbg_verbose( "[/%d] dont disable irq (it is req_irq_to_free): %d\n", __LINE__, dma_chan[chan_no].irq );
            }
            pCh->desc_base = (u32)g_desc_list + chan_no * IFX_DMA_DESCRIPTOR_OFFSET * 8;
            pCh->desc_len  = IFX_DMA_DESCRIPTOR_OFFSET;

            pCh->channel_packet_drop_enable = IFX_DMA_DEF_CHAN_BASED_PKT_DROP_EN;
            pCh->peri_to_peri = 0;
            pCh->loopback_enable = 0;
            pCh->req_irq_to_free = -1;
            pCh->no_cpu_data = 0;
        }
           /* XXX, Should free buffer that is not transferred by dma*/

    }
    /*  Port configuration stuff  */
//    DMA_IRQ_LOCK(dev);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    IFX_REG_W32(port_no, IFX_DMA_PS_REG);
    reg_val = DMA_PCTRL_TXWGT_SET_VAL(IFX_DMA_TX_PORT_DEFAULT_WEIGHT)   \
            | DMA_PCTRL_TXENDI_SET_VAL(IFX_DMA_DEFAULT_TX_ENDIANNESS) \
            | DMA_PCTRL_RXENDI_SET_VAL(IFX_DMA_DEFAULT_RX_ENDIANNESS) \
            | DMA_PCTRL_PDEN_SET_VAL(IFX_DMA_DEF_PORT_BASED_PKT_DROP_EN) \
            | DMA_PCTRL_TXBL_SET_VAL(IFX_DMA_PORT_DEFAULT_TX_BURST_LEN ) \
            | DMA_PCTRL_RXBL_SET_VAL(IFX_DMA_PORT_DEFAULT_RX_BURST_LEN);
    reg_val &= ~DMA_PCTRL_GPC;
    IFX_REG_W32(reg_val, IFX_DMA_PCTRL_REG);

//    DMA_IRQ_UNLOCK(dev);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

    return result;
}
EXPORT_SYMBOL(dma_device_unregister);

/*------------------------------------------------------------------------------------------*\
 * This function setup the descriptor of the DMA channel used by client driver.
 * The client driver will handle the buffer allocation and
 * do proper checking of buffer for DMA burst alignment.
 * Handle with care when call this function as well as dma_device_read function
 * (in both function the current descriptor is incrementing)
\*------------------------------------------------------------------------------------------*/
int dma_device_desc_setup(_dma_device_info *dma_dev, char *buf, size_t len, int chan_no) {
    int byte_offset = 0;
    struct rx_desc *rx_desc_p;

    _dma_channel_info *pCh = dma_dev->rx_chan[chan_no];

    rx_desc_p = (struct rx_desc *) pCh->desc_base + pCh->curr_desc;
    if (!(rx_desc_p->status.field.OWN == IFX_CPU_OWN )) {
        printk(KERN_ERR "%s: Err!!!(Already setup the descriptor)\n", __func__);
        return IFX_ERROR;
    }
#ifndef CONFIG_MIPS_UNCACHED
    dma_cache_inv ((unsigned long) buf, len);
#endif
    pCh->opt[pCh->curr_desc] = NULL;
    rx_desc_p->Data_Pointer = (u32)CPHYSADDR((u32)buf);
    rx_desc_p->status.word = (IFX_DMA_OWN << 31) |((byte_offset) << 23)| len;
    wmb();
#ifdef CONFIG_DMA_HW_POLL_DISABLED
    {
        unsigned int tmp;
//        DMA_IRQ_LOCK(pCh);
        DMA_IRQ_SPIN_LOCK(&dummy_chan);

        tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32((int)(pCh-dma_chan), IFX_DMA_CS_REG);
    /* trigger descriptor fetching by clearing DUR interrupt */
        IFX_REG_W32(DMA_CIS_DUR, IFX_DMA_CIS_REG);
        IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//        DMA_IRQ_UNLOCK(pCh);
        DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

    }
#endif /* CONFIG_DMA_HW_POLL_DISABLED */
    /* Increase descriptor index and process wrap around
    Handle it with care when use the dma_device_desc_setup fucntion**/
    if (!pCh->desc_handle ) {
        pCh->curr_desc++;
        if (pCh->curr_desc == pCh->desc_len) {
            pCh->curr_desc = 0;
        }
    }
    return IFX_SUCCESS;
}
EXPORT_SYMBOL(dma_device_desc_setup);

/*------------------------------------------------------------------------------------------*\
 * Clear the status word of the descriptor from the client driver once receive a pseudo interrupt(RCV_INT)
 * from the DMA module to avoid duplicate interrupts from tasklet.
 * This function used to exit from DMA tasklet(tasklet don't need to run again and again
 * once the DMA owner bit belongs to CPU i.e. packet is available in descriptor )
 * This is used to avoid multiple pseudo interrupt (RCV_INT) per packet.
 * All buffers are provided by DMA client. DMA client driver has no method to modify descriptor state.
 * The only method is to change descriptor status.
\*------------------------------------------------------------------------------------------*/
int dma_device_clear_desc_status_word(_dma_device_info *dma_dev, int dir, int chan_no) {
    struct rx_desc *rx_desc_p;
    struct tx_desc *tx_desc_p;
    _dma_channel_info *pCh=0;
    if (dir == IFX_DMA_TX_CH) {
        pCh = dma_dev->tx_chan[chan_no];
        if (!pCh->curr_desc) {
            tx_desc_p = (struct tx_desc *)pCh->desc_base;
            tx_desc_p->status.word = 0x0;
        } else {
            tx_desc_p = (struct tx_desc *)pCh->desc_base + (pCh->curr_desc-1);
            tx_desc_p->status.word = 0x0;
        }
        wmb();
    } else if (dir == IFX_DMA_RX_CH) {
        pCh = dma_dev->rx_chan[chan_no];
        if (!pCh->curr_desc) {
            rx_desc_p = (struct rx_desc *)pCh->desc_base;
            rx_desc_p->status.word = 0x0;
        } else {
            rx_desc_p = (struct rx_desc *)pCh->desc_base + (pCh->curr_desc -1);
            rx_desc_p->status.word = 0x0;
        }
        wmb();
    } else {
        printk(KERN_ERR "%s: Unknow channel direction\n", __func__);
        return IFX_ERROR;
    }
    return IFX_SUCCESS;
}
EXPORT_SYMBOL(dma_device_clear_desc_status_word);

/*------------------------------------------------------------------------------------------*\
 * This function used to setup the DMA class value..
\*------------------------------------------------------------------------------------------*/
void dma_device_setup_class_val(_dma_channel_info* pCh, int cls) {
    int tmp, chan_no = (int)(pCh - dma_chan);

//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    tmp = IFX_REG_R32(IFX_DMA_CS_REG);
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
    IFX_REG_W32((IFX_REG_R32(IFX_DMA_CCTRL_REG) | (DMA_CCTRL_CLASS_VAL_SET(cls))), IFX_DMA_CCTRL_REG);
    IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
}
EXPORT_SYMBOL(dma_device_setup_class_val);


/*------------------------------------------------------------------------------------------*\
 * This functions is called when the client driver gets a pseudo DMA interrupt(RCV_INT).
 * Handle with care when call this function as well as dma_device_desc_setup function.
 * Allocated memory data pointer should align with DMA burst length.
 * The follwoing action are done:
 * Get current receive descriptor
 * Allocate memory via buffer alloc callback function. If buffer allocation failed,
 * then return 0 and use the existing data pointer.
 * Pass valid descriptor data pointer to the client buffer pointer.
 * Update current descriptor position.
 * Clear the descriptor status once it's called when the buffer is not allocated with client driver and also set
 * desc_handle is 1
\*------------------------------------------------------------------------------------------*/
int dma_device_read(struct dma_device_info* dma_dev, unsigned char** dataptr, void** opt, int rx_chan_no) { //dataptr = &buf; opt = &skb
    unsigned char* buf;
    int len, byte_offset = 0;
    void* p = NULL;
    struct rx_desc* rx_desc_p;
    _dma_channel_info* pCh = dma_dev->rx_chan[rx_chan_no];

    DMA_IRQ_SPIN_LOCK(pCh);

    /* Get the rx data first */
    rx_desc_p = (struct rx_desc*)pCh->desc_base + pCh->curr_desc;
    if(unlikely(!(rx_desc_p->status.field.OWN == IFX_CPU_OWN && rx_desc_p->status.field.C))) {
        spin_unlock_irqrestore(&(pCh->irq_lock), __ilockflags);
        return -1;
    }
    buf = (u8*)__va(rx_desc_p->Data_Pointer);
    dma_cache_inv((unsigned long)buf, pCh->packet_size);
    mb();

    *(u32*)dataptr = (u32)buf;
    len = rx_desc_p->status.field.data_length;

    if (opt) {
        *(int*)opt = (int)pCh->opt[pCh->curr_desc];
    }

    /* Replace with a new allocated buffer */
    buf = dma_dev->buffer_alloc(pCh->packet_size, &byte_offset, &p);
    if(likely(buf != NULL)) {
        pCh->opt[pCh->curr_desc] = p;
#ifndef CONFIG_MIPS_UNCACHED
        BUG_ON( byte_offset > 3 );
#if defined(CONFIG_VR9)
        BUG_ON( (u32)buf & (128-1) ); // we see invalid data, if buffer is not 128-byte aligned
#endif

        /*
         * new allocated rx_skb might contain shared_info
         * this shared_info has to be written to memory, before
         * cache is invalidated!
         */
        if (likely( p )){

        	struct skb_shared_info *shinfo;
        	struct sk_buff* skb = p;

        	shinfo = skb_shinfo(skb);
        	dma_cache_wback((unsigned long)shinfo, sizeof(struct skb_shared_info));

        	// shared_info inside the buffer?! --> its not
        	// BUG_ON( ((unsigned char*)shinfo + sizeof(struct skb_shared_info)) > ((unsigned char *)buf + pCh->packet_size) );
        	// BUG_ON( (unsigned char*)shinfo < (unsigned  char *)buf);
        }

        mb();
        dma_cache_inv((unsigned long)buf, pCh->packet_size);
        mb();

#endif
        rx_desc_p->Data_Pointer = (u32)CPHYSADDR((u32)buf);
        rx_desc_p->status.word  = (IFX_DMA_OWN << 31) |((byte_offset) << 23) | pCh->packet_size;
        wmb();
    } else {
        printk(KERN_ERR "[%s] no buffer_alloc !!!!!!!!\n", __FUNCTION__ );

        /** It will handle client driver using the dma_device_desc_setup function.
        So, handle with care. */
        if (!pCh->desc_handle ) {
            *(u32*)dataptr = 0;
            if (opt) {
                *(int*)opt = 0;
            }
            len = 0;
            rx_desc_p->status.word  = ((IFX_DMA_OWN << 31) |((byte_offset) << 23) | pCh->packet_size);
            wmb();
        } else {
            /* Better to clear used descriptor status bits(C, SOP & EOP) to avoid
            multiple read access as well as to keep current descriptor pointers correctly */
            rx_desc_p->status.word  = 0;
        }
    }

    /* Increase descriptor index and process wrap around
    Handle with care when use the dma_device_desc_setup fucntion*/
    pCh->curr_desc++;
    if(unlikely(pCh->curr_desc == pCh->desc_len)) {
        pCh->curr_desc = 0;
    }

    DMA_IRQ_SPIN_UNLOCK(pCh);


#ifdef CONFIG_DMA_HW_POLL_DISABLED
//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    if (unlikely(pCh->dur)) {
        unsigned int tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32((int)(pCh-dma_chan), IFX_DMA_CS_REG);
        /* trigger descriptor fetching by clearing DUR interrupt */
        IFX_REG_W32(DMA_CIS_DUR, IFX_DMA_CIS_REG);
        IFX_REG_W32(tmp, IFX_DMA_CS_REG);
        pCh->dur = 0;
    }

//    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

#endif /* CONFIG_DMA_HW_POLL_DISABLED */
    return len;
}
EXPORT_SYMBOL(dma_device_read);



/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned long acquire_channel_lock(struct dma_device_info* dma_dev, int tx_chan_no){
    unsigned long lock_flags;
    _dma_channel_info* pCh;
    pCh = dma_dev->tx_chan[tx_chan_no];
    spin_lock_irqsave(&(pCh->irq_lock), lock_flags);
    return lock_flags;
}
EXPORT_SYMBOL(acquire_channel_lock);
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

void free_channel_lock(struct dma_device_info* dma_dev, int tx_chan_no, unsigned long lock_flags){
    _dma_channel_info* pCh;
    pCh = dma_dev->tx_chan[tx_chan_no];
    spin_unlock_irqrestore(&(pCh->irq_lock), lock_flags);
    return ;
}
EXPORT_SYMBOL(free_channel_lock);
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/

/* just for debugging - we dont wanna use this due to performance reasons! */
int dma_device_tx_buffs_avail(struct dma_device_info* dma_dev, int tx_chan_no) {
    _dma_channel_info* pCh;
    int free_buffers = 0;
    int check_desc;

    if (tx_chan_no >= dma_dev->max_tx_chan_num)
        return -1;

    pCh = dma_dev->tx_chan[tx_chan_no];

    DMA_IRQ_SPIN_LOCK(pCh);

    check_desc = pCh->curr_desc;
    do {
        struct tx_desc *tx_desc_p = (struct tx_desc*)pCh->desc_base + check_desc;

        if(tx_desc_p->status.field.OWN == IFX_DMA_OWN ){ // || tx_desc_p->status.field.C){
            break;
        } else {
            free_buffers += 1;
            check_desc = ( check_desc + 1 ) % (pCh->desc_len);
        }
    } while ( pCh->curr_desc !=  check_desc);


    DMA_IRQ_SPIN_UNLOCK(pCh);

    return free_buffers;

}
EXPORT_SYMBOL(dma_device_tx_buffs_avail);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int tx_channels_busy(struct dma_device_info* dma_dev) {
    int tx_chan_no;
    int busy_count = 0;

    for (tx_chan_no = 0; tx_chan_no < dma_dev->max_tx_chan_num; tx_chan_no++) {
        _dma_channel_info* pCh;
        int chan_no, free_descs;

        free_descs = dma_device_tx_buffs_avail(dma_dev, tx_chan_no);
        pCh = dma_dev->tx_chan[tx_chan_no];
        chan_no = (int)(pCh - (_dma_channel_info*)dma_chan);

        if (!pCh->loopback_enable &&
                !pCh->peri_to_peri &&
                !pCh->no_cpu_data &&
                is_enabled_chan(chan_no) &&
                ( free_descs < pCh->desc_len)){

            busy_count++;
        }
        printk(KERN_ERR "[%s][tx=%d, glob=%d, en=%d, loop=%d, peri=%d, no_cpu=%d ]: %d/%d\n", __func__,
                tx_chan_no, chan_no, is_enabled_chan(chan_no), pCh->loopback_enable, pCh->peri_to_peri,
                pCh->no_cpu_data, free_descs, pCh->desc_len);
    }
    return busy_count;
}
EXPORT_SYMBOL(tx_channels_busy);

/*------------------------------------------------------------------------------------------*\
 * This function gets the data packet from the client driver and sends over on DMA channel.
 * The following actions are done
 * Descriptor is not available then sends a pseudo DMA interrupt(TX_BUF_FULL_INT) to client driver to stop transmission.
 * The client driver should stop the transmission and enable the Tx dma channel interrupt.
 * Once descriptor is free then the DMA interrupt handler send an pseudo DMA interrupt(TRANSMIT_CPT_INT)
 * to client driver to start transmission.
 * The client driver should start the transmission and disable the Tx dma channel interrupt.
 * Previous descriptor already sendout, then Free memory via buffer free callback function
 * Update current descriptor position
 * If the channel is not open, then it will open.
\*------------------------------------------------------------------------------------------*/
int dma_device_write(struct dma_device_info* dma_dev, unsigned char* dataptr, int len_sop_eop_lock, void* opt, int tx_chan_no) {
    unsigned int tmp, reg_val, list_full = 0;
    _dma_channel_info* pCh;

    int chan_no, byte_offset, curr_desc;
    struct tx_desc *tx_desc_p,  *next_desc_p;

    int got_lock = len_sop_eop_lock & IFX_DMA_LOCK_BY_UPPER_LAYER;
    int begin = len_sop_eop_lock & IFX_DMA_NOT_SOP ? 0 : IFX_DMA_DESC_SOP_SET;
    int ende  = len_sop_eop_lock & IFX_DMA_NOT_EOP ? 0 : IFX_DMA_DESC_EOP_SET;
    int len = len_sop_eop_lock & ~(IFX_DMA_NOT_SOP | IFX_DMA_NOT_EOP | IFX_DMA_LOCK_BY_UPPER_LAYER);

    unsigned long __ilockflags;                     \

    pCh = dma_dev->tx_chan[tx_chan_no];
    chan_no = (int)(pCh - (_dma_channel_info*)dma_chan);

    /*--------------------------------------------------------------------------------------*\
     * Set the previous descriptor pointer to verify data is sendout or not 
     * If its sendout then clear the buffer based on the client driver buffer free callaback function 
    \*--------------------------------------------------------------------------------------*/
#if defined (CONFIG_AVM_SCATTER_GATHER)
    if (unlikely(!got_lock))
#else
    if (likely(!got_lock))
#endif
        spin_lock_irqsave(&(pCh->irq_lock), __ilockflags);
    do{
        tx_desc_p = (struct tx_desc*)pCh->desc_base + pCh->prev_desc;

        if(tx_desc_p->status.field.OWN == IFX_CPU_OWN && tx_desc_p->status.field.C){
            dma_dev->buffer_free((u8*)__va(tx_desc_p->Data_Pointer), pCh->opt[pCh->prev_desc]);
            memset(tx_desc_p, 0, sizeof(struct tx_desc));

            pCh->prev_desc = (pCh->prev_desc + 1) % (pCh->desc_len);
        } else {
            tx_desc_p = NULL;
        }
    }while(tx_desc_p);

    wmb();

    curr_desc = pCh->curr_desc;
    tx_desc_p = (struct tx_desc*)pCh->desc_base + curr_desc;

    if(unlikely(tx_desc_p->status.field.OWN == IFX_DMA_OWN || tx_desc_p->status.field.C)){
        /* If the current descriptor is owned by DMA, we have run out of descriptors.
         * If it is owned by the CPU, but still has the completion bit set, its buffer has
         * not been freed, so we can not use it and are out of descriptors as well. */
        tx_desc_p = NULL;
    } else {
        /* we have a free descriptor, advance the current descriptor pointer */
        pCh->curr_desc = (pCh->curr_desc + 1) % (pCh->desc_len);
    }

    /*
     * check if the descriptor list is full
     */

    if ( pCh->scatter_gather_channel ) {
        // we don't wanna know if the next buffer is free
        // we need to know if MAX_SKB_FRAGS are free

        int scatter_gather_range_end = ( pCh->curr_desc + MAX_SKB_FRAGS - 1 ) % (pCh->desc_len);
        next_desc_p = (struct tx_desc*)pCh->desc_base + scatter_gather_range_end;
    }
    else {
        next_desc_p = (struct tx_desc*)pCh->desc_base + pCh->curr_desc;
    }
    if (unlikely(next_desc_p->status.field.OWN == IFX_DMA_OWN)){
        list_full = 1;
    }


#if defined (CONFIG_AVM_SCATTER_GATHER)
    if (unlikely(!got_lock))
#else
    if (likely(!got_lock))
#endif
        spin_unlock_irqrestore(&(pCh->irq_lock), __ilockflags);

    if(unlikely(list_full)){
        if (printk_ratelimit())
            printk(KERN_ERR "TX_BUF_FULL_INT (global_full_irq_cnt=%d, global_complete_irq_cnt=%d) tx_chan = %d\n",
                global_channel_full_irq_counter, global_channel_complete_irq_counter, tx_chan_no);
        dma_dev->intr_handler(dma_dev, TX_BUF_FULL_INT, tx_chan_no);
        pCh->channel_full_irq_counter++;
        global_channel_full_irq_counter++;

        if (pCh->channel_hangs_since){
            BUG_ON(time_after(jiffies, (pCh->channel_hangs_since + 5 * HZ)));
        } else {
            pCh->channel_hangs_since = jiffies;
        }
    }

    if(likely(tx_desc_p)){
        pCh->opt[curr_desc] = opt;
        /** Adjust the starting address of the data buffer, should be allign with DMA burst length. */
        /*--- byte_offset = ((u32)CPHYSADDR((u32)dataptr))%((dma_dev->tx_burst_len) * 4); ---*/
        byte_offset = ((u32)CPHYSADDR((u32)dataptr)) & (((dma_dev->tx_burst_len) << 2) - 1);
        BUG_ON(byte_offset > 31); // wird vom controller nicht unterstuetzt

#ifndef CONFIG_MIPS_UNCACHED
#if defined (CONFIG_AMAZON_SE)
        dma_cache_wback_inv ((unsigned long) dataptr - byte_offset, len + byte_offset);
#else
        dma_cache_wback((unsigned long) dataptr - byte_offset, len + byte_offset);
#endif /* CONFIG_AMAZON_SE */
#endif  /* CONFIG_MIPS_UNCACHED */

        tx_desc_p->Data_Pointer = (u32)CPHYSADDR((u32)dataptr)- byte_offset;
        wmb();

        tx_desc_p->status.word = (IFX_DMA_OWN << 31) | begin | ende | ((byte_offset) << 23) | len;
        wmb();
    }



    if(likely(tx_desc_p)){
        DMA_IRQ_SPIN_LOCK(&dummy_chan);

        tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
        reg_val = IFX_REG_R32(IFX_DMA_CCTRL_REG);
        IFX_REG_W32(tmp, IFX_DMA_CS_REG);

        DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

        /* If not open this channel, open it */
        if (unlikely(!(reg_val & DMA_CCTRL_ON))) {
            printk(KERN_ERR "[%s] auto open tx_chan_no:%d\n", __func__, tx_chan_no);
            pCh->open(pCh);
        }

#ifdef CONFIG_DMA_HW_POLL_DISABLED
//      DMA_IRQ_LOCK(pCh);
        DMA_IRQ_SPIN_LOCK(&dummy_chan);

        tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32(chan_no, IFX_DMA_CS_REG);

        /* trigger descriptor fetching by clearing DUR interrupt */
        IFX_REG_W32(DMA_CIS_DUR, IFX_DMA_CIS_REG);

        IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//      DMA_IRQ_UNLOCK(pCh);
        DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

#endif /* CONFIG_DMA_HW_POLL_DISABLED */
    } else {
        len = 0;
    }

    return len;
}
EXPORT_SYMBOL(dma_device_write);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int dma_device_write_sdio (struct dma_device_info *dma_dev, u32 *chunkdataptr, void *opt, int tx_chan_no) {
    unsigned int tmp, byte_offset,tcs, status_word;
    _dma_channel_info *pCh;
    int chan_no, len = 0;
    int total_length, cur_length, cur_chunk;
    struct tx_desc *tx_desc_p;
    struct tx_desc *first_tx_desc;
    u8 *dataptr;

    pCh = dma_dev->tx_chan[tx_chan_no];
    chan_no = (int) (pCh - (_dma_channel_info *) dma_chan);
    tx_desc_p = (struct tx_desc *) pCh->desc_base + pCh->prev_desc;
    while (tx_desc_p->status.field.OWN == IFX_CPU_OWN && tx_desc_p->status.field.C) {
        dma_dev->buffer_free ((u8 *) __va (tx_desc_p->Data_Pointer),pCh->opt[pCh->prev_desc]);
        memset (tx_desc_p, 0, sizeof (struct tx_desc));
        pCh->prev_desc = (pCh->prev_desc + 1) % (pCh->desc_len);
        tx_desc_p = (struct tx_desc *) pCh->desc_base + pCh->prev_desc;
    }

    total_length = chunkdataptr[0];
    cur_length = 0;
    cur_chunk = 0;

/*  printk("  %s: total_length is %08X\n", __func__, total_length); */
    first_tx_desc = (struct tx_desc *) pCh->desc_base + pCh->curr_desc;
    while (cur_length < total_length) {
        len = chunkdataptr[1+cur_chunk];
        dataptr = (u8*)chunkdataptr[2+cur_chunk];
        cur_length += len;

        tx_desc_p = (struct tx_desc *) pCh->desc_base + pCh->curr_desc;

/*  printk("%s: Add data chunk (%d, %08X) at descr %08X}\n", */
/*      __func__, len, (uint32_t)dataptr, (uint32_t)tx_desc_p); */

/*Check whether this descriptor is available */
        if (tx_desc_p->status.field.OWN == IFX_DMA_OWN || tx_desc_p->status.field.C) {
            /*if not , the tell the upper layer device */
            dma_dev->intr_handler (dma_dev, TX_BUF_FULL_INT, tx_chan_no);
            /*--- printk (KERN_INFO "%s : failed to write!\n", __func__); ---*/
            return 0;
        }
        pCh->opt[pCh->curr_desc] = opt;
        /*byte offset----to adjust the starting address of the data buffer, should be multiple of the burst length. */
        byte_offset = ((u32) CPHYSADDR ((u32) dataptr)) % ((dma_dev->tx_burst_len) * 4);
#ifndef	CONFIG_MIPS_UNCACHED
        dma_cache_wback_inv ((unsigned long) dataptr, len);	/* dma_device_write */
#endif //CONFIG_MIPS_UNCACHED
        byte_offset = byte_offset & 0x1f;
        tx_desc_p->Data_Pointer = (u32) CPHYSADDR ((u32) dataptr) - byte_offset;
        wmb();
        /* For the first descriptor for this packet, we do not set the OWN bit to DM */
        /* to prevent starting the DMA before everything has been set up */
        status_word = ((byte_offset) << 23 /* DESCRIPTER_TX_BYTE_OFFSET */)| len;
        if (cur_chunk == 0) { /* First data packet for this transaction */
            status_word |= IFX_DMA_DESC_SOP_SET;
        } else{
            status_word |= (IFX_DMA_OWN << 31 /* DESCRIPTER_OWN_BIT_OFFSET */);
        }

        if (cur_length == total_length) { /* Last data packet */
            status_word |= IFX_DMA_DESC_EOP_SET;
        }
/*             printk("  %s: status_word is %08X\n", __func__, status_word); */
        tx_desc_p->status.word = status_word;
        wmb();

        pCh->curr_desc++;
        if (pCh->curr_desc == pCh->desc_len) {
            pCh->curr_desc = 0;
        }
        cur_chunk += 2;     /* 2 words per entry (length, dataptr) */
    }
    /* Now let the DMA start working, by releasing the first descriptor for this packet. */
    /*Check whether this descriptor is available */
    tx_desc_p = (struct tx_desc *) pCh->desc_base + pCh->curr_desc;
    if (tx_desc_p->status.field.OWN == IFX_DMA_OWN) {
        /*if not , the tell the upper layer device */
        dma_dev->intr_handler (dma_dev, TX_BUF_FULL_INT, tx_chan_no);
    }

//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    tcs = IFX_REG_R32(IFX_DMA_CS_REG);
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
    tmp = IFX_REG_R32(IFX_DMA_CCTRL_REG);
    first_tx_desc->status.word |= (IFX_DMA_OWN << 31 /* DESCRIPTER_OWN_BIT_OFFSET */);
/*  printk("first_tx_desc->status.word=%08X\n", first_tx_desc->status.word); */
    IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CIS_REG) | DMA_CIS_ALL), IFX_DMA_CIS_REG);
    IFX_REG_W32(tcs, IFX_DMA_CS_REG);

//    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

	if (!(tmp & 1)) {
        pCh->open (pCh);
    }

#ifdef CONFIG_DMA_HW_POLL_DISABLED
//    DMA_IRQ_LOCK(pCh);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    tmp = IFX_REG_R32(IFX_DMA_CS_REG);
    IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
    /* trigger descriptor fetching by clearing DUR interrupt */
    IFX_REG_W32(DMA_CIS_DUR, IFX_DMA_CIS_REG);
    IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//    DMA_IRQ_UNLOCK(pCh);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

#endif /* CONFIG_DMA_HW_POLL_DISABLED */
    return len;
}
EXPORT_SYMBOL(dma_device_write_sdio);

/*------------------------------------------------------------------------------------------*\
 * This function keeps polls the DMA owner bit status.
 * if valid packets available then  send a pseudo interrupt to the client driver
\*------------------------------------------------------------------------------------------*/
#if 0
int dma_device_poll(struct dma_device_info* dma_dev, int work_to_do, int *work_done) {
#ifdef CONFIG_NAPI_ENABLED
    int ret, i,dma_int_status_mask, chan_no = 0;
    unsigned int tmp, cis;
    _dma_channel_info *pCh;
    struct rx_desc *rx_desc_p;

    dma_int_status_mask = 0;
    for (i = 0; i < dma_dev->max_rx_chan_num; i++) {
        dma_int_status_mask |= 1 << (dma_dev->rx_chan[i] - dma_chan);
    }
    i = 0;
    while ((g_dma_poll_int_status & dma_int_status_mask)) {
        chan_no = dma_dev->rx_chan[i] - dma_chan;
        pCh = &dma_chan[chan_no];
        rx_desc_p = (struct rx_desc*)pCh->desc_base + pCh->curr_desc;

        if (rx_desc_p->status.field.OWN == IFX_CPU_OWN && rx_desc_p->status.field.C) {
            if (dma_dev->intr_handler && !dma_dev->rx_chan[i]->no_cpu_data){
                dma_dev->intr_handler(dma_dev, RCV_INT, pCh->rel_chan_no);
            }
//            DMA_IRQ_LOCK(pCh);
            DMA_IRQ_SPIN_LOCK(&dummy_chan);

            tmp = IFX_REG_R32(IFX_DMA_CS_REG);
            IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
            cis = IFX_REG_R32(IFX_DMA_CIS_REG);
            IFX_REG_W32((cis | DMA_CIS_ALL), IFX_DMA_CIS_REG);
            IFX_REG_W32(tmp, IFX_DMA_CS_REG);

#ifdef CONFIG_DMA_HW_POLL_DISABLED
        /* remember that we ran out of descriptors, don't trigger polling now */
            if (cis & DMA_CIS_DUR)
                pCh->dur = 1;
#endif /* CONFIG_DMA_HW_POLL_DISABLED */

//            DMA_IRQ_UNLOCK(pCh);
            DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

            if (++*work_done >= work_to_do) {
                return 1;
            }
        } else {
//            DMA_IRQ_LOCK(pCh);
            DMA_IRQ_SPIN_LOCK(&dummy_chan);

            /* Clear the DMA Node status register bit to remove the dummy interrupts */
            IFX_REG_W32((1 << chan_no), IFX_DMA_IRNCR_REG);
            tmp = IFX_REG_R32(IFX_DMA_CS_REG);
            IFX_REG_W32(chan_no, IFX_DMA_CS_REG);
            cis = IFX_REG_R32(IFX_DMA_CIS_REG);

            /*
             * In most cases, interrupt bit has been cleared in the first branch, it will
             * hit the following. However, in some corner cases, the above branch doesn't happen,
             * so it indicates that interrupt is received actually, but OWN bit and
             * Complete bit not update yet, so go back to DMA tasklet and let DMA tasklet
             * wait <polling>. This is the most efficient way.
             * NB, only EOP and Descript Complete interrupt enabled in Channel Interrupt Enable Register.
             */
            if ((cis & IFX_REG_R32(IFX_DMA_CIE_REG)) == 0) {
                if (rx_desc_p->status.field.OWN != IFX_CPU_OWN) {
                    g_dma_poll_int_status &= ~(1 << chan_no);
                    mod_timer(&(dma_fix_timer[chan_no]), jiffies + 5);
                }
                /*
                 * Enable this channel interrupt again after processing all packets available
                 * Placing this line at the last line will avoid interrupt coming again
                 * in heavy load to great extent
                 */
                IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);
            }
            IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//            DMA_IRQ_UNLOCK(pCh);
            DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
        }
#ifdef CONFIG_DMA_HW_POLL_DISABLED
    /* remember that we ran out of descriptors, don't trigger polling now */
        if (cis & DMA_CIS_DUR)
            pCh->dur = 1;
#endif /* CONFIG_DMA_HW_POLL_DISABLED */
        if (++i == dma_dev->max_rx_chan_num)
            i = 0;
    }

//    DMA_IRQ_LOCK(dma_dev);
    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    if ((g_dma_poll_int_status & dma_int_status_mask) == 0) {
        if (dma_dev->inactivate_poll)
            dma_dev->inactivate_poll(dma_dev);
        ret = 0;
    }else {
        ret = 1;
    }

//    DMA_IRQ_UNLOCK(dma_dev);
    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

    return ret;
#else
    return 0;
#endif /* CONFIG_NAPI_ENABLED */
}
#endif
#if 1
int dma_device_poll(struct dma_device_info* dma_dev, int work_to_do, int *work_done) {
#ifdef CONFIG_NAPI_ENABLED
    int ret, i,dma_int_status_mask, no_cpu_data_mask, chan_no = 0;
    unsigned int tmp, cis, cie;
    unsigned int polled_dma_chans = 0;
    _dma_channel_info *pCh;
    struct rx_desc *rx_desc_p;

    /*
     * prepare masks for channels.
     * - dma_int_status_mask contains the channels we have to handle
     * - no_cpu_data_mask contains RX loop back channels that may cause IRQs but are not
     *   handled by the CPU.
     */
    dma_int_status_mask = 0;
    no_cpu_data_mask = 0;
    for (i = 0; i < dma_dev->max_rx_chan_num; i++) {
        chan_no = dma_dev->rx_chan[i] - dma_chan;
        if( dma_dev->rx_chan[i]->no_cpu_data){
            no_cpu_data_mask |= 1 << chan_no;
        }
        dma_int_status_mask |= 1 << chan_no;
    }

    i = dma_dev->max_rx_chan_num;
    while ((g_dma_poll_int_status & dma_int_status_mask)) {

        if (unlikely(++i >= dma_dev->max_rx_chan_num)) {
            i = 0;
        }

        chan_no = dma_dev->rx_chan[i] - dma_chan;

        /*
         * skip channel if its IRQ-flag is not set
         */
        if(likely( !(g_dma_poll_int_status & (1 << chan_no)) )){
            continue;
        }

        pCh = &dma_chan[chan_no];
        rx_desc_p = (struct rx_desc*)pCh->desc_base + pCh->curr_desc;

        /*
         * just acknowledge all IRQs if channel is no_cpu_data
         */
        if(unlikely(no_cpu_data_mask & (1 << chan_no))){
            DMA_IRQ_SPIN_LOCK(&dummy_chan);

            tmp = IFX_REG_R32(IFX_DMA_CS_REG);
            IFX_REG_W32(chan_no, IFX_DMA_CS_REG);

            cis = IFX_REG_R32(IFX_DMA_CIS_REG);
            IFX_REG_W32((cis & DMA_CIS_ALL), IFX_DMA_CIS_REG);
            IFX_REG_W32(tmp, IFX_DMA_CS_REG);

            g_dma_poll_int_status &= ~(1 << chan_no);
            IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);

            DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
            continue;
        }


        /*
         * handle pending RX descriptor
         */
        if (likely(rx_desc_p->status.field.OWN == IFX_CPU_OWN && rx_desc_p->status.field.C) ){
            if (dma_dev->intr_handler) {
                dma_dev->intr_handler(dma_dev, RCV_INT, pCh->rel_chan_no);
            }

            DMA_IRQ_SPIN_LOCK(&dummy_chan);

            tmp = IFX_REG_R32(IFX_DMA_CS_REG);
            IFX_REG_W32(chan_no, IFX_DMA_CS_REG);

            cis = IFX_REG_R32(IFX_DMA_CIS_REG);
            if(cis & (DMA_CIS_RDERR | DMA_CIS_DUR)){
                dbg_verbose( "CIS: %08x\n", cis);
            }

            IFX_REG_W32((cis & DMA_CIS_ALL), IFX_DMA_CIS_REG);
            IFX_REG_W32(tmp, IFX_DMA_CS_REG);

            DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
#ifdef CONFIG_DMA_HW_POLL_DISABLED
        /* remember that we ran out of descriptors, don't trigger polling now */
            if (cis & DMA_CIS_DUR)
                pCh->dur = 1;
#endif /* CONFIG_DMA_HW_POLL_DISABLED */


            if(unlikely(++*work_done >= work_to_do)){
                return 1;
            }
        } else {
            /*
             * no pending RX descriptors left, clear IRQ flag for this channel
             */
            DMA_IRQ_SPIN_LOCK(&dummy_chan);

            /* Clear the DMA Node status register bit to remove the dummy interrupts */
            tmp = IFX_REG_R32(IFX_DMA_CS_REG);
            IFX_REG_W32(chan_no, IFX_DMA_CS_REG);

            cie = IFX_REG_R32(IFX_DMA_CIE_REG);
            cis = IFX_REG_R32(IFX_DMA_CIS_REG);

            /*
             * In most cases, interrupt bit has been cleared in the first branch, it will
             * hit the following. However, in some corner cases, the above branch doesn't happen,
             * so it indicates that interrupt is received actually, but OWN bit and
             * Complete bit not update yet, so go back to DMA tasklet and let DMA tasklet
             * wait <polling>. This is the most efficient way.
             * NB, only EOP and Descript Complete interrupt enabled in Channel Interrupt Enable Register.
             */
            if(likely((cis & cie) == 0 && rx_desc_p->status.field.OWN == IFX_DMA_OWN)){
                g_dma_poll_int_status &= ~(1 << chan_no);
                polled_dma_chans |= 1 << chan_no;
                /*
                 * Enable this channel interrupt again after processing all packets available
                 * Placing this line at the last line will avoid interrupt coming again
                 * in heavy load to great extent
                 */
                IFX_REG_W32((IFX_REG_R32(IFX_DMA_IRNEN_REG) | (1 << chan_no)), IFX_DMA_IRNEN_REG);
            }
            IFX_REG_W32(tmp, IFX_DMA_CS_REG);

            DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
        }
#ifdef CONFIG_DMA_HW_POLL_DISABLED
    /* remember that we ran out of descriptors, don't trigger polling now */
        if (cis & DMA_CIS_DUR)
            pCh->dur = 1;
#endif /* CONFIG_DMA_HW_POLL_DISABLED */
    }

    DMA_IRQ_SPIN_LOCK(&dummy_chan);

    /*
     * if we fetched all pending descriptors, turn of napi and schedule dma_fix_timers
     * for all polled channels
     */
    if(likely((g_dma_poll_int_status & dma_int_status_mask) == 0)){
        if(likely(dma_dev->inactivate_poll)) {
            dma_dev->inactivate_poll(dma_dev);
        }
        for(i = 0; i < dma_dev->max_rx_chan_num; ++i){
            chan_no = dma_dev->rx_chan[i] - dma_chan;
            if(unlikely(polled_dma_chans & (1 << chan_no))){
#if 0 && defined(CONFIG_VR9) || defined(CONFIG_AR10)
                mod_timer(&(dma_fix_timer[chan_no]), jiffies + 5);
#endif
            }
        }
        ret = 0;
    }else {
        ret = 1;
    }

    DMA_IRQ_SPIN_UNLOCK(&dummy_chan);

    return ret;
#else
    return 0;
#endif /* CONFIG_NAPI_ENABLED */
}
#endif
EXPORT_SYMBOL(dma_device_poll);

/*------------------------------------------------------------------------------------------*\
 * Display DMA channel descriptor list via proc file
 *  proc_read_desc_list - Create proc file output
 *  @buf:      Buffer to write the string to
 *  @start:    not used (Linux internal)
 *  @offset:   not used (Linux internal)
 *  @count:    not used (Linux internal)
 *  @eof:      Set to 1 when all data is stored in buffer
 *  @data:     not used (Linux internal)
 *
 *  This function creates the output for descriptos status bits.
 *
 *      Return Value:
 *  @len = Lenght of data in buffer
\*------------------------------------------------------------------------------------------*/
static int print_desc_list(char *buf, int chan_no, int desc_pos)
{
    int len = 0;
    u32 *p;

    p = (u32*)dma_chan[chan_no].desc_base;
    if ( desc_pos == 0 ) {
        if (dma_chan[chan_no].dir == IFX_DMA_RX_CH) {
            len += sprintf(buf + len, "channel %d %s Rx descriptor list:\n", \
                chan_no, ((_dma_device_info *)dma_chan[chan_no].dma_dev)->device_name);
        } else {
            len += sprintf(buf + len,"channel %d %s Tx descriptor list:\n", \
                chan_no, ((_dma_device_info *)dma_chan[chan_no].dma_dev)->device_name);
        }
        len += sprintf(buf + len," no  address        data pointer command bits (Own, Complete, SoP, EoP, Offset) \n");
        len += sprintf(buf + len,"---------------------------------------------------------------------------------\n");
    }
    len += sprintf(buf + len,"%3d  ",desc_pos);
    len += sprintf(buf + len,"0x%08x     ", (u32)(p + (desc_pos * 2)));
    len += sprintf(buf + len,"%08x     ", *(p + (desc_pos * 2 + 1)));
    len += sprintf(buf + len,"%08x     ", *(p + (desc_pos * 2)));

    if(*(p + (desc_pos * 2)) & 0x80000000)
        len += sprintf(buf + len, "D ");
    else
    	len += sprintf(buf + len, "C ");
    if(*(p + (desc_pos * 2)) & 0x40000000)
        len += sprintf(buf + len, "C ");
    else
        len += sprintf(buf + len, "c ");
    if(*(p + (desc_pos * 2)) & 0x20000000)
        len += sprintf(buf + len, "S ");
    else
        len += sprintf(buf + len, "s ");
    if(*(p + (desc_pos * 2)) & 0x10000000)
        len += sprintf(buf + len, "E ");
    else
        len += sprintf(buf + len, "e ");
    /* byte offset is different for rx and tx descriptors*/
    if (dma_chan[chan_no].dir == IFX_DMA_RX_CH) {
        len += sprintf(buf + len, "%01x ", (*(p + (desc_pos * 2)) & 0x01800000) >> 23);
    } else {
        len += sprintf(buf + len,"%02x ", (*(p + (desc_pos * 2)) & 0x0F800000) >> 23);
    }
    if( dma_chan[chan_no].curr_desc == desc_pos )
        len += sprintf(buf + len,"<- CURR");
    if( dma_chan[chan_no].prev_desc == desc_pos )
        len += sprintf(buf + len,"<- PREV");
    len += sprintf(buf + len,"\n");

    return len;
}


static int proc_write_desc_list (struct file *file __attribute__ ((unused)),
                          const char *buf,
                          unsigned long count,
                          void *data __attribute__ ((unused)))
{
	int len;
	int channr;
    len = count < sizeof(dma_proc_read_buff) ? count : sizeof(dma_proc_read_buff) - 1;
    copy_from_user(dma_proc_read_buff, buf, len);
    dma_proc_read_buff[len] = 0;

 	min_chan_dump = 0;
    max_chan_dump = MAX_DMA_CHANNEL_NUM;
    dbg_verbose( "parsing '%s'\n", dma_proc_read_buff  );
    if (len >= strlen("all")){
    	if (strncmp(dma_proc_read_buff, "all", strlen("all")) == 0){
    		return count;
    	}
    }
    sscanf(dma_proc_read_buff, "%d", &channr);
    if (channr >=0 && channr < MAX_DMA_CHANNEL_NUM){
    	min_chan_dump = channr ;
    	max_chan_dump = channr + 1;
    }
    dbg_verbose( "min=%d, max=%d \n", min_chan_dump, max_chan_dump );

    return count;
}

static int proc_read_desc_list(IFX_char_t *page, IFX_char_t **start, off_t off,
                         int count, int *eof, void *data)
{
    int len = 0;
    int len_max = off + count;
    char *pstr;
    char *str;
    int llen;
    int i, j;

    pstr = *start = page;

    str = vmalloc(1024);
    if(!str) {
        return -ENOMEM;
    }

    __sync();

    for ( i = min_chan_dump; i < max_chan_dump; i++ ) {
        for ( j = 0; j < dma_chan[i].desc_len; j++ ) {
            llen = print_desc_list(str, i, j);
            if ( len <= off && len + llen > off ) {
                memcpy(pstr, str + off - len, len + llen - off);
                pstr += len + llen - off;
            }
            else if ( len > off ) {
                memcpy(pstr, str, llen);
                pstr += llen;
            }
            len += llen;
            if ( len >= len_max )
                goto PROC_READ_DESC_LIST_OVERRUN_END;
        }
    }

    *eof = 1;

    vfree(str);

    return len - off;

PROC_READ_DESC_LIST_OVERRUN_END:
    vfree(str);

    return len - llen - off;
}

/*------------------------------------------------------------------------------------------*\
 * Driver version info
\*------------------------------------------------------------------------------------------*/
static inline int dma_core_drv_ver(char *buf) {
    return sprintf(buf, "IFX DMA driver, version %s,(c)2009 Infineon Technologies AG\n", version);
}

/*------------------------------------------------------------------------------------------*\
 * Displays the version of DMA module via proc file
\*------------------------------------------------------------------------------------------*/
static int dma_core_proc_version(IFX_char_t *buf, IFX_char_t **start, off_t offset, int count, int *eof, void *data) {
    int len = 0;
    /* No sanity check cos length is smaller than one page */
    len += dma_core_drv_ver(buf + len);
    *eof = 1;
    return len;
}

/*------------------------------------------------------------------------------------------*\
 * Displays the weight of all DMA channels via proc file
 *  proc_read_channel_weigh - Create proc file output
 *  @buf:      Buffer to write the string to
 *  @start:    not used (Linux internal)
 *  @offset:   not used (Linux internal)
 *  @count:    not used (Linux internal)
 *  @eof:      Set to 1 when all data is stored in buffer
 *  @data:     not used (Linux internal)
 *
 *  This function creates the output for the channels weight.
 *
 *      Return Value:
 *  @len = Lenght of data in buffer
\*------------------------------------------------------------------------------------------*/
static int proc_read_channel_weigh(IFX_char_t *buf, IFX_char_t **start, off_t offset, int count, int *eof, void *data) {
    int i, len = 0;

    len += sprintf(buf + len, "Qos dma channel weight list\n");
    len += sprintf(buf + len, "channel_num default_weight current_weight    device    Tx/Rx\n");
    len += sprintf(buf + len, "------------------------------------------------------------\n");
    for (i = 0; i < MAX_DMA_CHANNEL_NUM; i++) {
        _dma_channel_info* pCh = &dma_chan[i];
        if (pCh->dir == IFX_DMA_RX_CH) {
            len += sprintf( buf + len,"     %2d      %08x        %08x      %10s   Rx\n",    \
                i, pCh->default_weight, pCh->weight, ((_dma_device_info *)(pCh->dma_dev))->device_name);
        } else {
            len += sprintf( buf + len,"     %2d      %08x        %08x      %10s   Tx\n",    \
                i, pCh->default_weight, pCh->weight, ((_dma_device_info *)(pCh->dma_dev))->device_name);
        }
    }
    *eof = 1;
    return len;
}

#ifdef DEBUG_REGISTERS
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void dma_register_dump(void) {
    int i;
    static int blockcount = 0, channel_no = 0;
    dbg_verbose( "\nGeneral DMA Registers\n");
    dbg_verbose( "-----------------------------------------\n");
    dbg_verbose(  "CLC=        %08x\n", IFX_REG_R32(IFX_DMA_CLC_REG) );
    dbg_verbose(  "ID=         %08x\n", IFX_REG_R32(IFX_DMA_ID_REG) );
    dbg_verbose(  "DMA_CPOLL=  %08x\n", IFX_REG_R32(IFX_DMA_CPOLL_REG) );
    dbg_verbose(  "DMA_CS=     %08x\n", IFX_REG_R32(IFX_DMA_CS_REG) );
    dbg_verbose(  "DMA_PS=     %08x\n", IFX_REG_R32(IFX_DMA_PS_REG) );
    dbg_verbose(  "DMA_IRNEN=  %08x\n", IFX_REG_R32(IFX_DMA_IRNEN_REG) );
    dbg_verbose(  "DMA_IRNCR=  %08x\n", IFX_REG_R32(IFX_DMA_IRNCR_REG) );
    dbg_verbose(  "DMA_IRNICR= %08x\n", IFX_REG_R32(IFX_DMA_IRNICR_REG) );
    dbg_verbose( "\nDMA Channel Registers\n");
    for(i = channel_no; i < MAX_DMA_CHANNEL_NUM; i++) {
        _dma_channel_info* pCh = &dma_chan[i];
        dbg_verbose(  "-----------------------------------------\n");
        if ( pCh->dir == IFX_DMA_RX_CH) {
            dbg_verbose(  "Channel %d - Device %s Rx\n",    \
                    i, ((_dma_device_info *)(pCh->dma_dev))->device_name);
        } else {
            dbg_verbose(  "Channel %d - Device %s Tx\n",    \
                    i, ((_dma_device_info *)(pCh->dma_dev))->device_name);
        }
        IFX_REG_W32(i, IFX_DMA_CS_REG);
        dbg_verbose(  "DMA_CCTRL=  %08x\n", IFX_REG_R32(IFX_DMA_CCTRL_REG));
        dbg_verbose(  "DMA_CDBA=   %08x\n", IFX_REG_R32(IFX_DMA_CDBA_REG));
        dbg_verbose(  "DMA_CIE=    %08x\n", IFX_REG_R32(IFX_DMA_CIE_REG));
        dbg_verbose(  "DMA_CIS=    %08x\n", IFX_REG_R32(IFX_DMA_CIS_REG));
        dbg_verbose(  "DMA_CDLEN=  %08x\n", IFX_REG_R32(IFX_DMA_CDLEN_REG));
    }
    dbg_verbose(  "\nDMA Port Registers\n");
    dbg_verbose(  "-----------------------------------------\n");
    for(i = 0; i < MAX_DMA_DEVICE_NUM; i++) {
        IFX_REG_W32(i, IFX_DMA_PS_REG);
        dbg_verbose( "Port %d DMA_PCTRL= %08x\n", i, IFX_REG_R32(IFX_DMA_PCTRL_REG) );
    }
}
#endif /*--- #ifdef DEBUG_REGISTERS ---*/

static int proc_read_dma_reset(IFX_char_t *buf, IFX_char_t **start, off_t offset, int count, int *eof, void *data) {
	*eof = 1;
    return 0;
}

static int proc_write_dma_reset(struct file *file __attribute__ ((unused)),
                          const char *buf,
                          unsigned long count,
                          void *data __attribute__ ((unused)))
{
	int len;
	int channr;
    len = count < sizeof(dma_proc_read_buff) ? count : sizeof(dma_proc_read_buff) - 1;
    copy_from_user(dma_proc_read_buff, buf, len);
    dma_proc_read_buff[len] = 0;
    sscanf(dma_proc_read_buff, "reset chan %d", &channr);
    if (channr >=0 && channr < MAX_DMA_CHANNEL_NUM){
    	dbg_verbose( "resetting channel %d \n", channr);
    	reset_chan( &dma_chan[channr] );
    }

    return count;
}

static int proc_read_irq_reset(IFX_char_t *buf, IFX_char_t **start, off_t offset, int count, int *eof, void *data) {
    *eof = 1;
    return 0;
}

static int proc_write_irq_reset(struct file *file __attribute__ ((unused)),
                                const char *buf,
                                unsigned long count,
                                void *data __attribute__ ((unused)))
{
    int len;
    int channr;
    unsigned int tmp, cis;

    len = count < sizeof(dma_proc_read_buff) ? count : sizeof(dma_proc_read_buff) - 1;
    copy_from_user(dma_proc_read_buff, buf, len);
    dma_proc_read_buff[len] = 0;
    sscanf(dma_proc_read_buff, "reset chan %d", &channr);
    if(channr >= 0 && channr < MAX_DMA_CHANNEL_NUM){
        dbg_verbose( "Acknowledging all IRQs for channel %d \n", channr);

//        DMA_IRQ_LOCK(pCh);
        DMA_IRQ_SPIN_LOCK(&dummy_chan);

        tmp = IFX_REG_R32(IFX_DMA_CS_REG);
        IFX_REG_W32(channr, IFX_DMA_CS_REG);

        cis = IFX_REG_R32(IFX_DMA_CIS_REG);
        IFX_REG_W32((cis | DMA_CIS_ALL), IFX_DMA_CIS_REG);

        IFX_REG_W32(tmp, IFX_DMA_CS_REG);

//        DMA_IRQ_UNLOCK(pCh);
        DMA_IRQ_SPIN_UNLOCK(&dummy_chan);
    }

    return count;
}

/*------------------------------------------------------------------------------------------*\
 *   Provides DMA Register Content via proc file
 *   This function reads the content of general DMA Registers, DMA Channel
 *   Registers and DMA Port Registers 
 *  proc_read_dma_register - Create proc file output
 *  @buf:      Buffer to write the string to
 *  @start:    not used (Linux internal)
 *  @offset:   not used (Linux internal)
 *  @count:    not used (Linux internal)
 *  @eof:      Set to 1 when all data is stored in buffer
 *  @data:     not used (Linux internal)
 *
 *  This function creates the output for read dma registers.
 *
 *      Return Value:
 *  @len = Lenght of data in buffer
\*------------------------------------------------------------------------------------------*/
static int proc_read_dma_register(IFX_char_t *buf, IFX_char_t **start, off_t offset, int count, int *eof, void *data) {
    int i, len = 0, limit = count;
    static int blockcount = 0, channel_no = 0;

    if ((blockcount == 0) && (offset > count)) {
        *eof = 1;
        return 0;
    }
    switch (blockcount) {
        case 0:
            len += sprintf(buf + len,"\nGeneral DMA Registers\n");
            len += sprintf(buf + len,"-----------------------------------------\n");
            len += sprintf(buf + len, "CLC=        %08x\n", IFX_REG_R32(IFX_DMA_CLC_REG) );
            len += sprintf(buf + len, "ID=         %08x\n", IFX_REG_R32(IFX_DMA_ID_REG) );
            len += sprintf(buf + len, "DMA_CPOLL=  %08x\n", IFX_REG_R32(IFX_DMA_CPOLL_REG) );
            len += sprintf(buf + len, "DMA_CS=     %08x\n", IFX_REG_R32(IFX_DMA_CS_REG) );
            len += sprintf(buf + len, "DMA_PS=     %08x\n", IFX_REG_R32(IFX_DMA_PS_REG) );
            len += sprintf(buf + len, "DMA_IRNEN=  %08x\n", IFX_REG_R32(IFX_DMA_IRNEN_REG) );
            len += sprintf(buf + len, "DMA_IRNCR=  %08x\n", IFX_REG_R32(IFX_DMA_IRNCR_REG) );
            len += sprintf(buf + len, "DMA_IRNICR= %08x\n", IFX_REG_R32(IFX_DMA_IRNICR_REG) );
            len += sprintf(buf + len, "g_dma_int_status= %08x\n", g_dma_int_status );
            len += sprintf(buf + len, "g_dma_poll_int_status= %08x\n", g_dma_poll_int_status );
            len += sprintf(buf + len, "g_dma_fix_counter= %08x\n", g_dma_fix_counter );
            len += sprintf(buf + len, "g_dma_parachute_counter= %08x\n", g_dma_parachute_counter );
            len += sprintf(buf + len, "global_channel_complete_irq_counter= %08x\n", global_channel_complete_irq_counter);
            len += sprintf(buf + len, "global_channel_full_irq_counter= %08x\n", global_channel_full_irq_counter);
            len += sprintf(buf + len,"\nDMA Channel Registers\n");
            blockcount = 1;
            return len;
        case 1:
            /* If we had an overflow start at beginnig of buffer otherwise use offset */
            if (channel_no != 0) {
                *start = buf;
            } else {
                buf = buf + offset;
                *start = buf;
            }
            for(i = channel_no; i < MAX_DMA_CHANNEL_NUM; i++) {
            	char *dirstr = NULL;
            	u32 cctrl;
                _dma_channel_info* pCh = &dma_chan[i];
                if (len + 500 > limit) {
                    channel_no = i;
                    blockcount = 1;
                    return len;
                }
                len += sprintf(buf + len, "-----------------------------------------\n");
                if ( pCh->dir == IFX_DMA_RX_CH)
                	dirstr = "Rx";
                else if (pCh->dir == IFX_DMA_TX_CH )
                	dirstr = "Tx";
                else
                	dirstr = "Xx";
                len += sprintf(buf + len, "Channel %d - Device %s %s - (RelChan %d)\n",
                		i, ((_dma_device_info *)(pCh->dma_dev))->device_name, dirstr ,pCh->rel_chan_no);

                IFX_REG_W32(i, IFX_DMA_CS_REG);
				cctrl = IFX_REG_R32(IFX_DMA_CCTRL_REG);
                len += sprintf(buf + len, "Kernel-Control = %s\n",(pCh->control == IFX_DMA_CH_ON)?"en":"dis");
                len += sprintf(buf + len, "DMA_CCTRL=  %08x: %s%s%s%s \n", cctrl,
                		(cctrl & DMA_CCTRL_ON)?"en":"dis",
                		(cctrl & DMA_CCTRL_P2PCPY) ? ",p2p":"",
                		(cctrl & DMA_CCTRL_LBEN) ? ",loopback":"",
                		(cctrl & DMA_CCTRL_DIR) ? ",tx":",rx" );
                if (cctrl & DMA_CCTRL_LBEN)
                	len += sprintf(buf + len, "     Loopback_channel=  %d\n", DMA_CCTRL_LBCHNR_GET(cctrl));
                len += sprintf(buf + len, "DMA_CDBA=   %08x\n", IFX_REG_R32(IFX_DMA_CDBA_REG));
                len += sprintf(buf + len, "DMA_CIE=    %08x\n", IFX_REG_R32(IFX_DMA_CIE_REG));
                len += sprintf(buf + len, "DMA_CIS=    %08x\n", IFX_REG_R32(IFX_DMA_CIS_REG));
                len += sprintf(buf + len, "DMA_CDLEN=  %08x\n", IFX_REG_R32(IFX_DMA_CDLEN_REG));
#ifndef CONFIG_AR9
                len += sprintf(buf + len, "DMA_CDPNRD= %08x\n", IFX_REG_R32(IFX_DMA_CDPTNRD_REG));
#endif
                len += sprintf(buf + len, "IRQ_FULL_CNT=    %08x\n", pCh->channel_full_irq_counter);
                len += sprintf(buf + len, "IRQ_CMPT_CNT=    %08x\n", pCh->channel_complete_irq_counter);

            }
            blockcount = 2;
            channel_no = 0;
            return len;

        case 2:
            *start = buf;
            /* display port dependent registers */
            len += sprintf(buf + len, "\nDMA Port Registers\n");
            len += sprintf(buf + len, "-----------------------------------------\n");
            for(i = 0; i < MAX_DMA_DEVICE_NUM; i++) {
                IFX_REG_W32(i, IFX_DMA_PS_REG);
                len += sprintf( buf + len, "Port %d DMA_PCTRL= %08x\n", i, IFX_REG_R32(IFX_DMA_PCTRL_REG) );

            }
            blockcount = 0;
            *eof = 1;
            return len;
    }
    blockcount = 0;
    *eof = 1;
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int dma_open(struct inode * inode, struct file * file) {
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int dma_release(struct inode *inode, struct file *file) {
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int dma_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) {
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct file_operations dma_fops = {
    .owner       = THIS_MODULE,
    .open        = dma_open,
    .release     = dma_release,
    .ioctl       = dma_ioctl,
};

/*------------------------------------------------------------------------------------------*\
 *  Set the  default values in dma device/channel structure
\*------------------------------------------------------------------------------------------*/
static int map_dma_chan(_dma_chan_map* map) {
    int i,j, result;
    unsigned int reg_val;

    for (i = 0; i < MAX_DMA_DEVICE_NUM; i++) {
        strcpy(dma_devs[i].device_name, dma_device_name[i]);
    }
    for (i = 0; i < MAX_DMA_CHANNEL_NUM; i++) {
        dma_chan[i].irq = map[i].irq;
        sprintf( dma_chan[i].interrupt_name, "dma-core-%d", i);

        dbg_verbose("[%d]: dma_chan[%d] request_irq %d %s\n", __LINE__,i, dma_chan[i].irq, dma_chan[i].interrupt_name );

        result = request_irq(dma_chan[i].irq, dma_interrupt, IRQF_DISABLED, dma_chan[i].interrupt_name,(void*)&dma_chan[i]);
        if (result) {
            printk(KERN_ERR "%s: Request IRQ failed!!!\n", __func__);
            free_irq(dma_chan[i].irq, (void*)&dma_chan[i]);
            return -EFAULT;
        }
        disable_irq(dma_chan[i].irq);
        dbg_verbose( "disable irq: %d\n", dma_chan[i].irq );
    }
    for (i = 0; i < MAX_DMA_DEVICE_NUM; i++) {
        dma_devs[i].num_tx_chan     			= 0;
        dma_devs[i].num_rx_chan     			= 0;
        dma_devs[i].max_rx_chan_num 			= 0;
        dma_devs[i].max_tx_chan_num 			= 0;
        dma_devs[i].mem_port_control 			= 0;
		dma_devs[i].port_tx_weight 				= IFX_DMA_TX_PORT_DEFAULT_WEIGHT;
		dma_devs[i].tx_endianness_mode 			= IFX_DMA_DEFAULT_TX_ENDIANNESS;
		dma_devs[i].rx_endianness_mode 			= IFX_DMA_DEFAULT_RX_ENDIANNESS;
		dma_devs[i].port_packet_drop_enable		= IFX_DMA_DEF_PORT_BASED_PKT_DROP_EN;
		dma_devs[i].port_packet_drop_counter    =  0;
        dma_devs[i].buffer_alloc  				= &common_buffer_alloc;
        dma_devs[i].buffer_free     			= &common_buffer_free;
        dma_devs[i].intr_handler    			= NULL;
        dma_devs[i].tx_burst_len    			= IFX_DMA_PORT_DEFAULT_TX_BURST_LEN ;
        dma_devs[i].rx_burst_len    			= IFX_DMA_PORT_DEFAULT_RX_BURST_LEN;
//        dma_devs[i].port_num					= i;

        for (j = 0; j < MAX_DMA_CHANNEL_NUM; j++) {
            dma_chan[j].byte_offset     		= 0;
            dma_chan[j].open            		= &open_chan;
            dma_chan[j].close           		= &close_chan;
            dma_chan[j].reset           		= &reset_chan;
            dma_chan[j].enable_irq      		= &enable_ch_irq;
            dma_chan[j].disable_irq     		= &disable_ch_irq;
            dma_chan[j].rel_chan_no     		= map[j].rel_chan_no;
            dma_chan[j].control         		= IFX_DMA_CH_OFF;
            dma_chan[j].default_weight  		= IFX_DMA_CH_DEFAULT_WEIGHT;
            dma_chan[j].weight          		= dma_chan[j].default_weight;
            dma_chan[j].tx_channel_weight 		= IFX_DMA_TX_CHAN_DEFAULT_WEIGHT;
            dma_chan[j].channel_packet_drop_enable 	= IFX_DMA_DEF_CHAN_BASED_PKT_DROP_EN;
            dma_chan[j].channel_packet_drop_counter 	= 0;
            dma_chan[j].class_value				= 0;
            dma_chan[j].peri_to_peri			= 0;
            dma_chan[j].global_buffer_len		= 0;
            dma_chan[j].loopback_enable			= 0;
            dma_chan[j].loopback_channel_number	= 0;
            dma_chan[j].curr_desc       		= 0;
            dma_chan[j].desc_handle       		= 0;
            dma_chan[j].prev_desc       		= 0;
            dma_chan[j].req_irq_to_free			= -1;
            dma_chan[j].dur                     = 0;
        }

        for (j = 0; j < MAX_DMA_CHANNEL_NUM; j++) {
            if (strcmp(dma_devs[i].device_name, map[j].dev_name) == 0){

                // get DMA port number from ifx_default_dma_map
                // will be assigned multiple times. Not elegant, but it works...
                dbg_verbose("%s -> DMA port %d\n", dma_devs[i].device_name, map[j].port_num);
                dma_devs[i].port_num = map[j].port_num;
                if(map[j].dir == IFX_DMA_RX_CH) {
                    dma_chan[j].dir = IFX_DMA_RX_CH;
                    dma_devs[i].max_rx_chan_num++;
                    dma_devs[i].rx_chan[dma_devs[i].max_rx_chan_num - 1] =  &dma_chan[j];
                    dma_devs[i].rx_chan[dma_devs[i].max_rx_chan_num - 1]->class_value = map[j].pri;
                    dma_chan[j].dma_dev = (void*)&dma_devs[i];
                    /*Program default class value */
                    IFX_REG_W32(j, IFX_DMA_CS_REG);
                    reg_val = IFX_REG_R32(IFX_DMA_CCTRL_REG);
                    reg_val &= ~DMA_CCTRL_CLASS_MASK;
                    reg_val |= DMA_CCTRL_CLASS_VAL_SET(map[j].pri);
                    IFX_REG_W32( reg_val, IFX_DMA_CCTRL_REG);
                } else if (map[j].dir == IFX_DMA_TX_CH) {
                    dma_chan[j].dir = IFX_DMA_TX_CH;
                    dma_devs[i].max_tx_chan_num++;
                    dma_devs[i].tx_chan[dma_devs[i].max_tx_chan_num - 1] = &dma_chan[j];
                    dma_devs[i].tx_chan[dma_devs[i].max_tx_chan_num-1]->class_value = map[j].pri;
                    dma_chan[j].dma_dev = (void*)&dma_devs[i];
                    /*Program default class value */
                    IFX_REG_W32(j, IFX_DMA_CS_REG);
                    reg_val = IFX_REG_R32(IFX_DMA_CCTRL_REG);
                    reg_val &= ~DMA_CCTRL_CLASS_MASK;
                    reg_val |= DMA_CCTRL_CLASS_VAL_SET(map[j].pri);
                    IFX_REG_W32( reg_val, IFX_DMA_CCTRL_REG);
                } else {
                    printk("%s wrong dma channel map!\n", __func__);
                }
            }
        }
    }

    return IFX_SUCCESS;
}

/*------------------------------------------------------------------------------------------*\
 * Enable the DMA module power, reset the dma channels and disable channel interrupts
\*------------------------------------------------------------------------------------------*/
static int dma_chip_init(void) {
    int i;
#if defined(CONFIG_DMA_PACKET_ARBITRATION_ENABLED)
    ifx_chipid_t chipid;
#endif

    DMA_PMU_SETUP(IFX_PMU_ENABLE);
    /* Reset DMA */
    IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CTRL_REG) | DMA_CTRL_RST), IFX_DMA_CTRL_REG);
    /* Disable all the interrupts first */
    IFX_REG_W32( 0, IFX_DMA_IRNEN_REG);
    IFX_REG_W32((1 << MAX_DMA_CHANNEL_NUM) - 1 , IFX_DMA_IRNCR_REG);

    for (i = 0; i < MAX_DMA_CHANNEL_NUM; i++) {
        IFX_REG_W32( i, IFX_DMA_CS_REG);
        IFX_REG_W32( (IFX_REG_R32(IFX_DMA_CCTRL_REG) | DMA_CCTRL_RST), IFX_DMA_CCTRL_REG);
        while ((IFX_REG_R32(IFX_DMA_CCTRL_REG) & DMA_CCTRL_RST)){
                ;
        }
        /* dislabe all interrupts here */
        IFX_REG_W32(0, IFX_DMA_CIE_REG);
    }
    for (i = 0; i < MAX_DMA_CHANNEL_NUM; i++) {
        disable_chan(i);
    }
#ifndef CONFIG_DMA_HW_POLL_DISABLED
    /* enable polling for all channels */
    IFX_REG_W32(DMA_CPOLL_EN | DMA_CPOLL_CNT_VAL(DMA_DEFAULT_POLL_VALUE), IFX_DMA_CPOLL_REG);
#endif  /* CONFIG_DMA_HW_POLL_DISABLED */
#if defined(CONFIG_DMA_PACKET_ARBITRATION_ENABLED)
    ifx_get_chipid(&chipid);
    if ( chipid.family_id == IFX_FAMILY_xRX200 && chipid.family_ver == IFX_FAMILY_xRX200_A1x && !(IFX_REG_R32(0xBF401010) /* DDR_CCR01 */ & 0x01) ) {
        IFX_REG_W32((IFX_REG_R32(IFX_DMA_CTRL_REG) & ~DMA_CTRL_PKTARB), IFX_DMA_CTRL_REG);
    }
    else {
        IFX_REG_W32((IFX_REG_R32(IFX_DMA_CTRL_REG) | DMA_CTRL_PKTARB), IFX_DMA_CTRL_REG);
    }
#endif
    return IFX_SUCCESS;
}

/*------------------------------------------------------------------------------------------*\
 * remove of the proc entries, used ifx_dma_exit_module
\*------------------------------------------------------------------------------------------*/
static void dma_core_proc_delete(void) {
    remove_proc_entry("ifx_dma_irq_reset", g_dma_dir);
    remove_proc_entry("ifx_dma_reset", g_dma_dir);
    remove_proc_entry("ifx_dma_chan_weight", g_dma_dir);
    remove_proc_entry("dma_list", g_dma_dir);
    remove_proc_entry("ifx_dma_register", g_dma_dir);
    remove_proc_entry("version", g_dma_dir);
    remove_proc_entry("driver/ifx_dma",  NULL);
}

/*------------------------------------------------------------------------------------------*\
 * create proc for debug  info, used ifx_dma_init_module
\*------------------------------------------------------------------------------------------*/
static void dma_core_proc_create(void) {
	struct proc_dir_entry *	res = NULL;

    g_dma_dir = proc_mkdir("driver/ifx_dma", NULL);
    create_proc_read_entry("ifx_dma_register", 0, g_dma_dir,    \
                            proc_read_dma_register, NULL);

    res = create_proc_read_entry("g_desc_list", 0, g_dma_dir,         \
                            proc_read_desc_list, NULL);
	if (res)
		res->write_proc = proc_write_desc_list;

    create_proc_read_entry("ifx_dma_chan_weight", 0, g_dma_dir, \
                            proc_read_channel_weigh, NULL);

    res = create_proc_read_entry("ifx_dma_reset", 0, g_dma_dir, \
                            proc_read_dma_reset, NULL);
	if (res)
		res->write_proc = proc_write_dma_reset;

    res = create_proc_read_entry("ifx_irq_reset", 0, g_dma_dir, \
                            proc_read_irq_reset, NULL);
    if (res)
        res->write_proc = proc_write_irq_reset;

    create_proc_read_entry("version", 0, g_dma_dir, dma_core_proc_version,  NULL);
}

/*------------------------------------------------------------------------------------------*\
 * DMA Core module Initialization.
\*------------------------------------------------------------------------------------------*/
static int __init ifx_dma_init_module(void) {
    int i, result=0;
    char ver_str[128] = {0};
    /** register the dma core driver */
    result = register_chrdev(DMA_MAJOR, "ifx_dma_core", &dma_fops);
    if (result){
        printk(KERN_ERR "%s: char device registered failed!!!\n", __func__);
        return result;
    }

    INIT_LIST_HEAD(&parachute_head);
    DMA_IRQ_SPIN_LOCK_INIT(&dummy_chan);

    /** Reset the dma channels and disable channel interrupts */
    dma_chip_init();

    /** Map the default values in dma device/channel structure */
    map_dma_chan(ifx_default_dma_map);

    /*--------------------------------------------------------------------------------------*\
     * Alloc continus memory for all descriptor
     * Invalidate the DMA cache memory for all the descriptors
     * Descriptor memory will be used uncached !!
    \*--------------------------------------------------------------------------------------*/
    g_desc_list = (u64*)kmalloc(IFX_DMA_DESCRIPTOR_OFFSET * MAX_DMA_CHANNEL_NUM * sizeof(u64), GFP_DMA);
    if (g_desc_list == NULL) {
        printk(KERN_ERR "%s: No memory for DMA descriptors list\n", __func__);
        return -ENOMEM;
    }
    /** Invalidate the DMA cache memory for all the descriptors*/
    dma_cache_inv((unsigned long)g_desc_list, \
        IFX_DMA_DESCRIPTOR_OFFSET * MAX_DMA_CHANNEL_NUM * sizeof(u64));
    /** just backup when want to cleanup the module */
    g_desc_list_backup = g_desc_list;
    /** Set uncached memory for DMA descriptors */
    g_desc_list = (u64*)((u32)g_desc_list | 0xA0000000);
    /** Clear whole descriptor memory */
    memset(g_desc_list, 0, IFX_DMA_DESCRIPTOR_OFFSET * MAX_DMA_CHANNEL_NUM * sizeof(u64));
    for (i = 0; i < MAX_DMA_CHANNEL_NUM; i++) {
            /** set the Desc base address based per channel */
        dma_chan[i].desc_base = (u32)g_desc_list + i * IFX_DMA_DESCRIPTOR_OFFSET * 8;
        dma_chan[i].curr_desc = 0;
        /** Number of descriptor per channel */
        dma_chan[i].desc_len = IFX_DMA_DESCRIPTOR_OFFSET;
        /** select the channel */
        select_chan(i);
        /** set the desc base address and number of descriptord per channel */
        IFX_REG_W32( ( (u32)CPHYSADDR(dma_chan[i].desc_base)), IFX_DMA_CDBA_REG);
        IFX_REG_W32( dma_chan[i].desc_len  , IFX_DMA_CDLEN_REG);
/*    printk("channel %d-> address : %08X \n", i,(u32)dma_chan[i].desc_base);  */
    }
    /** create proc for debug  info*/
    dma_core_proc_create();
    /* Print the driver version info */
    dma_core_drv_ver(ver_str);
    printk(KERN_INFO "%s skb_shared_size:%d", ver_str, sizeof(struct skb_shared_info));

    // AVM-Busmaster zur Taktsteuerung registrieren
    avm_dma_busmaster_handler_init();

    for(i = 0; i < MAX_DMA_CHANNEL_NUM; ++i){
        init_timer(&(dma_fix_timer[i]));
        dma_fix_timer[i].function = do_dma_fix;
        dma_fix_timer[i].data = i;
    }

#if 0 && defined(CONFIG_VR9) || defined(CONFIG_AR10)
    init_timer(&dma_parachute_timer);
    dma_parachute_timer.function = do_dma_parachute_timer;
    dma_parachute_timer.data = 42;

    mod_timer(&dma_parachute_timer, jiffies + HZ);
#endif

    return 0;
}

/*------------------------------------------------------------------------------------------*\
 *   DMA Core Module Exit.
 *   Upon exit of the DMA core module this function will free all allocated 
 *   resources and unregister devices. 
\*------------------------------------------------------------------------------------------*/
static void __exit ifx_dma_exit_module(void) {
    int i;
    /** unregister the device */
    unregister_chrdev(DMA_MAJOR, "dma-core");
    /** free the allocated channel descriptor memory */
    if (g_desc_list_backup) {
        kfree(g_desc_list_backup);
    }
    /** remove of the proc entries */
    dma_core_proc_delete();
    /* Release the resources */
    for (i = 0; i < MAX_DMA_CHANNEL_NUM; i++) {
        free_irq(dma_chan[i].irq, (void*)&dma_chan[i]);
    }

    // AVM-Busmaster zur Taktsteuerung freigeben
    avm_dma_busmaster_handler_exit();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
module_init(ifx_dma_init_module);
module_exit(ifx_dma_exit_module);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Reddy.Mallikarjun@infineon.com");
MODULE_VERSION(IFX_DRV_MODULE_VERSION);
MODULE_DESCRIPTION("IFX CPE DMA device driver ");
MODULE_SUPPORTED_DEVICE ("IFX DSL CPE Devices (Supported Danube, Amazon-SE, AR9 & VR9 )");
