// fake.h - the fake soc the PDDRMmsm host tests run the driver against
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
#ifndef PD_MSM_FAKE_H
#define PD_MSM_FAKE_H

#include "pd_msm_host.h"

// a register window: plain memory, with optional hooks that act as the hardware
struct fake_win {
    const char *name;
    u8 *mem;
    size_t size;
    void *ctx;
    // a read the hook answers itself returns true and sets *v
    bool (*on_read)(struct fake_win *w, u32 off, u32 *v);
    void (*on_write)(struct fake_win *w, u32 off, u32 v);
};

struct fake_win *fake_win_new(const char *name, size_t size);
u32 fake_reg(struct fake_win *w, u32 off);
void fake_reg_set(struct fake_win *w, u32 off, u32 v);

// device tree nodes: phandle lists, register windows, clocks, a reserved range
#define FAKE_MAX 8
struct fake_phandle {
    const char *list;
    struct device_node *np;
    int args_count;
    u32 args[4];
};
struct device_node {
    const char *full_name;
    const char *compatible;
    struct fake_phandle phandles[FAKE_MAX];
    int nphandles;
    struct fake_win *wins[FAKE_MAX];
    int nwins;
    const char *clock_names[FAKE_MAX];
    struct clk *clocks[FAKE_MAX];
    int nclocks;
    struct device_node *children[FAKE_MAX];
    int nchildren;
    struct resource reg;
    bool has_reg;
    int power_fail;
};

struct device_node *fake_node(const char *name, const char *compatible);
void fake_phandle(struct device_node *np, const char *list, struct device_node *target, int n, ...);
struct fake_win *fake_node_win(struct device_node *np, size_t size);
struct clk *fake_node_clock(struct device_node *np, const char *name);
void fake_node_child(struct device_node *np, struct device_node *child);

// firmware by name, an empty table between tests
void fake_firmware(const char *name, const void *data, size_t size);

// secure monitor calls: every call recorded, the answer from the test's handler
struct fake_smc {
    u64 in[7];
};
extern struct fake_smc fake_smcs[64];
extern int fake_nsmc;
extern void (*fake_smc_handler)(const u64 in[7], u64 out[4], u64 *out6);

// called from every udelay: the fake hardware's step
extern void (*fake_tick)(void);

// live allocations, for leak checks
struct fake_counts {
    long kmem;          // kzalloc and kvmalloc blocks
    long dma;           // pdos_malloc_contig blocks
    long dma_bytes;
};
extern struct fake_counts fake_live;

// a physical range pdos_map_phys maps to the test's buffer, like a reserved-memory carve-out
void fake_phys_region(u64 phys, void *buf, size_t size);

void fake_reset(void);  // between tests: windows, nodes, firmware, counters and hooks
u64 fake_now_ns(void);

#endif
