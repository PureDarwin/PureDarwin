// the a690's gmu: its node, its power, its memory, and (Part H) its image, boot, hfi and oob
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pd_providers.h>

#include "pd_msm_a6xx.h"

#define GMU_CM3_ITCM_START    0x1b400         // 0x03d6d000: shows gmu address 0x0 at its first byte
#define GMU_CM3_DTCM_START    0x1c400         // 0x03d71000: shows gmu address 0x10004000
#define ITCM_END              0x4000
#define DTCM_BASE             0x10004000
#define DTCM_END              0x10008000
#define GMU_CORE_FW_VERSION           0x1d3f8         // in the dtcm window: gmu 0x10007fe0
#define GMU_ICACHE_CONFIG             0x1f400
#define GMU_DCACHE_CONFIG             0x1f401
#define GMU_SYS_BUS_CONFIG            0x1f40f
#define GMU_CM3_SYSRESET              0x1f800
#define GMU_CM3_BOOT_CONFIG           0x1f801
#define GMU_CM3_FW_INIT_RESULT                0x1f81c
#define GMU_CM3_CFG                   0x1f82d
#define GMU_PWR_COL_INTER_FRAME_CTRL  0x1f8c0
#define GMU_RPMH_CTRL                 0x1f8e8
#define GMU_CX_FAL_INTF                       0x1f8f0
#define GMU_CX_FALNEXT_INTF           0x1f8f1
#define GMU_PWR_COL_CP_MSG            0x1f900
#define GMU_PWR_COL_CP_RESP           0x1f901
#define GMU_HFI_CTRL_STATUS           0x1f980
#define GMU_HFI_SFR_ADDR              0x1f982
#define GMU_HFI_QTBL_INFO             0x1f984
#define GMU_HFI_QTBL_ADDR             0x1f985
#define GMU_HFI_CTRL_INIT             0x1f986
#define GMU_GMU2HOST_INTR_CLR         0x1f991
#define GMU_GMU2HOST_INTR_INFO                0x1f992
#define GMU_GMU2HOST_INTR_MASK                0x1f993
#define GMU_HOST2GMU_INTR_SET         0x1f994
#define GMU_GENERAL_7                 0x1f9cc
#define GMU_AO_HOST_INTERRUPT_CLR     0x23b04
#define GMU_AO_HOST_INTERRUPT_MASK    0x23b06
#define GMU_AHB_FENCE_RANGE_0         0x23b11
#define GBIF_QSB_SIDE0                        0x3c03          // a gpu register: gpu_write, 0x3c03..0x3c06

// a clock of np by its clock-names entry
static struct clk *
clk_by_name(struct device_node *np, const char *name)
{
    int i = of_property_match_string(np, "clock-names", name);
    
    return (i < 0 ? ERR_PTR(-ENOENT) : of_clk_get(np, i));
}

// the gmu node through the gpu's qcom,gmu
int
a6xx_gmu_find(struct pd_msm_dev *mdev, struct a6xx_state *st)
{
    struct device *dev = &mdev->pdev->dev;
    
    st->gmu_np = of_parse_phandle(dev->of_node, "qcom,gmu", 0);
    if (st->gmu_np == NULL) {
        dev_err(dev, "a6xx: no qcom,gmu\n");
        return (-ENODEV);
    }
    st->gmu = of_iomap(st->gmu_np, 0);
    st->rscc = of_iomap(st->gmu_np, 1);
    st->pdc = of_iomap(st->gmu_np, 2);
    if (st->gmu == NULL || st->rscc == NULL || st->pdc == NULL) {
        dev_err(dev, "a6xx: cannot map gmu windows\n");
        return (-ENOMEM);
    }
    st->gmu_clk = clk_by_name(st->gmu_np, "gmu");
    st->hub_clk = clk_by_name(st->gmu_np, "hub");
    st->axi_clk = clk_by_name(st->gmu_np, "axi");
    if (IS_ERR(st->gmu_clk) || IS_ERR(st->hub_clk) || IS_ERR(st->axi_clk)) {
        dev_err(dev, "a6xx: gmu clocks missing\n");
        return (-ENODEV);
    }
    return (0);
}

static const char *const sGmuClocks[] = { "memnoc", "ahb", "smmu_vote", "gmu", "hub" };

