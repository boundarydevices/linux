/*
 *  Based on drivers/serial/pxa.c by Nicolas Pitre
 *
 *  Author:	Troy Kisky
 *  Copyright:	(C) 2016 Boundary Devices
 *
 */
#include <linux/circ_buf.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/serial_reg.h>
#include <linux/slab.h>
#include <linux/sysrq.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>


struct i2c_work;
struct uart_max7w_sc;

struct uart_max7w_chan {
	struct uart_port        port;
	struct uart_max7w_sc	*sc;
	int			transmitting;
	int			drop_single_char;
	char			*name;
	spinlock_t		tx_lock;
};

struct uart_max7w_sc {
	struct uart_max7w_chan	chan;
	struct gpio_desc	*reset_gpio;

	struct i2c_client 	*client;
	/* thread waits on this, irq wakes up */
	int			work_ready;
	wait_queue_head_t	work_waitq;
	struct completion	init_exit;	/* thread init done notification */
	int			irq;
	struct gpio_desc	*irq_gpio;
	int			startup_code;
	struct task_struct	*sc_thread;
	unsigned char		int_pio;
	unsigned char		stop;
};

static struct uart_max7w_sc *serial_max7w_sc;


static int i2c_read(struct uart_max7w_chan *ch,
			   u8 reg, u8 *buf, int len)
{
	struct i2c_msg msgs[2];
	int ret;

	msgs[0].flags = 0;
	msgs[0].addr  = ch->sc->client->addr;
	msgs[0].len   = 1;
	msgs[0].buf   = (u8 *)&reg;

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = ch->sc->client->addr;
	msgs[1].len   = len;
	msgs[1].buf   = buf;

	ret = i2c_transfer(ch->sc->client->adapter, msgs, 2);
	return ret < 0 ? ret : (ret != ARRAY_SIZE(msgs) ? -EIO : 0);
}

static int i2c_read_more(struct uart_max7w_chan *ch, u8 *buf, int len)
{
	struct i2c_msg msgs[1];
	int ret;

	msgs[0].flags = I2C_M_RD;
	msgs[0].addr  = ch->sc->client->addr;
	msgs[0].len   = len;
	msgs[0].buf   = buf;

	ret = i2c_transfer(ch->sc->client->adapter, msgs, 1);
	return ret < 0 ? ret : (ret != ARRAY_SIZE(msgs) ? -EIO : 0);
}

/**
 * i2c_write - write data to a register of the i2c slave device.
 *
 * @client: i2c device.
 * @reg: the register to write to.
 * @buf: raw data buffer to write.
 * @len: length of the buffer to write
 */
static int i2c_write(struct uart_max7w_chan *ch, const u8 *buf,
			    unsigned len)
{
	struct i2c_msg msg;
	int ret;

	if (len < 2)
		return -EIO;

	while (1) {
		spin_lock(&ch->tx_lock);
		if (!ch->transmitting) {
			ch->transmitting = 1;
			spin_unlock(&ch->tx_lock);
			break;
		}
		spin_unlock(&ch->tx_lock);
		msleep(2);
	}

	msg.flags = 0;
	msg.addr = ch->sc->client->addr;
	msg.buf = (u8 *)buf;
	msg.len = len;

	ret = i2c_transfer(ch->sc->client->adapter, &msg, 1);
	ch->transmitting = 0;
	if (ret)
		pr_err("%s: %d, len=%d, 0x%x %x\n", __func__, ret, len, buf[0], buf[1]);
	return ret < 0 ? ret : (ret != 1 ? -EIO : 0);
}

static void serial_max7w_enable_ms(struct uart_port *port)
{
}

static void serial_max7w_start_tx(struct uart_port *port)
{
	struct uart_max7w_chan *ch = (struct uart_max7w_chan *)port;
	struct uart_max7w_sc *sc = ch->sc;

	sc->work_ready = 1;
	wake_up(&sc->work_waitq);
}

static void serial_max7w_stop_tx(struct uart_port *port)
{
}

