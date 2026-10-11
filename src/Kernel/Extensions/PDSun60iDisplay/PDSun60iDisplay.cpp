#include "PDSun60iDisplay.h"

#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>

#define super IOService
OSDefineMetaClassAndStructors(PDSun60iDisplay, IOService);

// the a733 user manual (v1.00, allwinner gitlab tina5.0_aiot/product/docs) has no display engine registers,
// so the layer ones are our own reads of the layer the firmware scans out of de block: the vendor device tree's de@5000000 reg
#define kDEPhys			0x05000000ULL
#define kDEMapOffset		0x100000ULL
#define kDEMapSize		0x2000
// board reads: 0xff008003 while scanning, nonzero is all that is relied on
#define kDE_LAY_ATTR		0x1000
// board reads: 0x0437077f, (height - 1) << 16 | (width - 1) for 1920x1080
#define kDE_LAY_SIZE		0x1004
// board reads: 0x1e00, the 7680-byte stride of the boot framebuffer
#define kDE_LAY_PITCH		0x100c
// board reads hold the scanout address, boot.cmd's write here moves the picture
#define kDE_LAY_ADDR		0x1018

// tv tcon, a733 user manual v1.00 section 15.2.10, base 0x05730000 from the memory map (p. 8)
// TV_CTL_REG 0x090 bit 31 TV_EN (15.2.10.6, p. 725)
#define kTCON_CTL		0x090
#define kTCON_CTL_EN		(1u << 31)
// TV_GINT0_REG 0x004: bit 30 TV_VB_INT_EN, bit 14 TV_VB_INT_FLAG, write 0 to clear (15.2.10.2, p. 723)
#define kTCON_GINT0		0x004
#define kTCON_VB_INT_EN		(1u << 30)
#define kTCON_VB_INT_FLAG	(1u << 14)
// TV_DEBUG_REG 0x0fc bits 15:0 TV_CURRENT_LINE (15.2.10.14, p. 729)
#define kTCON_DEBUG		0x0fc
#define kTCON_LINE_MASK		0xffffu

// more vblanks than this in 100 ms means the status is not being cleared
#define kStormLimit		100
#define kFlipTimeoutMs		100

#define DRM_FORMAT_ARGB8888	0x34325241u
#define DRM_FORMAT_XRGB8888	0x34325258u

static uint64_t
now_ns(void)
{
	uint64_t ns;

	absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
	return ns;
}

IOService *
PDSun60iDisplay::probe(IOService *provider, SInt32 *score)
{
	IOMemoryMap *map;
	uint32_t ctl, enable = 1;

	if (PE_parse_boot_argn("pddisplay", &enable, sizeof(enable)) && enable == 0)
		return NULL;
	// both tv tcons match by name, only the one the firmware left running carries hdmi
	map = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	if (map == NULL)
		return NULL;
	ctl = *(volatile uint32_t *)((uintptr_t)map->getVirtualAddress() + kTCON_CTL);
	map->release();
	if (!(ctl & kTCON_CTL_EN))
		return NULL;
	return super::probe(provider, score);
}

