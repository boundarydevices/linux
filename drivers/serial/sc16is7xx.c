/*
 *  Based on drivers/serial/pxa.c by Nicolas Pitre
 *
 *  Author:	Troy Kisky
 *  Copyright:	(C) 2011 Boundary Devices
 *
 */

#if defined(CONFIG_SERIAL_SC16IS7XX_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/circ_buf.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kthread.h>
#include <linux/mfd/core.h>
#include <linux/mfd/sc16is7xx-reg.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/serial_reg.h>
#include <linux/slab.h>
#include <linux/sysrq.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include "sc16is7xx.h"

static struct uart_sc16is7xx_sc *serial_sc16is7xx_sc;

static const unsigned short reg_map[] = {
/* always accessible */
[SC_LCR] =	3,
/* LCR != 0xBF, DLAB = 0 */
[SC_RHR] =	(0 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0) + _RO + _V),
[SC_THR] =	(0 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0) + _WO + _V),
[SC_IER] =	(1 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 0)),

/* LCR != 0xBF, DLAB = 1 */
[SC_DLL] =	(0 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 1)),
[SC_DLH] =	(1 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_DLAB, 1)),

/* LCR != 0xBF */
[SC_IIR] =	(2 + A_TYPE(SP_0xBF, 0) + _RO + _V),
[SC_FCR] =	(2 + A_TYPE(SP_0xBF, 0) + _WO + _V),
[SC_MCR] =	(4 + A_TYPE(SP_0xBF, 0)),
[SC_LSR] =	(5 + A_TYPE(SP_0xBF, 0) + _RO + _V),
[SC_TXLVL] =	(8 + A_TYPE(SP_0xBF, 0) + _RO + _V),
[SC_RXLVL] =	(9 + A_TYPE(SP_0xBF, 0) + _RO + _V),

[SC_IODIR] =	 (0x0A + A_TYPE(SP_0xBF, 0)),
[SC_IOSTATE_R] = (0x0B + A_TYPE(SP_0xBF, 0) + _RO + _V),
[SC_IOSTATE_W] = (0x0B + A_TYPE(SP_0xBF, 0) + _WO),
[SC_IOINTENA] =  (0x0C + A_TYPE(SP_0xBF, 0)),
[SC_IOCONTROL] = (0x0E + A_TYPE(SP_0xBF, 0)),
[SC_EFCR] =	 (0x0F + A_TYPE(SP_0xBF, 0)),

/*  LCR != 0xBF,  EFR[4]=0 ||  MCR[2]=0 */
[SC_MSR] =	(6 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_MCR2, 0) + _RO + _V),
[SC_SPR] =	(7 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_MCR2, 0)),

/*  LCR != 0xBF,  EFR[4]=1 &&  MCR[2]=1 */
[SC_TCR] =	(6 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_MCR2, 1)),
[SC_TLR] =	(7 + A_TYPE(SP_0xBF, 0) + A_TYPE(SP_MCR2, 1)),

/* LCR == 0xBF */
[SC_EFR] =	(2 + A_TYPE(SP_0xBF, 1)),
[SC_XON1] =	(4 + A_TYPE(SP_0xBF, 1)),
[SC_XON2] =	(5 + A_TYPE(SP_0xBF, 1)),
[SC_XOFF1] =	(6 + A_TYPE(SP_0xBF, 1)),
[SC_XOFF2] =	(7 + A_TYPE(SP_0xBF, 1)),
};

static int serial_in(struct uart_sc16is7xx_chan *ch, enum sc_reg reg)
{
	unsigned mreg;
	if (reg > ARRAY_SIZE(reg_map))
		return -EINVAL;
	mreg = reg_map[reg];
	if (mreg & _V)
		if (reg != SC_MSR)
			pr_err("%s: error reading volatile register %i\n",
					__func__, reg);

	if (reg < SC_CHAN_REG_CNT)
		return ch->reg_cache[reg];
	else
		return ch->sc->dev_cache[reg - SC_CHAN_REG_CNT];
}

struct i2c_work* get_work_entry(struct uart_sc16is7xx_sc *sc)
{
	struct i2c_work* w;
	spin_lock(&sc->work_free_lock);
	w = sc->work_free;
	if (w) {
		sc->work_free = w->next;
		w->next = NULL;
	}
	spin_unlock(&sc->work_free_lock);
	if (!w) {
		pr_err("%s: empty!!\n", __func__);
		sc->work_ready = 1;
		wake_up(&sc->work_waitq);
	}
	return w;
}

void free_work_entry(struct uart_sc16is7xx_sc *sc, struct i2c_work* w)
{
	if (w) {
		spin_lock(&sc->work_free_lock);
		w->next = sc->work_free;
		sc->work_free = w;
		spin_unlock(&sc->work_free_lock);
	}
}

void sc_queue_work(struct uart_sc16is7xx_chan *ch, struct i2c_work* w)
{
	int wake = 0;
	unsigned reg;
	if (!w)
		return;
	reg = w->reg;
	spin_lock(&ch->work_pending_lock);
	if ((w->mode == SCM_WRITE) && !(reg_map[reg] & _V)) {
		/*
		 * The reg_cache changes when queued for non-volatiles,
		 * and when actually written for volatiles.
		 */
		if (reg < SC_CHAN_REG_CNT)
			ch->reg_cache[reg] = w->val;
		else
			ch->sc->dev_cache[reg - SC_CHAN_REG_CNT] = w->val;
	}

	if (!ch->pending_tail) {
		ch->pending_head = w;
		wake = 1;
	} else {
		ch->pending_tail->next = w;
	}
	ch->pending_tail = w;
	spin_unlock(&ch->work_pending_lock);
	if (wake) {
		ch->sc->work_ready = 1;
		wake_up(&ch->sc->work_waitq);
	}
}

