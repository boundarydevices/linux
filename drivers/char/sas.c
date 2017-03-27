/*
 * sas: a simple 9-bit serial driver suitable for
 * us in implementing the Slot Accounting System (SAS)
 * protocol.
 *
 * - The driver exposes character devices /dev/sas0-N,
 * which use the 9-bit support of the UARTs on i.MX
 * SOCs to implement the framing portion of the protocol.
 *
 * - Each write() call is translated into a message with
 * MARK parity on the first byte and SPACE parity for the
 * remaining bytes of the message.
 *
 * Received data is parsed according to the same protocol,
 * such that a "message" is defined as a set of incoming
 * data that starts with a byte with MARK parity and
 * terminates when either a timeout occurs or another
 * byte with MARK parity is received
 *
 * - Each read() call returns zero or one complete
 * messages.
 *
 * - Improperly framed incoming messages are reported
 * through printk but are not returned to userspace.
 * e.g. if a timeout occurs to terminate a message and
 * is followed by data with SPACE parity, the characters
 * will be dropped.
 *
 * The driver supports poll().
 *
 *	POLLIN is signalled when a complete message is available.
 *
 *	POLLOUT is signalled when space for a maximum sized message
 *		is available (device tree maxtxmsg).
 *
 * Copyright (C) 2015, Boundary Devices
 */

#define DEBUG

#include <linux/cdev.h>
#include <linux/circ_buf.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

/* Register definitions */
#define URXD0 0x0  /* Receiver Register */
#define URTX0 0x40 /* Transmitter Register */
#define UCR1  0x80 /* Control Register 1 */
#define UCR2  0x84 /* Control Register 2 */
#define UCR3  0x88 /* Control Register 3 */
#define UCR4  0x8c /* Control Register 4 */
#define UFCR  0x90 /* FIFO Control Register */
#define USR1  0x94 /* Status Register 1 */
#define USR2  0x98 /* Status Register 2 */
#define UESC  0x9c /* Escape Character Register */
#define UTIM  0xa0 /* Escape Timer Register */
#define UBIR  0xa4 /* BRM Incremental Register */
#define UBMR  0xa8 /* BRM Modulator Register */
#define UBRC  0xac /* Baud Rate Count Register */
#define IMX21_ONEMS 0xb0 /* One Millisecond register */
#define IMX1_UTS 0xd0 /* UART Test Register on i.mx1 */
#define IMX21_UTS 0xb4 /* UART Test Register on all other i.mx*/
#define UMCR_MDEN 1		/* 9-bit/Multidrop enable */
#define UMCR_TXB8 (1<<2)	/* parity bit goes here */

