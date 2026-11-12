// test_smmu500.c - the adreno smmu-500 driver against a fake smmu: streams, tables, flushes
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
#include "test.h"

#include "../pd_msm_smmu500.c"

#undef printf

#define PG 0x1000
// idr1: 4 KB pages, two global pages (npages field 0), ncb banks
#define IDR1(ncb)       (u32)(ncb)
#define CB_BASE         (2 * PG)

struct rig {
    struct device_node *gpu, *gmu, *smmu;
    struct fake_win *win;
    struct device dev;
    bool tlb_stuck;
};

static struct rig R;

static bool
smmu_read(struct fake_win *w, u32 off, u32 *v)
{
    // every bank's tlb sync stays busy when the test asks for it
    if (off >= CB_BASE && (off - CB_BASE) % PG == CB_TLBSTATUS) {
        *v = R.tlb_stuck ? 1 : 0;
        return (true);
    }
    return (false);
}

// a gpu with ngpu iommus entries and a gmu, on a smmu reporting nsmr smrs and ncb banks
static void
rig(int ngpu, int nsmr, int ncb)
{
    memset(&R, 0, sizeof(R));
    R.smmu = fake_node("iommu@3da0000", "qcom,adreno-smmu");
    R.win = fake_node_win(R.smmu, CB_BASE + 8 * PG);
    R.win->on_read = smmu_read;
    fake_reg_set(R.win, SMMU_IDR0, (u32)nsmr);
    fake_reg_set(R.win, SMMU_IDR1, IDR1(ncb));
    R.gmu = fake_node("gmu@3d6a000", "qcom,adreno-gmu-690.0");
    fake_phandle(R.gmu, "iommus", R.smmu, 2, 5, 0x400);
    R.gpu = fake_node("gpu@3d00000", "qcom,adreno-690.0");
    if (ngpu > 0)
        fake_phandle(R.gpu, "iommus", R.smmu, 2, 0, 0xc00);
    if (ngpu > 1)
        fake_phandle(R.gpu, "iommus", R.smmu, 2, 1, 0xc00);
    fake_phandle(R.gpu, "qcom,gmu", R.gmu, 0);
    R.dev.of_node = R.gpu;
}

// the level 3 entry the smmu would use for iova, walked through the physical addresses
static u64
walk(struct smmu500 *s, int bank, u64 iova)
{
    u64 *t = (u64 *)(uintptr_t)s->root[bank]->phys;

    for (int level = 0; level < 3; level++) {
        u64 d = t[pt_index(iova, level)];

        if ((d & 3) != 3)
            return (0);
        t = (u64 *)(uintptr_t)(d & DESC_ADDR);
    }
    return (t[pt_index(iova, 3)]);
}

TEST(streams_from_both_cells)
{
    rig(2, 8, 4);
    struct smmu500 *s = smmu_attach(&R.dev);

    CHECK(s != NULL);
    CHECK_EQ(fake_reg(R.win, SMMU_SMR(0)), SMR_VALID | 0xc00u << 16 | 0);
    CHECK_EQ(fake_reg(R.win, SMMU_SMR(1)), SMR_VALID | 0xc00u << 16 | 1);
    // the gmu's mask is its own second cell, not the gpu's
    CHECK_EQ(fake_reg(R.win, SMMU_SMR(2)), SMR_VALID | 0x400u << 16 | 5);
    CHECK_EQ(fake_reg(R.win, SMMU_S2CR(0)), 0);
    CHECK_EQ(fake_reg(R.win, SMMU_S2CR(1)), 0);
    CHECK_EQ(fake_reg(R.win, SMMU_S2CR(2)), 1);
    CHECK_EQ(fake_reg(R.win, SMMU_SMR(3)), 0);
}

// one gpu entry must not leave a second smr matching stream 0
TEST(one_gpu_stream_no_duplicate_smr)
{
    rig(1, 8, 4);
    struct smmu500 *s = smmu_attach(&R.dev);

    CHECK(s != NULL);
    CHECK_EQ(s->nstreams, 2);
    CHECK_EQ(fake_reg(R.win, SMMU_SMR(0)), SMR_VALID | 0xc00u << 16 | 0);
    CHECK_EQ(fake_reg(R.win, SMMU_SMR(1)), SMR_VALID | 0x400u << 16 | 5);
    CHECK_EQ(fake_reg(R.win, SMMU_SMR(2)) & SMR_VALID, 0);
}

TEST(no_gpu_stream_fails_clean)
{
    rig(0, 8, 4);
    CHECK(smmu_attach(&R.dev) == NULL);
    CHECK_EQ(fake_live.kmem, 0);
    CHECK_EQ(fake_live.dma, 0);
}