static int serial_out(struct uart_sc16is7xx_chan *ch, enum sc_reg reg, unsigned value)
{
	struct i2c_work* w;
	unsigned mreg;
	if (reg > ARRAY_SIZE(reg_map))
		return -EINVAL;
	mreg = reg_map[reg];
	if (!(mreg & _V)) {
		unsigned v;
		if (reg < SC_CHAN_REG_CNT)
			v = ch->reg_cache[reg];
		else
			v = ch->sc->dev_cache[reg - SC_CHAN_REG_CNT];
		if (v == value)
			return 0;
		/* The reg_cache changes when queued for non-volatiles */
	}

	w = get_work_entry(ch->sc);
	if (!w)
		return -ENOMEM;
	w->mode = SCM_WRITE;
	w->reg = reg;
	w->state = F_PENDING;
	w->cnt = 1;
	w->channel_no = ch->channel_no;
	w->val = (unsigned char)value;
	sc_queue_work(ch, w);
	return 1;
}

static int serial_modify(struct uart_sc16is7xx_chan *ch, enum sc_reg reg, unsigned char clear_mask, unsigned char set_mask)
{
	int val = serial_in(ch, reg);
	if (val < 0)
		return val;
	val &= ~clear_mask;
	val |= set_mask;
	return serial_out(ch, reg, val);
}

static int serial_modify_ier(struct uart_sc16is7xx_chan *ch, unsigned char clear_mask, unsigned char set_mask)
{
	int val;
	int wake = 0;
	spin_lock(&ch->work_pending_lock);
	val = ch->new_ier;
	val &= ~clear_mask;
	val |= set_mask;
	if (ch->new_ier != val) {
		unsigned change = (ch->new_ier ^ val);
		if (change & 0xf0) {
			serial_modify(ch, SC_EFR, 0,  1 << 4);	//needed to allow writes
		}
		ch->new_ier = val;
		wake = 1;
	}
	spin_unlock(&ch->work_pending_lock);
	if (wake) {
		ch->sc->work_ready = 1;
		wake_up(&ch->sc->work_waitq);
	}
	return wake;
}

static void serial_sc16is7xx_enable_ms(struct uart_port *port)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	serial_modify_ier(ch, 0, UART_IER_MSI);
}

static void serial_sc16is7xx_start_tx(struct uart_port *port)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	serial_modify_ier(ch, 0, UART_IER_THRI);
}

static void serial_sc16is7xx_stop_tx(struct uart_port *port)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
#ifdef CONFIG_SERIAL_SC16IS7XX_CONSOLE
	if (ch->console_insert != ch->console_remove)
		return;
#endif
	serial_modify_ier(ch, UART_IER_THRI, 0);
}

static void serial_sc16is7xx_stop_rx(struct uart_port *port)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	ch->port.read_status_mask &= ~UART_LSR_DR;
	serial_modify_ier(ch, UART_IER_RLSI, 0);
}

static void serial_sc16is7xx_break_ctl(struct uart_port *port, int break_state)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	serial_modify(ch, SC_LCR, UART_LCR_SBC, (break_state == -1) ? UART_LCR_SBC : 0);
}

static unsigned int serial_sc16is7xx_tx_empty(struct uart_port *port)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	if (ch->tx_empty)
		return TIOCSER_TEMT;
	ch->check_tx_empty = 1;
	ch->sc->work_ready = 1;
	wake_up(&ch->sc->work_waitq);
	return 0;
}

static unsigned int serial_sc16is7xx_get_mctrl(struct uart_port *port)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	unsigned char status;
	unsigned int ret;

	/* hopefully, modem change interrupts are keeping msr up to date */
	status = serial_in(ch, SC_MSR);

	ret = 0;
	if (status & UART_MSR_DCD)
		ret |= TIOCM_CAR;
	if (status & UART_MSR_RI)
		ret |= TIOCM_RNG;
	if (status & UART_MSR_DSR)
		ret |= TIOCM_DSR;
	if (status & UART_MSR_CTS)
		ret |= TIOCM_CTS;
	return ret;
}

static void serial_sc16is7xx_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	unsigned char mcr = 0;

	if (mctrl & TIOCM_RTS)
		mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		mcr |= UART_MCR_LOOP;
	serial_out(ch, SC_MCR, mcr);
}

static int serial_sc16is7xx_startup(struct uart_port *port)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	unsigned long flags;
	unsigned ier;

	/*
	 * Clear the FIFO buffers and disable them.
	 * (they will be reenabled in set_termios())
	 */
	serial_out(ch, SC_FCR, UART_FCR_ENABLE_FIFO);
	serial_out(ch, SC_FCR, UART_FCR_ENABLE_FIFO |
			UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	serial_out(ch, SC_FCR, 0);

	/*
	 * Now, initialize the UART
	 */
	serial_out(ch, SC_LCR, UART_LCR_WLEN8);

	spin_lock_irqsave(&ch->port.lock, flags);
	ch->port.mctrl |= TIOCM_OUT2;
	serial_sc16is7xx_set_mctrl(&ch->port, ch->port.mctrl);
	spin_unlock_irqrestore(&ch->port.lock, flags);

	/*
	 * Finally, enable interrupts.  Note: Modem status interrupts
	 * are set via set_termios(), which will be occurring imminently
	 * anyway, so we don't enable them here.
	 */
	ier = UART_IER_RLSI | UART_IER_RDI;
	serial_modify_ier(ch, ~ier, ier);
	return 0;
}

static void serial_sc16is7xx_shutdown(struct uart_port *port)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	unsigned long flags;

	/*
	 * Disable interrupts from this port
	 */
	serial_modify_ier(ch, 0xff, 0);

	spin_lock_irqsave(&ch->port.lock, flags);
	ch->port.mctrl &= ~TIOCM_OUT2;
	serial_sc16is7xx_set_mctrl(&ch->port, ch->port.mctrl);
	spin_unlock_irqrestore(&ch->port.lock, flags);

	/*
	 * Disable break condition and FIFOs
	 */
	serial_modify(ch, SC_LCR, UART_LCR_SBC, 0);
	serial_out(ch, SC_FCR, UART_FCR_ENABLE_FIFO |
				  UART_FCR_CLEAR_RCVR |
				  UART_FCR_CLEAR_XMIT);
	serial_out(ch, SC_FCR, 0);
}

