// SPDX-License-Identifier: MIT
// a framebuffer in ram for a machine with no display: neuswc draws and flips through the
// presenter, a streamer maps the same buffers and waits for flips to send them somewhere

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <kern/clock.h>
#include <kern/sched_prim.h>
#include <pexpert/pexpert.h>

#include "PDHIDEventQueue.h"

#define kDepth		0

// display mode ids are indices in sModes plus one
static const struct { uint32_t width, height; } sModes[] = {
	{ 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1280, 720 }, { 1280, 1024 }, { 1920, 1080 },
};
#define kModeCount	(sizeof(sModes) / sizeof(sModes[0]))

// buffers 0 and 1 are the presenter's flip pair, 2 the aperture the console and copy path use
#define kBuffers	3
#define kAperture	2

class PDVirtualFramebuffer : public IOFramebuffer
{
	OSDeclareDefaultStructors(PDVirtualFramebuffer);

	IOBufferMemoryDescriptor *fBuffer[kBuffers];
	IOItemCount fModeCount;		// modes that fit the buffers
	IODisplayModeID fMode;
	IOLock *fLock;
	uint64_t fSeq;			// bumped by every flip, mode change and presenter close
	int fFront;			// buffer on screen
	uint64_t fPeriod;		// virtual vblank, abs time
	uint64_t fNextVblank;
	IOFBInterruptProc fConnectProc;
	OSObject *fConnectTarget;
	void *fConnectRef;
	UInt8 fMouseIndex;

public:
	bool start(IOService *provider) override;
	void free(void) override;
	IOReturn registerForInterruptType(IOSelect interruptType, IOFBInterruptProc proc,
		OSObject *target, void *ref, void **interruptRef) override;
	IOReturn unregisterInterrupt(void *interruptRef) override;
	IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
		IOUserClient **handler) override;

	IOReturn enableController() override;
	const char *getPixelFormats() override;
	IOReturn getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) override;
	IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
	IODeviceMemory *getApertureRange(IOPixelAperture aperture) override;
	IODeviceMemory *getVRAMRange(void) override;
	IOReturn getInformationForDisplayMode(IODisplayModeID displayMode,
		IODisplayModeInformation *info) override;
	UInt64 getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
	IOReturn getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
		IOPixelAperture aperture, IOPixelInformation *info) override;
	IOReturn getDisplayModes(IODisplayModeID *allDisplayModes) override;
	IOItemCount getDisplayModeCount(void) override;

	// for the user clients
	uint32_t width(void) { return sModes[fMode - 1].width; }
	uint32_t height(void) { return sModes[fMode - 1].height; }
	IOBufferMemoryDescriptor *buffer(UInt32 i) { return i < kBuffers ? fBuffer[i] : NULL; }
	void flip(int buffer, uint64_t *seq, uint64_t *ns);
	void presenterGone(void);
	IOReturn waitFrame(uint64_t last, uint32_t timeout_ms, uint64_t *seq, uint64_t *front);
	void state(uint64_t *seq, uint64_t *front);
	UInt8 mouseIndex(void) { return fMouseIndex; }
};

OSDefineMetaClassAndStructors(PDVirtualFramebuffer, IOFramebuffer);

