// sc8280xp's adreno smmu-500: the gpu/gmu's context bank
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pd_providers.h>

#include "pd_msm.h"

// global register space 0 (ARM-SMMU)
#define SMMU_SCR0        0x000
#define SMMU_SACR        0x010
#define SMMU_IDR0        0x020
#define SMMU_IDR1        0x024
#define SMMU_IDR2        0x028
#define SMMU_IDR7        0x03c
#define SMMU_SGFSR       0x048
#define SMMU_TLBIALLNSNH 0x068
#define SMMU_TLBIALLH    0x06c
#define SMMU_STLBGSYNC   0x070
#define SMMU_STLBGSTATUS 0x074
#define SMMU_SMR(n)      (0x800 + 4 * (n))
#define SMMU_S2CR(n)     (0xc00 + 4 * (n))
// inside a context bank
#define CB_SCTLR         0x000
#define CB_ACTLR         0x004
#define CB_FSR           0x058
// global register space 1, one page after space 0 (ARM-SMMU)
#define SMMU_CBAR(n)     (0x000 + 4 * (n))
#define SMMU_CBA2R(n)    (0x800 + 4 * (n))
// inside a context bank (CB_SCTLR, CB_ACTLR, CB_FSR as in smmu_reset)
#define CB_TCR2          0x010
#define CB_TTBR0         0x020 // 64-bit
#define CB_TTBR1         0x028 // 64-bit
#define CB_TCR           0x030
#define CB_MAIR0         0x038
#define CB_MAIR1         0x03c
#define SMR_VALID        (1u << 31)
#define SMR(id, mask)    (SMR_VALID | ((u32)(mask) << 16) | (id))
#define S2CR_CB(n)       (n) // type translate (0), context bank n
#define STREAM_MASK      0xc00 // the iommus cells' mask (spec 6.3)
// stage 1 descriptors, 4 KB granule (ARM-SMMU; the same bits as pd_msm_smmu.c)
#define DESC_VALID       0x3ull // table at levels 0-2, page at level 3
#define DESC_AP_RW       (1ull << 6) // AP 0b01: read-write
#define DESC_SH_OUTER    (2ull << 8)
#define DESC_AF          (1ull << 10)
#define DESC_NG          (1ull << 11)
#define DESC_ADDR        0x0000fffffffff000ull
// attribute index 0 (mair0 byte 0, 0x44, normal non-cacheable) is bits 4:2 = 0, and XN is
// left clear: spec 5.2b's gmu mappings are executable
#define DESC_PAGE        (DESC_VALID | DESC_AP_RW | DESC_SH_OUTER | DESC_AF | DESC_NG)
#define DESC_PAGE_PRIV   (DESC_VALID | DESC_SH_OUTER | DESC_AF | DESC_NG)

#define CB_TLBIASID       0x610
#define CB_TLBSYNC        0x7f0
#define CB_TLBSTATUS      0x7f4
// global space 0
#define SMMU_SGFAR        0x040 // 64-bit
#define SMMU_SGFSYNR0     0x050
// SMMU_SGFSR 0x048 as in smmu_reset
// global space 1
#define SMMU_CBFRSYNRA(n) (0x400 + 4 * (n)) // bits 15:0 the faulting stream id
// inside a context bank (CB_FSR 0x058 as in smmu_reset)
#define CB_FAR            0x060 // 64-bit
#define CB_FSYNR0         0x068

struct smmu500 {
    void __iomem *base; // 0x03da0000: global space 0, 1, then the banks
    u32 gpu_sid[2]; // from the gpu's iommus
    u32 gmu_sid; // from the gpu's iommus
    struct mutex lock;
    struct {
        u64 *va;
        u64 phys;
        void *alloc;
    } tabs[2048];
    int ntabs;
    u64 *root[2]; // each bank's level 0 table, tabs[] entries
    void __iomem *cb; // the first context bank, from IDR1
    u32 cb_size; // one bank, and one global space
    u64 pt_phys[2]; // each bank's root table, for ttbr0
};

