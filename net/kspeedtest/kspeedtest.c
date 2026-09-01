/*-------------------------------------------------------------------------------
 *
 *   Copyright (C) 2006,2007,2008,2009,2010 AVM GmbH <fritzbox_info@avm.de>
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
 *-------------------------------------------------------------------------------
 *
 *-------------------------------------------------------------------------------
 * kspeedtest: AVM Kernel Speedtest
 *-------------------------------------------------------------------------------
 *
 * After module load kspeedtest can be controlled via proc interface
 * /proc/net/kspeedtest
 *
 * echo <command> > /proc/net/kspeedtest/control
 *
 * Commands:
 *
 * - start tcp [port] 	- Start TCP server (default Port 4711)
 * - start udp [port] 	- Start UDP server (default Port 4711)
 * - stop tcp			- Stop TCP server
 * - stop udp			- Stop UDP server
 * - stop all			- Stop all servers
 * - verbose			- Print stats periodically as kernel message
 * - noverbose  		- Turn off verbose
 * - bidirect [port]    - Bidirect UDP traffic (default Port 4712)
 * - nobidirect         - Stop bidirectional traffic
 * - reset              - Reset stats
 * 
 * Show stats (Received Bytes, BPS, AVG BPS):
 *
 * cat /proc/net/kspeedtest/stats
 *
 * Show connections
 *
 * cat /proc/net/kspeedtest/connections
 *
 * vim: expandtab fileencoding=utf-8
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>

#include <linux/errno.h>
#include <linux/types.h>

#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/in6.h>
#include <linux/proc_fs.h>
#include <linux/smp_lock.h>

#include <linux/delay.h>
#include <linux/socket.h>
#include <linux/tcp.h>
#include <net/sock.h>
#include <net/tcp_states.h>

#define DEFAULT_UDP_PORT 4711
#define DEFAULT_UDP_BIDIRECT_PORT 4712
#define DEFAULT_TCP_PORT 4711
#define MODULE_NAME "kspeedtest"

#undef NO_AUTO_RWIN

const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;

MODULE_LICENSE("GPL");

static struct timer_list ktimer;

static void Second(unsigned long param);

static struct kspeedtest {
   int secondtimer;
   int verbose;
   int null_vpid;
   unsigned int sleepdelay;
   unsigned udp_port;
   unsigned udp_bidirect_port;
   unsigned tcp_port;
   unsigned int seconds;
   unsigned int packets;
   unsigned long total_packets;
   unsigned long bytes;
   unsigned long long total_bytes;
   unsigned int current_bps;
   unsigned int current_pps;
   unsigned int avg_bps;
   unsigned int avg_pps;
   unsigned int udp_received;
   unsigned int udp_bidirect_received;
   unsigned long last_udp_timestamp;
   unsigned long last_udp_bidirect_timestamp;
} kspeedtest;

enum client_state {
   client_init,
   client_listen,
   client_connected
};

#define MAX_CLIENTS 48

struct socket_list {
   struct socket_list *next;
   struct socket *sock;
};

struct kthread_t
{
    struct task_struct *thread;
	struct task_struct *worker;
    struct socket *sock;
    struct socket_list *clients;
	int current_client;
	enum client_state client_state;
    struct sockaddr_in6 addr;
    int running;
};

struct kthread_t *udp_thread = NULL;
struct kthread_t *udp_bidirect_thread = NULL;
struct kthread_t *tcp_thread = NULL;

/* function prototypes */
static int udp_receive(struct socket *sock, struct sockaddr_in6 *addr, unsigned char *buf, int len, int bidirect);
static int udp_send(struct socket *sock, struct sockaddr_in6 *addr, unsigned char *buf, int len);

static void __init kspeedtest_proc_init(void);
static void __init kspeedtest_proc_exit(void);

typedef int my_fprintf(void *, const char *, ...)
#ifdef __GNUC__
	        __attribute__ ((__format__(__printf__, 2, 3)))
#endif
	        ;

static void reset_stats(void)
{
	 kspeedtest.seconds = 0;
	 kspeedtest.packets = 0;
	 kspeedtest.total_packets = 0;
	 kspeedtest.bytes = 0;
	 kspeedtest.total_bytes = 0;
	 kspeedtest.current_bps = 0;
	 kspeedtest.current_pps = 0;
	 kspeedtest.avg_bps = 0;
	 kspeedtest.avg_pps = 0;
}


