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
	u16 *txbuf16 = (u16 *)par->txbuf[0].buf;
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
	if (!par->txbuf[0].buf)
		return par->fbtftops.write(par, vmem16, len);

	/* buffered write */
	tx_array_size = par->txbuf[0].len / 2;

	if (par->startbyte) {
		txbuf16 = (u16 *)(par->txbuf[0].buf + 1);
		tx_array_size -= 2;
		*(u8 *)(par->txbuf[0].buf) = par->startbyte | 0x2;
		startbyte_size = 1;
	}

	while (remain) {
		to_copy = min(tx_array_size, remain);
		dev_dbg(par->info->device, "    to_copy=%zu, remain=%zu\n",
						to_copy, remain - to_copy);

		for (i = 0; i < to_copy; i++)
			txbuf16[i] = cpu_to_be16(vmem16[i]);

		vmem16 = vmem16 + to_copy;
		ret = par->fbtftops.write(par, par->txbuf[0].buf,
						startbyte_size + to_copy * 2);
		if (ret < 0)
			return ret;
		remain -= to_copy;
	}

	return ret;
}
EXPORT_SYMBOL(fbtft_write_vmem16_bus8);

#if 0
#define IN_BYTES_PER_GROUP	6
#define OUT_BYTES_PER_GROUP	8
#define OUT_BITS_PER_WORD	27
#else
#define IN_BYTES_PER_GROUP	32
#define OUT_BYTES_PER_GROUP	36
#define OUT_BITS_PER_WORD	32
#endif


static void fill_txbuffer(struct message_transfer_par *mtp, u8 *vmem8, size_t group_cnt, size_t rem)
{
	u32 *txbuf32 = (u32 *)mtp->t.tx_buf;
	int i;

	int shift = 64 - 18;
	int limit = IN_BYTES_PER_GROUP * group_cnt;
	u32 v;
	u64 val = 0;

	if (group_cnt < mtp->max_tx)
		limit += rem;
	for (i = 0; i < limit; i += 2, vmem8 += 2) {
		v = BIT(17) | BIT(8) | (vmem8[1] << 9) | vmem8[0];
		val |= (u64)v << shift;
		shift -= 18;
		if (shift <= (64 - OUT_BITS_PER_WORD - 18)) {
			*txbuf32++ = (val >> (64 - OUT_BITS_PER_WORD));
			val <<= OUT_BITS_PER_WORD;
			shift += OUT_BITS_PER_WORD;
		}
	}
	if (shift < 64 - 18) {
		v = val >> 32;
		shift += 18 - 32;
		*txbuf32++ = (v | ((1 << shift) - 1)) >> (32 - OUT_BITS_PER_WORD);
	}

	mtp->t.len = (unsigned char *)txbuf32 - (unsigned char *)mtp->t.tx_buf;
}

#ifdef USE_WORKQ
static void txbuf_worker(struct work_struct *work)
{
	struct txbuf_work_struct *w = container_of(work, struct txbuf_work_struct, work);
	struct message_transfer_par *mtp = container_of(w, struct message_transfer_par, txbuf_work);

	fill_txbuffer(mtp, w->vmem8, w->group_cnt, w->rem);
}
#endif

static void bus9_complete(void *context)
{
	struct message_transfer_par *mtp = (struct message_transfer_par *)context;

	mtp->queued = 0;
	mtp->t.len = 0;
	complete(&mtp->complete);
}