static void
serial_sc16is7xx_set_termios(struct uart_port *port, struct ktermios *termios,
		       struct ktermios *old)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	unsigned char cval, rxlvl;
	unsigned char efr = 0x10;
	unsigned long flags;
	unsigned int baud, quot;

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		cval = UART_LCR_WLEN5;
		break;
	case CS6:
		cval = UART_LCR_WLEN6;
		break;
	case CS7:
		cval = UART_LCR_WLEN7;
		break;
	default:
	case CS8:
		cval = UART_LCR_WLEN8;
		break;
	}

	if (termios->c_cflag & CSTOPB)
		cval |= UART_LCR_STOP;
	if (termios->c_cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(termios->c_cflag & PARODD))
		cval |= UART_LCR_EPAR;

	baud = tty_termios_baud_rate(termios);
	/*
	 * Ask the core to calculate the divisor for us.
	 */
	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk/16);
	quot = uart_get_divisor(port, baud);

	if ((ch->port.uartclk / quot) <= (9600 * 16))
		rxlvl = 1;
	else if ((ch->port.uartclk / quot) <= (57600 * 16))
		rxlvl = 8;
	else
		rxlvl = 32;

	/*
	 * Ok, we're now changing the port state.  Do it with
	 * interrupts disabled.
	 */
	spin_lock_irqsave(&ch->port.lock, flags);

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	ch->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		ch->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		ch->port.read_status_mask |= UART_LSR_BI;

	/*
	 * Characters to ignore
	 */
	ch->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		ch->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		ch->port.ignore_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			ch->port.ignore_status_mask |= UART_LSR_OE;
	}

	/*
	 * ignore all characters if CREAD is not set
	 */
	if ((termios->c_cflag & CREAD) == 0)
		ch->port.ignore_status_mask |= UART_LSR_DR;

	/*
	 * CTS flow control flag and modem status interrupts
	 */
	if (UART_ENABLE_MS(&ch->port, termios->c_cflag))
		serial_modify_ier(ch, 0, UART_IER_MSI);
	else
		serial_modify_ier(ch, UART_IER_MSI, 0);


	serial_out(ch, SC_DLL, quot & 0xff);		/* LS of divisor */
	serial_out(ch, SC_DLH, quot >> 8);		/* MS of divisor */
	serial_sc16is7xx_set_mctrl(&ch->port, ch->port.mctrl);
	serial_out(ch, SC_EFR, efr);
	serial_out(ch, SC_FCR, UART_FCR_ENABLE_FIFO);
#define HALT_RESUME(halt, resume) (((halt)/4) | (((resume)/4) << 4))
#define TRIG_LVL_TX_RX(txlvl, rxlvl) (((txlvl)/4) | (((rxlvl)/4) << 4))
	serial_out(ch, SC_TCR,HALT_RESUME(60, 4));
	serial_out(ch, SC_TLR,TRIG_LVL_TX_RX(32, rxlvl));
	if (termios->c_cflag & CRTSCTS) {
		efr |= 0x80;
		serial_out(ch, SC_EFR, efr);
	}
	spin_unlock_irqrestore(&ch->port.lock, flags);
}

static void serial_sc16is7xx_pm(struct uart_port *port, unsigned int state,
		unsigned int oldstate)
{
}

static void serial_sc16is7xx_release_port(struct uart_port *port)
{
}

static int serial_sc16is7xx_request_port(struct uart_port *port)
{
	return 0;
}
#define PORT_SC16IS7XX PORT_PXA

static void serial_sc16is7xx_config_port(struct uart_port *port, int flags)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	ch->port.type = PORT_SC16IS7XX;
}

static int
serial_sc16is7xx_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	/* we don't want the core code to modify any port params */
	return -EINVAL;
}

static const char *
serial_sc16is7xx_type(struct uart_port *port)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	return ch->name;
}

static struct uart_driver serial_sc16is7xx_reg;

#ifdef CONFIG_SERIAL_SC16IS7XX_CONSOLE


static void serial_sc16is7xx_console_putchar(struct uart_port *port, int char1)
{
	struct uart_sc16is7xx_chan *ch = (struct uart_sc16is7xx_chan *)port;
	short insert = ch->console_insert;
	ch->console_buffer[insert++] = char1;
	if (insert >= ARRAY_SIZE(ch->console_buffer))
		insert = 0;
	if (insert != ch->console_remove)
		ch->console_insert = insert;
}

/*
 * Print a string to the serial port trying not to disturb
 * any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void
serial_sc16is7xx_console_write(struct console *co, const char *s, unsigned int count)
{
	struct uart_sc16is7xx_chan *ch = &serial_sc16is7xx_sc->chan[co->index];
	uart_console_write(&ch->port, s, count, serial_sc16is7xx_console_putchar);
//	serial_modify_ier(ch, 0, UART_IER_THRI);
}

static int __init
serial_sc16is7xx_console_setup(struct console *co, char *options)
{
	struct uart_sc16is7xx_sc *sc;
	struct uart_sc16is7xx_chan *ch;
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index == -1 || co->index >= serial_sc16is7xx_reg.nr)
		co->index = 0;
	sc = serial_sc16is7xx_sc;
	if (!sc)
		return -ENODEV;
	ch = &sc->chan[co->index];

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	serial_sc16is7xx_startup(&ch->port);
	return uart_set_options(&ch->port, co, baud, parity, bits, flow);
}

static struct console serial_sc16is7xx_console = {
	.name		= "ttySC",
	.write		= serial_sc16is7xx_console_write,
	.device		= uart_console_device,
	.setup		= serial_sc16is7xx_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &serial_sc16is7xx_reg,
};

#define SC16IS7XX_CONSOLE	&serial_sc16is7xx_console
#else
#define SC16IS7XX_CONSOLE	NULL
#endif

struct uart_ops serial_sc16is7xx_pops = {
	.tx_empty	= serial_sc16is7xx_tx_empty,
	.set_mctrl	= serial_sc16is7xx_set_mctrl,
	.get_mctrl	= serial_sc16is7xx_get_mctrl,
	.stop_tx	= serial_sc16is7xx_stop_tx,
	.start_tx	= serial_sc16is7xx_start_tx,
	.stop_rx	= serial_sc16is7xx_stop_rx,
	.enable_ms	= serial_sc16is7xx_enable_ms,
	.break_ctl	= serial_sc16is7xx_break_ctl,
	.startup	= serial_sc16is7xx_startup,
	.shutdown	= serial_sc16is7xx_shutdown,
	.set_termios	= serial_sc16is7xx_set_termios,
	.pm		= serial_sc16is7xx_pm,
	.type		= serial_sc16is7xx_type,
	.release_port	= serial_sc16is7xx_release_port,
	.request_port	= serial_sc16is7xx_request_port,
	.config_port	= serial_sc16is7xx_config_port,
	.verify_port	= serial_sc16is7xx_verify_port,
};

static struct uart_driver serial_sc16is7xx_reg = {
	.owner		= THIS_MODULE,
	.driver_name	= "sc16is7xx serial",
	.dev_name	= "ttySC",
	.major		= TTY_MAJOR,
	.minor		= 68,
	.nr		= 2,
	.cons		= SC16IS7XX_CONSOLE,
};
/*
 * ------------------------------------------------------------------
 */
