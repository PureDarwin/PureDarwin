// test_a6xx.c - the a6xx core and its gmu against a fake cp, gmu and secure monitor
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
#include "test.h"

#include "../pd_msm_a6xx.c"
#include "../pd_msm_a6xx_gmu.c"

extern long fake_bos;

// the rest of the driver, as far as these two files reach
int pd_msm_stage(void) { return (0); }

static int sGmuMaps;
int
pd_msm_smmu500_gmu_map(void *priv, u32 gmu_va, u64 phys, size_t size)
{
    sGmuMaps++;
    return (0);
}

void __iomem *pd_msm_smmu500_window(void *priv, int which) { return (NULL); }
void __iomem *pd_msm_gpucc8280_window(int which) { return (NULL); }

static void dump_fault(void *priv) { }
static const struct pd_msm_iommu sIommu = { .compatible = "fake", .dump_fault = dump_fault };

// the secure monitor: every call recorded, answers from fake_smc_handler or plain success
void
pd_msm_test_smc(const u64 in[7], u64 out[4], u64 *out6)
{
    if (fake_nsmc < (int)nitems(fake_smcs))
        memcpy(fake_smcs[fake_nsmc++].in, in, sizeof(fake_smcs[0].in));
    memset(out, 0, 4 * sizeof(u64));
    if (out6 != NULL)
        *out6 = 0;
    if (fake_smc_handler != NULL)
        fake_smc_handler(in, out, out6);
    else if (in[0] == 0x42000601)
        out[1] = 1;     // the sip service query: available
}

static int
smc_count(u64 fn)
{
    int n = 0;

    for (int i = 0; i < fake_nsmc; i++)
        n += fake_smcs[i].in[0] == fn;
    return (n);
}

// the fake soc around one a690
static struct {
    struct pd_msm_dev mdev;
    struct platform_device pdev;
    struct device_node *gpu, *gmu, *zap, *zapmem;
    struct fake_win *gpuw, *gmuw;
    struct clk *clk[6];
    u8 zapbuf[0x100000];
    bool cp_alive, gmu_boots, hfi_alive, hfi_bad_ack, oob_grants;
    u32 cp_rptr;
    u32 wptrs[32];          // every CP_RB_WPTR write
    int nwptr;
    u32 opcodes[256];       // every type 7 opcode the cp ran
    int nop;
    u32 host2gmu[64];       // every GMU_HOST2GMU_INTR_SET write
    int nhost2gmu;
    u32 written[2][1024];   // offsets written in the gpu (0) and gmu (1) windows
    int nwritten[2];
} F;

static void
note_write(int which, u32 off)
{
    for (int i = 0; i < F.nwritten[which]; i++)
        if (F.written[which][i] == off)
            return;
    if (F.nwritten[which] < 1024)
        F.written[which][F.nwritten[which]++] = off;
}

#define GMU_OFF(word)   (((word) - GMU_WINDOW_WORD) * 4)

static struct a6xx_state *
st(void)
{
    return (F.mdev.gpu.gen);
}

// the cp: runs what the ring holds up to the new wptr, fences land in memptrs
static void
cp_run(u32 wptr)
{
    struct pd_msm_gpu *g = &F.mdev.gpu;
    u32 *ring = g->ring->cpu;

    while (F.cp_rptr != wptr) {
        u32 h = ring[F.cp_rptr], count;

        if ((h >> 28) == 7) {
            count = h & 0x3fff;
            u32 op = (h >> 16) & 0x7f;
            if (F.nop < (int)nitems(F.opcodes))
                F.opcodes[F.nop++] = op;
            if (op == CP_EVENT_WRITE && count == 4) {
                u64 addr = ring[(F.cp_rptr + 2) % RING_DWORDS] |
                    (u64)ring[(F.cp_rptr + 3) % RING_DWORDS] << 32;
                if (addr == g->memptrs->iova)
                    *(volatile u32 *)g->memptrs->cpu = ring[(F.cp_rptr + 4) % RING_DWORDS];
            }
        } else if ((h >> 28) == 4)
            count = h & 0x7f;
        else
            count = 0;
        F.cp_rptr = (F.cp_rptr + 1 + count) % RING_DWORDS;
    }
    *(volatile u32 *)((u8 *)g->memptrs->cpu + MEMPTRS_RPTR) = F.cp_rptr;
}

