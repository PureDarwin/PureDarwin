#ifndef _PUREDARWIN_PDSUN50IMMC_H
#define _PUREDARWIN_PDSUN50IMMC_H

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <kern/thread.h>

// allwinner h616/h618 and a733 smhc0, the micro sd slot on the orange pi zero 3 and zero 4
// polled pio through the fifo, sd cards only. writes go through a queue that a worker sorts, merges
// and writes in large commands; a synchronize waits for it to drain

class PDSun50iMMCDisk;
struct PDSunxiMMCSoC;

class PDSun50iMMC : public IOService
{
	OSDeclareDefaultStructors(PDSun50iMMC)

public:
	bool     start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void     stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	void     free(void) APPLE_KEXT_OVERRIDE;
	IOReturn message(UInt32 type, IOService *provider, void *argument = NULL) APPLE_KEXT_OVERRIDE;

	// backing for the block storage nub
	IOReturn readWrite(bool write, UInt64 block, UInt64 nblks,
	    IOMemoryDescriptor *buffer);
	IOReturn queueWrite(UInt64 block, UInt64 nblks, IOMemoryDescriptor *buffer);
	IOReturn flushWrites(void);
	UInt64   blockCount(void) const { return fBlockCount; }
	UInt32   blockSize(void) const  { return 512; }
	bool     isReadOnly(void) const { return fReadOnly; }

private:
	IOMemoryMap      *fRegMap;
	IOMemoryMap      *fCCUMap;
	IOMemoryMap      *fPIOMap;
	volatile uint8_t *fRegs;
	volatile uint8_t *fCCU;
	volatile uint8_t *fPIO;
	const PDSunxiMMCSoC *fSoC;
	bool fFastOK;
	bool fHighSpeed;            // the card runs at 50 mhz
	IOLock           *fLock;
	uint32_t         *fBounce;
	PDSun50iMMCDisk  *fDisk;

	// the write queue, under fWQLock, while card access is under fLock, taken first
	struct PendingWrite {
		PendingWrite *next;
		uint64_t      block;
		uint32_t      nblks;
		bool          done;     // on the card, kept until its batch is freed
		uint8_t      *data;
	};
	IOLock       *fWQLock;
	PendingWrite *fWQ, *fWQTail;   // waiting for the worker
	PendingWrite *fWQFlight;       // the worker's batch, sorted
	uint32_t      fWQBytes;        // queued and in flight
	bool          fWQBusy, fWQFlush, fWQStop, fWriteBack;
	IOReturn      fWQError;        // a queued write that failed, for the next synchronize
	volatile SInt32 fReadWaiters;  // reads waiting for the card
	uint64_t      fStatCmds[2], fStatBlocks[2], fStatUs[2], fStatSyncs, fStatSyncUs, fStatLast;

	uint32_t fRCA;              // card address, already shifted for the arg
	uint64_t fBlockCount;
	bool     fBlockAddressed;   // sdhc and sdxc address in blocks
	bool     fReadOnly;
	bool     fQuiesced;
	IONotifier *fRestartNotifier;

	void     quiesce(const char *why);
	static IOReturn restartHandler(void *target, void *refCon, UInt32 messageType,
	    IOService *provider, void *messageArgument, vm_size_t argSize);

	uint32_t rd(uint32_t off) const
	{
		return *(volatile uint32_t *)(fRegs + off);
	}
	void wr(uint32_t off, uint32_t val) const
	{
		*(volatile uint32_t *)(fRegs + off) = val;
		__asm__ volatile ("dsb sy" ::: "memory");
	}

	bool     bringUpController(void);
	bool     updateClock(void);
	bool     setClock(uint32_t hz);
	bool     enableFastPll(void);
	bool     waitRint(uint32_t done, uint32_t timeoutMs);
	bool     command(uint32_t idx, uint32_t arg, uint32_t flags,
	    uint32_t *resp, uint32_t nblks = 0, uint32_t blksz = 512);
	bool     appCommand(uint32_t idx, uint32_t arg, uint32_t flags,
	    uint32_t *resp);
	bool     pio(bool write, uint32_t words);
	void     recover(bool abortData);
	bool     identifyCard(void);
	bool     switchHighSpeed(void);
	bool     decodeCSD(const uint32_t *csd);
	IOReturn transferBlocks(bool write, UInt64 block, UInt32 nblks,
	    IOMemoryDescriptor *buffer, UInt64 bufferOffset);
	IOReturn transferRetry(bool write, UInt64 block, UInt32 nblks,
	    IOMemoryDescriptor *buffer, UInt64 bufferOffset);
	void     overlayQueued(UInt64 block, UInt64 nblks, IOMemoryDescriptor *buffer);
	void     writeBatch(void);
	void     logStats(void);
	static void writeWorker(void *arg, wait_result_t wr);
};

#endif