static void Second(unsigned long param)
{
	u64 x64;

	// Stats of Null VPID
#ifdef CONFIG_TI_PACKET_PROCESSOR
	if (kspeedtest.null_vpid != -1) {
	   TI_PP_VPID_STATS vstats;
	   ti_ppm_get_vpid_stats(kspeedtest.null_vpid, &vstats);
	   ti_ppm_clear_vpid_stats(kspeedtest.null_vpid);
	   kspeedtest.bytes += vstats.tx_byte_lo;
	   kspeedtest.packets += vstats.tx_unicast_pkt;
	   if (vstats.tx_byte_lo > 0) {
	       kspeedtest.udp_received = 1;
		   kspeedtest.last_udp_timestamp = jiffies;
	   }
	}
#endif

	kspeedtest.seconds++;
	kspeedtest.total_bytes += kspeedtest.bytes;
	kspeedtest.total_packets += kspeedtest.packets;
	kspeedtest.current_bps = kspeedtest.bytes*8;
	x64 = kspeedtest.total_bytes*8;
	do_div(x64, kspeedtest.seconds);
	kspeedtest.avg_bps = x64;
	kspeedtest.current_pps = kspeedtest.packets;
	kspeedtest.avg_pps = kspeedtest.total_packets / kspeedtest.seconds;
    ktimer.expires = jiffies + HZ;
	add_timer(&ktimer);

	if (kspeedtest.verbose && kspeedtest.bytes)
		printk(KERN_ERR MODULE_NAME": Bytes %llu \t BPS %u AVG %u Packets %lu PPS %u AVG %u\n", 
			kspeedtest.total_bytes, kspeedtest.current_bps, 
			kspeedtest.avg_bps,	kspeedtest.total_packets, 
			kspeedtest.current_pps, kspeedtest.avg_pps);

	if (kspeedtest.verbose && tcp_thread) {
	   struct socket_list *sl = tcp_thread->clients;
	   int count = 0;
	   for (; sl; sl = sl->next) count++;
	   printk(KERN_ERR "nr clients %d \n", count);
	}

	kspeedtest.bytes = 0;
	kspeedtest.packets = 0;
	if (jiffies > kspeedtest.last_udp_timestamp + HZ)
		kspeedtest.udp_received = 0;
	if (jiffies > kspeedtest.last_udp_bidirect_timestamp + HZ)
		kspeedtest.udp_bidirect_received = 0;
}

static void kspeedtest_show_stats(my_fprintf fprintffunc, void *arg)
{
	(*fprintffunc)(arg, "Bytes %llu \t BPS %u \t AVG %u Packets %lu PPS %u AVG %u\n",
			kspeedtest.total_bytes, kspeedtest.current_bps, 
			kspeedtest.avg_bps,	kspeedtest.total_packets, 
			kspeedtest.current_pps, kspeedtest.avg_pps);
}

/***************************************************************************/
/*                             UDP                                         */
/***************************************************************************/

static int kspeedtest_udp_thread(void)
{
	int size, err;
	int bufsize = 1500;
	unsigned char buf[bufsize+1];

	udp_thread->running = 1;
	current->flags |= PF_NOFREEZE;

	daemonize(MODULE_NAME);
	allow_signal(SIGKILL);

	/* create a udp socket */
	if ((err = sock_create_kern(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, &udp_thread->sock)) < 0) {
		printk(KERN_ERR MODULE_NAME": Could not create a datagram socket, error = %d\n", -ENXIO);
		goto out;
	}

	memset(&udp_thread->addr, 0, sizeof(struct sockaddr));
	udp_thread->addr.sin6_family      = AF_INET6;
	udp_thread->addr.sin6_addr 		  = in6addr_any;
	udp_thread->addr.sin6_port        = 
		htons(kspeedtest.udp_port ? kspeedtest.udp_port : DEFAULT_UDP_PORT);

	if ( (err = udp_thread->sock->ops->bind(udp_thread->sock, 
					(struct sockaddr *)&udp_thread->addr, sizeof(struct sockaddr_in6) ) ) < 0) {
		printk(KERN_ERR MODULE_NAME": Could not bind or connect to udp socket, error = %d\n", -err);
		goto close_and_out;
	}

	printk(KERN_INFO MODULE_NAME": listening on port %d\n", 
			ntohs(udp_thread->addr.sin6_port));

	memset(&buf, 0, bufsize+1);

	for (;;) {
		
		if (signal_pending(current))
			break;

	    size = udp_receive(udp_thread->sock, &udp_thread->addr, buf, bufsize, 0);
		if (size < 0) {
			printk(KERN_ERR MODULE_NAME": sock_recvmsg error = %d\n", size);
		} else if (size > 0) {
		    kspeedtest.udp_received = 1;
			kspeedtest.last_udp_timestamp = jiffies;
			kspeedtest.bytes += size;
			kspeedtest.packets++;
		    if (kspeedtest.verbose > 1) 
			  printk(KERN_ERR "udp receive: got %d bytes\n", size);
		}

		if (size > 0)
		   udelay(kspeedtest.sleepdelay ? kspeedtest.sleepdelay : 10);
		else
		   msleep(10);
	}
close_and_out:
	sock_release(udp_thread->sock);
	udp_thread->sock = NULL;
out:
	udp_thread->thread = NULL;
	udp_thread->running = 0;
#ifdef CONFIG_TI_PACKET_PROCESSOR
	if (kspeedtest.null_vpid != -1)
	   ti_ppm_delete_vpid(kspeedtest.null_vpid);
    kspeedtest.null_vpid = -1;
#endif
	return 0;
}