// a smaller smmu than the streams need is refused, and nothing is left allocated
TEST(too_few_smrs_or_banks_fail_clean)
{
    rig(2, 2, 4);
    CHECK(smmu_attach(&R.dev) == NULL);
    CHECK_EQ(fake_live.kmem, 0);
    CHECK_EQ(fake_live.dma, 0);
    fake_reset();
    rig(2, 8, 1);
    CHECK(smmu_attach(&R.dev) == NULL);
    CHECK_EQ(fake_live.kmem, 0);
    CHECK_EQ(fake_live.dma, 0);
}

TEST(map_writes_page_descriptors)
{
    rig(2, 8, 4);
    struct smmu500 *s = smmu_attach(&R.dev);
    u64 phys = 0x812340000ull;

    CHECK_EQ(smmu_map(s, 0x100000000ull, phys, 3 * PG), 0);
    for (int i = 0; i < 3; i++)
        CHECK_EQ(walk(s, 0, 0x100000000ull + i * PG), (phys + i * PG) | DESC_PAGE);
    CHECK_EQ(walk(s, 0, 0x100000000ull + 3 * PG), 0);
    // the gmu bank's pages are privileged: no el0 access bit
    CHECK_EQ(pd_msm_smmu500_gmu_map(s, 0x60000000, phys, PG), 0);
    CHECK_EQ(walk(s, 1, 0x60000000), phys | DESC_PAGE_PRIV);
    CHECK_EQ(walk(s, 1, 0x60000000) & DESC_AP_RW, 0);
    // bank 0 and bank 1 have separate tables
    CHECK_EQ(walk(s, 0, 0x60000000), 0);
}

TEST(map_rejects_bad_ranges)
{
    rig(2, 8, 4);
    struct smmu500 *s = smmu_attach(&R.dev);

    CHECK_EQ(smmu_map(s, 0x100000800ull, 0x80000000, PG), -EINVAL);
    CHECK_EQ(smmu_map(s, 0x100000000ull, 0x80000800, PG), -EINVAL);
    CHECK_EQ(smmu_map(s, 0x100000000ull, 0x80000000, 0x800), -EINVAL);
    CHECK_EQ(smmu_map(s, 1ull << 48, 0x80000000, PG), -EINVAL);
    // the last page below 2^48 is fine, one more page crosses it
    CHECK_EQ(smmu_map(s, (1ull << 48) - PG, 0x80000000, PG), 0);
    CHECK_EQ(smmu_map(s, (1ull << 48) - PG, 0x80000000, 2 * PG), -EINVAL);
}

// a desktop's worth of allocate and free: the table count stays flat
TEST(churn_does_not_exhaust_tables)
{
    rig(2, 8, 4);
    struct smmu500 *s = smmu_attach(&R.dev);
    long base = fake_live.dma;
    u64 va = 0x100000000ull;
    int failed = 0;

    // 20000 x 2 MB = 40 GB of address space handed out over time, never reused
    for (int i = 0; i < 20000; i++, va += 0x200000) {
        if (smmu_map(s, va, 0x80000000, 0x200000) != 0) {
            failed++;
            break;
        }
        smmu_unmap(s, va, 0x200000);
    }
    CHECK_EQ(failed, 0);
    // what remains is the level 1 and 2 tables on the path, not one level 3 per map
    CHECK(fake_live.dma - base < 64);
    CHECK_EQ(walk(s, 0, va - 0x200000), 0);
}

// a partly unmapped table stays, the remaining page still translates
TEST(partial_unmap_keeps_table)
{
    rig(2, 8, 4);
    struct smmu500 *s = smmu_attach(&R.dev);

    CHECK_EQ(smmu_map(s, 0x100000000ull, 0x80000000, 2 * PG), 0);
    smmu_unmap(s, 0x100000000ull, PG);
    CHECK_EQ(walk(s, 0, 0x100000000ull), 0);
    CHECK_EQ(walk(s, 0, 0x100000000ull + PG), (0x80000000 + PG) | DESC_PAGE);
}

TEST(stuck_tlb_sync_is_an_error)
{
    rig(2, 8, 4);
    struct smmu500 *s = smmu_attach(&R.dev);

    R.tlb_stuck = true;
    CHECK_EQ(smmu_map(s, 0x100000000ull, 0x80000000, PG), -ETIMEDOUT);
}

TEST_MAIN("smmu500",
    T(streams_from_both_cells),
    T(one_gpu_stream_no_duplicate_smr),
    T(no_gpu_stream_fails_clean),
    T(too_few_smrs_or_banks_fail_clean),
    T(map_writes_page_descriptors),
    T(map_rejects_bad_ranges),
    T(churn_does_not_exhaust_tables),
    T(partial_unmap_keeps_table),
    T(stuck_tlb_sync_is_an_error))