/* UART Control Register Bit Fields.*/
#define URXD_CHARRDY	(1<<15)
#define URXD_ERR	(1<<14)
#define URXD_OVRRUN	(1<<13)
#define URXD_FRMERR	(1<<12)
#define URXD_BRK	(1<<11)
#define URXD_PRERR	(1<<10)
#define UCR1_ADEN	(1<<15) /* Auto detect interrupt */
#define UCR1_ADBR	(1<<14) /* Auto detect baud rate */
#define UCR1_TRDYEN	(1<<13) /* Transmitter ready interrupt enable */
#define UCR1_IDEN	(1<<12) /* Idle condition interrupt */
#define UCR1_ICD_REG(x) (((x) & 3) << 10) /* idle condition detect */
#define UCR1_RRDYEN	(1<<9)	/* Recv ready interrupt enable */
#define UCR1_RDMAEN	(1<<8)	/* Recv ready DMA enable */
#define UCR1_IREN	(1<<7)	/* Infrared interface enable */
#define UCR1_TXMPTYEN	(1<<6)	/* Transimitter empty interrupt enable */
#define UCR1_RTSDEN	(1<<5)	/* RTS delta interrupt enable */
#define UCR1_SNDBRK	(1<<4)	/* Send break */
#define UCR1_TDMAEN	(1<<3)	/* Transmitter ready DMA enable */
#define IMX1_UCR1_UARTCLKEN (1<<2) /* UART clock enabled, i.mx1 only */
#define UCR1_ATDMAEN    (1<<2)  /* Aging DMA Timer Enable */
#define UCR1_DOZE	(1<<1)	/* Doze */
#define UCR1_UARTEN	(1<<0)	/* UART enabled */
#define UCR2_ESCI	(1<<15)	/* Escape seq interrupt enable */
#define UCR2_IRTS	(1<<14)	/* Ignore RTS pin */
#define UCR2_CTSC	(1<<13)	/* CTS pin control */
#define UCR2_CTS	(1<<12)	/* Clear to send */
#define UCR2_ESCEN	(1<<11)	/* Escape enable */
#define UCR2_PREN	(1<<8)	/* Parity enable */
#define UCR2_PROE	(1<<7)	/* Parity odd/even */
#define UCR2_STPB	(1<<6)	/* Stop */
#define UCR2_WS		(1<<5)	/* Word size */
#define UCR2_RTSEN	(1<<4)	/* Request to send interrupt enable */
#define UCR2_ATEN	(1<<3)	/* Aging Timer Enable */
#define UCR2_TXEN	(1<<2)	/* Transmitter enabled */
#define UCR2_RXEN	(1<<1)	/* Receiver enabled */
#define UCR2_SRST	(1<<0)	/* SW reset */
#define UCR3_DTREN	(1<<13) /* DTR interrupt enable */
#define UCR3_PARERREN	(1<<12) /* Parity enable */
#define UCR3_FRAERREN	(1<<11) /* Frame error interrupt enable */
#define UCR3_DSR	(1<<10) /* Data set ready */
#define UCR3_DCD	(1<<9)	/* Data carrier detect */
#define UCR3_RI		(1<<8)	/* Ring indicator */
#define UCR3_ADNIMP	(1<<7)	/* Autobaud Detection Not Improved */
#define UCR3_RXDSEN	(1<<6)	/* Receive status interrupt enable */
#define UCR3_AIRINTEN	(1<<5)	/* Async IR wake interrupt enable */
#define UCR3_AWAKEN	(1<<4)	/* Async wake interrupt enable */
#define IMX21_UCR3_RXDMUXSEL	(1<<2)	/* RXD Muxed Input Select */
#define UCR3_INVT	(1<<1)	/* Inverted Infrared transmission */
#define UCR3_BPEN	(1<<0)	/* Preset registers enable */
#define UCR4_CTSTL_SHF	10	/* CTS trigger level shift */
#define UCR4_CTSTL_MASK	0x3F	/* CTS trigger is 6 bits wide */
#define UCR4_INVR	(1<<9)	/* Inverted infrared reception */
#define UCR4_ENIRI	(1<<8)	/* Serial infrared interrupt enable */
#define UCR4_WKEN	(1<<7)	/* Wake interrupt enable */
#define UCR4_REF16	(1<<6)	/* Ref freq 16 MHz */
#define UCR4_IDDMAEN    (1<<6)  /* DMA IDLE Condition Detected */
#define UCR4_IRSC	(1<<5)	/* IR special case */
#define UCR4_TCEN	(1<<3)	/* Transmit complete interrupt enable */
#define UCR4_BKEN	(1<<2)	/* Break condition interrupt enable */
#define UCR4_OREN	(1<<1)	/* Receiver overrun interrupt enable */
#define UCR4_DREN	(1<<0)	/* Recv data ready interrupt enable */
#define UFCR_RXTL_SHF	0	/* Receiver trigger level shift */
#define UFCR_DCEDTE	(1<<6)	/* DCE/DTE mode select */
#define UFCR_RFDIV	(7<<7)	/* Reference freq divider mask */
#define UFCR_RFDIV_REG(x)	(((x) < 7 ? 6 - (x) : 6) << 7)
#define UFCR_TXTL_SHF	10	/* Transmitter trigger level shift */
#define USR1_PARITYERR	(1<<15) /* Parity error interrupt flag */
#define USR1_RTSS	(1<<14) /* RTS pin status */
#define USR1_TRDY	(1<<13) /* Transmitter ready interrupt/dma flag */
#define USR1_RTSD	(1<<12) /* RTS delta */
#define USR1_ESCF	(1<<11) /* Escape seq interrupt flag */
#define USR1_FRAMERR	(1<<10) /* Frame error interrupt flag */
#define USR1_RRDY	(1<<9)	 /* Receiver ready interrupt/dma flag */
#define USR1_TIMEOUT	(1<<7)	 /* Receive timeout interrupt status */
#define USR1_RXDS	 (1<<6)	 /* Receiver idle interrupt flag */
#define USR1_AIRINT	 (1<<5)	 /* Async IR wake interrupt flag */
#define USR1_AWAKE	 (1<<4)	 /* Aysnc wake interrupt flag */
#define USR2_ADET	 (1<<15) /* Auto baud rate detect complete */
#define USR2_TXFE	 (1<<14) /* Transmit buffer FIFO empty */
#define USR2_DTRF	 (1<<13) /* DTR edge interrupt flag */
#define USR2_IDLE	 (1<<12) /* Idle condition */
#define USR2_IRINT	 (1<<8)	 /* Serial infrared interrupt flag */
#define USR2_WAKE	 (1<<7)	 /* Wake */
#define USR2_RTSF	 (1<<4)	 /* RTS edge interrupt flag */
#define USR2_TXDC	 (1<<3)	 /* Transmitter complete */
#define USR2_BRCD	 (1<<2)	 /* Break condition */
#define USR2_ORE	(1<<1)	 /* Overrun error */
#define USR2_RDR	(1<<0)	 /* Recv data ready */
#define UTS_FRCPERR	(1<<13) /* Force parity error */
#define UTS_LOOP	(1<<12)	 /* Loop tx and rx */
#define UTS_TXEMPTY	 (1<<6)	 /* TxFIFO empty */
#define UTS_RXEMPTY	 (1<<5)	 /* RxFIFO empty */
#define UTS_TXFULL	 (1<<4)	 /* TxFIFO full */
#define UTS_RXFULL	 (1<<3)	 /* RxFIFO full */
#define UTS_SOFTRST	 (1<<0)	 /* Software reset */

static int sas_major;
static dev_t devnum;
static struct class *sas_class;

#define DRIVER_NAME "sas"

#define MS_TO_NS(msec)	((msec) * 1000 * 1000)

static void hrtimer_mod(struct hrtimer *timer, u32 ms)
{
	ktime_t ktime = ktime_set(0, MS_TO_NS(ms));
	hrtimer_try_to_cancel(timer);
	hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
}

struct msg_t {
	int start;
	int end;
};


struct imx_sas_devdata {
	unsigned umcr_reg;
};

/*
 * Device structure, to keep track of everything device-specific here
 */
