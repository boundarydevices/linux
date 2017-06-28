#include <linux/export.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include "fbtft.h"

/*****************************************************************************
 *
 *   void (*write_reg)(struct fbtft_par *par, int len, ...);
 *
 *****************************************************************************/

#define define_fbtft_write_reg(func, type, modifier)                          \
void func(struct fbtft_par *par, int len, ...)                                \
{                                                                             \
	va_list args;                                                         \
	int i, ret;                                                           \
	int offset = 0;                                                       \
	type *buf = (type *)par->buf;                                         \
									      \
	if (unlikely(par->debug & DEBUG_WRITE_REGISTER)) {                    \
		va_start(args, len);                                          \
		for (i = 0; i < len; i++) {                                   \
			buf[i] = (type)va_arg(args, unsigned int);            \
		}                                                             \
		va_end(args);                                                 \
		fbtft_par_dbg_hex(DEBUG_WRITE_REGISTER, par, par->info->device, type, buf, len, "%s: ", __func__);   \
	}                                                                     \
									      \
	va_start(args, len);                                                  \
									      \
	if (par->startbyte) {                                                 \
		*(u8 *)par->buf = par->startbyte;                             \
		buf = (type *)(par->buf + 1);                                 \
		offset = 1;                                                   \
	}                                                                     \
									      \
	*buf = modifier((type)va_arg(args, unsigned int));                    \
	if (par->gpio.dc != -1)                                               \
		gpio_set_value(par->gpio.dc, 0);                              \
	ret = par->fbtftops.write(par, par->buf, sizeof(type) + offset);      \
	if (ret < 0) {                                                        \
		va_end(args);                                                 \
		dev_err(par->info->device, "%s: write() failed and returned %d\n", __func__, ret); \
		return;                                                       \
	}                                                                     \
	len--;                                                                \
									      \
	if (par->startbyte)                                                   \
		*(u8 *)par->buf = par->startbyte | 0x2;                       \
									      \
	if (len) {                                                            \
		i = len;                                                      \
		while (i--) {                                                 \
			*buf++ = modifier((type)va_arg(args, unsigned int));  \
		}                                                             \
		if (par->gpio.dc != -1)                                       \
			gpio_set_value(par->gpio.dc, 1);                      \
		ret = par->fbtftops.write(par, par->buf,		      \
					  len * (sizeof(type) + offset));     \
		if (ret < 0) {                                                \
			va_end(args);                                         \
			dev_err(par->info->device, "%s: write() failed and returned %d\n", __func__, ret); \
			return;                                               \
		}                                                             \
	}                                                                     \
	va_end(args);                                                         \
}                                                                             \
EXPORT_SYMBOL(func);

define_fbtft_write_reg(fbtft_write_reg8_bus8, u8, )
define_fbtft_write_reg(fbtft_write_reg16_bus8, u16, cpu_to_be16)
define_fbtft_write_reg(fbtft_write_reg16_bus16, u16, )

void fbtft_write_reg8_bus9(struct fbtft_par *par, int len, ...)
{
	va_list args;
	int i, ret;
	int pad = 0;
	u16 *buf = (u16 *)par->buf;

	if (unlikely(par->debug & DEBUG_WRITE_REGISTER)) {
		va_start(args, len);
		for (i = 0; i < len; i++)
			*(((u8 *)buf) + i) = (u8)va_arg(args, unsigned int);
		va_end(args);
		fbtft_par_dbg_hex(DEBUG_WRITE_REGISTER, par,
			par->info->device, u8, buf, len, "%s: ", __func__);
	}
	if (len <= 0)
		return;

	if (par->spi && (par->spi->bits_per_word == 8)) {
		/* we're emulating 9-bit, pad start of buffer with no-ops
		 * (assuming here that zero is a no-op)
		 */
		pad = (len % 4) ? 4 - (len % 4) : 0;
		for (i = 0; i < pad; i++)
			*buf++ = 0x000;
	}

	va_start(args, len);
	*buf++ = (u8)va_arg(args, unsigned int);
	i = len - 1;
	while (i--) {
		*buf = (u8)va_arg(args, unsigned int);
		*buf++ |= 0x100; /* dc=1 */
	}
	va_end(args);
	ret = par->fbtftops.write(par, par->buf, (len + pad) * sizeof(u16));
	if (ret < 0) {
		dev_err(par->info->device,
			"write() failed and returned %d\n", ret);
		return;
	}
}
EXPORT_SYMBOL(fbtft_write_reg8_bus9);

