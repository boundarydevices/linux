/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/hw/hdcpVerify.c
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

#include "hdcpVerify.h"

#define SIZE	(160/8)
#define KSIZE	(1024/8)

void sha_Reset(struct sha_t *sha)
{
	size_t i = 0;

	sha->mIndex = 0;
	sha->mComputed = FALSE;
	sha->mCorrupted = FALSE;
	for (i = 0; i < sizeof(sha->mLength); i++)
		sha->mLength[i] = 0;
	sha->mDigest[0] = 0x67452301;
	sha->mDigest[1] = 0xEFCDAB89;
	sha->mDigest[2] = 0x98BADCFE;
	sha->mDigest[3] = 0x10325476;
	sha->mDigest[4] = 0xC3D2E1F0;
}

int sha_Result(struct sha_t *sha)
{
	if (sha->mCorrupted == TRUE)
		return FALSE;
	if (sha->mComputed == FALSE) {
		sha_PadMessage(sha);
		sha->mComputed = TRUE;
	}
	return TRUE;
}

void sha_Input(struct sha_t *sha, const uint8_t *data, size_t size)
{
	int i = 0;
	unsigned int j = 0;
	int rc = TRUE;

	if (data == 0 || size == 0) {
		pr_info("invalid input data");
		return;
	}
	if (sha->mComputed == TRUE || sha->mCorrupted == TRUE) {
		sha->mCorrupted = TRUE;
		return;
	}
	while (size-- && sha->mCorrupted == FALSE) {
		sha->mBlock[sha->mIndex++] = *data;

		for (i = 0; i < 8; i++) {
			rc = TRUE;
			for (j = 0; j < sizeof(sha->mLength); j++) {
				sha->mLength[j]++;
				if (sha->mLength[j] != 0) {
					rc = FALSE;
					break;
				}
			}
			sha->mCorrupted = (sha->mCorrupted == TRUE ||
				rc == TRUE) ? TRUE : FALSE;
		}
		/* if corrupted then message is too long */
		if (sha->mIndex == 64)
			sha_ProcessBlock(sha);
		data++;
	}
}

