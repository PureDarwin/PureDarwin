#include "PDBcm2835SD.h"
#include "PDBcm2835SDDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>

#define super IOService
OSDefineMetaClassAndStructors(PDBcm2835SD, IOService);

/* Bus address 0x7E300000 is ARM physical 0x20300000. */
#define kEMMCPhys               0x20300000
#define kEMMCSize               0x100

#define kARG2                   0x00
#define kBLKSIZECNT             0x04
#define kARG1                   0x08
#define kCMDTM                  0x0c
#define kRESP0                  0x10
#define kRESP1                  0x14
#define kRESP2                  0x18
#define kRESP3                  0x1c
#define kDATA                   0x20
#define kSTATUS                 0x24
#define kCONTROL0               0x28
#define kCONTROL1               0x2c
#define kINTERRUPT              0x30
#define kIRPT_MASK              0x34
#define kIRPT_EN                0x38
#define kCONTROL2               0x3c
#define kSLOTISR_VER            0xfc

/* CONTROL1 */
#define kC1_SRST_DATA           (1u << 26)
#define kC1_SRST_CMD            (1u << 25)
#define kC1_SRST_HC             (1u << 24)
#define kC1_DATA_TOUNIT_SHIFT   16
#define kC1_CLK_FREQ8_SHIFT     8
#define kC1_CLK_FREQ_MS2_SHIFT  6
#define kC1_CLK_EN              (1u << 2)
#define kC1_CLK_STABLE          (1u << 1)
#define kC1_CLK_INTLEN          (1u << 0)

/* STATUS */
#define kST_DAT_INHIBIT         (1u << 1)
#define kST_CMD_INHIBIT         (1u << 0)

/* INTERRUPT */
#define kINT_ACMD_ERR           (1u << 24)
#define kINT_ERR                (1u << 15)
#define kINT_READ_RDY           (1u << 5)
#define kINT_WRITE_RDY          (1u << 4)
#define kINT_DATA_DONE          (1u << 1)
#define kINT_CMD_DONE           (1u << 0)
#define kINT_ERROR_MASK         0x017f0000u

/* CMDTM */
#define kCMD_INDEX_SHIFT        24
#define kCMD_ISDATA             (1u << 21)
#define kCMD_IXCHK_EN           (1u << 20)
#define kCMD_CRCCHK_EN          (1u << 19)
#define kCMD_RSPNS_NONE         (0u << 16)
#define kCMD_RSPNS_136          (1u << 16)
#define kCMD_RSPNS_48           (2u << 16)
#define kCMD_RSPNS_48B          (3u << 16)
#define kTM_MULTI_BLOCK         (1u << 5)
#define kTM_DAT_DIR_CARD_TO_HOST (1u << 4)
#define kTM_AUTO_CMD12          (1u << 2)
#define kTM_BLKCNT_EN           (1u << 1)

/*
 * Command encodings. The response-type flags are part of the command word
 * because the controller needs to be told what to expect; getting them wrong
 * makes the block behave erratically rather than reporting an error.
 */
