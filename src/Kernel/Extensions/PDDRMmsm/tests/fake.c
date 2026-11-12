// fake.c - the fake soc the PDDRMmsm host tests run the driver against
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
#include <stdarg.h>
#include <assert.h>

#include "fake.h"

#undef printf

struct fake_counts fake_live;
struct fake_smc fake_smcs[64];
int fake_nsmc;
void (*fake_smc_handler)(const u64 in[7], u64 out[4], u64 *out6);
void (*fake_tick)(void);

static u64 sNow;

#define MAX_WINS 64
static struct fake_win *sWins[MAX_WINS];
static int sNwins;
static struct device_node *sNodes[64];
static int sNnodes;
static struct { const char *name; const void *data; size_t size; } sFw[8];
static int sNfw;

int
fake_log(const char *fmt, ...)
{
    static int verbose = -1;
    va_list ap;

    if (verbose < 0)
        verbose = getenv("PD_MSM_TEST_VERBOSE") != NULL;
    if (!verbose)
        return (0);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    return (0);
}

u64
fake_now_ns(void)
{
    return (sNow);
}

void
udelay(unsigned long us)
{
    sNow += us * 1000;
    if (fake_tick != NULL)
        fake_tick();
}

void
usleep_range(unsigned long lo, unsigned long hi)
{
    udelay(lo);
}

u64
ktime_get_ns(void)
{
    return (sNow);
}

// windows

struct fake_win *
fake_win_new(const char *name, size_t size)
{
    struct fake_win *w = calloc(1, sizeof(*w));

    assert(sNwins < MAX_WINS);
    w->name = name;
    w->size = size;
    w->mem = aligned_alloc(4096, round_up(size, 4096));
    memset(w->mem, 0, round_up(size, 4096));
    sWins[sNwins++] = w;
    return (w);
}

static struct fake_win *
win_of(const volatile void *addr, u32 *off)
{
    for (int i = 0; i < sNwins; i++) {
        struct fake_win *w = sWins[i];

        if ((const u8 *)addr >= w->mem && (const u8 *)addr < w->mem + w->size) {
            *off = (u32)((const u8 *)addr - w->mem);
            return (w);
        }
    }
    return (NULL);
}

u32
fake_reg(struct fake_win *w, u32 off)
{
    u32 v;

    memcpy(&v, w->mem + off, 4);
    return (v);
}

void
fake_reg_set(struct fake_win *w, u32 off, u32 v)
{
    memcpy(w->mem + off, &v, 4);
}

u32
readl(const volatile void *addr)
{
    u32 off, v;
    struct fake_win *w = win_of(addr, &off);

    if (w != NULL && w->on_read != NULL && w->on_read(w, off, &v))
        return (v);
    return (*(const volatile u32 *)addr);
}

void
writel(u32 v, volatile void *addr)
{
    u32 off;
    struct fake_win *w = win_of(addr, &off);

    *(volatile u32 *)addr = v;
    if (w != NULL && w->on_write != NULL)
        w->on_write(w, off, v);
}

u64
readq(const volatile void *addr)
{
    return (readl(addr) | (u64)readl((const volatile u8 *)addr + 4) << 32);
}

void
writeq(u64 v, volatile void *addr)
{
    writel((u32)v, addr);
    writel((u32)(v >> 32), (volatile u8 *)addr + 4);
}

void
iounmap(volatile void *addr)
{
}

// memory

void *
kzalloc(size_t size, int flags)
{
    fake_live.kmem++;
    return (calloc(1, size));
}

void
kfree(const void *p)
{
    if (p != NULL)
        fake_live.kmem--;
    free((void *)p);
}

void
kvfree(const void *p)
{
    kfree(p);
}

void *
kvmalloc_array(size_t n, size_t size, int flags)
{
    return (kzalloc(n * size, flags));
}

// the host address doubles as the physical one, so tables can be walked by phys
void *
pdos_malloc_contig(size_t size, size_t align, u64 maxaddr, u64 *phys)
{
    size_t a = align < 4096 ? 4096 : align;
    void *p = aligned_alloc(a, round_up(size, a));

    if (p == NULL)
        return (NULL);
    memset(p, 0, size);
    *phys = (u64)(uintptr_t)p;
    if (*phys + size - 1 > maxaddr) {
        free(p);
        return (NULL);
    }
    fake_live.dma++;
    fake_live.dma_bytes += size;
    return (p);
}

void
pdos_free_contig(void *p, size_t size)
{
    if (p == NULL)
        return;
    fake_live.dma--;
    fake_live.dma_bytes -= size;
    free(p);
}

static struct { u64 phys; u8 *buf; size_t size; } sRegions[4];
static int sNregions;

void
fake_phys_region(u64 phys, void *buf, size_t size)
{
    sRegions[sNregions].phys = phys;
    sRegions[sNregions].buf = buf;
    sRegions[sNregions].size = size;
    sNregions++;
}

// a registered range, else the host address that stood in for the physical one
void *
pdos_map_phys(u64 phys, size_t size, int uncached)
{
    for (int i = 0; i < sNregions; i++)
        if (phys >= sRegions[i].phys && phys + size <= sRegions[i].phys + sRegions[i].size)
            return (sRegions[i].buf + (phys - sRegions[i].phys));
    return ((void *)(uintptr_t)phys);
}

void
pdos_unmap_phys(void *p, size_t size)
{
}