int
a6xx_gmu_power(struct pd_msm_dev *mdev, struct a6xx_state *st)
{
    struct device *dev = &mdev->pdev->dev;
    int error;
    
    // cx only: gdsc_xlate refuses gx, the second entry
    if ((error = pd_power_domains_on(st->gmu_np)) != 0) {
        dev_err(dev, "a6xx: cx did not come on: %d\n", error);
        return (error);
    }
    for (int i = 0; i < nitems(sGmuClocks); i++) {
        struct clk *c = clk_by_name(st->gmu_np, sGmuClocks[i]);
        
        error = IS_ERR(c) ? PTR_ERR(c) : clk_prepare_enable(c);
        if (error != 0) {
            dev_err(dev, "a6xx: gmu clock %s did not start: %d\n", sGmuClocks[i], error);
            return (error);
        }
    }
    if ((error = clk_set_rate(st->gmu_clk, 200000000)) != 0 ||
        (error = clk_set_rate(st->hub_clk, 200000000)) != 0) {
        dev_err(dev, "a6xx: gmu clock rates: %d\n", error);
        return (error);
    }
    if ((error = clk_prepare_enable(st->axi_clk)) != 0) {
        dev_err(dev, "a6xx: axi clock did not start: %d\n", error);
        return (error);
    }
    dev_info(dev, "a6xx: gmu powered, gmu %lu Hz, hub %lu Hz\n", clk_get_rate(st->gmu_clk),
             clk_get_rate(st->hub_clk));
    return (0);
}

// zeroed memory the gmu sees at gmu_va
struct a6xx_gmu_mem *
a6xx_gmu_alloc(struct pd_msm_dev *mdev, struct a6xx_state *st, u32 gmu_va, size_t size)
{
    struct a6xx_gmu_mem *m;
    
    if (st->nmem == nitems(st->mem))
        return (NULL);
    m = &st->mem[st->nmem];
    m->gmu_va = gmu_va;
    m->size = round_up(size, 0x1000);
    m->cpu = pd_msm_dma_alloc(m->size, &m->phys, &m->alloc);
    if (m->cpu == NULL) {
        printf("pd msm: a6xx: no memory for gmu 0x%08x +0x%zx\n", gmu_va, m->size);
        return (NULL);
    }
    if (pd_msm_smmu500_gmu_map(mdev->gpu.iommu_priv, gmu_va, m->phys, m->size) != 0) {
        pd_msm_dma_free(m->cpu, m->alloc, m->size);
        return (NULL);
    }
    st->nmem++;
    return (m);
}

static const struct { u32 va, size; } sGmuRanges[] = {
      { 0x00004000, 0x441000 }, { 0x00445000, 0x1000 }, { 0x00446000, 0x8000 },
      { 0x0044e000, 0xbb2000 }, { 0x60000000, 0x4000 }, { 0x60004000, 0x8000 },
      { 0x60400000, 0x7000 },
};

// one range, whole if it comes in one piece, else in 2 MB pieces at consecutive gmu addresses
static int
gmu_map_range(struct pd_msm_dev *mdev, struct a6xx_state *st, u32 va, u32 size)
{
      if (a6xx_gmu_alloc(mdev, st, va, size) != NULL)
              return (0);
      for (u32 off = 0; off < size; off += 0x200000) {
              if (a6xx_gmu_alloc(mdev, st, va + off, min_t(u32, size - off, 0x200000)) == NULL)
                      return (-ENOMEM);
      }
      printf("pd msm: a6xx: gmu 0x%08x +0x%x in 2 MB pieces\n", va, size);
      return (0);
}

// the cpu address behind gmu address va, or NULL if nothing mapped covers [va, va + len)
static void *
gmu_cpu(struct a6xx_state *st, u32 va, u32 len)
{
    for (int i = 0; i < st->nmem; i++) {
        struct a6xx_gmu_mem *m = &st->mem[i];
        
        if (va >= m->gmu_va && va + len <= m->gmu_va + m->size)
            return ((u8 *)m->cpu + (va - m->gmu_va));
    }
    return (NULL);
}

static struct a6xx_gmu_mem *
gmu_mem_at(struct a6xx_state *st, u32 va)
{
    for (int i = 0; i < st->nmem; i++)
        if (va >= st->mem[i].gmu_va && va < st->mem[i].gmu_va + st->mem[i].size)
            return (&st->mem[i]);
    return (NULL);
}

static void
gmu_tcm_write(struct a6xx_state *st, u32 window, u32 off, const u8 *src, u32 len)
{
    for (u32 i = 0; i < len; i += 4) {
        u32 w = 0;
        
        memcpy(&w, src + i, min_t(u32, 4, len - i));
        gmu_write(st, window + (off + i) / 4, w);
    }
}