bool
PDSun60iDisplay::start(IOService *provider)
{
	IOMemoryDescriptor *md;
	uint32_t attr, size;
	thread_t th = NULL;

	if (!super::start(provider))
		return false;
	fNub = provider;
	fTconMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	md = IOMemoryDescriptor::withPhysicalAddress(kDEPhys + kDEMapOffset, kDEMapSize, kIODirectionInOut);
	if (md != NULL) {
		fDEMap = md->map(kIOMapInhibitCache);
		md->release();
	}
	fLock = IOLockAlloc();
	fFlipLock = IOLockAlloc();
	if (fTconMap == NULL || fDEMap == NULL || fLock == NULL || fFlipLock == NULL) {
		IOLog("PDSun60iDisplay: cannot map the tcon or the display engine\n");
		return false;
	}
	fTcon = (volatile uint8_t *)fTconMap->getVirtualAddress();
	fDE = (volatile uint8_t *)fDEMap->getVirtualAddress();

	attr = de(kDE_LAY_ATTR);
	size = de(kDE_LAY_SIZE);
	fWidth = (size & 0x1fff) + 1;
	fHeight = ((size >> 16) & 0x1fff) + 1;
	fPitch = de(kDE_LAY_PITCH);
	fBootAddr = de(kDE_LAY_ADDR);
	// the firmware's own scanout dump names the format AR24
	fFormat = DRM_FORMAT_ARGB8888;
	if (attr == 0 || fBootAddr == 0 || fPitch < fWidth * 4) {
		IOLog("PDSun60iDisplay: layer 0 is not scanning out (attr 0x%x addr 0x%llx pitch %u)\n",
		    attr, fBootAddr, fPitch);
		return false;
	}
	measure();
	IOLog("PDSun60iDisplay: layer 0 %ux%u pitch %u attr 0x%x at 0x%llx, tcon gint0 0x%x, %u lines, %u.%03u hz\n",
	    fWidth, fHeight, fPitch, attr, fBootAddr, tcon(kTCON_GINT0), fTotalLines,
	    fRefreshMHz / 1000, fRefreshMHz % 1000);

	setProperty("width", fWidth, 32);
	setProperty("height", fHeight, 32);
	setProperty("pitch", fPitch, 32);
	setProperty("boot-address", fBootAddr, 64);
	setProperty("refresh-mhz", fRefreshMHz, 32);

	// registerInterrupt waits for every cpu to take interrupts, so not on the matching thread
	retain();
	if (kernel_thread_start(&PDSun60iDisplay::irqThread, this, &th) != KERN_SUCCESS) {
		release();
		return false;
	}
	thread_deallocate(th);
	registerService();
	return true;
}

void
PDSun60iDisplay::irqThread(void *arg, wait_result_t)
{
	PDSun60iDisplay *self = (PDSun60iDisplay *)arg;

	if (!self->attachInterrupt())
		IOLog("PDSun60iDisplay: no vblank interrupt, flips poll the line counter\n");
	self->release();
	thread_terminate(current_thread());
}

void
PDSun60iDisplay::stop(IOService *provider)
{
	if (fIntSource != NULL) {
		fIntSource->disable();
		tconWrite(kTCON_GINT0, tcon(kTCON_GINT0) & ~(kTCON_VB_INT_EN | kTCON_VB_INT_FLAG));
		if (fWorkLoop != NULL)
			fWorkLoop->removeEventSource(fIntSource);
		OSSafeReleaseNULL(fIntSource);
	}
	if (fDE != NULL && fBootAddr != 0)
		latch(fBootAddr, fPitch);
	super::stop(provider);
}

void
PDSun60iDisplay::free(void)
{
	OSSafeReleaseNULL(fTconMap);
	OSSafeReleaseNULL(fDEMap);
	if (fLock != NULL)
		IOLockFree(fLock);
	if (fFlipLock != NULL)
		IOLockFree(fFlipLock);
	super::free();
}

uint32_t
PDSun60iDisplay::line(void) const
{
	return tcon(kTCON_DEBUG) & kTCON_LINE_MASK;
}

void
PDSun60iDisplay::latch(uint64_t addr, uint32_t pitch)
{
	if (pitch != de(kDE_LAY_PITCH))
		deWrite(kDE_LAY_PITCH, pitch);
	deWrite(kDE_LAY_ADDR, (uint32_t)addr);
}

// lines per frame and the frame rate, from the line counter's wraps
void
PDSun60iDisplay::measure(void)
{
	uint64_t t0 = now_ns(), wraps[3] = { 0 };
	uint32_t prev = line(), maxl = prev, n = 0;

	while (n < 3 && now_ns() - t0 < 100000000ULL) {
		uint32_t l = line();

		if (l > maxl)
			maxl = l;
		if (l < prev)
			wraps[n++] = now_ns();
		prev = l;
		IODelay(5);
	}
	fTotalLines = maxl + 1;
	if (n == 3 && wraps[2] > wraps[0])
		fRefreshMHz = (uint32_t)(2000000000000ULL / (wraps[2] - wraps[0]));
}