static int sc_select(struct uart_sc16is7xx_chan *ch, enum sc_reg reg)
{
	int ret = 0;
	unsigned mreg;
	if (reg > ARRAY_SIZE(reg_map))
		return -EINVAL;
	mreg = reg_map[reg];
	if (mreg & A_TYPE(SP_0xBF, 1)) {
		/* LCR ==  or != 0xbf matters */
		unsigned char lcr;
		unsigned char mcr;
		unsigned char efr;
		if (mreg & A_TYPEV(SP_MCR2)) {
			/* efr[4] should be 1 as well */
			efr = ch->reg_cache[SC_EFR] | 0x10;
			if (ch->actual_efr != efr) {
				ret = sc_select(ch, SC_EFR);
				if (ret)
					return ret;
				ret = i2c_smbus_write_byte_data(ch->sc->client,
					MAKE_SUBADDRESS(ch->channel_no, UART_EFR),
					efr);
				if (ret)
					return ret;
				ch->actual_efr = efr;
			}
		}
		if (mreg & A_TYPE(SP_MCR2, 1)) {
			/* mcr[2] matters */
			mcr = ch->reg_cache[SC_MCR] & ~0x4;
			if (mreg & A_TYPEV(SP_MCR2))
				mcr |= 4;
			if (ch->actual_mcr != mcr) {
				ret = sc_select(ch, SC_MCR);
				if (ret)
					return ret;
				ret = i2c_smbus_write_byte_data(ch->sc->client,
					MAKE_SUBADDRESS(ch->channel_no, UART_MCR),
					mcr);
				if (ret)
					return ret;
				ch->actual_mcr = mcr;
			}
		}
		lcr = (mreg & A_TYPEV(SP_0xBF)) ? 0xbf : ch->reg_cache[SC_LCR];
		if (mreg & A_TYPE(SP_DLAB, 1)) {
			if (mreg & A_TYPEV(SP_DLAB))
				lcr |= 0x80;
			else
				lcr &= ~0x80;
		}
		if (ch->actual_lcr != lcr) {
			ret = i2c_smbus_write_byte_data(ch->sc->client,
				MAKE_SUBADDRESS(ch->channel_no, UART_LCR),
				lcr);
			if (ret)
				return ret;
			ch->actual_lcr = lcr;
		}
	}
	return ret;
}

static int sc_serial_out_cnt(struct uart_sc16is7xx_chan *ch, enum sc_reg reg, unsigned char* buf, int count)
{
	int ret;
	unsigned sub_address;
	unsigned char value = buf[count - 1];
	unsigned mreg;
	int retry = 0;
	if (reg > ARRAY_SIZE(reg_map))
		return -EINVAL;
	mreg = reg_map[reg];
	sub_address = MAKE_SUBADDRESS(ch->channel_no, mreg);
	do {
		ret = sc_select(ch, reg);
		if (!ret) {
			ret = i2c_smbus_write_i2c_block_data(ch->sc->client,
					sub_address, count, buf);
			if (!ret)
				break;
		}
	} while (retry++ < 3);

	if (ret) {
		pr_info("%s: error %i\n", __func__, ret);
		return ret;
	}
	if (reg < SC_CHAN_REG_CNT)
		buf = &ch->reg_cache[reg];
	else
		buf = &ch->sc->dev_cache[reg - SC_CHAN_REG_CNT];

	if (reg == SC_LCR)
		ch->actual_lcr = value;
	if (reg == SC_MCR)
		ch->actual_mcr = value;
	if (reg == SC_EFR)
		ch->actual_efr = value;
	if (mreg & _V)
		*buf = value;
	return ret;
}

static int sc_serial_out(struct uart_sc16is7xx_chan *ch, enum sc_reg reg, unsigned char value)
{
	return sc_serial_out_cnt(ch, reg, &value, 1);
}

static int sc_serial_in_cnt(struct uart_sc16is7xx_chan *ch, enum sc_reg reg,
		unsigned char * buf, int count)
{
	unsigned sub_address;
	int val = 0;
	int ret = 0;
	int retry = 0;
	if (reg > ARRAY_SIZE(reg_map))
		return -EINVAL;
	sub_address = MAKE_SUBADDRESS(ch->channel_no, reg_map[reg]);
	do {
		ret = sc_select(ch, reg);
		if (!ret) {
			ret = i2c_smbus_read_i2c_block_data(ch->sc->client,
					sub_address, count, buf);
			if (ret == count)
				break;
			if (ret > 0) {
				pr_info("%s: error %i of %i bytes read\n",
						__func__, ret, count);
				if (ret > count)
					ret = count;
				break;
			}
		}
	} while (retry++ < 3);

	if (ret <= 0) {
		pr_info("%s: error %i, count=%i\n", __func__, ret, count);
		return ret;
	}

	val = buf[ret - 1];
	if (reg == SC_LCR)
		ch->actual_lcr = val;
	else if (reg == SC_MCR)
		ch->actual_mcr = val;
	else if (reg == SC_EFR)
		ch->actual_efr = val;
	else if (reg == SC_LSR)
		ch->tx_empty = val & UART_LSR_TEMT;
	if (reg < SC_CHAN_REG_CNT)
		buf = &ch->reg_cache[reg];
	else
		buf = &ch->sc->dev_cache[reg - SC_CHAN_REG_CNT];
	*buf = val;
	return ret;
}

