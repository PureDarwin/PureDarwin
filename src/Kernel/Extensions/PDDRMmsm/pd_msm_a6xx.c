// the a6xx generation (a690 on sc8280xp): gpu init, sqe, ring, zap, submissions, fences
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "pd_fbsd_os.h"

#include "pd_msm_a6xx.h"

#define RING_BYTES                      (32 * 1024)     // spec 4.3
#define GMU_GX_SPTPRAC_CLOCK_CONTROL    0x1a880
#define GMU_CX_POWER_COUNTER_ENABLE     0x1f840
#define GMU_CX_POWER_COUNTER_SELECT_0   0x1f841
#define GMU_AO_CGC_MODE_CNTL            0x23b09
#define GMU_AO_CGC_DELAY_CNTL           0x23b0a
#define GMU_AO_CGC_HYST_CNTL            0x23b0b
#define GMU_AO_GPU_CX_BUSY_STATUS       0x23b0c
#define GMU_AO_GPU_CX_BUSY_MASK         0x23b0e
#define GMU_AHB_FENCE_STATUS            0x23b13
#define CP_RB_BASE                      0x0800
#define CP_RB_CNTL                      0x0802
#define CP_RB_RPTR_ADDR                 0x0804
#define CP_RB_WPTR                      0x0807
#define CP_SQE_CNTL                     0x0808
#define CP_SQE_INSTR_BASE               0x0830
#define RBBM_INT_0_STATUS               0x0201
#define RBBM_STATUS                     0x0210
#define RBBM_STATUS3                    0x0213

#define RING_DWORDS                     (RING_BYTES / 4)
#define RB_CNTL_32K                     0x0000020c
#define MEMPTRS_RPTR                    0x100

#define CP_ME_INIT                      0x48
#define CP_SET_SECURE_MODE              0x66

// spec 4.7: 64-bit addressing everywhere
static const u16 sAddrModeCntl[] = {
    0x0842, 0x0c01, 0x8601, 0x8e05, 0x9e01, 0xbe05, 0xa601, 0x9601, 0x0e00, 0xae01, 0xb601,
    0xf810,
};

// spec 4.7's hardware clock gating table
static const struct { u16 reg; u32 v; } sHwcg[] = {
    { 0x00b0, 0x02222222 }, { 0x00b4, 0x02222220 }, { 0x00b8, 0x00000080 }, { 0x00bc, 0x0000f3cf },
    { 0x00c0, 0x22222222 }, { 0x00c4, 0x22222222 }, { 0x00c8, 0x22222222 }, { 0x00cc, 0x00022222 },
    { 0x00d0, 0x11111111 }, { 0x00d4, 0x11111111 }, { 0x00d8, 0x11111111 }, { 0x00dc, 0x00011111 },
    { 0x00e0, 0x77777777 }, { 0x00e4, 0x77777777 }, { 0x00e8, 0x77777777 }, { 0x00ec, 0x00077777 },
    { 0x00f0, 0x22222222 }, { 0x00f4, 0x01002222 }, { 0x00f8, 0x00002220 }, { 0x0100, 0x00040f00 },
    { 0x0104, 0x25222022 }, { 0x0105, 0x00005555 }, { 0x0106, 0x00000011 }, { 0x0107, 0x00445044 },
    { 0x0108, 0x04222222 }, { 0x0111, 0x00002222 }, { 0x0114, 0x00222222 }, { 0x0117, 0x00000002 },
    { 0x011b, 0x00002222 }, { 0x0109, 0x00004000 }, { 0x0112, 0x00002222 }, { 0x0115, 0x00000200 },
    { 0x011c, 0 },          { 0x010a, 0 },          { 0x0113, 0 },          { 0x0116, 0x04104004 },
    { 0x011d, 0 },          { 0x0120, 0x00000222 }, { 0x0121, 0x00000111 }, { 0x0122, 0 },
    { 0x010b, 0x22222222 }, { 0x0110, 0x00000004 }, { 0x010f, 0x00000002 },
};

