// pdnv_fmt.c - a vsnprintf for the resource manager's format strings, written to the Linux kernel's rules
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
//
// xnu's printf reads some of Linux's conversions differently (the %p extensions, the z, t and j lengths),
// and one misread shifts every argument after it, so the resource manager's strings go through this instead
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "pdnv_fmt.h"

#define KPTR(p)     ((uintptr_t)(p) >= 0xffff800000000000ull)

struct out {
	char *buf;
	size_t size;
	size_t n;           // what would have been written, as vsnprintf returns
};

static void
put(struct out *o, char c)
{
	if (o->n + 1 < o->size)
		o->buf[o->n] = c;
	o->n++;
}

static void
pad(struct out *o, char c, int count)
{
	while (count-- > 0)
		put(o, c);
}

// one integer: base, sign, '#' prefix, width and precision as printf has them
static void
number(struct out *o, uint64_t v, int neg, int base, int upper, int width, int prec, int left, int zero,
    int plus, int space, int alt)
{
	const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	char tmp[24];
	int len = 0, extra;
	char sign = neg ? '-' : plus ? '+' : space ? ' ' : 0;
	const char *prefix = alt && v != 0 ? (base == 16 ? (upper ? "0X" : "0x") : base == 8 ? "0" : "") : "";
	int plen = prefix[0] == '\0' ? 0 : prefix[1] == '\0' ? 1 : 2;

	do {
		tmp[len++] = digits[v % (uint64_t)base];
		v /= (uint64_t)base;
	} while (v != 0 && len < (int)sizeof(tmp));
	if (prec == 0 && len == 1 && tmp[0] == '0')
		len = 0;

	extra = (prec > len ? prec - len : 0);
	width -= len + extra + plen + (sign != 0);
	if (!left && !(zero && prec < 0))
		pad(o, ' ', width);
	if (sign)
		put(o, sign);
	for (int i = 0; i < plen; i++)
		put(o, prefix[i]);
	if (!left && zero && prec < 0)
		pad(o, '0', width);
	pad(o, '0', extra);
	while (len > 0)
		put(o, tmp[--len]);
	if (left)
		pad(o, ' ', width);
}

static void
text(struct out *o, const char *s, int width, int prec, int left)
{
	int len = 0;

	if (s == NULL)
		s = "(null)";
	else if (!KPTR(s))
		s = "(bad ptr)";
	while (s[len] != '\0' && (prec < 0 || len < prec))
		len++;
	if (!left)
		pad(o, ' ', width - len);
	for (int i = 0; i < len; i++)
		put(o, s[i]);
	if (left)
		pad(o, ' ', width - len);
}

int
pdnv_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	struct out o = { buf, size, 0 };

	if (!KPTR(fmt))
		fmt = "(bad format)";

	for (const char *f = fmt; *f != '\0'; f++) {
		int left = 0, zero = 0, plus = 0, space = 0, alt = 0, width = 0, prec = -1, len = 0;
		int base = 10, upper = 0, is_signed = 0;
		uint64_t v;

		if (*f != '%') {
			put(&o, *f);
			continue;
		}
		f++;

		// flags
		for (;; f++) {
			if (*f == '-')
				left = 1;
			else if (*f == '0')
				zero = 1;
			else if (*f == '+')
				plus = 1;
			else if (*f == ' ')
				space = 1;
			else if (*f == '#')
				alt = 1;
			else
				break;
		}

		// width and precision, either can come from the arguments
		if (*f == '*') {
			width = va_arg(ap, int);
			if (width < 0)
				left = 1, width = -width;
			f++;
		} else {
			while (*f >= '0' && *f <= '9')
				width = width * 10 + (*f++ - '0');
		}
		if (*f == '.') {
			f++;
			prec = 0;
			if (*f == '*') {
				prec = va_arg(ap, int);
				f++;
			} else {
				while (*f >= '0' && *f <= '9')
					prec = prec * 10 + (*f++ - '0');
			}
		}

		// lengths: 1 hh, 2 h, 3 l, 4 ll, 5 z/t/j (all 64-bit on x86-64)
		if (*f == 'h') {
			len = 2, f++;
			if (*f == 'h')
				len = 1, f++;
		} else if (*f == 'l') {
			len = 3, f++;
			if (*f == 'l')
				len = 4, f++;
		} else if (*f == 'z' || *f == 't' || *f == 'j' || *f == 'L' || *f == 'q') {
			len = 5, f++;
		}

		switch (*f) {
		case 'd':
		case 'i':
			is_signed = 1;
			break;
		case 'u':
			break;
		case 'x':
			base = 16;
			break;
		case 'X':
			base = 16, upper = 1;
			break;
		case 'o':
			base = 8;
			break;
		case 'c':
			if (!left)
				pad(&o, ' ', width - 1);
			put(&o, (char)va_arg(ap, int));
			if (left)
				pad(&o, ' ', width - 1);
			continue;
		case 's':
			text(&o, va_arg(ap, const char *), width, prec, left);
			continue;
		case 'p':
			// Linux's %p is bare hex, its format strings add their own 0x. The %pX extensions take the
			// same pointer argument, the letters after it are consumed
			v = (uint64_t)(uintptr_t)va_arg(ap, void *);
			while ((f[1] >= 'a' && f[1] <= 'z') || (f[1] >= 'A' && f[1] <= 'Z') || (f[1] >= '0' && f[1] <= '9'))
				f++;
			number(&o, v, 0, 16, 0, width, prec, left, zero, 0, 0, alt);
			continue;
		case '%':
			put(&o, '%');
			continue;
		case '\0':
			f--;
			continue;
		default:
			// unknown: shown as is, no argument taken
			put(&o, '%');
			put(&o, *f);
			continue;
		}

		if (is_signed) {
			int64_t s = len >= 3 ? va_arg(ap, int64_t) : (int64_t)va_arg(ap, int);

			if (len == 1)
				s = (signed char)s;
			else if (len == 2)
				s = (short)s;
			v = s < 0 ? (uint64_t)-s : (uint64_t)s;
			number(&o, v, s < 0, base, upper, width, prec, left, zero, plus, space, alt);
		} else {
			v = len >= 3 ? va_arg(ap, uint64_t) : (uint64_t)va_arg(ap, unsigned int);
			if (len == 1)
				v = (unsigned char)v;
			else if (len == 2)
				v = (unsigned short)v;
			number(&o, v, 0, base, upper, width, prec, left, zero, 0, 0, alt);
		}
	}
	if (size > 0)
		buf[o.n < size ? o.n : size - 1] = '\0';
	return (int)o.n;
}
