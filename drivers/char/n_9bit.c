/*
 * 9 bit line discipline for Linux
 * A character received with mark parity denotes the start of a message.
 * A character received with space parity is continuing previous message.
 *
 * Copyright (C) 2011 Troy Kisky <troy.kisky@boundarydevices.com>
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#define DEBUG
#undef VERSION
#define VERSION(major,minor,patch) (((((major)<<8)+(minor))<<8)+(patch))

#include <linux/poll.h>
#include <linux/in.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/serial_core.h>
#include <linux/smp_lock.h>
#include <linux/string.h>	/* used in new tty drivers */
#include <linux/signal.h>	/* used in new tty drivers */
#include <linux/if.h>
#include <linux/bitops.h>

#include <asm/system.h>
#include <asm/termios.h>
#include <asm/uaccess.h>

#define _9BIT_MAGIC 0x35952121

/* max frame size for memory allocations */
static int maxframe = 4096;

/*
 * Buffers for individual 9 bit frames
 */
#define DEFAULT_RX_BUF_COUNT 10
#define MAX_RX_BUF_COUNT 60
#define DEFAULT_TX_BUF_COUNT 3

struct n_9bit_buf {
	struct n_9bit_buf *link;
	int		  count;
	char		  buf[0];
};

#define	N_9BIT_BUF_SIZE	(sizeof(struct n_9bit_buf) + maxframe)

struct n_9bit_buf_list {
	struct n_9bit_buf *head;
	struct n_9bit_buf *tail;
	int		  count;
	spinlock_t	  spinlock;
};

/*
 * struct n_9bit - per device instance data structure
 * @magic - magic value for structure
 * @flags - miscellaneous control flags
 * @tty - ptr to TTY structure
 * @tbusy - reentrancy flag for tx wakeup code
 * @tx_buf_list - list of pending transmit frame buffers
 * @rx_buf_list - list of received frame buffers
 * @tx_free_buf_list - list unused transmit frame buffers
 * @rx_free_buf_list - list unused received frame buffers
 */
struct n_9bit {
	int			magic;
	__u32			flags;
	struct tty_struct	*tty;
	int			tbusy;
	unsigned		rx_timeout_msec;
	/* bitmap of addresses to receive, default all */
	unsigned long		rx_map[256/32];
	struct timer_list	rx_eom_timer;
	struct n_9bit_buf_list	rx_fill_list;
	struct n_9bit_buf_list	tx_buf_list;
	struct n_9bit_buf_list	rx_buf_list;
	struct n_9bit_buf_list	tx_free_buf_list;
	struct n_9bit_buf_list	rx_free_buf_list;
};


/* TTY callbacks */
#define bset(p,b)	((p)[(b) >> 5] |= (1 << ((b) & 0x1f)))

struct n_9bit *tty2n_9bit(struct tty_struct *tty)
{
	struct n_9bit *n_9bit = (struct n_9bit *)tty->disc_data;
	if (n_9bit) {
		if (tty != n_9bit->tty) {
			pr_err("%s: tty mismatch\n", __func__);
			n_9bit = NULL;
		} else {
			/* verify line is using 9BIT discipline */
			if (n_9bit->magic != _9BIT_MAGIC) {
				pr_err("%s: line not using 9BIT discipline\n", __func__);
				n_9bit = NULL;
			}
		}
	} else {
		pr_err("%s: disc_data is NULL\n", __func__);
	}
	return n_9bit;
}
/*
 * add specified 9BIT buffer to tail of specified list
 * @list - pointer to buffer list
 * @buf	- pointer to buffer
 */
static void n_9bit_buf_put(struct n_9bit_buf_list *list,
		struct n_9bit_buf *buf)
{
	unsigned long flags;
	spin_lock_irqsave(&list->spinlock,flags);

	buf->link=NULL;
	if (list->tail)
		list->tail->link = buf;
	else
		list->head = buf;
	list->tail = buf;
	list->count++;

	spin_unlock_irqrestore(&list->spinlock,flags);

}

static struct n_9bit_buf* n_9bit_buf_put_head(struct n_9bit_buf_list *list,
		struct n_9bit_buf *buf)
{
	unsigned long flags;
	spin_lock_irqsave(&list->spinlock,flags);

