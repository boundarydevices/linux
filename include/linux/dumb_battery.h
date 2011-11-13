#ifndef __LINUX_DUMB_BATTERY_H
#define __LINUX_DUMB_BATTERY_H

struct dumb_battery_platform_t {
	unsigned    	init_level ;	/* at boot time, report this level */
	unsigned    	charge_sec ;	/* charge to full in this time period */
	unsigned	discharge_sec ; /* discharge to empty in this time period */
};

#endif
