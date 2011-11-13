#ifndef __LINUX_SPI_LTC1960_H
#define __LINUX_SPI_LTC1960_H

#include <linux/power_supply.h>

struct ltc1960_battery_info_t {
	char 	 	*name ;
	unsigned    	charge_uv ;
	unsigned    	charge_ua ;
	unsigned	trickle_seconds ;
};

struct ltc1960_platform_data_t {
	struct ltc1960_battery_info_t *batteries[2];
};

#endif