	buf->link = list->head;
	list->head = buf;
	if (!list->tail)
		list->tail = buf;
	list->count++;

	spin_unlock_irqrestore(&list->spinlock,flags);
	return buf;

}
/*
 * Remove and return an 9BIT buffer from list
 * @list - pointer to 9BIT buffer list
 *
 * Remove and return an 9BIT buffer from the head of the specified 9BIT buffer
 * list.
 * Returns a pointer to 9BIT buffer if available, otherwise %NULL.
 */
static struct n_9bit_buf* n_9bit_buf_get(struct n_9bit_buf_list *list)
{
	unsigned long flags;
	struct n_9bit_buf *buf;
	spin_lock_irqsave(&list->spinlock,flags);

	buf = list->head;
	if (buf) {
		list->head = buf->link;
		list->count--;
	}
	if (!list->head)
		list->tail = NULL;

	spin_unlock_irqrestore(&list->spinlock,flags);
	return buf;

}

static void flush_rx_queue(struct tty_struct *tty)
{
	struct n_9bit *n_9bit = tty2n_9bit(tty);
	struct n_9bit_buf *buf;

	if (n_9bit) {
		while ((buf = n_9bit_buf_get(&n_9bit->rx_buf_list)))
			n_9bit_buf_put(&n_9bit->rx_free_buf_list, buf);
		while ((buf = n_9bit_buf_get(&n_9bit->rx_fill_list)))
			n_9bit_buf_put(&n_9bit->rx_free_buf_list, buf);
	}
}

static void flush_tx_queue(struct tty_struct *tty)
{
	struct n_9bit *n_9bit = tty2n_9bit(tty);
	struct n_9bit_buf *buf;

	if (n_9bit) {
		while ((buf = n_9bit_buf_get(&n_9bit->tx_buf_list)))
			n_9bit_buf_put(&n_9bit->tx_free_buf_list, buf);
	}
}

void free_list(struct n_9bit_buf_list *list)
{
	for(;;) {
		struct n_9bit_buf *buf = n_9bit_buf_get(list);
		if (buf) {
			kfree(buf);
		} else
			break;
	}
}

/*
 * n_9bit_release - release an n_9bit per device line discipline info structure
 * @n_9bit - per device line discipline info structure
 */
static void n_9bit_release(struct n_9bit *n_9bit)
{
	struct tty_struct *tty = n_9bit->tty;

	pr_debug("%s: called\n", __func__);

	if (tty->disc_data == n_9bit)
		tty->disc_data = NULL;	/* Break the tty->n_9bit link */

	/* Ensure that the n_9bitd process is not hanging on select()/poll() */
	wake_up_interruptible(&tty->read_wait);
	wake_up_interruptible(&tty->write_wait);
	del_timer(&n_9bit->rx_eom_timer);

	/* Release transmit and receive buffers */
	free_list(&n_9bit->rx_free_buf_list);
	free_list(&n_9bit->tx_free_buf_list);
	free_list(&n_9bit->rx_buf_list);
	free_list(&n_9bit->tx_buf_list);
	free_list(&n_9bit->rx_fill_list);
	kfree(n_9bit);
}

/*
 * Line discipline close
 * @tty - pointer to tty info structure
 *
 * Called when the line discipline is changed to something
 * else, the tty is closed, or the tty detects a hangup.
 */
static void n_9bit_tty_close(struct tty_struct *tty)
{
	struct n_9bit *n_9bit = tty2n_9bit(tty);

	pr_debug("%s: called\n", __func__);

	if (n_9bit) {
#if defined(TTY_NO_WRITE_SPLIT)
		clear_bit(TTY_NO_WRITE_SPLIT,&tty->flags);
#endif
		tty->disc_data = NULL;
		n_9bit_release(n_9bit);
	}
	pr_debug("%s: success\n", __func__);
}

/*
 * initialize specified 9BIT buffer list
 * @list - pointer to buffer list
 */
static void n_9bit_buf_list_init(struct n_9bit_buf_list *list)
{
	memset(list, 0, sizeof(*list));
	spin_lock_init(&list->spinlock);
}