/*****************************************************************************
 *
 *   int (*write_vmem)(struct fbtft_par *par);
 *
 *****************************************************************************/

/* 16 bit pixel over 8-bit databus */
int fbtft_write_vmem16_bus8(struct fbtft_par *par, size_t offset, size_t len)
{
	u16 *vmem16;
	u16 *txbuf16 = par->txbuf.buf;
	size_t remain;
	size_t to_copy;
	size_t tx_array_size;
	int i;
	int ret = 0;
	size_t startbyte_size = 0;

	fbtft_par_dbg(DEBUG_WRITE_VMEM, par, "%s(offset=%zu, len=%zu)\n",
		__func__, offset, len);

	remain = len / 2;
	vmem16 = (u16 *)(par->info->screen_buffer + offset);

	if (par->gpio.dc != -1)
		gpio_set_value(par->gpio.dc, 1);

	/* non buffered write */
	if (!par->txbuf.buf)
		return par->fbtftops.write(par, vmem16, len);

	/* buffered write */
	tx_array_size = par->txbuf.len / 2;

	if (par->startbyte) {
		txbuf16 = par->txbuf.buf + 1;
		tx_array_size -= 2;
		*(u8 *)(par->txbuf.buf) = par->startbyte | 0x2;
		startbyte_size = 1;
	}

	while (remain) {
		to_copy = min(tx_array_size, remain);
		dev_dbg(par->info->device, "    to_copy=%zu, remain=%zu\n",
						to_copy, remain - to_copy);

		for (i = 0; i < to_copy; i++)
			txbuf16[i] = cpu_to_be16(vmem16[i]);

		vmem16 = vmem16 + to_copy;
		ret = par->fbtftops.write(par, par->txbuf.buf,
						startbyte_size + to_copy * 2);
		if (ret < 0)
			return ret;
		remain -= to_copy;
	}

	return ret;
}
EXPORT_SYMBOL(fbtft_write_vmem16_bus8);

struct vars {
	u8 *vmem8;
	size_t rem_tx;
	size_t rem;
	int status;
};

struct message_transfer_par {
	struct spi_message m;
	struct spi_transfer t;
	struct fbtft_par *par;
	size_t max_tx;
	struct vars *vars;
	struct completion complete;
	int queued;
};

int fill_txbuffer(struct message_transfer_par *mtp)
{
	u32 *txbuf32 = (u32 *)mtp->t.tx_buf;
	struct vars *v = mtp->vars;
	size_t to_copy;
	u8 *vmem8;
	int ret;
	int i;

	to_copy = v->rem_tx > mtp->max_tx ? mtp->max_tx : v->rem_tx;
	vmem8 = v->vmem8;
	dev_dbg(mtp->par->info->device, "to_copy=%zu, rem_tx=%zu\n",
			to_copy, v->rem_tx);
	v->rem_tx -= to_copy;

	for (i = 0; i < to_copy; i++, vmem8 += 6) {
		*txbuf32++ = BIT(8) | BIT(17) | BIT(26) |
			(vmem8[1] << 18) | (vmem8[0] << 9) |
			vmem8[3];
		*txbuf32++ = BIT(8) | BIT(17) | BIT(26) |
			(vmem8[2] << 18) | (vmem8[5] << 9) |
			vmem8[4];
	}
	if (v->rem && (i < mtp->max_tx)) {
		if (v->rem >= 4) {
			*txbuf32++ = BIT(8) | BIT(17) | BIT(26) |
				(vmem8[1] << 18) | (vmem8[0] << 9) |
				vmem8[3];
			*txbuf32++ = BIT(8) | BIT(17) | BIT(26) |
				(vmem8[2] << 18);
		} else {
			*txbuf32++ = BIT(8) | BIT(17) | BIT(26) |
				(vmem8[1] << 18) | (vmem8[0] << 9);
		}
		v->rem = 0;
	}
	mtp->t.len = (unsigned char *)txbuf32 - (unsigned char *)mtp->t.tx_buf;
	if (!mtp->t.len) {
		return 0;
	}
	v->vmem8 = vmem8;
	mtp->queued = 1;
	ret = spi_async(mtp->par->spi, &mtp->m);
	if (ret < 0) {
		v->rem = 0;
		v->rem_tx = 0;
		v->status = ret;
	}
	return ret;
}