// pdvfb=WWWWHHHH turns it on with that size as the largest mode, pdvfbhz the flip rate
bool
PDVirtualFramebuffer::start(IOService *provider)
{
	uint32_t want = 0, hz = 30;

	if (!PE_parse_boot_argn("pdvfb", &want, sizeof(want)) || want == 0)
		return false;
	PE_parse_boot_argn("pdvfbhz", &hz, sizeof(hz));
	hz = hz < 1 ? 1 : hz > 120 ? 120 : hz;
	uint32_t maxw = want / 10000, maxh = want % 10000;
	size_t bytes = 0;
	fModeCount = 0;
	for (IOItemCount i = 0; i < kModeCount; i++) {
		if (sModes[i].width > maxw || sModes[i].height > maxh)
			break;
		fModeCount = i + 1;
		bytes = (size_t)sModes[i].width * sModes[i].height * 4;
	}
	if (fModeCount == 0) {
		IOLog("PDVirtualFramebuffer: no mode fits %ux%u\n", maxw, maxh);
		return false;
	}
	fMode = fModeCount;
	fLock = IOLockAlloc();
	if (fLock == NULL)
		return false;
	// physically contiguous, as the aperture is handed out as device memory
	for (int i = 0; i < kBuffers; i++) {
		fBuffer[i] = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
		    kIODirectionInOut | kIOMemoryPhysicallyContiguous, round_page(bytes), 0xfffff000ULL);
		if (fBuffer[i] == NULL || fBuffer[i]->prepare() != kIOReturnSuccess) {
			IOLog("PDVirtualFramebuffer: no %zu bytes for buffer %d\n", bytes, i);
			return false;
		}
		bzero(fBuffer[i]->getBytesNoCopy(), bytes);
	}
	fFront = kAperture;
	nanoseconds_to_absolutetime(NSEC_PER_SEC / hz, &fPeriod);
	if (!IOFramebuffer::start(provider))
		return false;
	// a streamer's viewer is the keyboard and mouse: /dev/usb_hid_kbd and _mouse, which neuswc reads
	PDHIDPublishKeyboardDevice();
	PDHIDPublishMouseDevice();
	fMouseIndex = PDHIDAllocateMouseIndex();
	IOLog("PDVirtualFramebuffer: %ux%u, %u modes, flips paced at %u Hz\n", width(), height(),
	    (unsigned)fModeCount, hz);
	registerService();
	return true;
}

void
PDVirtualFramebuffer::free(void)
{
	for (int i = 0; i < kBuffers; i++) {
		if (fBuffer[i] != NULL) {
			fBuffer[i]->complete();
			fBuffer[i]->release();
		}
	}
	if (fLock != NULL)
		IOLockFree(fLock);
	IOFramebuffer::free();
}

IOReturn
PDVirtualFramebuffer::registerForInterruptType(IOSelect interruptType, IOFBInterruptProc proc,
	OSObject *target, void *ref, void **interruptRef)
{
	if (interruptType != kIOFBConnectInterruptType)
		return kIOReturnUnsupported;
	fConnectProc = proc;
	fConnectTarget = target;
	fConnectRef = ref;
	*interruptRef = (void *)&fConnectProc;
	return kIOReturnSuccess;
}

IOReturn
PDVirtualFramebuffer::unregisterInterrupt(void *interruptRef)
{
	if (interruptRef != (void *)&fConnectProc)
		return kIOReturnBadArgument;
	fConnectProc = NULL;
	return kIOReturnSuccess;
}

IOReturn
PDVirtualFramebuffer::enableController()
{
	return kIOReturnSuccess;
}

const char *
PDVirtualFramebuffer::getPixelFormats()
{
	return IO32BitDirectPixels;
}

IOReturn
PDVirtualFramebuffer::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth)
{
	*displayMode = fMode;
	*depth = kDepth;
	return kIOReturnSuccess;
}

IOReturn
PDVirtualFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
	if (depth != kDepth || displayMode < 1 || displayMode > (IODisplayModeID)fModeCount)
		return kIOReturnUnsupportedMode;
	IOLockLock(fLock);
	fMode = displayMode;
	fSeq++;
	IOLockWakeup(fLock, &fSeq, false);
	IOLockUnlock(fLock);
	IOLog("PDVirtualFramebuffer: mode %d, %ux%u\n", (int)displayMode, width(), height());
	return kIOReturnSuccess;
}

