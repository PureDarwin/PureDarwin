#include "PDAmdGPU.h"

#include <IOKit/IOLib.h>
#include <kern/clock.h>

// osssys 5.0 interrupt handler ring, segment 0, from the amdgpu headers (MIT)
#define mmIH_RB_CNTL            0x0080
#define mmIH_RB_BASE            0x0081
#define mmIH_RB_BASE_HI         0x0082
#define mmIH_RB_RPTR            0x0083
#define mmIH_RB_WPTR            0x0084
#define mmIH_RB_WPTR_ADDR_HI    0x0085
#define mmIH_RB_WPTR_ADDR_LO    0x0086
#define mmIH_DOORBELL_RPTR      0x0087

// nbio 7.2, segment 2
#define regBIF_BX0_INTERRUPT_CNTL   0x00f1
#define regBIF_BX0_INTERRUPT_CNTL2  0x00f2

#define kRbEnable           0x00000001
#define kRbGpuTsEnable      0x00000080
#define kWptrWriteback      0x00000100
#define kWptrOverflowEnable 0x00010000
#define kEnableIntr         0x00020000
#define kMcSnoop            0x00100000
#define kWptrOverflowClear  0x80000000
#define kRptrRearm          0x00200000

// carveout layout from ihBase: 4 KiB ring of 32-byte entries, then the wptr writeback
#define kIhRingSize     0x1000
#define kIhWptrOff      0x1000

static inline uint32_t ihField(uint32_t v, uint32_t mask, uint32_t shift, uint32_t field)
{
    return (v & ~mask) | ((field << shift) & mask);
}

// navi10_ih_irq_init for ring 0 in vram: nbio interrupt control, then the ring, then enable
void PDAmdGPU::ihInit(void)
{
    auto oss = [this](uint32_t off) { return soc15(kHwOSSSYS, 0, off); };
    ihBase = pspBase + 0x5000000;
    uint64_t ringMc = fbStart + ihBase;
    uint64_t wptrMc = ringMc + kIhWptrOff;
    uint64_t scratchPa = fbStart + pspBase + 0x3800000 - fbStart + fbOffset;

    writeVram(ihBase, 0, kIhRingSize + 0x100);
    flushHdp();

    // disable first, as navi10_ih_toggle_interrupts(false) does
    wreg(oss(mmIH_RB_CNTL), rreg(oss(mmIH_RB_CNTL)) & ~(kRbEnable | kEnableIntr));
    wreg(oss(mmIH_RB_RPTR), 0);
    wreg(oss(mmIH_RB_WPTR), 0);

    // nbio_v7_2_ih_control: dummy page, dummy reads follow msi, non-snooped requests since the ring is in vram
    wreg(soc15(kHwNBIF, 2, regBIF_BX0_INTERRUPT_CNTL2), (uint32_t)(scratchPa >> 8));
    uint32_t ic = rreg(soc15(kHwNBIF, 2, regBIF_BX0_INTERRUPT_CNTL));
    wreg(soc15(kHwNBIF, 2, regBIF_BX0_INTERRUPT_CNTL), (ic & ~0x1u) | 0x8);

    wreg(oss(mmIH_RB_BASE), (uint32_t)(ringMc >> 8));
    wreg(oss(mmIH_RB_BASE_HI), (uint32_t)(ringMc >> 40) & 0xff);

    // navi10_ih_rb_cntl: gpu address space, size in log2 dwords, wptr writeback, snooped
    uint32_t cntl = rreg(oss(mmIH_RB_CNTL));
    cntl = ihField(cntl, 0x70000000, 28, 4);
    cntl = ihField(cntl, 0x0000003e, 1, 10);
    cntl = ihField(cntl, 0x0f000000, 24, 0);
    cntl &= ~0x00400000u;
    cntl |= kWptrOverflowClear | kWptrOverflowEnable | kWptrWriteback | kMcSnoop;
    // with msi the ih re-raises when the rptr write leaves entries behind
    if (irqSource) cntl |= kRptrRearm;
    wreg(oss(mmIH_RB_CNTL), cntl);

    wreg(oss(mmIH_RB_WPTR_ADDR_LO), (uint32_t)wptrMc);
    wreg(oss(mmIH_RB_WPTR_ADDR_HI), (uint32_t)(wptrMc >> 32) & 0xffff);
    wreg(oss(mmIH_RB_WPTR), 0);
    wreg(oss(mmIH_RB_RPTR), 0);
    wreg(oss(mmIH_DOORBELL_RPTR), 0);
    ihRptr = 0;

    wreg(oss(mmIH_RB_CNTL), rreg(oss(mmIH_RB_CNTL)) | kRbEnable | kEnableIntr | kRbGpuTsEnable);
    LOG("ih: ring at mc 0x%llx, IH_RB_CNTL 0x%08x\n", ringMc, rreg(oss(mmIH_RB_CNTL)));
}