struct sas_dev {
	struct cdev cdev;
	struct device *chrdev;
	struct platform_device *pdev;
	void __iomem *base;
	wait_queue_head_t queue;
	spinlock_t lock; /* only one IRQ at a time */
	struct mutex rx_lock; /* only one reader */
	struct mutex tx_lock; /* only one writer */
	struct hrtimer timer;
	int open_count;

	struct clk *clk_ipg;
	struct clk *clk_per;
	unsigned umcr_reg;
	int irq;
	int rxirq;
	int txirq;
	u32 baud;
	u32 interbyte_delay;
	u32 rxbufsize;
	u32 maxrxmsgs;
	u32 txbufsize;
	u32 maxtxmsg;
	u32 flush_on_mark;
	struct circ_buf rxbuf;
	struct msg_t *rxmsgs;
	u32 rxmsgadd;
	u32 rxmsgtake;
	struct circ_buf txbuf;
	u8 *txpbuf; /* parity for outbound characters */
	u8 last_parity;
	u8 force_tx_par_err;
};

#define CIRC_NEXT(index, size) ((index + 1) & (size - 1))
#define CIRC_PREV(index, size) ((index - 1) & (size - 1))

/* end of last messsage added */
#define PREVMSGEND(dev) (dev->rxmsgs[CIRC_PREV(dev->rxmsgadd, \
					       dev->maxrxmsgs)].end)

/* start of next message */
#define NEXTMSGSTART(dev) (dev->rxmsgs[dev->rxmsgadd].start)

//#define DEBUG_READS

static void flush_msg(struct sas_dev *dev)
{
	if (dev->rxbuf.head != NEXTMSGSTART(dev)) {
		int nextmsg = CIRC_NEXT(dev->rxmsgadd, dev->maxrxmsgs);
		int end = CIRC_PREV(dev->rxbuf.head, dev->rxbufsize);
		if (dev->rxmsgtake != nextmsg) {
			dev->rxmsgs[dev->rxmsgadd].end = end;
			dev->rxmsgadd = nextmsg;
			dev->rxmsgs[nextmsg].start = dev->rxbuf.head;
		} else {
			/* append to the previous message, but whine */
			dev_err(&dev->pdev->dev, "message overflow\n");
			PREVMSGEND(dev) = end;
		}
		wake_up(&dev->queue);
	}
}

static enum hrtimer_restart rx_timer(struct hrtimer *timer)
{
	struct sas_dev *dev = container_of(timer, struct sas_dev, timer);
	flush_msg(dev);
	return HRTIMER_NORESTART;
}

static ssize_t sas_read
	(struct file *file, char __user *buf,
	 size_t count, loff_t *ppos)
{
#ifdef DEBUG_READS
	unsigned char dbg_buf[4];
	unsigned char *p = dbg_buf;
	int rem = 4;
	int len;
#endif
	struct sas_dev *dev = (struct sas_dev *)file->private_data;
	ssize_t numread = 0;

	mutex_lock(&dev->rx_lock);
	if (dev->rxmsgadd != dev->rxmsgtake) {
		struct msg_t *msg = dev->rxmsgs + dev->rxmsgtake;
		int start = msg->start;
		int firstseg;
		int msgleft = (msg->end - msg->start + 1)
				& (dev->rxbufsize - 1);
		if (msgleft > count) {
			numread = -ENOBUFS;
			goto out;
		}
		firstseg = dev->rxbufsize - start;
		if (firstseg > msgleft)
			firstseg = msgleft;
#ifdef DEBUG_READS
		len = firstseg < rem ? firstseg : rem;
		memcpy(p, dev->rxbuf.buf+start, len);
		p += len;
		rem -= len;
#endif
		if (copy_to_user(buf, dev->rxbuf.buf+start, firstseg)) {
			numread = -EFAULT;
			goto out;
		}
		count = msgleft;
		msgleft -= firstseg;
		numread = firstseg;
		if (0 < msgleft) {
			/* wrap: need to copy a second segment */
			buf += firstseg;
#ifdef DEBUG_READS
			len = msgleft < rem ? msgleft : rem;
			memcpy(p, dev->rxbuf.buf+0, len);
			p += len;
			rem -= len;
#endif
			if (copy_to_user(buf, dev->rxbuf.buf+0, msgleft)) {
				numread = -EFAULT;
				goto out;
			}
			dev->rxbuf.tail = msgleft;
			numread += msgleft;
		} else {
			dev->rxbuf.tail = start + firstseg;
		}
		dev->rxmsgtake = CIRC_NEXT(dev->rxmsgtake, dev->maxrxmsgs);
	} /* have a message */
out:
	mutex_unlock(&dev->rx_lock);
#ifdef DEBUG_READS
	p = dbg_buf;
	if (numread >= 4) {
		pr_info("%s: %02x %02x %02x %02x\n", __func__,
			p[0], p[1], p[2], p[3]);
	} else if (numread == 3) {
		pr_info("%s: %02x %02x %02x\n", __func__,
			p[0], p[1], p[2]);
	} else if (numread == 2) {
		pr_info("%s: %02x %02x\n", __func__,
			p[0], p[1]);
	} else if (numread == 1) {
		pr_info("%s: %02x\n", __func__,
			p[0]);
	} else {
		pr_info("%s: returning %d\n", __func__, numread);

	}
#endif
	return numread;
}