// spec 4.7: 32 in the xml's range, then three more this gpu has
static const u32 sProtect[32] = {
    0x13fc0000, 0x00040501, 0x0bd0050b, 0x8000050e, 0x80000510, 0x80000534, 0x82080800, 0x802008a0,
    0x809008ab, 0x02b808de, 0x81340900, 0x89c8098d, 0x80040e00, 0x80300e03, 0x830c3c00, 0x7ffc3cc4,
    0x873c8630, 0x80008e00, 0x801c8e08, 0x807c8e50, 0x89fc8e80, 0x876c9624, 0x80449e60, 0x861c9e78,
    0x873ca630, 0x8000ae02, 0x84bcae50, 0x8000b604, 0x8018b608, 0x8004be02, 0x857cbe20, 0x97fcd000,
};
static const struct { u16 reg; u32 v; } sProtectExtra[] = {
    { 0x0870, 0xaffcf000 }, { 0x0871, 0x7ffcfc00 }, { 0x087f, 0x80011c00 },
};

static const u32 sBicubic[5] = { 0x00000000, 0x3fe05ff4, 0x3fa0ebee, 0x3f5193ed, 0x3f0243f0 };

static void gpu_write64(struct pd_msm_dev *mdev, u32 word, u64 v)
{
    gpu_write(mdev, word, (u32)v);
    gpu_write(mdev, word + 1, (u32)(v >> 32));
}

static u64
gpu_read64(struct pd_msm_dev *mdev, u32 word)
{
    u64 low = gpu_read(mdev, word);
    return (low | (u64)gpu_read(mdev, word + 1) << 32);
}

static u32
odd_parity(u32 v)
{
    v ^= v >> 16;
    v ^= v >> 8;
    v ^= v >> 4;
    return ((0x9669 >> (v & 0xf)) & 1);
}

static u32
pkt7(u32 opcode, u32 count)
{
    return (0x70000000 | count | odd_parity(count) << 15 | (opcode & 0x7f) << 16 |
            odd_parity(opcode) << 23);
}

static u32
pkt4(u32 reg, u32 count)
{
    return (0x40000000 | count | odd_parity(count) << 7 | (reg & 0x3ffff) << 8 |
            odd_parity(reg) << 27);
}

static u32
ring_rptr(struct pd_msm_gpu *g)
{
    return (READ_ONCE(*(volatile u32 *)((u8 *)g->memptrs->cpu + MEMPTRS_RPTR)));
}

static void
ring_emit(struct pd_msm_gpu *g, u32 w)
{
    ((u32 *)g->ring->cpu)[g->wptr] = w;
    g->wptr = (g->wptr + 1) % RING_DWORDS;
}

static void
ring_kick(struct pd_msm_dev *mdev)
{
    struct a6xx_state *st = mdev->gpu.gen;
    u32 fs = 0;
    
    wmb();
    for (int i = 0; i < 100; i++) {
        gpu_write(mdev, CP_RB_WPTR, mdev->gpu.wptr);
        if ((fs = gmu_read(st, GMU_AHB_FENCE_STATUS)) == 0)
            return;
        udelay(1);
    }
    printf("pd msm: a6xx: wptr %u: ahb fence status 0x%08x\n", mdev->gpu.wptr, fs);
}

static int
ring_wait(struct pd_msm_dev *mdev, int ms)
{
    struct pd_msm_gpu *g = &mdev->gpu;
    
    for (int i = 0; i < ms * 100; i++) {
        if (ring_rptr(g) == g->wptr)
            return (0);
        udelay(10);
    }
    printf("pd msm: a6xx: ring stuck: rptr %u wptr %u status 0x%08x int0 0x%08x\n",
           ring_rptr(g), g->wptr, gpu_read(mdev, RBBM_STATUS), gpu_read(mdev, RBBM_INT_0_STATUS));
    mdev->gpu.iommu->dump_fault(mdev->gpu.iommu_priv);
    return (-ETIMEDOUT);
}
// spec 7.3
static int
a6xx_sip_c1b(void)
{
    u64 in[7] = { 0x42000601, 0x1, 0x02000c1b }, out[4], out6;
    
    a6xx_smc(in, out, &out6);
    if (out[0] != 0 || out[1] != 1) {
        printf("pd msm: a6xx: sip 0x42000c1b unavailable (x0 %lld x1 %lld), skipped\n",
               (long long)out[0], (long long)out[1]);
        return (0);
    }
    u64 call[7] = { 0x42000c1b, 0x4, 0xffff0000, 0xffffffff, 0xffffffff, 0xffffffff };
    a6xx_smc(call, out, &out6);
    printf("pd msm: a6xx: sip 0x42000c1b returned %lld\n", (long long)out[0]);
    return (out[0] == 0 ? 0 : -EIO);
}