static int sc_serial_in_always(struct uart_sc16is7xx_chan *ch, enum sc_reg reg)
{
	unsigned char buf[4];
	int ret = sc_serial_in_cnt(ch, reg, buf, 1);
	if (ret <= 0)
		return ret;
	return buf[0];
}

static int sc_serial_in(struct uart_sc16is7xx_chan *ch, enum sc_reg reg)
{
	if (reg > ARRAY_SIZE(reg_map))
		return -EINVAL;
	if (reg_map[reg] & _V)
		return sc_serial_in_always(ch, reg);

	if (reg < SC_CHAN_REG_CNT)
		return ch->reg_cache[reg];
	return ch->sc->dev_cache[reg - SC_CHAN_REG_CNT];
}

static int receive_chars(struct uart_sc16is7xx_chan *ch)
{
	int rxlvl;
	struct tty_struct *tty = ch->port.state->port.tty;
	unsigned flag;
	int max_count = 256;
	int cnt;
	int total = 0;
	int i;
	unsigned char buf[64];

	rxlvl = sc_serial_in(ch, SC_RXLVL);
	if ((rxlvl < 0) || (rxlvl > 64))
		rxlvl = 64;
	while (rxlvl) {
		int lsr = sc_serial_in(ch, SC_LSR);
		if (lsr < 0)
			lsr = 0;
		if (!(lsr & UART_LSR_DR))
			break;
		if (lsr & (0x80 | UART_LSR_BI)) {
			int char1;
			char1 = sc_serial_in(ch, SC_RHR);
			if (char1 < 0)
				char1 = 0;
			buf[0] = char1;
			cnt = 1;
		} else {
			cnt = rxlvl;
			if (cnt > max_count)
				cnt = max_count;
			/*
			 * This chip doesn't like to read more than 32 bytes
			 * from the fifo
			 */
			if (cnt > 32)
				cnt = 32;
			cnt = sc_serial_in_cnt(ch, SC_RHR, buf, cnt);
		}
		flag = TTY_NORMAL;
		ch->port.icount.rx++;

		if (unlikely(lsr & (UART_LSR_BI | UART_LSR_PE |
				       UART_LSR_FE | UART_LSR_OE))) {
			/*
			 * For statistics only
			 */
			if (lsr & UART_LSR_BI) {
				lsr &= ~(UART_LSR_FE | UART_LSR_PE);
				ch->port.icount.brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				if (uart_handle_break(&ch->port))
					goto break_done;
			} else if (lsr & UART_LSR_PE)
				ch->port.icount.parity++;
			else if (lsr & UART_LSR_FE)
				ch->port.icount.frame++;
			if (lsr & UART_LSR_OE)
				ch->port.icount.overrun++;

			/*
			 * Mask off conditions which should be ignored.
			 */
			lsr &= ch->port.read_status_mask;

			if (lsr & UART_LSR_BI) {
				flag = TTY_BREAK;
			} else if (lsr & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (lsr & UART_LSR_FE)
				flag = TTY_FRAME;
		}
		if (ch->port.state->port.tty) {
			for (i = 0; i < cnt; i++) {
				if (!uart_handle_sysrq_char(&ch->port, buf[i]))
					uart_insert_char(&ch->port, lsr, UART_LSR_OE, buf[i], flag);
				lsr = 0;	/* overrun only valid once */
			}
		} else {
			pr_err("%s: tty undefined %i\n", __func__, ch->channel_no);
		}
break_done:
		rxlvl -= cnt;
		max_count -= cnt;
		total += cnt;
		if (max_count < 0)
			break;
	}
	if (ch->port.state->port.tty)
		tty_flip_buffer_push(tty);
	else {
		pr_err("%s: tty undefined %i\n", __func__, ch->channel_no);
	}
	return total;
}

static int transmit_chars(struct uart_sc16is7xx_chan *ch)
{
	struct circ_buf *xmit = &ch->port.state->xmit;
	int count = 0;
	int i = 0;
	unsigned char buf[32];

	count = ch->port.fifosize / 2;
	/*
	 * This chip doesn't like to write more than 32 bytes
	 * to the fifo at a time.
	 * Tx Threshold is also fixed at 32 bytes
	 */
	if (count > 32)
		count = 32;

#ifdef CONFIG_SERIAL_SC16IS7XX_CONSOLE
	while (ch->console_insert != ch->console_remove) {
		unsigned remove = ch->console_remove;
		buf[i++] = ch->console_buffer[remove++];
		if (remove >= ARRAY_SIZE(ch->console_buffer))
			remove = 0;
		ch->console_remove = remove;
		count--;
		if (!count)
			break;
	}
#endif
	if (count) {
		if (ch->port.x_char) {
			buf[i++] = ch->port.x_char;
			ch->port.x_char = 0;
		}
		if (uart_tx_stopped(&ch->port)) {
			count = 0;
		}
	}
	while (count) {
		if (uart_circ_empty(xmit))
			break;
		buf[i++] = xmit->buf[xmit->tail];
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		count--;
	}
	if (i) {
		sc_serial_out_cnt(ch, SC_THR, buf, i);
		ch->port.icount.tx += i;
		ch->tx_empty = 0;
	}
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&ch->port);
	if (uart_circ_empty(xmit) || !i) {
#ifdef CONFIG_SERIAL_SC16IS7XX_CONSOLE
		if (ch->console_insert != ch->console_remove)
			return i;
#endif
		serial_modify_ier(ch, UART_IER_THRI, 0);
	}
	return i;
}

static void check_modem_status(struct uart_sc16is7xx_chan *ch)
{
	int status;
	status = sc_serial_in(ch, SC_MSR);
	if ((status & UART_MSR_ANY_DELTA) == 0)
		return;
	if (status & UART_MSR_TERI)
		ch->port.icount.rng++;
	if (status & UART_MSR_DDSR)
		ch->port.icount.dsr++;
	if (status & UART_MSR_DDCD)
		uart_handle_dcd_change(&ch->port, status & UART_MSR_DCD);
	if (status & UART_MSR_DCTS)
		uart_handle_cts_change(&ch->port, status & UART_MSR_CTS);
	wake_up_interruptible(&ch->port.state->port.delta_msr_wait);
}

