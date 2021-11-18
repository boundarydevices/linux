/* SPDX-License-Identifier: (GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause*/
/*
 * Copyright 2019-2022 NXP
 */

#ifndef ELE_MU_IOCTL_H
#define ELE_MU_IOCTL_H

/* IOCTL definitions. */

struct ele_mu_ioctl_signed_message {
	u8 *message;
	u32 msg_size;
	u32 error_code;
};


#define ELE_MU_IOCTL			0x0A /* like MISC_MAJOR. */
#define ELE_MU_IOCTL_SIGNED_MESSAGE	_IOWR(ELE_MU_IOCTL, 0x05, \
					struct ele_mu_ioctl_signed_message)
#endif
