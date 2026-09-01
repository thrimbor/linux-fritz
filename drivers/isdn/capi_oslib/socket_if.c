#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/list.h>
#include <net/tcp.h>
#include <net/ip.h>
#include <linux/inet.h>
#include <net/protocol.h>
#include <net/sock.h>
#include "consts.h"
#include <linux/new_capi.h>
#include "debug.h"
#include "appl.h"
#include "capi_pipe.h"
#include "local_capi.h"
#include "zugriff.h"
#include <linux/poll.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/init.h>
#include <linux/zugriff.h>
#include <linux/capi_oslib.h>


struct _capi_connections {
    struct list_head list;
    unsigned ApplId;
    u8 conindex;
    u16 send_seqnr;
    u16 recv_seqnr;
    struct work_struct rx_work;
    struct work_struct tx_work;
    struct work_struct remove;
    struct sk_buff_head recvqueue;  /* pakets without headers */
    u8 *recombine_buffer;           /* Buffer for recombining messages */
    unsigned recombine_len;
    unsigned B3BlockSize;
};

static struct workqueue_struct *capi_remote_put_workqueue;

struct rcapihdr {
   u8 type;                     /* RCAPI_TYPE_APPL */
   u8 conindex;                 /* application identifier */
   u16 seqnr;                   /* Sequence Number */
   u16 len;                     /* Payload length */
};

#define RCAPI_RESERVE   16      /* reserve for cpmac */
#define RCAPI_HEADROOM	(sizeof(struct ethhdr)+sizeof(struct rcapihdr))
#define ETH_P_RCAPI		0x8888

#define RCAPI_TYPE_RESERVED	    0
#define RCAPI_TYPE_APPL		    1       /* vollst. Message */
#define RCAPI_TYPE_APPL_CONT    2       /* unvollst. Message */
#define RCAPI_TYPE_APPL_END     3       /* Abschluss einer Message */
#define RCAPI_TYPE_PING         4
#define RCAPI_SEND_BUFSIZ       (128+2048)
#define RCAPI_MAX_FRAME_SIZE    1500

