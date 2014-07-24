#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>

int plat_kim_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}
int plat_kim_resume(struct platform_device *pdev)
{
	return 0;
}

int plat_kim_chip_enable(struct kim_data_s *kim_data)
{
	/* reset pulse to the BT controller */
	usleep_range(150, 220);
	gpio_set_value_cansleep(kim_data->nshutdown, 0);
	usleep_range(150, 220);
	gpio_set_value_cansleep(kim_data->nshutdown, 1);
	usleep_range(150, 220);
	gpio_set_value_cansleep(kim_data->nshutdown, 0);
	usleep_range(150, 220);
	gpio_set_value_cansleep(kim_data->nshutdown, 1);
	usleep_range(1, 2);

	return 0;
}

int plat_kim_chip_disable(struct kim_data_s *kim_data)
{
	gpio_set_value_cansleep(kim_data->nshutdown, 0);

	return 0;
}


