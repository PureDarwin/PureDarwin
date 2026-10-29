// sc8280xp's gcc gpu clocks, the gpu clock controller and its cx gdsc (spec 9)
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pd_providers.h>

// the ids board.dtb uses, matched to clocks by the consumers clock-names
enum { GCC_DDRSS_GPU_AXI = 0x25, GCC_GPU_MEMNOC_GFX = 0x4d, GCC_GPU_SNOC_DVM_GFX = 0x4e };
enum { GPUCC_AHB = 0x02, GPUCC_CX_GMU = 0x05, GPUCC_HUB_AON = 0x0e, GPUCC_HUB_CX_INT = 0x10,
    GPUCC_HLOS1_VOTE_SMMU = 0x13 };
// two roots no node names, reached through CX_GMU and HUB_CX_INT which no dtb uses
enum { GPUCC_GMU_CLK_SRC = 0x100, GPUCC_HUB_CLK_SRC = 0x101 };
enum { GPUCC_CX_GDSC = 0, GPUCC_GX_GDSC = 1 };

enum { K_BRANCH, K_RCG };

struct cc_clk {
    u32                 id;
    void __iomem        **base;     // &sGcc or &sGpucc
    int                 kind;
    u32                 reg;
    u32                 root;
    const char          *name;
};

static void __iomem *sGcc, *sGpucc;

static const struct cc_clk sGccClocks[] = {
    { GCC_GPU_MEMNOC_GFX,   &sGcc, K_BRANCH, 0x71010, 0, "gcc_gpu_memnoc_gfx_clk" },
    { GCC_GPU_SNOC_DVM_GFX, &sGcc, K_BRANCH, 0x71020, 0, "gcc_gpu_snoc_dvm_gfx_clk" },
    { GCC_DDRSS_GPU_AXI,    &sGcc, K_BRANCH, 0x7115c, 0, "gcc_ddrss_gpu_axi_clk" },
};
static const struct cc_clk sGpuccClocks[] = {
    { GPUCC_AHB,             &sGpucc, K_BRANCH, 0x1078, 0, "gpu_cc_ahb_clk" },
    { GPUCC_CX_GMU,          &sGpucc, K_BRANCH, 0x1098, GPUCC_GMU_CLK_SRC, "gpu_cc_cx_gmu_clk" },
    { GPUCC_HUB_AON,         &sGpucc, K_BRANCH, 0x1178, 0, "gpu_cc_hub_aon_clk" },
    { GPUCC_HUB_CX_INT,      &sGpucc, K_BRANCH, 0x1204, GPUCC_HUB_CLK_SRC, "gpu_cc_hub_cx_int_clk" },
    { GPUCC_HLOS1_VOTE_SMMU, &sGpucc, K_BRANCH, 0x5000, 0, "gpu_cc_hlos1_vote_gpu_smmu_clk" },
    // the roots: no dtb names them, so xlate never returns them; cc_set_rate finds them by id
    { GPUCC_GMU_CLK_SRC,     &sGpucc, K_RCG,    0x1120, 0, "gpu_cc_gmu_clk_src" },  // cfg 0x1124
    { GPUCC_HUB_CLK_SRC,     &sGpucc, K_RCG,    0x117c, 0, "gpu_cc_hub_clk_src" },  // cfg 0x1180
};

#define GCC_GPLL0_VOTE              0x52000
#define GPLL0_VOTE_MAIN             (1u << 15)      // gcc_gpu_gpll0_clk_src, 600 MHz: the hub root's source 5
#define GPLL0_VOTE_DIV              (1u << 16)      // gcc_gpu_gpll0_div_clk_src, 300 MHz: the gmu root's source 6
#define HUB_CFG_200MHZ              0x505           // source 5, divider field 5 (/3)
#define GMU_CFG_200MHZ              0x602           // source 6, divider field 2 (/1.5)
#define CBCR_EN                     (1u << 0)       // write 1: enable the branch
#define CBCR_OFF                    (1u << 31)      // reads 1 while the clock is off
#define RCG_UPDATE    (1u << 0)

static void *
xlate(void __iomem **base, const struct cc_clk *tab, int n, const struct of_phandle_args *spec)
{
    if (spec->args_count < 1)
        return (NULL);
    if (*base == NULL)
        *base = of_iomap(spec->np, 0);
    if (*base == NULL)
        return (NULL);
    for (int i = 0; i < n; i++) {
        if (tab[i].id == spec->args[0])
            return ((void *)&tab[i]);
    }
    printf("pd gpucc8280: %s clock %u is not described\n", spec->np->full_name, spec->args[0]);
    return (NULL);
}

static void *
gcc_xlate(void *priv, const struct of_phandle_args *spec)
{
    return (xlate(&sGcc, sGccClocks, nitems(sGccClocks), spec));
}

static void *
gpucc_xlate(void *priv, const struct of_phandle_args *spec)
{
    return (xlate(&sGpucc, sGpuccClocks, nitems(sGpuccClocks), spec));
}

// wait until (reg & mask) == want, up to us microseconds
static bool
poll(void __iomem *base, u32 reg, u32 mask, u32 want, int us)
{
    for (int i = 0; i < us; i += 10) {
        if ((readl(base + reg) & mask) == want)
            return (true);
        udelay(10);
    }
    printf("pd gpucc8280: 0x%05x stuck at 0x%08x\n", reg, readl(base + reg));
    return (false);
}

static int cc_enable(void *hw) {
    const struct cc_clk *c = hw;
    void __iomem *base = *c->base;
    switch (c->kind) {
        case K_RCG: return (0);
        case K_BRANCH: {
            u32 v = readl(base + c->reg);
            
            if ((v & CBCR_OFF) == 0) // already running
                return (0);
            writel(v | CBCR_EN, base + c->reg); // set bit 0
            return ((poll(base, c->reg, CBCR_OFF, 0, 2000)) ? 0 : -ETIMEDOUT);
        }
        default: return (-EINVAL);
    }
}

