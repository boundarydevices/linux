#ifndef __WL12XX_BT_H
#define __WL12XX_BT_H

int plat_kim_suspend(struct platform_device *pdev, pm_message_t state);
int plat_kim_resume(struct platform_device *pdev);
int plat_kim_chip_enable(struct kim_data_s *kim_data);
int plat_kim_chip_disable(struct kim_data_s *kim_data);

#endif