/* 16 bit pixel over 9-bit SPI bus: dc + high byte, dc + low byte */
int fbtft_write_vmem16_bus9(struct fbtft_par *par, size_t offset, size_t len)
{
#ifdef USE_WORKQ
	struct txbuf_work_struct *w;
#endif
	struct message_transfer_par *mtp;
	struct spi_transfer *t;
	struct txbuf *tx;
	u8 *vmem8;
	size_t rem, rem_tx;
	size_t group_cnt;
	int ret = 0;
	int i;

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

	if (!par->txbuf[0].buf || !par->txbuf_cnt) {
		dev_err(par->info->device, "%s: txbuf[0].buf is NULL\n", __func__);
		return -1;
	}

	fbtft_par_dbg(DEBUG_WRITE_VMEM, par, "%s(offset=%zu, len=%zu)\n",
		__func__, offset, len);

	/* process 3 pixels at a time */
	rem_tx = (len / IN_BYTES_PER_GROUP);
	rem = len - (rem_tx * IN_BYTES_PER_GROUP);
	vmem8 = par->info->screen_base + offset;

	for (i = 0; i < par->txbuf_cnt; i++) {
		tx = &par->txbuf[i];
		mtp = &par->_mtp[i];

		mtp->max_tx = tx->len / OUT_BYTES_PER_GROUP;
		mtp->queued = 0;
		mtp->t.len = 0;
#ifdef USE_WORKQ
		w = &mtp->txbuf_work;
		INIT_WORK(&w->work, txbuf_worker);
#endif
	}

	i = 0;
	while (1) {
		tx = &par->txbuf[i];
		mtp = &par->_mtp[i++];
		if (i >= par->txbuf_cnt)
			i = 0;
		dev_dbg(par->info->device, "rem_tx=%zu rem=%zu, i=%d\n",
				rem_tx, rem, i);
#ifdef USE_WORKQ
		w = &mtp->txbuf_work;
		flush_work(&w->work);
		if (mtp->t.len) {
			init_completion(&mtp->complete);
			mtp->queued = 1;
			ret = spi_async(par->spi, &mtp->m);
			if (ret < 0) {
				rem_tx = rem = 0;
				mtp->t.len = mtp->queued = 0;
			}
		}
#endif

		if (mtp->queued)
			wait_for_completion(&mtp->complete);

		if (!rem_tx && !rem) {
			mtp = &par->_mtp[i];
#ifdef USE_WORKQ
			w = &mtp->txbuf_work;
			flush_work(&w->work);
#endif
			if (mtp->t.len)
				continue;
			break;
		}

		spi_message_init(&mtp->m);
		mtp->m.complete = bus9_complete;
		mtp->m.context = mtp;
		mtp->m.is_dma_mapped = !!tx->dma;

		t = &mtp->t;
		memset(t, 0, sizeof(*t));
		t->tx_buf = tx->buf;
		t->tx_dma = tx->dma;
		t->bits_per_word = OUT_BITS_PER_WORD;

		spi_message_add_tail(t, &mtp->m);

		group_cnt = rem_tx > mtp->max_tx ? mtp->max_tx : rem_tx;
#ifdef USE_WORKQ
		w->vmem8 = vmem8;
		w->group_cnt = group_cnt;
		w->rem = rem;
		schedule_work(&w->work);
#else
		fill_txbuffer(mtp, vmem8, group_cnt, rem);
		if (mtp->t.len) {
			init_completion(&mtp->complete);
			mtp->queued = 1;
			ret = spi_async(par->spi, &mtp->m);
			if (ret < 0) {
				rem_tx = rem = 0;
				mtp->t.len = mtp->queued = 0;
			}
		}
#endif
		rem_tx -= group_cnt;
		if (group_cnt < mtp->max_tx)
			rem = 0;
		vmem8 += group_cnt * IN_BYTES_PER_GROUP;
	}
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
	return ret;
}
EXPORT_SYMBOL(fbtft_write_vmem16_bus9);

#define MIN(a, b) ((a) <= (b)) ? (a) : (b)

/* 18/24 bit pixel over 9-bit SPI bus: dc + high byte, dc + low byte */
int fbtft_write_vmem24_bus9(struct fbtft_par *par, size_t offset, size_t len)
{
	u8 *vmem8;
	u16 *txbuf16 = par->txbuf[0].buf;
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

	if (!txbuf16) {
		dev_err(par->info->device, "%s: txbuf[0].buf is NULL\n", __func__);
		return -1;
	}

	remain = len;
	vmem8 = par->info->screen_base + offset;

	tx_array_size = (par->txbuf[0].len / 6) * 3;

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