static int kspeedtest_udp_bidirect_thread(void)
{
	int size, err;
	int bufsize = 1500;
	unsigned char buf[bufsize+1];
	struct sockaddr_in6 sendaddr;

	udp_bidirect_thread->running = 1;
	current->flags |= PF_NOFREEZE;

	daemonize(MODULE_NAME);
	allow_signal(SIGKILL);

	/* create a udp socket */
	if ((err = sock_create_kern(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, &udp_bidirect_thread->sock)) < 0) {
		printk(KERN_ERR MODULE_NAME": Could not create a datagram socket, error = %d\n", -ENXIO);
		goto out;
	}

	memset(&udp_bidirect_thread->addr, 0, sizeof(struct sockaddr));
	udp_bidirect_thread->addr.sin6_family = AF_INET6;
	udp_bidirect_thread->addr.sin6_addr   = in6addr_any;
	udp_bidirect_thread->addr.sin6_port   = htons(kspeedtest.udp_bidirect_port 
			                                      ? kspeedtest.udp_bidirect_port 
			                                      : DEFAULT_UDP_BIDIRECT_PORT);

	if ( (err = udp_bidirect_thread->sock->ops->bind(udp_bidirect_thread->sock, 
					(struct sockaddr *)&udp_bidirect_thread->addr, sizeof(struct sockaddr_in6) ) ) < 0) {
		printk(KERN_ERR MODULE_NAME": Could not bind or connect to udp socket, error = %d\n", -err);
		goto close_and_out;
	}

	printk(KERN_INFO MODULE_NAME": listening on port %d\n", 
			ntohs(udp_bidirect_thread->addr.sin6_port));

	memset(&buf, 0, bufsize+1);

	for (;;) {
		
		if (signal_pending(current))
			break;

	    size = udp_receive(udp_bidirect_thread->sock, &udp_bidirect_thread->addr, buf, bufsize, 1);
		if (size < 0) {
			printk(KERN_ERR MODULE_NAME": sock_recvmsg error = %d\n", size);
		} else if (size > 0) {
			kspeedtest.bytes += size;
			kspeedtest.packets++;
		    kspeedtest.udp_bidirect_received = 1;
			kspeedtest.last_udp_bidirect_timestamp = jiffies;
		    if (kspeedtest.verbose > 1) 
			  printk(KERN_ERR "udp receive: got %d bytes\n", size);
		}

		sendaddr.sin6_addr = udp_bidirect_thread->addr.sin6_addr;
		sendaddr.sin6_family = AF_INET6;
		sendaddr.sin6_port = htons(kspeedtest.udp_bidirect_port
				                    ? kspeedtest.udp_bidirect_port
									: DEFAULT_UDP_BIDIRECT_PORT); 
        size = udp_send(udp_bidirect_thread->sock, &sendaddr, buf, size);
		if (size < 0)
		   printk(KERN_ERR MODULE_NAME": sock_sendmsg error = %d\n", size);
		else if (kspeedtest.verbose > 1) 
		   printk(KERN_ERR "udp_send: sent %d bytes\n", size);
	}
close_and_out:
	sock_release(udp_bidirect_thread->sock);
	udp_bidirect_thread->sock = NULL;
out:
	udp_bidirect_thread->thread = NULL;
	udp_bidirect_thread->running = 0;
	return 0;
}


static int udp_send(struct socket *sock, struct sockaddr_in6 *addr, unsigned char *buf, int len)
{
	struct msghdr msg;
	struct iovec iov;
	mm_segment_t oldfs;
	int size = 0;

	if (sock->sk==NULL)
	   return 0;

	iov.iov_base = buf;
	iov.iov_len = len;

	msg.msg_flags = 0;
	msg.msg_name = addr;
	msg.msg_namelen  = sizeof(struct sockaddr_in6);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	size = sock_sendmsg(sock,&msg,len);
	set_fs(oldfs);

	return size;
}