LIST_HEAD(capi_conn_list);
static struct net_device *capi_netdev;   /* interface "cpmac0", if it is there */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
#else
#define get_fast_time(x) do_gettimeofday(x)
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
static void capi_oslib_socket_get(void* arg);
#else/*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/
static void capi_oslib_socket_get(struct work_struct *work);
#endif/*--- #else ---*//*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static inline struct net_device *capi_device(void)
{
   if (capi_netdev)
      return capi_netdev;
   capi_netdev = dev_get_by_name(
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 24)
           &init_net,
#endif
           "cpmac0");
   if (capi_netdev)
      printk(KERN_NOTICE "capi_oslib: device %s now there.\n", capi_netdev->name);
   return capi_netdev;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void capi_oslib_remove_conn
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
    (void* arg) {
    struct _capi_connections *conn = (struct _capi_connections*)arg;
#else
    (struct work_struct *work) {
	struct _capi_connections *conn = container_of(work, struct _capi_connections, remove);
#endif/*--- #else ---*/

    /*--- printk(KERN_DEBUG "capi_oslib_remove_conn for conn %d\n", conn->conindex); ---*/
    skb_queue_purge(&conn->recvqueue);
    list_del(&conn->list);
    if (conn->ApplId != (unsigned)-1) {
        LOCAL_CAPI_SET_NOTIFY(SOURCE_SOCKET_CAPI, conn->ApplId, NULL);
        LOCAL_CAPI_RELEASE(SOURCE_SOCKET_CAPI, conn->ApplId);
        conn->ApplId = (unsigned)-1;
    }
    if (conn->recombine_buffer) {
        kfree(conn->recombine_buffer);
    }
    kfree(conn);
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static struct sk_buff* capi_oslib_allocskb(unsigned size, unsigned char type, unsigned char connindex, unsigned int seqnr, struct net_device *dev, unsigned char** dataptr, int priority)
{
    struct sk_buff* skb;
    struct ethhdr *ethh;
    struct rcapihdr *rhdr;
    u8 *data;

    skb = alloc_skb(RCAPI_RESERVE + RCAPI_HEADROOM + size, priority);
    if (unlikely(!skb)) return NULL;

    skb_reserve(skb, RCAPI_RESERVE);                        /* reserve headroom for cpmac */
    (void)skb_put(skb, RCAPI_HEADROOM + size);              /* set length */
    ethh = (struct ethhdr *)skb->data;
    memset(ethh->h_dest, 0xff, ETH_ALEN);
    memcpy(ethh->h_source, dev->dev_addr, ETH_ALEN);
    ethh->h_proto = __constant_htons(ETH_P_RCAPI);
    rhdr = (struct rcapihdr *) (ethh + 1);
    rhdr->type = type;
    rhdr->conindex = connindex;
    rhdr->seqnr = seqnr;
    rhdr->len = size;
    data = (u8 *) (rhdr + 1);
    skb->dev = dev;
    skb->protocol = ethh->h_proto;
    skb->pkt_type = PACKET_OUTGOING;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
    skb->nh.raw = skb->data;
#else/*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/
    skb_reset_network_header(skb);
#endif/*--- #else ---*//*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/

    if (dataptr) {
        *dataptr = data;
    }

    return skb;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void capi_oslib_send_too_big_conf(unsigned char* msg, struct _capi_connections *conn)
{
    struct sk_buff *skb;
    int rc;
    struct net_device *dev;
    unsigned char* data;

    dev = capi_device();
    if (unlikely(!dev)) {
        printk(KERN_WARNING "capi_oslib: send data conf but no dev!\n");
        return;
    }
    
    skb = capi_oslib_allocskb(17, RCAPI_TYPE_APPL, conn->conindex, conn->send_seqnr, dev, &data, GFP_KERNEL);
    if (unlikely(!skb)) {
        printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
        return;
    } else {
        *data++ = 1;    /* CAPI Message */
        memcpy(data, msg, 16);
        *(unsigned short*)&data[0] = 16;
        data[5] = 0x81;
        *(unsigned short*)&data[12] = *(unsigned short*)&data[18];
        *(unsigned short*)&data[14] = 0x300c;       /* Data Length not supported by current protocol */

        rc = dev_queue_xmit(skb); /* queue paket for transmitting */

        if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
            printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
            return;
        }
        conn->send_seqnr++;
    }
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void capi_oslib_socket_put
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
    (void* arg) {
    struct _capi_connections *conn = (struct _capi_connections*)arg;
#else
    (struct work_struct *work) {
	struct _capi_connections *conn = container_of(work, struct _capi_connections, tx_work);
#endif
    unsigned int status = ERR_SendBusy;
    struct sk_buff *skb;
    unsigned short ApplId;
    struct rcapihdr *rhdr;
    
    while ((skb = skb_dequeue(&conn->recvqueue))) {
        /*--- printk(KERN_DEBUG "capi_oslib_socket_put: skb %p\n", skb); ---*/
        rhdr = (struct rcapihdr *) skb->data;
        if ((rhdr->type == RCAPI_TYPE_APPL) || (rhdr->type == RCAPI_TYPE_APPL_END)) {
            unsigned msg_len = 0;
            struct __attribute__ ((packed)) _capi_message *C;
            unsigned char* buffer;
    
            if (rhdr->type == RCAPI_TYPE_APPL_END) {
                /*--- printk(KERN_DEBUG "capi_oslib_socket_put: got RCAPI_TYPE_APPL_END\n"); ---*/
                skb_pull(skb, sizeof(struct rcapihdr)); /* strip headers */
                /*--- printk(KERN_DEBUG "capi_oslib: appending to recombine buffer\n"); ---*/
                if (conn->recombine_len + rhdr->len < RCAPI_SEND_BUFSIZ) {
                    memcpy(&conn->recombine_buffer[conn->recombine_len], skb->data, rhdr->len);
                    conn->recombine_len = 0;
                    buffer = conn->recombine_buffer;
                } else {
                    __printk(KERN_ERR "capi_oslib: recombine_buffer too small!\n");
                    conn->recombine_len = 0;
                    break;
                }
            } else {
                /*--- printk(KERN_DEBUG "capi_oslib_socket_put: got RCAPI_TYPE_APPL\n"); ---*/
                skb_pull(skb, sizeof(struct rcapihdr) + 1); /* strip headers */
                buffer = (unsigned char*)skb->data;
            }
            C = (struct __attribute__ ((packed)) _capi_message *)buffer;
            msg_len = copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.Length);
  
            switch (buffer[4])
            {
                case 0x86:    /* DATA_B3_REQ */
                    if (buffer[5] == 0x80) {
                        struct __attribute__ ((packed)) _capi_message *C = (struct __attribute__ ((packed)) _capi_message *)buffer;
                        unsigned char* data_buffer = NULL;
      
                        if (conn->B3BlockSize >= copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen)) {
                            data_buffer = LOCAL_CAPI_NEW_DATA_B3_REQ_BUFFER(SOURCE_SOCKET_CAPI, 
                                    copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.ApplId), copy_dword_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.NCCI));
                            if (data_buffer) {
                                memcpy(data_buffer, &buffer[msg_len], copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen));
                                C->capi_message_part.data_b3_req.Data = data_buffer;
#if defined(CAPIOSLIB_CHECK_LATENCY)
                                /*--- capi_generate_timestamp(0x20 + (conn->ApplId & 0x1F), data_buffer, copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen)); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
                            }
                        } else {
                            printk(KERN_ERR "capi_oslib: received B3Msg of len=%d too big, dropping.\n", copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_req.DataLen));
                            capi_oslib_send_too_big_conf(buffer, conn);
                            goto next_message;
                        }
                    }
                    /* TODO Länge im skb ggf. setzen */
                    break;
                case 0xFD:
                    {
                        unsigned maxNCCIs;
                        unsigned windowsize;
                        unsigned b3_blocksize;
                        unsigned result;
                        struct sk_buff *response_skb;
                        u8 *data;
                        int rc;
                        struct net_device *dev;

                        /*--- printk(KERN_DEBUG "capi_oslib: got register\n"); ---*/
                        dev = capi_device();
                        if (unlikely(!dev)) {
                            printk(KERN_WARNING "capi_oslib: send register response but no dev!\n");
                            goto next_message;
                        }
      
                        if (conn->ApplId != (unsigned)-1) {
                            printk(KERN_WARNING "capi_oslib: register but conn %d already has ApplId %d\n", conn->conindex, conn->ApplId);
                            LOCAL_CAPI_SET_NOTIFY(SOURCE_SOCKET_CAPI, conn->ApplId, NULL);
                            LOCAL_CAPI_RELEASE(SOURCE_SOCKET_CAPI, conn->ApplId);
                            conn->ApplId = (unsigned)-1;
                            skb_queue_purge(&conn->recvqueue);
                        }
                        maxNCCIs = buffer[8] | (buffer[9] << 8) | (buffer[10] << 16) | (buffer[11] << 24);
                        windowsize = buffer[12] | (buffer[13] << 8) | (buffer[14] << 16) | (buffer[15] << 24);
                        b3_blocksize = buffer[16] | (buffer[17] << 8) | (buffer[18] << 16) | (buffer[19] << 24);
                        conn->B3BlockSize = b3_blocksize;
                        result = LOCAL_CAPI_REGISTER(SOURCE_SOCKET_CAPI, 1024 + (1024 * maxNCCIs), maxNCCIs, windowsize, b3_blocksize, &conn->ApplId);
                        /*--- printk(KERN_DEBUG "register result: %d\n", result); ---*/
                        if (result == ERR_NoError) {
                            unsigned char response[10];
                            /*--- list_add(&conn->list, &capi_conn_list); ---*/
                            
                            /* Antwort mit ApplID schicken */
                            memcpy((void*)response, buffer, 10);
                            response[2] = (conn->ApplId & 0xFF);         /* ApplID einsetzen */
                            response[3] = (conn->ApplId >> 8) & 0xFF;
                            response[8] = 0;
                            response[9] = 0;

                            response_skb = capi_oslib_allocskb(11, RCAPI_TYPE_APPL, conn->conindex, conn->send_seqnr, dev, &data, GFP_KERNEL);
                            if (unlikely(!response_skb)) {
                                printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
                                goto next_message;
                            } else {
                                *data++ = 1;    /* CAPI Message */
                                memcpy(data, response, 11);

                                rc = dev_queue_xmit(response_skb); /* queue paket for transmitting */

                                if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
                                    printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
                                    goto next_message;
                                }
                                conn->send_seqnr++;
                            }
      
                            /*--- printk(KERN_DEBUG "register success result sent\n"); ---*/
                            /* workqueue item erzeugen und einhängen */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
                            INIT_WORK(&conn->rx_work, capi_oslib_socket_get, (void*)conn);
#else
                            INIT_WORK(&conn->rx_work, capi_oslib_socket_get);
#endif
                            result = LOCAL_CAPI_SET_NOTIFY(SOURCE_SOCKET_CAPI, conn->ApplId, &conn->rx_work);
                        } else {
                            unsigned char response[10];
      
                            conn->ApplId = (unsigned)-1;
                            /* Fehlerwert zurückschicken */
                            memcpy((void*)response, buffer, 10);
                            response[8] = (result & 0xFF);
                            response[9] = (result >> 8) & 0xFF;
      
                            response_skb = capi_oslib_allocskb(11, RCAPI_TYPE_APPL, conn->conindex, conn->send_seqnr, dev, &data, GFP_KERNEL);
                            if (unlikely(!response_skb)) {
                                printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
                                goto next_message;
                            } else {
                                *data++ = 1;    /* CAPI Message */
                                memcpy(data, response, 11);

                                rc = dev_queue_xmit(response_skb); /* queue paket for transmitting */

                                if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
                                    printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
                                    goto next_message;
                                }
                                conn->send_seqnr++;
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
            ApplId = buffer[2] | (buffer[3] << 8);
            status = LOCAL_CAPI_PUT_MESSAGE(SOURCE_SOCKET_CAPI, ApplId, buffer);
            if ((status == ERR_SendBusy) || (status == ERR_QueueFull)) {
                if (rhdr->type == RCAPI_TYPE_APPL_END) {
                    conn->recombine_len -= rhdr->len;
                }
                skb_queue_head(&conn->recvqueue, skb);
                return;
            }
            if (status == ERR_OS_Resource) {
                unsigned short len;
                len = buffer[0] | (buffer[1] << 8);
                printk(KERN_ERR "capi_oslib_socket_put got ERR_OS_Resource for ApplId %d, Len=%d!\n", ApplId, len);
            }
        } else if (rhdr->type == RCAPI_TYPE_APPL_CONT) {
            /*--- printk(KERN_DEBUG "got RCAPI_TYPE_APPL_CONT\n"); ---*/
            if (conn->recombine_len == 0) {
                /*--- printk(KERN_DEBUG "erster Block buffer=%p len=%d\n", conn->recombine_buffer, rhdr->len); ---*/
                skb_pull(skb, sizeof(struct rcapihdr) + 1); /* strip headers */
                rhdr->len -= 1;
            } else {
                /*--- printk(KERN_DEBUG "weiterer Block len=%d\n", rhdr->len); ---*/
                skb_pull(skb, sizeof(struct rcapihdr)); /* strip headers */
            }
            if (conn->recombine_buffer == NULL) {
                conn->recombine_buffer = kmalloc(RCAPI_SEND_BUFSIZ, GFP_KERNEL);
                if (conn->recombine_buffer == NULL) {
                    printk(KERN_ERR "capi_oslib: kmalloc failed\n");
                    goto next_message;
                } else {
                    /*--- printk(KERN_DEBUG "capi_oslib: buffer %p alloced\n", conn->recombine_buffer); ---*/
                }
            }
            if (conn->recombine_len + rhdr->len <= RCAPI_SEND_BUFSIZ) {
                memcpy(&conn->recombine_buffer[conn->recombine_len], skb->data, rhdr->len);
                conn->recombine_len += rhdr->len;
            } else {
                printk(KERN_ERR "capi_oslib: recombine buffer too small!\n");
            }
        }
next_message:
        kfree_skb(skb);
    }
}
 
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void capi_oslib_socket_get
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
            (void* arg){
            struct _capi_connections *conn = (struct _capi_connections*)arg;
#else/*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/
            (struct work_struct *work){
            struct _capi_connections *conn = container_of(work, struct _capi_connections, rx_work);
#endif/*--- #else ---*//*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/
    unsigned int status = ERR_NoError;
    unsigned char* buffer = NULL;

    /*--- printk(KERN_DEBUG "capi_oslib_socket_get %p\n", arg); ---*/
    if (conn == NULL) return;
    /*--- printk(KERN_DEBUG "capi_oslib_socket_get for Appl %d\n", conn->ApplId); ---*/
    if (conn->ApplId == (unsigned)-1) return;


    while (status == ERR_NoError) {
        status = LOCAL_CAPI_GET_MESSAGE(SOURCE_SOCKET_CAPI, conn->ApplId, &buffer, CAPI_NO_SUSPEND);
        DEB_INFO("LOCAL_CAPI_GET_MESSAGE(%d, CAPI_NO_SUSPEND) -> %04x, %p\n", conn->ApplId, status, buffer);
        if (status == ERR_NoError) {
            struct __attribute__ ((packed)) _capi_message *C = (struct __attribute__ ((packed)) _capi_message *)buffer;
            unsigned total_msg_len;
            struct sk_buff *skb;
            u8 *data;
            int rc;
            struct net_device *dev;
            unsigned left;
            unsigned data_left;
            unsigned msg_left;

            dev = capi_device();
            if (likely(dev)) {
                total_msg_len = copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.Length) + 1;
                if (CA_IS_DATA_B3_IND(buffer)) {
                    total_msg_len += copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.DataLen);
                    data_left = copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.DataLen);
                } else {
                    data_left = 0;
                }
                msg_left = copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.Length);
                left = total_msg_len;
                while (left) {
                    unsigned int thissize;
                    unsigned int tocopy = 0;
                    unsigned char type;

                    thissize = min((unsigned int)left, (unsigned int)(RCAPI_MAX_FRAME_SIZE - RCAPI_HEADROOM));
                    if (thissize == total_msg_len) {
                        type = RCAPI_TYPE_APPL;
                    } else if (left > thissize) {
                        type = RCAPI_TYPE_APPL_CONT;
                    } else {
                        type = RCAPI_TYPE_APPL_END;
                    }
                    skb = capi_oslib_allocskb(thissize, type, conn->conindex, conn->send_seqnr, dev, &data, GFP_KERNEL);
                    if (unlikely(!skb)) {
                        printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
                        break;
                    } else {
                        left -= thissize;
                        if (msg_left == copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.Length)) {
                            *data++ = 1;    /* normal CAPI Message */
                            thissize -= 1;
                        }

                        if (msg_left) {
                            /* noch Message übrig */
                            tocopy = min((unsigned int)thissize, msg_left);
                            memcpy(data, &buffer[copy_word_from_le_aligned((unsigned char *)&C->capi_message_header.Length) - msg_left], tocopy);
                            msg_left -= tocopy;
                            data += tocopy;
                            thissize -= tocopy;
                        }
                        
                        if (CA_IS_DATA_B3_IND(buffer)) {
                            if ((data_left) && (thissize > 0)) {
                                /* Message komplett && noch Platz für Daten übrig */
                                tocopy = min((unsigned int)thissize, data_left);
                                memcpy(data, &C->capi_message_part.data_b3_ind.Data[copy_word_from_le_aligned((unsigned char *)&C->capi_message_part.data_b3_ind.DataLen) - data_left], tocopy);
#if defined(CAPIOSLIB_CHECK_LATENCY)
                                /*--- capi_parse_timestamp(0x20 + (conn->ApplId & 0x1F), (char *)__func__, data, tocopy); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
                                data_left -= tocopy;
                            }
                        }

                        rc = dev_queue_xmit(skb); /* queue paket for transmitting */

                        if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
                            printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
                        }
                        conn->send_seqnr++;
                    }
                }
            } else {
                printk(KERN_WARNING "capi_oslib: drop msg, no dev\n");
            }
        }
        /*--- printk(KERN_DEBUG "get done\n"); ---*/
    }
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
static int capi_oslib_recv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt __attribute__((unused)), struct net_device* orig_dev __attribute__((unused)))
#else
static int capi_oslib_recv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt)
#endif
{
   struct rcapihdr *rhdr;
   struct _capi_connections *conn = NULL;
   struct list_head *lp;

    if (dev != capi_device())
        goto freeskb;
 
#ifndef this_checks_are_not_really_needed_in_our_environment
    if ((skb = skb_share_check(skb, GFP_ATOMIC)) == NULL) {
        printk(KERN_ERR "capi_oslib: recv: skb_share_check failed\n");
        goto out_of_mem;
    }
 
    if (skb_is_nonlinear(skb)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
        if (skb_linearize(skb) != 0) {
#else
        if (skb_linearize(skb, GFP_ATOMIC) != 0) {
#endif
            printk(KERN_ERR "capi_oslib: recv: skb_linearize failed\n");
            goto freeskb;
        }
    }
    if (skb_cloned(skb) && !skb->sk) {
        struct sk_buff *nskb = skb_copy(skb, GFP_ATOMIC);
 
        if (!nskb) {
            printk(KERN_ERR "capi_oslib: recv: skb_copy failed\n");
            goto freeskb;
        }
        kfree_skb(skb);
        skb = nskb;
    }
#endif

    if (unlikely(skb->len < sizeof(struct rcapihdr))) {
        printk(KERN_ERR "capi_oslib: recv: packet too small\n");
        goto freeskb;
    }
    rhdr = (struct rcapihdr *) skb->data;

    switch (rhdr->type)
    {
        case RCAPI_TYPE_APPL:
        case RCAPI_TYPE_APPL_CONT:
        case RCAPI_TYPE_APPL_END:
            list_for_each(lp, &capi_conn_list) {
                conn = list_entry(lp, struct _capi_connections, list);
                if (conn->conindex == rhdr->conindex) break;
            }
            if ((conn == NULL) || (conn->conindex != rhdr->conindex)) {
                if (rhdr->type == RCAPI_TYPE_APPL) {
                    /*--- printk(KERN_WARNING "capi_oslib: recv: conindex %d not in use\n", rhdr->conindex); ---*/

                    conn = kmalloc(sizeof(struct _capi_connections), GFP_KERNEL);
                    if (conn == NULL) {
                        goto freeskb;
                    }
                    conn->conindex = rhdr->conindex;
                    conn->send_seqnr = 0;
                    conn->recv_seqnr = 0;
                    conn->ApplId = (unsigned)-1;
                    conn->recombine_buffer = NULL;
                    conn->recombine_len = 0;
                    conn->B3BlockSize = 0;
                    skb_queue_head_init(&conn->recvqueue);
                    list_add(&conn->list, &capi_conn_list);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
                    INIT_WORK(&conn->tx_work, capi_oslib_socket_put, (void*)conn);
#else
                    INIT_WORK(&conn->tx_work, capi_oslib_socket_put);
#endif
                } else {
                    printk(KERN_ERR "capi_oslib: got APPL_CONT or _END w/o conn!\n");
                    goto freeskb;
                }
            }
        default:
            break;
    }

    switch (rhdr->type) {
        case RCAPI_TYPE_APPL:
#if 0
            if (unlikely(rhdr->conindex >= RCAPIMAXAPPL)) {
                printk(KERN_ERR "capi_oslib: recv: illegal conindex %d\n", rhdr->conindex);
                goto freeskb;
            }
#endif
            if (rhdr->seqnr != conn->recv_seqnr) {
                printk(KERN_WARNING "capi_oslib: recv: conn %d lost packet(s)!\n", conn->conindex);
            }
            conn->recv_seqnr = rhdr->seqnr + 1;

#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 19)
            if (skb->tstamp.off_sec == 0) {
                struct timeval tv;
                do_gettimeofday(&tv);
                skb->tstamp.off_sec = tv.tv_sec;
                skb->tstamp.off_usec = tv.tv_usec;
            }
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
            {
            struct timeval tv;
            skb_get_timestamp(skb, &tv);
            if (tv.tv_sec == 0) {
                __net_timestamp(skb);
            }
            }
#else
            if (skb->stamp.tv_sec == 0) {
                get_fast_time(&skb->stamp);
            }
#endif/*--- #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) ---*/

            /*--- printk(KERN_DEBUG "skb->len now %d\n", skb->len); ---*/

            if (skb->len > 0) {
                unsigned char type = skb->data[sizeof(struct rcapihdr)];
                switch (type) {
                    case 0:
                        skb_pull(skb, sizeof(struct rcapihdr)); /* strip headers */
                        if (rhdr->type != RCAPI_TYPE_APPL) {
                            printk(KERN_ERR "capi_oslib: non-msg type only supported for RCAPI_TYPE_APPL!\n");
                            goto freeskb;
                        }
                        if ((skb->len >= 4) && (memcmp(skb->data, "\0Bye", 4) == 0)) {
                            /*--- printk(KERN_DEBUG "capi_oslib: client %d waves good-bye\n", conn->conindex); ---*/
                            goto exit_reader_thread;
                        } else
                        if ((skb->len >= 6) && (memcmp(skb->data, "\0Hallo", 6) == 0)) {
                            /*--- printk(KERN_DEBUG "capi_oslib: got greetings, send profile\n"); ---*/
                            /*-------------------------------------------------------------------------------------*\
                             * CAPI Profile etc.pp rüberschicken
                             *
                             * 4 Bytes Num. Controllers
                             *      64 Bytes Manufacturer
                             *      4  Bytes CAPI Major
                             *      4  Bytes CAPI Minor
                             *      4  Bytes Manu Major
                             *      4  Bytes Manu Minor
                             *      8  Bytes Serial No.
                             *      64 Bytes Profile
                            \*-------------------------------------------------------------------------------------*/
                            unsigned char info[64];
                            unsigned int num_controllers, i;
                            struct sk_buff *skb;
                            unsigned total_msg_len;
                            u8 *data;
                            int rc;
                            struct net_device *dev;

                            dev = capi_device();
                            if (unlikely(!dev)) {
                                printk(KERN_WARNING "capi_oslib: send profile but no dev!\n");
                                break;
                            }
                            CAPI_GET_PROFILE(info, 0);
                            num_controllers = EXTRACT_DWORD(info);

                            total_msg_len = 4 + (num_controllers * 152) + 1;
                            skb = capi_oslib_allocskb(total_msg_len, RCAPI_TYPE_APPL, conn->conindex, conn->send_seqnr, dev, &data, GFP_ATOMIC);
                            if (unlikely(!skb)) {
                                printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
                                goto exit_reader_thread;
                            } else {
                                *data++ = 0;    /* keine CAPI Message */
                                memcpy(data, info, sizeof(unsigned int));
                                data += sizeof(unsigned int);
                                for (i = 1; i <= num_controllers; i++) {
                                    CAPI_GET_MANUFACTURER(info);
                                    DEB_INFO("start info controller %d\n", i);
                                    memcpy(data, info, 64);
                                    data += 64;

                                    CAPI_GET_VERSION((unsigned int*)&info[0], (unsigned int*)&info[4], (unsigned int*)&info[8],
                                                     (unsigned int*)&info[12]);
                                    memcpy(data, info, 16);
                                    data += 16;

                                    CAPI_GET_SERIAL_NUMBER(i, info);
                                    memcpy(data, info, 8);
                                    data += 8;

                                    CAPI_GET_PROFILE(info, i);
                                    memcpy(data, info, 64);
                                    data += 64;
                                }

                                rc = dev_queue_xmit(skb); /* queue paket for transmitting */

                                if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
                                    printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
                                }
                                conn->send_seqnr++;
                            }
                        }
                        kfree_skb(skb);
                        return NET_RX_SUCCESS;
                        break;
                    case 1:
                        skb_queue_tail(&conn->recvqueue, skb); /* add to receive queue */
                        queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, capi_remote_put_workqueue, &conn->tx_work);
                        return NET_RX_SUCCESS;
                        break;
                    default:
                        printk(KERN_ERR "capi_oslib: unknown type2 %d\n", type);
                        goto freeskb;
                        break;
                }
            } else {
                goto freeskb;
            }
            break;

        case RCAPI_TYPE_APPL_CONT:
        case RCAPI_TYPE_APPL_END:
            skb_queue_tail(&conn->recvqueue, skb); /* add to receive queue */
            queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, capi_remote_put_workqueue, &conn->tx_work);
            return NET_RX_SUCCESS;
        case RCAPI_TYPE_RESERVED:
            printk(KERN_WARNING "capi_oslib: got msg of type RCAPI_TYPE_RESERVED\n");
            goto freeskb;
        case RCAPI_TYPE_PING:
            /*--- printk(KERN_DEBUG "capi_oslib: got PING!\n"); ---*/
            {
                struct sk_buff *pong_skb;
                int rc;
                struct net_device *dev;

                dev = capi_device();
                if (unlikely(!dev)) {
                    printk(KERN_WARNING "capi_oslib: got ping but have no dev!\n");
                    goto freeskb;
                }
                pong_skb = capi_oslib_allocskb(0, RCAPI_TYPE_PING, 0, 0, dev, NULL, GFP_ATOMIC);
                if (unlikely(!pong_skb)) {
                    printk(KERN_ERR "capi_oslib: cannot allocate skb!\n");
                } else {
                    rc = dev_queue_xmit(pong_skb); /* queue paket for transmitting */

                    if (unlikely(!(rc == NET_XMIT_SUCCESS || rc == NET_XMIT_CN))) {
                        printk(KERN_ERR "capi_oslib: dev_queue_xmit()=%d\n", rc);
                    }
                }
            }
            kfree_skb(skb);
            return NET_RX_SUCCESS;
            break;
        default:
            printk(KERN_ERR "capi_oslib: recv: type %d not implemented\n", rhdr->type);
            goto freeskb;
   }