void bus9_complete(void *context)
{
	struct message_transfer_par *mtp = (struct message_transfer_par *)context;

	mtp->queued = 0;
	complete(&mtp->complete);
}

/* 16 bit pixel over 9-bit SPI bus: dc + high byte, dc + low byte */
int fbtft_write_vmem16_bus9(struct fbtft_par *par, size_t offset, size_t len)
{
	struct vars vars;
	struct message_transfer_par mtp[] = {
		[0] = {
			.t = {
				.tx_buf = par->txbuf.buf,
				.tx_dma = par->txbuf.dma,
				.bits_per_word = 27,
			},
			.par = par,
			.max_tx = par->txbuf.len / 8,
			.vars = &vars,
			.queued = 0,
		},
		[1] = {
			.t = {
				.tx_buf = par->txbuf2.buf,
				.tx_dma = par->txbuf2.dma,
				.bits_per_word = 27,
			},
			.par = par,
			.max_tx = par->txbuf2.len / 8,
			.vars = &vars,
			.queued = 0,
		},
	};
	struct message_transfer_par *mtpp;
	int ret = 0;
	int i = 0;

#if 0
	long duration, duration_ms, duration_us;
	long fps;
	struct timespec ts_start, ts_end, ts_duration;
	getnstimeofday(&ts_start);
#endif

	if (!par->spi) {
		dev_err(par->info->device,
			"%s: par->spi is unexpectedly NULL\n", __func__);
		return -1;
	}

	if (!par->txbuf.buf || !par->txbuf2.buf) {
		dev_err(par->info->device, "%s: txbuf.buf is NULL\n", __func__);
		return -1;
	}

	fbtft_par_dbg(DEBUG_WRITE_VMEM, par, "%s(offset=%zu, len=%zu)\n",
		__func__, offset, len);

	/* process 3 pixels at a time */
	vars.rem_tx = (len / 6);
	vars.rem = len - (vars.rem_tx * 6);
	vars.vmem8 = par->info->screen_base + offset;
	vars.status = 0;
	mtpp = &mtp[0];
	spi_message_init(&mtpp->m);
	mtpp->m.complete = bus9_complete;
	mtpp->m.is_dma_mapped = !!par->txbuf.dma;
	mtpp->m.context = mtpp;

	spi_message_add_tail(&mtpp->t, &mtpp->m);

	mtpp = &mtp[1];
	spi_message_init(&mtpp->m);
	mtpp->m.complete = bus9_complete;
	mtpp->m.is_dma_mapped = !!par->txbuf2.dma;
	mtpp->m.context = mtpp;
	spi_message_add_tail(&mtpp->t, &mtpp->m);

	while (1) {
		mtpp = &mtp[i];
		i ^= 1;
		if (mtpp->queued)
			wait_for_completion(&mtpp->complete);

		if (!vars.rem && !vars.rem_tx)
			break;
		init_completion(&mtpp->complete);
		ret = fill_txbuffer(mtpp);
		if (ret < 0)
			break;
	}
	mtpp = &mtp[i];
	if (mtpp->queued)
		wait_for_completion(&mtpp->complete);
#if 0
	getnstimeofday(&ts_end);
//	ts_fps = timespec_sub(ts_start, par->update_time);
//	fps_ms = (ts_fps.tv_sec * 1000) + ((ts_fps.tv_nsec / 1000000) % 1000);
//	fps_us = (ts_fps.tv_nsec / 1000) % 1000;

	ts_duration = timespec_sub(ts_end, ts_start);
	duration_ms = (ts_duration.tv_sec * 1000) + ((ts_duration.tv_nsec / 1000000) % 1000);
	duration_us = (ts_duration.tv_nsec / 1000) % 1000;
	duration = duration_ms * 1000 + duration_us;
	fps = duration ? 1000000000 / duration : 0;

	dev_info(par->info->device,
		"Display update: (%ld.%.3ld ms), fps=%ld.%.3ld\n",
		duration_ms, duration_us, fps / 1000, fps % 1000);
#endif
	return vars.status;
}
EXPORT_SYMBOL(fbtft_write_vmem16_bus9);

