// pd_msm_host.h - the kernel interfaces PDDRMmsm uses, as the linux host tests provide them
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
#ifndef PD_MSM_HOST_H
#define PD_MSM_HOST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int64_t s64;

#define __iomem
#define __init
#define READ_ONCE(x)            (*(const volatile __typeof__(x) *)&(x))
#define wmb()                   __sync_synchronize()
#define rmb()                   __sync_synchronize()
#define nitems(a)               (sizeof(a) / sizeof((a)[0]))
#define ARRAY_SIZE(a)           nitems(a)
#define min_t(t, a, b)          ((t)(a) < (t)(b) ? (t)(a) : (t)(b))
#define round_up(x, y)          ((((x) + (y) - 1) / (y)) * (y))
#define le32_to_cpu(x)          (x)
#define container_of(p, t, m)   ((t *)((char *)(p) - offsetof(t, m)))
#define PAGE_SIZE               4096
#define GFP_KERNEL              0
#define MAX_ERRNO               4095

static inline void *ERR_PTR(long e) { return ((void *)e); }
static inline long PTR_ERR(const void *p) { return ((long)p); }
static inline bool IS_ERR(const void *p) { return ((unsigned long)p >= (unsigned long)-MAX_ERRNO); }

// every message the driver prints goes through here, shown with PD_MSM_TEST_VERBOSE=1
int fake_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#define printf fake_log

// time: udelay advances a fake clock and lets the fake hardware take a step
void udelay(unsigned long us);
void usleep_range(unsigned long lo, unsigned long hi);
u64 ktime_get_ns(void);

// mmio: plain memory unless a fake window has hooks
u32 readl(const volatile void *addr);
void writel(u32 v, volatile void *addr);
u64 readq(const volatile void *addr);
void writeq(u64 v, volatile void *addr);
void iounmap(volatile void *addr);

// memory, counted so a test can see leaks
void *kzalloc(size_t size, int flags);
void kfree(const void *p);
void kvfree(const void *p);
void *kvmalloc_array(size_t n, size_t size, int flags);

// a mutex that catches taking it twice
struct mutex { int held; };
void mutex_init(struct mutex *m);
void mutex_lock(struct mutex *m);
void mutex_unlock(struct mutex *m);

// a small device tree built by each test
struct fake_win;
struct device_node;
struct of_phandle_args {
    struct device_node *np;
    int args_count;
    u32 args[8];
};
struct resource { u64 start, end; };
static inline u64 resource_size(const struct resource *r) { return (r->end - r->start + 1); }
struct device { struct device_node *of_node; };
struct platform_device { struct device dev; };

int of_parse_phandle_with_args(struct device_node *np, const char *list, const char *cells, int index,
    struct of_phandle_args *out);
struct device_node *of_parse_phandle(struct device_node *np, const char *name, int index);
struct device_node *of_get_child_by_name(struct device_node *np, const char *name);
int of_address_to_resource(struct device_node *np, int index, struct resource *r);
void *of_iomap(struct device_node *np, int index);
int of_property_match_string(struct device_node *np, const char *prop, const char *s);
bool of_device_is_compatible(struct device_node *np, const char *compat);

// clocks and power domains
struct clk {
    const char *name;
    int enables;        // prepare_enable calls
    unsigned long rate;
    int fail;           // the error enable or set_rate returns
};
struct clk_bulk_data { const char *id; struct clk *clk; };
struct clk *of_clk_get(struct device_node *np, int index);
int clk_prepare_enable(struct clk *c);
int clk_set_rate(struct clk *c, unsigned long rate);
unsigned long clk_get_rate(struct clk *c);
int pd_power_domains_on(struct device_node *np);

// firmware files, registered by name
struct firmware { size_t size; const u8 *data; };
int request_firmware(const struct firmware **fw, const char *name, struct device *dev);
void release_firmware(const struct firmware *fw);

// just enough drm for the driver's own objects
struct drm_device { int unused; };
struct drm_file { int unused; };
struct drm_gem_object {
    size_t size;
    int refcount;
    struct drm_device *dev;
};
void drm_gem_object_put(struct drm_gem_object *obj);

// the pd os layer
void *pdos_malloc_contig(size_t size, size_t align, u64 maxaddr, u64 *phys);
void pdos_free_contig(void *p, size_t size);
void *pdos_map_phys(u64 phys, size_t size, int uncached);
void pdos_unmap_phys(void *p, size_t size);
int pdos_bootarg_int(const char *name, int def);

#endif
