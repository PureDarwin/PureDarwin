#ifndef ATOM_OS_H
#define ATOM_OS_H

// the linux kernel helpers atom.c uses, over iokit
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <kern/clock.h>
#include <IOKit/IOLib.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define GFP_KERNEL 0

// xnu has its own kfree, so the linux allocators get private names
#define kzalloc atom_os_kzalloc
#define kcalloc atom_os_kcalloc
#define kfree atom_os_kfree

// kfree takes no size, so a header in front of each block keeps it
static inline void *kzalloc(size_t size, int flags)
{
    (void)flags;
    size_t *p = (size_t *)IOMallocZero(size + 16);
    if (!p) return NULL;
    *p = size + 16;
    return (char *)p + 16;
}

static inline void *kcalloc(size_t n, size_t size, int flags) { return kzalloc(n * size, flags); }

static inline void kfree(const void *ptr)
{
    if (!ptr) return;
    size_t *p = (size_t *)((char *)ptr - 16);
    IOFree(p, *p);
}

// the interpreter runs from one thread at a time in this driver
struct mutex { int unused; };
#define mutex_lock(m) do { (void)(m); } while (0)
#define mutex_unlock(m) do { (void)(m); } while (0)

#define udelay(us) IODelay(us)
#define mdelay(ms) IODelay((ms) * 1000)
#define msleep(ms) IOSleep(ms)

// jiffies counted in milliseconds
static inline unsigned long atom_os_ms(void)
{
    uint64_t ns;
    absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
    return (unsigned long)(ns / 1000000);
}
#define jiffies atom_os_ms()
#define jiffies_to_msecs(j) (j)
#define time_after(a, b) ((long)((b) - (a)) < 0)

#define pr_info(...) IOLog("PDAmdGPU: atom: " __VA_ARGS__)
#define DRM_ERROR(...) IOLog("PDAmdGPU: atom: " __VA_ARGS__)
#define printk(...) IOLog(__VA_ARGS__)
#define KERN_DEBUG ""

#define drm_can_sleep() 1
#define lower_32_bits(n) ((u32)(n))
#define upper_32_bits(n) ((u32)((u64)(n) >> 32))
#define do_div(n, base) ({ u32 __rem = (u32)((n) % (base)); (n) /= (base); __rem; })

#define str_yes_no(b) ((b) ? "yes" : "no")
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)

static inline u32 get_unaligned_le32(const void *p)
{
    u32 v;
    memcpy(&v, p, 4);
    return v;
}

#endif
