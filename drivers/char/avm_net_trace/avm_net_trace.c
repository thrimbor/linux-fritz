
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/skbuff.h>
#include <linux/avm_net_trace.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <asm/mach_avm.h>

struct avm_net_trace avm_net_trace[MAX_AVM_NET_TRACE_DEVICES];

int __avm_net_trace_func (
		struct avm_net_trace *ant, struct sk_buff *skb, 
		int skb_property, int direction) {

	if ((direction != AVM_NET_TRACE_DIRECTION_OUTGOING) && 
	    (direction != AVM_NET_TRACE_DIRECTION_HOST))
		return -1;

	switch (skb_property) {
		case AVM_NET_TRACE_USE_SKB:
			break;
		case AVM_NET_TRACE_CLONE_SKB:
			skb = skb_clone (skb, GFP_ATOMIC);
			break;
		case AVM_NET_TRACE_COPY_SKB:
			skb = skb_copy (skb, GFP_ATOMIC);
			break;
		default:
			return -1;
	}

	if (skb == NULL)
		return -1;

	skb->pkt_type = direction == AVM_NET_TRACE_DIRECTION_OUTGOING ? 
		PACKET_OUTGOING : PACKET_HOST;
	skb->protocol = ant->ntd->pcap_protocol;
	skb_queue_tail (&ant->recvqueue, skb);

	if ((ant->rbuf_size > 0) && (skb_queue_len (&ant->recvqueue) > ant->rbuf_size)) {
		struct sk_buff * skb_del = skb_dequeue (&ant->recvqueue);
		if (NULL != skb_del) {
			kfree_skb (skb_del);
		}
	}

	wake_up_interruptible (&ant->recvwait);

	return 0;
}
EXPORT_SYMBOL(__avm_net_trace_func);

struct pcap_hdr_mgc {
	u32 magic;
	u16 version_major;
	u16 version_minor;
	u32 thiszone;
	u32 sigfigs;
	u32 snaplen;
	u32 network;
};

struct pcaprec_hdr {
	u32 ts_sec;
	u32 ts_usec;
	u32 incl_len;
	u32 orig_len;
};

struct pcaprec_modified_hdr {
	struct	pcaprec_hdr hdr;
	u32	ifindex;
	u16	protocol;
	u8	pkt_type;
	u8	pad;
};

static struct pcap_hdr_mgc pcap_hdr = {
	magic: 0xa1b2cd34,
	version_major: 2,
	version_minor: 4,
	thiszone: 0,
	sigfigs: 0,
	snaplen: 2048,
	network: 0
};

static long avm_net_device_ioctl(struct file *file,
                                           unsigned int cmd, unsigned long arg)
{
	DEFINE_SPINLOCK(lock);
	unsigned long flags;
	int ret = -EINVAL;

	switch (cmd)
	{
		case ANT_IOCTL_GET_DEVICES:
			{
				struct ioctl_ant_device_list liste;
				char *pos; /* pointer to position in userspace*/
				int len = -1;
				int entry_siz = sizeof(struct ioctl_ant_device);
				int total_siz = 0;

				memset(&liste, 0, sizeof(liste));

				if (copy_from_user(&liste, (void *)arg, sizeof(liste))) {
					return -EFAULT;
				} else {
					int i;
					len     = liste.buf_len; /* buffer length */
					pos     = liste.u_buf;   /* pointer to buffer */

					spin_lock_irqsave (&lock, flags);
					for (i = MAX_AVM_NET_TRACE_DEVICES - 1; i >= 0; i--) {

						if (len - entry_siz < 0) {
							spin_unlock_irqrestore (&lock, flags);
							return -ENOMEM;
						}

						if (avm_net_trace[i].ntd) {
							struct ioctl_ant_device dev;

							strncpy(dev.name, avm_net_trace[i].ntd->name, AVM_NET_TRACE_IFNAMSIZ);
							dev.name[AVM_NET_TRACE_IFNAMSIZ-1] = '\0';
							dev.minor = avm_net_trace[i].ntd->minor;
							dev.iface = avm_net_trace[i].ntd->iface;
							dev.type  = avm_net_trace[i].ntd->type;
							dev.is_open = avm_net_trace[i].is_open;

							/* copy in userspace */
							if (copy_to_user(pos, &dev, entry_siz)) {
								spin_unlock_irqrestore (&lock, flags);
								return -EFAULT;
							} else {
								len -= entry_siz;/* reduce len */
								pos += entry_siz; 
								total_siz += entry_siz; /* add to total size */
							}
						}
					}
					liste.buf_len = total_siz;
					spin_unlock_irqrestore (&lock, flags);

					if (copy_to_user((void *)arg, &liste, sizeof(liste))) {
						return -EFAULT;
					}

					ret = 0;
				}
			}
			break;
		default:
			break;
	}
	return ret;
}