// reads every entry the hardware has written since the last call and logs its source
uint32_t PDAmdGPU::ihDrain(const char *why)
{
    auto oss = [this](uint32_t off) { return soc15(kHwOSSSYS, 0, off); };
    uint32_t wptr = 0, n = 0;

    readVram(ihBase + kIhWptrOff, &wptr, 4);
    uint32_t reg = rreg(oss(mmIH_RB_WPTR));
    wptr &= kIhRingSize - 1;
    while (ihRptr != wptr && n < 64) {
        uint32_t e[8];
        readVram(ihBase + ihRptr, e, sizeof(e));
        // vega10_ih decode: client id, source id, ring id and vmid share dword 0
        LOG("ih: %s entry %u client 0x%02x src %u ring 0x%02x vmid %u, data 0x%08x 0x%08x\n", why, n, e[0] & 0xff,
            (e[0] >> 8) & 0xff, (e[0] >> 16) & 0xff, (e[0] >> 24) & 0xf, e[4], e[5]);
        ihRptr = (ihRptr + 32) & (kIhRingSize - 1);
        n++;
    }
    wreg(oss(mmIH_RB_RPTR), ihRptr);
    LOG("ih: %s drained %u entries, wptr writeback 0x%x register 0x%08x\n", why, n, wptr, reg);
    return n;
}

void PDAmdGPU::ihStop(void)
{
    uint32_t r = soc15(kHwOSSSYS, 0, mmIH_RB_CNTL);
    wreg(r, rreg(r) & ~(kRbEnable | kEnableIntr));
}

// the msi vector of the pci function, on the kext's work loop
bool PDAmdGPU::irqInit(void)
{
    int index = -1;

    for (int i = 0; ; i++) {
        int type = 0;
        if (pci->getInterruptType(i, &type) != kIOReturnSuccess) break;
        if (type & kIOInterruptTypePCIMessaged) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        LOG("irq: no msi vector on the pci function\n");
        return false;
    }

    irqLock = IOLockAlloc();
    irqSource = IOFilterInterruptEventSource::filterInterruptEventSource(this, &PDAmdGPU::irqAction, &PDAmdGPU::irqFilter, pci, index);
    IOWorkLoop *wl = getWorkLoop();
    if (!irqLock || !irqSource || !wl || wl->addEventSource(irqSource) != kIOReturnSuccess) {
        LOG("irq: cannot attach the msi source\n");
        OSSafeReleaseNULL(irqSource);
        return false;
    }
    pci->setBusMasterEnable(true);
    irqSource->enable();
    LOG("irq: msi vector index %d attached\n", index);
    return true;
}

void PDAmdGPU::irqStop(void)
{
    if (!irqSource) return;
    irqSource->disable();
    if (getWorkLoop()) getWorkLoop()->removeEventSource(irqSource);
    OSSafeReleaseNULL(irqSource);
}

// primary interrupt context: count it and wake the waiter, the ring is drained on the waiting thread
bool PDAmdGPU::irqFilter(OSObject *owner, IOFilterInterruptEventSource *src)
{
    PDAmdGPU *self = (PDAmdGPU *)owner;

    OSIncrementAtomic(&self->irqCount);
    IOLockWakeup(self->irqLock, (void *)&self->irqCount, false);
    return false;
}

void PDAmdGPU::irqAction(OSObject *owner, IOInterruptEventSource *src, int count)
{
}

// sleeps until the interrupt count passes seen, or the timeout runs out
bool PDAmdGPU::irqWait(UInt32 seen, uint32_t ms)
{
    if (!irqLock) return false;

    AbsoluteTime deadline;
    clock_interval_to_deadline(ms, kMillisecondScale, &deadline);
    IOLockLock(irqLock);
    while (irqCount == seen) {
        if (IOLockSleepDeadline(irqLock, (void *)&irqCount, deadline, THREAD_UNINT) == THREAD_TIMED_OUT)
            break;
    }
    bool got = irqCount != seen;
    IOLockUnlock(irqLock);
    return got;
}