static int
smmu_reset(struct smmu500 *s)
{
    u32 idr0 = readl(s->base + 0x020);
    u32 idr1 = readl(s->base + 0x024);
    u32 idr2 = readl(s->base + 0x028);
    u32 pagesize = (idr1 & (1u << 31)) ? 0x10000 : 0x1000;
    u32 npages = 1u << (((idr1 >> 28) & 7) + 1);
    int nsmr = idr0 & 0xff, ncb = idr1 & 0xff;

    printf("pd msm: smmu500: idr0 0x%08x idr1 0x%08x idr2 0x%08x, %d smrs, %d banks\n",
        idr0, idr1, idr2, nsmr, ncb);
    s->cb = s->base + npages * pagesize;
    s->cb_size = pagesize;

    writel(0x00200407, s->base + SMMU_SCR0); // bypass
    writel(readl(s->base + SMMU_SGFSR), s->base + SMMU_SGFSR); // clear faults
    for (int n = 0; n < nsmr; n++) { // every SMR invalid, every S2CR bypass
        writel(0x00010000, s->base + SMMU_S2CR(n));
        writel(0, s->base + SMMU_SMR(n));
    }
    for (int n = 0; n < ncb; n++) { // SCTLR_0 and FSR cleared
        writel(0, s->cb + n * pagesize + CB_SCTLR);
        writel(0xc00001fe, s->cb + n * pagesize + CB_FSR);
    }
    // tlbs invalidated
    writel(0xffffffff, s->base + SMMU_TLBIALLH);
    writel(0xffffffff, s->base + SMMU_TLBIALLNSNH);

    (void)readl(s->base + SMMU_IDR7);
    writel(0x51c, s->base + SMMU_SACR); // was 0x1c
    for (int n = 0; n < ncb; n++)
        writel(0x9, s->cb + n * pagesize + CB_ACTLR);
    writel(0, s->base + SMMU_STLBGSYNC);

    for (int i = 0; readl(s->base + SMMU_STLBGSTATUS) != 0; i++) {
        if (i == 1000) {
            printf("pd msm: smmu500: sTLBGSYNC never completed\n");
            return (-ETIMEDOUT);
        }
        udelay(1);
    }
    writel(0x00201836, s->base + SMMU_SCR0);
    u32 scr0 = readl(s->base + SMMU_SCR0);
    printf("pd msm: smmu500: scr0 0x%08x (wrote 0x00201836)%s\n", scr0,
        scr0 == 0x00201836 ? "" : ", differs");
    return (0);
}

static u64 *
table_new(struct smmu500 *s, u64 *phys)
{
    if (s->ntabs == nitems(s->tabs))
        return (NULL);
    s->tabs[s->ntabs].va = pd_msm_dma_alloc(0x1000, &s->tabs[s->ntabs].phys,
        &s->tabs[s->ntabs].alloc);
    if (s->tabs[s->ntabs].va == NULL)
        return (NULL);
    *phys = s->tabs[s->ntabs].phys;
    return (s->tabs[s->ntabs++].va);
}

// the next-level table below parent[index], made on first use
static u64 *
table_at(struct smmu500 *s, u64 *parent, u32 index)
{
    u64 d = parent[index], phys;
    u64 *t;

    if ((d & 1) != 0) {
        for (int i = 0; i < s->ntabs; i++) {
            if (s->tabs[i].phys == (d & DESC_ADDR))
                return (s->tabs[i].va);
        }
        return (NULL);
    }
    if ((t = table_new(s, &phys)) == NULL)
        return (NULL);
    parent[index] = (phys & DESC_ADDR) | DESC_VALID;
    return (t);
}

// the level 3 entry for a 48-bit iova: levels 0-3 from bits 47:39, 38:30, 29:21 and 20:12
static u64 *
leaf(struct smmu500 *s, int bank, u64 iova)
{
    u64 *l1 = table_at(s, s->root[bank], (iova >> 39) & 0x1ff);
    u64 *l2 = l1 != NULL ? table_at(s, l1, (iova >> 30) & 0x1ff) : NULL;
    u64 *l3 = l2 != NULL ? table_at(s, l2, (iova >> 21) & 0x1ff) : NULL;

    return (l3 != NULL ? &l3[(iova >> 12) & 0x1ff] : NULL);
}

// invalidate the bank's asid, then wait for the sync (spec 6.5, as pd_msm_smmu.c's flush)
static void
bank_flush(struct smmu500 *s, int bank)
{
    void __iomem *cb = s->cb + bank * s->cb_size;

    writel(bank, cb + CB_TLBIASID); // asid = bank number (0 gpu, 1 gmu)
    writel(0, cb + CB_TLBSYNC);
    for (int us = 0; us < 10000 && (readl(cb + CB_TLBSTATUS) & 1) != 0; us++)
        udelay(1);
}