static int check_for_tx_empty(struct uart_sc16is7xx_chan *ch)
{
	if (!ch->check_tx_empty)
		return 0;
	if (ch->tx_empty) {
		ch->check_tx_empty = 0;
		return 0;
	}
	sc_serial_in_always(ch, SC_LSR);
	if (ch->tx_empty) {
		ch->check_tx_empty = 0;
		return 0;
	}
	return 1;
}

/*
 * This handles the interrupt from one port.
 */
static int serial_sc16is7xx_irq(struct uart_sc16is7xx_chan *ch)
{
	unsigned iir;
	unsigned state;
	int count;
	iir = sc_serial_in(ch, SC_IIR);
	if (iir & UART_IIR_NO_INT) {
		return 0;
		/* gp is checked first, so should not happen */
		pr_err("%s: no interupt, ier=%x\n", __func__, ch->actual_ier);
		return 0;
	}
	switch (iir & 0x3f) {
	/* highest priority case to lowest */
	case UART_IIR_RLSI:	/* 6 */
		/* some errors in rx fifo */
	case 0x0c:
		/* Receiver time-out interrupt */
	case UART_IIR_RDI:	/* 4 */
		/* Rx fifo above trigger level */
		count = receive_chars(ch);
		if (0) {
			if (count)
				pr_debug("%s: %i rx chars\n", __func__, count);
			else
				pr_info("%s: 0 rx chars\n", __func__);
		}
		break;
	case UART_IIR_THRI:	/* 2 */
		/* Tx fifo empty slots above trigger level */
		count = transmit_chars(ch);
		pr_debug("%s: %i tx chars\n", __func__, count);
		break;
	case UART_IIR_MSI:	/* 0 */
		pr_debug("modem interrupt\n");
		check_modem_status(ch);
		break;
	case 0x10:
		/* Xoff interupt, lower prority than I/O change (0x30) */
		pr_debug("Xoff interupt, ier=%x\n", ch->actual_ier);
//		if (ch->actual_ier & 0x20)
//			break;
//		break;
//Chip bug alert, sometime Xoff Interrupt is given for I/O pin change
	case 0x30:
		state = sc_serial_in(ch, SC_IOSTATE_R);
		if (ch->sc->gpio_callback)
			ch->sc->gpio_callback(ch->sc->gpio_sg, state);
		/* I/O pins change of state */
		pr_debug("I/O pins change of state, ier=%x\n", ch->actual_ier);
		break;
	case 0x20:
		/* RTS/CTS now inactive */
		pr_debug("RTS/CTS now inactive interupt, ier=%x\n", ch->actual_ier);
		check_modem_status(ch);
		break;
	default:
		pr_err("unknown interupt 0x%x, ier=%x\n", iir, ch->actual_ier);
		break;
	}
	return 1;
}

static void process_work_queue(struct uart_sc16is7xx_chan *ch)
{
	struct i2c_work *w;
	spin_lock(&ch->work_pending_lock);
	w = ch->pending_head;
	if (w) {
		ch->pending_head = w->next;
		if (!ch->pending_head)
			ch->pending_tail = NULL;
	}
	spin_unlock(&ch->work_pending_lock);

	if (w) {
		if (w->mode == SCM_READ) {
			sc_serial_in_always(ch, w->reg);
		} else {
			sc_serial_out(ch, w->reg, w->val);
		}
		free_work_entry(ch->sc, w);
	}
}

static int sc16is7xx_thread(void *_sc)
{
	int ret;
	int channel_no;
	enum sc_reg reg;
	unsigned gp_int;
	struct uart_sc16is7xx_sc *sc = _sc;
	struct uart_sc16is7xx_chan *ch;
	int last_active = 0;

	struct sched_param param = { .sched_priority = 1 };
	sched_setscheduler(current, SCHED_FIFO, &param);

	for (channel_no = 0; channel_no < 2; channel_no++) {
		ch = &sc->chan[channel_no];
		ret = sc_serial_out(ch, SC_LCR, 0);
		if (!ret)
			ret = sc_serial_out(ch, SC_MCR, 0);
		if (!ret)
			ret = sc_serial_out(ch, SC_EFR, 0);
		if (ret)
			goto out1;
		for (reg = 0; reg < SC_CHAN_REG_CNT; reg++) {
			unsigned mreg = reg_map[reg];
			if (!(mreg & _WO)) {
				ret = sc_serial_in_always(ch, reg);
				if (ret < 0)
					goto out1;
			}
		}
	}
	ch = &sc->chan[0];
	for (reg = SC_CHAN_REG_CNT; reg < SC_REG_CNT; reg++) {
		unsigned mreg = reg_map[reg];
		if (!(mreg & _WO)) {
			ret = sc_serial_in_always(ch, reg);
			if (ret < 0)
				goto out1;
		}
	}

	if (ret < 0)
		goto out1;
	/* default gpios to modem control */
	ret = sc_serial_out(ch, SC_IOCONTROL, 7);
	if (ret < 0)
		goto out1;
	complete(&sc->init_exit);

	while (!kthread_should_stop()) {
		sc->work_ready = 0;
		for (channel_no = 0; channel_no < 2; channel_no++) {
			ch = &sc->chan[channel_no];
			while (ch->pending_head) {
				if (kthread_should_stop())
					goto out1;
#ifdef CONFIG_SERIAL_SC16IS7XX_CONSOLE
				if (!ch->tx_empty) {
					/* don't process entry unless tx empty */
					ch->check_tx_empty = 1;
					if (check_for_tx_empty(&sc->chan[0]))
						break;
				}
#endif
				process_work_queue(ch);
			}
			if (ch->actual_ier != ch->new_ier) {
				unsigned val = ch->new_ier;
				ch->actual_ier = val;
				sc_serial_out(ch, SC_IER, val);
			}
		}
		while (sc->write_pending) {
			unsigned reg;
			unsigned val;
			spin_lock(&sc->pending_lock);
			reg = __ffs(sc->write_pending);
			sc->write_pending &= ~(1 << reg);
			val = sc->dev_cache[reg];
			/*
			 * due to chip bug, always rewrite output state
			 * when direction register changes
			 */
			if (reg == (SC_IODIR - SC_CHAN_REG_CNT))
				sc->write_pending |= 1 << (SC_IOSTATE_W - SC_CHAN_REG_CNT);
			spin_unlock(&sc->pending_lock);
			reg += SC_CHAN_REG_CNT;
			sc_serial_out(&sc->chan[0], reg, val);
		}
		gp_int = gpio_get_value(sc->gp);
		pr_debug("sc16is7xx gp_int=%x\n", gp_int);
		if (gp_int) {
			/* high level means no interrupt pending */
			ret = check_for_tx_empty(&sc->chan[0]);
			ret |= check_for_tx_empty(&sc->chan[1]);
			if (ret)
				wait_event_interruptible_timeout(sc->work_waitq, sc->work_ready, 200);
			else
				wait_event_interruptible(sc->work_waitq, sc->work_ready);
			continue;
		}
		ch = &sc->chan[last_active];
		if (serial_sc16is7xx_irq(ch))
			continue;
		ch = &sc->chan[last_active ^ 1];
		if (serial_sc16is7xx_irq(ch)) {
			last_active ^= 1;
			continue;
		}
	}

out1:
	sc->startup_code = ret;
	complete(&sc->init_exit);
	return ret;
}