static int
a6xx_hw_init(struct pd_msm_dev *mdev, struct a6xx_state *st)
{
    int i;
    
    gpu_write(mdev, 0x3c45, 0);                     // GBIF_HALT
    gpu_write(mdev, 0x0016, 0);                     // RBBM_GBIF_HALT
    if (gpu_read(mdev, 0x3c45) != 0 || gpu_read(mdev, 0x0016) != 0)
        printf("pd msm: a6xx: gbif halt did not clear\n");
    gpu_write(mdev, 0xf803, 0);
    gpu_write64(mdev, 0xf800, 0);
    gpu_write(mdev, 0xf802, 0);
    for (i = 0; i < nitems(sAddrModeCntl); i++)
        gpu_write(mdev, sAddrModeCntl[i], 1);
    
    // clock gating, as observed on x13s
    gmu_write(st, GMU_AO_CGC_MODE_CNTL, 0x00020200);
    gmu_write(st, GMU_AO_CGC_DELAY_CNTL, 0x00010111);
    gmu_write(st, GMU_AO_CGC_HYST_CNTL, 0x00005555);
    gmu_write(st, GMU_GX_SPTPRAC_CLOCK_CONTROL, 0);
    for (i = 0; i < nitems(sHwcg); i++)
        gpu_write(mdev, sHwcg[i].reg, sHwcg[i].v);
    gpu_write(mdev, 0x00ae, 0x8aa8aa82);
    gpu_write(mdev, 0x0533, 0x00000182);
    gpu_write(mdev, 0x0044, 0);
    gpu_write(mdev, 0x0042, 0);
    gpu_write(mdev, 0x0118, 0x00000222);
    gpu_write(mdev, 0x0119, 0x00000111);
    gpu_write(mdev, 0x011a, 0x00000555);
    gmu_write(st, GMU_GX_SPTPRAC_CLOCK_CONTROL, 1);
    gpu_write(mdev, 0x00ae, 0x8aa8aa82);
    
    gpu_write(mdev, 0x0011, 3);
    gpu_write(mdev, 0x050b, 0xffffffff);
    
    gpu_write64(mdev, 0x0e05, 0x0000ffffffffffc0ull);
    gpu_write64(mdev, 0x0e09, 0x0000fffffffff000ull);
    gpu_write64(mdev, 0x0e07, 0x0000fffffffff000ull);
    gpu_write(mdev, 0x0e18, 0x00000804);
    gpu_write(mdev, 0x0e17, 0x00000004);
    gpu_write(mdev, 0x08c2, 0x02000140);
    gpu_write(mdev, 0x08c1, 0x8040362c);
    gpu_write(mdev, 0x0b34, 0x00000020);
    gpu_write(mdev, 0x08c3, 0x00000080);
    gpu_write(mdev, 0x9e00, 0x00800200);
    gpu_write(mdev, 0x098d, 0x00000001);
    gpu_write(mdev, 0x0500, 0x00000001);
    gpu_write(mdev, 0x08d0, 0);
    gpu_write(mdev, 0x8e08, 0x00000816);
    gpu_write(mdev, 0xb604, 0x00000006);
    gpu_write(mdev, 0xae02, 0x00000026);
    gpu_write(mdev, 0x0e01, 0x00600000);
    gpu_write(mdev, 0x0534, 0x00000001);
    gpu_write(mdev, 0x001f, 0x404fffff);
    gpu_write(mdev, 0x0e19, 0x00000081);
    for (i = 0; i < nitems(sBicubic); i++)
        gpu_write(mdev, 0xb608 + i, sBicubic[i]);
    
    gmu_write(st, GMU_AO_GPU_CX_BUSY_MASK, 0xff000000);
    gmu_write(st, GMU_CX_POWER_COUNTER_SELECT_0, 0x00000020);
    gmu_write(st, GMU_CX_POWER_COUNTER_ENABLE, 1);
    
    gpu_write(mdev, 0x084f, 0x0000000b);
    for (i = 0; i < nitems(sProtect); i++)
        gpu_write(mdev, 0x0850 + i, sProtect[i]);
    for (i = 0; i < nitems(sProtectExtra); i++)
        gpu_write(mdev, sProtectExtra[i].reg, sProtectExtra[i].v);
    
    gpu_write(mdev, 0x0841, 0x00028801);
    gpu_write(mdev, 0x0011, 0);
    gpu_write(mdev, 0x0e3c, 0x00000090);
    gpu_write(mdev, 0x0844, 0x0000006e);
    gpu_write(mdev, 0x0038, 0x03d0e242);
    
    printf("pd msm: a6xx: gpu registers set, rbbm_status 0x%08x\n", gpu_read(mdev, 0x0210));
    return (a6xx_sip_c1b());
}

