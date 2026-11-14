#include "IONVMEController.h"
#include "IONVMEDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/storage/IOStorage.h>
#include <pexpert/pexpert.h>

#define super IOService
OSDefineMetaClassAndStructors(IONVMEController, IOService);

enum {
    NVME_REG_CAP    = 0x0000,
    NVME_REG_VS     = 0x0008,
    NVME_REG_INTMS  = 0x000c,
    NVME_REG_CC     = 0x0014,
    NVME_REG_CSTS   = 0x001c,
    NVME_REG_AQA    = 0x0024,
    NVME_REG_ASQ    = 0x0028,
    NVME_REG_ACQ    = 0x0030,
    NVME_REG_DB     = 0x1000,
};

enum {
    NVME_CC_EN      = 1u << 0,
    NVME_CSTS_RDY   = 1u << 0,
    NVME_CSTS_CFS   = 1u << 1,
    NVME_ADMIN_IDENTIFY   = 0x06,
    NVME_ADMIN_CREATE_IOSQ = 0x01,
    NVME_ADMIN_CREATE_IOCQ = 0x05,
    NVME_IO_WRITE = 0x01,
    NVME_IO_READ  = 0x02,
    NVME_SC_SUCCESS = 0,
};

static bool gNVMEDebug;

static void
NVMELog(const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);
    IOLog("IONVMEFamily: %s\n", buf);
}

static void
NVMEDebug(const char *fmt, ...)
{
    if (!gNVMEDebug) return;

    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);
    IOLog("IONVMEFamily: %s\n", buf);
}

static bool
readMemoryBAR(IOPCIDevice *pci, UInt8 reg, UInt64 *outBase, UInt64 *outSize)
{
    if (!pci || !outBase || !outSize) return false;

    UInt16 savedCmd = pci->configRead16(kIOPCIConfigCommand);
    UInt32 savedLo = pci->configRead32(reg);
    UInt32 savedHi = 0;

    if (savedLo & 1) return false;
    bool is64 = ((savedLo & 0x6) == 0x4);
    if (is64) {
        if (reg > kIOPCIConfigBaseAddress4) return false;
        savedHi = pci->configRead32(reg + 4);
    }

    pci->configWrite16(kIOPCIConfigCommand, savedCmd & ~(UInt16)0x3);
    pci->configWrite32(reg, 0xffffffffU);
    if (is64) pci->configWrite32(reg + 4, 0xffffffffU);

    UInt32 maskLo = pci->configRead32(reg);
    UInt32 maskHi = is64 ? pci->configRead32(reg + 4) : 0xffffffffU;

    pci->configWrite32(reg, savedLo);
    if (is64) pci->configWrite32(reg + 4, savedHi);
    pci->configWrite16(kIOPCIConfigCommand, savedCmd);

    UInt64 base = savedLo & ~0x0fULL;
    UInt64 sizeMask = maskLo & ~0x0fULL;
    if (is64) {
        base |= (UInt64)savedHi << 32;
        sizeMask |= (UInt64)maskHi << 32;
    }
    if (!base || !sizeMask) return false;

    UInt64 size = (~sizeMask) + 1;
    if (!is64) size &= 0xffffffffULL;
    if (size < 0x4000) size = 0x4000;
    if (size > 0x1000000ULL) return false;

    *outBase = base;
    *outSize = size;
    return true;
}

static bool
mapBARFromAssignedAddresses(IOPCIDevice *pci, UInt8 reg,
                            IOMemoryDescriptor **outDesc,
                            IOMemoryMap **outMap)
{
    OSData *assigned;
    const UInt8 *bytes;
    UInt32 len;

    if (!pci || !outDesc || !outMap) return false;
    assigned = OSDynamicCast(OSData, pci->copyProperty(kNVMEAssignedAddrKey));
    if (!assigned) return false;

    bytes = (const UInt8 *)assigned->getBytesNoCopy();
    len = (UInt32)assigned->getLength();
    for (UInt32 off = 0; off + sizeof(IOPCIPhysicalAddress) <= len;
         off += sizeof(IOPCIPhysicalAddress)) {
        const IOPCIPhysicalAddress *a = (const IOPCIPhysicalAddress *)(bytes + off);
        if (a->physHi.s.registerNum != reg || a->physHi.s.space == 1)
            continue;

        UInt64 base = ((UInt64)a->physMid << 32) | a->physLo;
        UInt64 size = ((UInt64)a->lengthHi << 32) | a->lengthLo;
        if (!base || !size) continue;
        if (size < 0x4000) size = 0x4000;

        IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
            (IOPhysicalAddress)base, (IOByteCount)size,
            kIODirectionNone | kIOMemoryMapperNone);
        if (!desc) continue;

        IOMemoryMap *map = desc->map(kIOMapAnywhere);
        if (!map) {
            desc->release();
            continue;
        }

        *outDesc = desc;
        *outMap = map;
        assigned->release();
        return true;
    }

    assigned->release();
    return false;
}

