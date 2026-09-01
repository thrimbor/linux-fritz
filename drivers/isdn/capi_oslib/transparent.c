#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/net.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <net/tcp.h>
#include <net/ip.h>
#include <linux/inet.h>
#include <net/protocol.h>
#include <net/sock.h>
#include <linux/sched.h>
#include <linux/version.h>

#include "transparent.h"

/*--- #define TRANSP_DEBUG ---*/
/*--- #define CA_PRINTF printk ---*/

struct socket *MainSocket=NULL;
int pid;

static volatile int txin_count, txout_count;
static unsigned char txbuffer[2048];
static volatile int	close_transp = 1;
static volatile int	exit_transp = 0;
static DECLARE_WAIT_QUEUE_HEAD(tx_wait);
static DECLARE_COMPLETION(exit_sema);
static void (*CAP_DataInd)(const unsigned char *, int) = NULL;

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void Flush(struct socket *sock)
{
	struct msghdr		msg;
	struct iovec		iov;
	int			len;

	mm_segment_t		oldfs;
	
	
	if (sock->sk==NULL)
		return;

	len = 1;
		
	while (len>0)
	{
		char		Buffer[128];
		msg.msg_name     = 0;
		msg.msg_namelen  = 0;
		msg.msg_iov	 = &iov;
		msg.msg_iovlen   = 1;
		msg.msg_control  = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags    = MSG_DONTWAIT;
	
		msg.msg_iov->iov_base = &Buffer[0];
		msg.msg_iov->iov_len  = (__kernel_size_t)128;
	
		len = 0;
		oldfs = get_fs(); set_fs(KERNEL_DS);
		len = sock_recvmsg(sock,&msg,128,MSG_DONTWAIT);
		set_fs(oldfs);
	}
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static void Read(struct socket *sock, unsigned char* buffer, unsigned *length, int wait)
{
	struct msghdr		msg;
	struct iovec		iov;

	mm_segment_t		oldfs;
	
	
	if (sock->sk==NULL) {
		*length = 0;
		return;
	}

    iov.iov_base = (void *)buffer;
    iov.iov_len = *length;
    msg.msg_name     = 0;
    msg.msg_namelen  = 0;
    msg.msg_iov	 = &iov;
    msg.msg_iovlen   = 1;
    msg.msg_control  = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags    = MSG_DONTWAIT;

    oldfs = get_fs(); set_fs(KERNEL_DS);
    *length = sock_recvmsg(sock,&msg,*length,wait ? 0 : MSG_DONTWAIT);
    set_fs(oldfs);
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static unsigned int Write(struct socket *sock, unsigned char* buffer, unsigned length, int wait) {
	struct msghdr		msg;
	struct iovec		iov;
	
	mm_segment_t oldfs;

	if (sock->sk==NULL) {
		return 0;
	}

    iov.iov_base = (void *)buffer;
    iov.iov_len = length;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
	msg.msg_flags    = wait ? 0 : MSG_DONTWAIT;

    oldfs = get_fs(); set_fs(KERNEL_DS);
    length = sock_sendmsg(sock, &msg, length);
    set_fs(oldfs);
    return length;
}

/*-------------------------------------------------------------------------------------*\
 * may block while connecting...
\*-------------------------------------------------------------------------------------*/
static int E1Tx_Transparent(void *unused __attribute__((unused)))
{
	
	int len;
	char buf[128];
    struct sockaddr_in addr;
    int transp_open; 

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	daemonize();
	sprintf(current->comm,"capitransp");
#else
	daemonize("capitransp");
#endif

	transp_open = 0;

	for (;(exit_transp == 0);) {


#if 0
		if (signal_pending (current)) {
			sigset_t these;
			siginfo_t info;
			int signr;

			spin_lock_irq(&current->sigmask_lock);
			signr = dequeue_signal(&these, &info);
			spin_unlock_irq(&current->sigmask_lock);
			switch (signr) {
			case SIGKILL: 
#if defined(TRANSP_DEBUG)
				CA_PRINTF("E1Tx_Transparent(0): SIGKILL\n");
#endif
				printk("E1Tx_Transparent(0): SIGKILL\n");
				exit_transp = 1;
				break;
			case SIGTERM :
#if defined(TRANSP_DEBUG)
				CA_PRINTF("E1Tx_Transparent(0): SIGTERM\n");
#endif
				printk("E1Tx_Transparent(0): SIGTERM\n");
				exit_transp = 1;
				break;
			}

			printk("E1Tx_Transparent(0): SIG%d\n",signr);
			continue;
		}
#endif

        if (transp_open == 0) interruptible_sleep_on_timeout (&tx_wait, (HZ*10)); /*--- Verbindunsaufbau nur alle 10s ---*/
        else transp_open = 0;

		if (sock_create(PF_INET,SOCK_STREAM,IPPROTO_TCP,&MainSocket)<0)
		{
#if defined(TRANSP_DEBUG)
			CA_PRINTF("E1Tx_Transparent(): Error during creation of socket\n");
#endif
			MainSocket = NULL;
			continue;
		}
		/*--- Connect to 127.0.0.1 port 1011 ---*/
		addr.sin_family = AF_INET;
		addr.sin_port = htons((unsigned short)1011);
		addr.sin_addr.s_addr = htonl(0x7F000001);
    
		if (MainSocket->ops->connect(MainSocket, (struct sockaddr*)&addr, sizeof(addr), O_RDWR)) {
#if defined(TRANSP_DEBUG)
			CA_PRINTF("E1Tx_Transparent(): connect error\n");
#endif
			sock_release(MainSocket);
			MainSocket = NULL;
			continue;
		}
#if defined(TRANSP_DEBUG)
		CA_PRINTF("E1Tx_Transparent(): connect done\n");
#endif
    
		for (;;)
		{
#if 0
			if (signal_pending (current)) {
				sigset_t these;
				siginfo_t info;
				int signr;

				spin_lock_irq(&current->sigmask_lock);
				signr = dequeue_signal(&these, &info);
				spin_unlock_irq(&current->sigmask_lock);
				switch (signr) {
				case SIGKILL: 
#if defined(TRANSP_DEBUG)
					CA_PRINTF("E1Tx_Transparent(1): SIGKILL\n");
#endif
					printk("E1Tx_Transparent(1): SIGKILL\n");
					exit_transp = 1;
					break;
				case SIGTERM :
#if defined(TRANSP_DEBUG)
					CA_PRINTF("E1Tx_Transparent(1): SIGTERM\n");
#endif
					printk("E1Tx_Transparent(1): SIGTERM\n");
					exit_transp = 1;
					break;
				}
			}
#endif
			/* If the connection is lost, remove from queue */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
			if ((MainSocket->sk->state != TCP_ESTABLISHED
				 /*&& MainSocket->sk->state != TCP_CLOSE_WAIT */) ||
				(exit_transp) || ((transp_open) && (close_transp))
    			/*--- || signal_pending (current) ---*/
				)
#else
			if ((MainSocket->sk->sk_state != TCP_ESTABLISHED
				 /*&& MainSocket->sk->sk_state != TCP_CLOSE_WAIT */) ||
				(exit_transp) || ((transp_open) && (close_transp))
    			/*--- || signal_pending (current) ---*/
				)
#endif
			{
#if defined(TRANSP_DEBUG)
				CA_PRINTF("E1Tx_Transparent(): socket %s\n",(transp_open) ? "close" : "lost");
#endif
				/* Close the socket ....*/
				if ((MainSocket!=NULL)&&(MainSocket->sk!=NULL))
				{
#if defined(TRANSP_DEBUG)
					CA_PRINTF("E1Tx_Transparent(): Flush\n");
#endif
					Flush(MainSocket);
					sock_release(MainSocket);
				}
				MainSocket = NULL;
				break;
			}

			if (close_transp) {
		        interruptible_sleep_on_timeout (&tx_wait, (HZ*10)); /*--- Keepalivetest alle 10s ---*/
#if defined(TRANSP_DEBUG)
                /*--- CA_PRINTF("E1Tx_Transparent(): wakup2\n"); ---*/
#endif
				continue;
			}

			transp_open = 1;

			len = (txin_count - txout_count);
			if (len != 0) {
				if (len < 0) {	
					len = (sizeof(txbuffer) - txout_count);
				}

				if (((int)Write(MainSocket, &txbuffer[txout_count], len, 0)) > 0) {
					if (txout_count >= (int)(sizeof(txbuffer) - len)) txout_count = 0;
					else txout_count += len;
#if defined(TRANSP_DEBUG)
					CA_PRINTF("E1Tx_Transparent(): %d bytes sent\n", len);
#endif	
				} else{	
#if defined(TRANSP_DEBUG)
					CA_PRINTF("E1Tx_Transparent(): %d bytes not sent\n", len);
#endif
		        	interruptible_sleep_on_timeout (&tx_wait, (HZ/100)); /*--- Aufwachen nach 10ms ---*/
				}
			} else {
				interruptible_sleep_on_timeout (&tx_wait, (HZ/100)); /*--- Aufwachen nach 10ms ---*/
			}
			len = 128;
			Read(MainSocket, buf, &len, 0);
			if (len > 0) {
#if defined(TRANSP_DEBUG)
				CA_PRINTF("E1Tx_Transparent(): got %d bytes\n", len);
#endif
            	/*--- local_irq_save(flags); ---*/
				local_bh_disable();
				if (CAP_DataInd != NULL) CAP_DataInd(buf, len);
				local_bh_enable();
            	/*--- local_irq_restore(flags); ---*/
			}
		}
    }
#if defined(TRANSP_DEBUG)
	CA_PRINTF("E1Tx_Transparent(): Close done!\n");
#endif
    complete_and_exit(&exit_sema, 0);
	return 0;
} 
EXPORT_SYMBOL(E1Tx_Transparent);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int E1Tx_OpenTransparent(void) {

	if (close_transp == 0) {
#if defined(TRANSP_DEBUG)
    	CA_PRINTF("E1Tx_OpenTransparent(): Error already open\n");
#endif
		return 1;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	if ((MainSocket == NULL) || (MainSocket->sk==NULL) || (MainSocket->sk->state != TCP_ESTABLISHED)) {
#else
	if ((MainSocket == NULL) || (MainSocket->sk==NULL) || (MainSocket->sk->sk_state != TCP_ESTABLISHED)) {
#endif
#if defined(TRANSP_DEBUG)
    	CA_PRINTF("E1Tx_OpenTransparent(): Error no socket open\n");
#endif
		wake_up_interruptible (&tx_wait);
		return 1;
	}

	txin_count = 0;
	txout_count = 0;
	close_transp = 0;

#if defined(TRANSP_DEBUG)
	CA_PRINTF("E1Tx_OpenTransparent(): Open start\n");
#endif
	return 0;
}
EXPORT_SYMBOL(E1Tx_OpenTransparent);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int E1Tx_CloseTransparent(void) {
	if (close_transp == 0) {
#if defined(TRANSP_DEBUG)
		CA_PRINTF("E1Tx_CloseTransparent(): Start close!\n");
#endif
		close_transp = 1;
		wake_up_interruptible (&tx_wait);
    }
#if defined(TRANSP_DEBUG)
	else
		CA_PRINTF("E1Tx_CloseTransparent(): Not open!\n");
#endif
    return 0;
}
EXPORT_SYMBOL(E1Tx_CloseTransparent);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int E1Tx_SendTransparent(unsigned char *Buffer, unsigned int BufferLength) {
	int len;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	if ((close_transp) || (MainSocket == NULL) || (MainSocket->sk==NULL) || (MainSocket->sk->state != TCP_ESTABLISHED)) {
#else
	if ((close_transp) || (MainSocket == NULL) || (MainSocket->sk==NULL) || (MainSocket->sk->sk_state != TCP_ESTABLISHED)) {
#endif
#if defined(TRANSP_DEBUG)
		CA_PRINTF("E1Tx_SendTransparent(): MainSocket == NULL (%u bytes)\n", BufferLength);
#endif
		wake_up_interruptible (&tx_wait);
		return 1;
	}
	if (BufferLength >= sizeof(txbuffer)) {
#if defined(TRANSP_DEBUG)
		CA_PRINTF("E1Tx_SendTransparent(): too much bytes (%u)\n", BufferLength);
#endif
		return 1;
	}
	len = txin_count;
	if ((len + BufferLength) < sizeof(txbuffer)) {
		memcpy(&txbuffer[len], Buffer, BufferLength);
#if defined(TRANSP_DEBUG)
		if ((txin_count < txout_count) && ((len + BufferLength) >= txout_count))
			CA_PRINTF("E1Tx_SendTransparent(): Overrun\n");
#endif
		txin_count = (len + BufferLength);
	} else {
		int len1 = sizeof(txbuffer) - len; 
		memcpy(&txbuffer[len], Buffer, len1);
#if defined(TRANSP_DEBUG)
		if (len < txout_count)
			CA_PRINTF("E1Tx_SendTransparent(): Overrun\n");
#endif
		len = BufferLength - len1;
		memcpy(&txbuffer[0], &Buffer[len1], len);
#if defined(TRANSP_DEBUG)
		if (len >= txout_count)
			CA_PRINTF("E1Tx_SendTransparent(): Overrun\n");
#endif
		txin_count = len;
	}
	wake_up_interruptible (&tx_wait);

	return 0;
}
EXPORT_SYMBOL(E1Tx_SendTransparent);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
int Transparent_Init(void (*DataInd)(const unsigned char *, int)) {

	txin_count = 0;
	txout_count = 0;
	exit_transp = 0;

	CAP_DataInd = DataInd;
	init_waitqueue_head (&tx_wait);

	pid = kernel_thread(E1Tx_Transparent, NULL, 0);
	if (pid < 0) {
#if defined(TRANSP_DEBUG)
        CA_PRINTF("Transparent_Init(): Error during kernel thread creation = %d\n", pid);
#endif
        CAP_DataInd = NULL;
        return 1;
	}
	return 0; 
}
EXPORT_SYMBOL(Transparent_Init);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void Transparent_Deinit(void) {

#if defined(TRANSP_DEBUG)
	CA_PRINTF("Transparent_Deinit(): Start \n");
#endif
    if(CAP_DataInd == NULL) {
        return;
    }
	close_transp = 1;
	exit_transp = 1;
	wake_up_interruptible (&tx_wait);

    wait_for_completion(&exit_sema);
	CAP_DataInd = NULL;
}
EXPORT_SYMBOL(Transparent_Deinit);