static void serial_max7w_stop_rx(struct uart_port *port)
{
}

static void serial_max7w_break_ctl(struct uart_port *port, int break_state)
{
}

static unsigned int serial_max7w_tx_empty(struct uart_port *port)
{
	struct uart_max7w_chan *ch = (struct uart_max7w_chan *)port;
	struct circ_buf *xmit = &ch->port.state->xmit;

	if (!uart_circ_empty(xmit))
		return 0;
	if (!ch->transmitting)
		return TIOCSER_TEMT;
	return 0;
}

static unsigned int serial_max7w_get_mctrl(struct uart_port *port)
{
	return  TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
}

static void serial_max7w_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static int serial_max7w_startup(struct uart_port *port)
{
	struct uart_max7w_chan *ch = (struct uart_max7w_chan *)port;
	struct uart_max7w_sc *sc = ch->sc;

	sc->work_ready = 1;
	wake_up(&sc->work_waitq);
	return 0;
}

static void serial_max7w_shutdown(struct uart_port *port)
{
}

static void
serial_max7w_set_termios(struct uart_port *port, struct ktermios *termios,
		       struct ktermios *old)
{
}

static void serial_max7w_pm(struct uart_port *port, unsigned int state,
		unsigned int oldstate)
{
}

static void serial_max7w_release_port(struct uart_port *port)
{
}

static int serial_max7w_request_port(struct uart_port *port)
{
	return 0;
}
#define PORT_MAX7W PORT_PXA

static void serial_max7w_config_port(struct uart_port *port, int flags)
{
	struct uart_max7w_chan *ch = (struct uart_max7w_chan *)port;
	ch->port.type = PORT_MAX7W;
}

static int
serial_max7w_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	/* we don't want the core code to modify any port params */
	return -EINVAL;
}

static const char *
serial_max7w_type(struct uart_port *port)
{
	struct uart_max7w_chan *ch = (struct uart_max7w_chan *)port;
	return ch->name;
}

struct uart_ops serial_max7w_pops = {
	.tx_empty	= serial_max7w_tx_empty,
	.set_mctrl	= serial_max7w_set_mctrl,
	.get_mctrl	= serial_max7w_get_mctrl,
	.stop_tx	= serial_max7w_stop_tx,
	.start_tx	= serial_max7w_start_tx,
	.stop_rx	= serial_max7w_stop_rx,
	.enable_ms	= serial_max7w_enable_ms,
	.break_ctl	= serial_max7w_break_ctl,
	.startup	= serial_max7w_startup,
	.shutdown	= serial_max7w_shutdown,
	.set_termios	= serial_max7w_set_termios,
	.pm		= serial_max7w_pm,
	.type		= serial_max7w_type,
	.release_port	= serial_max7w_release_port,
	.request_port	= serial_max7w_request_port,
	.config_port	= serial_max7w_config_port,
	.verify_port	= serial_max7w_verify_port,
};

static struct uart_driver serial_max7w_reg = {
	.owner		= THIS_MODULE,
	.driver_name	= "max7w serial",
	.dev_name	= "ttyGPS",
	.major		= TTY_MAJOR,
	.minor		= 74,
	.nr		= 1,
};
/*
 * ------------------------------------------------------------------
 */
static int receive_chars(struct uart_max7w_chan *ch)
{
	int max_count = 256;
	int cnt;
	int total = 0;
	int i;
	unsigned char buf[132];
	int ret = 0;

	ret = i2c_read(ch, 0xfd, buf, 2);
	if (ret)
		return ret;
	cnt = (buf[0] << 8) + buf[1];
	if (!cnt)
		return 0;
	if (cnt > max_count)
		cnt = max_count;

	while (cnt) {
		int c = cnt;
		if (c > 132)
			c = 128;
		ret = i2c_read_more(ch, buf, c);
		if (ret)
			break;
		ch->port.icount.rx++;

		if (ch->port.state->port.tty) {
			for (i = 0; i < c; i++) {
				if (!uart_handle_sysrq_char(&ch->port, buf[i]))
					uart_insert_char(&ch->port, 0, 0, buf[i], TTY_NORMAL);
			}
		} else {
			pr_err("%s: tty undefined, c=%d\n", __func__, c);
			print_hex_dump_bytes("gpsrx:", DUMP_PREFIX_NONE, buf, c);
		}

		cnt -= c;
		total += c;
	}
	if (ch->port.state->port.tty)
		tty_flip_buffer_push(&ch->port.state->port);
	else {
		pr_err("%s: tty undefined, total=%d\n", __func__, total);
	}
	return ret ? ret : total;
}

