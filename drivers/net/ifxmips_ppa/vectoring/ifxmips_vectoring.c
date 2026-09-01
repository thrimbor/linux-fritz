/******************************************************************************
**
** FILE NAME    : vectoring.c
** PROJECT      : UEIP
** MODULES      : MII0/1 + PTM Acceleration Package (VR9 PPA E5)
**
** DATE         : 09 OCT 2012
** AUTHOR       : Xu Liang
** DESCRIPTION  : Vectoring support for VDSL WAN.
** COPYRIGHT    :              Copyright (c) 2009
**                          Lantiq Deutschland GmbH
**                   Am Campeon 3; 85579 Neubiberg, Germany
**
**   For licensing information, see the file 'LICENSE' in the root folder of
**   this software module.
**
** HISTORY
** $Date        $Author         $Comment
** 09 OCT 2012  Xu Liang        Initiate Version
*******************************************************************************/



/*
 * ####################################
 *              Head File
 * ####################################
 */

/*
 *  Common Head File
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/etherdevice.h>

/*
 *  Chip Specific Head File
 */
#include "ifxmips_vectoring_stub.h"


/*
 * ####################################
 *              Definition
 * ####################################
 */

#define DMA_PACKET_SIZE                         1600

#define ERB_MAX_BACHCHANNEL_DATA_SIZE           (1024 - 5)
#define ERB_BACHCHANNEL_DATA_OFF_LLC            (8 + 5)
#define ERB_MAX_PAYLOAD                         3600
#define ERB_SIZE                                ((int)&((struct erb_head *)0)->backchannel_data)

#define err(format, arg...)                     do { if ( (g_dbg_enable & DBG_ENABLE_MASK_ERR) ) printk(KERN_ERR __FILE__ ":%d:%s: " format "\n", __LINE__, __FUNCTION__, ##arg); } while ( 0 )
#define dbg(format, arg...)                     do { if ( (g_dbg_enable & DBG_ENABLE_MASK_DEBUG_PRINT) ) printk(KERN_WARNING __FILE__ ":%d:%s: " format "\n", __LINE__, __FUNCTION__, ##arg); } while ( 0 )
#define ASSERT(cond, format, arg...)            do { if ( (g_dbg_enable & DBG_ENABLE_MASK_ASSERT) && !(cond) ) printk(KERN_ERR __FILE__ ":%d:%s: " format "\n", __LINE__, __FUNCTION__, ##arg); } while ( 0 )

#define DBG_ENABLE_MASK_ERR                     (1 << 0)
#define DBG_ENABLE_MASK_DEBUG_PRINT             (1 << 1)
#define DBG_ENABLE_MASK_ASSERT                  (1 << 2)
#define DBG_ENABLE_MASK_DUMP_SKB_TX             (1 << 9)
#define DBG_ENABLE_MASK_ALL                     (DBG_ENABLE_MASK_ERR | DBG_ENABLE_MASK_DEBUG_PRINT | DBG_ENABLE_MASK_ASSERT | DBG_ENABLE_MASK_DUMP_SKB_TX)

#if defined(CONFIG_IFX_VECTOR_TIMER_CHECK) && CONFIG_IFX_VECTOR_TIMER_CHECK
static unsigned int vector_timer_interval=32; /*in millseconds */
static unsigned int en_vector_tx_api=1;
static unsigned int en_vector_tx_api_test=0;
static unsigned int avm_mei_dsm_cb_func_call_cnt = 0;
static unsigned int avm_mei_dsm_cb_func_call_too_fast = 0;
static uint64_t avm_sum_delta_ms = 0;
static uint32_t avm_sum_delta_ms_count = 0;
#endif /* CONFIG_IFX_VECTOR_TIMER_CHECK*/
/*
 * ####################################
 *             Declaration
 * ####################################
 */

static int32_t mei_dsm_cb_func(uint32_t *p_error_vector);

static void register_netdev_event_handler(void);
static void unregister_netdev_event_handler(void);
static int netdev_event_handler(struct notifier_block *nb, unsigned long event, void *netdev);

static int proc_read_dbg(char *page, char **start, off_t off, int count, int *eof, void *data);
static int proc_write_dbg(struct file *file, const char *buf, unsigned long count, void *data);
static void dump_data(void *data, unsigned int len, char *title);
static void dump_skb(struct sk_buff *skb, u32 len, char *title, int port, int ch, int is_tx, int enforce);



