#ifndef _PD_IONVME_CONTROLLER_H
#define _PD_IONVME_CONTROLLER_H

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

#define kNVMEAssignedAddrKey "assigned-addresses"

class IONVMEDisk;

class IONVMEController : public IOService
{
    OSDeclareDefaultStructors(IONVMEController);
    friend class IONVMEDisk;

public:
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;

    IOReturn readWrite(bool write, UInt64 block, UInt64 nblks,
                       IOMemoryDescriptor *buffer, UInt64 bufferOffset);
    IOReturn flush(void);

    UInt64 blockCount(void) const { return fBlockCount; }
    UInt32 blockSize(void) const { return fBlockSize; }
    const char *modelString(void) const { return fModel; }
    const char *serialString(void) const { return fSerial; }
    const char *firmwareString(void) const { return fFirmware; }

private:
    struct NVMeCommand {
        UInt8  opc;
        UInt8  fuse;
        UInt16 cid;
        UInt32 nsid;
        UInt64 rsvd2;
        UInt64 mptr;
        UInt64 prp1;
        UInt64 prp2;
        UInt32 cdw10;
        UInt32 cdw11;
        UInt32 cdw12;
        UInt32 cdw13;
        UInt32 cdw14;
        UInt32 cdw15;
    } __attribute__((packed));

    struct NVMeCompletion {
        UInt32 result;
        UInt32 rsvd;
        UInt16 sqHead;
        UInt16 sqId;
        UInt16 cid;
        UInt16 status;
    } __attribute__((packed));

    IOPCIDevice *fProvider;
    IOService *fANSProvider;    // apple ans2: a platform nub instead of a pci function
    IOMemoryMap *fASCMap;       // its coprocessor's mailbox
    volatile UInt8 *fASC;
    IOBufferMemoryDescriptor *fAdminTCBMem;
    IOBufferMemoryDescriptor *fIOTCBMem;
    IOMemoryMap *fRegMap;
    IOMemoryDescriptor *fRegDesc;
    volatile UInt8 *fRegs;
    UInt32 fDoorbellStride;
    IOLock *fCommandLock;
    IONVMEDisk *fDisk;

    IOBufferMemoryDescriptor *fAdminSQMem;
    IOBufferMemoryDescriptor *fAdminCQMem;
    IOBufferMemoryDescriptor *fIOSQMem;
    IOBufferMemoryDescriptor *fIOCQMem;
    IOBufferMemoryDescriptor *fDMAMem;
    volatile NVMeCommand *fAdminSQ;
    volatile NVMeCompletion *fAdminCQ;
    volatile NVMeCommand *fIOSQ;
    volatile NVMeCompletion *fIOCQ;
    void *fDMABuffer;
    IOPhysicalAddress fDMAPhys;
    UInt16 fNextCID;
    UInt16 fAdminSQTail;
    UInt16 fAdminCQHead;
    UInt16 fIOSQTail;
    UInt16 fIOCQHead;
    bool fAdminCQPhase;
    bool fIOCQPhase;
    UInt32 fQueueDepth;
    UInt32 fNamespaceID;
    UInt64 fBlockCount;
    UInt32 fBlockSize;
    char fModel[41];
    char fSerial[21];
    char fFirmware[9];

    bool mapRegisters(void);
    bool mapANSRegisters(void);
    bool bootANS(void);
    bool ascSend(UInt64 msg, UInt8 ep);
    bool ascRecv(UInt64 *msg, UInt8 *ep, UInt32 timeoutMs);
    void ansSetTCB(bool admin, UInt16 tag, const NVMeCommand *cmd);
    bool allocQueues(void);
    void freeQueues(void);
    bool resetController(void);
    bool enableController(void);
    bool identifyController(void);
    bool identifyNamespace(UInt32 nsid);
    bool createIOQueues(void);

    IOReturn submitAdmin(NVMeCommand *cmd, UInt32 timeoutMs, UInt32 *resultOut = NULL);
    IOReturn submitIO(NVMeCommand *cmd, UInt32 timeoutMs);
    IOReturn submitReadWrite(bool write, UInt64 lba, UInt32 nblks, void *data, UInt32 bytes);

    UInt32 regRead32(UInt32 offset) const;
    UInt64 regRead64(UInt32 offset) const;
    void regWrite32(UInt32 offset, UInt32 value) const;
    void regWrite64(UInt32 offset, UInt64 value) const;
    void ringAdminDoorbell(void);
    void ringIODoorbell(void);

    static void trimCopy(char *dst, size_t dstLen, const UInt8 *src, size_t srcLen);
};

#endif