/* process context */ 
static ssize_t avm_net_device_read (
		struct file *file, char *buf, size_t count, loff_t *ppos) {

	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
	struct avm_net_trace *ant = (struct avm_net_trace *) file->private_data;
	struct sk_buff *skb;
	char *to = buf;

	if (minor == 0)
		return 0;

	if ((skb = skb_dequeue (&ant->recvqueue)) == NULL) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		if (wait_event_interruptible (ant->recvwait, (skb = skb_dequeue(&ant->recvqueue))))
			return -ERESTARTNOHAND;
#if 0
		for (;;) {
			interruptible_sleep_on(&ant->recvwait);
			if ((skb = skb_dequeue(&ant->recvqueue)) != 0)
				break;
			if (signal_pending(current))
				break;
		}
		if (skb == 0)
			return -ERESTARTNOHAND;
#endif
	}

	if (!ant->got_header) {

		if (count < sizeof (pcap_hdr))
			return -EMSGSIZE;

		skb_queue_head (&ant->recvqueue, skb);

		if (ant->ntd->pcap_encap)
			pcap_hdr.network = ant->ntd->pcap_encap;
		else 
			pcap_hdr.network = skb->protocol;

		if (copy_to_user (buf, &pcap_hdr, sizeof (pcap_hdr)))
			return -EFAULT;

		ant->got_header = 1;

		return sizeof (pcap_hdr);
	}

	do {
		struct pcaprec_modified_hdr hdr;

		if (count < sizeof (hdr) + skb->len) {
			skb_queue_head(&ant->recvqueue, skb);
			if (to == buf)
				return -EMSGSIZE;
			break;
		}
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 18)
		{
			struct timeval stamp;
			skb_get_timestamp(skb, &stamp);
			hdr.hdr.ts_sec = stamp.tv_sec;
	  		hdr.hdr.ts_usec = stamp.tv_usec;
		}
#else
		hdr.hdr.ts_sec = skb->stamp.tv_sec;
		hdr.hdr.ts_usec = skb->stamp.tv_usec;
#endif
		hdr.hdr.incl_len = skb->len;
		hdr.hdr.orig_len = skb->len;
		hdr.ifindex = 0;
		hdr.protocol = skb->protocol;
		hdr.pkt_type = skb->pkt_type;
		hdr.pad = 0;

		if (copy_to_user(to, &hdr, sizeof (hdr))) {
			skb_queue_head (&ant->recvqueue, skb);
			return -EFAULT;
		}
		to += sizeof (hdr);
		count -= sizeof (hdr);
		if (copy_to_user (to, skb->data, skb->len)) {
			skb_queue_head (&ant->recvqueue, skb);
			return -EFAULT;
		}
		to += skb->len;
		count -= skb->len;

		kfree_skb (skb);

	} while ((skb = skb_dequeue (&ant->recvqueue)) != 0);

	return to - buf;
}