static int udp_receive(struct socket* sock, struct sockaddr_in6* addr, unsigned char* buf, int len, int bidirect)
{
   int size = 0;
   if (bidirect) {
	   struct msghdr msg;
	   struct iovec iov;
	   mm_segment_t oldfs;

	   if (sock->sk==NULL) return 0;

	   iov.iov_base = buf;
	   iov.iov_len = len;

	   msg.msg_flags = 0;
	   msg.msg_name = addr;
	   msg.msg_namelen  = sizeof(struct sockaddr_in6);
	   msg.msg_control = NULL;
	   msg.msg_controllen = 0;
	   msg.msg_iov = &iov;
	   msg.msg_iovlen = 1;
	   msg.msg_control = NULL;

	   oldfs = get_fs();
	   set_fs(KERNEL_DS);
	   size = sock_recvmsg(sock,&msg,len,msg.msg_flags);
	   set_fs(oldfs);
   /* 
	* As we don't need to know the recv_addr we can speed up:
	* - peek in sk_receive_queue and just count bytes and packets
	* - setup null VPID for packet accelerators 
	*/
   } else {
	  struct sk_buff_head *rcvq = &sock->sk->sk_receive_queue;
	  struct sk_buff *skb;

	  spin_lock_bh(&rcvq->lock);
	  while ((skb = skb_peek(rcvq)) != NULL) {
#ifdef CONFIG_TI_PACKET_PROCESSOR
		   // create null vpid
		   if (kspeedtest.null_vpid == -1
				 && skb->input_dev->vpid_handle != -1) {
			   kspeedtest.null_vpid = 
				  ti_ppm_create_vpid(&skb->input_dev->vpid_block);
			   if (kspeedtest.null_vpid < 0)
				  printk(KERN_ERR "creating null vpid failed \n");
			   else {
				  ti_ppm_set_vpid_flags( kspeedtest.null_vpid, TI_PP_VPID_FLG_RX_DISBL );
			   }
		   }
		   if (kspeedtest.null_vpid != -1) {
			   skb->pp_packet_info.ti_pp_flags |= TI_PPM_SESSION_ROUTED;
			   ti_hil_null_hook(skb, kspeedtest.null_vpid);
		   }
#endif
		   size += skb->len;
		   __skb_unlink(skb, rcvq);
		   kfree_skb(skb);
	   }
	   spin_unlock_bh(&rcvq->lock);
   }
   return size;
}

static int start_udp_thread(void) 
{
	if (udp_thread != NULL && udp_thread->thread != NULL) {
	   printk(KERN_ERR MODULE_NAME": udp bidirect thread already started!\n");
	   return -1;
	}
	if (udp_thread) kfree(udp_thread);

	udp_thread = kmalloc(sizeof(struct kthread_t), GFP_KERNEL);
	memset(udp_thread, 0, sizeof(struct kthread_t));
	udp_thread->thread = kthread_run((void *)kspeedtest_udp_thread, NULL, MODULE_NAME);

	if (IS_ERR(udp_thread->thread)) {
	  printk(KERN_ERR MODULE_NAME": unable to start kernel thread\n");
	  kfree(udp_thread);
	  udp_thread = NULL;
	  return -ENOMEM;
	}
	printk(KERN_INFO MODULE_NAME": kspeedtest_init: udp bidirect thread started \n");
	return 0;
}

static int start_udp_bidirect_thread(void)
{
	if (udp_bidirect_thread != NULL && udp_bidirect_thread->thread != NULL) {
	   printk(KERN_ERR MODULE_NAME": udp bidirect thread already started!\n");
	   return -1;
	}
	if (udp_bidirect_thread) kfree(udp_bidirect_thread);

	udp_bidirect_thread = kmalloc(sizeof(struct kthread_t), GFP_KERNEL);
	memset(udp_bidirect_thread, 0, sizeof(struct kthread_t));
	udp_bidirect_thread->thread = kthread_run((void *)kspeedtest_udp_bidirect_thread, NULL, MODULE_NAME);

	if (IS_ERR(udp_bidirect_thread->thread)) {
	  printk(KERN_ERR MODULE_NAME": unable to start kernel thread\n");
	  kfree(udp_bidirect_thread);
	  udp_bidirect_thread = NULL;
	  return -ENOMEM;
	}
	printk(KERN_INFO MODULE_NAME": kspeedtest_init: udp bidirect thread started \n");
	return 0;
}

static int stop_udp_thread(void)
{
	if (udp_thread == NULL) return 0;

	if (udp_thread->thread) {
	   (void)send_sig(SIGKILL, udp_thread->thread, 1);
	}
	if (udp_thread->thread==NULL)
		printk(KERN_ERR MODULE_NAME": no kernel thread to kill\n");
	else 
	{
		while(udp_thread->running) msleep(10);
		printk(KERN_INFO MODULE_NAME": successfully killed kernel udp thread\n");
	}
	kfree(udp_thread);
	udp_thread = NULL;
	return 0;
}