static void
gpu_write_hook(struct fake_win *w, u32 off, u32 v)
{
    note_write(0, off);
    if (off != CP_RB_WPTR * 4)
        return;
    if (F.nwptr < (int)nitems(F.wptrs))
        F.wptrs[F.nwptr++] = v;
    if (F.cp_alive)
        cp_run(v);
}

// the gmu's side of hfi: takes one message off queue 0, answers on queue 1
static void
hfi_answer(void)
{
    u32 *qh = hfi_qhdr(st(), 0), *q = hfi_at(st(), HFI_Q0);
    u32 *ah = hfi_qhdr(st(), 1), *aq = hfi_at(st(), HFI_Q1);
    u32 r = qh[QH_READ], hdr = q[r], w = ah[QH_WRITE];

    qh[QH_READ] = (r + round_up((hdr >> 8) & 0xff, 4)) % HFI_QWORDS;
    aq[w] = ((hdr & 0xff) + (F.hfi_bad_ack ? 1 : 0)) | 3 << 8;
    aq[(w + 1) % HFI_QWORDS] = hdr;
    aq[(w + 2) % HFI_QWORDS] = 0;
    aq[(w + 3) % HFI_QWORDS] = 0;
    ah[QH_WRITE] = (w + 4) % HFI_QWORDS;
    fake_reg_set(F.gmuw, GMU_OFF(GMU_GMU2HOST_INTR_INFO),
        fake_reg(F.gmuw, GMU_OFF(GMU_GMU2HOST_INTR_INFO)) | 1);
}

static void
gmu_write_hook(struct fake_win *w, u32 off, u32 v)
{
    note_write(1, off);
    if (off == GMU_OFF(GMU_CM3_SYSRESET) && v == 0 && F.gmu_boots)
        fake_reg_set(w, GMU_OFF(GMU_CM3_FW_INIT_RESULT), 0x100);
    if (off == GMU_OFF(GMU_GMU2HOST_INTR_CLR))
        fake_reg_set(w, GMU_OFF(GMU_GMU2HOST_INTR_INFO),
            fake_reg(w, GMU_OFF(GMU_GMU2HOST_INTR_INFO)) & ~v);
    if (off != GMU_OFF(GMU_HOST2GMU_INTR_SET))
        return;
    if (F.nhost2gmu < (int)nitems(F.host2gmu))
        F.host2gmu[F.nhost2gmu++] = v;
    if ((v & 1) != 0 && F.hfi_alive)
        hfi_answer();
    if ((v & OOB_GPU_REQUEST) != 0 && F.oob_grants)
        fake_reg_set(w, GMU_OFF(GMU_GMU2HOST_INTR_INFO),
            fake_reg(w, GMU_OFF(GMU_GMU2HOST_INTR_INFO)) | OOB_GPU_GRANTED);
}

// firmware the fake gmu and cp take: block headers are va, size and two unused words
static u8 sSqe[64];
static u8 sGmuImage[4 * 16 + 16 + 16 + 32];
static u8 sZap[0x400];

static size_t
gmu_block(u8 *p, u32 va, u32 size, u8 fill)
{
    u32 h[4] = { va, size, 0, 0 };

    memcpy(p, h, sizeof(h));
    memset(p + 16, fill, size);
    return (16 + round_up(size, 4));
}

static size_t
build_gmu_image(void)
{
    size_t n = 0;

    n += gmu_block(sGmuImage + n, 0x00000000, 16, 0x11);       // itcm
    n += gmu_block(sGmuImage + n, 0x10004000, 16, 0x22);       // dtcm
    n += gmu_block(sGmuImage + n, 0x60000000, 0, 0);           // a descriptor
    n += gmu_block(sGmuImage + n, 0x00004000, 32, 0x33);       // memory
    return (n);
}

#define ZAP_PHYS        0x8bf00000ull
#define ZAP_SIZE        0x00100000u