/*
 * ####################################
 *            Data Structure
 * ####################################
 */

struct erb_head {
    unsigned char   vce_mac[6];
    unsigned char   vtu_r_mac[6];
    unsigned short  length;
    unsigned char   llc_header[3];
    unsigned char   itu_t_oui[3];
    unsigned short  protocol_id;
    unsigned short  line_id;
    unsigned short  sync_symbol_count;
    unsigned char   segment_code;
    unsigned char   backchannel_data[0];    //  place holder of backchannel data
};



/*
 * ####################################
 *           Global Variable
 * ####################################
 */

static struct notifier_block g_netdev_event_handler_nb = {0};
static struct net_device *g_ptm_net_dev = NULL;

static int g_dbg_enable = 0;



/*
 * ####################################
 *            Local Function
 * ####################################
 */

static int mei_dsm_cb_func(unsigned int *p_error_vector)
{
    int rc, ret;
    struct sk_buff *skb_list = NULL;
    struct sk_buff *skb;
    struct erb_head *erb;
    unsigned int total_size, sent_size, block_size;
    unsigned int num_blocks;
    unsigned int segment_code;
    unsigned int i;

#if defined(CONFIG_IFX_VECTOR_TIMER_CHECK) && CONFIG_IFX_VECTOR_TIMER_CHECK
    static int f_first_call=1;    
    static unsigned long last_jiffies=0;

    avm_mei_dsm_cb_func_call_cnt++;

    if( f_first_call )
    {
        last_jiffies = jiffies;
        f_first_call = 0;
    }
    else 
    {
        unsigned long curr_jiffies = jiffies;
        if( last_jiffies < curr_jiffies ) 
        {
            unsigned int delta_ms = jiffies_to_msecs(curr_jiffies - last_jiffies);
            avm_sum_delta_ms += delta_ms;
            avm_sum_delta_ms_count ++;
            if( delta_ms < vector_timer_interval ) /*Linux jiffies will overflow after 5 minutes*/
            {
                printk(KERN_ERR "Warning for vector timer %u ms less than expected %u ms. Current/last jiffies=%lu/%lu\n", delta_ms, vector_timer_interval, curr_jiffies, last_jiffies);
                avm_mei_dsm_cb_func_call_too_fast++;
            }
            
        }
        //update last_jiffies
        last_jiffies = curr_jiffies;
    }
    if( !en_vector_tx_api || en_vector_tx_api_test )  //tx API is tempariary disabled or just in test mode 
    {
        dbg("en_vector_tx_api is %s\n", en_vector_tx_api?"Enabled":"Disabled" );
        return -EINVAL;
    }
#endif    /*CONFIG_IFX_VECTOR_TIMER_CHECK */
    if ( (g_dbg_enable & DBG_ENABLE_MASK_DEBUG_PRINT) )
    {
        dump_data(p_error_vector, 128, "vectoring raw data");
    }

    if ( g_ptm_net_dev == NULL )
    {
        err("g_ptm_net_dev == NULL");
        return -ENODEV;
    }

    erb = (struct erb_head *)(p_error_vector + 1);
    total_size = erb->length;
    if ( total_size < ERB_BACHCHANNEL_DATA_OFF_LLC || total_size > ERB_MAX_PAYLOAD )
    {
        err("p_error_vector[0] = %u, erb->length = %u", p_error_vector[0], total_size);
        return -EINVAL;
    }

    total_size -= ERB_BACHCHANNEL_DATA_OFF_LLC;
    num_blocks = (total_size + ERB_MAX_BACHCHANNEL_DATA_SIZE - 1) / ERB_MAX_BACHCHANNEL_DATA_SIZE;

    for ( i = 0; i < num_blocks; i++ ) {
        skb = dev_alloc_skb(DMA_PACKET_SIZE);
        if ( !skb ) {
            while ( (skb = skb_list) != NULL ) {
                skb_list = skb_list->next;
                skb->next = NULL;
                dev_kfree_skb_any(skb);
            }
            err("dev_alloc_skb fail");
            return -ENOMEM;
        }
        else {
            skb->next = skb_list;
            skb_list = skb;
        }
    }

    rc = 0;
    sent_size = 0;
    segment_code = 0;
    while ( (skb = skb_list) != NULL ) {
        skb_list = skb_list->next;
        skb->next = NULL;

        block_size = min(total_size, sent_size + ERB_MAX_BACHCHANNEL_DATA_SIZE) - sent_size;
        skb_put(skb, block_size + ERB_SIZE);
        memcpy(skb->data, erb, ERB_SIZE);
        memcpy(((struct erb_head *)skb->data)->backchannel_data, &erb->backchannel_data[sent_size], block_size);
        sent_size += block_size;

        ((struct erb_head *)skb->data)->length = (unsigned short)(block_size + ERB_BACHCHANNEL_DATA_OFF_LLC);
        if ( skb_list == NULL )
            segment_code |= 0xC0;
        ((struct erb_head *)skb->data)->segment_code = segment_code;

        skb->cb[13] = 0x5A; /* magic number indicating forcing QId */
        skb->cb[15] = 0x00; /* highest priority queue */
        skb->dev = g_ptm_net_dev;

        dump_skb(skb, ~0, "vectoring TX", 0, 0, 1, 0);

        ret = g_ptm_net_dev->netdev_ops->ndo_start_xmit(skb, g_ptm_net_dev);
        if ( rc == 0 )
            rc = ret;

        segment_code++;
    }

    *p_error_vector = 0;    /* notify DSL firmware that ERB is sent */

    ASSERT(rc == 0, "dev_queue_xmit fail - %d", rc);
    return rc;
}

