// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Ming Hsiu Tsai <minghsiu.tsai@mediatek.com>
 *         Rick Chang <rick.chang@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/videodev2.h>

#include "mtk_jpeg_dec_parse.h"

#define TEM	0x01
#define SOF0	0xc0
#define DHT	0xc4
#define RST	0xd0
#define SOI	0xd8
#define EOI	0xd9

struct mtk_jpeg_stream {
	u8 *addr;
	u32 size;
	u32 curr;
};

static int read_byte(struct mtk_jpeg_stream *stream)
{
	if (stream->curr >= stream->size)
		return -1;
	return stream->addr[stream->curr++];
}

static int read_word_be(struct mtk_jpeg_stream *stream, u32 *word)
{
	u32 temp;
	int byte;

	byte = read_byte(stream);
	if (byte == -1)
		return -1;
	temp = byte << 8;
	byte = read_byte(stream);
	if (byte == -1)
		return -1;
	*word = (u32)byte | temp;

	return 0;
}

static void read_skip(struct mtk_jpeg_stream *stream, long len)
{
	if (len <= 0)
		return;
	while (len--)
		read_byte(stream);
}

static bool parse_header(struct mtk_jpeg_stream *stream,
			 struct mtk_jpeg_dec_param *param)
{
	int i, byte;
	u32 word;

	/* length */
	if (read_word_be(stream, &word))
		return false;

	/* precision */
	if (read_byte(stream) == -1)
		return false;

	if (read_word_be(stream, &word))
		return false;
	param->pic_h = word;

	if (read_word_be(stream, &word))
		return false;
	param->pic_w = word;

	param->comp_num = read_byte(stream);
	if (param->comp_num != 1 && param->comp_num != 3)
		return false;

	for (i = 0; i < param->comp_num; i++) {
		param->comp_id[i] = read_byte(stream);
		if (param->comp_id[i] == -1)
			break;

		/* sampling */
		byte = read_byte(stream);
		if (byte == -1)
			break;
		param->sampling_w[i] = (byte >> 4) & 0x0F;
		param->sampling_h[i] = byte & 0x0F;

		param->qtbl_num[i] = read_byte(stream);
		if (param->qtbl_num[i] == -1)
			break;
	}

	return (i == param->comp_num);
}

static bool mtk_jpeg_do_parse(struct mtk_jpeg_dec_param *param, u8 *src_addr_va,
			      u32 src_size)
{
	bool found = false;
	bool file_end = false;
	struct mtk_jpeg_stream stream;

	stream.addr = src_addr_va;
	stream.size = src_size;
	stream.curr = 0;
	while (!(file_end || (param->huffman_tb_exist && found))) {
		int length, byte;
		u32 word;

		byte = read_byte(&stream);
		if (byte == -1)
			return false;
		if (byte != 0xff)
			continue;
		do
			byte = read_byte(&stream);
		while (byte == 0xff);
		if (byte == -1)
			return false;
		if (byte == 0)
			continue;

		length = 0;
		switch (byte) {
		case SOF0:
			found = parse_header(&stream, param);
			break;
		case RST ... RST + 7:
		case SOI:
		case TEM:
			break;
		case EOI:
			file_end = true;
			break;
		case DHT:
			param->huffman_tb_exist = 1;
			break;
		default:
			if (read_word_be(&stream, &word))
				break;
			length = (long)word - 2;
			read_skip(&stream, length);
			break;
		}
	}

	return found;
}

bool mtk_jpeg_parse(struct mtk_jpeg_dec_param *param, u8 *src_addr_va,
		    u32 src_size)
{
	if (!mtk_jpeg_do_parse(param, src_addr_va, src_size))
		return false;
	if (mtk_jpeg_dec_fill_param(param))
		return false;

	return true;
}