static void eom_timer(unsigned long data)
{
	struct n_9bit_buf *rbuf;
	struct tty_struct *tty = (struct tty_struct *)data;
	struct n_9bit *n_9bit = tty2n_9bit(tty);

	pr_debug("%s: called\n", __func__);

	if (!n_9bit)
		return;
	del_timer(&n_9bit->rx_eom_timer);
	rbuf = n_9bit_buf_get(&n_9bit->rx_fill_list);
	if (rbuf) {
		n_9bit_buf_put(&n_9bit->rx_buf_list, rbuf);
		wake_up_interruptible(&tty->read_wait);
		if (n_9bit->tty->fasync != NULL)
			kill_fasync(&n_9bit->tty->fasync, SIGIO, POLL_IN);
	}
}

/*
 * Allocate an n_9bit instance data structure
 *
 * Returns a pointer to newly created structure if success, otherwise %NULL
 */
static struct n_9bit *n_9bit_alloc(struct tty_struct *tty)
{
	struct n_9bit_buf *buf;
	int i;
	struct n_9bit *n_9bit = kmalloc(sizeof(*n_9bit), GFP_KERNEL);

	if (!n_9bit)
		return NULL;

	memset(n_9bit, 0, sizeof(*n_9bit));
	memset(&n_9bit->rx_map, 0xff, sizeof(n_9bit->rx_map));
	/* 3 millisecond gap denotes end of message */
	n_9bit->rx_timeout_msec = 3;
	n_9bit_buf_list_init(&n_9bit->rx_free_buf_list);
	n_9bit_buf_list_init(&n_9bit->tx_free_buf_list);
	n_9bit_buf_list_init(&n_9bit->rx_buf_list);
	n_9bit_buf_list_init(&n_9bit->tx_buf_list);
	n_9bit_buf_list_init(&n_9bit->rx_fill_list);
	init_timer(&n_9bit->rx_eom_timer);
	n_9bit->rx_eom_timer.data = (unsigned long)tty;
	n_9bit->rx_eom_timer.function = eom_timer;

	/* allocate free rx buffer list */
	for (i = 0; i < DEFAULT_RX_BUF_COUNT; i++) {
		buf = kmalloc(N_9BIT_BUF_SIZE, GFP_KERNEL);
		if (buf) {
			n_9bit_buf_put(&n_9bit->rx_free_buf_list,buf);
		} else {
			pr_err("%s: kalloc() failed for rx buffer %d\n",
					__func__, i);
			break;
		}
	}

	/* allocate free tx buffer list */
	for (i = 0; i < DEFAULT_TX_BUF_COUNT; i++) {
		buf = kmalloc(N_9BIT_BUF_SIZE, GFP_KERNEL);
		if (buf) {
			n_9bit_buf_put(&n_9bit->tx_free_buf_list,buf);
		} else {
			pr_err("%s:, kalloc() failed for tx buffer %d\n",
					__func__, i);
			break;
		}
	}

	/* Initialize the control block */
	n_9bit->magic  = _9BIT_MAGIC;
	n_9bit->flags  = 0;
	tty->disc_data = n_9bit;
	n_9bit->tty    = tty;
	return n_9bit;
}

/*
 * Called when line discipline changed to n_9bit
 * @tty - pointer to tty info structure
 *
 * Returns 0 if success, otherwise error code
 */
static int n_9bit_tty_open(struct tty_struct *tty)
{
	struct n_9bit *n_9bit = (struct n_9bit *)tty->disc_data;

	pr_debug("%s: called(device=%s)\n", __func__, tty->name);

	/* There should not be an existing table for this slot. */
	if (n_9bit) {
		pr_err("%s: tty already associated!\n", __func__);
		return -EEXIST;
	}

	n_9bit = n_9bit_alloc(tty);
	if (!n_9bit) {
		pr_err("n_9bit_alloc failed\n");
		return -ENFILE;
	}
	tty->receive_room = 65536;

#if defined(TTY_NO_WRITE_SPLIT)
	/* change tty_io write() to not split large writes into 8K chunks */
	set_bit(TTY_NO_WRITE_SPLIT,&tty->flags);
#endif

	/* flush receive data from driver */
	tty_driver_flush_buffer(tty);
	pr_debug("%s: success\n", __func__);
	return 0;
}