// an elf32 zap image: the metadata segment, a hash segment and one loadable segment
static size_t
build_zap(u32 load_paddr, u32 load_filesz, u32 load_memsz, bool with_hash)
{
    struct zap_ehdr *eh = (void *)sZap;
    struct zap_phdr *ph = (void *)(sZap + sizeof(*eh));
    u32 data = sizeof(*eh) + 3 * sizeof(*ph);

    memset(sZap, 0, sizeof(sZap));
    memcpy(eh->ident, "\177ELF", 4);
    eh->ident[4] = 1;
    eh->phoff = sizeof(*eh);
    eh->phentsize = sizeof(*ph);
    eh->phnum = 3;
    ph[0].type = 0;
    ph[0].offset = 0;
    ph[0].filesz = data;
    ph[1].type = 0;
    ph[1].flags = with_hash ? MDT_TYPE_HASH : 0;
    ph[1].offset = data;
    ph[1].filesz = 0x40;
    ph[2].type = ELF_PT_LOAD;
    ph[2].offset = data + 0x40;
    ph[2].paddr = load_paddr;
    ph[2].filesz = load_filesz;
    ph[2].memsz = load_memsz;
    memset(sZap + data + 0x40, 0x5a, load_filesz);
    return (data + 0x40 + load_filesz);
}

// a gpu node with its gmu, clocks, zap carve-out and register windows, and a working soc
static void
soc(void)
{
    static const char *const clocks[] = { "gmu", "hub", "axi", "memnoc", "ahb", "smmu_vote" };

    memset(&F, 0, sizeof(F));
    sGmuMaps = 0;
    fake_bos = 0;
    F.gpu = fake_node("gpu@3d00000", "qcom,adreno-690.0");
    F.gmu = fake_node("gmu@3d6a000", "qcom,adreno-gmu-690.0");
    F.zap = fake_node("zap-shader", NULL);
    F.zapmem = fake_node("gpu-mem@8bf00000", NULL);
    F.zapmem->has_reg = true;
    F.zapmem->reg.start = ZAP_PHYS;
    F.zapmem->reg.end = ZAP_PHYS + ZAP_SIZE - 1;
    fake_phys_region(ZAP_PHYS, F.zapbuf, ZAP_SIZE);
    fake_phandle(F.zap, "memory-region", F.zapmem, 0);
    fake_node_child(F.gpu, F.zap);
    fake_phandle(F.gpu, "qcom,gmu", F.gmu, 0);
    F.gpuw = fake_node_win(F.gpu, 0x40000);
    F.gpuw->on_write = gpu_write_hook;
    F.gmuw = fake_node_win(F.gmu, 0x26000);
    F.gmuw->on_write = gmu_write_hook;
    fake_node_win(F.gmu, 0x1000);       // rscc
    fake_node_win(F.gmu, 0x5000);       // pdc
    for (int i = 0; i < (int)nitems(clocks); i++)
        F.clk[i] = fake_node_clock(F.gmu, clocks[i]);
    F.pdev.dev.of_node = F.gpu;
    F.mdev.pdev = &F.pdev;
    F.mdev.gpu.mmio = F.gpuw->mem;
    F.mdev.gpu.iommu = &sIommu;
    mutex_init(&F.mdev.gpu.lock);
    F.cp_alive = F.gmu_boots = F.hfi_alive = F.oob_grants = true;
    fake_firmware("qcom/a660_sqe.fw", sSqe, sizeof(sSqe));
    fake_firmware("qcom/a660_gmu.bin", sGmuImage, build_gmu_image());
    fake_firmware("qcom/sc8280xp/LENOVO/21BX/qcdxkmsuc8280.mbn", sZap,
        build_zap((u32)ZAP_PHYS, 0x80, 0x100, true));
}

static u32
last_host2gmu(void)
{
    return (F.nhost2gmu > 0 ? F.host2gmu[F.nhost2gmu - 1] : 0);
}

// packets

// odd parity, counted bit by bit rather than the driver's table
static int
popcount_odd(u32 v)
{
    int n = 0;

    for (; v != 0; v >>= 1)
        n += v & 1;
    return (n & 1);
}

// the header the hardware ran and cffdump decoded on the x13s (spec 4.5)
TEST(pkt7_matches_observed_header)
{
    CHECK_EQ(pkt7(CP_MEM_WRITE, 3), 0x703d8003);
}