#define CMD(idx, flags)         (((uint32_t)(idx) << kCMD_INDEX_SHIFT) | (flags))
#define kCMD_GO_IDLE            CMD(0,  kCMD_RSPNS_NONE)
#define kCMD_ALL_SEND_CID       CMD(2,  kCMD_RSPNS_136 | kCMD_CRCCHK_EN)
#define kCMD_SEND_REL_ADDR      CMD(3,  kCMD_RSPNS_48  | kCMD_CRCCHK_EN | kCMD_IXCHK_EN)
#define kCMD_SELECT_CARD        CMD(7,  kCMD_RSPNS_48B | kCMD_CRCCHK_EN | kCMD_IXCHK_EN)
#define kCMD_SEND_IF_COND       CMD(8,  kCMD_RSPNS_48  | kCMD_CRCCHK_EN | kCMD_IXCHK_EN)
#define kCMD_SEND_CSD           CMD(9,  kCMD_RSPNS_136 | kCMD_CRCCHK_EN)
#define kCMD_SET_BLOCKLEN       CMD(16, kCMD_RSPNS_48  | kCMD_CRCCHK_EN | kCMD_IXCHK_EN)
#define kCMD_READ_SINGLE        CMD(17, kCMD_RSPNS_48  | kCMD_CRCCHK_EN | kCMD_IXCHK_EN | kCMD_ISDATA | kTM_DAT_DIR_CARD_TO_HOST)
#define kCMD_READ_MULTI         CMD(18, kCMD_RSPNS_48  | kCMD_CRCCHK_EN | kCMD_IXCHK_EN | kCMD_ISDATA | kTM_DAT_DIR_CARD_TO_HOST | kTM_MULTI_BLOCK | kTM_BLKCNT_EN | kTM_AUTO_CMD12)
#define kCMD_WRITE_SINGLE       CMD(24, kCMD_RSPNS_48  | kCMD_CRCCHK_EN | kCMD_IXCHK_EN | kCMD_ISDATA)
#define kCMD_WRITE_MULTI        CMD(25, kCMD_RSPNS_48  | kCMD_CRCCHK_EN | kCMD_IXCHK_EN | kCMD_ISDATA | kTM_MULTI_BLOCK | kTM_BLKCNT_EN | kTM_AUTO_CMD12)
#define kCMD_APP_CMD            CMD(55, kCMD_RSPNS_48  | kCMD_CRCCHK_EN | kCMD_IXCHK_EN)
#define kACMD_SET_BUS_WIDTH     CMD(6,  kCMD_RSPNS_48  | kCMD_CRCCHK_EN | kCMD_IXCHK_EN)
#define kACMD_SD_SEND_OP_COND   CMD(41, kCMD_RSPNS_48)

/*
 * The EMMC base clock comes from the VideoCore and is not discoverable through
 * any register. 250MHz is what the firmware programs on this SoC family; if it
 * were ever lower the dividers below would simply produce a slower card clock,
 * which is safe, so nothing here depends on the value being exact.
 */
#define kBaseClockHz            250000000u
#define kIdentClockHz           400000u
#define kTransferClockHz        25000000u

#define kMaxBlocksPerTransfer   64

bool
PDBcm2835SD::start(IOService *provider)
{
	if (!super::start(provider)) {
		return false;
	}

	IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
		(IOPhysicalAddress)kEMMCPhys, kEMMCSize, kIODirectionOutIn);
	if (desc == NULL) {
		return false;
	}
	fRegMap = desc->map(kIOMapAnywhere | kIOMapInhibitCache);
	desc->release();
	if (fRegMap == NULL) {
		IOLog("PDBcm2835SD: cannot map EMMC at 0x%x\n", kEMMCPhys);
		return false;
	}
	fRegs = (volatile uint8_t *)fRegMap->getVirtualAddress();

	fLock = IOLockAlloc();
	if (fLock == NULL) {
		return false;
	}

	IOLog("PDBcm2835SD: EMMC host version 0x%x\n", read32(kSLOTISR_VER));

	if (!resetHost() || !identifyCard()) {
		IOLog("PDBcm2835SD: no usable card\n");
		return false;
	}

	IOLog("PDBcm2835SD: %llu blocks of 512 bytes (%llu MB), %s addressing%s\n",
	    fBlockCount, (fBlockCount * 512) / (1024 * 1024),
	    fBlockAddressed ? "block" : "byte",
	    fReadOnly ? ", write protected" : "");

	fDisk = new PDBcm2835SDDisk;
	if (fDisk == NULL || !fDisk->initWithController(this)) {
		OSSafeReleaseNULL(fDisk);
		return false;
	}
	if (!fDisk->attach(this)) {
		OSSafeReleaseNULL(fDisk);
		return false;
	}
	fDisk->registerService();

	registerService();
	return true;
}

void
PDBcm2835SD::stop(IOService *provider)
{
	if (fDisk != NULL) {
		fDisk->detach(this);
		OSSafeReleaseNULL(fDisk);
	}
	super::stop(provider);
}

