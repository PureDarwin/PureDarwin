// SPDX-License-Identifier: MIT
// the IOFramebuffer face of a drm kext, one per lit display, its aperture is that display's pinned
// scanout buffer. the console, neuswc and WindowServer draw straight into what KMS shows

#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <kern/thread_call.h>
#include <pexpert/pexpert.h>

#include "pd_drm_fb.h"

extern "C" const char *pd_driver_last_error(void);



// display mode ids are the client's mode indices plus one
#define kDepth		0

class PDDRMFramebuffer : public IOFramebuffer
{
	OSDeclareDefaultStructors(PDDRMFramebuffer);

	struct pd_drm_fb_info fInfo;
	IODisplayModeID fMode;
	// which of the display client's displays this is, display 0 makes the others' framebuffers
	int fDisplay;
	IOService *fProvider;
	IOFBInterruptProc fConnectProc;
	OSObject *fConnectTarget;
	void *fConnectRef;

	thread_call_t fSwitchCall;
	int fSwitchTo;

	static void displayChanged(void *context);
	static void switchTest(thread_call_param_t self, thread_call_param_t);
	void spawnDisplays();

public:
	int display() const { return fDisplay; }
	bool start(IOService *provider) override;
	void stop(IOService *provider) override;
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
};

OSDefineMetaClassAndStructors(PDDRMFramebuffer, IOFramebuffer);

// with debug=0x8 kprintf reaches serial, the screen and the log buffer by itself,
// IOLog as well would print every line twice
static bool
pd_pddrm_fb_log_kprintf(void)
{
	int debug = 0;
	return PE_parse_boot_argn("debug", &debug, sizeof(debug)) && (debug & 0x8) != 0;
}
#define PDDRM_FB_LOG(fmt, ...) do { if (pd_pddrm_fb_log_kprintf()) kprintf(fmt, ##__VA_ARGS__); else IOLog(fmt, ##__VA_ARGS__); } while (0)

bool
PDDRMFramebuffer::start(IOService *provider)
{
	OSNumber *index = OSDynamicCast(OSNumber, getProperty("PDDRMDisplay"));

	fDisplay = index != NULL ? (int)index->unsigned32BitValue() : 0;
	fProvider = provider;
	if (pd_drm_fb == NULL) {
		PDDRM_FB_LOG("PDDRMFramebuffer: the drm kext has no display client\n");
		if (provider->getProvider() != NULL)
			provider->getProvider()->setProperty("PDDRMStatus", "no display client (the gpu has no heads)");
		return false;
	}
	// display 0 probes and lights them all, the others find theirs already lit
	int error = fDisplay == 0 ? pd_drm_fb->setup(&fInfo) : pd_drm_fb->on_screen(fDisplay, &fInfo);
	if (error != 0 && fDisplay != 0) {
		PDDRM_FB_LOG("PDDRMFramebuffer: display %d is not lit (%d)\n", fDisplay, error);
		return false;
	}
	if (error != 0) {
		PDDRM_FB_LOG("PDDRMFramebuffer: no display lit (%d)\n", error);
		IOService *gpu = provider->getProvider();
		char why[224];
		// the display client returns ENODEV when no connector reports a display
		if (error == -19 || error == 19)
			snprintf(why, sizeof(why), "%s", pd_driver_last_error());
		else
			snprintf(why, sizeof(why), "modeset failed (%d): %s", error, pd_driver_last_error());
		if (gpu != NULL)
			gpu->setProperty("PDDRMStatus", why);
		return false;
	}
	fMode = 1;
	pd_drm_fb->changed = displayChanged;
	pd_drm_fb->changed_context[fDisplay] = this;
	if (!IOFramebuffer::start(provider))
		return false;
	PDDRM_FB_LOG("PDDRMFramebuffer: display %d %ux%u@%u pitch %u at phys 0x%llx, %d modes\n", fDisplay,
		fInfo.width, fInfo.height, fInfo.refresh, fInfo.pitch, fInfo.phys, pd_drm_fb->mode_count(fDisplay));
	registerService();
	if (fDisplay == 0)
		spawnDisplays();

	// IOGOPFramebuffer stays registered: terminating an IOFramebuffer from another driver's
	// start panics in IOFramebuffer::terminate, and neuswc picks this one over it

	// bring-up: pdfb_switch=WWWWHHHH switches to that mode 40 s after start,
	// through setDisplayMode, then has IOGraphics resense the display
	int want = 0;
	if (PE_parse_boot_argn("pdfb_switch", &want, sizeof(want)) && want > 0) {
		struct pd_drm_fb_mode mode;
		for (int i = 0; i < pd_drm_fb->mode_count(fDisplay); i++) {
			if (pd_drm_fb->get_mode(fDisplay, i, &mode) == 0 && (int)mode.width == want / 10000 &&
			    (int)mode.height == want % 10000) {
				fSwitchTo = i + 1;
				break;
			}
		}
		if (fSwitchTo != 0) {
			uint64_t deadline;
			fSwitchCall = thread_call_allocate(switchTest, this);
			clock_interval_to_deadline(40, kSecondScale, &deadline);
			thread_call_enter_delayed(fSwitchCall, deadline);
		}
	}
	return true;
}