static int
a6xx_cp_start(struct pd_msm_dev *mdev, struct a6xx_state *st)
{
    static const u32 me_init[8] = { 0x0000002f, 0x00000003, 0x20000000, 0, 0, 0, 0, 0 };
    struct pd_msm_gpu *g = &mdev->gpu;
    int error;
    
    // the ring restarts at 0 (spec 4.3): clear the old contents and the old read pointer
    memset(g->ring->cpu, 0, RING_BYTES);
    *(volatile u32 *)((u8 *)g->memptrs->cpu + MEMPTRS_RPTR) = 0;
    g->wptr = 0;
    wmb();
    
    gpu_write64(mdev, CP_SQE_INSTR_BASE, st->sqe->iova);
    gpu_write64(mdev, CP_RB_BASE, g->ring->iova);
    gpu_write(mdev, CP_RB_CNTL, RB_CNTL_32K);
    gpu_write64(mdev, CP_RB_RPTR_ADDR, g->memptrs->iova + MEMPTRS_RPTR);
    gpu_write(mdev, CP_SQE_CNTL, 1);
    
    ring_emit(g, pkt7(CP_ME_INIT, nitems(me_init)));
    for (int i = 0; i < nitems(me_init); i++)
        ring_emit(g, me_init[i]);
    ring_kick(mdev);
    
    if ((error = ring_wait(mdev, 1000)) != 0)
        return (error);
    
    // spec 4.7: cx busy bits settle once the cp is idle
    for (int i = 0; i < 1000 && (gmu_read(st, GMU_AO_GPU_CX_BUSY_STATUS) & 0x00800000); i++)
        udelay(10);
    printf("pd msm: a6xx: cp running: rptr %u, sqe 0x%llx ring 0x%llx, fence %u, cx busy 0x%08x, "
           "rbbm_status 0x%08x int0 0x%08x\n", ring_rptr(g), (unsigned long long)st->sqe->iova,
           (unsigned long long)g->ring->iova, gmu_read(st, GMU_AHB_FENCE_STATUS),
           gmu_read(st, GMU_AO_GPU_CX_BUSY_STATUS), gpu_read(mdev, RBBM_STATUS),
           gpu_read(mdev, RBBM_INT_0_STATUS));
    return (0);
}

#define SCM_PAS_INIT_IMAGE      0x42000201      // spec 7.2
#define SCM_PAS_MEM_SETUP       0x42000202
#define SCM_PAS_AUTH_AND_RESET  0x42000205
#define PAS_ID_GPU              13
#define ZAP_ALIGN               0x1000          // not PAGE_SIZE: xnu's is 16 KB, run1's extent was 0x1000
#define MDT_TYPE_MASK           (7u << 24)      // OpenBSD qcpas.c: segment kind in p_flags
#define MDT_TYPE_HASH           (2u << 24)
#define ELF_PT_LOAD             1