#define MIN(a, b) ((a) <= (b)) ? (a) : (b)

/* 18/24 bit pixel over 9-bit SPI bus: dc + high byte, dc + low byte */
int fbtft_write_vmem24_bus9(struct fbtft_par *par, size_t offset, size_t len)
{
	u8 *vmem8;
	u16 *txbuf16 = par->txbuf.buf;
	size_t remain;
	size_t to_copy;
	size_t tx_array_size;
	int line_len = par->info->fix.line_length;
	int active_len = (par->info->var.xres * 3);
	int pad = line_len - active_len;
	int i, row_offset;
	int ret = 0;

	fbtft_par_dbg(DEBUG_WRITE_VMEM, par, "%s(offset=%zu, len=%zu)\n",
		__func__, offset, len);

	if (!par->txbuf.buf) {
		dev_err(par->info->device, "%s: txbuf.buf is NULL\n", __func__);
		return -1;
	}

	remain = len;
	vmem8 = par->info->screen_base + offset;

	tx_array_size = (par->txbuf.len / 6) * 3;

	i = 0;
	row_offset = 0;
	while (remain) {
		to_copy = MIN(remain, tx_array_size - i);
		if (pad) {
			if (to_copy > active_len - row_offset)
				to_copy = active_len - row_offset;
		}
		dev_dbg(par->info->device, "to_copy=%zu, remain=%zu i=%d row_offset=%d\n",
			to_copy, remain, i, row_offset);

#ifdef __LITTLE_ENDIAN
		for (; i < to_copy; i += 3) {
			txbuf16[i]   = 0x0100 | vmem8[i+2];
			txbuf16[i+1] = 0x0100 | vmem8[i+1];
			txbuf16[i+2] = 0x0100 | vmem8[i+0];
		}
#else
		for (; i < to_copy; i++) {
			txbuf16[i]   = 0x0100 | vmem8[i];
		}
#endif
		vmem8 = vmem8 + to_copy;
		row_offset += to_copy;
		remain -= to_copy;
		if (row_offset >= active_len) {
			vmem8 += pad;
			if (remain >= pad)
				remain -= pad;
			else
				remain = 0;
			row_offset = 0;
			if (remain  && (i < tx_array_size))
				continue;
		}

		ret = par->fbtftops.write(par, txbuf16, i * 2);
		if (ret < 0)
			return ret;
		i = 0;
	}

	return ret;
}
EXPORT_SYMBOL(fbtft_write_vmem24_bus9);

int fbtft_write_vmem8_bus8(struct fbtft_par *par, size_t offset, size_t len)
{
	dev_err(par->info->device, "%s: function not implemented\n", __func__);
	return -1;
}
EXPORT_SYMBOL(fbtft_write_vmem8_bus8);

/* 16 bit pixel over 16-bit databus */
int fbtft_write_vmem16_bus16(struct fbtft_par *par, size_t offset, size_t len)
{
	u16 *vmem16;

	fbtft_par_dbg(DEBUG_WRITE_VMEM, par, "%s(offset=%zu, len=%zu)\n",
		__func__, offset, len);

	vmem16 = (u16 *)(par->info->screen_buffer + offset);

	if (par->gpio.dc != -1)
		gpio_set_value(par->gpio.dc, 1);

	/* no need for buffered write with 16-bit bus */
	return par->fbtftops.write(par, vmem16, len);
}
EXPORT_SYMBOL(fbtft_write_vmem16_bus16);
