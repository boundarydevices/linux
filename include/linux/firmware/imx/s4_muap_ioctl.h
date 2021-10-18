/* SPDX-License-Identifier: (GPL-2.0 WITH Linux-syscall-note) OR BSD-3-Clause*/
/*
 * Copyright 2019-2021 NXP
 */

#ifndef S4_MUAP_IOCTL_H
#define S4_MUAP_IOCTL_H

/* IOCTL definitions. */

#define MAX_IMG_COUNT	5
#define HASH_MAX_LEN	64
#define IV_MAX_LEN	32

struct s4_muap_ioctl_signed_message {
	u8 *message;
	u32 msg_size;
	u32 error_code;
};



#define S4_MUAP_IOCTL			0x0A /* like MISC_MAJOR. */

#define S4_MUAP_IOCTL_SIGNED_MSG_CMD	_IOWR(S4_MUAP_IOCTL, 0x03,\
					struct s4_muap_ioctl_signed_message)
#endif