static void register_netdev_event_handler(void)
{
    g_netdev_event_handler_nb.notifier_call = netdev_event_handler;
    register_netdevice_notifier(&g_netdev_event_handler_nb);
}

static void unregister_netdev_event_handler(void)
{
    unregister_netdevice_notifier(&g_netdev_event_handler_nb);
}

static int netdev_event_handler(struct notifier_block *nb, unsigned long event, void *netdev)
{
    struct net_device *netif;

    if ( event != NETDEV_REGISTER
        && event != NETDEV_UNREGISTER )
        return NOTIFY_DONE;

    netif = (struct net_device *)netdev;
    //AVMbk our interface is called ptm_vr9
    if ( strcmp(netif->name, "ptm_vr9") != 0 )
        return NOTIFY_DONE;

    g_ptm_net_dev = event == NETDEV_REGISTER ? netif : NULL;
    return NOTIFY_OK;
}

static void ignore_space(char **p, int *len)
{
    while ( *len > 0 && (**p <= ' ' || **p == ':' || **p == '.' || **p == ',') )
    {
        (*p)++;
        (*len)--;
    }
}

static unsigned int get_number(char **p, int *len, int is_hex)
{
    unsigned int ret = 0;
    int n = 0;
    
    if ( (*p)[0] == '0' && (*p)[1] == 'x' )
    {
        is_hex = 1;
        (*p) += 2;
        (*len) -= 2;
    }

    if ( is_hex )
    {
        while ( *len && ((**p >= '0' && **p <= '9') || (**p >= 'a' && **p <= 'f') || (**p >= 'A' && **p <= 'F')) )
        {
            if ( **p >= '0' && **p <= '9' )
                n = **p - '0';
            else if ( **p >= 'a' && **p <= 'f' )
               n = **p - 'a' + 10;
            else if ( **p >= 'A' && **p <= 'F' )
                n = **p - 'A' + 10;
            ret = (ret << 4) | n;
            (*p)++;
            (*len)--;
        }
    }
    else
    {
        while ( *len && **p >= '0' && **p <= '9' )
        {
            n = **p - '0';
            ret = ret * 10 + n;
            (*p)++;
            (*len)--;
        }
    }

    return ret;
}
static int proc_read_dbg(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
#if defined(CONFIG_IFX_VECTOR_TIMER_CHECK) && CONFIG_IFX_VECTOR_TIMER_CHECK  
    uint32_t avm_mean_time = avm_sum_delta_ms_count ? do_div(avm_sum_delta_ms, avm_sum_delta_ms_count) : 0;
#endif /*CONFIG_IFX_VECTOR_TIMER_CHECK */        

    len += sprintf(page + len, "error print      - %s\n", (g_dbg_enable & DBG_ENABLE_MASK_ERR)              ? "enabled" : "disabled");
    len += sprintf(page + len, "debug print      - %s\n", (g_dbg_enable & DBG_ENABLE_MASK_DEBUG_PRINT)      ? "enabled" : "disabled");
    len += sprintf(page + len, "assert           - %s\n", (g_dbg_enable & DBG_ENABLE_MASK_ASSERT)           ? "enabled" : "disabled");
    len += sprintf(page + len, "dump tx skb      - %s\n", (g_dbg_enable & DBG_ENABLE_MASK_DUMP_SKB_TX)      ? "enabled" : "disabled");

#if defined(CONFIG_IFX_VECTOR_TIMER_CHECK) && CONFIG_IFX_VECTOR_TIMER_CHECK  
    len += sprintf(page + len, "\n");
    len += sprintf(page + len, "Current expected vector timer(ms): %u\n", vector_timer_interval);
    len += sprintf(page + len, "Vector tx API is %s\n", en_vector_tx_api?"Enabled":"Disabled");
    len += sprintf(page + len, "Vector tx API is in %s\n", en_vector_tx_api_test?"Test mode":"Working mode");
    len += sprintf(page + len, "AVM mei_dsl_cb_func call cnt %u\n", avm_mei_dsm_cb_func_call_cnt);
    len += sprintf(page + len, "AVM mei_dsl_cb_func call too fast %u\n", avm_mei_dsm_cb_func_call_too_fast);
    len += sprintf(page + len, "AVM mei_dsl_cb_func mean time between func calls=%u \n", avm_mean_time);
#endif /*CONFIG_IFX_VECTOR_TIMER_CHECK */        
    *eof = 1;
    return len;
}