/*
 * Apple ANS2 (t8103): NVMe behind an RTKit coprocessor. The coprocessor is woken
 * through its mailbox, then the controller runs linear submission queues: a command
 * sits in the slot named by its tag, the doorbell takes the tag, and an NVMMU TCB per
 * slot repeats the command's DMA. After AsahiLinux's nvme-apple and m1n1's nvme.c.
 */
#define ANS_ASC_CPU_CTRL        0x44
#define ANS_ASC_CPU_RUN         (1U << 4)
#define ANS_MBOX_A2I_SEND0      0x8800
#define ANS_MBOX_A2I_SEND1      0x8808
#define ANS_MBOX_I2A_CTRL       0x8114
#define ANS_MBOX_I2A_RECV0      0x8830
#define ANS_MBOX_I2A_RECV1      0x8838
#define ANS_MBOX_EMPTY          (1U << 17)
#define ANS_BOOT_STATUS         0x1300
#define ANS_BOOT_STATUS_OK      0xde71ce55U
#define ANS_MAX_PEND_CMDS       0x1210
#define ANS_LINEAR_SQ_CTRL      0x24908
#define ANS_LINEAR_ASQ_DB       0x2490c
#define ANS_LINEAR_IOSQ_DB      0x24910
#define ANS_NVMMU_NUM_TCBS      0x28100
#define ANS_NVMMU_ASQ_TCB_BASE  0x28108
#define ANS_NVMMU_IOSQ_TCB_BASE 0x28110
#define ANS_NVMMU_TCB_INVAL     0x28118
#define ANS_TCB_SIZE            128
#define RTK_MGMT(t)             ((UInt64)(t) << 52)
#define RTK_MGMT_TYPE(m)        (((m) >> 52) & 0xff)

static bool
ansSocPhys(UInt64 *socPhys)
{
    IORegistryEntry *armio = IORegistryEntry::fromPath("/arm-io", gIODTPlane);
    OSData *ranges = armio ? OSDynamicCast(OSData, armio->getProperty("ranges")) : NULL;
    bool ok = ranges != NULL && ranges->getLength() >= 16;
    if (ok) *socPhys = ((const UInt64 *)ranges->getBytesNoCopy())[1];
    OSSafeReleaseNULL(armio);
    return ok;
}

static bool
ansIsProvider(IOService *provider)
{
    OSData *type = OSDynamicCast(OSData, provider->getProperty("device_type"));
    return type != NULL && strncmp((const char *)type->getBytesNoCopy(), "ans",
                                   type->getLength()) == 0;
}

IOService *
IONVMEController::probe(IOService *provider, SInt32 *score)
{
    if (ansIsProvider(provider)) {
        if (score) *score = 4500;
        return super::probe(provider, score);
    }
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci) return NULL;

    UInt32 classCode = pci->configRead32(kIOPCIConfigRevisionID) >> 8;
    NVMELog("probe vendor=%04x device=%04x class=%06x",
            pci->configRead16(kIOPCIConfigVendorID),
            pci->configRead16(kIOPCIConfigDeviceID), classCode);

    if ((classCode & 0xffff00) != 0x010800)
        return NULL;

    if (score) *score = 4500;
    return super::probe(provider, score);
}

