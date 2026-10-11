/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */

#include "PDRadeonCard.h"
#include "PDRadeonRenderShared.h"
#define RADEON_PACKET_TYPE3 3u
#include "RadeonR700Regs.h"

#include <libkern/OSAtomic.h>
#include <pexpert/pexpert.h>

// Firmware remains an unmodified binary; its redistribution notice is installed with the kext.
__asm__(".section __TEXT,__const\n.p2align 4\n"
        ".private_extern _pd_radeon_pfp_start\n_pd_radeon_pfp_start:\n"
        ".incbin \"" PDRADEON_FW_DIR "/RV730_pfp.bin\"\n"
        ".private_extern _pd_radeon_pfp_end\n_pd_radeon_pfp_end:\n"
        ".p2align 4\n.private_extern _pd_radeon_me_start\n_pd_radeon_me_start:\n"
        ".incbin \"" PDRADEON_FW_DIR "/RV730_me.bin\"\n"
        ".private_extern _pd_radeon_me_end\n_pd_radeon_me_end:\n.text\n");
extern "C" const uint8_t pd_radeon_pfp_start[], pd_radeon_pfp_end[];
extern "C" const uint8_t pd_radeon_me_start[], pd_radeon_me_end[];

static const uint32_t ringOffset = 0x2200000;
static const uint32_t ringWords = 0x10000 / sizeof(uint32_t);
static const uint32_t packet2 = 0x80000000;