void
PDDRMFramebuffer::switchTest(thread_call_param_t param, thread_call_param_t)
{
	PDDRMFramebuffer *self = (PDDRMFramebuffer *)param;

	IOLog("PDDRMFramebuffer: switch test to mode %d\n", self->fSwitchTo);
	if (self->setDisplayMode(self->fSwitchTo, kDepth) == kIOReturnSuccess &&
	    self->fConnectProc != NULL)
		self->fConnectProc(self->fConnectTarget, self->fConnectRef);
}

void
PDDRMFramebuffer::stop(IOService *provider)
{
	if (fSwitchCall != NULL) {
		thread_call_cancel_wait(fSwitchCall);
		thread_call_free(fSwitchCall);
		fSwitchCall = NULL;
	}
	if (pd_drm_fb != NULL) {
		pd_drm_fb->changed_context[fDisplay] = NULL;
		// the buffers belong to the display client, display 0 hands them all back
		if (fDisplay == 0)
			pd_drm_fb->release();
	}
	IOFramebuffer::stop(provider);
}

// display 0 makes a framebuffer for every lit display that has none yet, on the same gpu
void
PDDRMFramebuffer::spawnDisplays()
{
	for (int i = 1; i < pd_drm_fb->display_count() && i < PD_DRM_FB_MAX_DISPLAYS; i++) {
		struct pd_drm_fb_info info;

		if (pd_drm_fb->changed_context[i] != NULL || pd_drm_fb->on_screen(i, &info) != 0)
			continue;

		PDDRMFramebuffer *fb = OSTypeAlloc(PDDRMFramebuffer);

		if (fb == NULL || !fb->init()) {
			OSSafeReleaseNULL(fb);
			continue;
		}
		fb->setProperty("PDDRMDisplay", i, 32);
		if (!fb->attach(fProvider))
			PDDRM_FB_LOG("PDDRMFramebuffer: display %d could not attach\n", i);
		else if (!fb->start(fProvider)) {
			PDDRM_FB_LOG("PDDRMFramebuffer: display %d did not start\n", i);
			fb->detach(fProvider);
		}
		fb->release();
	}
}

// the client re-probed after a connector change and lit the preferred mode
void
PDDRMFramebuffer::displayChanged(void *context)
{
	PDDRMFramebuffer *self = (PDDRMFramebuffer *)context;
	struct pd_drm_fb_mode mode;

	if (pd_drm_fb->on_screen(self->fDisplay, &self->fInfo) != 0) {
		bzero(&self->fInfo, sizeof(self->fInfo));
		IOLog("PDDRMFramebuffer: display %d went dark\n", self->fDisplay);
	} else if (pd_drm_fb->get_mode(self->fDisplay, 0, &mode) == 0) {
		IOLog("PDDRMFramebuffer: display %d changed, now %ux%u@%u\n", self->fDisplay, mode.width,
			mode.height, mode.refresh);
	}
	self->fMode = 1;
	if (self->fConnectProc != NULL)
		self->fConnectProc(self->fConnectTarget, self->fConnectRef);
	// a monitor that was plugged in gets its framebuffer
	if (self->fDisplay == 0)
		self->spawnDisplays();
}

IOReturn
PDDRMFramebuffer::registerForInterruptType(IOSelect interruptType, IOFBInterruptProc proc,
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
PDDRMFramebuffer::unregisterInterrupt(void *interruptRef)
{
	if (interruptRef != (void *)&fConnectProc)
		return kIOReturnBadArgument;
	fConnectProc = NULL;
	return kIOReturnSuccess;
}

IOReturn
PDDRMFramebuffer::enableController()
{
	return kIOReturnSuccess;
}

const char *
PDDRMFramebuffer::getPixelFormats()
{
	return IO32BitDirectPixels;
}

IOReturn
PDDRMFramebuffer::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth)
{
	*displayMode = fMode;
	*depth = kDepth;
	return kIOReturnSuccess;
}