bool
IONVMEController::start(IOService *provider)
{
    PE_parse_boot_argn("nvme_debug", &gNVMEDebug, sizeof(gNVMEDebug));
    if (!super::start(provider)) return false;

    fProvider = OSDynamicCast(IOPCIDevice, provider);
    if (fProvider) {
        fProvider->retain();
        fProvider->setMemoryEnable(true);
        fProvider->setBusMasterEnable(true);
    } else if (ansIsProvider(provider)) {
        fANSProvider = provider;
        fANSProvider->retain();
    } else {
        return false;
    }

    fQueueDepth = 16;
    fNamespaceID = 1;
    fBlockSize = 512;
    fBlockCount = 0;
    fNextCID = 1;
    fCommandLock = IOLockAlloc();
    if (!fCommandLock) return false;

    if (!(fANSProvider ? mapANSRegisters() && bootANS() : mapRegisters()) ||
        !allocQueues() || !resetController() ||
        !enableController() || !identifyController() ||
        !createIOQueues() || !identifyNamespace(fNamespaceID)) {
        NVMELog("controller bring-up failed");
        return false;
    }

    IONVMEDisk *disk = new IONVMEDisk();
    if (!disk || !disk->initWithController(this) || !disk->attach(this) ||
        !disk->start(this)) {
        NVMELog("disk nub creation failed");
        if (disk) disk->release();
        return false;
    }

    fDisk = disk;
    disk->registerService();
    NVMELog("published nvme0 nsid=%u blocks=%llu blockSize=%u model='%s'",
            fNamespaceID, fBlockCount, fBlockSize, fModel);
    return true;
}

void
IONVMEController::stop(IOService *provider)
{
    if (fDisk) {
        fDisk->stop(this);
        fDisk->detach(this);
        fDisk->release();
        fDisk = NULL;
    }
    if (fRegs) {
        regWrite32(NVME_REG_CC, regRead32(NVME_REG_CC) & ~NVME_CC_EN);
    }
    if (fRegMap) {
        fRegMap->release();
        fRegMap = NULL;
        fRegs = NULL;
    }
    if (fRegDesc) {
        fRegDesc->release();
        fRegDesc = NULL;
    }
    freeQueues();
    if (fCommandLock) {
        IOLockFree(fCommandLock);
        fCommandLock = NULL;
    }
    if (fProvider) {
        fProvider->release();
        fProvider = NULL;
    }
    if (fASCMap) {
        fASCMap->release();
        fASCMap = NULL;
        fASC = NULL;
    }
    if (fANSProvider) {
        fANSProvider->release();
        fANSProvider = NULL;
    }
    super::stop(provider);
}

void
IONVMEController::free()
{
    fProvider = NULL;
    fRegMap = NULL;
    fRegDesc = NULL;
    fRegs = NULL;
    fCommandLock = NULL;
    fDisk = NULL;
    fAdminSQMem = fAdminCQMem = fIOSQMem = fIOCQMem = fDMAMem = NULL;
    fAdminTCBMem = fIOTCBMem = NULL;
    fANSProvider = NULL;
    fASCMap = NULL;
    fASC = NULL;
    super::free();
}

bool
IONVMEController::mapRegisters(void)
{
    IODeviceMemory *range = fProvider->getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    if (range && range->getLength())
        fRegMap = range->map(kIOMapAnywhere);

    if (!fRegMap)
        mapBARFromAssignedAddresses(fProvider, kIOPCIConfigBaseAddress0, &fRegDesc, &fRegMap);

    if (!fRegMap) {
        UInt64 base = 0, size = 0;
        if (readMemoryBAR(fProvider, kIOPCIConfigBaseAddress0, &base, &size)) {
            fRegDesc = IOMemoryDescriptor::withPhysicalAddress(
                (IOPhysicalAddress)base, (IOByteCount)size,
                kIODirectionNone | kIOMemoryMapperNone);
            if (fRegDesc)
                fRegMap = fRegDesc->map(kIOMapAnywhere);
        }
    }

    if (!fRegMap) {
        NVMELog("failed to map BAR0");
        return false;
    }

    fRegs = (volatile UInt8 *)fRegMap->getVirtualAddress();
    UInt64 cap = regRead64(NVME_REG_CAP);
    UInt32 dstrd = (UInt32)((cap >> 32) & 0xf);
    fDoorbellStride = 4U << dstrd;
    NVMELog("CAP=%016llx VS=%08x doorbellStride=%u",
            cap, regRead32(NVME_REG_VS), fDoorbellStride);
    return true;
}