void
PDBcm2835SD::free(void)
{
	if (fLock != NULL) {
		IOLockFree(fLock);
		fLock = NULL;
	}
	OSSafeReleaseNULL(fRegMap);
	fRegs = NULL;
	super::free();
}

bool
PDBcm2835SD::resetHost(void)
{
	write32(kCONTROL0, 0);
	write32(kCONTROL1, kC1_SRST_HC);

	for (int i = 0; i < 1000; i++) {
		IODelay(100);
		if ((read32(kCONTROL1) & kC1_SRST_HC) == 0) {
			break;
		}
	}
	if ((read32(kCONTROL1) & kC1_SRST_HC) != 0) {
		IOLog("PDBcm2835SD: host reset timed out\n");
		return false;
	}

	if (!setClock(kIdentClockHz)) {
		return false;
	}

	/*
	 * Unmask every flag so the INTERRUPT register records them, but leave
	 * IRPT_EN clear: the flags are polled and no interrupt is ever routed to
	 * the CPU.
	 */
	write32(kIRPT_EN, 0);
	write32(kIRPT_MASK, 0xffffffff);
	write32(kINTERRUPT, 0xffffffff);
	return true;
}

bool
PDBcm2835SD::setClock(uint32_t targetHz)
{
	/*
	 * Divided-clock mode: the divisor field holds half the actual divider, and
	 * the hardware only implements powers of two, so round up to one.
	 */
	uint32_t divisor = 1;
	while ((kBaseClockHz / divisor) > targetHz && divisor < 0x400) {
		divisor <<= 1;
	}
	uint32_t half = divisor >> 1;

	uint32_t c1 = read32(kCONTROL1);
	c1 &= ~kC1_CLK_EN;
	write32(kCONTROL1, c1);
	IODelay(10);

	c1 &= ~((0xffu << kC1_CLK_FREQ8_SHIFT) | (0x3u << kC1_CLK_FREQ_MS2_SHIFT));
	c1 &= ~(0xfu << kC1_DATA_TOUNIT_SHIFT);
	c1 |= (half & 0xff) << kC1_CLK_FREQ8_SHIFT;
	c1 |= ((half >> 8) & 0x3) << kC1_CLK_FREQ_MS2_SHIFT;
	c1 |= 0xeu << kC1_DATA_TOUNIT_SHIFT;   /* longest data timeout */
	c1 |= kC1_CLK_INTLEN;
	write32(kCONTROL1, c1);

	for (int i = 0; i < 1000; i++) {
		IODelay(100);
		if ((read32(kCONTROL1) & kC1_CLK_STABLE) != 0) {
			break;
		}
	}
	if ((read32(kCONTROL1) & kC1_CLK_STABLE) == 0) {
		IOLog("PDBcm2835SD: clock never reported stable\n");
		return false;
	}

	write32(kCONTROL1, read32(kCONTROL1) | kC1_CLK_EN);
	IODelay(10);
	return true;
}

bool
PDBcm2835SD::waitForInterrupt(uint32_t mask, uint32_t timeoutUs)
{
	uint32_t waited = 0;

	for (;;) {
		uint32_t irq = read32(kINTERRUPT);

		if ((irq & kINT_ERROR_MASK) != 0) {
			write32(kINTERRUPT, irq);
			return false;
		}
		if ((irq & mask) == mask) {
			write32(kINTERRUPT, mask);
			return true;
		}
		if (waited >= timeoutUs) {
			return false;
		}
		IODelay(10);
		waited += 10;
	}
}