static ssize_t sas_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	int written = 0;
	u32 reg;
	struct sas_dev *dev = (struct sas_dev *)file->private_data;

	mutex_lock(&dev->tx_lock);
	if (CIRC_SPACE(dev->txbuf.head,
		       dev->txbuf.tail,
		       dev->txbufsize) >= count) {
		int start = dev->txbuf.head;
		int firstseg = dev->txbufsize - start;
		if (firstseg > count)
			firstseg = count;
		if (copy_from_user(dev->txbuf.buf+start, buf, firstseg)) {
			written = -EFAULT;
		} else if (count > firstseg) {
			/* wrap: copy second segment */

			int left = count - firstseg;
			buf += firstseg;
			if (copy_from_user(dev->txbuf.buf, buf, left))
				written = -EFAULT;
		}
		if (0 == written) {
			u32 nextbyte = start / 8;
			u32 const pbytemask = (dev->txbufsize / 8) - 1;
			u8 mask = 1 << (start & 7);
			u8 byteval = dev->txpbuf[start/8] | mask;
			mask = ~(mask << 1);
			written = 1;
			while (--count) {
				if ('\xff' == mask) {
					dev->txpbuf[nextbyte] = byteval;
					nextbyte = (nextbyte + 1) & pbytemask;
					byteval = dev->txpbuf[nextbyte];
					mask = (u8)~1;
				}
				byteval &= mask;
				mask = (mask << 1) | 1;
				written++;
			}
			dev->txpbuf[nextbyte] = byteval;
			dev->txbuf.head = (dev->txbuf.head + written)
						& (dev->txbufsize - 1);
			reg = readl(dev->base + UCR1);
			if (!(reg & UCR1_TRDYEN)) {
				reg = readl(dev->base + UCR4);
				if (!(reg & UCR4_TCEN)) {
					reg |= UCR4_TCEN;
					writel(reg, dev->base + UCR4);
				}
			}
		}
	}

	mutex_unlock(&dev->tx_lock);
	return written;
}

static unsigned int sas_poll(struct file *file, struct poll_table_struct *table)
{
	unsigned int flags = 0;
	struct sas_dev *dev = (struct sas_dev *)file->private_data;
	if (!dev)
		return -EINVAL;

	poll_wait(file, &dev->queue, table);

	if (dev->rxbuf.head != dev->rxbuf.tail)
		flags |= POLLIN | POLLRDNORM;
	if (CIRC_SPACE(dev->txbuf.head,
		       dev->txbuf.tail,
		       dev->txbufsize) >= dev->maxtxmsg) {
		flags |= POLLOUT;
	}
	return flags;
}

static void sas_rxint(struct sas_dev *dev)
{
	int space = CIRC_SPACE(dev->rxbuf.head,
			       dev->rxbuf.tail,
			       dev->rxbufsize);
	while (space >= 3) {
		u32 in = readl(dev->base + URXD0);
		if (!(in & URXD_CHARRDY))
			break;
		if (!dev->umcr_reg) {
                        u32 ch_parity = in;

                        ch_parity ^= ch_parity >> 4;
                        ch_parity ^= ch_parity >> 2;
                        ch_parity ^= ch_parity >> 1;
                        ch_parity &= 1;
                        /* URXD_PRERR is bit 10 */
                        in ^= (ch_parity << 10);
		}
		if (in & URXD_PRERR) {
			/*
			 * if a part of a message is present, terminate it
			 * and notify userspace
			 */
			if (dev->flush_on_mark)
				flush_msg(dev);
			dev->rxbuf.buf[dev->rxbuf.head] = 0xff;
			dev->rxbuf.head = CIRC_NEXT(dev->rxbuf.head,
						    dev->rxbufsize);
			dev->rxbuf.buf[dev->rxbuf.head] = 0;
			dev->rxbuf.head = CIRC_NEXT(dev->rxbuf.head,
						    dev->rxbufsize);
			dev->rxbuf.buf[dev->rxbuf.head] = in;
			dev->rxbuf.head = CIRC_NEXT(dev->rxbuf.head,
						    dev->rxbufsize);

			space -= 3;
		} else {
			dev->rxbuf.buf[dev->rxbuf.head] = in;
			dev->rxbuf.head = CIRC_NEXT(dev->rxbuf.head,
						    dev->rxbufsize);
			space--;
		}

		hrtimer_mod(&dev->timer, dev->interbyte_delay);
		if (!(readl(dev->base + USR2) & USR2_RDR))
			break;
	}
	if (3 >= space) {
		dev_err(&dev->pdev->dev, "overrun");
		while (readl(dev->base + URXD0) & URXD_CHARRDY)
			;
	}
}