static int stop_udp_bidirect_thread(void)
{
	if (udp_bidirect_thread == NULL) return 0;

	if (udp_bidirect_thread->thread) {
	   (void)send_sig(SIGKILL, udp_bidirect_thread->thread, 1);
	}
	if (udp_bidirect_thread->thread==NULL)
		printk(KERN_ERR MODULE_NAME": no kernel thread to kill\n");
	else 
	{
		while(udp_bidirect_thread->running) msleep(10);
		printk(KERN_INFO MODULE_NAME": successfully killed kernel udp bidirect thread\n");
	}
	kfree(udp_bidirect_thread);
	udp_bidirect_thread = NULL;
	return 0;
}

/***************************************************************************/
/*                             TCP                                         */
/***************************************************************************/

static int tcp_receive(struct socket* sock, struct sockaddr_in6* addr, unsigned char* buf, int len)
{
	struct msghdr msg;
	struct iovec iov;
	mm_segment_t oldfs;
	int size = 0;

	if (sock->sk==NULL) return 0;

	iov.iov_base = buf;
	iov.iov_len = len;

	msg.msg_flags = MSG_MORE;
	msg.msg_name = addr;
	msg.msg_namelen  = sizeof(struct sockaddr_in);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	size = sock_recvmsg(sock,&msg,len,msg.msg_flags);
	set_fs(oldfs);

	return size;
}

