/* SPDX-License-Identifier: GPL-2.0 */
/*
 * kabi_ex.h
 *
 * Copyright (C) 2024 Google LLC
 *
 * Examples for kABI stability features with --stable.
 */

/*
 * The comments below each example contain the expected gendwarfksyms
 * output, which can be verified using LLVM's FileCheck tool:
 *
 * https://llvm.org/docs/CommandGuide/FileCheck.html
 *
 * Usage:
 *
 * $ gcc -g -c examples/kabi_ex.c -o examples/kabi_ex.o
 *
 * $ nm examples/kabi_ex.o | awk '{ print $NF }' | \
 * 	./gendwarfksyms --stable --dump-dies \
 * 		examples/kabi_ex.o 2>&1 >/dev/null | \
 * 	FileCheck examples/kabi_ex.h --check-prefix=STABLE
 */

#ifndef __KABI_EX_H__
#define __KABI_EX_H__

#include "kabi.h"

/*
 * Example: kABI rules
 */

struct s {
	int a;
};

KABI_DECLONLY(s);

/*
 * STABLE:      variable structure_type s {
 * STABLE-NEXT: }
 */

enum e {
	A,
	B,
	C,
	D,
};

KABI_ENUMERATOR_IGNORE(e, B);
KABI_ENUMERATOR_IGNORE(e, C);
KABI_ENUMERATOR_VALUE(e, D, 123456789);

/*
 * STABLE:      variable enumeration_type e {
 * STABLE-NEXT:   enumerator A = 0 ,
 * STABLE-NEXT:   enumerator D = 123456789
 * STABLE-NEXT: } byte_size(4)
*/

/*
 * Example: Reserved fields
 */
struct ex0a {
	int a;
	KABI_RESERVE(0);
	KABI_RESERVE(1);
};

/*
 * STABLE:      variable structure_type ex0a {
 * STABLE-NEXT:   member base_type int byte_size(4) encoding(5) a data_member_location(0) ,
 * STABLE-NEXT:   member base_type [[ULONG:long unsigned int|unsigned long]] byte_size(8) encoding(7) data_member_location(8) ,
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) data_member_location(16)
 * STABLE-NEXT: } byte_size(24)
 */

struct ex0b {
	int a;
	KABI_RESERVE(0);
	KABI_USE2(1, int b, int c);
};

/*
 * STABLE:      variable structure_type ex0b {
 * STABLE-NEXT:   member base_type int byte_size(4) encoding(5) a data_member_location(0) ,
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) data_member_location(8) ,
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) data_member_location(16)
 * STABLE-NEXT: } byte_size(24)
 */

struct ex0c {
	int a;
	KABI_USE(0, void *p);
	KABI_USE2(1, int b, int c);
};

/*
 * STABLE:      variable structure_type ex0c {
 * STABLE-NEXT:   member base_type int byte_size(4) encoding(5) a data_member_location(0) ,
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) data_member_location(8) ,
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) data_member_location(16)
 * STABLE-NEXT: } byte_size(24)
 */

/*
 * Example: A reserved array
 */

struct ex1a {
	unsigned int a;
	KABI_RESERVE_ARRAY(0, 64);
};

/*
 * STABLE:      variable structure_type ex1a {
 * STABLE-NEXT:   member base_type unsigned int byte_size(4) encoding(7) a data_member_location(0) ,
 * STABLE-NEXT:   member array_type[64] {
 * STABLE-NEXT:     base_type unsigned char byte_size(1) encoding(8)
 * STABLE-NEXT:   } data_member_location(8)
 * STABLE-NEXT: } byte_size(72)
 */

struct ex1b {
	unsigned int a;
	KABI_USE_ARRAY(
		0, 64, struct {
			void *p;
			KABI_RESERVE_ARRAY(1, 56);
		});
};

/*
 * STABLE:      variable structure_type ex1b {
 * STABLE-NEXT:   member base_type unsigned int byte_size(4) encoding(7) a data_member_location(0) ,
 * STABLE-NEXT:   member array_type[64] {
 * STABLE-NEXT:     base_type unsigned char byte_size(1) encoding(8)
 * STABLE-NEXT:   } data_member_location(8)
 * STABLE-NEXT: } byte_size(72)
 */

