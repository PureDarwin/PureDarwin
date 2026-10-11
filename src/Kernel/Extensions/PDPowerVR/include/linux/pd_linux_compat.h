#ifndef _PD_LINUX_COMPAT_H
#define _PD_LINUX_COMPAT_H

// the few linux types and macros the imagination firmware interface headers use, so they stay unmodified
#include <stdint.h>
#include <stddef.h>

#ifndef __cplusplus
#include <stdbool.h>
#ifndef static_assert
#define static_assert _Static_assert
#endif
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef u64 aligned_u64 __attribute__((aligned(8)));
typedef u64 dma_addr_t;

#ifndef __aligned
#define __aligned(x)	__attribute__((aligned(x)))
#endif
#ifndef __packed
#define __packed	__attribute__((packed))
#endif

#ifndef BIT
#define BIT(n)		(1UL << (n))
#endif
#ifndef BITS_PER_LONG
#define BITS_PER_LONG	64
#endif
#ifndef BIT_MASK
#define BIT_MASK(nr)	(1UL << ((nr) % BITS_PER_LONG))
#endif
#ifndef BIT_WORD
#define BIT_WORD(nr)	((nr) / BITS_PER_LONG)
#endif
#ifndef BIT_ULL
#define BIT_ULL(n)	(1ULL << (n))
#endif
#ifndef ALIGN
#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((__typeof__(x))(a) - 1))
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))
#endif

#define SZ_4K		0x00001000
#define SZ_64K		0x00010000
#define SZ_1M		0x00100000
#define SZ_2M		0x00200000
#define SZ_16M		0x01000000
#define SZ_256M		0x10000000
#define SZ_4G		0x100000000ULL
#define SZ_16G		0x400000000ULL

#endif