static int tcp_handle_data(struct kthread_t *kthread, struct socket *client)
{
   int size;
   int bufsize = 1500;
   unsigned char buf[bufsize+1];

#if 0
   switch(kthread->client_state)
   {
   case client_init:
	   break;
   case client_listen:
	   {
#ifdef NO_AUTO_RWIN
		   int rwin = 256000;
#endif
		   err = sock_create_lite(AF_INET6, SOCK_STREAM, IPPROTO_TCP, &kthread->client);
		   if (err || !kthread->client) {
			   printk(KERN_ERR MODULE_NAME": error creating client socket");
			   return -1;
		   }

		   kthread->client->type = kthread->sock->type;
		   kthread->client->ops  = kthread->sock->ops;


		   err = kthread->sock->ops->accept(kthread->sock, kthread->client, 0);
		   if (err < 0) {
			   sock_release(kthread->client);
			   kthread->client = NULL;
			   printk(KERN_ERR MODULE_NAME": error accepting client connection");
			   return -2;
		   }
		   printk(KERN_INFO MODULE_NAME": client connection accepted \n ");
#ifdef NO_AUTO_RWIN
		   err = sock_setsockopt(kthread->client, SOL_SOCKET, SO_RCVBUF, 
			   (char *)&rwin, sizeof(rwin)); 
		   printk(KERN_INFO MODULE_NAME": setting rwin %d ret=%d\n", rwin, err);
#endif
		   kthread->client_state = client_connected;
		   kspeedtest.seconds = 0;
		   kspeedtest.total_bytes = 0;
		   kspeedtest.total_packets = 0;
	   }
	   break;
   case client_connected:
#endif
	   if (client->sk->sk_state == TCP_CLOSE_WAIT) {
		   printk(KERN_INFO MODULE_NAME": client connection closed\n");
		   return -1;
	   } else {
		   size = tcp_receive(client, &kthread->addr, buf, bufsize);

		   if (size < 0) {
			   printk(KERN_ERR MODULE_NAME": error getting bytes, sock_recvmsg error = %d\n", size);
		       printk(KERN_INFO MODULE_NAME": client connection closed\n");
		   } else {
			   kspeedtest.bytes += size;
			   kspeedtest.packets++;
		   }
	   }
   return size;
}

int kspeedtest_tcp_worker(void) 
{

   printk(KERN_ERR "kspeedtest_tcp_worker fired\n");


   lock_kernel();
   {
		tcp_thread->running = 1;
		current->flags |= PF_NOFREEZE;
		daemonize("kspeedtest_tcp");
		allow_signal(SIGKILL);
   }
   unlock_kernel();


   while (tcp_thread && tcp_thread->running == 1) {
	  struct socket_list **pp;
	  int count = 0;

      if (signal_pending(current)) 
		 break;

      for (pp = &tcp_thread->clients; *pp; ) {
		 if ((*pp)->sock) {
		    if (tcp_handle_data(tcp_thread, (*pp)->sock) < 0) {
			   struct socket_list *p = *pp;
			   sock_release((*pp)->sock);
			   *pp = (*pp)->next;
			   printk("delete client \n");
			   kfree(p);
			   continue;
			}
		 }
		 pp = &(*pp)->next;
		 count++;
	  }
	  if (count == 0) msleep(1000);
   }
   return 0;
}

static int kspeedtest_tcp_thread(void)
{
	int err;
#ifdef NO_AUTO_RWIN
	int rwin = 256000;
#endif
   struct inet_connection_sock *isock;

	DECLARE_WAITQUEUE(wait, current);

	lock_kernel();
	{
		/* kernel thread initialization */
		tcp_thread->running = 1;
		current->flags |= PF_NOFREEZE;
		/* daemonize (take care with signals, after daemonize() they are disabled) */
		daemonize(MODULE_NAME);
		allow_signal(SIGKILL);
	}
	unlock_kernel();

	/* create a tcp socket */
	if ((err = sock_create_kern(AF_INET6, SOCK_STREAM, IPPROTO_TCP, &tcp_thread->sock)) < 0) {
		printk(KERN_ERR MODULE_NAME": Could not create a stream socket, error = %d\n", -ENXIO);
		goto out;
	}

	memset(&tcp_thread->addr, 0, sizeof(struct sockaddr));
	tcp_thread->addr.sin6_family      = AF_INET6;
	tcp_thread->addr.sin6_addr        = in6addr_any;
	tcp_thread->addr.sin6_port        = 
		htons(kspeedtest.tcp_port ? kspeedtest.tcp_port : DEFAULT_TCP_PORT);

#ifdef NO_AUTO_RWIN
	err = sock_setsockopt(tcp_thread->sock, SOL_SOCKET, SO_RCVBUF, 
			(char *)&rwin, sizeof(rwin)); 
	printk(KERN_INFO MODULE_NAME":  rwin %d ret=%d\n", rwin, err);
#endif

	if ( (err = tcp_thread->sock->ops->bind(tcp_thread->sock, 
					(struct sockaddr *)&tcp_thread->addr, sizeof(struct sockaddr_in6) ) ) < 0) {
		printk(KERN_ERR MODULE_NAME": Could not bind or connect to tcp socket, error = %d\n", -err);
		goto close_and_out;
	}
	//tcp_thread->sock->sk->sk_reuse = 1;
	tcp_thread->sock->ops->listen(tcp_thread->sock, 48); //why 48?
	tcp_thread->client_state = client_listen;

	printk(KERN_INFO MODULE_NAME"(tcp_thread): listening on port %d\n", 
			ntohs(tcp_thread->addr.sin6_port));

	tcp_thread->worker = kthread_run((void *)kspeedtest_tcp_worker, NULL, MODULE_NAME);
    
	isock = inet_csk(tcp_thread->sock->sk);

	while (err >= 0) {
	    struct socket *client_sock;
		struct socket_list **pp, *p;

		if (signal_pending(current)) 
			break;

		if (reqsk_queue_empty(&isock->icsk_accept_queue)) {
			add_wait_queue(tcp_thread->sock->sk->sk_sleep, &wait);
			__set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ);
			__set_current_state(TASK_RUNNING);
			remove_wait_queue(tcp_thread->sock->sk->sk_sleep, &wait);
			continue;
		}
        err = sock_create_lite(AF_INET6, SOCK_STREAM, IPPROTO_TCP, &client_sock);

        client_sock->type = tcp_thread->sock->type;
        client_sock->ops  = tcp_thread->sock->ops;

        if (err || !client_sock) {
	       printk(KERN_ERR MODULE_NAME"(tcp_thread): error creating client socket");
		   break;
        }
		for (pp = &tcp_thread->clients; *pp; pp = &(*pp)->next)
		   ;

	    p = kmalloc(sizeof(struct socket_list), GFP_KERNEL);
		p->sock = client_sock;
		p->next = 0;

		printk(KERN_ERR MODULE_NAME"(tcp_thread): accept new connection \n");
		err = tcp_thread->sock->ops->accept(tcp_thread->sock, client_sock, O_NONBLOCK);

		*pp = p;
	}

close_and_out:
	sock_release(tcp_thread->sock);
	tcp_thread->sock = NULL;
out:
	tcp_thread->thread = NULL;
	tcp_thread->running = 0;
	return 0;
}

static int start_tcp_thread(void) 
{
	if (tcp_thread != NULL && tcp_thread->thread != NULL) {
		printk(KERN_ERR MODULE_NAME": tcp thread already started!\n");
		return -1;
	}
	if (tcp_thread) kfree(tcp_thread);
	tcp_thread = kmalloc(sizeof(struct kthread_t), GFP_KERNEL);
	memset(tcp_thread, 0, sizeof(struct kthread_t));
	tcp_thread->thread = kthread_run((void *)kspeedtest_tcp_thread, 
			                         NULL, MODULE_NAME);
	tcp_thread->client_state = client_init;
	if (IS_ERR(tcp_thread->thread)) 
	{
		printk(KERN_ERR MODULE_NAME": unable to start kernel thread\n");
		kfree(tcp_thread);
		tcp_thread = NULL;
		return -ENOMEM;
	}
	printk(KERN_INFO MODULE_NAME": kspeedtest_init: tcp thread started \n"); 
	return 0;
}