static void cc_disable(void *hw) { }

static const struct cc_clk *
find(const struct cc_clk *tab, int n, u32 id)
{
      for (int i = 0; i < n; i++) {
              if (tab[i].id == id)
                      return (&tab[i]);
      }
      return (NULL);
}

static int cc_set_rate(void *hw, unsigned long rate) {
    const struct cc_clk *c = hw, *r;
    void __iomem *base;
    u32 cfg, vote, cmd;
    
    if (c->root == 0 || rate != 200000000)
        return (-EINVAL);
    
    r = find(sGpuccClocks, nitems(sGpuccClocks), c->root);
    if (r == NULL || sGcc == NULL)
        return (-ENODEV);
    base = *r->base;
    if (r->id == GPUCC_HUB_CLK_SRC) {
        cfg = HUB_CFG_200MHZ;
        vote = GPLL0_VOTE_MAIN;
    } else {
        cfg = GMU_CFG_200MHZ;
        vote = GPLL0_VOTE_DIV;
    }
    
    writel(readl(sGcc + GCC_GPLL0_VOTE) | vote, sGcc + GCC_GPLL0_VOTE);
    
    writel(2, base + r->reg);
    writel(cfg, base + r->reg + 4);
    cmd = readl(base + r->reg);
    writel(0x13, base + r->reg);
    if (!poll(base, r->reg, RCG_UPDATE, 0, 2000))
        return (-ETIMEDOUT);
    writel(0, base + r->reg);
    
    printf("pd gpucc8280: %s cfg 0x%03x, cmd read 0x%02x, vote 0x%08x\n", r->name,
           readl(base + r->reg + 4), cmd, readl(sGcc + GCC_GPLL0_VOTE));
    
    return (0);
}

static unsigned long
cc_get_rate(void *hw)
{
    const struct cc_clk *c = hw, *r;
    u32 cfg;

    if (c->root == 0)
        return (0);                     // only the gmu and hub branches have a rate
    r = find(sGpuccClocks, nitems(sGpuccClocks), c->root);
    if (r == NULL || *r->base == NULL)
        return (0);
    cfg = readl(*r->base + r->reg + 4);
    if (cfg == HUB_CFG_200MHZ || cfg == GMU_CFG_200MHZ)
        return (200000000);
    if (cfg == 0)
        return (19200000);              // spec 9.3: every root ran from the crystal at handoff
    printf("pd gpucc8280: %s cfg 0x%03x\n", r->name, cfg);
    return (0);
}

static long
cc_round_rate(void *hw, unsigned long rate)
{
    const struct cc_clk *c = hw;

    return (c->root != 0 ? 200000000 : 0);
}

static const struct pd_clk_ops sGccOps = {
    .xlate = gcc_xlate, .enable = cc_enable, .disable = cc_disable,
    .set_rate = cc_set_rate, .get_rate = cc_get_rate, .round_rate = cc_round_rate,
};
static const struct pd_clk_ops sGpuccOps = {
    .xlate = gpucc_xlate, .enable = cc_enable, .disable = cc_disable,
    .set_rate = cc_set_rate, .get_rate = cc_get_rate, .round_rate = cc_round_rate,
};

static void *
gdsc_xlate(void *priv, const struct of_phandle_args *spec)
{
    // cx only. the gmu node lists gx too and refusing it here would keep pd_power_domains_on off it
    // gx is the gmu's to switch
    if (spec->args_count < 1 || spec->args[0] != GPUCC_CX_GDSC)
        return (NULL);
    if (sGpucc == NULL)
        sGpucc = of_iomap(spec->np, 0);
    return (sGpucc != NULL ? (void *)&sGpucc : NULL);
}

#define GDSC_COLLAPSE (1u << 0)       // gdscr bit 0: set = powered down
#define GDSC_PWR_ON   (1u << 31)      // gdscr and its status register: reads 1 once on
#define CX_GDSCR      0x106c          // spec 9.2
#define CX_GDSC_STATUS        0x1540

static int
gdsc_on(void *hw)
{
    void __iomem *base = *(void __iomem **)hw;
    u32 v = readl(base + CX_GDSCR);
    
    if ((v & GDSC_PWR_ON) != 0 && (v & GDSC_COLLAPSE) == 0) {
        printf("pd gpucc8280: cx gdsc already on 0x%08x\n", readl(base + CX_GDSCR));
        return (0); // already on
    }
    
    writel(v & ~GDSC_COLLAPSE, base + CX_GDSCR);
    if (!poll(base, CX_GDSC_STATUS, GDSC_PWR_ON, GDSC_PWR_ON, 10000))
        return (-ETIMEDOUT);
    
    printf("pd gpucc8280: cx gdsc 0x%08x, status 0x%08x\n", readl(base + CX_GDSCR),
           readl(base + CX_GDSC_STATUS));
    
    return (0);
}
static int gdsc_off(void *hw) { return (0); }

static const struct pd_power_ops sPowerOps = {
    .xlate = gdsc_xlate,
    .power_on = gdsc_on,
    .power_off = gdsc_off,
};

void
pd_msm_gpucc8280_register(void)
{
    pd_clk_provider_add("qcom,gcc-sc8280xp", &sGccOps, NULL);
    pd_clk_provider_add("qcom,sc8280xp-gpucc", &sGpuccOps, NULL);
    pd_power_provider_add("qcom,sc8280xp-gpucc", &sPowerOps, NULL);
}