IODeviceMemory *
PDVirtualFramebuffer::getApertureRange(IOPixelAperture aperture)
{
	if (aperture != kIOFBSystemAperture)
		return NULL;
	return IODeviceMemory::withRange(fBuffer[kAperture]->getPhysicalAddress(),
	    (IOPhysicalLength)width() * height() * 4);
}

IODeviceMemory *
PDVirtualFramebuffer::getVRAMRange(void)
{
	return getApertureRange(kIOFBSystemAperture);
}

IOReturn
PDVirtualFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode,
	IODisplayModeInformation *info)
{
	if (displayMode < 1 || displayMode > (IODisplayModeID)fModeCount)
		return kIOReturnUnsupportedMode;
	bzero(info, sizeof(*info));
	info->nominalWidth = sModes[displayMode - 1].width;
	info->nominalHeight = sModes[displayMode - 1].height;
	info->refreshRate = 60 << 16;
	info->maxDepthIndex = kDepth;
	return kIOReturnSuccess;
}

UInt64
PDVirtualFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
	return 0;
}

// XRGB8888, b g r x in memory, rows packed
IOReturn
PDVirtualFramebuffer::getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
	IOPixelAperture aperture, IOPixelInformation *info)
{
	if (aperture != kIOFBSystemAperture || depth != kDepth || displayMode < 1 ||
	    displayMode > (IODisplayModeID)fModeCount)
		return kIOReturnUnsupportedMode;
	bzero(info, sizeof(*info));
	info->activeWidth = sModes[displayMode - 1].width;
	info->activeHeight = sModes[displayMode - 1].height;
	info->bytesPerRow = info->activeWidth * 4;
	strlcpy(info->pixelFormat, IO32BitDirectPixels, sizeof(info->pixelFormat));
	info->pixelType = kIORGBDirectPixels;
	info->componentMasks[0] = 0x00ff0000;
	info->componentMasks[1] = 0x0000ff00;
	info->componentMasks[2] = 0x000000ff;
	info->bitsPerPixel = 32;
	info->componentCount = 3;
	info->bitsPerComponent = 8;
	return kIOReturnSuccess;
}

IOReturn
PDVirtualFramebuffer::getDisplayModes(IODisplayModeID *allDisplayModes)
{
	for (IOItemCount i = 0; i < fModeCount; i++)
		allDisplayModes[i] = i + 1;
	return kIOReturnSuccess;
}

IOItemCount
PDVirtualFramebuffer::getDisplayModeCount(void)
{
	return fModeCount;
}

// a finished frame: on screen at once, the caller held to the virtual vblank so it
// renders no faster than a display would show
void
PDVirtualFramebuffer::flip(int buffer, uint64_t *seq, uint64_t *ns)
{
	uint64_t now = mach_absolute_time();

	// sleep for up to a whole period
	if (fNextVblank > now) {
		assert_wait_deadline((event_t)&fNextVblank, THREAD_UNINT, fNextVblank);
		thread_block(THREAD_CONTINUE_NULL);
	}
	now = mach_absolute_time();
	fNextVblank = (fNextVblank > now - fPeriod ? fNextVblank : now) + fPeriod;
	IOLockLock(fLock);
	fFront = buffer;
	*seq = ++fSeq;
	IOLockWakeup(fLock, &fSeq, false);
	IOLockUnlock(fLock);
	absolutetime_to_nanoseconds(now, ns);
}

// the compositor went: the aperture is what shows again
void
PDVirtualFramebuffer::presenterGone(void)
{
	IOLockLock(fLock);
	fFront = kAperture;
	fSeq++;
	IOLockWakeup(fLock, &fSeq, false);
	IOLockUnlock(fLock);
}

void
PDVirtualFramebuffer::state(uint64_t *seq, uint64_t *front)
{
	IOLockLock(fLock);
	*seq = fSeq;
	*front = (uint64_t)fFront;
	IOLockUnlock(fLock);
}