static int transmit_chars(struct uart_max7w_chan *ch)
{
	struct circ_buf *xmit;
	int count;
	int i = 0;
	unsigned char buf[68];

	xmit = &ch->port.state->xmit;
	count = uart_circ_chars_pending(xmit);
	if (count > 65)
		count = 64;
	ch->port.x_char = 0;
	if (uart_tx_stopped(&ch->port)) {
		count = 0;
	}
	if (count == 1) {
		count = 0;	/* cannot transmit 1 character */
		if (ch->drop_single_char)
			xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
	}
	while (count) {
		if (uart_circ_empty(xmit))
			break;
		buf[i++] = xmit->buf[xmit->tail];
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		count--;
	}
	if (i) {
		i2c_write(ch, buf, i);
		ch->port.icount.tx += i;
	}
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&ch->port);
	return i;
}

#define SPLIT32(a) (a) & 0xff, ((a) >> 8) & 0xff, ((a) >> 16) & 0xff, ((a) >> 24) & 0xff
#define SPLIT16(a) (a) & 0xff, ((a) >> 8) & 0xff

static const unsigned char uart_disable[] = {
	0x01,	/* uart */
	0x00,
	SPLIT16(0),	/* no txReady */

	SPLIT16(0x8d0),		/* 8 bit, no parity */
	SPLIT16(0),

	SPLIT32(9600),	/* baud */

	SPLIT16(0),	/* inProtoMask */
	SPLIT16(0),	/* outProtoMask */

	SPLIT16(0),	/* flags */
	SPLIT16(0),	/* reserved5 */
};


static unsigned char ddc_enable[] = {
	0x00,		/* ddc */
	0x00,
	SPLIT16(0),	/* txReady */

	SPLIT32(0x42 << 1),		/* mode, slave addr */
	SPLIT32(0),		/* reserved3 */

	SPLIT16(7),	/* inProtoMask */
	SPLIT16(3),	/* outProtoMask */

	SPLIT16(0),	/* flags */
	SPLIT16(0),	/* reserved5 */
};

static int send_ubx(struct uart_max7w_chan *ch, u16 class, const unsigned char *src, int length)
{
	unsigned char buf[68];
	unsigned char *p = buf;
	unsigned char ck_a = 0;
	unsigned char ck_b = 0;
	int l = length + 4;
	int ret;
	int cnt;

	if (length > (sizeof(buf) - 8))
		return -EINVAL;
	*p++ = 0xb5;
	*p++ = 0x62;
	*p++ = class & 0xff;
	*p++ = (class >> 8) & 0xff;
	*p++ = length & 0xff;
	*p++ = (length >> 8) & 0xff;
	memcpy(p, src, length);
	p -= 4;
	while (l) {
		ck_a += *p++;
		ck_b += ck_a;
		l--;
	}
	*p++ = ck_a;
	*p++ = ck_b;
	ret = i2c_write(ch, buf, length + 8);
	if (ret < 0)
		return ret;

	ret = i2c_read(ch, 0xfd, buf, 2);
	if (ret)
		return ret;
	cnt = (buf[0] << 8) + buf[1];

	while (cnt) {
		int c = cnt;
		if (c > sizeof(buf))
			c = sizeof(buf);
		ret = i2c_read_more(ch, buf, c);
		if (ret < 0)
			return ret;
		print_hex_dump_bytes("ubx:", DUMP_PREFIX_NONE, buf, c);
		cnt -= c;
	}
	return ret;
}

