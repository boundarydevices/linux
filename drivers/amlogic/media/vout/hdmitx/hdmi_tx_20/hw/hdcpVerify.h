/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/hdcpVerify.h
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

#ifndef HDCPVERIFY_H_
#define HDCPVERIFY_H_

#include <linux/types.h>
#include <linux/printk.h>
#include "hdcp.h"

struct sha_t {
	uint8_t mLength[8];
	uint8_t mBlock[64];
	int mIndex;
	int mComputed;
	int mCorrupted;
	unsigned int mDigest[5];
};

#define KSV_LEN 5
#define KSV_MSK 0x7F
#define VRL_LENGTH 0x05
#define VRL_HEADER 5
#define VRL_NUMBER 3
#define HEADER 10
#define SHAMAX 20
#define DSAMAX 20

void sha_Reset(struct sha_t *sha);

int sha_Result(struct sha_t *sha);

void sha_Input(struct sha_t *sha, const uint8_t *data, size_t size);

void sha_ProcessBlock(struct sha_t *sha);

void sha_PadMessage(struct sha_t *sha);

int hdcpVerify_DSA(const uint8_t *M, size_t n, const uint8_t *r,
	const uint8_t *s);

int hdcpVerify_ArrayADD(uint8_t *r, const uint8_t *a, const uint8_t *b,
	size_t n);

int hdcpVerify_ArrayCMP(const uint8_t *a, const uint8_t *b, size_t n);

void hdcpVerify_ArrayCPY(uint8_t *dst, const uint8_t *src, size_t n);

int hdcpVerify_ArrayDIV(uint8_t *r, const uint8_t *D, const uint8_t *d,
	size_t n);

int hdcpVerify_ArrayMAC(uint8_t *r, const uint8_t *M, const uint8_t m,
	size_t n);

int hdcpVerify_ArrayMUL(uint8_t *r, const uint8_t *M, const uint8_t *m,
	size_t n);

void hdcpVerify_ArraySET(uint8_t *dst, const uint8_t src, size_t n);

int hdcpVerify_ArraySUB(uint8_t *r, const uint8_t *a, const uint8_t *b,
	size_t n);

void hdcpVerify_ArraySWP(uint8_t *r, size_t n);

int hdcpVerify_ArrayTST(const uint8_t *a, const uint8_t b, size_t n);

int hdcpVerify_ComputeEXP(uint8_t *c, const uint8_t *M, const uint8_t *e,
	const uint8_t *p, size_t n, size_t nE);

int hdcpVerify_ComputeINV(uint8_t *out, const uint8_t *z, const uint8_t *a,
	size_t n);

int hdcpVerify_ComputeMOD(uint8_t *dst, const uint8_t *src, const uint8_t *p,
	size_t n);

int hdcpVerify_ComputeMUL(uint8_t *p, const uint8_t *a, const uint8_t *b,
	const uint8_t *m, size_t n);

int hdcpVerify_KSV(const uint8_t *data, size_t size);

int hdcpVerify_SRM(const uint8_t *data, size_t size);

#endif /* HDCPVERIFY_H_ */