static irqreturn_t sc16is7xx_interrupt(int irq, void *id)
{
	struct uart_sc16is7xx_sc *sc = id;
	sc->work_ready = 1;
	wake_up(&sc->work_waitq);
	return IRQ_HANDLED;
}

static const char *client_name = "sc16is7xx-uart";

int access_read(struct sc16is7xx_access *access, enum sc_reg reg)
{
	struct uart_sc16is7xx_sc *sc = container_of(access,
			struct uart_sc16is7xx_sc, sc_access);
	if ((reg < SC_CHAN_REG_CNT) || (reg >= SC_REG_CNT))
		return -EINVAL;

	if (reg_map[reg] & _V)
		return sc_serial_in_always(&sc->chan[0], reg);
	return sc->dev_cache[reg - SC_CHAN_REG_CNT];
}

int access_write(struct sc16is7xx_access *access, enum sc_reg reg, unsigned value)
{
	struct uart_sc16is7xx_sc *sc = container_of(access,
			struct uart_sc16is7xx_sc, sc_access);
	int wake = 0;
	if ((reg < SC_CHAN_REG_CNT) || (reg >= SC_REG_CNT))
		return -EINVAL;
	spin_lock(&sc->pending_lock);
	if (sc->dev_cache[reg - SC_CHAN_REG_CNT] != value) {
		sc->dev_cache[reg - SC_CHAN_REG_CNT] = value;
		sc->write_pending |= 1 << (reg - SC_CHAN_REG_CNT);
		wake = 1;
	}
	spin_unlock(&sc->pending_lock);
	if (wake) {
		sc->work_ready = 1;
		wake_up(&sc->work_waitq);
	}
	return wake;
}

int access_modify(struct sc16is7xx_access *access, enum sc_reg reg,
		unsigned clear_mask, unsigned set_mask)
{
	unsigned val, new_val;
	struct uart_sc16is7xx_sc *sc = container_of(access,
			struct uart_sc16is7xx_sc, sc_access);
	int wake = 0;
	if ((reg < SC_CHAN_REG_CNT) || (reg >= SC_REG_CNT))
		return -EINVAL;
	spin_lock(&sc->pending_lock);
	val = sc->dev_cache[reg - SC_CHAN_REG_CNT];
	new_val = (val & ~clear_mask) | set_mask;
	if (val != new_val) {
		sc->dev_cache[reg - SC_CHAN_REG_CNT] = new_val;
		sc->write_pending |= 1 << (reg - SC_CHAN_REG_CNT);
		wake = 1;
	}
	spin_unlock(&sc->pending_lock);
	if (wake) {
		sc->work_ready = 1;
		wake_up(&sc->work_waitq);
	}
	return wake;
}

int access_register_gpio_interrupt(struct sc16is7xx_access *access,
		void (*call_back)(struct sc16is7xx_gpio *sg, unsigned),
		struct sc16is7xx_gpio *sg)
{
	struct uart_sc16is7xx_sc *sc = container_of(access,
			struct uart_sc16is7xx_sc, sc_access);
	sc->gpio_callback = call_back;
	sc->gpio_sg = sg;
	return 0;
}

static int sc16is7xx_add_gpio_device(struct uart_sc16is7xx_sc *sc, struct device *parent,
		const char *name, void *pdata, size_t pdata_size)
{
	struct mfd_cell cell = {
		.name = name,
		.platform_data = pdata,
		.data_size = pdata_size,
	};
	sc->sc_access.read = access_read;
	sc->sc_access.write = access_write;
	sc->sc_access.modify = access_modify;
	sc->sc_access.register_gpio_interrupt = &access_register_gpio_interrupt;

	return mfd_add_devices(parent, -1, &cell, 1, NULL, 0);
}

static const char *reg_names[] = {
	"LCR",
	"RHR",
	"THR",
	"IER",
	"DLL",
	"DLH",
	"IIR",
	"FCR",
	"MCR",
	"LSR",
	"TXLVL",
	"RXLVL",
	"EFCR",
	"MSR",
	"SPR",
	"TCR",
	"TLR",
	"EFR",
	"XON1",
	"XON2",
	"XOFF1",
	"XOFF2",
	"IOSTATE_R",
	"IOSTATE_W",
	"IODIR",
	"IOINTENA",
	"IOCONTROL",
};