static int avm_net_device_open(struct inode *inode, struct file *file) {

	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
	struct avm_net_trace *ant = NULL;
	DEFINE_SPINLOCK(lock);
	unsigned long flags;

	if (minor == 0)
		return 0;

	if (file->private_data)
		return -EEXIST;

	if (minor >= MAX_AVM_NET_TRACE_DEVICES)
		return -ENXIO;

	spin_lock_irqsave (&lock, flags);

	ant = avm_net_trace + minor;

	if (ant->ntd == NULL) {
		spin_unlock_irqrestore (&lock, flags);
		return -ENXIO;
	}

	ant->is_open = 1;
	ant->do_trace = 1;
	ant->got_header = 0;
	ant->dropped = 0;

	file->private_data = (void *) ant;

	printk (KERN_DEBUG "Starting new trace on device '%s'.\n", ant->ntd->name);

	spin_unlock_irqrestore (&lock, flags);

	if (ant->ant_state_change != NULL) {
		ant->ant_state_change(ant->ntd, AVM_NET_TRACE_STATE_CHG_OPEN);
	}

	return 0;
}

static int avm_net_device_release(struct inode *inode, struct file *file) {

	unsigned int minor = MINOR (file->f_dentry->d_inode->i_rdev);
	struct avm_net_trace *ant = (struct avm_net_trace *) file->private_data;
	DEFINE_SPINLOCK(lock);
	unsigned long flags;

	if (minor == 0)
		return 0;

	if (ant->ant_state_change != NULL) {
		ant->ant_state_change(ant->ntd, AVM_NET_TRACE_STATE_CHG_RELEASE);
	}

	spin_lock_irqsave (&lock, flags);

	file->private_data = NULL;
	ant->is_open = 0;
	if (ant->rbuf_size <= 0) {
		ant->do_trace = 0;
	}

	printk (KERN_DEBUG "avm_net_trace: Stopping trace on device '%s' (%lu pakets dropped).\n", 
			ant->ntd->name, ant->dropped);

	spin_unlock_irqrestore (&lock, flags);

	return 0;
}