// until something newer than `last`, or the timeout: then the caller looks anyway, as a
// client drawing straight into the aperture never flips
IOReturn
PDVirtualFramebuffer::waitFrame(uint64_t last, uint32_t timeout_ms, uint64_t *seq, uint64_t *front)
{
	uint64_t deadline;

	clock_interval_to_deadline(timeout_ms, kMillisecondScale, &deadline);
	IOLockLock(fLock);
	while (fSeq == last) {
		if (IOLockSleepDeadline(fLock, &fSeq, deadline, THREAD_INTERRUPTIBLE) != THREAD_AWAKENED)
			break;
	}
	*seq = fSeq;
	*front = (uint64_t)fFront;
	IOLockUnlock(fLock);
	return kIOReturnSuccess;
}

// the presenter neuswc opens with type 'pdpf', the same calls PDDRMFramebuffer's takes
#define kPresenterType	'pdpf'
// a streamer: maps buffers 0 to 2 and waits for flips
#define kStreamerType	'pdvs'

enum {
	kPresenterGetInfo = 0,		// out: width, height, pitch, buffer count
	kPresenterFlip = 1,		// in: buffer, out: sequence, time in ns
	kPresenterMethodCount = 2,
};

enum {
	kStreamerGetInfo = 0,		// out: width, height, pitch, sequence, front buffer
	kStreamerWait = 1,		// in: last sequence, timeout ms, out: sequence, front buffer
	kStreamerInput = 2,		// in: 1 and usage, down for a key, 2 and buttons, dx, dy, wheel for the mouse
	kStreamerMethodCount = 3,
};

class PDVirtualFramebufferClient : public IOUserClient
{
	OSDeclareDefaultStructors(PDVirtualFramebufferClient);

	PDVirtualFramebuffer *fFramebuffer;
	UInt32 fType;