int
a6xx_gmu_load(struct pd_msm_dev *mdev, struct a6xx_state *st, const struct firmware *fw)
{
    const u8 *p = fw->data;
    size_t off = 0;
    int error, nblocks = 0;
    
    if (st->nmem == 0) {
        for (int i = 0; i < nitems(sGmuRanges); i++) {
            if ((error = gmu_map_range(mdev, st, sGmuRanges[i].va, sGmuRanges[i].size)) != 0)
                return (error);
        }
        st->hfi = gmu_mem_at(st, 0x60008000);
    }
    while (off + 16 <= fw->size) {
        u32 h[4];
        
        memcpy(h, p + off, sizeof(h));
        u32 va = le32_to_cpu(h[0]), size = le32_to_cpu(h[1]);
        const u8 *payload = p + off + 16;
        
        if (off + 16 + size > fw->size) {
            printf("pd msm: a6xx: gmu image: block at 0x%zx runs past the end\n", off);
            return (-EINVAL);
        }
        if (size == 0) {
            // a descriptor: nothing to copy, its range is one of sGmuRanges (spec 5.2a)
        } else if (va + size <= ITCM_END) {
            gmu_tcm_write(st, GMU_CM3_ITCM_START, va, payload, size);
            printf("pd msm: a6xx: gmu block 0x%08x +0x%x -> itcm\n", va, size);
            nblocks++;
        } else if (va >= DTCM_BASE && va + size <= DTCM_END) {
            gmu_tcm_write(st, GMU_CM3_DTCM_START, va - DTCM_BASE, payload, size);
            printf("pd msm: a6xx: gmu block 0x%08x +0x%x -> dtcm\n", va, size);
            nblocks++;
        } else {
            void *dst = gmu_cpu(st, va, size);
            
            if (dst == NULL) {
                printf("pd msm: a6xx: gmu block 0x%08x +0x%x is not mapped\n", va, size);
                return (-EINVAL);
            }
            memcpy(dst, payload, size);
            printf("pd msm: a6xx: gmu block 0x%08x +0x%x -> memory\n", va, size);
            nblocks++;
        }
        off += 16 + round_up(size, 4);
    }
    wmb();
    printf("pd msm: a6xx: gmu image: %d blocks placed, hfi at gmu 0x%08x\n", nblocks,
            st->hfi != NULL ? 0x60008000 : 0);
    return (st->hfi != NULL ? 0 : -ENOMEM);
}

static const struct { u16 off; u32 v; } sRscc[] = {
    { 0x010, 0x01000000 }, { 0x02c, 1 }, { 0x038, 0 }, { 0x034, 0 }, { 0x040, 0 },
    { 0x03c, 0 }, { 0x048, 0x80000000 }, { 0x044, 0 }, { 0x400, 0 },
    { 0x020, 0x4520 }, { 0x024, 0x4510 }, { 0x028, 0x4514 },
    { 0x600, 0xeaaae5a0 }, { 0x604, 0xe1a1ebab }, { 0x608, 0xa2e0a581 },
    { 0x60c, 0xecac82e2 }, { 0x610, 0x0020edad },
};

static void
gmu_rscc_init(struct a6xx_state *st)
{
    for (int i = 0; i < nitems(sRscc); i++)
        writel(sRscc[i].v, (u8 __iomem *)st->rscc + sRscc[i].off);
    writel(0, (u8 __iomem *)st->pdc + 0x4520);
    writel(0x80000001, (u8 __iomem *)st->pdc + 0x4500);
    wmb();
    st->rscc_done = true;
    printf("pd msm: a6xx: rscc and pdc programmed\n");
}

#define HFI_TABLE     0x60008000
#define HFI_Q0                0x60009000      // host to gmu
#define HFI_Q1                0x6000a000      // gmu to host
#define HFI_QWORDS    1024
#define QH_READ               10
#define QH_WRITE      11

static u32 *
hfi_at(struct a6xx_state *st, u32 va)
{
    return ((u32 *)((u8 *)st->hfi->cpu + (va - st->hfi->gmu_va)));
}

static u32 *
hfi_qhdr(struct a6xx_state *st, int q)
{
    return (hfi_at(st, HFI_TABLE) + 6 + 12 * q);
}

// spec 5.3: written fresh before every boot, so both queues start empty
static void
hfi_table_init(struct a6xx_state *st)
{
    u32 *t = hfi_at(st, HFI_TABLE);
    
    memset(t, 0, (6 + 2 * 12) * 4);
    t[0] = 0; t[1] = 120; t[2] = 6; t[3] = 12; t[4] = 2; t[5] = 2;
    
    for (int q = 0; q < 2; q++) {
        u32 *h = hfi_qhdr(st, q);
        
        h[0] = 1;                           // status
        h[1] = q == 0 ? HFI_Q0 : HFI_Q1;
        h[2] = 0xa00 + q;                   // type
        h[3] = HFI_QWORDS;
        h[6] = h[7] = h[8] = 1;             // rx_watermark, tx_watermark, rx_request
    }
    st->hfi_seq = 0;
    wmb();
}