// the gic spi the tcon node names, as the PDArmGIC controller's 32-bit intid
bool
PDSun60iDisplay::attachInterrupt(void)
{
	OSData *prop = OSDynamicCast(OSData, fNub->getProperty("interrupts"));
	const OSSymbol *gic = OSSymbol::withCString("PDArmGIC");
	OSArray *controllers, *specifiers;
	const uint32_t *cells;
	uint32_t type, num, intid;
	OSData *spec;

	if (prop == NULL || prop->getLength() < 12 || gic == NULL) {
		OSSafeReleaseNULL(gic);
		return false;
	}
	cells = (const uint32_t *)prop->getBytesNoCopy();
	type = cells[0];
	num = cells[1];
	// the tree may still hold its cells big-endian
	if (type > 1 || num > 987) {
		type = OSSwapInt32(type);
		num = OSSwapInt32(num);
	}
	intid = num + (type == 0 ? 32 : 16);
	spec = OSData::withBytes(&intid, sizeof(intid));
	controllers = OSArray::withObjects((const OSObject **)&gic, 1);
	specifiers = OSArray::withObjects((const OSObject **)&spec, 1);
	gic->release();
	OSSafeReleaseNULL(spec);
	if (controllers == NULL || specifiers == NULL) {
		OSSafeReleaseNULL(controllers);
		OSSafeReleaseNULL(specifiers);
		return false;
	}
	fNub->setProperty(gIOInterruptControllersKey, controllers);
	fNub->setProperty(gIOInterruptSpecifiersKey, specifiers);
	controllers->release();
	specifiers->release();

	fWorkLoop = getWorkLoop();
	if (fWorkLoop == NULL)
		return false;
	fIntSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
	    OSMemberFunctionCast(IOInterruptEventAction, this, &PDSun60iDisplay::vblankAction),
	    OSMemberFunctionCast(IOFilterInterruptAction, this, &PDSun60iDisplay::vblankFilter),
	    fNub, 0);
	if (fIntSource == NULL || fWorkLoop->addEventSource(fIntSource) != kIOReturnSuccess) {
		OSSafeReleaseNULL(fIntSource);
		return false;
	}
	fStormStart = now_ns();
	tconWrite(kTCON_GINT0, (tcon(kTCON_GINT0) & ~kTCON_VB_INT_FLAG) | kTCON_VB_INT_EN);
	fIntSource->enable();
	IOSleep(100);
	fIrqOK = fVblanks > 0;
	IOLog("PDSun60iDisplay: vblank interrupt intid %u, %llu vblanks in 100 ms, status 0x%x at line %u\n",
	    intid, fVblanks, fFirstFlags, fVblankLine);
	return fIrqOK;
}

// primary interrupt: ack the tcon, then do the pending flip while the engine is in vblank
bool
PDSun60iDisplay::vblankFilter(IOFilterInterruptEventSource *)
{
	uint32_t g = tcon(kTCON_GINT0);
	uint64_t t = now_ns();

	if (!(g & kTCON_VB_INT_FLAG))
		return false;
	tconWrite(kTCON_GINT0, g & ~kTCON_VB_INT_FLAG);
	if (fFirstFlags == 0) {
		fFirstFlags = g;
		fVblankLine = line();
	}
	if (fPending) {
		latch(fPendingAddr, fPendingPitch);
		fPending = false;
	}
	fVblankTime = t;
	fVblanks = fVblanks + 1;

	if (t - fStormStart >= 100000000ULL) {
		fStormStart = t;
		fStormCount = 0;
	} else if (++fStormCount > kStormLimit) {
		// the status bit is not what we clear: stop the interrupt, flips fall back to polling
		tconWrite(kTCON_GINT0, tcon(kTCON_GINT0) & ~(kTCON_VB_INT_EN | kTCON_VB_INT_FLAG));
		fIrqOK = false;
	}
	return true;
}

