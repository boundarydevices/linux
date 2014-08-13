enum sc_reg {
	SC_LCR = 0,
	SC_RHR,
	SC_THR,
	SC_IER,
	SC_DLL,
	SC_DLH,
	SC_IIR,
	SC_FCR,
	SC_MCR,
	SC_LSR,
	SC_TXLVL,
	SC_RXLVL,
	SC_EFCR,
	SC_MSR,
	SC_SPR,
	SC_TCR,
	SC_TLR,
	SC_EFR,
	SC_XON1,
	SC_XON2,
	SC_XOFF1,
	SC_XOFF2,
	SC_CHAN_REG_CNT,
/*
 *  keep IOSTATE before IODIR, so that it's write will occur before direction
 *  changes.
 */
	SC_IOSTATE_R = SC_CHAN_REG_CNT,
	SC_IOSTATE_W,
	SC_IODIR,
	SC_IOINTENA,
	SC_IOCONTROL,
	SC_REG_CNT,
};

struct sc16is7xx_gpio;

struct sc16is7xx_access {
	int (*read) (struct sc16is7xx_access *access, enum sc_reg reg);
	int (*write) (struct sc16is7xx_access *access, enum sc_reg reg,
			unsigned value);
	int (*modify) (struct sc16is7xx_access *access, enum sc_reg reg,
			unsigned clear_mask, unsigned set_mask);
	int (*register_gpio_interrupt)(struct sc16is7xx_access *access,
		void (*call_back)(struct sc16is7xx_gpio *sg, unsigned state),
		struct sc16is7xx_gpio *sg);
};

struct sc16is7xx_gpio_platform_data {
	unsigned gpio_base;
	unsigned irq_base;
};