exit_reader_thread:
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
    INIT_WORK(&conn->remove, capi_oslib_remove_conn, (void*)conn);
#else
    INIT_WORK(&conn->remove, capi_oslib_remove_conn);
#endif
    queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, capi_remote_put_workqueue, &conn->remove);
freeskb:
   kfree_skb(skb);
out_of_mem:
   return NET_RX_DROP;
}

static struct packet_type rcapi_packet_type = {
 type:__constant_htons(ETH_P_RCAPI),
 func:capi_oslib_recv,
};


/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static int capi_oslib_netdev_notifier_event(struct notifier_block *notifier __attribute__((unused)),
                                       unsigned long event, void *ptr)
{
   struct net_device *dev = (struct net_device *) ptr;

   switch (event) {
      case NETDEV_UNREGISTER:
         if (dev == capi_netdev) {
            printk(KERN_NOTICE "capi_oslib: device %s gone.\n", dev->name);
            capi_netdev = 0;
         }
         break;

      case NETDEV_REGISTER:
         (void) capi_device();
         break;
   }
   return NOTIFY_DONE;
}

static struct notifier_block capi_oslib_netdev_notifier = {
   .notifier_call = capi_oslib_netdev_notifier_event
};

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void capi_oslib_socket_init(void)
{
#if 0
    struct sockaddr_in addr;
    
    if (sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &capi_socket) < 0)
    {
        DEB_INFO("capi_oslib_socket_init(): Error during creation of socket\n");
        capi_socket = NULL;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)1234);
    addr.sin_addr.s_addr = 0;

    if (capi_socket->ops->bind(capi_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        DEB_INFO("capi_oslib_socket_init(): bind failed\n");
        sock_release(capi_socket);
        capi_socket = NULL;
        return;
    }

    if (capi_socket->ops->listen(capi_socket, 1) < 0)
    {
        DEB_INFO("capi_oslib_socket_init(): listen failed\n");
        sock_release(capi_socket);
        capi_socket = NULL;
        return;
    }
#endif

    LOCAL_CAPI_INIT(SOURCE_SOCKET_CAPI);

    register_netdevice_notifier(&capi_oslib_netdev_notifier);
    dev_add_pack(&rcapi_packet_type);
    /*--- kernel_thread(capi_oslib_socket_accept_thread, NULL, 0); ---*/
    capi_remote_put_workqueue = create_workqueue("capi_remote_put");
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void capi_oslib_socket_release(void)
{
#if 0
    if (capi_socket) {
        sock_release(capi_socket);
        capi_socket = NULL;
    }
#endif
    unregister_netdevice_notifier(&capi_oslib_netdev_notifier);
    dev_remove_pack(&rcapi_packet_type);
}