static void sas_txint(struct sas_dev *dev)
{
	u32 reg;
	while (CIRC_CNT(dev->txbuf.head,
			dev->txbuf.tail,
			dev->txbufsize) &&
			!(readl(dev->base + IMX21_UTS) & UTS_TXFULL)) {
		u8 ch = dev->txbuf.buf[dev->txbuf.tail];
		u8 shift = dev->txbuf.tail & 7;
		u8 mark_space = (dev->txpbuf[dev->txbuf.tail/8] >> shift) & 1;
		u8 change_needed;

		if (dev->umcr_reg) {
			change_needed = mark_space ^ dev->last_parity;
		} else {
			unsigned ch_parity = dev->force_tx_par_err ^ mark_space ^ ch;
	                ch_parity ^= ch_parity >> 4;
	                ch_parity ^= ch_parity >> 2;
	                ch_parity ^= ch_parity >> 1;
	                change_needed = ch_parity & 1;
		}
		if (change_needed) {
			reg = readl(dev->base + USR2);
			if (!(reg & USR2_TXDC)) {
				/*
				 * must wait until tx complete to change force
				 * parity error status or
				 * TXB8 status
				 */
				reg = readl(dev->base + UCR4);
				if (!(reg & UCR4_TCEN)) {
					reg |= UCR4_TCEN;
					writel(reg, dev->base + UCR4);
				}
				reg = readl(dev->base + UCR1);
				writel(reg & ~(UCR1_TXMPTYEN | UCR1_TRDYEN), dev->base + UCR1);
				return;
			}
			if (dev->umcr_reg) {
				reg = readl(dev->base + dev->umcr_reg);
				if (mark_space)
					reg |= UMCR_TXB8;
				else
					reg &= ~UMCR_TXB8;
				writel(reg, dev->base + dev->umcr_reg);
				dev->last_parity = mark_space;
			} else {
	                        reg = readl(dev->base + IMX21_UTS);
	                        dev->force_tx_par_err ^= 1;
	                        if (dev->force_tx_par_err)
	                                reg |= UTS_FRCPERR;
	                        else
	                                reg &= ~UTS_FRCPERR;
	                        writel(reg, dev->base + IMX21_UTS);
			}
		}

		writel(ch, dev->base + URTX0);
		dev->txbuf.tail = CIRC_NEXT(dev->txbuf.tail, dev->txbufsize);
		if (CIRC_SPACE(dev->txbuf.head,
			       dev->txbuf.tail,
			       dev->txbufsize) >= dev->maxtxmsg)
			wake_up(&dev->queue);
	}
	if (!CIRC_CNT(dev->txbuf.head,
		      dev->txbuf.tail,
		      dev->txbufsize)) {
		reg = readl(dev->base + UCR1);
		if (reg & (UCR1_TRDYEN | UCR1_TXMPTYEN)) {
			reg &= ~(UCR1_TRDYEN | UCR1_TXMPTYEN);
			writel(reg, dev->base + UCR1);
		}
		reg = readl(dev->base + UCR4);
		if (reg & UCR4_TCEN) {
			reg &= ~UCR4_TCEN;
			writel(reg, dev->base + UCR4);
		}
		if (CIRC_CNT(dev->txbuf.head,
			      dev->txbuf.tail,
			      dev->txbufsize)) {
			/* Turn interrupt back on */
			reg |= UCR4_TCEN;
			writel(reg, dev->base + UCR4);
		}
	} else {
		reg = readl(dev->base + UCR1);
		if (!(reg & UCR1_TRDYEN)) {
			reg |= UCR1_TRDYEN;
			writel(reg, dev->base + UCR1);
		}
	}
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct sas_dev *dev = dev_id;
	unsigned int sts;

	sts = readl(dev->base + USR1);
	if (sts & USR1_RRDY)
		sas_rxint(dev_id);

	if (sts & USR1_TRDY)
		sas_txint(dev_id);

	sts = readl(dev->base + USR2);
	if (sts & USR2_ORE)
		writel(USR2_ORE, dev->base + USR2);
	return IRQ_HANDLED;
}

/*
 * We don't want address detection, so force an
 * address match by entering loopback and transmitting
 * a character with the mark bit set.
 */
static void force_address_match(struct sas_dev *dev)
{
	if (!dev->umcr_reg)
		return;
	writel(UCR1_UARTEN, dev->base + UCR1);

	/* loopback */
	writel(UTS_LOOP, dev->base + IMX21_UTS);

	/* mark parity */
	writel(UMCR_TXB8 | UMCR_MDEN, dev->base + dev->umcr_reg);

	/* transmit a null */
	writel(0, dev->base + URTX0);

	/* wait for receiver ready */
	usleep_range(1000, 2000);
	(void)readl(dev->base + URXD0);

	/* out of loopback */
	writel(0, dev->base + IMX21_UTS);

	/* and space parity */
	writel(UMCR_MDEN, dev->base + dev->umcr_reg);
}