struct zap_ehdr {
    u8 ident[16];
    u16 type, machine;
    u32 version, entry, phoff, shoff, flags;
    u16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct zap_phdr {
    u32 type, offset, vaddr, paddr, filesz, memsz, flags, align;
};

// spec 7.2: x0 1 means interrupted, so resume with x0 1, the same x1-x3 + x6 it returned
static s64
scm_call(u64 in[7], int *resumes)
{
    u64 out[4], out6;
    
    *resumes = 0;
    a6xx_smc(in, out, &out6);
    while (out[0] == 1 && *resumes < 1000) {
        in[0] = 1;
        in[6] = out6;
        a6xx_smc(in, out, &out6);
        (*resumes)++;
    }
    return (s64)out[0];
}

// cvac zap shader so secure world can read it
static void
dcache_clean(const void *va, size_t size)
{
    uintptr_t p = (uintptr_t)va & ~63ul, end = (uintptr_t)va + size;
    
    for (; p < end; p += 64)
        __asm__ volatile("dc cvac, %0" : : "r"(p) : "memory");
    __asm__ volatile("dsb sy" : : : "memory");
}

// the zap region is device memory to xnu: single-byte stores only
static void
zap_copy(volatile u8 *dst, const u8 *src, size_t filesz, size_t memsz)
{
    for (size_t i = 0; i < memsz; i++)
        dst[i] = i < filesz ? src[i] : 0;
}

// spec 7.2 with OpenBSD qcpas.c's layout: metadata = segment 0 (elf + program headers) then the
// hash segment. Loadable segments go into the carveout at paddr - lowest paddr
static int
a6xx_zap(struct pd_msm_dev *mdev, struct a6xx_state *st, const struct firmware *zap)
{
    const u8 *fw = zap->data;
    const struct zap_ehdr *eh = (const void *)fw;
    const struct zap_phdr *ph;
    u64 minpa = ~0ull, maxpa = 0, md_phys, in[7];
    size_t md_size;
    u8 *md, *region;
    int hash = 0, n;
    s64 r;
    
    if (st->zap_phys == 0) {
        printf("pd msm: a6xx: zap: no carve-out (does the loader keep /reserved-memory?)\n");
        return (-ENODEV);
    }
    if (zap->size < sizeof(*eh) || memcmp(eh->ident, "\177ELF", 4) != 0 || eh->ident[4] != 1 ||
        eh->phentsize != sizeof(*ph) || eh->phnum < 2 ||
        eh->phoff + (size_t)eh->phnum * sizeof(*ph) > zap->size) {
        printf("pd msm: a6xx: zap: not an elf32 image\n");
        return (-EINVAL);
    }
    ph = (const void *)(fw + eh->phoff);
    for (int i = 0; i < eh->phnum; i++) {
        if ((u64)ph[i].offset + ph[i].filesz > zap->size)
            return (-EINVAL);
        if ((ph[i].flags & MDT_TYPE_MASK) == MDT_TYPE_HASH) {
            if (hash == 0 && i > 0)
                hash = i;
            continue;
        }
        if (ph[i].type != ELF_PT_LOAD || ph[i].memsz == 0)
            continue;
        if (ph[i].paddr < minpa)
            minpa = ph[i].paddr;
        if (round_up((u64)ph[i].paddr + ph[i].memsz, ZAP_ALIGN) > maxpa)
            maxpa = round_up((u64)ph[i].paddr + ph[i].memsz, ZAP_ALIGN);
    }
    if (hash == 0 || maxpa == 0 || maxpa - minpa > st->zap_size) {
        printf("pd msm: a6xx: zap: hash segment %d, extent 0x%llx, carve-out 0x%x\n", hash,
               (unsigned long long)(maxpa - minpa), st->zap_size);
        return (-EINVAL);
    }
    
    md_size = round_up(ph[0].filesz + ph[hash].filesz, PAGE_SIZE);
    md = pdos_malloc_contig(md_size, PAGE_SIZE, pd_msm_dma_max, &md_phys);
    if (md == NULL)
        return (-ENOMEM);
    memcpy(md, fw + ph[0].offset, ph[0].filesz);
    memcpy(md + ph[0].filesz, fw + ph[hash].offset, ph[hash].filesz);
    dcache_clean(md, ph[0].filesz + ph[hash].filesz);
    
    memset(in, 0, sizeof(in));
    in[0] = SCM_PAS_INIT_IMAGE; in[1] = 0x82; in[2] = PAS_ID_GPU; in[3] = md_phys;
    r = scm_call(in, &n);
    printf("pd msm: a6xx: zap: PAS_INIT_IMAGE metadata 0x%llx: %lld after %d resumes\n",
           (unsigned long long)md_phys, (long long)r, n);
    if (r != 0)
        goto out;
    
    memset(in, 0, sizeof(in));
    in[0] = SCM_PAS_MEM_SETUP; in[1] = 0x3; in[2] = PAS_ID_GPU; in[3] = st->zap_phys;
    in[4] = maxpa - minpa;
    r = scm_call(in, &n);
    printf("pd msm: a6xx: zap: PAS_MEM_SETUP 0x%llx +0x%llx: %lld\n",
           (unsigned long long)st->zap_phys, (unsigned long long)(maxpa - minpa), (long long)r);
    if (r != 0)
        goto out;
    
    region = pdos_map_phys(st->zap_phys, st->zap_size, 1);
    if (region == NULL) {
        r = -ENOMEM;
        goto out;
    }
    for (int i = 0; i < eh->phnum; i++) {
        if ((ph[i].flags & MDT_TYPE_MASK) == MDT_TYPE_HASH || ph[i].type != ELF_PT_LOAD ||
            ph[i].memsz == 0)
            continue;
        zap_copy(region + (ph[i].paddr - minpa), fw + ph[i].offset, ph[i].filesz, ph[i].memsz);
    }
    __asm__ volatile("dsb sy" : : : "memory");
    pdos_unmap_phys(region, st->zap_size);
    
    memset(in, 0, sizeof(in));
    in[0] = SCM_PAS_AUTH_AND_RESET; in[1] = 0x1; in[2] = PAS_ID_GPU;
    r = scm_call(in, &n);
    printf("pd msm: a6xx: zap: PAS_AUTH_AND_RESET: %lld\n", (long long)r);
out:
    pdos_free_contig(md, md_size);
    return (r == 0 ? 0 : -EIO);
}

// spec 4.4: CP_SET_SECURE_MODE 0 after the zap, at every power-up
static int
a6xx_secure_exit(struct pd_msm_dev *mdev, struct a6xx_state *st)
{
    struct pd_msm_gpu *g = &mdev->gpu;
    int error;
    
    ring_emit(g, pkt7(CP_SET_SECURE_MODE, 1));
    ring_emit(g, 0);
    ring_kick(mdev);
    if ((error = ring_wait(mdev, 1000)) != 0)
        return (error);
    printf("pd msm: a6xx: secure mode left, rptr %u\n", ring_rptr(g));
    return 0;
}

#define RBBM_INT_CLEAR_CMD      0x0037
#define CP_SCRATCH_2            0x0885          // CP_SCRATCH[0] is 0x0883 in crashdec
#define GMU_PWR_COL_KEEPALIVE   0x1f8c3         // run1 0x03d7e30c

#define CP_MEM_WRITE            0x3d
#define CP_INDIRECT_BUFFER      0x3f
#define CP_EVENT_WRITE          0x46
#define EV_CACHE_FLUSH_TS       0x04
#define EV_IRQ                  (1u << 31)
#define INT0_CACHE_FLUSH_TS     (1u << 20)

static u32
ring_space(struct pd_msm_gpu *g)
{
    return ((ring_rptr(g) + RING_DWORDS - g->wptr - 1) % RING_DWORDS);
}

static int
ring_reserve(struct pd_msm_dev *mdev, u32 n)
{
    struct pd_msm_gpu *g = &mdev->gpu;
    
    for (int i = 0; i < 100000; i++) {
        if (ring_space(g) >= n)
            return 0;
        udelay(10);
    }
    printf("pd msm: a6xx: ring full: rptr %u wptr %u status 0x%08x\n", ring_rptr(g), g->wptr,
           gpu_read(mdev, RBBM_STATUS));
    return -EBUSY;
}

static int
a6xx_submit(struct pd_msm_dev *mdev, struct drm_file *file, const u64 *ib_iova,
            const u32 *ib_dwords, int nib, u32 seqno)
{
    struct pd_msm_gpu *g = &mdev->gpu;
    u64 fence = g->memptrs->iova;
    int error;
    
    if ((error = ring_reserve(mdev, nib * 4 + 11)) != 0)
        return (error);
    ring_emit(g, pkt7(CP_EVENT_WRITE, 1));
    ring_emit(g, 0x18); // PC_CCU_INVALIDATE_DEPTH = 0x18
    ring_emit(g, pkt7(CP_EVENT_WRITE, 1));
    ring_emit(g, 0x19); // PC_CCU_INVALIDATE_COLOR = 0x19
    for (int i = 0; i < nib; i++) {
        ring_emit(g, pkt7(CP_INDIRECT_BUFFER, 3));
        ring_emit(g, (u32)ib_iova[i]);
        ring_emit(g, (u32)(ib_iova[i] >> 32));
        ring_emit(g, ib_dwords[i]);
    }
    ring_emit(g, pkt4(CP_SCRATCH_2, 1));
    ring_emit(g, seqno);
    ring_emit(g, pkt7(CP_EVENT_WRITE, 4));
    ring_emit(g, EV_IRQ | EV_CACHE_FLUSH_TS);
    ring_emit(g, (u32)fence);
    ring_emit(g, (u32)(fence >> 32));
    ring_emit(g, seqno);
    ring_kick(mdev);
    return 0;
}

static u32
a6xx_completed(struct pd_msm_dev *mdev)
{
    rmb();
    return (READ_ONCE(*(volatile u32 *)mdev->gpu.memptrs->cpu));
}

// polled waits, so the interrupt is acked here
static void
a6xx_fence_ack(struct pd_msm_dev *mdev)
{
    struct a6xx_state *st = mdev->gpu.gen;
    static int logged;
    u32 s;
    
    gmu_write(st, GMU_PWR_COL_KEEPALIVE, 1);
    s = gpu_read(mdev, RBBM_INT_0_STATUS);
    if (s != 0)
        gpu_write(mdev, RBBM_INT_CLEAR_CMD, s);
    gmu_write(st, GMU_PWR_COL_KEEPALIVE, 0);
    if (logged < 4 || (s & ~INT0_CACHE_FLUSH_TS) != 0) {
        logged++;
        printf("pd msm: a6xx: fence ack: int0 0x%08x\n", s);
    }
}

static u32
a6xx_selftest_ib(u32 *ib, u64 dst, u32 value)
{
    ib[0] = pkt7(CP_MEM_WRITE, 3);
    ib[1] = (u32)dst;
    ib[2] = (u32)(dst >> 32);
    ib[3] = value;
    return 4;
}

void
a6xx_smc(const u64 in[7], u64 out[4], u64 *out6)
{
    register u64 x0 __asm__("x0") = in[0];
    register u64 x1 __asm__("x1") = in[1];
    register u64 x2 __asm__("x2") = in[2];
    register u64 x3 __asm__("x3") = in[3];
    register u64 x4 __asm__("x4") = in[4];
    register u64 x5 __asm__("x5") = in[5];
    register u64 x6 __asm__("x6") = in[6];
    
    __asm__ volatile("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5),
                     "+r"(x6) : : "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16",
                     "x17", "memory");
    out[0] = x0;
    out[1] = x1;
    out[2] = x2;
    out[3] = x3;
    if (out6 != NULL)
        *out6 = x6;
}

// probe: the generation's state, gmu node, kernel's buffers. gpu is still off so no reads here
static int
a6xx_init(struct pd_msm_dev *mdev)
{
    struct pd_msm_gpu *g = &mdev->gpu;
    struct a6xx_state *st;
    struct device_node *zap_np, *mem_np;
    struct resource res;
    int error;
    
    st = kzalloc(sizeof(*st), GFP_KERNEL);
    if (st == NULL)
        return (-ENOMEM);
    g->gen = st;
    if ((error = a6xx_gmu_find(mdev, st)) != 0)
        return (error);
    // zap shader: the gpu's zap-shader child names a reserved-memory node
    zap_np = of_get_child_by_name(mdev->pdev->dev.of_node, "zap-shader");
    mem_np = zap_np != NULL ? of_parse_phandle(zap_np, "memory-region", 0) : NULL;
    if (mem_np != NULL && of_address_to_resource(mem_np, 0, &res) == 0) {
        st->zap_phys = res.start;
        st->zap_size = resource_size(&res);
    } else
        dev_err(&mdev->pdev->dev, "a6xx: zap region: child %p, memory-region %p\n", zap_np, mem_np);
    g->ring = pd_msm_bo_new(&mdev->drm, RING_BYTES, PD_MSM_BO_KERNEL);
    g->memptrs = pd_msm_bo_new(&mdev->drm, PAGE_SIZE, PD_MSM_BO_KERNEL);
    if (IS_ERR(g->ring) || IS_ERR(g->memptrs))
        return (-ENOMEM);
    if ((error = pd_msm_bo_iova(mdev, g->ring)) != 0 ||
        (error = pd_msm_bo_iova(mdev, g->memptrs)) != 0)
        return (error);
    dev_info(&mdev->pdev->dev, "a6xx: ring 0x%llx, memptrs 0x%llx, zap 0x%llx +0x%x\n",
             (unsigned long long)g->ring->iova, (unsigned long long)g->memptrs->iova,
             (unsigned long long)st->zap_phys, st->zap_size);
    return (0);
}

static bool
stop_at(int stage)
{
    if (pd_msm_stage() != stage)
        return (false);
    printf("pd msm: a6xx: stage %d reached, stopping (pdmsmstage)\n", stage);
    return (true);
}

// the first open: firmware from /Library/Firmware, then the bring-up in spec 9.1's order
static int
a6xx_start(struct pd_msm_dev *mdev)
{
    struct device *dev = &mdev->pdev->dev;
    struct a6xx_state *st = mdev->gpu.gen;
    const struct firmware *sqe = NULL, *gmu = NULL, *zap = NULL;
    int error;
    
    if ((error = request_firmware(&sqe, "qcom/a660_sqe.fw", dev)) != 0 ||
        (error = request_firmware(&gmu, "qcom/a660_gmu.bin", dev)) != 0 ||
        (error = request_firmware(&zap, "qcom/sc8280xp/LENOVO/21BX/qcdxkmsuc8280.mbn", dev)) != 0) {
        dev_err(dev, "a6xx: firmware: %d\n", error);
        goto out;
    }
    st->sqe = pd_msm_bo_new(&mdev->drm, sqe->size - 4, PD_MSM_BO_KERNEL);
    if (IS_ERR(st->sqe) || (error = pd_msm_bo_iova(mdev, st->sqe)) != 0) {
        error = -ENOMEM;
        goto out;
    }
    // spec 4.2: the gpu is given the file from byte 4
    memcpy(st->sqe->cpu, sqe->data + 4, sqe->size - 4);
    wmb();
    
    if ((error = a6xx_gmu_power(mdev, st)) != 0)
        goto out;
    if (stop_at(1)) {
        error = -EAGAIN;
        goto out;
    }
    if ((error = a6xx_gmu_load(mdev, st, gmu)) != 0 || (error = a6xx_gmu_start(mdev, st)) != 0)
        goto out;
    if (stop_at(2)) {
        error = -EAGAIN;
        goto out;
    }
    if ((error = a6xx_gmu_oob_gpu(st, true)) != 0 || (error = a6xx_hw_init(mdev, st)) != 0 ||
        (error = a6xx_cp_start(mdev, st)) != 0)
        goto out;
    if (stop_at(3)) {
        error = -EAGAIN;
        goto out;
    }
    if (!st->zap_done) {
        if ((error = a6xx_zap(mdev, st, zap)) != 0)
            goto out;
        st->zap_done = true;
    }
    if ((error = a6xx_secure_exit(mdev, st)) != 0)
        goto out;
    if (stop_at(4)) {
        error = -EAGAIN;
        goto out;
    }
    error = a6xx_gmu_oob_gpu(st, false);
    if (error == 0)
        dev_info(dev, "a6xx: cp running\n");
out:
    if (sqe != NULL)
        release_firmware(sqe);
    if (gmu != NULL)
        release_firmware(gmu);
    if (zap != NULL)
        release_firmware(zap);
    return (error);
}

static void
a6xx_dump(struct pd_msm_dev *mdev)
{
    struct pd_msm_gpu *g = &mdev->gpu;
    struct a6xx_state *st = g->gen;
    
    gmu_write(st, GMU_PWR_COL_KEEPALIVE, 1);
    printf("pd msm: a6xx: dump: fence %u of %u, rptr %u wptr %u, rbbm_status 0x%08x status3 0x%08x "
           "int0 0x%08x\n", a6xx_completed(mdev), g->seqno, ring_rptr(g), g->wptr,
           gpu_read(mdev, RBBM_STATUS), gpu_read(mdev, RBBM_STATUS3),
           gpu_read(mdev, RBBM_INT_0_STATUS));
    printf("pd msm: a6xx: dump: ib1 0x%llx rem %u, ib2 0x%llx rem %u, hw_fault 0x%08x "
           "cp_int 0x%08x protect 0x%08x\n",
           (unsigned long long)gpu_read64(mdev, 0x0928), gpu_read(mdev, 0x092a),
           (unsigned long long)gpu_read64(mdev, 0x092b), gpu_read(mdev, 0x092d),
           gpu_read(mdev, 0x0821), gpu_read(mdev, 0x0823), gpu_read(mdev, 0x0824));
    gmu_write(st, GMU_PWR_COL_KEEPALIVE, 0);
    g->iommu->dump_fault(g->iommu_priv);
}

const struct pd_msm_gpu_funcs pd_msm_a6xx_funcs = {
    .init = a6xx_init,
    .start = a6xx_start,
    .submit = a6xx_submit,
    .completed = a6xx_completed,
    .fence_ack = a6xx_fence_ack,
    .dump = a6xx_dump,
    .selftest_ib = a6xx_selftest_ib,
};