void
PDSun60iDisplay::vblankAction(IOInterruptEventSource *, int)
{
	IOLockLock(fLock);
	IOLockWakeup(fLock, (event_t)&fVblanks, false);
	IOLockUnlock(fLock);
}

// no interrupt: wait for the line counter to leave the active area, then write
IOReturn
PDSun60iDisplay::pollFlip(uint64_t addr, uint32_t pitch)
{
	uint64_t t0 = now_ns();
	uint32_t prev = line();

	while (now_ns() - t0 < 50000000ULL) {
		uint32_t l = line();

		if (l >= fHeight && prev < fHeight) {
			latch(addr, pitch);
			fVblankTime = now_ns();
			fVblanks = fVblanks + 1;
			// stay past the end of this vblank so the caller's next write is a frame later
			while (line() >= fHeight && now_ns() - t0 < 60000000ULL) {
				IODelay(50);
			}
			return kIOReturnSuccess;
		}
		prev = l;
		IODelay(50);
	}
	latch(addr, pitch);
	return kIOReturnTimeout;
}

IOReturn
PDSun60iDisplay::flip(uint64_t addr, uint32_t pitch, struct pd_display_flip *out)
{
	IOReturn ret = kIOReturnSuccess;

	if (addr == 0 || addr >= (1ULL << 32) || (addr & 0xf) || pitch < fWidth * 4)
		return kIOReturnBadArgument;

	IOLockLock(fFlipLock);
	if (fIrqOK) {
		uint64_t deadline;

		IOLockLock(fLock);
		fPendingAddr = addr;
		fPendingPitch = pitch;
		__asm__ volatile ("dmb ish" ::: "memory");
		fPending = true;
		clock_interval_to_deadline(kFlipTimeoutMs, kMillisecondScale, &deadline);
		while (fPending) {
			if (IOLockSleepDeadline(fLock, (event_t)&fVblanks, *(AbsoluteTime *)&deadline, THREAD_UNINT) == THREAD_TIMED_OUT)
				break;
		}
		if (fPending) {
			fPending = false;
			fIrqOK = false;
			ret = kIOReturnTimeout;
		}
		IOLockUnlock(fLock);
		if (ret == kIOReturnTimeout) {
			IOLog("PDSun60iDisplay: no vblank in %u ms, polling from now on\n", kFlipTimeoutMs);
			ret = pollFlip(addr, pitch);
		}
	} else {
		ret = pollFlip(addr, pitch);
	}
	if (out != NULL) {
		out->sequence = fVblanks;
		out->time_ns = fVblankTime;
	}
	IOLockUnlock(fFlipLock);
	return ret;
}

void
PDSun60iDisplay::fillInfo(struct pd_display_info *info)
{
	info->width = fWidth;
	info->height = fHeight;
	info->pitch = fPitch;
	info->format = fFormat;
	info->boot_address = fBootAddr;
	info->current_address = de(kDE_LAY_ADDR);
	info->refresh_mhz = fRefreshMHz;
	info->total_lines = fTotalLines;
	info->vblanks = fVblanks;
}

IOReturn
PDSun60iDisplay::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
    void *param1, void *param2, void *param3, void *param4)
{
	if (functionName->isEqualTo(PD_DISPLAY_FN_FLIP))
		return flip((uint64_t)(uintptr_t)param1, (uint32_t)(uintptr_t)param2, (struct pd_display_flip *)param3);
	if (functionName->isEqualTo(PD_DISPLAY_FN_RESTORE))
		return flip(fBootAddr, fPitch, NULL);
	if (functionName->isEqualTo(PD_DISPLAY_FN_INFO)) {
		if (param1 == NULL)
			return kIOReturnBadArgument;
		fillInfo((struct pd_display_info *)param1);
		return kIOReturnSuccess;
	}
	return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}