// reg[0] is the coprocessor (its mailbox at +0x8000), reg[3] the nvme block; both are
// offsets into the arm-io range
bool
IONVMEController::mapANSRegisters(void)
{
    OSData *reg = OSDynamicCast(OSData, fANSProvider->getProperty("reg"));
    UInt64 soc = 0;
    if (reg == NULL || reg->getLength() < 4 * 16 || !ansSocPhys(&soc)) {
        NVMELog("ans: no usable reg / arm-io ranges");
        return false;
    }
    const UInt64 *r = (const UInt64 *)reg->getBytesNoCopy();
    IOMemoryDescriptor *asc = IOMemoryDescriptor::withPhysicalAddress(
        (IOPhysicalAddress)(soc + r[0]), 0x10000, kIODirectionNone | kIOMemoryMapperNone);
    fRegDesc = IOMemoryDescriptor::withPhysicalAddress(
        (IOPhysicalAddress)(soc + r[6]), (IOByteCount)r[7], kIODirectionNone | kIOMemoryMapperNone);
    if (asc) fASCMap = asc->map(kIOMapAnywhere | kIOMapInhibitCache);
    if (fRegDesc) fRegMap = fRegDesc->map(kIOMapAnywhere | kIOMapInhibitCache);
    OSSafeReleaseNULL(asc);
    if (!fASCMap || !fRegMap) {
        NVMELog("ans: failed to map registers");
        return false;
    }
    fASC = (volatile UInt8 *)fASCMap->getVirtualAddress();
    fRegs = (volatile UInt8 *)fRegMap->getVirtualAddress();
    fDoorbellStride = 4;
    NVMELog("ans: nvme at 0x%llx, coprocessor at 0x%llx", soc + r[6], soc + r[0]);
    return true;
}

bool
IONVMEController::ascSend(UInt64 msg, UInt8 ep)
{
    *(volatile UInt64 *)(fASC + ANS_MBOX_A2I_SEND0) = msg;
    *(volatile UInt64 *)(fASC + ANS_MBOX_A2I_SEND1) = ep;
    return true;
}

bool
IONVMEController::ascRecv(UInt64 *msg, UInt8 *ep, UInt32 timeoutMs)
{
    for (UInt32 i = 0; i <= timeoutMs; i++) {
        if (!(*(volatile UInt32 *)(fASC + ANS_MBOX_I2A_CTRL) & ANS_MBOX_EMPTY)) {
            *msg = *(volatile UInt64 *)(fASC + ANS_MBOX_I2A_RECV0);
            *ep = (UInt8)*(volatile UInt64 *)(fASC + ANS_MBOX_I2A_RECV1);
            return true;
        }
        IOSleep(1);
    }
    return false;
}

// rtkit: power the iop up, answer hello and the endpoint map, wait for power on
bool
IONVMEController::bootANS(void)
{
    UInt64 m;
    UInt8 ep;

    *(volatile UInt32 *)(fASC + ANS_ASC_CPU_CTRL) |= ANS_ASC_CPU_RUN;
    ascSend(RTK_MGMT(6) | 0x220, 0);
    if (!ascRecv(&m, &ep, 1000) || RTK_MGMT_TYPE(m) != 1) {
        NVMELog("ans: no rtkit hello");
        return false;
    }
    UInt64 ver = (m >> 16) & 0xffff;
    if (ver > 12) ver = 12;
    ascSend(RTK_MGMT(2) | (ver << 16) | ver, 0);
    for (UInt32 n = 0; n < 64; n++) {
        if (!ascRecv(&m, &ep, 1000)) {
            NVMELog("ans: rtkit stalled during boot");
            return false;
        }
        if (ep != 0) continue;
        if (RTK_MGMT_TYPE(m) == 8) {           // endpoint map: acknowledge each page
            bool last = (m >> 51) & 1;
            ascSend(RTK_MGMT(8) | (m & (7ULL << 32)) | (last ? (1ULL << 51) : 1), 0);
        } else if (RTK_MGMT_TYPE(m) == 7 && (m & 0xffff) == 0x20) {
            break;                              // iop power on
        }
    }
    for (UInt32 i = 0; i < 1000; i++) {
        if (regRead32(ANS_BOOT_STATUS) == ANS_BOOT_STATUS_OK) {
            NVMELog("ans: coprocessor booted");
            return true;
        }
        IOSleep(1);
    }
    NVMELog("ans: boot status 0x%x", regRead32(ANS_BOOT_STATUS));
    return false;
}

