
struct i2c_work;

enum sc_mode {
	SCM_READ,
	SCM_WRITE,
};

struct i2c_work
{
	struct i2c_work *next;
	unsigned char mode;
	unsigned char reg;
#define F_PENDING 0
#define F_SUCCESS 1
#define F_FAILURE 2
	unsigned char state;
	unsigned char cnt;
	unsigned char channel_no;
	unsigned char val;
};

struct uart_sc16is7xx_chan {
	struct uart_port        port;
	struct uart_sc16is7xx_sc *sc;
	unsigned char		channel_no;
	unsigned char		actual_lcr;
	unsigned char		actual_mcr;
	unsigned char		actual_efr;
	unsigned char		actual_ier;
	unsigned char		new_ier;
	unsigned char		tx_empty;
	unsigned char		check_tx_empty;
	char			*name;
	spinlock_t		work_pending_lock;
	struct i2c_work		*pending_head;
	struct i2c_work		*pending_tail;
	unsigned char		reg_cache[SC_CHAN_REG_CNT];
#ifdef CONFIG_SERIAL_SC16IS7XX_CONSOLE
	unsigned short		console_insert;
	unsigned short		console_remove;
	unsigned char		console_buffer[1024];
#endif
};

struct uart_sc16is7xx_sc {
	struct sc16is7xx_access	sc_access;

	void (*gpio_callback)(struct sc16is7xx_gpio *sg, unsigned state);
	struct sc16is7xx_gpio	*gpio_sg;
	struct uart_sc16is7xx_chan chan[2];
	struct i2c_client 	*client;
	/* thread waits on this, irq wakes up */
	int			work_ready;
	wait_queue_head_t	work_waitq;
	struct completion	init_exit;	/* thread init done notification */
	int			irq;
	unsigned		gp;
	int			startup_code;
	int			shutdown;
	struct task_struct	*sc_thread;
	unsigned char		dev_cache[SC_REG_CNT - SC_CHAN_REG_CNT];
	spinlock_t		work_free_lock;
	spinlock_t		pending_lock;
	unsigned		write_pending;
	struct i2c_work		*work_free;
#define MAX_WORK 32
	struct i2c_work		work_entries[MAX_WORK];
};

/*
 * 7 6543  21 0
 * |   |   |  |
 * |   |   |  unused
 * |   |   00 channel A
 * |   |   01 channel B
 * |   |   1x reserved
 * | internal register select
 * unused
 */

#define MAKE_SUBADDRESS(port, reg) (unsigned char)(((reg & 0x0f) << 3) | (port << 1))

#define SP_0xBF		0
#define SP_DLAB		1
#define SP_MCR2		2

#define A_TYPEV(bit) (1 << ((bit) + 4))
#define A_TYPE(bit, val) ((0x10 | (val)) << ((bit) + 4))
#define _RO		0x1000		/* Read-only */
#define _WO		0x2000		/* Write-only */
#define _V		0x4000		/* Volatile */


#define IER_SLEEP		(1 << 4)	/* EFR[4] needs to be set */
#define IER_XOFF_INT		(1 << 5)
#define IER_RTS_INT		(1 << 6)
#define IER_CTS_INT		(1 << 7)
