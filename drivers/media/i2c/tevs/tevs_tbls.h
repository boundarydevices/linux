#ifndef __SENSOR_TABLES_H__
#define __SENSOR_TABLES_H__

struct resolution {
	u16 width;
	u16 height;
	u16 framerates;
	u16 mode;
};

/* AR0144 default setting for 4 data lanes and data frequency 800 MHz */
static struct resolution ar0144_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 60, .mode = 0 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 0 },
	{ .width = 1280, .height = 800, .framerates = 60, .mode = 0 },
};

/* AR0145 default setting for 4 data lanes and data frequency 800 MHz */
static struct resolution ar0145_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 120, .mode = 0 },
	{ .width = 1280, .height = 720, .framerates = 120, .mode = 0 },
	{ .width = 1280, .height = 800, .framerates = 120, .mode = 0 },
};

/* AR0234 default setting for 4 data lanes and data frequency 800 MHz */
static struct resolution ar0234_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 120, .mode = 1 },
	{ .width = 1280, .height = 720, .framerates = 120, .mode = 0 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 0 },
	{ .width = 1920, .height = 1200, .framerates = 60, .mode = 0 },
};

/* AR0521 default setting for 4 data lanes and data frequency 800 MHz */
static struct resolution ar0521_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 120, .mode = 3 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 3 },
	{ .width = 1280, .height = 960, .framerates = 60, .mode = 3 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 1 },
	{ .width = 2560, .height = 1440, .framerates = 32, .mode = 1 },
	{ .width = 2592, .height = 1944, .framerates = 24, .mode = 1 },
};

/* AR0522 default setting for 4 data lanes and data frequency 800 MHz */
static struct resolution ar0522_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 120, .mode = 3 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 3 },
	{ .width = 1280, .height = 960, .framerates = 60, .mode = 3 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 1 },
	{ .width = 2560, .height = 1440, .framerates = 32, .mode = 1 },
	{ .width = 2592, .height = 1944, .framerates = 24, .mode = 1 },
};

/* AR0821 default setting for 4 data lanes and data frequency 800 MHz */
static struct resolution ar0821_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 60, .mode = 2 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 2 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 2 },
	{ .width = 2560, .height = 1440, .framerates = 30, .mode = 0 },
	{ .width = 3840, .height = 2160, .framerates = 15, .mode = 0 },
};

/* AR0822 default setting for 4 data lanes and data frequency 800 MHz */
static struct resolution ar0822_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 60, .mode = 1 },
	{ .width = 1280, .height = 720, .framerates = 60, .mode = 1 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 1 },
	{ .width = 2560, .height = 1440, .framerates = 30, .mode = 0 },
	{ .width = 3840, .height = 2160, .framerates = 15, .mode = 0 },
};

/* AR1335 default setting for 4 data lanes and data frequency 800 MHz */
static struct resolution ar1335_res_list[] = {
	{ .width = 640, .height = 480, .framerates = 60, .mode = 4 },
	{ .width = 1280, .height = 720, .framerates = 120, .mode = 4 },
	{ .width = 1920, .height = 1080, .framerates = 60, .mode = 3 },
	{ .width = 2560, .height = 1440, .framerates = 30, .mode = 1 },
	{ .width = 3840, .height = 2160, .framerates = 15, .mode = 0 },
	{ .width = 4208, .height = 3120, .framerates = 10, .mode = 0 },
};

struct sensor_info {
	const char *sensor_name;
	struct resolution *res_list;
	u32 res_list_size;
};

static struct sensor_info tevs_sensor_table[] = {
	{ .sensor_name = "TEVS-AR0144",
	  .res_list = ar0144_res_list,
	  .res_list_size = ARRAY_SIZE(ar0144_res_list) },
	{ .sensor_name = "TEVS-AR0145",
	  .res_list = ar0145_res_list,
	  .res_list_size = ARRAY_SIZE(ar0145_res_list) },
	{ .sensor_name = "TEVS-AR0234",
	  .res_list = ar0234_res_list,
	  .res_list_size = ARRAY_SIZE(ar0234_res_list) },
	{ .sensor_name = "TEVS-AR0521",
	  .res_list = ar0521_res_list,
	  .res_list_size = ARRAY_SIZE(ar0521_res_list) },
	{ .sensor_name = "TEVS-AR0522",
	  .res_list = ar0522_res_list,
	  .res_list_size = ARRAY_SIZE(ar0522_res_list) },
	{ .sensor_name = "TEVS-AR0821",
	  .res_list = ar0821_res_list,
	  .res_list_size = ARRAY_SIZE(ar0821_res_list) },
	{ .sensor_name = "TEVS-AR0822",
	  .res_list = ar0822_res_list,
	  .res_list_size = ARRAY_SIZE(ar0822_res_list) },
	{ .sensor_name = "TEVS-AR1335",
	  .res_list = ar1335_res_list,
	  .res_list_size = ARRAY_SIZE(ar1335_res_list) },
};

#endif //__SENSOR_TABLES_H__