struct ex1c {
	unsigned int a;
	KABI_USE_ARRAY(0, 64, void *p[8]);
};

/*
 * STABLE:      variable structure_type ex1c {
 * STABLE-NEXT:   member base_type unsigned int byte_size(4) encoding(7) a data_member_location(0) ,
 * STABLE-NEXT:   member array_type[64] {
 * STABLE-NEXT:     base_type unsigned char byte_size(1) encoding(8)
 * STABLE-NEXT:   } data_member_location(8)
 * STABLE-NEXT: } byte_size(72)
 */

/*
 * Example: An ignored field added to an alignment hole
 */

struct ex2a {
	int a;
	unsigned long b;
	int c;
	unsigned long d;
};

/*
 * STABLE:      variable structure_type ex2a {
 * STABLE-NEXT:   member base_type int byte_size(4) encoding(5) a data_member_location(0) ,
 * STABLE-NEXT:   member base_type [[ULONG:long unsigned int|unsigned long]] byte_size(8) encoding(7) b data_member_location(8)
 * STABLE-NEXT:   member base_type int byte_size(4) encoding(5) c data_member_location(16) ,
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) d data_member_location(24)
 * STABLE-NEXT: } byte_size(32)
 */

struct ex2b {
	int a;
	KABI_IGNORE(0, unsigned int n);
	unsigned long b;
	int c;
	unsigned long d;
};

_Static_assert(sizeof(struct ex2a) == sizeof(struct ex2b), "ex2a size doesn't match ex2b");

/*
 * STABLE:      variable structure_type ex2b {
 * STABLE-NEXT:   member base_type int byte_size(4) encoding(5) a data_member_location(0) ,
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) b data_member_location(8)
 * STABLE-NEXT:   member base_type int byte_size(4) encoding(5) c data_member_location(16) ,
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) d data_member_location(24)
 * STABLE-NEXT: } byte_size(32)
 */

struct ex2c {
	int a;
	KABI_IGNORE(0, unsigned int n);
	unsigned long b;
	int c;
	KABI_IGNORE(1, unsigned int m);
	unsigned long d;
};

_Static_assert(sizeof(struct ex2a) == sizeof(struct ex2c), "ex2a size doesn't match ex2c");

/*
 * STABLE:      variable structure_type ex2c {
 * STABLE-NEXT:   member base_type int byte_size(4) encoding(5) a data_member_location(0) ,
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) b data_member_location(8)
 * STABLE-NEXT:   member base_type int byte_size(4) encoding(5) c data_member_location(16) ,
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) d data_member_location(24)
 * STABLE-NEXT: } byte_size(32)
 */


/*
 * Example: A replaced field
 */

struct ex3a {
	unsigned long a;
	unsigned long unused;
};

/*
 * STABLE:      variable structure_type ex3a {
 * STABLE-NEXT:   member base_type [[ULONG:long unsigned int|unsigned long]] byte_size(8) encoding(7) a data_member_location(0)
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) unused data_member_location(8)
 * STABLE-NEXT: } byte_size(16)
 */

struct ex3b {
	unsigned long a;
	KABI_REPLACE(unsigned long, unused, unsigned long renamed);
};

_Static_assert(sizeof(struct ex3a) == sizeof(struct ex3b), "ex3a size doesn't match ex3b");

/*
 * STABLE:      variable structure_type ex3b {
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) a data_member_location(0)
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) unused data_member_location(8)
 * STABLE-NEXT: } byte_size(16)
 */

struct ex3c {
	unsigned long a;
	KABI_REPLACE(unsigned long, unused, long replaced);
};

_Static_assert(sizeof(struct ex3a) == sizeof(struct ex3c), "ex3a size doesn't match ex3c");

/*
 * STABLE:      variable structure_type ex3c {
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) a data_member_location(0)
 * STABLE-NEXT:   member base_type [[ULONG]] byte_size(8) encoding(7) unused data_member_location(8)
 * STABLE-NEXT: } byte_size(16)
 */

#endif /* __KABI_EX_H__ */