void
IONVMEController::ansSetTCB(bool admin, UInt16 tag, const NVMeCommand *cmd)
{
    IOBufferMemoryDescriptor *mem = admin ? fAdminTCBMem : fIOTCBMem;
    UInt8 *t = (UInt8 *)mem->getBytesNoCopy() + tag * ANS_TCB_SIZE;
    bzero(t, ANS_TCB_SIZE);
    t[1] = cmd->prp1 == 0 ? 0 : (cmd->opc & 1) ? 2 : 1;   // dma to / from the device
    t[2] = (UInt8)tag;
    *(UInt32 *)(t + 4) = cmd->cdw12;
    *(UInt64 *)(t + 24) = cmd->prp1;
    *(UInt64 *)(t + 32) = cmd->prp2;
}

bool
IONVMEController::allocQueues(void)
{
    UInt64 mask = 0xfffffffffffff000ULL;
    UInt32 queueBytes = fQueueDepth * sizeof(NVMeCommand);
    UInt32 cqBytes = fQueueDepth * sizeof(NVMeCompletion);

    fAdminSQMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
        queueBytes, mask);
    fAdminCQMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
        cqBytes, mask);
    fIOSQMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
        queueBytes, mask);
    fIOCQMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
        cqBytes, mask);
    fDMAMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
        4096, mask);
    if (!fAdminSQMem || !fAdminCQMem || !fIOSQMem || !fIOCQMem || !fDMAMem)
        return false;

    fAdminSQ = (volatile NVMeCommand *)fAdminSQMem->getBytesNoCopy();
    fAdminCQ = (volatile NVMeCompletion *)fAdminCQMem->getBytesNoCopy();
    fIOSQ = (volatile NVMeCommand *)fIOSQMem->getBytesNoCopy();
    fIOCQ = (volatile NVMeCompletion *)fIOCQMem->getBytesNoCopy();
    fDMABuffer = fDMAMem->getBytesNoCopy();
    fDMAPhys = fDMAMem->getPhysicalAddress();
    bzero((void *)fAdminSQ, queueBytes);
    bzero((void *)fAdminCQ, cqBytes);
    bzero((void *)fIOSQ, queueBytes);
    bzero((void *)fIOCQ, cqBytes);
    bzero(fDMABuffer, 4096);
    fAdminCQPhase = true;
    fIOCQPhase = true;
    if (fANSProvider) {
        fAdminTCBMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
            fQueueDepth * ANS_TCB_SIZE, mask);
        fIOTCBMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
            fQueueDepth * ANS_TCB_SIZE, mask);
        if (!fAdminTCBMem || !fIOTCBMem) return false;
        bzero(fAdminTCBMem->getBytesNoCopy(), fQueueDepth * ANS_TCB_SIZE);
        bzero(fIOTCBMem->getBytesNoCopy(), fQueueDepth * ANS_TCB_SIZE);
    }
    return true;
}

void
IONVMEController::freeQueues(void)
{
    if (fAdminSQMem) { fAdminSQMem->release(); fAdminSQMem = NULL; }
    if (fAdminCQMem) { fAdminCQMem->release(); fAdminCQMem = NULL; }
    if (fIOSQMem) { fIOSQMem->release(); fIOSQMem = NULL; }
    if (fIOCQMem) { fIOCQMem->release(); fIOCQMem = NULL; }
    if (fDMAMem) { fDMAMem->release(); fDMAMem = NULL; }
    if (fAdminTCBMem) { fAdminTCBMem->release(); fAdminTCBMem = NULL; }
    if (fIOTCBMem) { fIOTCBMem->release(); fIOTCBMem = NULL; }
    fAdminSQ = NULL; fAdminCQ = NULL; fIOSQ = NULL; fIOCQ = NULL;
    fDMABuffer = NULL; fDMAPhys = 0;
}

bool
IONVMEController::resetController(void)
{
    regWrite32(NVME_REG_INTMS, 0xffffffffU);
    UInt32 cc = regRead32(NVME_REG_CC);
    if (cc & NVME_CC_EN) {
        regWrite32(NVME_REG_CC, cc & ~NVME_CC_EN);
        for (UInt32 i = 0; i < 5000; i++) {
            if ((regRead32(NVME_REG_CSTS) & NVME_CSTS_RDY) == 0)
                break;
            IOSleep(1);
        }
    }
    return (regRead32(NVME_REG_CSTS) & NVME_CSTS_RDY) == 0;
}