static uint32_t firmwareWord(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void emit(RadeonCard *card, uint32_t word)
{
    volatile uint32_t *ring = (volatile uint32_t *)(card->vramCpu + ringOffset);
    ring[card->ringWrite] = word;
    card->ringWrite = (card->ringWrite + 1) & (ringWords - 1);
}

static void commit(RadeonCard *card)
{
    while (card->ringWrite & 15)
        emit(card, packet2);
    // The ring and IB use write-combined BAR memory. OSSynchronizeIO is a
    // no-op on x86; drain these stores before publishing the new write pointer.
#if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile ("sfence" ::: "memory");
#else
    OSSynchronizeIO();
#endif
    // PCIe RV7xx HDP flush workaround from r600_mmio_hdp_flush: writing the
    // older flush register is unreliable on these chips. Follow HDP_DEBUG1
    // with an uncached framebuffer read before starting GPU memory fetches.
    card->wreg(HDP_DEBUG1, 0);
    (void)*(volatile uint32_t *)(card->vramCpu + ringOffset);
    card->wreg(CP_RB_WPTR, card->ringWrite);
    card->rreg(CP_RB_WPTR);
}

// read-only dump of the clock block, the pll status bits show the board never completes ctlreq_chg
static void clockProbe(RadeonCard *card)
{
    LOG("clk 600: %08x %08x %08x %08x\n", card->rreg(0x600), card->rreg(0x604), card->rreg(0x608), card->rreg(0x60c));
    LOG("clk 61c: %08x %08x %08x %08x\n", card->rreg(0x61c), card->rreg(0x624), card->rreg(0x628), card->rreg(0x62c));
}

// diagnostic only: pick the sclk mux source with the bios handshake (rv730_sclksel=N)
static void sclkMuxSelect(RadeonCard *card)
{
    uint32_t sel = 0;
    if (!PE_parse_boot_argn("rv730_sclksel", &sel, sizeof(sel)))
        return;
    uint32_t base = card->rreg(0x604) & ~0x1ffu & ~(1u << 26);
    card->wreg(0x604, base | sel);
    card->wreg(0x604, base | sel | SCLK_MUX_UPDATE);
    for (unsigned i = 0; i < 50 && !(card->rreg(0x60c) & 2); i++)
        IOSleep(1);
    card->wreg(0x604, base | sel);
    LOG("sclk mux set to %u: 604=%08x 60c=%08x\n", sel, card->rreg(0x604), card->rreg(0x60c));
}

bool RadeonCard::accelInit(void)
{
    if (vramMap->getLength() < ringOffset + ringWords * sizeof(uint32_t))
        return false;
    if (pd_radeon_pfp_end - pd_radeon_pfp_start != 848 * 4 ||
        pd_radeon_me_end - pd_radeon_me_start != 1360 * 4) {
        LOG("unexpected command processor firmware size\n");
        return false;
    }
    uint64_t visible = vramMap->getLength();
    if (visible > vramSize)
        visible = vramSize;
    if (visible <= PD_RADEON_HEAP_START)
        return false;
    renderPageCount = (uint32_t)((visible - PD_RADEON_HEAP_START) / 4096);
    renderPages = (uint8_t *)IOMallocZero(renderPageCount);
    if (!renderPages)
        return false;
    IOLockLock(lock);
    // The graphics memory clients need TLB/cache setup even with no GART.
    // Keep every VM context disabled: the BO heap uses local physical MC addresses.
    wreg(VM_L2_CNTL, ENABLE_L2_CACHE | ENABLE_L2_FRAGMENT_PROCESSING |
        ENABLE_L2_PTE_CACHE_LRU_UPDATE_BY_WRITE | EFFECTIVE_L2_QUEUE_SIZE(7));
    wreg(VM_L2_CNTL2, 0);
    wreg(VM_L2_CNTL3, BANK_SELECT(0) | CACHE_UPDATE_MODE(2));
    uint32_t tlb = ENABLE_L1_TLB | ENABLE_L1_FRAGMENT_PROCESSING |
        SYSTEM_ACCESS_MODE_NOT_IN_SYS | SYSTEM_APERTURE_UNMAPPED_ACCESS_PASS_THRU |
        EFFECTIVE_L1_TLB_SIZE(5) | EFFECTIVE_L1_QUEUE_SIZE(5);
    wreg(MC_VM_MD_L1_TLB0_CNTL, tlb);
    wreg(MC_VM_MD_L1_TLB1_CNTL, tlb);
    wreg(MC_VM_MD_L1_TLB2_CNTL, tlb);
    wreg(MC_VM_MB_L1_TLB0_CNTL, tlb);
    wreg(MC_VM_MB_L1_TLB1_CNTL, tlb);
    wreg(MC_VM_MB_L1_TLB2_CNTL, tlb);
    wreg(MC_VM_MB_L1_TLB3_CNTL, tlb);
    for (unsigned i = 0; i < 7; i++)
        wreg(VM_CONTEXT0_CNTL + i * 4, 0);
    LOG("graphics memory clients: L2=0x%08x L1=0x%08x physical VRAM, VM contexts off\n",
        rreg(VM_L2_CNTL), rreg(MC_VM_MB_L1_TLB0_CNTL));
    gpuInit();
    wreg(CP_ME_CNTL, CP_ME_HALT | CP_PFP_HALT);
    wreg(SCRATCH_UMSK, 0);
    wreg(CP_RB_CNTL, RB_NO_UPDATE | RB_BLKSZ(15) | RB_BUFSZ(3));
    wreg(GRBM_SOFT_RESET, SOFT_RESET_CP);
    rreg(GRBM_SOFT_RESET);
    IOSleep(15);
    wreg(GRBM_SOFT_RESET, 0);
    wreg(CP_PFP_UCODE_ADDR, 0);
    for (unsigned i = 0; i < 848; i++)
        wreg(CP_PFP_UCODE_DATA, firmwareWord(pd_radeon_pfp_start + i * 4));
    wreg(CP_PFP_UCODE_ADDR, 0);
    wreg(CP_ME_RAM_WADDR, 0);
    for (unsigned i = 0; i < 1360; i++)
        wreg(CP_ME_RAM_DATA, firmwareWord(pd_radeon_me_start + i * 4));
    wreg(CP_ME_RAM_WADDR, 0);
    wreg(CP_ME_RAM_RADDR, 0);

    volatile uint32_t *ring = (volatile uint32_t *)(vramCpu + ringOffset);
    for (unsigned i = 0; i < ringWords; i++)
        ring[i] = packet2;
    ringWrite = 0;
    uint32_t ctl = RB_BLKSZ(9) | RB_BUFSZ(13) | RB_NO_UPDATE;
    wreg(CP_RB_CNTL, ctl | RB_RPTR_WR_ENA);
    wreg(CP_RB_RPTR_WR, 0);
    wreg(CP_RB_WPTR, 0);
    wreg(CP_RB_RPTR_ADDR, ringOffset + 0x10000);
    wreg(CP_RB_RPTR_ADDR_HI, 0);
    wreg(SCRATCH_ADDR, (ringOffset + 0x10100) >> 8);
    wreg(CP_SEM_WAIT_TIMER, 0);
    wreg(CP_RB_WPTR_DELAY, 0);
    wreg(CP_RB_CNTL, ctl);
    wreg(0xc100, ringOffset >> 8);
    wreg(0xc1fc, (1u << 27) | (1u << 28));
    wreg(CP_QUEUE_THRESHOLDS, ROQ_IB1_START(0x16) | ROQ_IB2_START(0x2b));
    wreg(CP_MEQ_THRESHOLDS, STQ_SPLIT(0x30));

    emit(this, PACKET3(0x44, 5));
    emit(this, 1);
    emit(this, 0);
    emit(this, 7);
    emit(this, 1u << 16);
    emit(this, 0);
    emit(this, 0);
    wreg(SCRATCH_REG0, 0xcafedead);
    emit(this, PACKET3(0x68, 1));
    emit(this, (SCRATCH_REG0 - 0x8000) >> 2);
    emit(this, 0xdeadbeef);
    commit(this);
    wreg(CP_ME_CNTL, 0xff);
    for (unsigned i = 0; i < 1000; i++) {
        if (rreg(SCRATCH_REG0) == 0xdeadbeef) {
            accelReady = true;
            break;
        }
        IODelay(1000);
    }
    LOG("CP ring test %s: scratch=0x%08x rptr=%u wptr=%u status=0x%08x\n",
        accelReady ? "passed" : "failed", rreg(SCRATCH_REG0), rreg(CP_RB_RPTR),
        ringWrite, rreg(0x8010));
    if (!accelReady)
        wreg(CP_ME_CNTL, CP_ME_HALT | CP_PFP_HALT);
    IOLockUnlock(lock);
    if (accelReady)
        gartInit();
    if (accelReady)
        clockProbe(this);
    sclkMuxSelect(this);
    return accelReady;
}

static const uint32_t gartTableOffset = 0x2400000;
static const uint64_t gartStart = 0x40000000ull;
static const uint32_t gartPages = 8192;

// pcie gart window for gpu writes into system memory, page table lives in vram
bool RadeonCard::gartInit(void)
{
    if (!accelReady || vramMap->getLength() < gartTableOffset + 0x80000)
        return false;
    gartLock = IOLockAlloc();
    if (!gartLock)
        return false;
    pci->setBusMasterEnable(true);
    volatile uint64_t *table = (volatile uint64_t *)(vramCpu + gartTableOffset);
    for (uint32_t i = 0; i < gartPages; i++)
        table[i] = 0;
    wreg(VM_CONTEXT0_PAGE_TABLE_START_ADDR, (uint32_t)(gartStart >> 12));
    wreg(VM_CONTEXT0_PAGE_TABLE_END_ADDR, (uint32_t)((gartStart + (uint64_t)gartPages * 4096 - 1) >> 12));
    wreg(VM_CONTEXT0_PAGE_TABLE_BASE_ADDR, gartTableOffset >> 12);
    wreg(VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR, (gartTableOffset + 0x80000) >> 12);
    wreg(VM_CONTEXT0_CNTL, ENABLE_CONTEXT | PAGE_TABLE_DEPTH(0) | RANGE_PROTECTION_FAULT_ENABLE_DEFAULT);
    gartReady = true;
    LOG("gart: %u pages at mc 0x%llx, table at vram 0x%x, bus master on\n", gartPages, gartStart, gartTableOffset);
    return true;
}

static void gartFlush(RadeonCard *card)
{
#if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile ("sfence" ::: "memory");
#endif
    card->wreg(HDP_DEBUG1, 0);
    (void)*(volatile uint32_t *)(card->vramCpu + gartTableOffset);
    card->wreg(0x1490, (uint32_t)(gartStart >> 12));
    card->wreg(0x14B0, (uint32_t)((gartStart + (uint64_t)gartPages * 4096 - 1) >> 12));
    card->wreg(0x1470, 1);
    for (unsigned i = 0; i < 2000; i++) {
        uint32_t resp = (card->rreg(0x1470) & 0xf0) >> 4;
        if (resp)
            return;
        IODelay(1);
    }
}

bool RadeonCard::copyOut(IOMemoryDescriptor *dst, uint32_t srcOffset, uint32_t srcPitch, uint32_t rowBytes,
                         uint32_t rows, uint32_t dstPitch)
{
    if (!gartReady)
        return false;
    uint64_t length = (uint64_t)(rows - 1) * dstPitch + rowBytes;
    if (dst->prepare() != kIOReturnSuccess)
        return false;
    IOLockLock(gartLock);
    volatile uint64_t *table = (volatile uint64_t *)(vramCpu + gartTableOffset);
    uint32_t pages = 0;
    uint32_t firstOffset = 0;
    bool ok = true;
    for (uint64_t o = 0; o < length && ok;) {
        IOByteCount seg = 0;
        addr64_t phys = dst->getPhysicalSegment(o, &seg, kIOMemoryMapperNone);
        if (!phys || !seg) { ok = false; break; }
        if (!o)
            firstOffset = (uint32_t)(phys & 0xfff);
        for (addr64_t a = phys & ~0xfffull; a < phys + seg; a += 4096) {
            if (pages >= gartPages) { ok = false; break; }
            // radeon.h R600_PTE_VALID|SYSTEM|SNOOPED|READABLE|WRITEABLE, built as rs600_gart_get_page_entry does
            table[pages++] = a | 0x1 | 0x2 | 0x4 | 0x20 | 0x40;
        }
        o += seg;
    }
    if (ok) {
        gartFlush(this);
        uint32_t base = (uint32_t)gartStart + firstOffset;
        uint32_t count = 0;
        uint32_t *ib = (uint32_t *)IOMalloc(65536 * 4);
        ok = ib != NULL;
        if (ok) {
            bool contiguous = srcPitch == dstPitch;
            uint32_t runs = contiguous ? 1 : rows;
            uint32_t runBytes = contiguous ? (uint32_t)length : rowBytes;
            for (uint32_t r = 0; r < runs && ok; r++) {
                uint32_t done = 0;
                while (done < runBytes) {
                    uint32_t n = runBytes - done > 0x100000 ? 0x100000 : runBytes - done;
                    if (count + 6 > 65536 - 8) { ok = false; break; }
                    bool last = r == runs - 1 && done + n == runBytes;
                    ib[count++] = PACKET3(0x41, 4);
                    ib[count++] = srcOffset + r * srcPitch + done;
                    ib[count++] = last ? (1u << 31) : 0;
                    ib[count++] = base + r * dstPitch + done;
                    ib[count++] = 0;
                    ib[count++] = n;
                    done += n;
                }
            }
            if (ok)
                ok = accelSubmit(ib, count);
            IOFree(ib, 65536 * 4);
        }
    }
    for (uint32_t i = 0; i < pages; i++)
        table[i] = 0;
    IOLockUnlock(gartLock);
    dst->complete();
    return ok;
}

bool RadeonCard::accelSubmit(const uint32_t *words, uint32_t count)
{
    if (!accelReady || !count || count > PD_RADEON_MAX_SUBMIT_DW)
        return false;
    IOLockLock(lock);
    // Submits are synchronous: never overwrite words the command processor still owns.
    if ((rreg(CP_RB_RPTR) & (ringWords - 1)) != ringWrite) {
        IOLockUnlock(lock);
        return false;
    }
    uint32_t seq = rreg(SCRATCH_REG0) + 1;
    static const uint32_t ibOffset = 0x2300000;
    volatile uint32_t *ib = (volatile uint32_t *)(vramCpu + ibOffset);
    for (unsigned i = 0; i < count; i++)
        ib[i] = words[i];
    emit(this, PACKET3(0x32, 2));
    emit(this, ibOffset);
    emit(this, 0);
    emit(this, count);
    emit(this, PACKET3(0x43, 3)); // Invalidate shader, vertex and texture caches.
    emit(this, (1u << 20) | (1u << 23) | (1u << 24) | (1u << 27));
    emit(this, 0xffffffff);
    emit(this, 0);
    emit(this, 10);
    emit(this, PACKET3(0x46, 0)); // CACHE_FLUSH_AND_INV_EVENT
    emit(this, 0x16);
    emit(this, PACKET3(0x68, 1));
    emit(this, (0x8040 - 0x8000) >> 2); // WAIT_UNTIL
    emit(this, (1u << 15) | (1u << 17)); // 3D idle and all rendering writes complete.
    emit(this, PACKET3(0x68, 1));
    emit(this, (SCRATCH_REG0 - 0x8000) >> 2);
    emit(this, seq);
    commit(this);
    bool done = false;
    for (unsigned i = 0; i < 2000; i++) {
        if (rreg(SCRATCH_REG0) == seq && (rreg(CP_RB_RPTR) & (ringWords - 1)) == ringWrite) {
            done = true;
            break;
        }
        IODelay(1000);
    }
    if (!done) {
        LOG("CP submit timeout: scratch=0x%08x expected=0x%08x rptr=%u wptr=%u status=0x%08x\n",
            rreg(SCRATCH_REG0), seq, rreg(CP_RB_RPTR), ringWrite, rreg(0x8010));
        LOG("CP timeout state: GRBM2=0x%08x CP_STAT=0x%08x SRBM=0x%08x VM_L2=0x%08x\n",
            rreg(0x8014), rreg(0x8680), rreg(0xe50), rreg(VM_L2_STATUS));
        accelReady = false;
        wreg(CP_ME_CNTL, CP_ME_HALT | CP_PFP_HALT);
    }
    IOLockUnlock(lock);
    return done;
}

uint32_t RadeonCard::renderAlloc(uint32_t size, uint32_t alignment)
{
    uint32_t pages = size / 4096;
    if (!pages || pages > renderPageCount)
        return 0;
    if (alignment < 4096)
        alignment = 4096;
    IOLockLock(lock);
    uint32_t offset = 0;
    for (uint32_t i = 0; i <= renderPageCount - pages;) {
        uint32_t address = (PD_RADEON_HEAP_START + i * 4096 + alignment - 1) & ~(alignment - 1);
        i = (address - PD_RADEON_HEAP_START) / 4096;
        if (i > renderPageCount - pages)
            break;
        uint32_t j;
        for (j = 0; j < pages && !renderPages[i + j]; j++) {}
        if (j == pages) {
            memset(renderPages + i, 1, pages);
            offset = address;
            volatile uint32_t *p = (volatile uint32_t *)(vramCpu + offset);
            for (uint32_t n = 0; n < size / 4; n++)
                p[n] = 0;
            break;
        }
        i += j + 1;
    }
    IOLockUnlock(lock);
    return offset;
}

void RadeonCard::renderFree(uint32_t offset, uint32_t size)
{
    if (offset < PD_RADEON_HEAP_START || (offset & 4095) || (size & 4095))
        return;
    uint32_t first = (offset - PD_RADEON_HEAP_START) / 4096, count = size / 4096;
    if (first > renderPageCount || count > renderPageCount - first)
        return;
    IOLockLock(lock);
    memset(renderPages + first, 0, count);
    IOLockUnlock(lock);
}