static int sas_open(struct inode *inode, struct file *file)
{
	int rval = 0;
	unsigned long flags;
	unsigned cnt;
	struct sas_dev *dev = container_of(inode->i_cdev,
					   struct sas_dev, cdev);
	file->private_data = dev;
	spin_lock_irqsave(&dev->lock, flags);
	cnt = ++dev->open_count;
	spin_unlock_irqrestore(&dev->lock, flags);

	if (1 == cnt) {
		rval = clk_prepare_enable(dev->clk_per);
		if (rval)
			goto out;
		rval = clk_prepare_enable(dev->clk_ipg);
		if (rval) {
			clk_disable_unprepare(dev->clk_per);
			goto out;
		}
		writel(0x0, dev->base + UCR1);
		writel(0x0, dev->base + UCR2);

		while (!(readl(dev->base + UCR2) & UCR2_SRST))
			;

		writel(IMX21_UCR3_RXDMUXSEL | UCR3_ADNIMP,
		       dev->base + UCR3);
		writel(0x8000, dev->base + UCR4);
		writel(0x002b, dev->base + UESC);
		writel(0x0, dev->base + UTIM);
		writel(0x0, dev->base + IMX1_UTS);
		writel(0, dev->base + IMX21_UTS);
		dev->force_tx_par_err = 0;

		/* divide input clock by 2, receive fifo 1, txtl 2 */
		writel((4 << 7) | 0x801, dev->base + UFCR);
		writel(0xf, dev->base + UBIR);
		writel(clk_get_rate(dev->clk_per) / (2 * dev->baud),
		       dev->base + UBMR);

		if (dev->umcr_reg) {
			writel(UMCR_MDEN, dev->base + dev->umcr_reg);
		}
		writel(UCR2_WS | UCR2_IRTS
		       | UCR2_RXEN | UCR2_TXEN
		       | UCR2_SRST | UCR2_PREN,
		       dev->base + UCR2);

		dev->rxbuf.head =
		dev->rxbuf.tail =
		dev->txbuf.head =
		dev->txbuf.tail = 0;
		dev->rxmsgadd =
		dev->rxmsgtake = 0;
		dev->rxmsgs[0].start = 0;
		dev->last_parity = 0;
		force_address_match(dev);

		rval = devm_request_irq(&dev->pdev->dev,
					  dev->irq, irq_handler,
					  IRQF_TRIGGER_PROBE, DRIVER_NAME, dev);
		if (!rval) {
			writel(UCR1_UARTEN | UCR1_RRDYEN, dev->base + UCR1);
		} else {
			writel(0x0, dev->base + UCR1);
			clk_disable_unprepare(dev->clk_ipg);
			clk_disable_unprepare(dev->clk_per);
			dev_err(&dev->pdev->dev,
				"Error %d requesting irq %d\n",
				rval, dev->irq);
			--dev->open_count;
		}
	}
out:
	return rval;
}

static int sas_release(struct inode *inode, struct file *file)
{
	int loop = 0;
	unsigned long flags;
	struct sas_dev *dev = container_of(inode->i_cdev,
					   struct sas_dev, cdev);

	while (1) {
		int cnt = CIRC_CNT(dev->txbuf.head,
				dev->txbuf.tail,
				dev->txbufsize);
		if (!cnt)
			break;
		if (loop++ > 10)
			break;
		msleep(100);
	}
	spin_lock_irqsave(&dev->lock, flags);
	if (0 == --dev->open_count) {
		writel(0x0, dev->base + UCR1);
		writel(0x0, dev->base + UCR2);
		devm_free_irq(&dev->pdev->dev, dev->irq, dev);
		clk_disable_unprepare(dev->clk_ipg);
		clk_disable_unprepare(dev->clk_per);
	}

	spin_unlock_irqrestore(&dev->lock, flags);
	return -1;
}

static struct file_operations const sas_fops = {
	.owner = THIS_MODULE,
	.read = sas_read,
	.write = sas_write,
	.poll = sas_poll,
	.open = sas_open,
	.release = sas_release,
};

static ssize_t show_ibdelay(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sas_dev *sas = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", sas ? sas->interbyte_delay : -1);
}

static ssize_t store_ibdelay(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf,
			     size_t count)
{
	u32 val;
	struct sas_dev *sas = dev_get_drvdata(dev);
	if (1 == sscanf(buf, "%u", &val)) {
		if (sas) {
			sas->interbyte_delay = val;
			return count;
		} else {
			return -ENODEV;
		}
	} else {
		return -EINVAL;
	}
}

static struct kobj_attribute ibdelay =
__ATTR(ibdelay, 0644, (void *)show_ibdelay, (void *)store_ibdelay);

static ssize_t show_rxhead(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sas_dev *sas = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", sas ? sas->rxbuf.head : -1);
}

static struct kobj_attribute rxhead =
__ATTR(rxhead, 0644, (void *)show_rxhead, NULL);

static ssize_t show_rxtail(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sas_dev *sas = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", sas ? sas->rxbuf.tail : -1);
}

static struct kobj_attribute rxtail =
__ATTR(rxtail, 0644, (void *)show_rxtail, NULL);

static ssize_t show_txhead(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sas_dev *sas = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", sas ? sas->txbuf.head : -1);
}

static struct kobj_attribute txhead =
__ATTR(txhead, 0644, (void *)show_txhead, NULL);

static ssize_t show_txtail(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sas_dev *sas = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", sas ? sas->txbuf.tail : -1);
}

static struct kobj_attribute txtail =
__ATTR(txtail, 0644, (void *)show_txtail, NULL);

static ssize_t show_rxmsgadd(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sas_dev *sas = dev_get_drvdata(dev);
	return sprintf(buf, "%d: [%d:%d]\n",
		sas ? sas->rxmsgadd : -1,
		sas ? sas->rxmsgs[sas->rxmsgadd].start : -1,
		sas ? sas->rxmsgs[sas->rxmsgadd].end : -1);
}

static struct kobj_attribute rxmsgadd =
__ATTR(rxmsgadd, 0644, (void *)show_rxmsgadd, NULL);

static ssize_t show_rxmsgtake(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sas_dev *sas = dev_get_drvdata(dev);
	return sprintf(buf, "%d: [%d:%d]\n",
		sas ? sas->rxmsgtake : -1,
		sas ? sas->rxmsgs[sas->rxmsgtake].start : -1,
		sas ? sas->rxmsgs[sas->rxmsgtake].end : -1);
}

static struct kobj_attribute rxmsgtake =
__ATTR(rxmsgtake, 0644, (void *)show_rxmsgtake, NULL);