static int stop_tcp_thread(void)
{
    struct socket_list *sl;

	if (tcp_thread == NULL) return 0;

    for(sl = tcp_thread->clients; sl; sl = sl->next) {
	   struct socket_list *tmp;
	   if (sl->sock)
		  sock_release(sl->sock);
	   tmp = sl;
	   sl = sl->next;
	   kfree(tmp);
	}

	if (tcp_thread->thread) {
	   (void)send_sig(SIGKILL, tcp_thread->thread, 1);
	}
	if (tcp_thread->worker) {
	   (void)send_sig(SIGKILL, tcp_thread->worker, 1);
	}

	if (tcp_thread->thread==NULL)
		printk(KERN_ERR MODULE_NAME": no kernel thread to kill\n");
	else 
	{
		while(tcp_thread->running) msleep(10);
		printk(KERN_INFO MODULE_NAME": successfully killed kernel tcp thread\n");
	}
	kfree(tcp_thread);
	tcp_thread = NULL;
	return 0;
}

/*********************************************************************************/

static int __init kspeedtest_init(void)
{


	init_timer(&ktimer);
	ktimer.function = Second;
	ktimer.data = 0;
	ktimer.expires = jiffies + HZ;
	add_timer(&ktimer);
	printk(KERN_INFO MODULE_NAME": kspeedtest_init: second timer started \n"); 
	kspeedtest.null_vpid = -1;
	reset_stats();

#ifdef CONFIG_PROC_FS
	kspeedtest_proc_init();
#endif

	printk(KERN_INFO MODULE_NAME": kspeedtest_init done\n"); 

	return 0;
}

static void __exit kspeedtest_exit(void)
{
	del_timer_sync(&ktimer);

	stop_udp_thread();
	stop_udp_bidirect_thread();
	stop_tcp_thread();

#ifdef CONFIG_PROC_FS
	kspeedtest_proc_exit();
#endif

	printk(KERN_INFO MODULE_NAME": module unloaded\n");
}

#ifdef CONFIG_PROC_FS

/* -------------------------------------------------------------------- */

static int stats_show(struct seq_file *m, void *v)
{
	kspeedtest_show_stats((my_fprintf *)seq_printf, m);
	return 0;
}

static int stats_show_open(struct inode *inode, struct file *file)
{
	    return single_open(file, stats_show, PDE(inode)->data);
}

static const struct file_operations stats_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
	.owner   = THIS_MODULE,
#endif
	.open    = stats_show_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

static int connections_show(struct seq_file *m, void *v)
{
	if (tcp_thread && tcp_thread->running) {
    	struct socket_list *sl;
		int tcpclients = 0;
    	for(sl = tcp_thread->clients; sl; sl = sl->next) 
		   tcpclients++;

		seq_printf(m, "TCP %sconnected %d\n",
				tcpclients > 0 ? "" : "dis",
				kspeedtest.tcp_port ? kspeedtest.tcp_port : DEFAULT_TCP_PORT);
	} else {
		seq_printf(m, "TCP off\n");
	}
	if (udp_thread && udp_thread->running) {
		seq_printf(m, "UDP %s %d\n",
				kspeedtest.udp_received == 1 ? "connected" : "disconnected",
				kspeedtest.udp_port ? kspeedtest.udp_port : DEFAULT_UDP_PORT);
	} else {
		seq_printf(m, "UDP off\n");
	}
	if (udp_bidirect_thread && udp_bidirect_thread->running) {
		seq_printf(m, "UDP bidirect %s %d\n", 
			  kspeedtest.udp_bidirect_received == 1 
			  ? "connected" : "disconnected",
			  kspeedtest.udp_bidirect_port 
			  ? kspeedtest.udp_bidirect_port : DEFAULT_UDP_BIDIRECT_PORT);
	} else {
		seq_printf(m, "UDP bidirect off\n");
	}
	return 0;
}

static int connections_show_open(struct inode *inode, struct file *file)
{
	    return single_open(file, connections_show, PDE(inode)->data);
}

static const struct file_operations connections_show_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
	.owner   = THIS_MODULE,
#endif
	.open    = connections_show_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};