bool
IONVMEController::enableController(void)
{
    if (fANSProvider) {
        regWrite32(ANS_LINEAR_SQ_CTRL, 1);
        regWrite32(ANS_MAX_PEND_CMDS, fQueueDepth | (fQueueDepth << 16));
        regWrite32(ANS_NVMMU_NUM_TCBS, fQueueDepth - 1);
        regWrite64(ANS_NVMMU_ASQ_TCB_BASE, fAdminTCBMem->getPhysicalAddress());
        regWrite64(ANS_NVMMU_IOSQ_TCB_BASE, fIOTCBMem->getPhysicalAddress());
    }
    regWrite32(NVME_REG_AQA, (fQueueDepth - 1) | ((fQueueDepth - 1) << 16));
    regWrite64(NVME_REG_ASQ, fAdminSQMem->getPhysicalAddress());
    regWrite64(NVME_REG_ACQ, fAdminCQMem->getPhysicalAddress());

    UInt32 cc = (6U << 16) | (4U << 20) | NVME_CC_EN;
    regWrite32(NVME_REG_CC, cc);
    for (UInt32 i = 0; i < 5000; i++) {
        UInt32 csts = regRead32(NVME_REG_CSTS);
        if (csts & NVME_CSTS_CFS) return false;
        if (csts & NVME_CSTS_RDY) return true;
        IOSleep(1);
    }
    return false;
}

bool
IONVMEController::identifyController(void)
{
    NVMeCommand cmd;
    bzero(&cmd, sizeof(cmd));
    bzero(fDMABuffer, 4096);
    cmd.opc = NVME_ADMIN_IDENTIFY;
    cmd.prp1 = fDMAPhys;
    cmd.cdw10 = 1; /* CNS=controller */
    if (submitAdmin(&cmd, 1000) != kIOReturnSuccess)
        return false;

    const UInt8 *id = (const UInt8 *)fDMABuffer;
    trimCopy(fSerial, sizeof(fSerial), id + 4, 20);
    trimCopy(fModel, sizeof(fModel), id + 24, 40);
    trimCopy(fFirmware, sizeof(fFirmware), id + 64, 8);
    NVMELog("controller serial='%s' model='%s' fw='%s'",
            fSerial, fModel, fFirmware);
    return true;
}

bool
IONVMEController::identifyNamespace(UInt32 nsid)
{
    NVMeCommand cmd;
    bzero(&cmd, sizeof(cmd));
    bzero(fDMABuffer, 4096);
    cmd.opc = NVME_ADMIN_IDENTIFY;
    cmd.nsid = nsid;
    cmd.prp1 = fDMAPhys;
    cmd.cdw10 = 0; /* CNS=namespace */
    if (submitAdmin(&cmd, 1000) != kIOReturnSuccess)
        return false;

    const UInt8 *id = (const UInt8 *)fDMABuffer;
    UInt8 flbas = id[26] & 0xf;
    const UInt8 *lbaf = id + 128 + flbas * 4;
    UInt8 lbads = lbaf[2];
    UInt64 nsze = 0;
    bcopy(id, &nsze, sizeof(nsze));
    if (lbads < 9 || lbads > 16 || nsze == 0) {
        NVMELog("invalid namespace identify nsze=%llu flbas=%u lbads=%u",
                nsze, flbas, lbads);
        return false;
    }
    fNamespaceID = nsid;
    fBlockCount = nsze;
    fBlockSize = 1U << lbads;
    return true;
}

bool
IONVMEController::createIOQueues(void)
{
    NVMeCommand cmd;

    bzero(&cmd, sizeof(cmd));
    cmd.opc = NVME_ADMIN_CREATE_IOCQ;
    cmd.prp1 = fIOCQMem->getPhysicalAddress();
    cmd.cdw10 = 1 | ((fQueueDepth - 1) << 16);
    cmd.cdw11 = 1; /* physically contiguous, interrupts disabled */
    if (submitAdmin(&cmd, 1000) != kIOReturnSuccess)
        return false;

    bzero(&cmd, sizeof(cmd));
    cmd.opc = NVME_ADMIN_CREATE_IOSQ;
    cmd.prp1 = fIOSQMem->getPhysicalAddress();
    cmd.cdw10 = 1 | ((fQueueDepth - 1) << 16);
    cmd.cdw11 = 1 | (1 << 16); /* physically contiguous, CQID=1 */
    if (submitAdmin(&cmd, 1000) != kIOReturnSuccess)
        return false;

    return true;
}