static struct attribute *sas_attrs[] = {
	&ibdelay.attr,
	&rxhead.attr,
	&rxtail.attr,
	&txhead.attr,
	&txtail.attr,
	&rxmsgadd.attr,
	&rxmsgtake.attr,
	NULL,
};

static struct attribute_group sas_attr_grp = {
	.attrs = sas_attrs,
};

static int sas_of_probe(struct platform_device *pdev,
			struct device_node *np,
			struct sas_dev *dev)
{
	dev->irq = irq_of_parse_and_map(np, 0);
	if (of_property_read_u32(np, "baud", &dev->baud))
		return -EINVAL;
	if (of_property_read_u32(np, "interbyte_delay", &dev->interbyte_delay))
		return -EINVAL;
	if (of_property_read_u32(np, "rxbufsize", &dev->rxbufsize))
		return -EINVAL;
	dev->maxrxmsgs = dev->rxbufsize / 4;
	if ((dev->maxrxmsgs == 0) ||
	    (dev->maxrxmsgs & (dev->maxrxmsgs-1))) {
		dev_err(&pdev->dev,
			"maxrxmsgs %u must be a non-zero power of 2\n",
			dev->maxrxmsgs);
	}
	if (of_property_read_u32(np, "txbufsize", &dev->txbufsize))
		return -EINVAL;
	if (of_property_read_u32(np, "maxtxmsg", &dev->maxtxmsg))
		return -EINVAL;
	if (of_property_read_u32(np, "flush_on_mark", &dev->flush_on_mark))
		return -EINVAL;
	/* force power of two for transmit and receive buffers */
	if ((dev->rxbufsize & (dev->rxbufsize-1)) ||
	    (dev->rxbufsize == 0) ||
	    (dev->txbufsize & (dev->txbufsize-1)) ||
	    (dev->txbufsize == 0)) {
		dev_err(&pdev->dev,
			"rx/txbufsize must be a non-zero power of 2\n");
		return -EINVAL;
	}
	return 0;
}

static struct imx_sas_devdata imx51_sas_data  = {
	.umcr_reg = 0,
};

static struct imx_sas_devdata imx6_sas_data = {
	.umcr_reg = 0xb8,
};

static const struct of_device_id sas_ids[] = {
	{ .compatible = "boundary,imx51-sas", .data = &imx51_sas_data, },
	{ .compatible = "boundary,sas", .data = &imx6_sas_data },
	{ /* sentinel */ }
};