/*
 * n_9bit_send_frames - send frames on pending send buffer list
 * @n_9bit - pointer to ldisc instance data
 * @tty - pointer to tty instance data
 *
 * Send frames on pending send buffer list until the driver does not accept a
 * frame (busy) this function is called after adding a frame to the send buffer
 * list and by the tty wakeup callback.
 */
static void n_9bit_send_frames(struct n_9bit *n_9bit, struct tty_struct *tty)
{
	int actual;
	unsigned long flags;
	int again;

	pr_debug("%s: called\n", __func__);
 check_again:

	spin_lock_irqsave(&n_9bit->tx_buf_list.spinlock, flags);
	if (n_9bit->tbusy) {
		n_9bit->tbusy = 2;
		spin_unlock_irqrestore(&n_9bit->tx_buf_list.spinlock, flags);
		return;
	}
	n_9bit->tbusy = 1;
	spin_unlock_irqrestore(&n_9bit->tx_buf_list.spinlock, flags);

	/* get current transmit buffer or get new transmit */
	/* buffer from list of pending transmit buffers */


	for (;;) {
		struct n_9bit_buf *tbuf;
		/*
		 * wait for queue to empty before starting new message
		 */
		set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
		if (tty_chars_in_buffer(tty))
			break;
		tbuf = n_9bit_buf_get(&n_9bit->tx_buf_list);
		if (!tbuf) {
			clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
			break;
		} else {
			struct uart_state *state = tty->driver_data;
			struct uart_port *uport = state->uart_port;
			int ret = -ENOIOCTLCMD;
			if (uport->ops->ioctl) {
				ret = uport->ops->ioctl(uport, TIOC_FORCE_TX_PARITY_ERROR, 0);
				if (ret)
					pr_err("TIOC_FORCE_TX_PARITY_ERROR not supported by driver\n");
			}
			pr_debug("%s: sending frame %p, count=%d ret=%d\n",
					__func__, tbuf, tbuf->count, ret);
		}

		/* Send the next block of data to device */
		actual = tty->ops->write(tty, tbuf->buf, tbuf->count);

		if (actual == -ERESTARTSYS) {
			/* rollback was possible and has been done */
			n_9bit_buf_put_head(&n_9bit->tx_buf_list, tbuf);
			break;
		}
		/* if transmit error, throw frame away by */
		/* pretending it was accepted by driver */
		if (actual < 0)
			actual = tbuf->count;

		if (actual == tbuf->count) {
			pr_debug("%s: frame %p completed\n",
					__func__, tbuf);

			/* free current transmit buffer */
			n_9bit_buf_put(&n_9bit->tx_free_buf_list, tbuf);

			/* wait up sleeping writers */
			wake_up_interruptible(&tty->write_wait);

			/* get next pending transmit buffer */
			tbuf = n_9bit_buf_get(&n_9bit->tx_buf_list);
		} else {
			pr_debug("%s: frame %p pending\n",
					__func__, tbuf);
			n_9bit_buf_put_head(&n_9bit->tx_buf_list, tbuf);
			break;
		}
	}


	/* Clear the re-entry flag */
	spin_lock_irqsave(&n_9bit->tx_buf_list.spinlock, flags);
	again = n_9bit->tbusy;
	n_9bit->tbusy = 0;
	spin_unlock_irqrestore(&n_9bit->tx_buf_list.spinlock, flags);

	if (again == 2)
		goto check_again;
	pr_debug("%s: exit\n", __func__);
}

/*
 * Callback for transmit wakeup
 * @tty	- pointer to associated tty instance data
 *
 * Called when low level device driver can accept more send data.
 */
static void n_9bit_tty_wakeup(struct tty_struct *tty)
{
	struct n_9bit *n_9bit = tty2n_9bit(tty);

	pr_debug("%s: called\n", __func__);

	if (!n_9bit)
		return;
	n_9bit_send_frames(n_9bit, tty);
}

