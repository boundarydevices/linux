#define XR_INT0 0x80
#define XR_INT1 0x81
#define XR_INT2 0x82
#define XR_INT3 0x83

enum mpio_base {
	xr_mpio0 = 0x8f,
	xr_mpio1 = 0x95,
};

enum xr_reg_offset {
	XR_MPIOINT = 0,
	XR_MPIOLVL = 1,
	XR_MPIO3T = 2,
	XR_MPIOINV = 3,
	XR_MPIOSEL = 4,
	XR_MPIOOD = 5,
};

struct xr17v35x_gpio;
struct serial_private;

struct xr17v35x_access {
	int (*read) (struct serial_private *priv, int reg);
	int (*write) (struct serial_private *priv, int reg,
			unsigned value);
	int (*modify) (struct serial_private *priv, int reg,
			unsigned clear_mask, unsigned set_mask);
	int (*register_gpio_interrupt)(struct serial_private *priv,
		void (*call_back)(struct xr17v35x_gpio *xr),
		struct xr17v35x_gpio *xr);
};

struct xr17v35x_gpio_platform_data {
	unsigned gpio_base;
	unsigned irq_base;
	const struct xr17v35x_access *access;
	struct serial_private *priv;
};