static int sas_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id = of_match_device(sas_ids, &pdev->dev);
	int result = 0;
	struct sas_dev *dev;
	struct device_node *np = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res;

	if (!np)
		return -ENODEV;

	dev = devm_kzalloc(&pdev->dev, sizeof(struct sas_dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "%s: alloc failure", DRIVER_NAME);
		result = -ENOMEM;
		goto fail_entry;
	}

	dev->pdev = pdev;
	cdev_init(&dev->cdev, &sas_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &sas_fops;

	if (of_id) {
		const struct imx_sas_devdata *pdata = of_id->data;

		if (pdata)
			dev->umcr_reg = pdata->umcr_reg;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "%s: IORESOURCE_MEM", DRIVER_NAME);
		result = -ENODEV;
		goto fail_entry;
	}

	base = devm_ioremap(&pdev->dev, res->start, PAGE_SIZE);
	if (!base) {
		dev_err(&pdev->dev, "%s: ioremap", DRIVER_NAME);
		result = -ENOMEM;
		goto fail_entry;
	}
	dev->base = base;

	dev->clk_ipg = devm_clk_get(&pdev->dev, "ipg");
	if (IS_ERR(dev->clk_ipg)) {
		result = PTR_ERR(dev->clk_ipg);
		dev_err(&pdev->dev, "failed to get ipg clk: %d\n", result);
		return result;
	}

	dev->clk_per = devm_clk_get(&pdev->dev, "per");
	if (IS_ERR(dev->clk_per)) {
		result = PTR_ERR(dev->clk_per);
		dev_err(&pdev->dev, "failed to get per clk: %d\n", result);
		return result;
	}

	if (0 != sas_of_probe(pdev, np, dev)) {
		dev_err(&pdev->dev, "Invalid dt spec\n");
		result = -ENODEV;
		goto fail_entry;
	}

	dev->rxbuf.buf = devm_kzalloc(&pdev->dev, dev->rxbufsize, GFP_KERNEL);
	if (!dev->rxbuf.buf) {
		dev_err(&pdev->dev, "%s: allocating rxbuf(%d)",
			DRIVER_NAME, dev->rxbufsize);
		goto fail_entry;
	}

	dev->rxmsgs = devm_kzalloc(&pdev->dev,
				   dev->maxrxmsgs * sizeof(dev->rxmsgs[0]),
				   GFP_KERNEL);
	if (!dev->rxmsgs) {
		dev_err(&pdev->dev, "%s: allocating rxmsgs(%d)",
			DRIVER_NAME, dev->maxrxmsgs);
		goto fail_entry;
	}

	dev->txbuf.buf = devm_kzalloc(&pdev->dev, dev->txbufsize, GFP_KERNEL);
	if (!dev->txbuf.buf) {
		dev_err(&pdev->dev, "%s: allocating txbuf(%d)",
			DRIVER_NAME, dev->txbufsize);
		goto fail_entry;
	}

	dev->txpbuf = devm_kzalloc(&pdev->dev,
				   (dev->txbufsize+7)/8,
				   GFP_KERNEL);
	if (!dev->txpbuf) {
		dev_err(&pdev->dev, "%s: allocating txbuf(%d)", DRIVER_NAME,
			(dev->txbufsize+7)/8);
		goto fail_entry;
	}

	mutex_init(&dev->rx_lock);
	mutex_init(&dev->tx_lock);

	init_waitqueue_head(&dev->queue);

	hrtimer_init(&dev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dev->timer.function = rx_timer;

	result = cdev_add(&dev->cdev, devnum, 1);
	if (result < 0) {
		dev_err(&pdev->dev, "%s: couldn't add device: err %d\n",
			DRIVER_NAME, result);
		goto fail_cdevadd;
	}

	result = sysfs_create_group(&pdev->dev.kobj, &sas_attr_grp);
	if (result) {
		pr_err("failed to create sysfs entries");
		goto fail_cdevadd;
	}

	spin_lock_init(&dev->lock);

	dev->chrdev = device_create(sas_class, &platform_bus,
				    devnum, NULL, "%s", DRIVER_NAME);
	platform_set_drvdata(pdev, dev);

	dev_dbg(&pdev->dev, "%s: uart_clk == %ld\n",
		DRIVER_NAME, clk_get_rate(dev->clk_per));
	dev_dbg(&pdev->dev, "%s: ipg_clk == %ld\n",
		DRIVER_NAME, clk_get_rate(dev->clk_per));
	dev_dbg(&pdev->dev, "%s: irq == %d\n",
		DRIVER_NAME, dev->irq);
	dev_dbg(&pdev->dev, "%s: rxirq == %d\n",
		DRIVER_NAME, dev->rxirq);
	dev_dbg(&pdev->dev, "%s: txirq == %d\n",
		DRIVER_NAME, dev->txirq);
	dev_dbg(&pdev->dev, "%s: baud  == %u\n",
		DRIVER_NAME, dev->baud);
	dev_dbg(&pdev->dev, "%s: ib_delay == %u ms\n",
		DRIVER_NAME, dev->interbyte_delay);
	dev_dbg(&pdev->dev, "%s: clks == %p/%p\n",
		DRIVER_NAME, dev->clk_ipg, dev->clk_per);
	dev_dbg(&pdev->dev, "%s: mem == %p\n",
		DRIVER_NAME, (void *)res->start);
	dev_dbg(&pdev->dev, "%s: rxbufsize == %u\n",
		DRIVER_NAME, dev->rxbufsize);
	dev_dbg(&pdev->dev, "%s: maxrxmsgs == %u\n",
		DRIVER_NAME, dev->maxrxmsgs);
	dev_dbg(&pdev->dev, "%s: txbufsize == %u\n",
		DRIVER_NAME, dev->txbufsize);
	dev_dbg(&pdev->dev, "%s: maxtxmsg == %u\n",
		DRIVER_NAME, dev->maxtxmsg);
	dev_dbg(&pdev->dev, "%s: sas_dev == %p\n",
		DRIVER_NAME, dev);
	dev_dbg(&pdev->dev, "%s: flush_on_mark == %d\n",
		DRIVER_NAME, dev->flush_on_mark);
	dev_dbg(&pdev->dev, "%s: umcr_reg == 0x%x\n",
		DRIVER_NAME, dev->umcr_reg);
	dev_info(&pdev->dev, "sas driver installed\n");
	return 0;

fail_cdevadd:
	devm_kfree(&pdev->dev, dev);
fail_entry:
	return result;
}

static int sas_remove(struct platform_device *pdev)
{
	struct sas_dev *dev = platform_get_drvdata(pdev);
	if (dev) {
		sysfs_remove_group(&pdev->dev.kobj, &sas_attr_grp);
		cdev_del(&dev->cdev);
	}

	dev_info(&pdev->dev, "sas driver released\n");
	return 0;
}

MODULE_DEVICE_TABLE(of, sas_ids);

static struct platform_driver sas_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sas_ids,
		},
	.probe = sas_probe,
	.remove = sas_remove,
};

static char *sas_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = S_IRUGO | S_IWUSR;
	return kasprintf(GFP_KERNEL, "%s", DRIVER_NAME);
}

static int __init sas_init(void)
{
	int result;

	/* Create a sysfs class. */
	sas_class = class_create(THIS_MODULE, "sas");
	if (IS_ERR(sas_class)) {
		result = PTR_ERR(sas_class);
		goto fail_class;
	}
	sas_class->devnode = sas_devnode;

	result = alloc_chrdev_region(&devnum, 0, 1, DRIVER_NAME);
	if (result < 0) {
		pr_err("%s: couldn't chrdevs: err %d\n",
		       DRIVER_NAME, result);
		goto fail_chrdev;
	}
	sas_major = MAJOR(devnum);

	result = platform_driver_register(&sas_driver);
	if (result < 0) {
		pr_err("%s: couldn't register driver, err %d\n",
		       DRIVER_NAME, result);
		goto fail_platform;
	}

	return 0;

fail_platform:
	unregister_chrdev_region(devnum, 1);

fail_chrdev:
	class_destroy(sas_class);
	sas_class = NULL;

fail_class:
	return result;
}

static void __exit sas_cleanup(void)
{
	platform_driver_unregister(&sas_driver);

	unregister_chrdev_region(devnum, 1);

	if (!IS_ERR(sas_class))
		class_destroy(sas_class);
}

module_init(sas_init);
module_exit(sas_cleanup);
MODULE_LICENSE("GPL");