bool
PDBcm2835SD::sendCommand(uint32_t cmd, uint32_t arg, uint32_t *resp)
{
	/* Wait for the command line, and the data line too if this moves data. */
	uint32_t busy = kST_CMD_INHIBIT;
	if ((cmd & kCMD_ISDATA) != 0 || (cmd & 0x30000u) == kCMD_RSPNS_48B) {
		busy |= kST_DAT_INHIBIT;
	}
	for (int i = 0; i < 10000; i++) {
		if ((read32(kSTATUS) & busy) == 0) {
			break;
		}
		IODelay(100);
	}
	if ((read32(kSTATUS) & busy) != 0) {
		IOLog("PDBcm2835SD: controller busy before cmd %u\n",
		    cmd >> kCMD_INDEX_SHIFT);
		return false;
	}

	write32(kINTERRUPT, read32(kINTERRUPT));
	write32(kARG1, arg);
	write32(kCMDTM, cmd);

	if (!waitForInterrupt(kINT_CMD_DONE, 1000000)) {
		IOLog("PDBcm2835SD: cmd %u failed (int 0x%x)\n",
		    cmd >> kCMD_INDEX_SHIFT, read32(kINTERRUPT));
		return false;
	}

	if (resp != NULL) {
		resp[0] = read32(kRESP0);
		resp[1] = read32(kRESP1);
		resp[2] = read32(kRESP2);
		resp[3] = read32(kRESP3);
	}
	return true;
}

bool
PDBcm2835SD::sendAppCommand(uint32_t cmd, uint32_t arg, uint32_t *resp)
{
	if (!sendCommand(kCMD_APP_CMD, fRCA, NULL)) {
		return false;
	}
	return sendCommand(cmd, arg, resp);
}

bool
PDBcm2835SD::decodeCSD(const uint32_t *resp)
{
	/*
	 * A 136-bit response arrives with the CRC and stop bit already stripped,
	 * so RESP0..3 hold the CSD shifted right by 8. Every bit position below is
	 * expressed in that shifted space.
	 */
	uint32_t structure = (resp[3] >> 22) & 0x3;

	if (structure == 1) {
		/* SDHC/SDXC: capacity is (C_SIZE + 1) * 512KB. */
		uint32_t csize = (resp[1] >> 8) & 0x3fffff;
		fBlockCount = ((uint64_t)csize + 1) * 1024;
		fBlockAddressed = true;
	} else if (structure == 0) {
		uint32_t csize = ((resp[2] & 0x3) << 10) | ((resp[1] >> 22) & 0x3ff);
		uint32_t csizemult = (resp[1] >> 7) & 0x7;
		uint32_t readbllen = (resp[2] >> 8) & 0xf;
		uint64_t bytes = ((uint64_t)csize + 1) *
		    (1ULL << (csizemult + 2)) * (1ULL << readbllen);
		fBlockCount = bytes / 512;
		fBlockAddressed = false;
	} else {
		IOLog("PDBcm2835SD: unknown CSD structure %u\n", structure);
		return false;
	}

	return fBlockCount != 0;
}

bool
PDBcm2835SD::identifyCard(void)
{
	uint32_t resp[4];

	fRCA = 0;

	if (!sendCommand(kCMD_GO_IDLE, 0, NULL)) {
		return false;
	}

	/*
	 * CMD8 with the 2.7-3.6V pattern. A card that echoes it back is SD 2.0 or
	 * later and may be high capacity; one that does not answer is older, and
	 * the host capacity bit must then stay clear in ACMD41.
	 */
	bool sdV2 = sendCommand(kCMD_SEND_IF_COND, 0x1aa, resp) &&
	    ((resp[0] & 0xfff) == 0x1aa);
	write32(kINTERRUPT, 0xffffffff);

	/*
	 * ACMD41 until the card leaves initialisation. Bit 31 of the response is
	 * the "not busy" flag; bit 30 reports block addressing.
	 */
	uint32_t ocr = 0;
	bool ready = false;
	for (int i = 0; i < 1000; i++) {
		uint32_t arg = 0x00ff8000u | (sdV2 ? (1u << 30) : 0u);
		if (!sendAppCommand(kACMD_SD_SEND_OP_COND, arg, resp)) {
			return false;
		}
		ocr = resp[0];
		if ((ocr & (1u << 31)) != 0) {
			ready = true;
			break;
		}
		IOSleep(10);
	}
	if (!ready) {
		IOLog("PDBcm2835SD: card never left initialisation\n");
		return false;
	}

	if (!sendCommand(kCMD_ALL_SEND_CID, 0, resp)) {
		return false;
	}
	if (!sendCommand(kCMD_SEND_REL_ADDR, 0, resp)) {
		return false;
	}
	fRCA = resp[0] & 0xffff0000u;

	if (!sendCommand(kCMD_SEND_CSD, fRCA, resp) || !decodeCSD(resp)) {
		return false;
	}
	/* CSD bit populated above tells us block vs byte addressing for SDHC. */
	if ((ocr & (1u << 30)) == 0) {
		fBlockAddressed = false;
	}

	if (!sendCommand(kCMD_SELECT_CARD, fRCA, resp)) {
		return false;
	}

	/* Four-bit bus; harmless to skip if the card refuses. */
	if (sendAppCommand(kACMD_SET_BUS_WIDTH, 2, resp)) {
		write32(kCONTROL0, read32(kCONTROL0) | (1u << 1));
	}

	if (!sendCommand(kCMD_SET_BLOCKLEN, 512, resp)) {
		return false;
	}

	return setClock(kTransferClockHz);
}