// called with s->lock held (smmu_map, pd_msm_smmu500_gmu_map). 4 KB leaves: the kernel's
// pages are 16 KB, the smmu's granule is not
static int
table_map(struct smmu500 *s, int bank, u64 iova, u64 phys, size_t size)
{
    u64 attr = bank == 1 ? DESC_PAGE_PRIV : DESC_PAGE;
    if (((iova | phys | size) & 0xfff) != 0 || (iova >> 48) != 0)
        return (-EINVAL);
    for (size_t off = 0; off < size; off += 0x1000) {
        u64 *e = leaf(s, bank, iova + off);

        if (e == NULL) {
            printf("pd msm: smmu500: bank %d: no table for 0x%llx\n", bank,
                (unsigned long long)(iova + off));
            return (-ENOMEM);
        }
        *e = ((phys + off) & DESC_ADDR) | attr;
    }
    wmb();
    bank_flush(s, bank);
    return (0);
}

static void
table_unmap(struct smmu500 *s, int bank, u64 iova, size_t size)
{
    for (size_t off = 0; off < size; off += 0x1000) {
        u64 *e = leaf(s, bank, iova + off);

        if (e != NULL)
            *e = 0;
    }
    wmb();
    bank_flush(s, bank);
}

static int
bank_table(struct smmu500 *s, int bank)
{
    u64 phys;

    if ((s->root[bank] = table_new(s, &phys)) == NULL)
        return (-ENOMEM);
    s->pt_phys[bank] = phys; // keep this one field for the ttbr0 write
    return (0);
}

static int
smmu_bank_gpu(struct smmu500 *s)
{
    void __iomem *gr1 = s->base + s->cb_size;
    void __iomem *cb = s->cb + 0 * s->cb_size;
    int error;

    if ((error = bank_table(s, 0)) != 0)
        return (error);
    writel(0x00000303, cb + CB_ACTLR);
    writel(0x00000001, gr1 + SMMU_CBA2R(0)); // va64
    writel(0x0001f300, gr1 + SMMU_CBAR(0)); // type 1: stage 1, stage 2 bypass
    writel(0x00038011, cb + CB_TCR2); // pasize 36-bit, 16-bit asid, sep 7
    writel(0x00803510, cb + CB_TCR);
    writeq(s->pt_phys[0] | (0ull << 48), cb + CB_TTBR0); // asid 1
    writeq(0, cb + CB_TTBR1);
    writel(0xf404ff44, cb + CB_MAIR0);
    writel(0xffffffff, cb + CB_MAIR1);
    writel(0x00001167, cb + CB_SCTLR);
    printf("pd msm: smmu500: bank 0 ttbr0 0x%016llx tcr 0x%08x sctlr 0x%08x\n",
        (unsigned long long)readq(cb + CB_TTBR0), readl(cb + CB_TCR), readl(cb + CB_SCTLR));
    return (0);
}

static int
smmu_bank_gmu(struct smmu500 *s)
{
    void __iomem *gr1 = s->base + s->cb_size;
    void __iomem *cb = s->cb + 1 * s->cb_size;
    int error;

    if ((error = bank_table(s, 1)) != 0)
        return (error);
    writel(0x00000303, cb + CB_ACTLR);
    writel(0x00000001, gr1 + SMMU_CBA2R(1)); // va64
    writel(0x0001f300, gr1 + SMMU_CBAR(1)); // type 1: stage 1, stage 2 bypass
    writel(0x00038011, cb + CB_TCR2); // pasize 36-bit, 16-bit asid, sep 7
    writel(0x00803510, cb + CB_TCR);
    writeq(s->pt_phys[1] | (1ull << 48), cb + CB_TTBR0); // asid 0
    writeq(1ull << 48, cb + CB_TTBR1);
    writel(0xf404ff44, cb + CB_MAIR0);
    writel(0xffffffff, cb + CB_MAIR1);
    writel(0x00001167, cb + CB_SCTLR);
    printf("pd msm: smmu500: bank 1 ttbr0 0x%016llx tcr 0x%08x sctlr 0x%08x\n",
        (unsigned long long)readq(cb + CB_TTBR0), readl(cb + CB_TCR), readl(cb + CB_SCTLR));
    return (0);
}
static int
smmu_streams(struct smmu500 *s)
{
    const u32 sid[3] = { s->gpu_sid[0], s->gpu_sid[1], s->gmu_sid };
    const u32 bank[3] = { 0, 0, 1 };

    for (int i = 0; i < 3; i++)
        writel(SMR(sid[i], STREAM_MASK), s->base + SMMU_SMR(i));
    for (int i = 0; i < 3; i++)
        writel(S2CR_CB(bank[i]), s->base + SMMU_S2CR(i));
    for (int i = 0; i < 3; i++)
        printf("pd msm: smmu500: smr%d 0x%08x s2cr%d 0x%08x\n", i,
            readl(s->base + SMMU_SMR(i)), i, readl(s->base + SMMU_S2CR(i)));
    return (0);
}