TEST(pkt_parity_bits_make_fields_odd)
{
    for (u32 count = 0; count < 0x80; count++)
        for (u32 op = 0; op < 0x80; op++) {
            u32 h = pkt7(op, count);

            CHECK(popcount_odd(count) ^ ((h >> 15) & 1));
            CHECK(popcount_odd(op) ^ ((h >> 23) & 1));
        }
    for (u32 reg = 0; reg < 0x40000; reg += 0x123) {
        u32 h = pkt4(reg, 1);

        CHECK(popcount_odd(reg) ^ ((h >> 27) & 1));
        CHECK_EQ((h >> 8) & 0x3ffff, reg);
    }
}

TEST(ring_space_wraps)
{
    struct pd_msm_bo mem = { 0 };
    u32 memptrs[0x200] = { 0 };
    struct pd_msm_gpu g = { 0 };

    mem.cpu = memptrs;
    g.memptrs = &mem;
    g.wptr = 0;
    CHECK_EQ(ring_space(&g), RING_DWORDS - 1);
    memptrs[MEMPTRS_RPTR / 4] = 100;
    g.wptr = 100;
    CHECK_EQ(ring_space(&g), RING_DWORDS - 1);
    g.wptr = 99;        // one short of a full lap behind the reader
    CHECK_EQ(ring_space(&g), 0);
    memptrs[MEMPTRS_RPTR / 4] = 10;
    g.wptr = RING_DWORDS - 5;
    CHECK_EQ(ring_space(&g), 14);
}

// bring-up

TEST(start_matches_observed_ring_and_zap)
{
    soc();
    CHECK_EQ(a6xx_init(&F.mdev), 0);
    CHECK_EQ(a6xx_start(&F.mdev), 0);
    // spec 4.4: wptr 9 after CP_ME_INIT, 0xb after CP_SET_SECURE_MODE
    CHECK_EQ(F.nwptr, 2);
    CHECK_EQ(F.wptrs[0], 9);
    CHECK_EQ(F.wptrs[1], 0xb);
    // spec 7.2: the three pas calls, once, and the loadable segment copied in
    CHECK_EQ(smc_count(SCM_PAS_INIT_IMAGE), 1);
    CHECK_EQ(smc_count(SCM_PAS_MEM_SETUP), 1);
    CHECK_EQ(smc_count(SCM_PAS_AUTH_AND_RESET), 1);
    CHECK_EQ(F.zapbuf[0], 0x5a);
    CHECK_EQ(F.zapbuf[0x7f], 0x5a);
    CHECK_EQ(F.zapbuf[0x80], 0);
    // the gpu oob was handed back
    CHECK_EQ(last_host2gmu(), OOB_GPU_RELEASE);
    CHECK_EQ(st()->sqe->base.refcount, 1);
}

// a warm start: the same two packets, no pas calls (spec 4.4, 7.2)
TEST(warm_start_skips_zap)
{
    soc();
    a6xx_init(&F.mdev);
    CHECK_EQ(a6xx_start(&F.mdev), 0);
    F.nwptr = 0;
    fake_nsmc = 0;
    F.cp_rptr = 0;
    CHECK_EQ(a6xx_start(&F.mdev), 0);
    CHECK_EQ(F.nwptr, 2);
    CHECK_EQ(F.wptrs[0], 9);
    CHECK_EQ(F.wptrs[1], 0xb);
    CHECK_EQ(smc_count(SCM_PAS_INIT_IMAGE), 0);
}

// a cp that never runs: the start fails, hands the oob back, and a retry neither re-allocates
// the sqe nor enables the clocks a second time
TEST(retry_after_cp_hang_leaks_nothing)
{
    soc();
    a6xx_init(&F.mdev);
    F.cp_alive = false;
    CHECK_EQ(a6xx_start(&F.mdev), -ETIMEDOUT);
    CHECK_EQ(last_host2gmu(), OOB_GPU_RELEASE);
    long bos = fake_bos;
    CHECK_EQ(bos, 3);   // ring, memptrs, sqe
    for (int i = 0; i < (int)nitems(F.clk); i++)
        CHECK_EQ(F.clk[i]->enables, 1);

    F.cp_alive = true;
    F.cp_rptr = 0;
    F.nwptr = 0;
    CHECK_EQ(a6xx_start(&F.mdev), 0);
    CHECK_EQ(fake_bos, bos);
    for (int i = 0; i < (int)nitems(F.clk); i++)
        CHECK_EQ(F.clk[i]->enables, 1);
    CHECK_EQ(smc_count(SCM_PAS_AUTH_AND_RESET), 1);
}

