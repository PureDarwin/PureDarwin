// the a6xx generation's private state, shared by pd_msm_a6xx.c and pd_msm_a6xx_gmu.c
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
#ifndef PD_MSM_A6XX_H
#define PD_MSM_A6XX_H

#include <linux/io.h>
#include <linux/firmware.h>
#include <linux/clk.h>

#include "pd_msm.h"

#define GMU_WINDOW_WORD    0x1a800        // the gmu window's first word in the gpu word space (spec 3)

// one piece of memory the gmu reaches through its smmu bank
struct a6xx_gmu_mem {
    u32         gmu_va;
    size_t      size;       // multiple of 4 KB
    u64         phys;
    void        *cpu;       // uncached
    void        *alloc;
};

struct a6xx_state {
    struct device_node      *gmu_np;
    void __iomem            *gmu;           // gmu window 0: 0x03d6a000, gpu words 0x1a800 up
    void __iomem            *rscc;          // gmu window 1: 0x03de0000
    void __iomem            *pdc;           // gmu window 2: 0x0b290000
    struct clk              *gmu_clk;       // gpu_cc_cx_gmu_clk: its rate is its root's, 200 MHz
    struct clk              *hub_clk;       // gpu_cc_hub_cx_int_clk, the same for the hub root
    struct clk              *axi_clk;       // gcc_ddrss_gpu_axi_clk, on after the rates
    struct a6xx_gmu_mem     mem[16];
    int                     nmem;
    struct a6xx_gmu_mem     *hfi;           // the hfi region at gmu 0x60008000
    u32                     hfi_seq;
    bool                    rscc_done;      // once per boot (spec 9.1)
    bool                    zap_done;       // once per boot (spec 7.2)
    struct pd_msm_bo        *sqe;           // the sqe image from file byte 4 (spec 4.2)
    u64                     zap_phys;       // the gpu's zap-shader memory-region (gpu-mem@8bf00000)
    u32                     zap_size;
};

static inline u32
gpu_read(struct pd_msm_dev *mdev, u32 word)
{
    return (readl((u8 __iomem *)mdev->gpu.mmio + word * 4));
}

static inline void
gpu_write(struct pd_msm_dev *mdev, u32 word, u32 v)
{
    writel(v, (u8 __iomem *)mdev->gpu.mmio + word * 4);
}

static inline u32
gmu_read(struct a6xx_state *st, u32 word)
{
    return (readl((u8 __iomem *)st->gmu + (word - GMU_WINDOW_WORD) * 4));
}

static inline void
gmu_write(struct a6xx_state *st, u32 word, u32 v)
{
    writel(v, (u8 __iomem *)st->gmu + (word - GMU_WINDOW_WORD) * 4);
}

// pd_msm_a6xx.c, plumbing: one arm smccc call, x0-x6 in, x0-x3 and x6 out
void a6xx_smc(const u64 in[7], u64 out[4], u64 *out6);

// pd_msm_a6xx_gmu.c, plumbing
int a6xx_gmu_find(struct pd_msm_dev *mdev, struct a6xx_state *st);
int a6xx_gmu_power(struct pd_msm_dev *mdev, struct a6xx_state *st);
struct a6xx_gmu_mem *a6xx_gmu_alloc(struct pd_msm_dev *mdev, struct a6xx_state *st,
                                    u32 gmu_va, size_t size);

// pd_msm_a6xx_gmu.c
int a6xx_gmu_load(struct pd_msm_dev *mdev, struct a6xx_state *st, const struct firmware *fw);
int a6xx_gmu_start(struct pd_msm_dev *mdev, struct a6xx_state *st);
int a6xx_gmu_oob_gpu(struct a6xx_state *st, bool request);

#endif
