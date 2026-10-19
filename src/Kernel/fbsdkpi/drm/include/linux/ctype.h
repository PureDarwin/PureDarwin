// SPDX-License-Identifier: MIT
// linux/ctype.h, the ascii classes drm parses mode strings and ioctl names with
#ifndef _PD_LINUX_CTYPE_H_
#define _PD_LINUX_CTYPE_H_

#define isascii(c) (((c) & ~0x7f) == 0)
#define isdigit(c) ((c) >= '0' && (c) <= '9')
#define isgraph(c) ((c) > ' ' && (c) < 0x7f)
#ifndef isprint
#define isprint(c) ((c) >= ' ' && (c) <= '~')
#endif
#define isspace(c) ((c) == ' ' || ((c) >= '\t' && (c) <= '\r'))
#define isupper(c) ((c) >= 'A' && (c) <= 'Z')
#define islower(c) ((c) >= 'a' && (c) <= 'z')
#define isalpha(c) (isupper(c) || islower(c))
#define isalnum(c) (isalpha(c) || isdigit(c))
#define isxdigit(c) (isdigit(c) || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#define tolower(c) (isupper(c) ? (c) - 'A' + 'a' : (c))
#define toupper(c) (islower(c) ? (c) - 'a' + 'A' : (c))

#endif