TEST(gmu_that_never_boots_fails)
{
    soc();
    a6xx_init(&F.mdev);
    F.gmu_boots = false;
    CHECK_EQ(a6xx_start(&F.mdev), -ETIMEDOUT);
    // the oob was never asked for, so nothing to hand back
    for (int i = 0; i < F.nhost2gmu; i++)
        CHECK((F.host2gmu[i] & OOB_GPU_REQUEST) == 0);
}

TEST(oob_not_granted_fails)
{
    soc();
    a6xx_init(&F.mdev);
    F.oob_grants = false;
    CHECK_EQ(a6xx_start(&F.mdev), -ETIMEDOUT);
    CHECK_EQ(F.nwptr, 0);
}

// registers written per run, not configuration: pointers, doorbells, resets, acks, tcm loads
static bool
per_run(int which, u32 off)
{
    static const u32 gpu[] = { CP_RB_WPTR, CP_SQE_INSTR_BASE, CP_SQE_INSTR_BASE + 1, CP_RB_BASE,
        CP_RB_BASE + 1, CP_RB_RPTR_ADDR, CP_RB_RPTR_ADDR + 1, RBBM_INT_CLEAR_CMD };
    static const u32 gmu[] = { GMU_HOST2GMU_INTR_SET, GMU_GMU2HOST_INTR_CLR, GMU_CM3_SYSRESET,
        GMU_PWR_COL_CP_RESP, GMU_HFI_CTRL_INIT, GMU_AO_HOST_INTERRUPT_CLR, GMU_PWR_COL_KEEPALIVE };

    if (which == 0) {
        for (int i = 0; i < (int)nitems(gpu); i++)
            if (off == gpu[i] * 4)
                return (true);
        return (false);
    }
    // the itcm and dtcm windows take the gmu image
    if (off >= MW(GMU_CM3_ITCM_START) && off < MW(GMU_CM3_DTCM_START) + 0x4000)
        return (true);
    for (int i = 0; i < (int)nitems(gmu); i++)
        if (off == MW(gmu[i]))
            return (true);
    return (false);
}

static bool
listed(int which, u32 off)
{
    static const struct { int w; u32 off; } regs[] = {
#define LIST(w, o, name) { w, o },
        A6XX_REGS(LIST)
#undef LIST
    };
    int want = which == 0 ? W_GPU : W_GMU;

    for (int i = 0; i < (int)nitems(regs); i++)
        if (regs[i].w == want && regs[i].off == off)
            return (true);
    return (false);
}

// every configuration register the bring-up writes is in pd_msm_a6xx_regs.h, so the linux
// capture and the pdmsmregs dump cover all of it
TEST(register_list_covers_bringup)
{
    soc();
    a6xx_init(&F.mdev);
    CHECK_EQ(a6xx_start(&F.mdev), 0);
    for (int which = 0; which < 2; which++)
        for (int i = 0; i < F.nwritten[which]; i++) {
            u32 off = F.written[which][i];

            if (per_run(which, off) || listed(which, off))
                continue;
            fprintf(stdout, "  %s offset 0x%05x (word 0x%05x) written but not listed\n",
                which == 0 ? "gpu" : "gmu", off, which == 0 ? off / 4 : off / 4 + GMU_WINDOW_WORD);
            test_failures++;
        }
}

// submissions and fences

TEST(submit_emits_fence_and_completes)
{
    soc();
    a6xx_init(&F.mdev);
    CHECK_EQ(a6xx_start(&F.mdev), 0);
    u64 ib = 0x123456000ull;
    u32 dwords = 16;
    F.nop = 0;
    CHECK_EQ(a6xx_submit(&F.mdev, NULL, &ib, &dwords, 1, 7), 0);
    CHECK_EQ(a6xx_completed(&F.mdev), 7);
    CHECK_EQ(F.nop, 4);
    CHECK_EQ(F.opcodes[2], CP_INDIRECT_BUFFER);
    CHECK_EQ(F.opcodes[3], CP_EVENT_WRITE);
    // spec 4.5: the fence event is CACHE_FLUSH_TS with the irq bit
    u32 *ring = F.mdev.gpu.ring->cpu;
    u32 w = F.mdev.gpu.wptr;
    CHECK_EQ(ring[(w + RING_DWORDS - 4) % RING_DWORDS], 0x80000004);
}