IOReturn
PDDRMFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
	if (depth != kDepth || displayMode < 1 || displayMode > pd_drm_fb->mode_count(fDisplay))
		return kIOReturnUnsupportedMode;
	if (displayMode == fMode)
		return kIOReturnSuccess;

	struct pd_drm_fb_info info;
	int error = pd_drm_fb->set_mode(fDisplay, (int)displayMode - 1, &info);
	if (error != 0) {
		IOLog("PDDRMFramebuffer: mode %d failed (%d)\n", (int)displayMode, error);
		return kIOReturnError;
	}
	fInfo = info;
	fMode = displayMode;
	IOLog("PDDRMFramebuffer: mode %d is %ux%u@%u at phys 0x%llx\n", (int)displayMode,
		fInfo.width, fInfo.height, fInfo.refresh, fInfo.phys);
	return kIOReturnSuccess;
}

IODeviceMemory *
PDDRMFramebuffer::getApertureRange(IOPixelAperture aperture)
{
	if (aperture != kIOFBSystemAperture)
		return NULL;
	return IODeviceMemory::withRange(fInfo.phys, (IOPhysicalLength)fInfo.pitch * fInfo.height);
}

IODeviceMemory *
PDDRMFramebuffer::getVRAMRange(void)
{
	return getApertureRange(kIOFBSystemAperture);
}

IOReturn
PDDRMFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode,
	IODisplayModeInformation *info)
{
	struct pd_drm_fb_mode mode;

	if (pd_drm_fb->get_mode(fDisplay, (int)displayMode - 1, &mode) != 0)
		return kIOReturnUnsupportedMode;
	bzero(info, sizeof(*info));
	info->nominalWidth = mode.width;
	info->nominalHeight = mode.height;
	info->refreshRate = mode.refresh << 16;
	info->maxDepthIndex = kDepth;
	return kIOReturnSuccess;
}

UInt64
PDDRMFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
	return 0;
}

