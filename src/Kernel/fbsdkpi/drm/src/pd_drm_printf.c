// SPDX-License-Identifier: MIT
// printf for drm code, with linux's %p extensions

#include <sys/types.h>
#include <stdarg.h>
#include <linux/printk.h>
#include <linux/string.h>

#undef printf
#undef vprintf

int printf(const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int pd_drm_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

#define PD_PRINT_MAX	1024

struct pd_out {
	char *buf;
	size_t size, len;
};

static void
pd_put(struct pd_out *o, const char *s, size_t n)
{
	while (n-- > 0 && *s != '\0') {
		if (o->len + 1 < o->size)
			o->buf[o->len] = *s;
		o->len++;
		s++;
	}
	if (o->size > 0)
		o->buf[o->len < o->size ? o->len : o->size - 1] = '\0';
}

int
pd_drm_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	struct pd_out o = { buf, size, 0 };
	char spec[32], piece[256];

	if (size > 0)
		buf[0] = '\0';
	while (*fmt != '\0') {
		const char *start = fmt;
		size_t n = 0;

		if (*fmt != '%') {
			while (fmt[n] != '\0' && fmt[n] != '%')
				n++;
			pd_put(&o, fmt, n);
			fmt += n;
			continue;
		}
		if (fmt[1] == '%') {
			pd_put(&o, "%", 1);
			fmt += 2;
			continue;
		}

		// flags, width and precision, with * taken from the arguments
		int star[2], nstar = 0;
		fmt++;
		while (*fmt != '\0' && strchr("-+ #0", *fmt) != NULL)
			fmt++;
		if (*fmt == '*') {
			star[nstar++] = va_arg(ap, int);
			fmt++;
		} else {
			while (*fmt >= '0' && *fmt <= '9')
				fmt++;
		}
		if (*fmt == '.') {
			fmt++;
			if (*fmt == '*') {
				star[nstar++] = va_arg(ap, int);
				fmt++;
			} else {
				while (*fmt >= '0' && *fmt <= '9')
					fmt++;
			}
		}
		// length
		int lng = 0;
		bool size_t_arg = false;
		if (*fmt == 'h') {
			fmt++;
			if (*fmt == 'h')
				fmt++;
		} else if (*fmt == 'l') {
			lng = 1;
			fmt++;
			if (*fmt == 'l') {
				lng = 2;
				fmt++;
			}
		} else if (*fmt == 'z' || *fmt == 't' || *fmt == 'j') {
			size_t_arg = true;
			fmt++;
		} else if (*fmt == 'L') {
			lng = 2;
			fmt++;
		}
		char conv = *fmt;
		if (conv == '\0')
			break;
		fmt++;

		n = (size_t)(fmt - start);
		if (n >= sizeof(spec))
			n = sizeof(spec) - 1;
		memcpy(spec, start, n);
		spec[n] = '\0';

		if (conv == 'p') {
			void *ptr = va_arg(ap, void *);
			char ext = *fmt;

			if (ext == 'V') {
				struct va_format *vaf = ptr;
				va_list nested;

				fmt++;
				if (vaf == NULL)
					continue;
				va_copy(nested, *vaf->va);
				pd_drm_vsnprintf(piece, sizeof(piece), vaf->fmt, nested);
				va_end(nested);
				pd_put(&o, piece, sizeof(piece));
				continue;
			}
			if (ext == 'e') {
				// an error pointer prints as its number
				fmt++;
				snprintf(piece, sizeof(piece), "%ld", (long)(intptr_t)ptr);
			} else if (ext == 'a') {
				// a physical or dma address passed by reference
				fmt++;
				if (*fmt == 'd' || *fmt == 'p')
					fmt++;
				snprintf(piece, sizeof(piece), "0x%llx",
				    ptr != NULL ? (unsigned long long)*(uint64_t *)ptr : 0ULL);
			} else {
				// other forms (%ps, %pS, %ph, %pOF...) print the pointer
				while ((*fmt >= 'a' && *fmt <= 'z') || (*fmt >= 'A' && *fmt <= 'Z'))
					fmt++;
				snprintf(piece, sizeof(piece), "%p", ptr);
			}
			pd_put(&o, piece, sizeof(piece));
			continue;
		}

		switch (conv) {
		case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': case 'c':
			if (size_t_arg) {
				size_t v = va_arg(ap, size_t);
				char z[32];
				// the kernel snprintf may not know z, so print it as a long
				size_t k = strlen(spec);
				memcpy(z, spec, k - 2);
				z[k - 2] = 'l';
				z[k - 1] = conv;
				z[k] = '\0';
				if (nstar == 1)
					snprintf(piece, sizeof(piece), z, star[0], (long)v);
				else if (nstar == 2)
					snprintf(piece, sizeof(piece), z, star[0], star[1], (long)v);
				else
					snprintf(piece, sizeof(piece), z, (long)v);
			} else if (lng == 2) {
				long long v = va_arg(ap, long long);
				if (nstar == 1)
					snprintf(piece, sizeof(piece), spec, star[0], v);
				else if (nstar == 2)
					snprintf(piece, sizeof(piece), spec, star[0], star[1], v);
				else
					snprintf(piece, sizeof(piece), spec, v);
			} else if (lng == 1) {
				long v = va_arg(ap, long);
				if (nstar == 1)
					snprintf(piece, sizeof(piece), spec, star[0], v);
				else if (nstar == 2)
					snprintf(piece, sizeof(piece), spec, star[0], star[1], v);
				else
					snprintf(piece, sizeof(piece), spec, v);
			} else {
				int v = va_arg(ap, int);
				if (nstar == 1)
					snprintf(piece, sizeof(piece), spec, star[0], v);
				else if (nstar == 2)
					snprintf(piece, sizeof(piece), spec, star[0], star[1], v);
				else
					snprintf(piece, sizeof(piece), spec, v);
			}
			break;
		case 's': {
			const char *v = va_arg(ap, const char *);
			if (v == NULL)
				v = "(null)";
			if (nstar == 1)
				snprintf(piece, sizeof(piece), spec, star[0], v);
			else if (nstar == 2)
				snprintf(piece, sizeof(piece), spec, star[0], star[1], v);
			else
				snprintf(piece, sizeof(piece), spec, v);
			break;
		}
		default:
			// floating point and anything unknown is not printed by the kernel
			piece[0] = '\0';
			break;
		}
		pd_put(&o, piece, sizeof(piece));
	}
	return (int)o.len;
}

int
pd_drm_vprintf(const char *fmt, va_list ap)
{
	char buf[PD_PRINT_MAX];
	int n = pd_drm_vsnprintf(buf, sizeof(buf), fmt, ap);

	printf("%s", buf);
	return n;
}

int
pd_drm_printf(const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = pd_drm_vprintf(fmt, ap);
	va_end(ap);
	return n;
}