// enough submissions to lap the ring several times, every fence seen in order
TEST(submissions_wrap_the_ring)
{
    soc();
    a6xx_init(&F.mdev);
    CHECK_EQ(a6xx_start(&F.mdev), 0);
    u64 ib[4] = { 0x100000000ull, 0x100001000ull, 0x100002000ull, 0x100003000ull };
    u32 dw[4] = { 4, 4, 4, 4 };
    int bad = 0;

    for (u32 seq = 0xffffff00u; seq != 0x400u; seq++) {
        if (a6xx_submit(&F.mdev, NULL, ib, dw, 4, seq) != 0 || a6xx_completed(&F.mdev) != seq) {
            bad++;
            break;
        }
    }
    CHECK_EQ(bad, 0);
}

// a ring the cp stopped reading fills up, and submit says so instead of overwriting it
TEST(full_ring_is_refused)
{
    soc();
    a6xx_init(&F.mdev);
    CHECK_EQ(a6xx_start(&F.mdev), 0);
    F.cp_alive = false;
    u64 ib = 0x100000000ull;
    u32 dw = 4;
    int err = 0, n;

    for (n = 0; n < RING_DWORDS && err == 0; n++)
        err = a6xx_submit(&F.mdev, NULL, &ib, &dw, 1, n + 1);
    CHECK_EQ(err, -EBUSY);
    CHECK(n > 1);
}

// gmu image

TEST(gmu_image_places_blocks)
{
    soc();
    a6xx_init(&F.mdev);
    struct firmware fw = { build_gmu_image(), sGmuImage };

    CHECK_EQ(a6xx_gmu_load(&F.mdev, st(), &fw), 0);
    CHECK_EQ(fake_reg(F.gmuw, GMU_OFF(GMU_CM3_ITCM_START)), 0x11111111);
    CHECK_EQ(fake_reg(F.gmuw, GMU_OFF(GMU_CM3_DTCM_START)), 0x22222222);
    CHECK_EQ(*(u8 *)gmu_cpu(st(), 0x4000, 32), 0x33);
    CHECK(st()->hfi != NULL);
    CHECK_EQ(sGmuMaps, st()->nmem);
}

// a block whose end wraps past 2^32 must not land in the itcm
TEST(gmu_image_rejects_wrapping_block)
{
    soc();
    a6xx_init(&F.mdev);
    u8 img[16 + 0x2000];
    size_t n = gmu_block(img, 0xfffff000u, 0x2000, 0x44);
    struct firmware fw = { n, img };

    CHECK_EQ(a6xx_gmu_load(&F.mdev, st(), &fw), -EINVAL);
    CHECK_EQ(fake_reg(F.gmuw, GMU_OFF(GMU_CM3_ITCM_START)), 0);
}

TEST(gmu_image_rejects_truncated_block)
{
    soc();
    a6xx_init(&F.mdev);
    u8 img[64];
    gmu_block(img, 0x0, 16, 0x11);
    u32 h[4] = { 0, 0x1000, 0, 0 };     // claims more than the file holds
    memcpy(img, h, sizeof(h));
    struct firmware fw = { 32, img };

    CHECK_EQ(a6xx_gmu_load(&F.mdev, st(), &fw), -EINVAL);
}

// hfi

static void
hfi_ready(void)
{
    soc();
    a6xx_init(&F.mdev);
    struct firmware fw = { build_gmu_image(), sGmuImage };
    a6xx_gmu_load(&F.mdev, st(), &fw);
    hfi_table_init(st());
}

TEST(hfi_queue_wraps)
{
    hfi_ready();
    int bad = 0;

    // 58 words a message: the 1024-word queue laps many times
    for (int i = 0; i < 200 && bad == 0; i++)
        bad = hfi_send(&F.mdev, st(), 4, sPerfTable, nitems(sPerfTable)) != 0;
    CHECK_EQ(bad, 0);
}

TEST(hfi_wrong_ack_is_an_error)
{
    hfi_ready();
    F.hfi_bad_ack = true;
    CHECK_EQ(hfi_send(&F.mdev, st(), 10, NULL, 0), -EIO);
}