static ssize_t sc16is7xx_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct uart_sc16is7xx_sc *sc = serial_sc16is7xx_sc;
	struct uart_sc16is7xx_chan *ch;
	int total = 0;
	int cnt;
	int reg;
	int i;
	if (!sc)
		return -ENODEV;

	for (i = 0; i < 2; i++) {
		ch = &sc->chan[i];
		cnt = sprintf(buf, "Channel %i: ", i);
		total += cnt;
		buf += cnt;
		for (reg = 0; reg < SC_CHAN_REG_CNT; reg++) {
			cnt = sprintf(buf, "%s=%02x ", reg_names[reg], ch->reg_cache[reg]);
			total += cnt;
			buf += cnt;
		}
		cnt = sprintf(buf, "\n");
		total += cnt;
		buf += cnt;
	}
	cnt = sprintf(buf, "IO_Regs: ");
	total += cnt;
	buf += cnt;
	for (reg = SC_CHAN_REG_CNT; reg < SC_REG_CNT; reg++) {
		cnt = sprintf(buf, "%s=%02x ", reg_names[reg], sc->dev_cache[reg - SC_CHAN_REG_CNT]);
		total += cnt;
		buf += cnt;
	}
	cnt = sprintf(buf, "\n");
	total += cnt;
	return total;
}

static DEVICE_ATTR(sc16is7xx_reg, 0444, sc16is7xx_reg_show, NULL);

static int sc16is7xx_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	int i;
	struct uart_sc16is7xx_sc *sc;
	struct uart_sc16is7xx_chan *ch;
	struct device *dev = &client->dev;
	struct sc16is7xx_platform_data *plat = client->dev.platform_data;

	printk(KERN_ERR "%s: %s version 1.0\n", __func__, client_name);
	sc = kzalloc(sizeof(struct uart_sc16is7xx_sc), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	init_waitqueue_head(&sc->work_waitq);
	sc->client = client;
	sc->irq = plat->irq;
	sc->gp = plat->gp;
	init_completion(&sc->init_exit);
	spin_lock_init(&sc->work_free_lock);
	spin_lock_init(&sc->pending_lock);
	for (i = 0; i < MAX_WORK - 1; i++) {
		sc->work_entries[i].next = &sc->work_entries[i + 1];
	}
	sc->work_free = &sc->work_entries[0];
	for (i = 0; i < 2; i++) {
		ch = &sc->chan[i];
		spin_lock_init(&ch->work_pending_lock);
		ch->port.type = PORT_SC16IS7XX;
		ch->port.iotype = UPIO_MEM;
		/* needed to pass serial_core check for existence */
		ch->port.mapbase = 0xfff8 + i;
		ch->port.fifosize = 64;
		ch->port.ops = &serial_sc16is7xx_pops;
		ch->port.line = i;
		ch->port.dev = dev;
		ch->port.flags = UPF_BOOT_AUTOCONF;
		ch->port.uartclk = 3686400;
		ch->sc = sc;
		ch->channel_no = i;
		ch->tx_empty = 1;
		ch->name = (!i) ? "UARTA" : "UARTB";
	}

	serial_sc16is7xx_sc = sc;

	ret = request_irq(sc->irq, &sc16is7xx_interrupt, IRQF_TRIGGER_FALLING, client_name, sc);
	if (ret) {
		pr_err("%s: request_irq failed, irq:%i\n", client_name,sc->irq);
		goto out;
	}
	i2c_set_clientdata(client, sc);

	init_completion(&sc->init_exit);
	sc->sc_thread = kthread_run(sc16is7xx_thread, sc, "sc16is7xxd");
	if (ret < 0)
		goto out2;
	wait_for_completion(&sc->init_exit);	/* wait for thread to Start */
	ret = sc->startup_code;
	if (ret < 0)
		goto out2;

	for (i = 0; i < 2; i++) {
		ch = &sc->chan[i];
		uart_add_one_port(&serial_sc16is7xx_reg, &ch->port);
	}
	sc16is7xx_add_gpio_device(sc, dev, "sc16is7xx-gpio", plat->gpio_data,
			sizeof(struct sc16is7xx_gpio_platform_data));

	ret = device_create_file(&client->dev, &dev_attr_sc16is7xx_reg);
	if (ret < 0)
		printk(KERN_WARNING "failed to add mma7660 sysfs files\n");

	pr_err("%s: succeeded\n", __func__);
	return 0;
out2:
	free_irq(sc->irq, sc);
out:
	kfree(sc);
	pr_err("%s: failed %i\n", __func__, ret);
	return ret;
}

static int sc16is7xx_remove(struct i2c_client *client)
{
	struct uart_sc16is7xx_sc *sc = i2c_get_clientdata(client);
	kthread_stop(sc->sc_thread);

	uart_remove_one_port(&serial_sc16is7xx_reg, &sc->chan[1].port);
	uart_remove_one_port(&serial_sc16is7xx_reg, &sc->chan[0].port);
	free_irq(sc->irq, sc);
	kfree(sc);
	return 0;
}

/* Return 0 if detection is successful, -ENODEV otherwise */
static int sc16is7xx_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -ENODEV;
	strlcpy(info->type, client_name, I2C_NAME_SIZE);
	return 0;
}


/*-----------------------------------------------------------------------*/

static const struct i2c_device_id sc16is7xx_idtable[] = {
	{ "sc16is7xx-uart", 0 },
	{ }
};

static struct i2c_driver sc16is7xx_driver = {
	.driver = {
		.owner		= THIS_MODULE,
		.name		= "sc16is7xx-uart",
	},
	.id_table	= sc16is7xx_idtable,
	.probe		= sc16is7xx_probe,
	.remove		= __devexit_p(sc16is7xx_remove),
	.detect		= sc16is7xx_detect,
};

static int __init sc16is7xx_init(void)
{
	int ret = uart_register_driver(&serial_sc16is7xx_reg);
	if (ret != 0)
		return ret;
	if ((ret = i2c_add_driver(&sc16is7xx_driver))) {
		uart_unregister_driver(&serial_sc16is7xx_reg);
		pr_warning("%s: i2c_add_driver failed\n",client_name);
		return ret;
	}
	pr_err("%s: version 1.0\n",client_name);
	return 0;
}

static void __exit sc16is7xx_exit(void)
{
	i2c_del_driver(&sc16is7xx_driver);
	uart_unregister_driver(&serial_sc16is7xx_reg);
}

module_init(sc16is7xx_init)
module_exit(sc16is7xx_exit)

MODULE_AUTHOR("Troy Kisky <troy.kisky@boundarydevices.com>");
MODULE_DESCRIPTION("I2C dual serial ports");
MODULE_LICENSE("GPL");