/*
 * Called by tty driver when receive data is available
 * @tty	- pointer to tty instance data
 * @data - pointer to received data
 * @flags - pointer to flags for data
 * @count - count of received data in bytes
 *
 * Called by tty low level driver when receive data is available.
 */
static void n_9bit_tty_receive_buf(struct tty_struct *tty, const __u8 *data,
			       char *flags, int count)
{
	struct n_9bit *n_9bit = tty2n_9bit(tty);
	struct n_9bit_buf *rbuf = NULL;
	int wake = 0;

	pr_debug("%s: called count=%d\n", __func__, count);

	if (!n_9bit)
		return;

	for (;;) {
		int chars_till_parity_err = 0;
		unsigned ch;
		while (count) {
			if (*flags == TTY_PARITY)
				break;
			flags++;
			count--;
			chars_till_parity_err++;
		}
		if (!rbuf) {
			rbuf = n_9bit_buf_get(&n_9bit->rx_fill_list);
			if (rbuf)
				del_timer(&n_9bit->rx_eom_timer);
		}
		if (chars_till_parity_err) {
			/* add to existing buffer or drop(not for us) */
			if (rbuf) {
				unsigned max = maxframe - rbuf->count;
				unsigned c = chars_till_parity_err;
				if (c > max) {
					pr_debug("%s: c(%d) > max(%d)\n",
						__func__, c, max);
					c = max;
				}
				memcpy(rbuf->buf + rbuf->count, data, c);
				rbuf->count += c;
			}
			data += chars_till_parity_err;
		}
		if (!count) {
			if (rbuf) {
				n_9bit_buf_put(&n_9bit->rx_fill_list, rbuf);
				n_9bit->rx_eom_timer.expires = jiffies + msecs_to_jiffies(n_9bit->rx_timeout_msec);
				add_timer(&n_9bit->rx_eom_timer);
			}
			break;
		}
		if (rbuf) {
			/* add 9BIT buffer to list of received frames */
			n_9bit_buf_put(&n_9bit->rx_buf_list, rbuf);
			wake = 1;
			rbuf = NULL;
		}
		ch = *data++;
		flags++;
		count--;
		if (!test_bit(ch, n_9bit->rx_map))
			continue;

		/* get a free 9BIT buffer */
		rbuf = n_9bit_buf_get(&n_9bit->rx_free_buf_list);
		if (!rbuf) {
			/*
			 * no buffers in free list, attempt to allocate
			 * another rx buffer unless the maximum count has
			 * been reached
			 */
			if (n_9bit->rx_buf_list.count < MAX_RX_BUF_COUNT)
				rbuf = kmalloc(N_9BIT_BUF_SIZE, GFP_ATOMIC);
			if (!rbuf) {
				pr_debug("%s: no more rx buffers, data discarded\n",
						__func__);
				continue;
			}
		}
		*rbuf->buf = (unsigned char)ch;
		rbuf->count = 1;
	}
	/* wake up any blocked reads and perform async signaling */
	if (wake) {
		wake_up_interruptible(&tty->read_wait);
		if (n_9bit->tty->fasync != NULL)
			kill_fasync(&n_9bit->tty->fasync, SIGIO, POLL_IN);
	}
}

/*
 * Called to retrieve one frame of data (if available)
 * @tty - pointer to tty instance data
 * @file - pointer to open file object
 * @buf - pointer to returned data buffer
 * @nr - size of returned data buffer
 *
 * Returns the number of bytes returned or error code.
 */