IOReturn
PDDRMFramebuffer::getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
	IOPixelAperture aperture, IOPixelInformation *info)
{
	struct pd_drm_fb_mode mode;

	if (aperture != kIOFBSystemAperture || depth != kDepth ||
	    pd_drm_fb->get_mode(fDisplay, (int)displayMode - 1, &mode) != 0)
		return kIOReturnUnsupportedMode;

	// XRGB8888, which is 32 bit direct pixels in memory order b, g, r, x. The
	// client's buffers are packed, so the pitch follows from the width.
	bzero(info, sizeof(*info));
	info->activeWidth = mode.width;
	info->activeHeight = mode.height;
	info->bytesPerRow = mode.width * 4;
	// the lit mode draws in the area all displays share, which can be smaller than the mode
	if (displayMode == fMode) {
		info->activeWidth = fInfo.width;
		info->activeHeight = fInfo.height;
		info->bytesPerRow = fInfo.pitch;
	}
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
PDDRMFramebuffer::getDisplayModes(IODisplayModeID *allDisplayModes)
{
	int count = pd_drm_fb->mode_count(fDisplay);

	for (int i = 0; i < count; i++)
		allDisplayModes[i] = i + 1;
	return kIOReturnSuccess;
}

IOItemCount
PDDRMFramebuffer::getDisplayModeCount(void)
{
	return pd_drm_fb->mode_count(fDisplay);
}

// the presenter (type kPDDRMPresenterType): a compositor draws into the mapped back buffer
// of a flip pair (memory types 0 and 1) and flips it at the next vblank, which stops tearing
#define kPDDRMPresenterType	'pdpf'

enum {
	kPDDRMPresenterGetInfo = 0,	// out: width, height, pitch, buffer count
	kPDDRMPresenterFlip = 1,	// in: buffer, out: vblank sequence, time in ns
	kPDDRMPresenterExport = 2,	// in: buffer, out: a dma-buf fd of the caller
	kPDDRMPresenterMethodCount
};

class PDDRMPresenter : public IOUserClient
{
	OSDeclareDefaultStructors(PDDRMPresenter);

	PDDRMFramebuffer *fFramebuffer;

	static IOReturn getInfo(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn flip(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn exportBuffer(OSObject *target, void *reference, IOExternalMethodArguments *args);

public:
	bool start(IOService *provider) override;
	IOReturn clientClose(void) override;
	IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
		IOMemoryDescriptor **memory) override;
	IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
		IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) override;
};

OSDefineMetaClassAndStructors(PDDRMPresenter, IOUserClient);

bool
PDDRMPresenter::start(IOService *provider)
{
	fFramebuffer = OSDynamicCast(PDDRMFramebuffer, provider);
	if (fFramebuffer == NULL || !IOUserClient::start(provider))
		return false;
	IOLog("PDDRMFramebuffer: presenter opened\n");
	return true;
}

// whatever the presenter left on screen, the console's buffer comes back
IOReturn
PDDRMPresenter::clientClose(void)
{
	uint64_t sequence, ns;

	if (pd_drm_fb != NULL)
		pd_drm_fb->flip(fFramebuffer->display(), 0, &sequence, &ns);
	terminate();
	return kIOReturnSuccess;
}

IOReturn
PDDRMPresenter::clientMemoryForType(UInt32 type, IOOptionBits *options,
	IOMemoryDescriptor **memory)
{
	struct pd_drm_fb_info info;
	uint64_t phys;

	if (pd_drm_fb == NULL || type > 1 || pd_drm_fb->on_screen(fFramebuffer->display(), &info) != 0 ||
	    pd_drm_fb->buffer(fFramebuffer->display(), (int)type, &phys) != 0)
		return kIOReturnBadArgument;
	*memory = IODeviceMemory::withRange(phys, (IOPhysicalLength)info.pitch * info.height);
	*options = kIOMapWriteCombineCache;
	return *memory != NULL ? kIOReturnSuccess : kIOReturnNoMemory;
}

IOReturn
PDDRMPresenter::getInfo(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
	struct pd_drm_fb_info info;
	int display = ((PDDRMPresenter *)target)->fFramebuffer->display();

	if (pd_drm_fb->on_screen(display, &info) != 0)
		return kIOReturnNotReady;
	args->scalarOutput[0] = info.width;
	args->scalarOutput[1] = info.height;
	args->scalarOutput[2] = info.pitch;
	args->scalarOutput[3] = 2;
	return kIOReturnSuccess;
}

IOReturn
PDDRMPresenter::flip(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
	uint64_t sequence, ns;

	static uint64_t sFlips, sLastNs;

	int display = ((PDDRMPresenter *)target)->fFramebuffer->display();

	if (pd_drm_fb->flip(display, (int)args->scalarInput[0], &sequence, &ns) != 0)
		return kIOReturnError;
	// a sign of life: the first flips, then one line per thousand
	if (++sFlips <= 3 || sFlips % 1000 == 0)
		IOLog("PDDRMFramebuffer: flip %llu to buffer %llu at vblank %llu, %llu us after the last\n",
			sFlips, args->scalarInput[0], sequence, sLastNs != 0 ? (ns - sLastNs) / 1000 : 0);
	sLastNs = ns;
	args->scalarOutput[0] = sequence;
	args->scalarOutput[1] = ns;
	return kIOReturnSuccess;
}

IOReturn
PDDRMPresenter::exportBuffer(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
	int fd;

	if (pd_drm_fb->export_fd == NULL || args->scalarInput[0] > 1)
		return kIOReturnUnsupported;
	int ret = pd_drm_fb->export_fd(((PDDRMPresenter *)target)->fFramebuffer->display(),
		(int)args->scalarInput[0], &fd);
	if (ret != 0) {
		IOLog("PDDRMFramebuffer: exporting buffer %llu failed (%d)\n", args->scalarInput[0], ret);
		return kIOReturnError;
	}
	args->scalarOutput[0] = (uint64_t)fd;
	return kIOReturnSuccess;
}

IOReturn
PDDRMPresenter::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
	static const IOExternalMethodDispatch methods[kPDDRMPresenterMethodCount] = {
		{ &PDDRMPresenter::getInfo, 0, 0, 4, 0 },
		{ &PDDRMPresenter::flip, 1, 0, 2, 0 },
		{ &PDDRMPresenter::exportBuffer, 1, 0, 1, 0 },
	};

	if (selector >= kPDDRMPresenterMethodCount || pd_drm_fb == NULL)
		return kIOReturnBadArgument;
	return IOUserClient::externalMethod(selector, args,
		(IOExternalMethodDispatch *)&methods[selector], this, reference);
}

IOReturn
PDDRMFramebuffer::newUserClient(task_t owningTask, void *securityID, UInt32 type,
	IOUserClient **handler)
{
	if (type != kPDDRMPresenterType)
		return IOFramebuffer::newUserClient(owningTask, securityID, type, handler);

	PDDRMPresenter *client = OSTypeAlloc(PDDRMPresenter);
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
