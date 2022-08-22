// SPDX-License-Identifier: GPL-2.0
/*
 * rational fractions
 *
 * Copyright (C) 2009 emlix GmbH, Oskar Schirmer <oskar@scara.com>
 * Copyright (C) 2019 Trent Piepho <tpiepho@gmail.com>
 *
 * helper functions when coping with rational numbers
 */

#include <linux/rational.h>
#include <linux/compiler.h>
#include <linux/export.h>
#include <linux/minmax.h>
#include <linux/limits.h>
#include <linux/module.h>

/*
 * continued fraction
 *      2  1  2  1  2
 * 0 1  2  3  8 11 30
 * 1 0  1  1  3  4 11
 */
void rational_best_ratio_bigger(unsigned long *pnum, unsigned long *pdenom, unsigned max_n, unsigned max_d)
{
	unsigned long a = *pnum;
	unsigned long b = *pdenom;
	unsigned long c;
	unsigned n0 = 0;
	unsigned n1 = 1;
	unsigned d0 = 1;
	unsigned d1 = 0;
	unsigned _n = 0;
	unsigned _d = 1;
	unsigned whole;

	while (b) {
		whole = a / b;
		/* n0/d0 is the earlier term */
		n0 = n0 + (n1 * whole);
		d0 = d0 + (d1 * whole);

		c = a - (b * whole);
		a = b;
		b = c;

		if (b) {
			/* n1/d1 is the earlier term */
			whole = a / b;
			_n = n1 + (n0 * whole);
			_d = d1 + (d0 * whole);
		} else {
			_n = n0;
			_d = d0;
		}
		pr_debug("%s: cf=%i %d/%d, %d/%d\n", __func__, whole, n0, d0, _n, _d);
		if ((_n > max_n) || (_d > max_d)) {
			unsigned h;

			h = n0;
			if (h) {
				_n = max_n - n1;
				_n /= h;
				if (whole > _n)
					whole = _n;
			}
			h = d0;
			if (h) {
				_d = max_d - d1;
				_d /= h;
				if (whole > _d)
					whole = _d;
			}
			_n = n1 + (n0 * whole);
			_d = d1 + (d0 * whole);
			pr_debug("%s: b=%ld, n=%d of %d, d=%d of %d\n", __func__, b, _n, max_n, _d, max_d);
			if (!_d) {
				/* Don't choose infinite for a bigger ratio */
				_n = n0 + 1;
				_d = d0;
				pr_err("%s: %d/%d is too big\n", __func__, _n, _d);
			}
			break;
		}

		if (!b)
			break;
		n1 = _n;
		d1 = _d;
		c = a - (b * whole);
		a = b;
		b = c;
	}

	*pnum = _n;
	*pdenom = _d;
}
EXPORT_SYMBOL(rational_best_ratio_bigger);

/*
 * calculate best rational approximation for a given fraction
 * taking into account restricted register size, e.g. to find
 * appropriate values for a pll with 5 bit denominator and
 * 8 bit numerator register fields, trying to set up with a
 * frequency ratio of 3.1415, one would say:
 *
 * rational_best_approximation(31415, 10000,
 *		(1 << 8) - 1, (1 << 5) - 1, &n, &d);
 *
 * you may look at given_numerator as a fixed point number,
 * with the fractional part size described in given_denominator.
 *
 * for theoretical background, see:
 * https://en.wikipedia.org/wiki/Continued_fraction
 */

void rational_best_approximation(
	unsigned long given_numerator, unsigned long given_denominator,
	unsigned long max_numerator, unsigned long max_denominator,
	unsigned long *best_numerator, unsigned long *best_denominator)
{
	/* n/d is the starting rational, which is continually
	 * decreased each iteration using the Euclidean algorithm.
	 *
	 * dp is the value of d from the prior iteration.
	 *
	 * n2/d2, n1/d1, and n0/d0 are our successively more accurate
	 * approximations of the rational.  They are, respectively,
	 * the current, previous, and two prior iterations of it.
	 *
	 * a is current term of the continued fraction.
	 */
	unsigned long n, d, n0, d0, n1, d1, n2, d2;
	n = given_numerator;
	d = given_denominator;
	n0 = d1 = 0;
	n1 = d0 = 1;

	for (;;) {
		unsigned long dp, a;

		if (d == 0)
			break;
		/* Find next term in continued fraction, 'a', via
		 * Euclidean algorithm.
		 */
		dp = d;
		a = n / d;
		d = n % d;
		n = dp;

		/* Calculate the current rational approximation (aka
		 * convergent), n2/d2, using the term just found and
		 * the two prior approximations.
		 */
		n2 = n0 + a * n1;
		d2 = d0 + a * d1;

		/* If the current convergent exceeds the maxes, then
		 * return either the previous convergent or the
		 * largest semi-convergent, the final term of which is
		 * found below as 't'.
		 */
		if ((n2 > max_numerator) || (d2 > max_denominator)) {
			unsigned long t = ULONG_MAX;

			if (d1)
				t = (max_denominator - d0) / d1;
			if (n1)
				t = min(t, (max_numerator - n0) / n1);

			/* This tests if the semi-convergent is closer than the previous
			 * convergent.  If d1 is zero there is no previous convergent as this
			 * is the 1st iteration, so always choose the semi-convergent.
			 */
			if (!d1 || 2u * t > a || (2u * t == a && d0 * dp > d1 * d)) {
				n1 = n0 + t * n1;
				d1 = d0 + t * d1;
			}
			break;
		}
		n0 = n1;
		n1 = n2;
		d0 = d1;
		d1 = d2;
	}
	*best_numerator = n1;
	*best_denominator = d1;
}

EXPORT_SYMBOL(rational_best_approximation);

MODULE_LICENSE("GPL v2");