void sha_ProcessBlock(struct sha_t *sha)
{
#define shaCircularShift(bits, word) \
	((((word) << (bits)) & 0xFFFFFFFF) | ((word) >> (32-(bits))))

	const unsigned int K[] = {
		/* constants defined in SHA-1 */
		0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
	unsigned int W[80]; /* word sequence */
	unsigned int A, B, C, D, E; /* word buffers */
	unsigned int temp = 0;
	int t = 0;

	/* Initialize the first 16 words in the array W */
	for (t = 0; t < 80; t++) {
		if (t < 16) {
			W[t] = ((unsigned int) sha->mBlock[t * 4 + 0]) << 24;
			W[t] |= ((unsigned int) sha->mBlock[t * 4 + 1]) << 16;
			W[t] |= ((unsigned int) sha->mBlock[t * 4 + 2]) << 8;
			W[t] |= ((unsigned int) sha->mBlock[t * 4 + 3]) << 0;
		} else
			W[t] = shaCircularShift(1,
				W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
	}

	A = sha->mDigest[0];
	B = sha->mDigest[1];
	C = sha->mDigest[2];
	D = sha->mDigest[3];
	E = sha->mDigest[4];

	for (t = 0; t < 80; t++) {
		temp = shaCircularShift(5, A);
		if (t < 20)
			temp += ((B & C) | ((~B) & D)) + E + W[t] + K[0];
		else if (t < 40)
			temp += (B ^ C ^ D) + E + W[t] + K[1];
		else if (t < 60)
			temp += ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
		else
			temp += (B ^ C ^ D) + E + W[t] + K[3];
		E = D;
		D = C;
		C = shaCircularShift(30, B);
		B = A;
		A = (temp & 0xFFFFFFFF);
	}

	sha->mDigest[0] = (sha->mDigest[0] + A) & 0xFFFFFFFF;
	sha->mDigest[1] = (sha->mDigest[1] + B) & 0xFFFFFFFF;
	sha->mDigest[2] = (sha->mDigest[2] + C) & 0xFFFFFFFF;
	sha->mDigest[3] = (sha->mDigest[3] + D) & 0xFFFFFFFF;
	sha->mDigest[4] = (sha->mDigest[4] + E) & 0xFFFFFFFF;

	sha->mIndex = 0;
}

void sha_PadMessage(struct sha_t *sha)
{
	/*
	 *  Check to see if the current message block is too small to hold
	 *  the initial padding bits and length.  If so, we will pad the
	 *  block, process it, and then continue padding into a second
	 *  block.
	 */
	if (sha->mIndex > 55) {
		sha->mBlock[sha->mIndex++] = 0x80;
		while (sha->mIndex < 64)
			sha->mBlock[sha->mIndex++] = 0;
		sha_ProcessBlock(sha);
		while (sha->mIndex < 56)
			sha->mBlock[sha->mIndex++] = 0;
	} else {
		sha->mBlock[sha->mIndex++] = 0x80;
		while (sha->mIndex < 56)
			sha->mBlock[sha->mIndex++] = 0;
	}

	/* Store the message length as the last 8 octets */
	sha->mBlock[56] = sha->mLength[7];
	sha->mBlock[57] = sha->mLength[6];
	sha->mBlock[58] = sha->mLength[5];
	sha->mBlock[59] = sha->mLength[4];
	sha->mBlock[60] = sha->mLength[3];
	sha->mBlock[61] = sha->mLength[2];
	sha->mBlock[62] = sha->mLength[1];
	sha->mBlock[63] = sha->mLength[0];

	sha_ProcessBlock(sha);
}

int hdcpVerify_DSA(const uint8_t *M, size_t n, const uint8_t *r,
	const uint8_t *s)
{
	int i = 0;
	struct sha_t sha;
	static const uint8_t q[] = {
		0xE7, 0x08, 0xC7, 0xF9, 0x4D, 0x3F, 0xEF, 0x97,
		0xE2, 0x14, 0x6D, 0xCD,	0x6A, 0xB5, 0x6D, 0x5E,
		0xCE, 0xF2, 0x8A, 0xEE };
	static const uint8_t p[] = {
		0x27, 0x75, 0x28, 0xF3, 0x2B, 0x80, 0x59, 0x8C,
		0x11, 0xC2, 0xED, 0x46, 0x1C, 0x95, 0x39, 0x2A,
		0x54, 0x19, 0x89, 0x96, 0xFD, 0x49, 0x8A, 0x02,
		0x3B, 0x73, 0x75, 0x32, 0x14, 0x9C, 0x7B, 0x5C,
		0x49, 0x20, 0x98, 0xB9, 0x07, 0x32, 0x3F, 0xA7,
		0x30, 0x15, 0x72, 0xB3, 0x09, 0x55, 0x71, 0x10,
		0x3A, 0x4C, 0x97, 0xD1, 0xBC, 0xA0, 0x04, 0xF4,
		0x35, 0xCF, 0x47, 0x54, 0x0E, 0xA7, 0x2B, 0xE5,
		0x83, 0xB9, 0xC6, 0xD4, 0x47, 0xC7, 0x44, 0xB8,
		0x67, 0x76, 0x7C, 0xAE, 0x0C, 0xDC, 0x34, 0x4F,
		0x4B, 0x9E, 0x96, 0x1D, 0x82, 0x84, 0xD2, 0xA0,
		0xDC, 0xE0, 0x00, 0xF5, 0x64, 0xA1, 0x7F, 0x8E,
		0xFF, 0x58, 0x70, 0x6A,	0xC3, 0x4F, 0xA2, 0xA1,
		0xB8, 0xC7, 0x52, 0x5A, 0x35, 0x5B, 0x39, 0x17,
		0x6B, 0x78, 0x43, 0x93, 0xF7, 0x75, 0x8D, 0x01,
		0xB7, 0x61, 0x17, 0xFD, 0xB2, 0xF5, 0xC3, 0xD3 };
	static const uint8_t g[] = {
		0xD9, 0x0B, 0xBA, 0xC2, 0x42, 0x24, 0x46, 0x69,
		0x5B, 0x40, 0x67, 0x2F, 0x5B, 0x18, 0x3F, 0xB9,
		0xE8, 0x6F, 0x21, 0x29, 0xAC, 0x7D, 0xFA, 0x51,
		0xC2, 0x9D, 0x4A, 0xAB, 0x8A, 0x9B, 0x8E, 0xC9,
		0x42, 0x42, 0xA5, 0x1D, 0xB2, 0x69, 0xAB, 0xC8,
		0xE3, 0xA5, 0xC8, 0x81, 0xBE, 0xB6, 0xA0, 0xB1,
		0x7F, 0xBA, 0x21, 0x2C, 0x64, 0x35, 0xC8, 0xF7,
		0x5F, 0x58, 0x78, 0xF7, 0x45, 0x29, 0xDD, 0x92,
		0x9E, 0x79, 0x3D, 0xA0, 0x0C, 0xCD, 0x29, 0x0E,
		0xA9, 0xE1, 0x37, 0xEB, 0xBF, 0xC6, 0xED, 0x8E,
		0xA8, 0xFF, 0x3E, 0xA8, 0x7D, 0x97, 0x62, 0x51,
		0xD2, 0xA9, 0xEC, 0xBD, 0x4A, 0xB1, 0x5D, 0x8F,
		0x11, 0x86, 0x27, 0xCD, 0x66, 0xD7, 0x56, 0x5D,
		0x31, 0xD7, 0xBE, 0xA9, 0xAC, 0xDE, 0xAF, 0x02,
		0xB5, 0x1A, 0xDE, 0x45, 0x24, 0x3E, 0xE4, 0x1A,
		0x13, 0x52, 0x4D, 0x6A, 0x1B, 0x5D, 0xF8, 0x92 };
	static const uint8_t y[] = {
		0x99, 0x37, 0xE5, 0x36, 0xFA, 0xF7, 0xA9, 0x62,
		0x83, 0xFB, 0xB3, 0xE9,	0xF7, 0x9D, 0x8F, 0xD8,
		0xCB, 0x62, 0xF6, 0x66, 0x8D, 0xDC, 0xC8, 0x95,
		0x10, 0x24, 0x6C, 0x88, 0xBD, 0xFF, 0xB7, 0x7B,
		0xE2, 0x06, 0x52, 0xFD, 0xF7, 0x5F, 0x43, 0x62,
		0xE6, 0x53, 0x65, 0xB1, 0x38, 0x90, 0x25, 0x87,
		0x8D, 0xA4, 0x9E, 0xFE, 0x56, 0x08, 0xA7, 0xA2,
		0x0D, 0x4E, 0xD8, 0x43, 0x3C, 0x97, 0xBA, 0x27,
		0x6C, 0x56, 0xC4, 0x17, 0xA4, 0xB2, 0x5C, 0x8D,
		0xDB, 0x04, 0x17, 0x03, 0x4F, 0xE1, 0x22, 0xDB,
		0x74, 0x18, 0x54, 0x1B, 0xDE, 0x04, 0x68, 0xE1,
		0xBD, 0x0B, 0x4F, 0x65, 0x48, 0x0E, 0x95, 0x56,
		0x8D, 0xA7, 0x5B, 0xF1,	0x55, 0x47, 0x65, 0xE7,
		0xA8, 0x54, 0x17, 0x8A, 0x65, 0x76, 0x0D, 0x4F,
		0x0D, 0xFF, 0xAC, 0xA3, 0xE0, 0xFB, 0x80, 0x3A,
		0x86, 0xB0, 0xA0, 0x6B, 0x52, 0x00, 0x06, 0xC7 };
	uint8_t w[SIZE];
	uint8_t z[SIZE];
	uint8_t u1[SIZE];
	uint8_t u2[SIZE];
	uint8_t gu1[KSIZE];
	uint8_t yu2[KSIZE];
	uint8_t pro[KSIZE];
	uint8_t v[SIZE];

	/* adapt to the expected format by arithmetic functions */
	uint8_t r1[SIZE];
	uint8_t s1[SIZE];

	sha_Reset(&sha);
	hdcpVerify_ArrayCPY(r1, r, sizeof(r1));
	hdcpVerify_ArrayCPY(s1, s, sizeof(s1));
	hdcpVerify_ArraySWP(r1, sizeof(r1));
	hdcpVerify_ArraySWP(s1, sizeof(s1));

	hdcpVerify_ComputeINV(w, s1, q, sizeof(w));
	sha_Input(&sha, M, n);
	if (sha_Result(&sha) == TRUE) {
		for (i = 0; i < 5; i++) {
			z[i * 4 + 0] = sha.mDigest[i] >> 24;
			z[i * 4 + 1] = sha.mDigest[i] >> 16;
			z[i * 4 + 2] = sha.mDigest[i] >> 8;
			z[i * 4 + 3] = sha.mDigest[i] >> 0;
		}
		hdcpVerify_ArraySWP(z, sizeof(z));
	} else {
		pr_info("cannot digest message");
		return FALSE;
	}
	if (hdcpVerify_ComputeMUL(u1, z, w, q, sizeof(u1)) == FALSE)
		return FALSE;
	if (hdcpVerify_ComputeMUL(u2, r1, w, q, sizeof(u2)) == FALSE)
		return FALSE;
	if (hdcpVerify_ComputeEXP(gu1, g, u1, p, sizeof(gu1), sizeof(u1))
		== FALSE)
		return FALSE;
	if (hdcpVerify_ComputeEXP(yu2, y, u2, p, sizeof(yu2), sizeof(u2))
		 == FALSE)
		return FALSE;
	if (hdcpVerify_ComputeMUL(pro, gu1, yu2, p, sizeof(pro)) == FALSE)
		return FALSE;
	if (hdcpVerify_ComputeMOD(v, pro, q, sizeof(v)) == FALSE)
		return FALSE;
	return hdcpVerify_ArrayCMP(v, r1, sizeof(v)) == 0;
}

int hdcpVerify_ArrayADD(uint8_t *r, const uint8_t *a, const uint8_t *b,
	size_t n)
{
	uint8_t c = 0;
	size_t i = 0;

	for (i = 0; i < n; i++) {
		u16 s = a[i] + b[i] + c;

		c = (uint8_t) (s >> 8);
		r[i] = (uint8_t) s;
	}
	return c;
}

int hdcpVerify_ArrayCMP(const uint8_t *a, const uint8_t *b, size_t n)
{
	int i = 0;

	for (i = n; i > 0; i--) {
		if (a[i - 1] > b[i - 1])
			return 1;
		else if (a[i - 1] < b[i - 1])
			return -1;
	}
	return 0;
}

void hdcpVerify_ArrayCPY(uint8_t *dst, const uint8_t *src, size_t n)
{
	size_t i = 0;

	for (i = 0; i < n; i++)
		dst[i] = src[i];
}

int hdcpVerify_ArrayDIV(uint8_t *r, const uint8_t *D, const uint8_t *d,
	size_t n)
{
	int i = 0;

	if (r == D || r == d || (!hdcpVerify_ArrayTST(d, 0, n)) == TRUE) {
		pr_info("invalid input data");
		return FALSE;
	}
	hdcpVerify_ArraySET(&r[n], 0, n);
	hdcpVerify_ArrayCPY(r, D, n);
	for (i = n; i > 0; i--) {
		r[i - 1 + n] = 0;
		while (hdcpVerify_ArrayCMP(&r[i - 1], d, n) >= 0) {
			hdcpVerify_ArraySUB(&r[i - 1], &r[i - 1], d, n);
			r[i - 1 + n] += 1;
		}
	}
	return TRUE;
}

int hdcpVerify_ArrayMAC(uint8_t *r, const uint8_t *M, const uint8_t m, size_t n)
{
	u16 c = 0;
	size_t i = 0;

	for (i = 0; i < n; i++) {
		u16 p = (M[i] * m) + c + r[i];

		c = p >> 8;
		r[i] = (uint8_t) p;
	}
	return (uint8_t) c;
}

int hdcpVerify_ArrayMUL(uint8_t *r, const uint8_t *M, const uint8_t *m,
	size_t n)
{
	size_t i = 0;

	if (r == M || r == m) {
		pr_info("invalid input data");
		return FALSE;
	}
	hdcpVerify_ArraySET(r, 0, n);
	for (i = 0; i < n; i++) {
		if (m[i] == 0)
			continue;
		else if (m[i] == 1)
			hdcpVerify_ArrayADD(&r[i], &r[i], M, n - i);
		else
			hdcpVerify_ArrayMAC(&r[i], M, m[i], n - i);
	}
	return TRUE;
}

void hdcpVerify_ArraySET(uint8_t *dst, const uint8_t src, size_t n)
{
	size_t i = 0;

	for (i = 0; i < n; i++)
		dst[i] = src;
}

int hdcpVerify_ArraySUB(uint8_t *r, const uint8_t *a, const uint8_t *b,
	size_t n)
{
	uint8_t c = 1;
	size_t i = 0;

	for (i = 0; i < n; i++) {
		u16 s = ((uint8_t) a[i] + (uint8_t) (~b[i])) + c;

		c = (uint8_t) (s >> 8);
		r[i] = (uint8_t) s;
	}
	return c;
}

void hdcpVerify_ArraySWP(uint8_t *r, size_t n)
{
	size_t i = 0;

	for (i = 0; i < (n / 2); i++) {
		uint8_t tmp = r[i];

		r[i] = r[n - 1 - i];
		r[n - 1 - i] = tmp;
	}
}

int hdcpVerify_ArrayTST(const uint8_t *a, const uint8_t b, size_t n)
{
	size_t i = 0;

	for (i = 0; i < n; i++) {
		if (a[i] != b)
			return FALSE;
	}
	return TRUE;
}

int hdcpVerify_ComputeEXP(uint8_t *c, const uint8_t *M, const uint8_t *e,
	const uint8_t *p, size_t n, size_t nE)
{
	int i = 8 * nE - 1;
	int rc = TRUE;

	/* LR Binary Method */
	if ((e[i / 8] & (1 << (i % 8))) != 0)
		hdcpVerify_ArrayCPY(c, M, n);
	else {
		hdcpVerify_ArraySET(c, 0, n);
		c[0] = 1;
	}
	for (i -= 1; i >= 0; i--) {
		rc |= hdcpVerify_ComputeMUL(c, c, c, p, n);
		if ((e[i / 8] & (1 << (i % 8))) != 0)
			rc &= hdcpVerify_ComputeMUL(c, c, M, p, n);
	}
	return rc;
}

int hdcpVerify_ComputeINV(uint8_t *out, const uint8_t *z, const uint8_t *a,
	size_t n)
{
	uint8_t w[2][SIZE];
	uint8_t x[2][SIZE];
	uint8_t y[2][SIZE];
	uint8_t r[2*SIZE];
	uint8_t *i, *j, *q, *t;
	uint8_t *x1, *x2;
	uint8_t *y1, *y2;

	if ((n > SIZE) || (hdcpVerify_ArrayTST(z, 0, n) == TRUE)
			|| (hdcpVerify_ArrayTST(a, 0, n) == TRUE)
			|| (hdcpVerify_ArrayCMP(z, a, n) >= 0)) {
		pr_info("invalid input data");
		return FALSE;
	}

	hdcpVerify_ArrayCPY(w[0], a, n);
	hdcpVerify_ArrayCPY(w[1], z, n);
	i = w[0];
	j = w[1];

	hdcpVerify_ArraySET(x[1], 0, n);
	x[1][0] = 1;
	hdcpVerify_ArraySET(x[0], 0, n);
	x2 = x[1];
	x1 = x[0];

	hdcpVerify_ArraySET(y[1], 0, n);
	hdcpVerify_ArraySET(y[0], 0, n);
	y[0][0] = 1;
	y2 = y[1];
	y1 = y[0];

	do {
		hdcpVerify_ArrayDIV(r, i, j, n);
		hdcpVerify_ArrayCPY(i, r, n);
		q = &r[n];
		t = i; /* swap i <-> j */
		i = j;
		j = t;

		hdcpVerify_ArrayMUL(r, x1, q, n);
		hdcpVerify_ArraySUB(x2, x2, r, n);
		t = x2; /* swap x1 <-> x2 */
		x2 = x1;
		x1 = t;

		hdcpVerify_ArrayMUL(r, y1, q, n);
		hdcpVerify_ArraySUB(y2, y2, r, n);
		t = y2; /* swap y1 <-> y2 */
		y2 = y1;
		y1 = t;

	} while (hdcpVerify_ArrayTST(j, 0, n) == FALSE);

	j[0] = 1;
	if (hdcpVerify_ArrayCMP(i, j, n) != 0) {
		pr_info("i != 1");
		return FALSE;
	}
	hdcpVerify_ArrayCPY(out, y2, n);
	return TRUE;
}

int hdcpVerify_ComputeMOD(uint8_t *dst, const uint8_t *src, const uint8_t *p,
	size_t n)
{
	uint8_t aux[KSIZE];
	uint8_t ext[SIZE + 1];
	uint8_t tmp[2 * (KSIZE + 1)];
	int i = 0;

	if (n > SIZE) {
		pr_info("invalid input data");
		return FALSE;
	}
	hdcpVerify_ArrayCPY(aux, src, sizeof(aux));
	/* TODO: remove extension */
	hdcpVerify_ArrayCPY(ext, p, n);
	ext[n] = 0;
	for (i = sizeof(aux)-n-1; i >= 0; i--) {
		hdcpVerify_ArrayDIV(tmp, &aux[i], ext, n+1);
		hdcpVerify_ArrayCPY(&aux[i], tmp, n+1);
	}
	hdcpVerify_ArrayCPY(dst, aux, n);
	return TRUE;
}

int hdcpVerify_ComputeMUL(uint8_t *p, const uint8_t *a, const uint8_t *b,
	const uint8_t *m, size_t n)
{
	uint8_t aux[2 * KSIZE + 1];
	uint8_t ext[KSIZE + 1];
	uint8_t tmp[2 * (KSIZE + 1)];
	size_t i = 0;
	int j = 0;

	if (n > KSIZE) {
		pr_info("invalid input data");
		return FALSE;
	}
	hdcpVerify_ArraySET(aux, 0, sizeof(aux));
	for (i = 0; i < n; i++) {
		/* TODO: extension was faster */
		aux[n+i] = hdcpVerify_ArrayMAC(&aux[i], a, b[i], n);
	}
	/* TODO: reuse ComputeMOD */
	hdcpVerify_ArrayCPY(ext, m, n);
	ext[n] = 0;
	for (j = n; j >= 0; j--) {
		hdcpVerify_ArrayDIV(tmp, &aux[j], ext, n+1);
		hdcpVerify_ArrayCPY(&aux[j], tmp, n+1);
	}
	hdcpVerify_ArrayCPY(p, aux, n);
	return TRUE;
}

int hdcpVerify_KSV(const uint8_t *data, size_t size)
{
	size_t i = 0;
	struct sha_t sha;

	if (data == 0 || size < (HEADER + SHAMAX)) {
		pr_info("invalid input data");
		return FALSE;
	}
	sha_Reset(&sha);
	sha_Input(&sha, data, size - SHAMAX);
	if (sha_Result(&sha) == FALSE) {
		pr_info("cannot process SHA digest");
		return FALSE;
	}
	for (i = 0; i < SHAMAX; i++) {
		if (data[size - SHAMAX + i] != (uint8_t) (sha.mDigest[i / 4]
			>> ((i % 4) * 8))) {
			pr_info("SHA digest does not match");
			return FALSE;
		}
	}
	return TRUE;
}

int hdcpVerify_SRM(const uint8_t *data, size_t size)
{
	if (data == 0 || size < (VRL_HEADER + VRL_NUMBER + 2 * DSAMAX)) {
		pr_info("invalid input data");
		return FALSE;
	}
	/* M, n, r, s */
	return hdcpVerify_DSA(data, size - 2 * DSAMAX, &data[size - 2 * DSAMAX],
		&data[size - DSAMAX]);
}