static int proc_write_dbg(struct file *file, const char *buf, unsigned long count, void *data)
{
    static const char *dbg_enable_mask_str[] = {
        " err",
        " error print",
        " dbg",
        " debug print",
        " assert",
        " assert",
        " tx",
        " dump tx skb",
        " all"
    };
    static const int dbg_enable_mask_str_len[] = {
        4, 12,
        4, 12,
        7,  7,
        3, 12,
        4
    };
    u32 dbg_enable_mask[] = {
        DBG_ENABLE_MASK_ERR,
        DBG_ENABLE_MASK_DEBUG_PRINT,
        DBG_ENABLE_MASK_ASSERT,
        DBG_ENABLE_MASK_DUMP_SKB_TX,
        DBG_ENABLE_MASK_ALL
    };

    char str[2048];
    char *p;

    int len, rlen;

    int f_enable = 0;
    int i;

    len = count < sizeof(str) ? count : sizeof(str) - 1;
    rlen = len - copy_from_user(str, buf, len);
    while ( rlen && str[rlen - 1] <= ' ' )
        rlen--;
    str[rlen] = 0;
    for ( p = str; *p && *p <= ' '; p++, rlen-- );
    if ( !*p )
        return 0;

    if ( strncasecmp(p, "enable", 6) == 0 )
    {
        p += 6;
        f_enable = 1;
    }
    else if ( strncasecmp(p, "disable", 7) == 0 )
    {
        p += 7;
        f_enable = -1;
    }
    else if ( strncasecmp(p, "help", 4) == 0 || *p == '?' )
    {
        printk("echo <enable/disable> [");
        for ( i = 0; i < ARRAY_SIZE(dbg_enable_mask_str); i += 2 )
        {
            if ( i != 0 )
                printk("/%s", dbg_enable_mask_str[i] + 1);
            else
                printk(dbg_enable_mask_str[i] + 1);
        }
        printk("] > /proc/vectoring\n");
#if defined(CONFIG_IFX_VECTOR_TIMER_CHECK) && CONFIG_IFX_VECTOR_TIMER_CHECK    
        printk("echo set_timer value: to adjust vector timer\n");
        printk("echo set_api 0 / 1: to enable/disablet vector tx API\n");
        printk("echo test_api sleep_in_ms: to test vector tx API after delay x ms\n");
#endif /*CONFIG_IFX_VECTOR_TIMER_CHECK */        
    }
#if defined(CONFIG_IFX_VECTOR_TIMER_CHECK) && CONFIG_IFX_VECTOR_TIMER_CHECK    
    else if ( strncasecmp(p, "set_timer", 9) == 0 )
    {        
        p += 9;
        rlen -= 9;
        ignore_space( &p, &rlen);
        if ( rlen > 0 )
        {
            printk("last timer=%u\n", vector_timer_interval);
            vector_timer_interval = get_number(&p, &rlen, 0);
            printk("new  timer=%u\n", vector_timer_interval);
        }
        return count;
    }
    else if ( strncasecmp(p, "set_api", 7) == 0 )
    {        
        p += 7;
        rlen -= 7;
        ignore_space( &p, &rlen);
        if ( rlen > 0 )
        {
            en_vector_tx_api = get_number(&p, &rlen, 0);
            printk("Vectoring Tx API is %s\n", en_vector_tx_api?"Enabled":"Disabled");
        }
        return count;
    }
    else if ( strncasecmp(p, "test_api", 8) == 0 )
    {   
        unsigned int delay_ms=0;
        p += 8;
        rlen -= 8;        
        ignore_space( &p, &rlen);
        if ( rlen > 0 )
        {
            delay_ms = get_number(&p, &rlen, 0);
            en_vector_tx_api_test=1;
            mei_dsm_cb_func(NULL);
            msleep(delay_ms);
            mei_dsm_cb_func(NULL);
            en_vector_tx_api_test=0;
            printk("Vector Tx API has been called two times with msleep %u ms\n", delay_ms );
        }

        return count;
    }
#endif /*CONFIG_IFX_VECTOR_TIMER_CHECK */
    if ( f_enable )
    {
        if ( *p == 0 )
        {
            if ( f_enable > 0 )
                g_dbg_enable |= DBG_ENABLE_MASK_ALL;
            else
                g_dbg_enable &= ~DBG_ENABLE_MASK_ALL;
        }
        else
        {
            do
            {
                for ( i = 0; i < ARRAY_SIZE(dbg_enable_mask_str); i++ )
                    if ( strncasecmp(p, dbg_enable_mask_str[i], dbg_enable_mask_str_len[i]) == 0 )
                    {
                        if ( f_enable > 0 )
                            g_dbg_enable |= dbg_enable_mask[i >> 1];
                        else
                            g_dbg_enable &= ~dbg_enable_mask[i >> 1];
                        p += dbg_enable_mask_str_len[i];
                        break;
                    }
            } while ( i < ARRAY_SIZE(dbg_enable_mask_str) );
        }
    }

    return count;
}