static ssize_t n_9bit_tty_read(struct tty_struct *tty, struct file *file,
			   __u8 __user *buf, size_t nr)
{
	struct n_9bit *n_9bit;
	int ret;
	struct n_9bit_buf *rbuf;

	/* verify user access to buffer */
	if (!access_ok(VERIFY_WRITE, buf, nr)) {
		pr_warn("%s: can't verify user buffer\n", __func__);
		return -EFAULT;
	}

	for (;;) {
		if (test_bit(TTY_OTHER_CLOSED, &tty->flags)) {
			return -EIO;
		}

		n_9bit = tty2n_9bit(tty);
		if (!n_9bit)
			return 0;

		rbuf = n_9bit_buf_get(&n_9bit->rx_buf_list);
		if (rbuf)
			break;

		/* no data */
		if (file->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		}
		ret = wait_event_interruptible(tty->read_wait, n_9bit->rx_buf_list.head);
		if (ret)
			return ret;
	}

	if (rbuf->count > nr) {
		/* frame too large for caller's buffer */
		n_9bit_buf_put_head(&n_9bit->rx_buf_list, rbuf);
		return -EOVERFLOW;
	} else {
		/* Copy the data to the caller's buffer */
		if (copy_to_user(buf, rbuf->buf, rbuf->count)) {
			n_9bit_buf_put_head(&n_9bit->rx_buf_list, rbuf);
			return -EFAULT;
		} else {
			ret = rbuf->count;
		}
	}

	/* return 9BIT buffer to free list unless the free list */
	/* count has exceeded the default value, in which case the */
	/* buffer is freed back to the OS to conserve memory */
	if (n_9bit->rx_free_buf_list.count > DEFAULT_RX_BUF_COUNT)
		kfree(rbuf);
	else
		n_9bit_buf_put(&n_9bit->rx_free_buf_list, rbuf);
	return ret;
}

/*
 * write a single frame of data to device
 * @tty	- pointer to associated tty device instance data
 * @file - pointer to file object data
 * @data - pointer to transmit data (one frame)
 * @count - size of transmit frame in bytes
 *
 * Returns the number of bytes written (or error code).
 */
static ssize_t n_9bit_tty_write(struct tty_struct *tty, struct file *file,
			    const unsigned char *data, size_t count)
{
	struct n_9bit *n_9bit = tty2n_9bit(tty);
	int error = 0;
	struct n_9bit_buf *tbuf;

	pr_debug("%s: called count=%Zd\n", __func__, count);

	/* Verify pointers */
	if (!n_9bit)
		return -EIO;

	/* verify frame size */
	if (count > maxframe ) {
		pr_warn("%s: truncating user packet from %lu to %d\n",
			__func__, (unsigned long) count, maxframe);
		count = maxframe;
	}

	/* Allocate transmit buffer */
	/* sleep until transmit buffer available */
	for (;;) {
		tbuf = n_9bit_buf_get(&n_9bit->tx_free_buf_list);
		if (tbuf)
			break;
		if (file->f_flags & O_NONBLOCK) {
			error = -EAGAIN;
			break;
		}
		error = wait_event_interruptible(tty->write_wait, n_9bit->tx_free_buf_list.head);
		if (error)
			break;

		n_9bit = tty2n_9bit(tty);
		if (!n_9bit) {
			pr_err("%s: invalid after wait!\n", __func__);
			error = -EIO;
			break;
		}
	}

	if (!error) {
		/* User's buffer is already in kernel space */
		memcpy(tbuf->buf, data, count);
		/* Send the data */
		tbuf->count = error = count;
		n_9bit_buf_put(&n_9bit->tx_buf_list, tbuf);
		n_9bit_send_frames(n_9bit, tty);
	}
	pr_debug("%s: exit %d\n", __func__, error);
	return error;
}

/*
 * process IOCTL system call for the tty device.
 * @tty - pointer to tty instance data
 * @file - pointer to open file object for device
 * @cmd - IOCTL command code
 * @arg - argument for IOCTL call (cmd dependent)
 *
 * Returns command dependent result.
 */