TEST(hfi_silent_gmu_times_out)
{
    hfi_ready();
    F.hfi_alive = false;
    CHECK_EQ(hfi_send(&F.mdev, st(), 10, NULL, 0), -ETIMEDOUT);
}

// zap shader

static int
zap_with(size_t n)
{
    struct firmware fw = { n, sZap };

    return (a6xx_zap(&F.mdev, st(), &fw));
}

TEST(zap_rejects_bad_images_without_secure_calls)
{
    soc();
    a6xx_init(&F.mdev);
    size_t n = build_zap((u32)ZAP_PHYS, 0x80, 0x100, false);
    CHECK_EQ(zap_with(n), -EINVAL);          // no hash segment
    n = build_zap((u32)ZAP_PHYS, 0x80, ZAP_SIZE + 0x1000, true);
    CHECK_EQ(zap_with(n), -EINVAL);          // larger than the carve-out
    n = build_zap((u32)ZAP_PHYS, 0x80, 0x100, true);
    sZap[0] = 0;
    CHECK_EQ(zap_with(n), -EINVAL);          // not an elf
    n = build_zap((u32)ZAP_PHYS, 0x80, 0x100, true);
    CHECK_EQ(zap_with(sizeof(struct zap_ehdr) + 8), -EINVAL);   // headers cut short
    CHECK_EQ(fake_nsmc, 0);
}

TEST(zap_without_carveout_fails)
{
    soc();
    a6xx_init(&F.mdev);
    st()->zap_phys = 0;
    CHECK_EQ(zap_with(build_zap((u32)ZAP_PHYS, 0x80, 0x100, true)), -ENODEV);
    CHECK_EQ(fake_nsmc, 0);
}

// spec 7.2: x0 1 means interrupted, resumed with x0 1 and the x6 it returned
static int sInterrupts;
static void
interrupting_smc(const u64 in[7], u64 out[4], u64 *out6)
{
    // a resume must carry the x6 the interrupted call returned
    if (in[0] == 1)
        CHECK_EQ(in[6], 0xabc0 + sInterrupts);
    if ((in[0] == SCM_PAS_INIT_IMAGE || in[0] == 1) && sInterrupts < 3) {
        sInterrupts++;
        out[0] = 1;
        *out6 = 0xabc0 + sInterrupts;
        return;
    }
    out[0] = 0;
}

TEST(zap_resumes_interrupted_calls)
{
    soc();
    a6xx_init(&F.mdev);
    sInterrupts = 0;
    fake_smc_handler = interrupting_smc;
    CHECK_EQ(zap_with(build_zap((u32)ZAP_PHYS, 0x80, 0x100, true)), 0);
    CHECK_EQ(sInterrupts, 3);
    CHECK_EQ(smc_count(1), 3);
}

static void
failing_auth(const u64 in[7], u64 out[4], u64 *out6)
{
    out[0] = in[0] == SCM_PAS_AUTH_AND_RESET ? (u64)-22 : 0;
}

TEST(zap_auth_failure_is_reported)
{
    soc();
    a6xx_init(&F.mdev);
    fake_smc_handler = failing_auth;
    long dma = fake_live.dma;
    CHECK_EQ(zap_with(build_zap((u32)ZAP_PHYS, 0x80, 0x100, true)), -EIO);
    CHECK_EQ(fake_live.dma, dma);       // the metadata buffer went back
}

TEST_MAIN("a6xx",
    T(pkt7_matches_observed_header),
    T(pkt_parity_bits_make_fields_odd),
    T(ring_space_wraps),
    T(start_matches_observed_ring_and_zap),
    T(register_list_covers_bringup),
    T(warm_start_skips_zap),
    T(retry_after_cp_hang_leaks_nothing),
    T(gmu_that_never_boots_fails),
    T(oob_not_granted_fails),
    T(submit_emits_fence_and_completes),
    T(submissions_wrap_the_ring),
    T(full_ring_is_refused),
    T(gmu_image_places_blocks),
    T(gmu_image_rejects_wrapping_block),
    T(gmu_image_rejects_truncated_block),
    T(hfi_queue_wraps),
    T(hfi_wrong_ack_is_an_error),
    T(hfi_silent_gmu_times_out),
    T(zap_rejects_bad_images_without_secure_calls),
    T(zap_without_carveout_fails),
    T(zap_resumes_interrupted_calls),
    T(zap_auth_failure_is_reported))