	static IOReturn getInfo(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn flip(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn streamInfo(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn wait(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn input(OSObject *target, void *reference, IOExternalMethodArguments *args);

public:
	bool initWithTask(task_t owningTask, void *securityID, UInt32 type) override;
	bool start(IOService *provider) override;
	IOReturn clientClose(void) override;
	IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
		IOMemoryDescriptor **memory) override;
	IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
		IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) override;
};

OSDefineMetaClassAndStructors(PDVirtualFramebufferClient, IOUserClient);

bool
PDVirtualFramebufferClient::initWithTask(task_t owningTask, void *securityID, UInt32 type)
{
	fType = type;
	return IOUserClient::initWithTask(owningTask, securityID, type);
}

bool
PDVirtualFramebufferClient::start(IOService *provider)
{
	fFramebuffer = OSDynamicCast(PDVirtualFramebuffer, provider);
	return fFramebuffer != NULL && IOUserClient::start(provider);
}

IOReturn
PDVirtualFramebufferClient::clientClose(void)
{
	if (fType == kPresenterType)
		fFramebuffer->presenterGone();
	terminate();
	return kIOReturnSuccess;
}

// plain cached ram on both sides: no device reads it, the streamer copies it out
IOReturn
PDVirtualFramebufferClient::clientMemoryForType(UInt32 type, IOOptionBits *options,
	IOMemoryDescriptor **memory)
{
	UInt32 count = fType == kPresenterType ? 2 : kBuffers;
	IOBufferMemoryDescriptor *b = type < count ? fFramebuffer->buffer(type) : NULL;

	if (b == NULL)
		return kIOReturnBadArgument;
	b->retain();
	*memory = b;
	*options = 0;
	return kIOReturnSuccess;
}

IOReturn
PDVirtualFramebufferClient::getInfo(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
	PDVirtualFramebuffer *fb = ((PDVirtualFramebufferClient *)target)->fFramebuffer;

	args->scalarOutput[0] = fb->width();
	args->scalarOutput[1] = fb->height();
	args->scalarOutput[2] = fb->width() * 4;
	args->scalarOutput[3] = 2;
	return kIOReturnSuccess;
}

IOReturn
PDVirtualFramebufferClient::flip(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
	PDVirtualFramebuffer *fb = ((PDVirtualFramebufferClient *)target)->fFramebuffer;
	uint64_t seq, ns;

	if (args->scalarInput[0] > 1)
		return kIOReturnBadArgument;
	fb->flip((int)args->scalarInput[0], &seq, &ns);
	args->scalarOutput[0] = seq;
	args->scalarOutput[1] = ns;
	return kIOReturnSuccess;
}

IOReturn
PDVirtualFramebufferClient::streamInfo(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
	PDVirtualFramebuffer *fb = ((PDVirtualFramebufferClient *)target)->fFramebuffer;

	args->scalarOutput[0] = fb->width();
	args->scalarOutput[1] = fb->height();
	args->scalarOutput[2] = fb->width() * 4;
	fb->state(&args->scalarOutput[3], &args->scalarOutput[4]);
	return kIOReturnSuccess;
}

IOReturn
PDVirtualFramebufferClient::wait(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
	PDVirtualFramebuffer *fb = ((PDVirtualFramebufferClient *)target)->fFramebuffer;
	uint64_t ms = args->scalarInput[1];

	return fb->waitFrame(args->scalarInput[0], ms > 10000 ? 10000 : (uint32_t)ms,
	    &args->scalarOutput[0], &args->scalarOutput[1]);
}

IOReturn
PDVirtualFramebufferClient::input(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
	PDVirtualFramebuffer *fb = ((PDVirtualFramebufferClient *)target)->fFramebuffer;
	const uint64_t *in = args->scalarInput;

	if (in[0] == 1 && in[1] < 256)
		PDHIDPushKeyboardEvent((UInt8)in[1], in[2] != 0);
	else if (in[0] == 2)
		PDHIDPushMouseEvent(fb->mouseIndex(), (UInt8)in[1], (SInt8)(int64_t)in[2], (SInt8)(int64_t)in[3],
		    (SInt8)(int64_t)in[4]);
	else
		return kIOReturnBadArgument;
	return kIOReturnSuccess;
}

IOReturn
PDVirtualFramebufferClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
	static const IOExternalMethodDispatch presenter[kPresenterMethodCount] = {
		{ &PDVirtualFramebufferClient::getInfo, 0, 0, 4, 0 },
		{ &PDVirtualFramebufferClient::flip, 1, 0, 2, 0 },
	};
	static const IOExternalMethodDispatch streamer[kStreamerMethodCount] = {
		{ &PDVirtualFramebufferClient::streamInfo, 0, 0, 5, 0 },
		{ &PDVirtualFramebufferClient::wait, 2, 0, 2, 0 },
		{ &PDVirtualFramebufferClient::input, 5, 0, 0, 0 },
	};
	const IOExternalMethodDispatch *table = fType == kPresenterType ? presenter : streamer;
	uint32_t count = fType == kPresenterType ? kPresenterMethodCount : kStreamerMethodCount;

	if (selector >= count)
		return kIOReturnBadArgument;
	return IOUserClient::externalMethod(selector, args,
		(IOExternalMethodDispatch *)&table[selector], this, reference);
}

IOReturn
PDVirtualFramebuffer::newUserClient(task_t owningTask, void *securityID, UInt32 type,
	IOUserClient **handler)
{
	if (type != kPresenterType && type != kStreamerType)
		return IOFramebuffer::newUserClient(owningTask, securityID, type, handler);

	PDVirtualFramebufferClient *client = OSTypeAlloc(PDVirtualFramebufferClient);
	if (client == NULL)
		return kIOReturnNoMemory;
	if (!client->initWithTask(owningTask, securityID, type) || !client->attach(this) ||
	    !client->start(this)) {
		client->detach(this);
		client->release();
		return kIOReturnError;
	}
	*handler = client;
	return kIOReturnSuccess;
}