// both banks and the global fault state; call it whenever a later step times out. Faults are
// write-1-to-clear, so each is cleared after it's printed and the next one is recorded fresh
static void
smmu_dump_fault(void *priv)
{
    struct smmu500 *s = priv;
    void __iomem *gr1 = s->base + s->cb_size;
    u32 gfsr = readl(s->base + SMMU_SGFSR);

    printf("pd msm: smmu500: sgfsr 0x%08x sgfsynr0 0x%08x sgfar 0x%016llx\n", gfsr,
        readl(s->base + SMMU_SGFSYNR0), (unsigned long long)readq(s->base + SMMU_SGFAR));
    if (gfsr != 0)
        writel(gfsr, s->base + SMMU_SGFSR);
    for (int bank = 0; bank < 2; bank++) {
        void __iomem *cb = s->cb + bank * s->cb_size;
        u32 fsr = readl(cb + CB_FSR);

        printf("pd msm: smmu500: bank %d (%s) fsr 0x%08x far 0x%016llx fsynr0 0x%08x "
               "stream 0x%x\n",
            bank, bank == 0 ? "gpu" : "gmu", fsr,
            (unsigned long long)readq(cb + CB_FAR), readl(cb + CB_FSYNR0),
            readl(gr1 + SMMU_CBFRSYNRA(bank)) & 0xffff);
        if (fsr != 0)
            writel(fsr, cb + CB_FSR);
    }
}

static void *
smmu_attach(struct device *dev)
{
    struct smmu500 *s;
    struct of_phandle_args args;
    struct device_node *np, *gmu;

    if (of_parse_phandle_with_args(dev->of_node, "iommus", "#iommu-cells", 0, &args) != 0)
        return (NULL);
    np = args.np;
    s = kzalloc(sizeof(*s), GFP_KERNEL);
    mutex_init(&s->lock);
    s->gpu_sid[0] = args.args[0];
    if (of_parse_phandle_with_args(dev->of_node, "iommus", "#iommu-cells", 1, &args) == 0)
        s->gpu_sid[1] = args.args[0];
    gmu = of_parse_phandle(dev->of_node, "qcom,gmu", 0);
    if (gmu == NULL || of_parse_phandle_with_args(gmu, "iommus", "#iommu-cells", 0, &args) != 0) {
        dev_err(dev, "smmu500: no gmu stream\n");
        goto fail;
    }
    s->gmu_sid = args.args[0];
    if (pd_power_domains_on(np) != 0) {
        dev_err(dev, "smmu500: power domain did not come on\n");
        goto fail;
    }
    for (int i = 0;; i++) {
        struct clk *c = of_clk_get(np, i);
        if (IS_ERR(c))
            break;
        if (clk_prepare_enable(c) != 0)
            dev_err(dev, "smmu500: clock %d did not start\n", i);
    }
    s->base = of_iomap(np, 0);
    if (s->base == NULL) {
        dev_err(dev, "smmu500: cannot map %s\n", np->full_name);
        goto fail;
    }
    dev_info(dev, "smmu500: %s, gpu streams %u %u, gmu stream %u\n", np->full_name,
        s->gpu_sid[0], s->gpu_sid[1], s->gmu_sid);
    if (smmu_reset(s) != 0 || smmu_bank_gpu(s) != 0 || smmu_bank_gmu(s) != 0 || smmu_streams(s) != 0) {
        dev_err(dev, "smmu500: setup failed\n");
        goto fail;
    }
    return (s);
fail:
    kfree(s);
    return (NULL);
}

static int
smmu_map(void *priv, u64 iova, u64 phys, size_t size)
{
    struct smmu500 *s = priv;
    int error;

    mutex_lock(&s->lock);
    error = table_map(s, 0, iova, phys, size);
    mutex_unlock(&s->lock);
    return (error);
}

static void
smmu_unmap(void *priv, u64 iova, size_t size)
{
    struct smmu500 *s = priv;

    mutex_lock(&s->lock);
    table_unmap(s, 0, iova, size);
    mutex_unlock(&s->lock);
}

int pd_msm_smmu500_gmu_map(void *priv, u32 gmu_va, u64 phys, size_t size)
{
    struct smmu500 *s = priv;
    int error;

    mutex_lock(&s->lock);
    error = table_map(s, 1, gmu_va, phys, size);
    mutex_unlock(&s->lock);
    return (error);
}

const struct pd_msm_iommu pd_msm_iommu_smmu500 = {
    .compatible = "qcom,adreno-smmu",
    .attach = smmu_attach,
    .map = smmu_map,
    .unmap = smmu_unmap,
    .dump_fault = smmu_dump_fault,
};