static int n_9bit_tty_ioctl(struct tty_struct *tty, struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	struct n_9bit *n_9bit = tty2n_9bit(tty);
	int error = 0;
	int count;
	unsigned long flags;

	pr_debug("%s: entry cmd=%d\n", __func__, cmd);

	/* Verify the status of the device */
	if (!n_9bit)
		return -EBADF;

	switch (cmd) {
	case FIONREAD:
		/* report count of read data available */
		/* in next available frame (if any) */
		spin_lock_irqsave(&n_9bit->rx_buf_list.spinlock, flags);
		if (n_9bit->rx_buf_list.head)
			count = n_9bit->rx_buf_list.head->count;
		else
			count = 0;
		spin_unlock_irqrestore(&n_9bit->rx_buf_list.spinlock, flags);
		error = put_user(count, (int __user *)arg);
		break;

	case TIOCOUTQ:
		/* get the pending tx byte count in the driver */
		count = tty_chars_in_buffer(tty);
		/* add size of next output frame in queue */
		spin_lock_irqsave(&n_9bit->tx_buf_list.spinlock, flags);
		if (n_9bit->tx_buf_list.head)
			count += n_9bit->tx_buf_list.head->count;
		spin_unlock_irqrestore(&n_9bit->tx_buf_list.spinlock, flags);
		error = put_user(count, (int __user *)arg);
		break;

	case TIOC_SET_RX_MASK:
		if (copy_from_user(&n_9bit->rx_map, (int __user *)arg, 32))
			error = -EFAULT;
		break;

	case TIOC_SET_RX_TIMEOUT:
		if (copy_from_user(&n_9bit->rx_timeout_msec, (int __user *)arg, sizeof(int)))
			error = -EFAULT;
		break;
	case TCFLSH:
		switch (arg) {
		case TCIOFLUSH:
		case TCOFLUSH:
			flush_tx_queue(tty);
		}
		/* fall through to default */

	default:
		error = n_tty_ioctl_helper(tty, file, cmd, arg);
		break;
	}
	return error;

}

/*
 * TTY callback for poll system call
 * @tty - pointer to tty instance data
 * @filp - pointer to open file object for device
 * @poll_table - wait queue for operations
 *
 * Determine which operations (read/write) will not block and return info
 * to caller.
 * Returns a bit mask containing info on which ops will not block.
 */
static unsigned int n_9bit_tty_poll(struct tty_struct *tty, struct file *filp,
				    poll_table *wait)
{
	struct n_9bit *n_9bit = tty2n_9bit(tty);
	unsigned int mask = 0;

	pr_debug("%s: entry\n", __func__);

	if (n_9bit) {
		/* queue current process into any wait queue that */
		/* may awaken in the future (read and write) */

		poll_wait(filp, &tty->read_wait, wait);
		poll_wait(filp, &tty->write_wait, wait);

		/* set bits for operations that won't block */
		if (n_9bit->rx_buf_list.head)
			mask |= POLLIN | POLLRDNORM;	/* readable */
		if (test_bit(TTY_OTHER_CLOSED, &tty->flags))
			mask |= POLLHUP;
		if (tty_hung_up_p(filp))
			mask |= POLLHUP;
		if (!tty_is_writelocked(tty) &&
				n_9bit->tx_free_buf_list.head)
			mask |= POLLOUT | POLLWRNORM;	/* writable */
	}
	return mask;
}

static struct tty_ldisc_ops n_9bit_ldisc = {
	.owner		= THIS_MODULE,
	.magic		= TTY_LDISC_MAGIC,
	.name		= "9bit",
	.open		= n_9bit_tty_open,
	.close		= n_9bit_tty_close,
	.read		= n_9bit_tty_read,
	.write		= n_9bit_tty_write,
	.ioctl		= n_9bit_tty_ioctl,
	.poll		= n_9bit_tty_poll,
	.receive_buf	= n_9bit_tty_receive_buf,
	.write_wakeup	= n_9bit_tty_wakeup,
	.flush_buffer   = flush_rx_queue,
};


static int __init n_9bit_init(void)
{
	int status;

	/* range check maxframe arg */
	if (maxframe < 4096)
		maxframe = 4096;
	else if (maxframe > 65535)
		maxframe = 65535;

	status = tty_register_ldisc(N_9BIT, &n_9bit_ldisc);
	if (status)
		pr_err("N_9BIT: error registering line discipline: %d\n", status);
	else
		pr_info("N_9BIT: line discipline registered. maxframe=%u\n", maxframe);

	return status;

}	/* end of init_module() */

static void __exit n_9bit_exit(void)
{
	/* Release tty registration of line discipline */
	int status = tty_unregister_ldisc(N_9BIT);

	if (status)
		pr_err("N_9BIT: can't unregister line discipline(err = %d)\n", status);
	else
		pr_info("N_9BIT: line discipline unregistered\n");
}

module_init(n_9bit_init);
module_exit(n_9bit_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Troy Kisky troy.kisky@boundarydevices.com");
MODULE_ALIAS_LDISC(N_9BIT);
