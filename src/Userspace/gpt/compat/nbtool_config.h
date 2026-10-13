// what NetBSD's tool build expects from the host, filled in for PureDarwin
#ifndef PD_GPT_NBTOOL_CONFIG_H
#define PD_GPT_NBTOOL_CONFIG_H

#include <sys/types.h>
#include <stdint.h>
#include <sys/disk.h>

#define HAVE_SYS_ENDIAN_H 1
#define __RCSID(x)
#define __FBSDID(x)
#define __COPYRIGHT(x)
#define __UNCONST(a) ((void *)(uintptr_t)(const void *)(a))
#define __arraycount(a) (sizeof(a) / sizeof((a)[0]))
#define __dead __attribute__((__noreturn__))
#define __printflike(f, a) __attribute__((__format__(__printf__, f, a)))
#define __packed __attribute__((__packed__))

// sector size maps straight onto Darwin's, media size is worked out in pd_gpt_ioctl
#define DIOCGSECTORSIZE DKIOCGETBLOCKSIZE
#define DIOCGMEDIASIZE  0x7064676d
int pd_gpt_ioctl(int fd, unsigned long request, void *arg);
#define ioctl(fd, request, arg) pd_gpt_ioctl((fd), (request), (arg))

#define _DIAGASSERT(e) ((void)0)

int stravis(char **, const char *, int);
uintmax_t strtou(const char *, char **, int, uintmax_t, uintmax_t, int *);

int humanize_number(char *, size_t, int64_t, const char *, int, int);
int dehumanize_number(const char *, int64_t *);
#define HN_DECIMAL     0x01
#define HN_NOSPACE     0x02
#define HN_B           0x04
#define HN_DIVISOR_1000 0x08
#define HN_GETSCALE    0x10
#define HN_AUTOSCALE   0x20

#endif