IOReturn
IONVMEController::submitAdmin(NVMeCommand *cmd, UInt32 timeoutMs, UInt32 *resultOut)
{
    // ans completes by tag, the slot the command sits in
    UInt16 tag = fAdminSQTail;
    UInt16 cid = fANSProvider ? tag : fNextCID++;
    cmd->cid = cid;
    bcopy(cmd, (void *)&fAdminSQ[fAdminSQTail], sizeof(*cmd));
    fAdminSQTail = (fAdminSQTail + 1) % fQueueDepth;
    if (fANSProvider) {
        ansSetTCB(true, tag, cmd);
        OSSynchronizeIO();
        regWrite32(ANS_LINEAR_ASQ_DB, tag);
    } else {
        ringAdminDoorbell();
    }

    for (UInt32 i = 0; i < timeoutMs; i++) {
        volatile NVMeCompletion *cqe = &fAdminCQ[fAdminCQHead];
        bool phase = (cqe->status & 1) != 0;
        if (phase == fAdminCQPhase) {
            UInt16 status = (cqe->status >> 1) & 0x7fff;
            if (resultOut) *resultOut = cqe->result;
            fAdminCQHead = (fAdminCQHead + 1) % fQueueDepth;
            if (fAdminCQHead == 0) fAdminCQPhase = !fAdminCQPhase;
            if (fANSProvider) regWrite32(ANS_NVMMU_TCB_INVAL, cqe->cid);
            regWrite32(NVME_REG_DB + fDoorbellStride, fAdminCQHead);
            if (cqe->cid != cid)
                NVMELog("admin completion cid mismatch got=%u want=%u", cqe->cid, cid);
            if (status == NVME_SC_SUCCESS) return kIOReturnSuccess;
            NVMELog("admin opcode=0x%x status=0x%x", cmd->opc, status);
            return kIOReturnError;
        }
        IOSleep(1);
    }
    NVMELog("admin opcode=0x%x timed out", cmd->opc);
    return kIOReturnTimeout;
}

IOReturn
IONVMEController::submitIO(NVMeCommand *cmd, UInt32 timeoutMs)
{
    UInt16 tag = fIOSQTail;
    UInt16 cid = fANSProvider ? tag : fNextCID++;
    cmd->cid = cid;
    bcopy(cmd, (void *)&fIOSQ[fIOSQTail], sizeof(*cmd));
    fIOSQTail = (fIOSQTail + 1) % fQueueDepth;
    if (fANSProvider) {
        ansSetTCB(false, tag, cmd);
        OSSynchronizeIO();
        regWrite32(ANS_LINEAR_IOSQ_DB, tag);
    } else {
        ringIODoorbell();
    }

    for (UInt32 i = 0; i < timeoutMs; i++) {
        volatile NVMeCompletion *cqe = &fIOCQ[fIOCQHead];
        bool phase = (cqe->status & 1) != 0;
        if (phase == fIOCQPhase) {
            UInt16 status = (cqe->status >> 1) & 0x7fff;
            fIOCQHead = (fIOCQHead + 1) % fQueueDepth;
            if (fIOCQHead == 0) fIOCQPhase = !fIOCQPhase;
            if (fANSProvider) regWrite32(ANS_NVMMU_TCB_INVAL, cqe->cid);
            regWrite32(NVME_REG_DB + (3 * fDoorbellStride), fIOCQHead);
            if (cqe->cid != cid)
                NVMELog("io completion cid mismatch got=%u want=%u", cqe->cid, cid);
            if (status == NVME_SC_SUCCESS) return kIOReturnSuccess;
            NVMELog("io opcode=0x%x status=0x%x", cmd->opc, status);
            return kIOReturnError;
        }
        IOSleep(1);
    }
    NVMELog("io opcode=0x%x timed out", cmd->opc);
    return kIOReturnTimeout;
}