IOReturn
PDBcm2835SD::transferBlocks(bool write, UInt64 block, UInt32 nblks,
    IOMemoryDescriptor *buffer, UInt64 bufferOffset)
{
	uint32_t cmd;
	if (write) {
		cmd = (nblks > 1) ? kCMD_WRITE_MULTI : kCMD_WRITE_SINGLE;
	} else {
		cmd = (nblks > 1) ? kCMD_READ_MULTI : kCMD_READ_SINGLE;
	}

	write32(kBLKSIZECNT, (nblks << 16) | 512);

	uint32_t arg = fBlockAddressed ? (uint32_t)block : (uint32_t)(block * 512);
	if (!sendCommand(cmd, arg, NULL)) {
		return kIOReturnIOError;
	}

	uint32_t words[128];
	for (uint32_t b = 0; b < nblks; b++) {
		if (write) {
			if (!waitForInterrupt(kINT_WRITE_RDY, 1000000)) {
				return kIOReturnIOError;
			}
			if (buffer->readBytes(bufferOffset + (UInt64)b * 512,
			    words, sizeof(words)) != sizeof(words)) {
				return kIOReturnIOError;
			}
			for (uint32_t i = 0; i < 128; i++) {
				write32(kDATA, words[i]);
			}
		} else {
			if (!waitForInterrupt(kINT_READ_RDY, 1000000)) {
				return kIOReturnIOError;
			}
			for (uint32_t i = 0; i < 128; i++) {
				words[i] = read32(kDATA);
			}
			if (buffer->writeBytes(bufferOffset + (UInt64)b * 512,
			    words, sizeof(words)) != sizeof(words)) {
				return kIOReturnIOError;
			}
		}
	}

	/*
	 * A multi-block transfer ends with the automatic CMD12 the controller was
	 * told to issue, so DATA_DONE covers both cases.
	 */
	if (!waitForInterrupt(kINT_DATA_DONE, 5000000)) {
		return kIOReturnIOError;
	}

	return kIOReturnSuccess;
}

IOReturn
PDBcm2835SD::readWrite(bool write, UInt64 block, UInt64 nblks,
    IOMemoryDescriptor *buffer)
{
	if (fRegs == NULL || buffer == NULL) {
		return kIOReturnNoDevice;
	}
	if (write && fReadOnly) {
		return kIOReturnNotWritable;
	}
	if (block + nblks > fBlockCount) {
		return kIOReturnBadArgument;
	}
	if (nblks == 0) {
		return kIOReturnSuccess;
	}

	IOReturn ret = kIOReturnSuccess;

	IOLockLock(fLock);
	UInt64 done = 0;
	while (done < nblks) {
		UInt32 chunk = (UInt32)min(nblks - done, (UInt64)kMaxBlocksPerTransfer);

		ret = transferBlocks(write, block + done, chunk, buffer, done * 512);
		if (ret != kIOReturnSuccess) {
			IOLog("PDBcm2835SD: %s failed at block %llu\n",
			    write ? "write" : "read", block + done);
			break;
		}
		done += chunk;
	}
	IOLockUnlock(fLock);

	return ret;
}