static int kspeedtest_write_cmds (struct file *file, const char *buffer,
                                  unsigned long count, void *data)
{
	char cmd[100];
	char *argv[10];
	int  argc = 0;
	char *ptr_cmd;
	char *delimitters = " \n\t";
	char *ptr_next_tok;

	if (count > 100) count = 100;

	memset((void *)&cmd[0], 0, sizeof(cmd));
	memset((void *)&argv[0], 0, sizeof(argv));

	if (copy_from_user(&cmd, buffer, count))
		return -EFAULT;

	ptr_next_tok = &cmd[0];
	ptr_cmd = strsep(&ptr_next_tok, delimitters);
	if (ptr_cmd == NULL) return -1;

	do 
	{
		argv[argc++] = ptr_cmd;

		if (argc >= 10) {
			printk(KERN_ERR MODULE_NAME": too many parameters. dropping cmd.\n");
			return -EIO;
		}

		ptr_cmd = strsep(&ptr_next_tok, delimitters);
	} while (ptr_cmd != NULL);

	argc--;

	if (strcmp(argv[0], "start") == 0) {
	   if (argv[1] && strcmp(argv[1], "tcp") == 0) {
		   start_tcp_thread();
		   kspeedtest.tcp_port = simple_strtoul(argv[2], 0, 10);;
	   } else if (argv[1] && strcmp(argv[1], "udp") == 0) {
		   kspeedtest.udp_port = simple_strtoul(argv[2], 0, 10);;
		   start_udp_thread();
	   } else if (argv[1] && strcmp(argv[1], "udp_bidirect") == 0) {
           kspeedtest.udp_bidirect_port = simple_strtoul(argv[2], 0, 10);
		   start_udp_bidirect_thread();
	   } else {
		   printk(KERN_ERR MODULE_NAME": command wrong or incomplete (usage example: 'start tcp 4711'\n");
	   }
	} else if (strcmp(argv[0], "stop") == 0) {
	   if (argv[1] && strcmp(argv[1], "tcp") == 0) {
		   stop_tcp_thread();
	   } else if (argv[1] && strcmp(argv[1], "udp") == 0) {
		   stop_udp_thread();
	   } else if (argv[1] && strcmp(argv[1], "udp_bidirect") == 0) {
		   stop_udp_bidirect_thread();
	   } else if (argv[1] && strcmp(argv[1], "all") == 0) {
		   stop_tcp_thread();
		   stop_udp_thread();
		   stop_udp_bidirect_thread();
	   } else {
		   printk(KERN_ERR MODULE_NAME": command wrong or incomplete (usage example: 'stop tcp'\n");
	   }
	} else if (strcmp(argv[0], "sleepdelay") == 0) {
		kspeedtest.sleepdelay = simple_strtoul(argv[1], 0, 10);;
	} else if (strcmp(argv[0], "verbose") == 0) {
	    int level = 1; 
	    if (argv[1] && *argv[1]) level = simple_strtoul(argv[1], 0, 10);
		kspeedtest.verbose = level;
		printk(KERN_ERR MODULE_NAME": verbose %d \n", level);
	} else if (strcmp(argv[0], "noverbose") == 0) {
		kspeedtest.verbose = 0;
	} else if (strcmp(argv[0], "reset") == 0) {
	    reset_stats();
	} else {
		printk(KERN_ERR MODULE_NAME": wrong command\n");
	}
	return count;
}

static struct proc_dir_entry *dir_entry = 0;

static void __init kspeedtest_proc_init(void)
{
	struct proc_dir_entry *file_entry;

	dir_entry = proc_net_mkdir(&init_net, "kspeedtest", init_net.proc_net);
	if (dir_entry) {
		dir_entry->read_proc = 0;
		dir_entry->write_proc = 0;
	}

	file_entry = create_proc_entry("control", S_IFREG|S_IWUSR, dir_entry);
	if (file_entry) {
		file_entry->data       = NULL;
		file_entry->read_proc  = NULL;
        file_entry->write_proc = kspeedtest_write_cmds;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
		file_entry->owner      = THIS_MODULE;
#endif
	}
	file_entry = proc_create("stats", S_IRUGO, dir_entry, &stats_show_fops);
	file_entry = proc_create("connections", S_IRUGO, dir_entry, 
			&connections_show_fops);
}

static void __init kspeedtest_proc_exit(void)
{
	remove_proc_entry("control", dir_entry);
	remove_proc_entry("stats", dir_entry);
	remove_proc_entry("connections", dir_entry);
	remove_proc_entry("kspeedtest", init_net.proc_net);
}

#endif

/* -------------------------------------------------------------------- */

/* init and cleanup functions */
module_init(kspeedtest_init);
module_exit(kspeedtest_exit);

/* module information */
MODULE_DESCRIPTION("kernel speedtest");

/* -------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

#undef unix
struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
// .name = __stringify(KBUILD_MODNAME),
 .name = "kspeedtest",
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
};