IOReturn
IONVMEController::submitReadWrite(bool write, UInt64 lba, UInt32 nblks,
                                  void *data, UInt32 bytes)
{
    NVMeCommand cmd;
    if (!data || bytes == 0 || bytes > 4096 || nblks == 0)
        return kIOReturnBadArgument;

    bcopy(data, fDMABuffer, bytes);
    bzero(&cmd, sizeof(cmd));
    cmd.opc = write ? NVME_IO_WRITE : NVME_IO_READ;
    cmd.nsid = fNamespaceID;
    cmd.prp1 = fDMAPhys;
    cmd.cdw10 = (UInt32)(lba & 0xffffffffU);
    cmd.cdw11 = (UInt32)(lba >> 32);
    cmd.cdw12 = nblks - 1;

    IOReturn ret = submitIO(&cmd, 5000);
    if (ret == kIOReturnSuccess && !write)
        bcopy(fDMABuffer, data, bytes);
    return ret;
}

IOReturn
IONVMEController::readWrite(bool write, UInt64 block, UInt64 nblks,
                            IOMemoryDescriptor *buffer, UInt64 bufferOffset)
{
    if (!buffer || nblks == 0 || fBlockSize == 0)
        return kIOReturnBadArgument;
    if (block >= fBlockCount || nblks > (fBlockCount - block))
        return kIOReturnBadArgument;

    IOLockLock(fCommandLock);
    UInt32 blocksPerCmd = 4096 / fBlockSize;
    if (blocksPerCmd == 0) blocksPerCmd = 1;

    UInt64 done = 0;
    IOReturn ret = kIOReturnSuccess;
    while (done < nblks) {
        UInt32 cur = (UInt32)((nblks - done) > blocksPerCmd ? blocksPerCmd : (nblks - done));
        UInt32 bytes = cur * fBlockSize;
        if (write) {
            IOByteCount got = buffer->readBytes(bufferOffset + done * fBlockSize,
                                                fDMABuffer, bytes);
            if (got != bytes) { ret = kIOReturnIOError; break; }
        } else {
            bzero(fDMABuffer, bytes);
        }

        ret = submitReadWrite(write, block + done, cur, fDMABuffer, bytes);
        if (ret != kIOReturnSuccess) break;

        if (!write) {
            IOByteCount put = buffer->writeBytes(bufferOffset + done * fBlockSize,
                                                 fDMABuffer, bytes);
            if (put != bytes) { ret = kIOReturnIOError; break; }
        }
        done += cur;
    }
    IOLockUnlock(fCommandLock);
    return ret;
}

IOReturn
IONVMEController::flush(void)
{
    NVMeCommand cmd;
    bzero(&cmd, sizeof(cmd));
    cmd.opc = 0x00; /* FLUSH */
    cmd.nsid = fNamespaceID;
    IOLockLock(fCommandLock);
    IOReturn ret = submitIO(&cmd, 5000);
    IOLockUnlock(fCommandLock);
    return ret;
}

UInt32 IONVMEController::regRead32(UInt32 offset) const
{
    return *(volatile UInt32 *)(fRegs + offset);
}

UInt64 IONVMEController::regRead64(UInt32 offset) const
{
    UInt64 lo = regRead32(offset);
    UInt64 hi = regRead32(offset + 4);
    return lo | (hi << 32);
}

void IONVMEController::regWrite32(UInt32 offset, UInt32 value) const
{
    *(volatile UInt32 *)(fRegs + offset) = value;
}

void IONVMEController::regWrite64(UInt32 offset, UInt64 value) const
{
    regWrite32(offset, (UInt32)(value & 0xffffffffU));
    regWrite32(offset + 4, (UInt32)(value >> 32));
}

void IONVMEController::ringAdminDoorbell(void)
{
    regWrite32(NVME_REG_DB, fAdminSQTail);
}

void IONVMEController::ringIODoorbell(void)
{
    regWrite32(NVME_REG_DB + (2 * fDoorbellStride), fIOSQTail);
}

void
IONVMEController::trimCopy(char *dst, size_t dstLen, const UInt8 *src, size_t srcLen)
{
    if (!dst || dstLen == 0) return;
    size_t n = srcLen;
    if (n >= dstLen) n = dstLen - 1;
    bcopy(src, dst, n);
    dst[n] = '\0';
    while (n > 0 && dst[n - 1] == ' ') {
        dst[n - 1] = '\0';
        n--;
    }
}