static int init_max7w(struct uart_max7w_chan *ch)
{
	int ret;
	ret = send_ubx(ch, 0x0006, uart_disable, sizeof(uart_disable));
	if (ret < 0)
		return ret;
	if (ch->sc->irq_gpio) {
		ddc_enable[2] = ch->sc->int_pio ?
			(1 << 7) | (ch->sc->int_pio << 2) |
			(gpiod_is_active_low(ch->sc->irq_gpio) ? 2 : 0) | 1 : 0;
	}
	ret = send_ubx(ch, 0x0006, ddc_enable, sizeof(ddc_enable));
	if (ret < 0)
		return ret;
	ret = send_ubx(ch, 0x0006, ddc_enable, 1);
	if (ret < 0)
		return ret;
	return ret;
}

static int max7w_thread(void *_sc)
{
	int ret;
	unsigned gp_int = 0;
	struct uart_max7w_sc *sc = _sc;
	struct uart_max7w_chan *ch;
	int poll_delay = (sc->irq_gpio) ? msecs_to_jiffies(30000) : msecs_to_jiffies(1000);

	struct sched_param param = { .sched_priority = 1 };
	sched_setscheduler(current, SCHED_FIFO, &param);

	ch = &sc->chan;
	ret = init_max7w(ch);
	if (ret < 0)
		goto exit;

	complete(&sc->init_exit);

	while (!sc->stop && !kthread_should_stop()) {
		int cnt;

		sc->work_ready = 0;
		if (!ch->port.state || !ch->port.state->port.tty) {
			wait_event_interruptible(sc->work_waitq, sc->work_ready);
			continue;
		}
		ret = receive_chars(ch);
		ch->drop_single_char = 0;
		transmit_chars(ch);
		cnt = uart_circ_chars_pending(&ch->port.state->xmit);
		if (cnt > 1)
			continue;
		if (cnt == 1) {
			ret = wait_event_interruptible_timeout(sc->work_waitq,
				sc->work_ready, msecs_to_jiffies(100));
			if (!ret) {
				/* timed out */
				ch->drop_single_char = 1;
				transmit_chars(ch);
			}
			continue;
		}
		if (ret == 256)
			continue;
		if (sc->irq_gpio)
			gp_int = gpiod_get_value(sc->irq_gpio);
		if (!gp_int) {
			/* no interrupt pending */
			wait_event_interruptible_timeout(sc->work_waitq,
				sc->work_ready, poll_delay);
			continue;
		}
	}
	ret = 1;
exit:
	sc->startup_code = ret;
	complete(&sc->init_exit);
	return ret;
}

static irqreturn_t max7w_interrupt(int irq, void *id)
{
	struct uart_max7w_sc *sc = id;
	sc->work_ready = 1;
	wake_up(&sc->work_waitq);
	return IRQ_HANDLED;
}

static const char *client_name = "max7w-uart";

