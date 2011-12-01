#ifndef __NITROGEN_H__
#define __NITROGEN_H__

#include <asm/mach-types.h>

#define machine_is_nitrogen_51() (machine_is_nitrogen_imx51() \
				  || machine_is_nitrogen_vm_imx51() \
				  || machine_is_nitrogen_p_imx51() \
				  || machine_is_nitrogen_ej_imx51())
#define machine_is_nitrogen_53() (machine_is_nitrogen_imx53() \
				  || machine_is_nitrogen_v1_imx53() \
				  || machine_is_nitrogen_a_imx53() \
				  || machine_is_nitrogen_v2_imx53())

#endif
