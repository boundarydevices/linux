#ifndef _ILI210X_H
#define _ILI210X_H

struct ili210x_platform_data {
	unsigned long irq_flags;
	unsigned int poll_period;
	int gp;
	bool (*get_pendown_state)(void);
};

#endif