static int max7w_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	struct uart_max7w_sc *sc;
	struct uart_max7w_chan *ch;
	struct device *dev = &client->dev;
        struct device_node *np = client->dev.of_node;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *irq_gpio;
	unsigned int_pio = 0;

	printk(KERN_ERR "%s: %s version 1.0\n", __func__, client_name);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C check functionality failed.\n");
		return -ENXIO;
	}

	sc = devm_kzalloc(&client->dev, sizeof(*sc), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->client = client;

	init_waitqueue_head(&sc->work_waitq);
	sc->client = client;
	sc->irq = client->irq ;

	irq_gpio = devm_gpiod_get_index(dev, "wakeup", 0, GPIOD_IN);
	if (IS_ERR(irq_gpio))
		irq_gpio = NULL;

	if (irq_gpio) {
		of_property_read_u32(np, "int-pio", &int_pio);
		sc->int_pio = int_pio;
		if (!int_pio)
			irq_gpio = NULL;
	}
	if (irq_gpio) {
		pr_info("%s:int_pio=%d, irq_gpio=%d active_low=%d\n", __func__,
			int_pio, desc_to_gpio(irq_gpio),
			gpiod_is_active_low(irq_gpio));
	}
	reset_gpio = devm_gpiod_get_index(dev, "reset", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(reset_gpio)) {
		ret = PTR_ERR(reset_gpio);
		dev_err(&client->dev, "reset failed: %d\n", ret);
		goto out1;
	}
	dev_err(&client->dev, "reset is %d\n", desc_to_gpio(reset_gpio));
	sc->irq_gpio = irq_gpio;
	sc->reset_gpio = reset_gpio;

	init_completion(&sc->init_exit);

	ch = &sc->chan;
	ch->port.type = PORT_MAX7W;
	ch->port.iotype = UPIO_MEM;
	/* needed to pass serial_core check for existence */
	ch->port.mapbase = 0xfffa;
	ch->port.fifosize = 128;
	ch->port.ops = &serial_max7w_pops;
	ch->port.line = 0;
	ch->port.dev = dev;
	ch->port.flags = UPF_BOOT_AUTOCONF;
	ch->port.uartclk = 3686400;
	ch->sc = sc;
	ch->name = "UARTGPS";

	serial_max7w_sc = sc;

	if (irq_gpio) {
		ret = request_irq(sc->irq, &max7w_interrupt,
			  gpiod_is_active_low(irq_gpio) ?
			  IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING,
			  client_name, sc);
		if (ret) {
			pr_err("%s: request_irq failed, irq:%i\n",
					client_name,sc->irq);
			goto out1;
		}
	}
	i2c_set_clientdata(client, sc);

	if (!IS_ERR(reset_gpio)) {
		gpiod_set_value(reset_gpio, 0);
		msleep(100);
	}
	init_completion(&sc->init_exit);
	sc->sc_thread = kthread_run(max7w_thread, sc, "max7wd");
	if (ret < 0)
		goto out2;
	wait_for_completion(&sc->init_exit);	/* wait for thread to Start */
	ret = sc->startup_code;
	if (ret < 0)
		goto out2;

	ch = &sc->chan;
	uart_add_one_port(&serial_max7w_reg, &ch->port);

	pr_err("%s: succeeded\n", __func__);
	return 0;
out2:
	if (!IS_ERR(reset_gpio))
		gpiod_set_value(reset_gpio, 1);
	free_irq(sc->irq, sc);
out1:
	pr_err("%s: failed %i\n", __func__, ret);
	return ret;
}

static int max7w_remove(struct i2c_client *client)
{
	struct uart_max7w_sc *sc = i2c_get_clientdata(client);
	sc->stop = 1;
	sc->work_ready = 1;
	wake_up(&sc->work_waitq);
	kthread_stop(sc->sc_thread);

	uart_remove_one_port(&serial_max7w_reg, &sc->chan.port);
	free_irq(sc->irq, sc);
	if (sc->reset_gpio)
		gpiod_set_value(sc->reset_gpio, 1);
	return 0;
}

/*-----------------------------------------------------------------------*/

static const struct i2c_device_id max7w_idtable[] = {
	{ "max-7w", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, max7w_idtable);

static struct i2c_driver max7w_driver = {
	.driver = {
		.name		= "max-7w",
	},
	.id_table	= max7w_idtable,
	.probe		= max7w_probe,
	.remove		= max7w_remove,
};

static int __init max7w_init(void)
{
	int ret = uart_register_driver(&serial_max7w_reg);
	if (ret != 0)
		return ret;
	if ((ret = i2c_add_driver(&max7w_driver))) {
		uart_unregister_driver(&serial_max7w_reg);
		pr_warning("%s: i2c_add_driver failed\n",client_name);
		return ret;
	}
	pr_err("%s: version 1.0\n",client_name);
	return 0;
}

static void __exit max7w_exit(void)
{
	i2c_del_driver(&max7w_driver);
	uart_unregister_driver(&serial_max7w_reg);
}

module_init(max7w_init);
module_exit(max7w_exit);

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("I2C gps max7w serial port");
MODULE_LICENSE("GPL");