static unsigned int avm_net_device_poll(struct file *file, poll_table * wait) {

	struct avm_net_trace *ant = (struct avm_net_trace *) file->private_data;
	unsigned int mask = 0;

	poll_wait (file, &ant->recvwait, wait);

	if (skb_queue_len (&ant->recvqueue))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static struct file_operations avm_net_trace_fops = {
	owner:		THIS_MODULE,
	unlocked_ioctl:		avm_net_device_ioctl,
	read:		avm_net_device_read,
	open:		avm_net_device_open,
	release:	avm_net_device_release,
	poll:		avm_net_device_poll,
};


int register_avm_net_trace_device (struct avm_net_trace_device *ntd) {
	return register_avm_net_trace_device_permanent (ntd, -1);
}
EXPORT_SYMBOL(register_avm_net_trace_device);

int register_avm_net_trace_device_permanent (struct avm_net_trace_device *ntd, int rbuf_size) {

	int i;
	DEFINE_SPINLOCK(lock);
	unsigned long flags;
	struct avm_net_trace *ant = NULL;

	/* minor 0 not for a device - only for ioctl to get registered devices */
	if (ntd->minor == 0)
		return -1;

	spin_lock_irqsave (&lock, flags);

	for (i = 1; i < MAX_AVM_NET_TRACE_DEVICES; i++) {
		if (ntd == avm_net_trace[i].ntd) {
			spin_unlock_irqrestore (&lock, flags);
			return -EEXIST;
		}
	}
	
	if ((ntd->minor != AVM_NET_TRACE_DYNAMIC_MINOR) && (avm_net_trace[ntd->minor].ntd)) {
		spin_unlock_irqrestore (&lock, flags);
		return -EEXIST;
	}

	if (ntd->minor == AVM_NET_TRACE_DYNAMIC_MINOR) {
		for (i = MAX_AVM_NET_TRACE_DEVICES - 1; i >= 1; i--) { 
			if (avm_net_trace[i].ntd == NULL) {
				ant = avm_net_trace + i;
				ant->minor = i;
				ntd->minor = i;
				break;
			}
		}
	} else {
		ant = avm_net_trace + ntd->minor;
		ant->minor =  ntd->minor;
	}

	if (ant == NULL) {
		spin_unlock_irqrestore (&lock, flags);
		return -ENFILE;
	}

	skb_queue_head_init (&ant->recvqueue);
	init_waitqueue_head (&ant->recvwait);

	ant->ntd = ntd;
	ant->is_open = 0;
	ant->do_trace = (rbuf_size > 0);
	ant->ant_state_change = NULL;
	ant->rbuf_size = rbuf_size;
	ntd->ant = ant;

	printk (KERN_INFO "avm_net_trace: New net trace device '%s' registered with minor %d.\n",
			ntd->name, ant->minor);

	spin_unlock_irqrestore (&lock, flags);

	return 0;
}
EXPORT_SYMBOL(register_avm_net_trace_device_permanent);

void deregister_avm_net_trace_device (struct avm_net_trace_device *ntd) {

	DEFINE_SPINLOCK(lock);
	unsigned long flags;

	spin_lock_irqsave (&lock, flags);

	if (ntd->minor < 1 || ntd->minor >= MAX_AVM_NET_TRACE_DEVICES) {
		spin_unlock_irqrestore (&lock, flags);
		return;
	}

	if (avm_net_trace[ntd->minor].ntd != ntd) {
		spin_unlock_irqrestore (&lock, flags);
		return;
	}

	if ((NULL != ntd->ant) && (ntd->ant->rbuf_size > 0)) {
		skb_queue_purge(&ntd->ant->recvqueue);
	}

	avm_net_trace[ntd->minor].ntd = NULL;
	ntd->ant = NULL;

	spin_unlock_irqrestore (&lock, flags);
}
EXPORT_SYMBOL(deregister_avm_net_trace_device);

int register_avm_net_device_state_change_cb (struct avm_net_trace_device *ntd, void (*cb)(struct avm_net_trace_device *, int)) {

	spinlock_t lock = SPIN_LOCK_UNLOCKED;
	unsigned long flags;
	struct avm_net_trace *ant;

	spin_lock_irqsave (&lock, flags);

	ant = ntd->ant;

	if (ant == NULL) {
		spin_unlock_irqrestore (&lock, flags);
		return -EINVAL;
	}

	ntd->ant->ant_state_change = cb;

	spin_unlock_irqrestore (&lock, flags);

	return 0;
}
EXPORT_SYMBOL(register_avm_net_device_state_change_cb);

int deregister_avm_net_device_state_change_cb (struct avm_net_trace_device *ntd) {

	spinlock_t lock = SPIN_LOCK_UNLOCKED;
	unsigned long flags;
	struct avm_net_trace *ant;

	spin_lock_irqsave (&lock, flags);

	ant = ntd->ant;

	if (ant == NULL) {
		spin_unlock_irqrestore (&lock, flags);
		return -EINVAL;
	}

	ntd->ant->ant_state_change = NULL;

	spin_unlock_irqrestore (&lock, flags);

	return 0;
}
EXPORT_SYMBOL(deregister_avm_net_device_state_change_cb);

static int __init avm_net_trace_init (void) {

	int ret;

	if ((ret = register_chrdev (AVM_NET_TRACE_MAJOR, "avm_net_trace", &avm_net_trace_fops)) < 0) {
		printk (KERN_ERR "avm_net_trace: register_chrdev failed\n");
		return -EIO;
	}

	memset (&avm_net_trace, 0, sizeof (struct avm_net_trace));

	printk (KERN_INFO "avm_net_trace: Up and running.\n");

	return 0;
}

static void __exit avm_net_trace_exit (void) {

	int i;
	DEFINE_SPINLOCK(lock);
	unsigned long flags;

	spin_lock_irqsave (&lock, flags);

	for (i = 0; i< MAX_AVM_NET_TRACE_DEVICES; i++) {
		struct avm_net_trace *ant = avm_net_trace + i;

		skb_queue_purge(&ant->recvqueue);
		ant->ntd = NULL;
	}

	spin_unlock_irqrestore (&lock, flags);

	unregister_chrdev(AVM_NET_TRACE_MAJOR, "avm_net_device");
}

module_init (avm_net_trace_init);
module_exit (avm_net_trace_exit);

