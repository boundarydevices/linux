#ifndef MAGSTRIPE_H
#define MAGSTRIPE_H

struct mag_platform_data {
	int front_pin;
	int rear_pin;
	int clock_pin;
	int data_pin;
	char edge;
	int timeout;
};

#endif
