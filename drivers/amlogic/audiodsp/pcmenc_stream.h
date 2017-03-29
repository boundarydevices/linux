/*
 * drivers/amlogic/audiodsp/pcmenc_stream.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __PCMENC_STREAM_H__
#define __PCMENC_STREAM_H__
#include <linux/uaccess.h>
struct pcm51_encoded_info_t {
	unsigned int InfoValidFlag;
	unsigned int SampFs;
	unsigned int NumCh;
	unsigned int AcMode;
	unsigned int LFEFlag;
	unsigned int BitsPerSamp;
};

#define AUDIODSP_PCMENC_GET_RING_BUF_SIZE      _IOR('l', 0x01, unsigned long)
#define AUDIODSP_PCMENC_GET_RING_BUF_CONTENT   _IOR('l', 0x02, unsigned long)
#define AUDIODSP_PCMENC_GET_RING_BUF_SPACE  _IOR('l', 0x03, unsigned long)
#define AUDIODSP_PCMENC_SET_RING_BUF_RPTR	_IOW('l', 0x04, unsigned long)
#define AUDIODSP_PCMENC_GET_PCMINFO		_IOR('l', 0x05, unsigned long)

/* initialize  stream FIFO
 * return value: on success, zero is returned, on error, -1 is returned
 */
extern int pcmenc_stream_init(void);

/* return space of  stream FIFO, unit:byte
 */
extern int pcmenc_stream_space(void);

/* return content of  stream FIFO, unit:byte
 */
extern int pcmenc_stream_content(void);

/* deinit  stream  FIFO
 * return value: on success, zero is returned, on error, -1 is returned
 */
extern int pcmenc_stream_deinit(void);

/*
 * read  data out of FIFO, the minimum of the FIFO's conten
 * and size will be read, if the FIFO is empty, read will be failed
 * return value: on success, the number of bytes read are returned,
 * othewise, 0 is returned
 */
extern int pcmenc_stream_read(char __user *buffer, int size);

#endif				/*  */