static void dump_data(void *data, unsigned int len, char *title)
{
    int i;

    if ( title )
        printk("%s\n", title);
    for ( i = 1; i <= len; i++ )
    {
        if ( i % 16 == 1 )
            printk("  %4d:", i - 1);
        printk(" %02X", (int)(*((char*)data + i - 1) & 0xFF));
        if ( i % 16 == 0 )
            printk("\n");
    }
    if ( (i - 1) % 16 != 0 )
        printk("\n");
}

static void dump_skb(struct sk_buff *skb, u32 len, char *title, int port, int ch, int is_tx, int enforce)
{
    if ( !enforce && !(g_dbg_enable & (is_tx ? DBG_ENABLE_MASK_DUMP_SKB_TX : 0)) )
        return;

    if ( skb->len < len )
        len = skb->len;

    if ( len > DMA_PACKET_SIZE )
    {
        printk("too big data length: skb = %08x, skb->data = %08x, skb->len = %d\n", (u32)skb, (u32)skb->data, skb->len);
        return;
    }

    if ( ch >= 0 )
        printk("%s (port %d, ch %d)\n", title, port, ch);
    else
        printk("%s\n", title);
    printk("  skb->data = %08X, skb->tail = %08X, skb->len = %d\n", (u32)skb->data, (u32)skb->tail, (int)skb->len);
    dump_data(skb->data, len, NULL);
}



/*
 * ####################################
 *           Init/Cleanup API
 * ####################################
 */

static int __init vectoring_init(void)
{
    struct proc_dir_entry *res;

    res = create_proc_entry("vectoring",
                            0,
                            NULL);
    if ( res )
    {
        res->read_proc  = proc_read_dbg;
        res->write_proc = proc_write_dbg;
    }
    printk("res = %p\n", res);

    register_netdev_event_handler();
    //AVMbk our interface is called ptm_vr9
    g_ptm_net_dev = dev_get_by_name(&init_net, "ptm_vr9");
    if ( g_ptm_net_dev != NULL )
        dev_put(g_ptm_net_dev);

    mei_dsm_cb_func_hook = mei_dsm_cb_func;

    return 0;
}

static void __exit vectoring_exit(void)
{
    mei_dsm_cb_func_hook = NULL;

    unregister_netdev_event_handler();

    remove_proc_entry("vectoring", NULL);
}

module_init(vectoring_init);
module_exit(vectoring_exit);

MODULE_LICENSE("GPL");