#define HFI_HDR(id, size, type, seq)  ((id) | (size) << 8 | (type) << 16 | ((seq) & 0xfff) << 20)

static int
hfi_send(struct pd_msm_dev *mdev, struct a6xx_state *st, u32 id, const u32 *payload, u32 n)
{
    u32 *qh = hfi_qhdr(st, 0), *q = hfi_at(st, HFI_Q0);
    u32 *ah = hfi_qhdr(st, 1), *aq = hfi_at(st, HFI_Q1);
    u32 size = n + 1, slot = round_up(size, 4), w = qh[QH_WRITE];
    u32 hdr = HFI_HDR(id, size, 0, st->hfi_seq++);
    int i;
    
    q[w] = hdr;
    for (i = 0; i < n; i++)
        q[(w + 1 + i) % HFI_QWORDS] = payload[i];
    for (i = size; i < slot; i++)
        q[(w + i) % HFI_QWORDS] = 0xfafafafa;
    wmb();                                          // the message before its index
    qh[QH_WRITE] = (w + slot) % HFI_QWORDS;
    wmb();
    gmu_write(st, GMU_HOST2GMU_INTR_SET, 1);        // doorbell
    
    for (i = 0; (gmu_read(st, GMU_GMU2HOST_INTR_INFO) & 1) == 0; i++) {
        if (i == 1000) {
            printf("pd msm: a6xx: hfi %u: no acknowledgement\n", id);
            return (-ETIMEDOUT);
        }
        udelay(10);
    }
    gmu_write(st, GMU_GMU2HOST_INTR_CLR, 1);
    
    u32 r = ah[QH_READ];
    if (r == ah[QH_WRITE]) {
        printf("pd msm: a6xx: hfi %u: doorbell but queue 1 empty\n", id);
        return (-EIO);
    }
    rmb();
    u32 a0 = aq[r], a1 = aq[(r + 1) % HFI_QWORDS], err = aq[(r + 2) % HFI_QWORDS];
    ah[QH_READ] = (r + round_up((a0 >> 8) & 0xff, 4)) % HFI_QWORDS;
    wmb();
    printf("pd msm: a6xx: hfi %u acknowledged, error %u\n", a0 & 0xff, err);
    if ((a0 & 0xff) != id || a1 != hdr || err != 0)
        return (-EIO);
    return (0);
}

static const u32 sPerfTable[58] = {
      9, 3,
      0x00000000, 0xffffffff, 0,       0x00400201, 0xffffffff, 270000,
      0x00800202, 0xffffffff, 410000,  0x00c00303, 0xffffffff, 500000,
      0x00e00404, 0xffffffff, 547000,  0x01000505, 0xffffffff, 606000,
      0x01400606, 0xffffffff, 640000,  0x01800707, 0xffffffff, 655000,
      0x01a00808, 0xffffffff, 690000,
      [50] = 0x00000000, 0,  0x00400202, 200000,  0x00800203, 500000,
};

static const u32 sBwTable[159] = {
      1, 1, 3, 1, 1,
      [5] = 0x0005003c,
      [11] = 0x40000000,
      [17] = 0x60000001,
      [23] = 0x00050004, 0x00050000, 0x000500ac,
      [31] = 0x40000000, 0x40000000, 0x40000000,
};

#define GBIF_SCACHE_CNTL0     0x3c01
#define GBIF_SCACHE_CNTL1     0x3c02

