/*
 * Driver for Techwell TW6864/68 based DVR cards
 *
 * (c) 2009-10 liran <jlee@techwellinc.com.cn> [Techwell China]
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef TW686X_DEVICE_H_INCLUDED
#define TW686X_DEVICE_H_INCLUDED

int  tw686x_dev_init( struct tw686x_chip *chip );
int  tw686x_dev_init_286x( struct tw686x_chip *chip );
void tw686x_dev_uninit( struct tw686x_chip *chip );
void tw686x_dev_setdma( struct tw686x_chip *chip, u32 dma_cmd);
int  tw686x_dev_set_mux( struct tw686x_vdev *dev, unsigned int input );
void tw686x_dev_set_standard( struct tw686x_vdev *dev, v4l2_std_id value, bool b_auto );
void tw686x_dev_set_pixelformat( struct tw686x_vdev *dev, u32 fourcc );
void tw686x_dev_set_size(struct tw686x_vdev *dev, int width, int height);
void tw686x_dev_set_size2(struct tw686x_vdev *dev, int width, int height);
void tw686x_dev_set_vdelay(struct tw686x_vdev *dev, int value);
void tw686x_dev_set_bright(struct tw686x_vdev *dev, int val);
void tw686x_dev_set_contrast(struct tw686x_vdev *dev, int val);
void tw686x_dev_set_hue(struct tw686x_vdev *dev, int val);
void tw686x_dev_set_saturation(struct tw686x_vdev *dev, int val);
void tw686x_dev_set_sharpness(struct tw686x_vdev *dev, int val);
void tw686x_dev_set_framerate(struct tw686x_vdev *dev, int val, int field);
u32  tw686x_dev_get_decoderstatus(struct tw686x_vdev *dev);
int  tw686x_dev_get_signal(struct tw686x_vdev *dev);
void tw686x_dev_run(struct tw686x_vdev *dev, bool b_run);
void tw686x_dev_set_dma(struct tw686x_vdev *dev, int width, int height, int linewidth);
int  tw686x_dev_set_dmabuf(struct tw686x_vdev *dev, int nbuf, struct tw686x_buf *buf);
void tw686x_dev_set_pbdesc(struct tw686x_vdev *dev);
int  tw686x_dev_set_dmadesc(struct tw686x_vdev *dev, int nbuf, struct tw686x_buf *buf);

bool tw686x_dev_enable_gpiooutput(struct tw686x_chip *chip, bool enable, int gpio, int bits);
bool tw686x_dev_set_gpiobits(struct tw686x_chip *chip, int gpio, int bits, u32* value);
bool tw686x_dev_get_gpiobits(struct tw686x_chip *chip, int gpio, int bits, u32* pValue);
int  tw686x_dev_set_audio(struct tw686x_chip *chip, int samplerate, int bits, int channels, int blksize);
int  tw686x_dev_set_adma_buffer(struct tw686x_adev *dev, u32 buf_addr, int pb);
void tw686x_dev_run_audio(struct tw686x_adev *dev, bool b_run);

#endif // TW686X_DEVICE_H_INCLUDED