int
pdos_bootarg_int(const char *name, int def)
{
    return (def);
}

void
mutex_init(struct mutex *m)
{
    m->held = 0;
}

void
mutex_lock(struct mutex *m)
{
    assert(m->held == 0 && "mutex taken twice");
    m->held = 1;
}

void
mutex_unlock(struct mutex *m)
{
    assert(m->held == 1 && "mutex released while free");
    m->held = 0;
}

// device tree

struct device_node *
fake_node(const char *name, const char *compatible)
{
    struct device_node *np = calloc(1, sizeof(*np));

    np->full_name = name;
    np->compatible = compatible;
    sNodes[sNnodes++] = np;
    return (np);
}

void
fake_phandle(struct device_node *np, const char *list, struct device_node *target, int n, ...)
{
    struct fake_phandle *p = &np->phandles[np->nphandles++];
    va_list ap;

    p->list = list;
    p->np = target;
    p->args_count = n;
    va_start(ap, n);
    for (int i = 0; i < n; i++)
        p->args[i] = va_arg(ap, u32);
    va_end(ap);
}

struct fake_win *
fake_node_win(struct device_node *np, size_t size)
{
    struct fake_win *w = fake_win_new(np->full_name, size);

    np->wins[np->nwins++] = w;
    return (w);
}

struct clk *
fake_node_clock(struct device_node *np, const char *name)
{
    struct clk *c = calloc(1, sizeof(*c));

    c->name = name;
    np->clock_names[np->nclocks] = name;
    np->clocks[np->nclocks++] = c;
    return (c);
}

void
fake_node_child(struct device_node *np, struct device_node *child)
{
    np->children[np->nchildren++] = child;
}

int
of_parse_phandle_with_args(struct device_node *np, const char *list, const char *cells, int index,
    struct of_phandle_args *out)
{
    for (int i = 0, n = 0; i < np->nphandles; i++) {
        struct fake_phandle *p = &np->phandles[i];

        if (strcmp(p->list, list) != 0 || n++ != index)
            continue;
        out->np = p->np;
        out->args_count = p->args_count;
        memcpy(out->args, p->args, sizeof(p->args));
        return (0);
    }
    return (-ENOENT);
}

struct device_node *
of_parse_phandle(struct device_node *np, const char *name, int index)
{
    struct of_phandle_args a;

    return (of_parse_phandle_with_args(np, name, NULL, index, &a) == 0 ? a.np : NULL);
}

struct device_node *
of_get_child_by_name(struct device_node *np, const char *name)
{
    for (int i = 0; i < np->nchildren; i++)
        if (strcmp(np->children[i]->full_name, name) == 0)
            return (np->children[i]);
    return (NULL);
}

int
of_address_to_resource(struct device_node *np, int index, struct resource *r)
{
    if (!np->has_reg || index != 0)
        return (-EINVAL);
    *r = np->reg;
    return (0);
}

void *
of_iomap(struct device_node *np, int index)
{
    return (index < np->nwins ? np->wins[index]->mem : NULL);
}

int
of_property_match_string(struct device_node *np, const char *prop, const char *s)
{
    for (int i = 0; i < np->nclocks; i++)
        if (strcmp(np->clock_names[i], s) == 0)
            return (i);
    return (-ENOENT);
}

bool
of_device_is_compatible(struct device_node *np, const char *compat)
{
    return (np->compatible != NULL && strcmp(np->compatible, compat) == 0);
}

struct clk *
of_clk_get(struct device_node *np, int index)
{
    return (index < np->nclocks ? np->clocks[index] : ERR_PTR(-ENOENT));
}

int
clk_prepare_enable(struct clk *c)
{
    if (c->fail != 0)
        return (c->fail);
    c->enables++;
    return (0);
}

int
clk_set_rate(struct clk *c, unsigned long rate)
{
    if (c->fail != 0)
        return (c->fail);
    c->rate = rate;
    return (0);
}

unsigned long
clk_get_rate(struct clk *c)
{
    return (c->rate);
}

int
pd_power_domains_on(struct device_node *np)
{
    return (np->power_fail);
}

// firmware

void
fake_firmware(const char *name, const void *data, size_t size)
{
    sFw[sNfw].name = name;
    sFw[sNfw].data = data;
    sFw[sNfw].size = size;
    sNfw++;
}

int
request_firmware(const struct firmware **fw, const char *name, struct device *dev)
{
    for (int i = 0; i < sNfw; i++) {
        if (strcmp(sFw[i].name, name) != 0)
            continue;
        struct firmware *f = kzalloc(sizeof(*f), 0);

        f->data = sFw[i].data;
        f->size = sFw[i].size;
        *fw = f;
        return (0);
    }
    return (-ENOENT);
}

void
release_firmware(const struct firmware *fw)
{
    kfree(fw);
}

void
fake_reset(void)
{
    for (int i = 0; i < sNwins; i++) {
        free(sWins[i]->mem);
        free(sWins[i]);
    }
    sNwins = 0;
    for (int i = 0; i < sNnodes; i++) {
        for (int c = 0; c < sNodes[i]->nclocks; c++)
            free(sNodes[i]->clocks[c]);
        free(sNodes[i]);
    }
    sNnodes = 0;
    sNfw = 0;
    sNregions = 0;
    fake_nsmc = 0;
    fake_smc_handler = NULL;
    fake_tick = NULL;
    memset(&fake_live, 0, sizeof(fake_live));
    sNow = 0;
}