int
a6xx_gmu_start(struct pd_msm_dev *mdev, struct a6xx_state *st)
{
    static const u32 core_fw_start[] = { 0 };           // handle 0
    static const u32 vote[] = { 1, 8, 0 };              // ack_type 1, gx level 8 (690 MHz), bw 0
    int error, i;
    
    if (!st->rscc_done)
        gmu_rscc_init(st);
    hfi_table_init(st);
    
    // spec 5.2
    gmu_write(st, GMU_AO_HOST_INTERRUPT_CLR, 0xffffffff);
    gmu_write(st, GMU_AO_HOST_INTERRUPT_MASK, 0xffffffd6);
    gmu_write(st, GMU_CX_FALNEXT_INTF, 1);
    gmu_write(st, GMU_CX_FAL_INTF, 1);
    gmu_write(st, GMU_GENERAL_7, 1);
    gmu_write(st, GMU_CM3_FW_INIT_RESULT, 0);
    gmu_write(st, GMU_CM3_BOOT_CONFIG, 2);
    gmu_write(st, GMU_HFI_QTBL_ADDR, HFI_TABLE);
    gmu_write(st, GMU_HFI_QTBL_INFO, 1);
    gmu_write(st, GMU_AHB_FENCE_RANGE_0, 0x802800a0);
    gmu_write(st, GMU_CM3_CFG, 0x00004052);
    gmu_write(st, GMU_HFI_SFR_ADDR, 0x06090000);
    gmu_write(st, GMU_PWR_COL_CP_MSG, 0x60004003);
    for (i = 0; i < 4; i++)
        gpu_write(mdev, GBIF_QSB_SIDE0 + i, 0x00071620);
    gmu_write(st, GMU_SYS_BUS_CONFIG, 1);
    gmu_write(st, GMU_ICACHE_CONFIG, 1);
    gmu_write(st, GMU_DCACHE_CONFIG, 1);
    gmu_write(st, GMU_PWR_COL_INTER_FRAME_CTRL, 0x09c40400);
    gmu_write(st, GMU_RPMH_CTRL, 0x00000f11);
    
    gmu_write(st, GMU_CM3_SYSRESET, 1);
    gmu_write(st, GMU_PWR_COL_CP_RESP, 0);
    gmu_write(st, GMU_CM3_SYSRESET, 0);
    
    for (i = 0; gmu_read(st, GMU_CM3_FW_INIT_RESULT) != 0x100; i++) {
        if (i == 1000) {
            printf("pd msm: a6xx: gmu never initialised, result 0x%08x\n",
                   gmu_read(st, GMU_CM3_FW_INIT_RESULT));
            mdev->gpu.iommu->dump_fault(mdev->gpu.iommu_priv);
            return (-ETIMEDOUT);
        }
        udelay(100);
    }
    printf("pd msm: a6xx: gmu fw 0x%08x up after ~%d us\n",
           gmu_read(st, GMU_CORE_FW_VERSION), i * 100);
    
    gmu_write(st, GMU_HFI_CTRL_INIT, 1);
    printf("pd msm: a6xx: hfi status %u\n", gmu_read(st, GMU_HFI_CTRL_STATUS));
    
    // spec 5.5
    if ((error = hfi_send(mdev, st, 4, sPerfTable, nitems(sPerfTable))) != 0 ||
        (error = hfi_send(mdev, st, 3, sBwTable, nitems(sBwTable))) != 0 ||
        (error = hfi_send(mdev, st, 14, core_fw_start, 1)) != 0 ||
        (error = hfi_send(mdev, st, 10, NULL, 0)) != 0)
        return error;
    gmu_write(st, GMU_GMU2HOST_INTR_CLR, 0xffffffff);
    gmu_write(st, GMU_GMU2HOST_INTR_MASK, 0xff7fffff);      // only CM3_FAULT
    if ((error = hfi_send(mdev, st, 30, vote, nitems(vote))) != 0)
        return (error);
    gpu_write(mdev, GBIF_SCACHE_CNTL0, 0x00003100);
    gpu_write(mdev, GBIF_SCACHE_CNTL1, 0x00c6318c);
    return (0);
}

// spec 5.4: this firmware's gpu oob is bit 30 to request, bit 31 granted and to release.
// a6xx_gmu.xml's oob bits (18, 26) don't apply
#define OOB_GPU_REQUEST               (1u << 30)
#define OOB_GPU_GRANTED               (1u << 31)
#define OOB_GPU_RELEASE               (1u << 31)

int
a6xx_gmu_oob_gpu(struct a6xx_state *st, bool request)
{
    int i;
    
    if (!request) {
        gmu_write(st, GMU_HOST2GMU_INTR_SET, OOB_GPU_RELEASE);
        return (0);
    }
    gmu_write(st, GMU_HOST2GMU_INTR_SET, OOB_GPU_REQUEST);
    for (i = 0; (gmu_read(st, GMU_GMU2HOST_INTR_INFO) & OOB_GPU_GRANTED) == 0; i++) {
        if (i == 1000) {
            printf("pd msm: a6xx: gpu oob not granted, info 0x08x\n",
                   gmu_read(st, GMU_GMU2HOST_INTR_INFO));
            return (-ETIMEDOUT);
        }
        udelay(10);
    }
    gmu_write(st, GMU_GMU2HOST_INTR_CLR, OOB_GPU_GRANTED);
    printf("pd msm: a6xx: gpu oob granted after ~%d us\n", i * 10);
    return (0);
}
