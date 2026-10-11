// IOSurfaceRoot and its user client: surfaces, scanout, traps and the external method table

#include "PDIOSurface.h"
#include "../PDSun60iDisplay/pd_display.h"



OSData *sShaders[PD_MAX_SHADERS];	// every function ever registered, by a global key
static uint32_t sShaderHash[PD_MAX_SHADERS];
static IOLock *sShaderLock;
uint32_t sShaderN;

// the key (+ 1) of a function's metallib, adding it when no client registered the same bytes before,
// 0 when the table is full. Language Chooser, WindowServer and Setup Assistant share Core Animation's shaders
uint32_t
pdShaderRegister(const uint8_t *b, uint32_t len)
{
	uint32_t hash = 2166136261u, key = 0;
	static bool told;

	for (uint32_t i = 0; i < len; i++) {
		hash = (hash ^ b[i]) * 16777619u;
	}
	if (sShaderLock == NULL) {
		IOLock *l = IOLockAlloc();

		if (!OSCompareAndSwapPtr(NULL, l, (void * volatile *)&sShaderLock))
			IOLockFree(l);
	}
	IOLockLock(sShaderLock);
	for (uint32_t k = 0; k < sShaderN; k++) {
		if (sShaderHash[k] == hash && sShaders[k]->getLength() == len && memcmp(sShaders[k]->getBytesNoCopy(), b, len) == 0) {
			key = k + 1;
			break;
		}
	}
	if (key == 0 && sShaderN < PD_MAX_SHADERS) {
		OSData *copy = OSData::withBytes(b, len);

		if (copy != NULL) {
			sShaders[sShaderN] = copy;
			sShaderHash[sShaderN] = hash;
			key = ++sShaderN;
			// the table used to stop at 256: every 256 more says how far past that a boot gets
			if ((sShaderN & 0xff) == 0)
				IOLog("PDIOSurface: shader table holds %u functions\n", sShaderN);
		}
	} else if (key == 0 && !told) {
		told = true;
		IOLog("PDIOSurface: shader table full (%u functions), new functions are dropped\n", sShaderN);
	}
	IOLockUnlock(sShaderLock);
	return key;
}

OSDefineMetaClassAndStructors(IOSurfaceRoot, IOService);
OSDefineMetaClassAndStructors(IOSurfaceRootUserClient, IOUserClient);

// a notification queue that can announce entries enqueued before its port was registered:
// the stock queue only messages on empty to non-empty, so those entries were never announced
class PDNotifyQueue : public IOSharedDataQueue {
	OSDeclareDefaultStructors(PDNotifyQueue);
public:
	void notify(void) { sendDataAvailableNotification(); }
};
OSDefineMetaClassAndStructors(PDNotifyQueue, IOSharedDataQueue);

// this lock protects completed scanout pixels and both back buffers, and GPU work stays outside it.
static IOLock *sScanoutLock;
static thread_call_t sCursorCall;
static void pdCursorPresent(thread_call_param_t, thread_call_param_t);

bool
IOSurfaceRoot::start(IOService *provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	sScanoutLock = IOLockAlloc();
	sCursorCall = thread_call_allocate(&pdCursorPresent, NULL);
	if (!sScanoutLock || !sCursorCall) {
		if (sScanoutLock) IOLockFree(sScanoutLock);
		if (sCursorCall) thread_call_free(sCursorCall);
		sScanoutLock = NULL;
		sCursorCall = NULL;
		return false;
	}
	// IONameMatch compares the registry name,
	// so set it explicitly instead of relying on whatever the class happens to be named
	setName("IOSurfaceRoot");
	registerService();
	PDDisplayNub::publish(this);
	AppleARMIODevice::publishGPU(this);
	PDShaderBridge::publish(this);
	return true;
}

IOReturn
IOSurfaceRoot::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}

// Offsets from AppleParavirtGPUMetalIOGPUFamily's APVFeatures and capability type encodings
enum {
	kAPVCapsSize = 224,
	kAPVFeaturesSize = 60,
	kAPVMaxFIFOCount = 20,
	kAPVS8ByteCount = 24,
};
// The features served: every byte but 1 (unsupported) and 17-19. supportsInfoEncoder is 29,
// supportsCPUBuffers 52 (the plugin's APVFeatures string)
static const uint64_t kAPVFeatureMask = 0x0FFFFFFFF001FFFDULL;

// WindowServer registers its notification ports on one framebuffer client and submits
// swaps on another, so the ports are shared across the framebuffer's clients by type
static mach_port_t sFBNotifyPorts[8];
// sel 72 follows the port registration with (callback, refcon, type, 0): the callout
// the client expects in the frame notification message
static mach_vm_address_t sFBCallback[8];
static io_user_reference_t sFBRefcon[8];
static task_t sFBTask;			// the task that registered the ports, for the 64-bit async reference
static IOUserClient *sFBOwner;		// the client that registered them: WindowServer opens and closes others
static uint64_t sFBFrames;		// vsync notifications sent so far
static thread_call_t sFBVsync;
static uint32_t sSwapID;		// last id handed out at sel 4
static OSData *sGammaTable;		// set by sel 17, read back by sel 27
static uint64_t sPendingSwap;		// submitted at sel 5, not yet reported done
static uint64_t sPresentedSwap;		// last swap shown at a vblank
static uint32_t sSwapsDone;
static uint32_t sSurfaceNext = 16;	// ids start where Apple's did on the reference machine
static uint32_t sSurfaceLive;
enum { kPDVsyncHz = 60 };

// Reports a swap done on the type 1 frame port, through its own sel 72 callback, in six words
void
IOSurfaceRootUserClient::completeSwap(uint64_t id)
{
	io_user_reference_t res[8] = { id, 0, 0, 0, 0, 0, 0, 0 };
	OSAsyncReference64 ref;

	if (sFBNotifyPorts[1] == MACH_PORT_NULL || sFBTask == TASK_NULL || sFBCallback[1] == 0) {
		return;
	}
	res[1] = mach_absolute_time();
	// The task variant marks the reference 64-bit. Without that the reply goes out
	// in the 32-bit layout with every pointer truncated
	bzero(ref, sizeof(ref));
	setAsyncReference64(ref, sFBNotifyPorts[1], sFBCallback[1], sFBRefcon[1], sFBTask);
	sendAsyncResult64(ref, kIOReturnSuccess, res, 6);
	sSwapsDone++;
}

// One notification on a registered port: the port type's own callback and refcon, then
// the words. The task variant marks the reference 64-bit, without it every pointer is truncated
IOReturn
IOSurfaceRootUserClient::sendFrameNotification(uint32_t type, const io_user_reference_t *words, uint32_t count)
{
	OSAsyncReference64 ref;

	if (type >= 8 || sFBNotifyPorts[type] == MACH_PORT_NULL || sFBCallback[type] == 0 || sFBTask == TASK_NULL) {
		return kIOReturnNotReady;
	}
	bzero(ref, sizeof(ref));
	setAsyncReference64(ref, sFBNotifyPorts[type], sFBCallback[type], sFBRefcon[type], sFBTask);
	return sendAsyncResult64(ref, kIOReturnSuccess, (io_user_reference_t *)words, count > 8 ? 8 : count);
}

// Periodic vsync on port type 3. Words: frame count, now, 0s

static void
vsyncFired(thread_call_param_t unused0, thread_call_param_t unused1)
{
	io_user_reference_t words[8] = { ++sFBFrames, mach_absolute_time(), 0, 0, 0, 0, 0, 0 };
	uint64_t deadline;

	// the vblank keeps running with or without a client: swaps are paced by it, not by port 3
	IOSurfaceRootUserClient::sendFrameNotification(3, words, 6);
	// releasing a drawable while work is pending permits producer reuse mid-frame.
	if (sPendingSwap == 0 && pdAsyncIdle())
		pdFireDueListeners(sFBFrames);
	// a heartbeat every 10 s: whether WindowServer still swaps while the screen looks frozen
	if (sFBFrames % (10 * kPDVsyncHz) == 0)
		IOLog("PDIOSurface: vblank %llu swaps done %u last id %u presented %llu pending %llu surfaces %u\n",
		    (unsigned long long)sFBFrames, sSwapsDone, sSwapID, (unsigned long long)sPresentedSwap,
		    (unsigned long long)sPendingSwap, sSurfaceLive);
	clock_interval_to_deadline(1000000 / kPDVsyncHz, kMicrosecondScale, &deadline);
	thread_call_enter_delayed(sFBVsync, deadline);
}

// pdgputrace=NAME logs every selector and trap of processes whose name starts with NAME
static char sGpuTraceName[17];
static int sGpuTraceState;

static bool
pdGpuTraced(char *name)
{
	if (sGpuTraceState == 0)
		sGpuTraceState = PE_parse_boot_argn("pdgputrace", sGpuTraceName, sizeof(sGpuTraceName)) ? 2 : 1;
	if (sGpuTraceState != 2)
		return false;
	proc_selfname(name, 17);
	return strncmp(name, sGpuTraceName, strlen(sGpuTraceName)) == 0;
}

IOReturn
IOSurfaceRootUserClient::open(IOService *provider, task_t owningTask, void *securityID,
    UInt32 type, IOUserClient **handler)
{
	IOSurfaceRootUserClient *uc = OSTypeAlloc(IOSurfaceRootUserClient);

	if (uc == NULL || !uc->initWithTask(owningTask, securityID, type)) {
		OSSafeReleaseNULL(uc);
		return kIOReturnNoMemory;
	}
	char name[17];

	uc->fTag = provider->getName();
	uc->fType = type;
	uc->fTraced = pdGpuTraced(name);
	uc->fTask = owningTask;
	uc->fPendLock = IOLockAlloc();
	uc->fSubmitLock = IOLockAlloc();
	if (!uc->attach(provider)) {
		uc->release();
		return kIOReturnError;
	}
	if (!uc->start(provider)) {
		uc->detach(provider);
		uc->release();
		return kIOReturnError;
	}
	*handler = uc;
	return kIOReturnSuccess;
}

// Traps 0-15 and 256-263 each get a slot of their own, others share fTrap
IOExternalTrap *
IOSurfaceRootUserClient::getTargetAndTrapForIndex(IOService **targetP, UInt32 index)
{
	static const IOTrap sFuncs[kPDTrapSlots] = {
		(IOTrap)&IOSurfaceRootUserClient::trapN<0>, (IOTrap)&IOSurfaceRootUserClient::trapN<1>,
		(IOTrap)&IOSurfaceRootUserClient::trapN<2>, (IOTrap)&IOSurfaceRootUserClient::trapN<3>,
		(IOTrap)&IOSurfaceRootUserClient::trapN<4>, (IOTrap)&IOSurfaceRootUserClient::trapN<5>,
		(IOTrap)&IOSurfaceRootUserClient::trapN<6>, (IOTrap)&IOSurfaceRootUserClient::trapN<7>,
		(IOTrap)&IOSurfaceRootUserClient::trapN<8>, (IOTrap)&IOSurfaceRootUserClient::trapN<9>,
		(IOTrap)&IOSurfaceRootUserClient::trapN<10>, (IOTrap)&IOSurfaceRootUserClient::trapN<11>,
		(IOTrap)&IOSurfaceRootUserClient::trapN<12>, (IOTrap)&IOSurfaceRootUserClient::trapN<13>,
		(IOTrap)&IOSurfaceRootUserClient::trapN<14>, (IOTrap)&IOSurfaceRootUserClient::trapN<15>,
		(IOTrap)&IOSurfaceRootUserClient::trapN<16>, (IOTrap)&IOSurfaceRootUserClient::trapN<17>,
		(IOTrap)&IOSurfaceRootUserClient::trapN<18>, (IOTrap)&IOSurfaceRootUserClient::trapN<19>,
		(IOTrap)&IOSurfaceRootUserClient::trapN<20>, (IOTrap)&IOSurfaceRootUserClient::trapN<21>,
		(IOTrap)&IOSurfaceRootUserClient::trapN<22>, (IOTrap)&IOSurfaceRootUserClient::trapN<23>,
	};
	UInt32 slot = index < 16 ? index : (index >= 256 && index < 264 ? index - 256 + 16 : kPDTrapSlots);

	*targetP = this;
	if (slot < kPDTrapSlots) {
		fTraps[slot].object = this;
		fTraps[slot].func = sFuncs[slot];
		return &fTraps[slot];
	}
	fTrapIndex = index;
	fTrap.object = this;
	fTrap.func = (IOTrap)&IOSurfaceRootUserClient::trap;
	return &fTrap;
}

IOReturn
IOSurfaceRootUserClient::trap(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
	return trapDispatch(fTrapIndex, p1, p2, p3, p4, p5, p6);
}

IOReturn
IOSurfaceRootUserClient::trapDispatch(UInt32 trapIndex, void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
	char name[17];
	IOReturn ret;

	if (trapIndex != 0 || fSubmitLock == NULL) {
		ret = trapIndexed(trapIndex, p1, p2, p3, p4, p5, p6);
	} else {
		IOLockLock(fSubmitLock);
		ret = trapIndexed(trapIndex, p1, p2, p3, p4, p5, p6);
		IOLockUnlock(fSubmitLock);
	}
	if (pdGpuTraced(name))
		IOLog("PD-gputrace: %s[%d] %s trap %u (0x%llx 0x%llx 0x%llx) -> 0x%x\n", name, proc_selfpid(), fTag,
		    trapIndex, (unsigned long long)(uintptr_t)p1, (unsigned long long)(uintptr_t)p2,
		    (unsigned long long)(uintptr_t)p3, ret);
	return ret;
}
#define PD_SURF_REGION   0x200000
#define PD_SURF_RO_BASE  0x121000
#define PD_SURF_RO_SIZE  0xb8
#define PD_SURF_RW_BASE  0x125000
#define PD_SURF_RW_SIZE  0x40
#define PD_DIRTY_BYTES   0x10000
PDSurface sSurfaces[PD_SURF_MAX];

// pixel format of a live surface, 0 when there is none
uint32_t
pdSurfFormat(uint32_t sid)
{
	return sid < PD_SURF_MAX && sSurfaces[sid].md != NULL ? sSurfaces[sid].format : 0;
}

// row pitch of a live surface, 0 when there is none
uint64_t
pdSurfBpr(uint32_t sid)
{
	return sid < PD_SURF_MAX && sSurfaces[sid].md != NULL ? sSurfaces[sid].bytesPerRow : 0;
}
static uint32_t sSurfaceSeed = 0x17;
static IOBufferMemoryDescriptor *sSurfRegion;

// The shared region, mapped once per client
static uint64_t
pdSurfRegionAddress(IOSurfaceRootUserClient *uc, task_t task, IOMemoryMap **slot)
{
	if (sSurfRegion == NULL) {
		sSurfRegion = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
		    kIODirectionInOut | kIOMemoryKernelUserShared, PD_SURF_REGION, page_size);
		if (sSurfRegion != NULL) {
			bzero(sSurfRegion->getBytesNoCopy(), PD_SURF_REGION);
		}
	}
	if (sSurfRegion == NULL) {
		return 0;
	}
	if (*slot == NULL) {
		*slot = sSurfRegion->createMappingInTask(task, 0, kIOMapAnywhere);
	}
	return *slot != NULL ? (*slot)->getAddress() : 0;
}

// The 3176 byte reply for a surface, mapped into this client
static IOReturn
pdSurfReply(IOSurfaceRootUserClient *uc, task_t task, IOMemoryMap **regionSlot, OSArray **maps,
    uint32_t id, uint8_t *out, uint32_t outSize)
{
	PDSurface *sf = &sSurfaces[id];
	uint64_t region = pdSurfRegionAddress(uc, task, regionSlot), *w = (uint64_t *)out;
	IOMemoryMap *map;

	if (id >= PD_SURF_MAX || sf->md == NULL || region == 0 || outSize < 0x80) {
		return kIOReturnBadArgument;
	}
	map = sf->md->createMappingInTask(task, 0, kIOMapAnywhere);
	if (map == NULL) {
		return kIOReturnNoMemory;
	}
	if (*maps == NULL) {
		*maps = OSArray::withCapacity(16);
	}
	if (*maps != NULL) {
		(*maps)->setObject(map);
	}
	{
		uint8_t *ro = (uint8_t *)sSurfRegion->getBytesNoCopy() + PD_SURF_RO_BASE + id * PD_SURF_RO_SIZE;
		uint64_t one = 1, mark = 0x100001000ULL;

		memcpy(ro + 0x20, &one, 8);
		memcpy(ro + 0x28, &mark, 8);
	}
	bzero(out, outSize);
	w[0] = map->getAddress();
	w[1] = region + PD_SURF_RO_BASE + id * PD_SURF_RO_SIZE;
	w[2] = region + PD_SURF_RW_BASE + id * PD_SURF_RW_SIZE;
	w[3] = id;
	w[4] = sf->allocSize;
	w[5] = sf->width;
	w[6] = sf->height;
	w[7] = sf->bytesPerRow;
	w[9] = sf->format;
	w[10] = sf->allocSize;
	w[12] = (sf->bytesPerElement & 0xff) | (1ULL << 16) | (1ULL << 24);
	w[15] = 0x0100000000000000ULL | sSurfaceSeed++;
	map->release();
	return kIOReturnSuccess;
}

static uint64_t
pdDictNumber(OSDictionary *d, const char *key, uint64_t dflt)
{
	OSNumber *n = d ? OSDynamicCast(OSNumber, d->getObject(key)) : NULL;

	return n != NULL ? n->unsigned64BitValue() : dflt;
}

// create_surface: a binary serialized property dictionary in
static IOReturn
pdSurfCreate(const uint8_t *in, uint32_t inSize, uint32_t *idOut)
{
	OSString *err = NULL;
	OSObject *obj = OSUnserializeXML((const char *)in, inSize, &err);
	OSDictionary *d = OSDynamicCast(OSDictionary, obj);
	uint32_t id;
	PDSurface *sf;

	if (d == NULL) {
		IOLog("PDIOSurface: IOSurfaceRoot create: cannot unserialize %u bytes (%s)\n", inSize,
		    err ? err->getCStringNoCopy() : "no error");
		OSSafeReleaseNULL(obj);
		OSSafeReleaseNULL(err);
		return kIOReturnBadArgument;
	}
	// a free slot, round robin so a just released id is not handed out again at once
	id = 0;
	for (uint32_t n = 0; n < PD_SURF_MAX - 16; n++) {
		uint32_t at = 16 + (sSurfaceNext - 16 + n) % (PD_SURF_MAX - 16);

		if (sSurfaces[at].md == NULL && sSurfaces[at].refs == 0) {
			id = at;
			break;
		}
	}
	if (id == 0) {
		static bool sFullLogged;

		if (!sFullLogged) {
			sFullLogged = true;
			IOLog("PDIOSurface: surface table full, %u live, creates now fail\n", sSurfaceLive);
		}
		obj->release();
		return kIOReturnNoResources;
	}
	sSurfaceNext = id + 1;
	sf = &sSurfaces[id];
	sf->width = pdDictNumber(d, "IOSurfaceWidth", 0);
	sf->height = pdDictNumber(d, "IOSurfaceHeight", 0);
	sf->bytesPerElement = pdDictNumber(d, "IOSurfaceBytesPerElement", 4);
	sf->bytesPerRow = pdDictNumber(d, "IOSurfaceBytesPerRow", (sf->width * sf->bytesPerElement + 63) & ~63ULL);
	sf->format = pdDictNumber(d, "IOSurfacePixelFormat", 0x42475241);
	sf->allocSize = pdDictNumber(d, "IOSurfaceAllocSize", sf->bytesPerRow * sf->height);
	sf->allocSize = round_page(sf->allocSize ? sf->allocSize : page_size);
	sf->md = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
	    kIODirectionInOut | kIOMemoryKernelUserShared, sf->allocSize, page_size);
	// Fresh memory carries whatever was there before (console text showed through once)
	if (sf->md != NULL) {
		bzero(sf->md->getBytesNoCopy(), sf->md->getLength());
	}
	OSSafeReleaseNULL(sf->props);
	sf->props = d;
	d->retain();
	d->setObject("IOSurfacePixelSizeCastingAllowed", kOSBooleanTrue);
	if (d->getObject("IOSurfaceName") == NULL) {
		char name[24];
		OSString *str;

		snprintf(name, sizeof(name), "pid%d", proc_selfpid());
		str = OSString::withCString(name);
		if (str != NULL) {
			d->setObject("IOSurfaceName", str);
			str->release();
		}
	}
	OSSafeReleaseNULL(sf->values);
	sf->values = OSDictionary::withCapacity(4);
	obj->release();
	if (sf->md == NULL) {
		OSSafeReleaseNULL(sf->props);
		OSSafeReleaseNULL(sf->values);
		return kIOReturnNoMemory;
	}
	bzero(sf->md->getBytesNoCopy(), sf->allocSize);
	sf->refs = 1;
	sSurfaceLive++;
	*idOut = id;
	return kIOReturnSuccess;
}

// one holder lets go (trap 5): the last one frees the memory. Client mappings hold their
// own reference to it, so a task still mapped keeps its pages
static void
pdSurfRelease(uint32_t id)
{
	PDSurface *sf = &sSurfaces[id];

	if (id < 16 || id >= PD_SURF_MAX || sf->md == NULL || sf->refs == 0) return;
	if (--sf->refs != 0) return;

	OSSafeReleaseNULL(sf->md);
	OSSafeReleaseNULL(sf->props);
	OSSafeReleaseNULL(sf->values);
	sf->width = sf->height = sf->bytesPerRow = sf->allocSize = 0;
	sSurfaceLive--;
}

// copy_all_values (sel 10): {u32 id, u64 0, u8 0} in, out through a descriptor:
// 12 bytes {0, 1, 0} then the binary serialized {CreationProperties: creation dict}
static IOReturn
pdSurfCopyValues(uint32_t id, IOMemoryDescriptor *outDesc, uint32_t *outSize)
{
	PDSurface *sf = &sSurfaces[id];
	OSDictionary *wrap;
	OSSerialize *ser;
	uint32_t len;
	uint8_t header[12] = { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 };
	IOReturn kr = kIOReturnNoMemory;

	if (id >= PD_SURF_MAX || sf->md == NULL || sf->props == NULL || outDesc == NULL) {
		return kIOReturnBadArgument;
	}
	wrap = OSDictionary::withCapacity(8);
	ser = OSSerialize::binaryWithCapacity(4096);
	if (wrap != NULL && ser != NULL) {
		wrap->setObject("CreationProperties", sf->props);
		if (sf->values != NULL) {
			OSCollectionIterator *it = OSCollectionIterator::withCollection(sf->values);
			OSObject *k;

			while (it != NULL && (k = it->getNextObject()) != NULL) {
				OSSymbol *sym = OSDynamicCast(OSSymbol, k);

				if (sym != NULL) {
					wrap->setObject(sym, sf->values->getObject(sym));
				}
			}
			OSSafeReleaseNULL(it);
		}
		if (wrap->serialize(ser) && outDesc->prepare() == kIOReturnSuccess) {
			len = ser->getLength();
			if (sizeof(header) + len <= outDesc->getLength()) {
				outDesc->writeBytes(0, header, sizeof(header));
				outDesc->writeBytes(sizeof(header), ser->text(), len);
				*outSize = (uint32_t)sizeof(header) + len;
				kr = kIOReturnSuccess;
			} else {
				kr = kIOReturnNoSpace;
			}
			outDesc->complete();
		}
	}
	OSSafeReleaseNULL(wrap);
	OSSafeReleaseNULL(ser);
	return kr;
}

// Scanout: the console framebuffer QEMU shows (ramfb), mapped once from its physical address
static uint8_t *sScanBase;
static uint32_t sScanW, sScanH, sScanStride, sScanDepth;
static IOMemoryMap *sScanMap;
static IOService *sScanDisplay;
static IOBufferMemoryDescriptor *sScanBuffers[2];
static uint32_t sScanBack;
static uint8_t *sScanTarget, *sScanBasePixels;

static IOReturn
pdScanDisplayCall(const char *fn, void *p1, void *p2, void *p3)
{
	const OSSymbol *sym = OSSymbol::withCString(fn);
	if (!sym)
		return kIOReturnNoMemory;
	IOReturn ret = sScanDisplay->callPlatformFunction(sym, true, p1, p2, p3, NULL);
	sym->release();
	return ret;
}


static bool
pdScanoutOpen(void)
{
	PE_Video console;

	if (sScanBase != NULL) {
		return sScanBasePixels != NULL && (!sScanDisplay || (sScanBuffers[0] && sScanBuffers[1]));
	}
	if (IOService::getPlatform()->getConsoleInfo(&console) != kIOReturnSuccess || console.v_baseAddr == 0 ||
	    console.v_width == 0 || console.v_height == 0) {
		return false;
	}
	sScanW = (uint32_t)console.v_width;
	sScanH = (uint32_t)console.v_height;
	sScanStride = (uint32_t)console.v_rowBytes;
	sScanDepth = (uint32_t)console.v_depth;
	// The platform hands out the physical address of the ramfb buffer here
	{
		uint64_t phys = console.v_baseAddr & ~3ULL;
		IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(phys, sScanStride * sScanH,
		    kIODirectionOut);

		sScanMap = md ? md->map(kIOMapInhibitCache) : NULL;
		OSSafeReleaseNULL(md);
		sScanBase = sScanMap ? (uint8_t *)sScanMap->getVirtualAddress() : NULL;
	}
	if (sScanBase == NULL)
		return false;
	sScanBasePixels = (uint8_t *)IOMalloc((size_t)sScanStride * sScanH);
	if (sScanBasePixels == NULL)
		return false;
	memcpy(sScanBasePixels, sScanBase, (size_t)sScanStride * sScanH);
	OSDictionary *match = IOService::serviceMatching(PD_DISPLAY_CLASS);
	sScanDisplay = match ? IOService::copyMatchingService(match) : NULL;
	OSSafeReleaseNULL(match);
	if (sScanDisplay) {
		for (uint32_t i = 0; i < 2; i++) {
			sScanBuffers[i] = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
			    kIODirectionInOut | kIOMemoryPhysicallyContiguous,
			    (size_t)sScanStride * sScanH, 0x00000000ffffc000ULL);
			if (!sScanBuffers[i] || sScanBuffers[i]->prepare() != kIOReturnSuccess) {
				IOLog("PDIOSurface: cannot allocate scanout back buffers\n");
				return false;
			}
		}
		IOLog("PDIOSurface: complete-frame scanout with hardware page flips\n");
	}
	return true;
}

// a sel 9 texture request ends in a descriptor laid out as the heap query's (u8 type 0x42, u16 pixel
// format, u32 usage, u32 width, u32 height): the pixel format of the one whose size matches, 0 if none
static uint16_t
pdDescFormat(const uint8_t *in, uint32_t len, uint32_t w, uint32_t h)
{
	for (uint32_t at = len >= 0x40 ? len - 0x40 : 0; at + 0xf <= len; at++) {
		uint32_t dw, dh;
		uint16_t pf;

		if (in[at] != 0x42)
			continue;
		memcpy(&dw, in + at + 7, 4);
		memcpy(&dh, in + at + 0xb, 4);
		memcpy(&pf, in + at + 1, 2);
		if (dw == w && dh == h && pf != 0)
			return pf;
	}
	return 0;
}

// fp16 to a clamped 8 bit channel, enough for scanout
static uint8_t
pdHalfToByte(uint16_t h)
{
	uint32_t exp = (h >> 10) & 0x1f, man = h & 0x3ff;
	uint32_t v;

	// negative, denormal and NaN are 0, as a GPU writes NaN to a UNORM channel
	if (h & 0x8000 || exp == 0 || (exp == 31 && man != 0)) {
		return 0;
	}
	if (exp >= 15) {
		// 1.0 and up (exp 15 is [1,2)) clamps to full
		return 255;
	}
	// value = (1 + man/1024) * 2^(exp-15), scaled to 255
	v = ((1024 + man) * 255) >> (10 + 15 - exp);
	return (uint8_t)(v > 255 ? 255 : v);
}

// Copy a surface to the top left of the console as BGRA8, clipped. WindowServer's
// framebuffer is 'RGhA' (64 bit RGBA half), converted per pixel
static void
pdScanoutSurface(const PDSurface *sf)
{
	uint32_t w, h;
	const uint8_t *src;

	if (sf->md == NULL || !pdScanoutOpen() || sScanDepth != 32) {
		return;
	}
	w = (uint32_t)(sf->width < sScanW ? sf->width : sScanW);
	h = (uint32_t)(sf->height < sScanH ? sf->height : sScanH);
	src = (const uint8_t *)sf->md->getBytesNoCopy();
	for (uint32_t y = 0; y < h; y++) {
		uint8_t *dst = sScanTarget + y * sScanStride;
		const uint8_t *row = src + y * sf->bytesPerRow;

		if (sf->format == 0x52476841 && sf->bytesPerElement == 8) {
			for (uint32_t x = 0; x < w; x++) {
				uint16_t px[4];

				memcpy(px, row + x * 8, 8);
				dst[x * 4 + 0] = pdHalfToByte(px[2]);
				dst[x * 4 + 1] = pdHalfToByte(px[1]);
				dst[x * 4 + 2] = pdHalfToByte(px[0]);
				dst[x * 4 + 3] = pdHalfToByte(px[3]);
			}
		} else {
			memcpy(dst, row, w * 4);
		}
	}
}

// Upper layers blend premultiplied "over" what is already on the console, at their
// destination origin (dx, dy), clipped to the screen
static void
pdScanoutLayerOver(const PDSurface *sf, uint32_t dx, uint32_t dy)
{
	uint32_t w, h;
	const uint8_t *src;

	if (sf->md == NULL || !pdScanoutOpen() || sScanDepth != 32 ||
	    dx >= sScanW || dy >= sScanH) {
		return;
	}
	w = (uint32_t)(sf->width < sScanW - dx ? sf->width : sScanW - dx);
	h = (uint32_t)(sf->height < sScanH - dy ? sf->height : sScanH - dy);
	src = (const uint8_t *)sf->md->getBytesNoCopy();
	for (uint32_t y = 0; y < h; y++) {
		uint8_t *dst = sScanTarget + (dy + y) * sScanStride + dx * 4;
		const uint8_t *row = src + y * sf->bytesPerRow;

		for (uint32_t x = 0; x < w; x++) {
			uint32_t c[4], a;       // B G R A, premultiplied

			if (sf->format == 0x52476841 && sf->bytesPerElement == 8) {
				uint16_t px[4];

				memcpy(px, row + x * 8, 8);
				c[0] = pdHalfToByte(px[2]);
				c[1] = pdHalfToByte(px[1]);
				c[2] = pdHalfToByte(px[0]);
				c[3] = pdHalfToByte(px[3]);
			} else if (sf->bytesPerElement == 4) {
				c[0] = row[x * 4 + 0];
				c[1] = row[x * 4 + 1];
				c[2] = row[x * 4 + 2];
				c[3] = row[x * 4 + 3];
			} else {
				return;
			}
			a = c[3];
			if (a == 0) {
				continue;
			}
			for (uint32_t k = 0; k < 3; k++) {
				uint32_t v = c[k] + (dst[x * 4 + k] * (255 - a)) / 255;
				dst[x * 4 + k] = (uint8_t)(v > 255 ? 255 : v);
			}
			dst[x * 4 + 3] = 255;
		}
	}
}

// only the render FIFO updates these immutable snapshots. A cursor-position
// update may show them independently without reading a GPU's mutable surface.
static PDSurface sScanCursor;
static uint32_t sScanCursorX, sScanCursorY;
static uint64_t sCursorPositionGeneration, sCursorDisplayedGeneration;

static bool
pdScanoutFrame(void)
{
	IOBufferMemoryDescriptor *back = sScanDisplay ? sScanBuffers[sScanBack] : NULL;
	sScanTarget = back ? (uint8_t *)back->getBytesNoCopy() : sScanBase;
	memcpy(sScanTarget, sScanBasePixels, (size_t)sScanStride * sScanH);
	if (sScanCursor.md)
		pdScanoutLayerOver(&sScanCursor, sScanCursorX, sScanCursorY);
	if (back) {
		for (size_t off = 0; off < (size_t)sScanStride * sScanH; off += 64)
			__asm__ volatile ("dc cvac, %0" :: "r"(sScanTarget + off) : "memory");
		__asm__ volatile ("dsb sy" ::: "memory");
		IOByteCount span = 0;
		uint64_t phys = back->getPhysicalSegment(0, &span, kIOMemoryMapperNone);
		pd_display_flip flip = {};
		IOReturn ret = pdScanDisplayCall(PD_DISPLAY_FN_FLIP, (void *)(uintptr_t)phys,
		    (void *)(uintptr_t)sScanStride, &flip);
		if (ret == kIOReturnSuccess) {
			sScanBack ^= 1;
		} else {
			IOLog("PDIOSurface: presentation flip failed 0x%x\n", ret);
			return false;
		}
	}
	sCursorDisplayedGeneration = sCursorPositionGeneration;
	return true;
}

static void
pdCursorPresent(thread_call_param_t unused0, thread_call_param_t unused1)
{
	(void)unused0;
	(void)unused1;
	bool retry = false;
	IOLockLock(sScanoutLock);
	if (sCursorDisplayedGeneration != sCursorPositionGeneration && sScanBasePixels &&
	    sScanCursor.md && sScanCursor.width <= 256 && sScanCursor.height <= 256) {
		// normal ordered presents draw the latest position. Only long GPU work
		// needs this independent visual update, it never acknowledges a swap.
		if (pdAsyncRenderingSlow())
			retry = !pdScanoutFrame();
		else
			retry = true;
	}
	IOLockUnlock(sScanoutLock);
	if (retry) {
		uint64_t deadline;
		clock_interval_to_deadline(10000, kMicrosecondScale, &deadline);
		thread_call_enter_delayed(sCursorCall, deadline);
	}
}

// a present stays ordered behind its rendering, including its completion ack.
struct PDPresentJob {
	OSData *desc;
	uint64_t id;
	PDSurface layers[2];
};

static void
pdScanoutSwap(void *argument)
{
	PDPresentJob *job = (PDPresentJob *)argument;
	const uint8_t *desc = (const uint8_t *)job->desc->getBytesNoCopy();
	uint32_t len = job->desc->getLength();
	IOLockLock(sScanoutLock);
	bool ready = pdScanoutOpen();
	if (ready && job->layers[0].md) {
		sScanTarget = sScanBasePixels;
		pdScanoutSurface(&job->layers[0]);
	}
	if (job->layers[1].md) {
		// retaining the producer's descriptor does not make its bytes immutable.
		// Save the completed cursor pixels before a later GPU job can reuse them.
		IOBufferMemoryDescriptor *copy = IOBufferMemoryDescriptor::withBytes(
		    job->layers[1].md->getBytesNoCopy(), job->layers[1].md->getLength(), kIODirectionNone);
		if (copy) {
			OSSafeReleaseNULL(sScanCursor.md);
			sScanCursor = job->layers[1];
			sScanCursor.md = copy;
		} else {
			ready = false;
		}
		if ((job->layers[1].width > 256 || job->layers[1].height > 256) && len >= 0x124) {
			memcpy(&sScanCursorX, desc + 0x11c, 4);
			memcpy(&sScanCursorY, desc + 0x120, 4);
		}
	}
	if (ready)
		ready = pdScanoutFrame();
	if (ready) {
		sPresentedSwap = job->id;
		if (sPendingSwap == job->id)
			sPendingSwap = 0;
	}
	IOLockUnlock(sScanoutLock);
	if (ready)
		IOSurfaceRootUserClient::completeSwap(job->id);
	for (uint32_t i = 0; i < 2; i++)
		OSSafeReleaseNULL(job->layers[i].md);
	job->desc->release();
	IOFree(job, sizeof(*job));
}

static bool
pdQueueSwap(const uint8_t *desc, uint32_t len, uint64_t id)
{
	PDPresentJob *job = (PDPresentJob *)IOMallocZero(sizeof(*job));
	if (!job)
		return false;
	job->desc = OSData::withBytes(desc, len);
	job->id = id;
	if (!job->desc) {
		IOFree(job, sizeof(*job));
		return false;
	}
	for (uint32_t i = 0; i < 2 && len >= 0xa0 + i * 4; i++) {
		uint32_t surface;
		memcpy(&surface, desc + 0x9c + i * 4, 4);
		if (surface >= 16 && surface < PD_SURF_MAX && sSurfaces[surface].md) {
			job->layers[i] = sSurfaces[surface];
			job->layers[i].md->retain();
		}
	}
	uint32_t x = 0, y = 0;
	bool position = len >= 0x124 && job->layers[1].md &&
	    job->layers[1].width <= 256 && job->layers[1].height <= 256;
	if (position) {
		memcpy(&x, desc + 0x11c, 4);
		memcpy(&y, desc + 0x120, 4);
	}
	IOLockLock(sScanoutLock);
	uint64_t previous = sPendingSwap, oldGeneration = sCursorPositionGeneration;
	uint32_t oldX = sScanCursorX, oldY = sScanCursorY;
	sPendingSwap = id;
	if (position && (x != oldX || y != oldY)) {
		sScanCursorX = x;
		sScanCursorY = y;
		sCursorPositionGeneration++;
	}
	// publish the position before the worker can run this job. Both lock orders
	// are scanout then async, the render worker drops its queue lock first.
	if (pdAsyncPerform(pdScanoutSwap, job)) {
		IOLockUnlock(sScanoutLock);
		if (position)
			thread_call_enter(sCursorCall);
		return true;
	}
	sPendingSwap = previous;
	sScanCursorX = oldX;
	sScanCursorY = oldY;
	sCursorPositionGeneration = oldGeneration;
	IOLockUnlock(sScanoutLock);
	for (uint32_t i = 0; i < 2; i++)
		OSSafeReleaseNULL(job->layers[i].md);
	job->desc->release();
	IOFree(job, sizeof(*job));
	return false;
}

// Buffers are shared with the executor, so ring entries need no copy: consuming them
// is catching the read index up. A full ring asserts in addDirtyResource:
void
IOSurfaceRootUserClient::drainDirty(void)
{
	volatile uint32_t *ro;
	uint32_t w;

	if (fDirty == NULL) {
		return;
	}
	ro = (volatile uint32_t *)fDirty->getBytesNoCopy();
	w = ro[0x1000 / 4];
	if (ro[0] != w) {
		ro[0] = w;
	}
}

// one command buffer's record, from trap 0 or a sel 29 batch: {u32 list shmem id, u32 header shmem id, u64 0, u64 block0, u64 block1, 0, 0}.
// its two callback blocks belong to THIS submission, since posting an older pair had WindowServer call a freed block
void
IOSurfaceRootUserClient::submitRecord(uint32_t queue, const uint8_t *rec)
{
	uint32_t ids[2];
	uint64_t blocks[2];

	memcpy(ids, rec, sizeof(ids));
	memcpy(blocks, rec + 0x10, sizeof(blocks));
	// without its blocks a command buffer never hears it completed
	if (blocks[0] == 0 && strcmp(fTag, "AppleParavirtGPU") == 0)
		IOLog("PDIOSurface: %s submit on queue %u has no completion blocks\n", fTag, queue);
	fSubmitBlocks[0] = blocks[0];
	fSubmitBlocks[1] = blocks[1];
	fSubList = fSubHdr = 0;
	if (ids[0] < kShmemIds && ids[1] < kShmemIds && fShmemById[ids[0]] != NULL && fShmemById[ids[1]] != NULL) {
		fSubList = ids[0];
		fSubHdr = ids[1];
	}
	submitAsync(queue);
}

IOReturn
IOSurfaceRootUserClient::trapIndexed(UInt32 trapIndex, void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
{
	drainDirty();

	// IOSurfaceRoot traps 4 then 5 {id} come once per holder's release, 5 drops the reference
	if (trapIndex == 5 && strcmp(fTag, "IOSurfaceRoot") == 0)
		pdSurfRelease((uint32_t)(uintptr_t)p1);
	// Trap 3 is setPurgeableState {resource id, state, u32 *old}: *old gets the state before the
	// call. Resources start NonVolatile (2), KeepCurrent (1) only queries
	if (trapIndex == 3 && strcmp(fTag, "AppleParavirtGPU") == 0 && p3 != NULL) {
		uint32_t rid = (uint32_t)(uintptr_t)p1, want = (uint32_t)(uintptr_t)p2, old = 2;

		uint32_t rs = idSlot(kIdRid, rid);

		old = fRidMap.at(rs) ? fRidMap.at(rs) : 2;
		if (want >= 2 && want <= 4 && (rs = idSlotNew(kIdRid, rid)) != 0) {
			fRidMap.at(rs) = (uint8_t)want;
		}
		IOMemoryDescriptor *od = IOMemoryDescriptor::withAddressRange((mach_vm_address_t)(uintptr_t)p3, 4,
		    kIODirectionIn, fTask);
		if (od != NULL && od->prepare() == kIOReturnSuccess) {
			od->writeBytes(0, &old, 4);
			od->complete();
		}
		OSSafeReleaseNULL(od);
	}
	// Trap 257 wants the device's _dirtyRO, _dirtyRW and _dirtyRing client pointers. One shared
	// allocation: RO at 0, RW at 0x1000, ring of {u32 id, u32, u64 offset, u64 length} at 0x2000
	if (trapIndex == 257 && p1 != NULL) {
		uint64_t triple[3] = { 0, 0, 0 };

		if (fDirty == NULL) {
			fDirty = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
			    kIODirectionInOut | kIOMemoryKernelUserShared, PD_DIRTY_BYTES, page_size);
			if (fDirty != NULL) {
				uint32_t *ro = (uint32_t *)fDirty->getBytesNoCopy();

				// RO is {read index 0, capacity 1024}: the other order hangs the plugin
				bzero(ro, PD_DIRTY_BYTES);
				ro[1] = 1024;
				fDirtyMap = fDirty->createMappingInTask(fTask, 0, kIOMapAnywhere);
			}
		}
		if (fDirtyMap != NULL) {
			uint64_t base = fDirtyMap->getAddress();

			triple[0] = base;
			triple[1] = base + 0x1000;
			triple[2] = base + 0x2000;
		}
		{
			IOMemoryDescriptor *wd = IOMemoryDescriptor::withAddressRange(
			    (mach_vm_address_t)(uintptr_t)p1, sizeof(triple), kIODirectionIn, fTask);

			if (wd != NULL && wd->prepare() == kIOReturnSuccess) {
				wd->writeBytes(0, triple, sizeof(triple));
				wd->complete();
			}
			OSSafeReleaseNULL(wd);
		}
	}
	// IOSurfaceRoot trap 7 {event port, value}: MTLSharedEvent setSignaledValue from the CPU
	if (strcmp(fTag, "IOSurfaceRoot") == 0 && trapIndex == 7 && sEventPage != NULL) {
		OSObject *obj = NULL;

		if (IOUserClient::copyObjectForPortNameInTask(fTask, (mach_port_name_t)(uintptr_t)p1, &obj) == kIOReturnSuccess) {
			OSNumber *num = OSDynamicCast(OSNumber, obj);

			if (num != NULL && (num->unsigned32BitValue() & kPDEventTag) != 0 &&
			    (num->unsigned32BitValue() & ~kPDEventTag) < kPDEventMax) {
				pdEventSet(num->unsigned32BitValue() & ~kPDEventTag, (uint64_t)(uintptr_t)p2);
			}
			OSSafeReleaseNULL(obj);
		}
	}
	// IOSurfaceRoot traps 2 and 3 lock and unlock a surface: (id, options, seed pointer). The read
	// only record is marked at +0x0c while locked, the seed at +0x10 bumped on unlock and returned
	if (strcmp(fTag, "IOSurfaceRoot") == 0 && (trapIndex == 2 || trapIndex == 3) && sSurfRegion != NULL) {
		uint32_t id = (uint32_t)(uintptr_t)p1;

		if (id < PD_SURF_MAX && sSurfaces[id].md != NULL) {
			uint8_t *ro = (uint8_t *)sSurfRegion->getBytesNoCopy() + PD_SURF_RO_BASE + id * PD_SURF_RO_SIZE;
			uint32_t locked = trapIndex == 2 ? 1 : 0;
			uint64_t seed;

			// a CPU lock waits for the queued GPU work using the surface (only its writers for a read-only
			// lock), as IOSurfaceLock does unless asked to avoid the sync: a CPU redraw raced queued reads
			if (trapIndex == 2 && !((uint32_t)(uintptr_t)p2 & 2))
				pdAsyncDrainSurface(id, 3000, ((uint32_t)(uintptr_t)p2 & 1) != 0);
			memcpy(&seed, ro + 0x10, 8);
			if (trapIndex == 3) {
				seed++;
				memcpy(ro + 0x10, &seed, 8);
			}
			memcpy(ro + 0x0c, &locked, 4);
			if (p3 != NULL) {
				IOMemoryDescriptor *wd = IOMemoryDescriptor::withAddressRange((mach_vm_address_t)(uintptr_t)p3, 4,
				    kIODirectionIn, fTask);
				uint32_t seed32 = (uint32_t)seed;

				if (wd != NULL && wd->prepare() == kIOReturnSuccess) {
					wd->writeBytes(0, &seed32, 4);
					wd->complete();
				}
				OSSafeReleaseNULL(wd);
			}
			return kIOReturnSuccess;
		}
		return kIOReturnBadArgument;
	}
	// trap 1 {id} releases a resource. In trap order, since the client can create the next one
	// under the same id right away. Queued submissions run first, so none loses a buffer it reads
	if (trapIndex == 1 && strcmp(fTag, "AppleParavirtGPU") == 0 && p1 != NULL) {
		pdAsyncDrain(3000);
		resLock();
		releaseResource((uint32_t)(uintptr_t)p1);
		resUnlock();
	}
	// trap 0 submits one command buffer: p2 bytes at p3 are its 0x30-byte record
	if (trapIndex == 0) {
		uint8_t rec[0x30] = {};
		int err = p3 != NULL && (uintptr_t)p2 > 0 && (uintptr_t)p2 <= 0x400 ?
		    copyin((user_addr_t)(uintptr_t)p3, rec, sizeof(rec)) : EINVAL;

		if (err != 0 && strcmp(fTag, "AppleParavirtGPU") == 0)
			IOLog("PDIOSurface: %s submit on queue %llu unreadable (copyin %d)\n", fTag,
			    (unsigned long long)(uintptr_t)p1, err);
		submitRecord((uint32_t)(uintptr_t)p1, rec);
	}
	return kIOReturnSuccess;
}

IOReturn
IOSurfaceRootUserClient::registerNotificationPort(mach_port_t port, UInt32 type,
    io_user_reference_t refCon)
{
	if (type < 8 && strcmp(fTag, "IOMobileFramebuffer") == 0) {
		sFBNotifyPorts[type] = port;
		sFBOwner = this;
	}
	if (fTraced)
		IOLog("PD-gputrace: %s port type %u ref %llu\n", fTag, type, (unsigned long long)refCon);
	{
		char key[24];
		IOSharedDataQueue *q = NULL;

		snprintf(key, sizeof(key), "%llu", (unsigned long long)refCon);
		if (fQueues != NULL)
			q = OSDynamicCast(IOSharedDataQueue, fQueues->getObject(key));
		// no queue yet: keep the port for when sel 16 makes it
		if (q == NULL && refCon != 0) {
			for (uint32_t i = 0; i < 4; i++) {
				if (fEarlyRef[i] == 0 || fEarlyRef[i] == refCon) {
					fEarlyRef[i] = refCon;
					fEarlyPort[i] = port;
					break;
				}
			}
		}
		if (q != NULL) {
			q->setNotificationPort(port);
			// completions that landed before the port: announce them now
			if (q->peek() != NULL && OSDynamicCast(PDNotifyQueue, q) != NULL) {
				IOLog("PDIOSurface: %s queue %s had entries before its port\n", fTag, key);
				((PDNotifyQueue *)q)->notify();
			}
		}
	}
	return kIOReturnSuccess;
}

IOReturn
IOSurfaceRootUserClient::clientMemoryForType(UInt32 type, IOOptionBits *options,
    IOMemoryDescriptor **memory)
{
	if (type == PD_BRIDGE_TYPE) {
		if (sBridge == NULL) {
			sBridge = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
			    kIODirectionInOut | kIOMemoryKernelUserShared, PD_BRIDGE_BYTES, page_size);
			if (sBridge != NULL) bzero(sBridge->getBytesNoCopy(), sBridge->getLength());
		}
		if (sBridge == NULL) return kIOReturnNoMemory;
		sBridge->retain();
		*memory = sBridge;
		*options = 0;
		return kIOReturnSuccess;
	}
	IOBufferMemoryDescriptor *md = NULL;
	uint32_t index;

	if (fShared == NULL) {
		fShared = OSArray::withCapacity(8);
	}
	if (fShared == NULL) {
		return kIOReturnNoMemory;
	}
	for (index = 0; index < fShared->getCount(); index++) {
		if (fSharedType[index] == type) {
			*memory = (IOMemoryDescriptor *)fShared->getObject(index);
			(*memory)->retain();
			*options = 0;
			return kIOReturnSuccess;
		}
	}
	md = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
	    kIODirectionInOut | kIOMemoryKernelUserShared, 0x100000, page_size);
	if (md == NULL) {
		return kIOReturnNoMemory;
	}
	bzero(md->getBytesNoCopy(), md->getLength());
	index = fShared->getCount();
	if (index < 16) {
		fSharedType[index] = type;
		fShared->setObject(md);
	}
	*memory = md;
	*options = 0;
	return kIOReturnSuccess;
}

// A client buffer by object id. No-copy buffers wrap client memory (sel 9 +0x40): refresh our
// copy from it first, since that is where the client wrote
IOBufferMemoryDescriptor *
IOSurfaceRootUserClient::lookupBuffer(const char *key)
{
	resLock();
	IOBufferMemoryDescriptor *md = fBuffers ? OSDynamicCast(IOBufferMemoryDescriptor, fBuffers->getObject(key)) : NULL;
	IOMemoryDescriptor *u = (md != NULL && fUserBuffers != NULL) ?
	    OSDynamicCast(IOMemoryDescriptor, fUserBuffers->getObject(key)) : NULL;

	if (u != NULL) {
		IOByteCount n = u->getLength() < md->getLength() ? u->getLength() : md->getLength();

		u->readBytes(0, md->getBytesNoCopy(), n);
	}
	resUnlock();
	return md;
}

// a view of a heap or buffer texture aliases that texture: follow the chain to the memory's
// owner (its record's off already holds the offset into it)
IOBufferMemoryDescriptor *
IOSurfaceRootUserClient::texMemory(uint32_t id, uint64_t *off)
{
	uint32_t m = id, o = texSlot(id);
	char key[16];

	*off = 0;
	if (id == 0)
		return NULL;
	if (texInfo(o).alias == 0 && texInfo(o).surf != 0 && sSurfaces[texInfo(o).surf].md != NULL)
		return sSurfaces[texInfo(o).surf].md;
	for (uint32_t i = 0; i < 4 && texInfo(texSlot(m)).alias != 0; i++) {
		m = texInfo(texSlot(m)).alias;
	}
	if (texInfo(o).alias != 0)
		*off = texInfo(o).off;
	snprintf(key, sizeof(key), "%u", m);
	return lookupBuffer(key);
}

void
IOSurfaceRootUserClient::answerQuery(uint32_t buffer, uint64_t off, const void *data, uint32_t len)
{
	IOBufferMemoryDescriptor *md;
	char key[16];

	snprintf(key, sizeof(key), "%u", buffer);
	md = lookupBuffer(key);
	if (md == NULL || off > md->getLength() || len > md->getLength() - off) {
		IOLog("PDIOSurface: %s query answer to buffer %u+0x%llx has nowhere to go\n", fTag, buffer,
		    (unsigned long long)off);
		return;
	}
	memcpy((uint8_t *)md->getBytesNoCopy() + off, data, len);
}

void
IOSurfaceRootUserClient::resLock(void)
{
	if (fResLock == NULL) {
		IORecursiveLock *l = IORecursiveLockAlloc();

		if (!OSCompareAndSwapPtr(NULL, l, (void * volatile *)&fResLock)) {
			IORecursiveLockFree(l);
		}
	}
	if (fResLock != NULL) {
		IORecursiveLockLock(fResLock);
	}
}

void
IOSurfaceRootUserClient::resUnlock(void)
{
	if (fResLock != NULL) {
		IORecursiveLockUnlock(fResLock);
	}
}

IOReturn
IOSurfaceRootUserClient::clientClose(void)
{
	// a completion post still pending holds a reference on us. One already running
	// iterates fQueues, so wait for it before anything below is released
	if (fCompletionCall != NULL && thread_call_cancel_wait(fCompletionCall)) {
		release();
	}
	// The notification ports belong to the client that registered them. Once it is gone
	// nothing may send to them, the timer included
	if (strcmp(fTag, "IOMobileFramebuffer") == 0 && sFBOwner == this) {
		sFBOwner = NULL;
		if (sFBVsync != NULL) {
			thread_call_cancel(sFBVsync);
		}
		sFBTask = TASK_NULL;
		bzero(sFBNotifyPorts, sizeof(sFBNotifyPorts));
		bzero(sFBCallback, sizeof(sFBCallback));
		bzero(sFBRefcon, sizeof(sFBRefcon));
	}
	OSSafeReleaseNULL(fMaps);
	OSSafeReleaseNULL(fResMaps);
	OSSafeReleaseNULL(fResObj);
	OSSafeReleaseNULL(fShmems);
	OSSafeReleaseNULL(fQueues);
	OSSafeReleaseNULL(fBuffers);
	if (fUserBuffers != NULL) {
		OSCollectionIterator *it = OSCollectionIterator::withCollection(fUserBuffers);
		OSSymbol *k;

		while (it != NULL && (k = OSDynamicCast(OSSymbol, it->getNextObject())) != NULL) {
			IOMemoryDescriptor *u = OSDynamicCast(IOMemoryDescriptor, fUserBuffers->getObject(k));

			if (u != NULL) {
				u->complete();
			}
		}
		OSSafeReleaseNULL(it);
		OSSafeReleaseNULL(fUserBuffers);
	}
	OSSafeReleaseNULL(fResources);
	OSSafeReleaseNULL(fEncoders);
	OSSafeReleaseNULL(fSurfRegion);
	OSSafeReleaseNULL(fDirtyMap);
	OSSafeReleaseNULL(fDirty);
	OSSafeReleaseNULL(fSel2MD);
	OSSafeReleaseNULL(fRegionB);
	OSSafeReleaseNULL(fSideband[0]);
	OSSafeReleaseNULL(fSideband[1]);
	for (uint32_t i = 0; i < fSbCount; i++) {
		OSSafeReleaseNULL(fSbPair[i][0]);
		OSSafeReleaseNULL(fSbPair[i][1]);
	}
	fSbCount = 0;
	OSSafeReleaseNULL(fEventMap);
	if (fCompletionCall != NULL) {
		thread_call_free(fCompletionCall);
		fCompletionCall = NULL;
	}
	if (fSubmitLock != NULL) {
		IOLockFree(fSubmitLock);
		fSubmitLock = NULL;
	}
	if (fPendLock != NULL) {
		IOLockFree(fPendLock);
		fPendLock = NULL;
	}
	OSSafeReleaseNULL(fSwapDesc);
	OSSafeReleaseNULL(fShared);
	terminate();
	return kIOReturnSuccess;
}

IOReturn
IOSurfaceRootUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
	char name[17];
	// sel 9 creates resources: serialised against trap 1 releasing them
	bool create = selector == 9 && strcmp(fTag, "AppleParavirtGPU") == 0;

	if (create) {
		resLock();
	}
	IOReturn ret = externalMethodImpl(selector, args, dispatch, target, reference);
	if (create) {
		resUnlock();
	}
	if (pdGpuTraced(name))
		IOLog("PD-gputrace: %s[%d] %s sel %u in %u/%u out %u/%u%s -> 0x%x\n", name, proc_selfpid(), fTag,
		    selector, args->scalarInputCount, args->structureInputSize, args->scalarOutputCount,
		    args->structureOutputSize, args->asyncWakePort != MACH_PORT_NULL ? " async" : "", ret);
	return ret;
}

IOReturn
IOSurfaceRootUserClient::externalMethodImpl(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
	if ((selector == PD_BRIDGE_SEL_WAIT_JOB || selector == PD_BRIDGE_SEL_DONE) && strcmp(fTag, "PDShaderBridge") == 0)
		return pdBridgeDoorbell(selector);
	// sel 29 submits several command buffers at once (IOGPU's batch when more than one is ready on a queue): scalars {queue id, 0, count, record size 0x30},
	// the records back to back as trap 0's. Left unanswered, those command buffers never ran and their waiters slept for good
	if (selector == 29 && strcmp(fTag, "AppleParavirtGPU") == 0 && args->scalarInputCount >= 4) {
		uint32_t queue = (uint32_t)args->scalarInput[0], n = (uint32_t)args->scalarInput[2];
		uint32_t sz = (uint32_t)args->scalarInput[3];
		uint8_t rec[0x30];

		if (sz < 0x20 || sz > 0x400 || n == 0 || n > 64)
			return kIOReturnBadArgument;
		drainDirty();
		if (fSubmitLock != NULL)
			IOLockLock(fSubmitLock);
		for (uint32_t i = 0; i < n; i++) {
			bzero(rec, sizeof(rec));
			if (args->structureInputDescriptor != NULL)
				args->structureInputDescriptor->readBytes((IOByteCount)i * sz, rec, min(sz, (uint32_t)sizeof(rec)));
			else if (args->structureInput != NULL && (uint64_t)(i + 1) * sz <= args->structureInputSize)
				memcpy(rec, (const uint8_t *)args->structureInput + (size_t)i * sz, min(sz, (uint32_t)sizeof(rec)));
			submitRecord(queue, rec);
		}
		if (fSubmitLock != NULL)
			IOLockUnlock(fSubmitLock);
		if (args->scalarOutputCount >= 1)
			args->scalarOutput[0] = 0;
		return kIOReturnSuccess;
	}
	if (selector == PD_BRIDGE_SEL_LOG && strcmp(fTag, "PDShaderBridge") == 0) {
		const char *s = (const char *)args->structureInput;
		uint32_t n = args->structureInputSize;

		if (s == NULL || n == 0 || n > PD_BRIDGE_LOG_MAX)
			return kIOReturnBadArgument;
		while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\0'))
			n--;
		IOLog("airjitd: %.*s\n", (int)n, s);
		return kIOReturnSuccess;
	}

	drainDirty();

	// The real 26.6.2 display answers these unsupported. Saying yes sends WindowServer down
	// paths nothing here completes
	if (strcmp(fTag, "IOMobileFramebuffer") == 0 && (selector == 33 || selector == 50 || selector == 55 ||
	    selector == 68 || selector == 74 || selector == 79 || selector == 84)) {
		return kIOReturnUnsupported;
	}
	// sel 15 {0, 0x10} fails outright on the real display (kIOReturnError, output untouched)
	if (strcmp(fTag, "IOMobileFramebuffer") == 0 && selector == 15) {
		return kIOReturnError;
	}

	if (selector == 257 && args->structureInput != NULL && args->structureInputSize >= 12) {
		uint32_t w1;

		memcpy(&w1, (const uint8_t *)args->structureInput + 4, 4);
		// Type 0xe creates a pipeline: {id, 0xe, len, id, len, count, then [tag][04][u32]...}
		// where tag 1 is the vertex (or kernel) function id and tag 2 the fragment's
		if (w1 == 0xe && args->structureInputSize >= 0x20) {
			const uint8_t *m = (const uint8_t *)args->structureInput;
			uint32_t id = 0, count = m[0x14], fn[3] = { 0, 0, 0 }, k;
			uint32_t pos = 0x15;

			memcpy(&id, m, 4);
			for (k = 0; k < count && pos + 6 <= args->structureInputSize && m[pos + 1] == 4; k++, pos += 6) {
				uint32_t v;

				memcpy(&v, m + pos + 2, 4);
				if (m[pos] < 3) {
					fn[m[pos]] = v;
				}
			}
			uint32_t ps = idSlotNew(kIdPipe, id);
			if (ps != 0) {
				pipeInfo(ps).tileFn = 0;
				pipeInfo(ps).vertexFn = fn[1];
				pipeInfo(ps).fragmentFn = fn[2];
				pipeInfo(ps).blend = pdParseBlend(m, args->structureInputSize);
				for (uint32_t a = 1; a < 4; a++) {
					pipeInfo(ps).blendRT[a] = pdParseBlend(m, args->structureInputSize, a);
				}
			}
			// The daemon introspects each new function's interface once
			for (k = 1; k <= 2; k++) {
				uint32_t f = fn[k];
				PDBridgeGuard bridgeGuard;

				if (f == 0 || shader(f) == NULL || sIntrospected[shaderKey(f)] || sBridge == NULL) {
					continue;
				}
				PDBridgeHeader *h = (PDBridgeHeader *)sBridge->getBytesNoCopy();
				uint32_t msize = (uint32_t)shader(f)->getLength();

				if (h->status != 0x44525652 || h->state != 0 || 0x100 + msize > PD_BRIDGE_BYTES) {
					continue;
				}
				sIntrospected[shaderKey(f)] = 1;
				h->serial = ++sBridgeSerial; h->functionID = shaderKey(f); h->pipelineID = id;
				h->metallibSize = msize; h->dataSize = 0; h->dataOffset = 0x100 + msize;
				h->kind = 1;
				memcpy((uint8_t *)h + 0x100, shader(f)->getBytesNoCopy(), msize);
				pdBridgeRing(h);
				pdBridgeWait(h, 30000);
				__sync_synchronize();
				pdBridgeRelease(h);
			}
		}
		// type 0x33 is a tile pipeline: {id, 0x33, len, id, len, count, [tag][04][u32]...}, tag 1 the tile
		// kernel, then its label and colour attachment formats
		if (w1 == 0x33 && args->structureInputSize >= 0x15) {
			const uint8_t *m = (const uint8_t *)args->structureInput;
			uint32_t pid = 0, n = m[0x14], at = 0x15;

			memcpy(&pid, m, 4);
			uint32_t ps = idSlotNew(kIdPipe, pid);
			if (ps != 0) {
				pipeInfo(ps).tileFn = 0;
				pipeInfo(ps).vertexFn = 0;
				pipeInfo(ps).fragmentFn = 0;
			}
			for (uint32_t e = 0; e < n && at + 6 <= args->structureInputSize && m[at + 1] == 4; e++, at += 6) {
				uint32_t v;

				memcpy(&v, m + at + 2, 4);
				if (m[at] == 1 && ps != 0)
					pipeInfo(ps).tileFn = v;
			}
		}
		// type 0xb is a compute pipeline: {id, 0xb, len, id, len, count, [tag][04][u32]...}, tag 0 the kernel
		if (w1 == 0xb && args->structureInputSize >= 0x15) {
			const uint8_t *m = (const uint8_t *)args->structureInput;
			uint32_t pid = 0, n = m[0x14], at = 0x15;

			memcpy(&pid, m, 4);
			uint32_t ps = idSlotNew(kIdPipe, pid);
			if (ps != 0)
				pipeInfo(ps).maxThreads = 0;
			for (uint32_t e = 0; e < n && at + 6 <= args->structureInputSize; e++, at += 6) {
				uint32_t v;

				memcpy(&v, m + at + 2, 4);
				if (m[at] == 0 && m[at + 1] == 4 && ps != 0) {
					pipeInfo(ps).fn = v;
				}
				// tag 8: the descriptor's maxTotalThreadsPerThreadgroup
				if (m[at] == 8 && m[at + 1] == 4 && ps != 0 && v <= 1024)
					pipeInfo(ps).maxThreads = v;
			}
		}
		// a sampler: {id, 3, 0x24, {id, flags, 0, 0, FLT_MAX, 0, 0}}
		if (w1 == 3 && args->structureInputSize >= 0x14) {
			uint32_t sid = 0, sflags = 0;

			memcpy(&sid, args->structureInput, 4);
			memcpy(&sflags, (const uint8_t *)args->structureInput + 0x10, 4);
			// the valid bit tells airjitd a nearest/clamp/pixel sampler (all flags clear) from an unknown one
			uint32_t ss = idSlotNew(kIdSamp, sid);
			if (ss != 0)
				fSampMap.at(ss) = sflags | 0x40000000u;
		}
	}

	// sel 259 destroys a host object: {id, 0, kind, 0xc, id}. kind 0x3eb a sampler, 0x3ed a function,
	// 0x3ee a pipeline, 0x3ea a depth stencil state (kept nowhere). others are logged under pdobjmap=1
	if (selector == 259 && args->structureInput != NULL && args->structureInputSize >= 12) {
		static uint32_t logged;
		uint32_t id, kind;

		memcpy(&id, args->structureInput, 4);
		memcpy(&kind, (const uint8_t *)args->structureInput + 8, 4);
		if (kind == 0x3eb) {
			idDrop(kIdSamp, id);
		} else if (kind == 0x3ed) {
			idDrop(kIdFn, id);
		} else if (kind == 0x3ee) {
			idDrop(kIdPipe, id);
		} else if (kind != 0x3ea && pdObjMapLog() && logged < 32) {
			logged++;
			IOLog("PDIOSurface: %s sel 259 destroys id %u kind 0x%x (%u bytes)\n", fTag, id, kind,
			    (uint32_t)args->structureInputSize);
		}
	}

	// BootPolicy (AppleVPBootPolicy stand-in): SEP commands {'BPch', cmd, ...}. libbootpolicy
	// wants the reply header echoed with magic 'BPrh' (0x42507268), payload zeroed
	if (strcmp(fTag, "BootPolicy") == 0) {
		// the policy identity of the reference VM whose iBoot system container the image carries:
		// iSCPreboot names each LocalPolicy after these nonce digests, read back with bputil -d
		static const uint8_t lpnh[48] = {	// cmd 14, local policy nonce hash
			0x11, 0xf0, 0x1a, 0x42, 0xcd, 0xf8, 0x3e, 0x8a, 0x76, 0x29, 0xb6, 0xf3, 0x09, 0x5d, 0x21, 0x8a,
			0xb1, 0x03, 0xd0, 0x38, 0x42, 0x38, 0xf1, 0x95, 0x23, 0x57, 0xd6, 0xf1, 0x46, 0x39, 0x33, 0xb6,
			0xcc, 0xba, 0x02, 0xfe, 0xc1, 0x25, 0x82, 0x92, 0x69, 0x34, 0x3e, 0x18, 0x4a, 0x60, 0x96, 0xfc,
		};
		static const uint8_t cmd11[48] = {	// cmd 11
			0x73, 0x49, 0x8a, 0x9c, 0xb3, 0x8a, 0x6e, 0x1f, 0xbe, 0xcb, 0x5a, 0x9f, 0x9c, 0x14, 0x6a, 0x1d,
			0xf7, 0x4a, 0xb5, 0x66, 0xa1, 0xbe, 0x7a, 0x4a, 0xbd, 0xf8, 0x8a, 0xde, 0xed, 0xb7, 0xb1, 0x1d,
			0x93, 0x25, 0x02, 0xc8, 0x00, 0xd2, 0x75, 0x4b, 0xc8, 0xfd, 0x36, 0xe8, 0x48, 0xe9, 0x60, 0x70,
		};
		static const uint8_t ronh[48] = {	// cmd 16, recoveryOS policy nonce hash
			0x43, 0x54, 0x1c, 0xf2, 0x87, 0xff, 0x0d, 0xd8, 0xd4, 0x1f, 0xcf, 0x18, 0x89, 0xe6, 0x1b, 0x8d,
			0x15, 0xb4, 0x74, 0xbe, 0x22, 0xc5, 0xb0, 0xf9, 0x63, 0x8c, 0x1b, 0xf8, 0x9a, 0x5b, 0x0d, 0xee,
			0x12, 0xb7, 0x4f, 0xe2, 0x96, 0x11, 0x57, 0xdb, 0x38, 0x32, 0x1d, 0x8a, 0xc9, 0x86, 0xd5, 0x3f,
		};
		static const uint8_t paired[4] = { 1, 0, 0, 0 };	// cmd 67, pairing status
		const uint8_t *payload = NULL;
		uint32_t payloadLen = 0;
		static const uint32_t replyMagic = 0x42507268;
		// Reply +4 is the SEP status: 3 ("SEP storage", nothing stored) for get_oic (38), as a
		// real VM answers. Everything else succeeds with a zero payload
		uint32_t cmd = 0, status = 0;

		if (args->structureInput != NULL && args->structureInputSize >= 8) {
			memcpy(&cmd, (const uint8_t *)args->structureInput + 4, sizeof(cmd));
		}
		if (cmd == 38) {
			status = 3;
		} else if (cmd == 14) {
			payload = lpnh;
			payloadLen = sizeof(lpnh);
		} else if (cmd == 11) {
			payload = cmd11;
			payloadLen = sizeof(cmd11);
		} else if (cmd == 16) {
			payload = ronh;
			payloadLen = sizeof(ronh);
		} else if (cmd == 67) {
			payload = paired;
			payloadLen = sizeof(paired);
		}

		for (uint32_t i = 0; i < args->scalarOutputCount; i++) {
			args->scalarOutput[i] = 0;
		}
		if (args->structureOutput != NULL && args->structureOutputSize > 0) {
			bzero(args->structureOutput, args->structureOutputSize);
			if (args->structureInput != NULL && args->structureInputSize >= 16 && args->structureOutputSize >= 16) {
				memcpy(args->structureOutput, args->structureInput, 16);
				memcpy(args->structureOutput, &replyMagic, sizeof(replyMagic));
				memcpy((uint8_t *)args->structureOutput + 4, &status, sizeof(status));
			}
			if (payload != NULL && args->structureOutputSize >= 0x20 + payloadLen) {
				memcpy((uint8_t *)args->structureOutput + 0x20, payload, payloadLen);
			}
		}
		// mobileactivationd passes a large output as a descriptor: same reply, full length
		if (args->structureOutputDescriptor != NULL) {
			IOMemoryDescriptor *md = args->structureOutputDescriptor;
			uint64_t len = md->getLength();
			static const uint8_t zeros[256] = { 0 };

			if (md->prepare() == kIOReturnSuccess) {
				for (uint64_t off = 0; off < len; off += sizeof(zeros)) {
					md->writeBytes(off, zeros, (IOByteCount)(len - off < sizeof(zeros) ? len - off : sizeof(zeros)));
				}
				if (args->structureInput != NULL && args->structureInputSize >= 16 && len >= 16) {
					md->writeBytes(0, args->structureInput, 16);
					md->writeBytes(0, &replyMagic, sizeof(replyMagic));
					md->writeBytes(4, &status, sizeof(status));
				}
				if (payload != NULL && len >= 0x20 + payloadLen) {
					md->writeBytes(0x20, payload, payloadLen);
				}
				md->complete();
			}
			args->structureOutputDescriptorSize = (uint32_t)len;
		}
		return kIOReturnSuccess;
	}

	// Tagged accelerator replies: each 8-byte word names client type,
	// selector and offset, and its non-canonical top half faults if used as a pointer
	if (strcmp(fTag, "IOSurfaceRoot") == 0) {
		for (uint32_t i = 0; i < args->scalarOutputCount; i++) {
			args->scalarOutput[i] = 0;
		}
		if (args->structureOutput != NULL && args->structureOutputSize > 0) {
			bzero(args->structureOutput, args->structureOutputSize);
		}
		// The contract as Apple's kext answers it (PROTO.md section 8)
		if (selector == 32 && args->scalarOutputCount >= 1) {
			args->scalarOutput[0] = pdSurfRegionAddress(this, fTask, &fSurfRegion);
			return args->scalarOutput[0] ? kIOReturnSuccess : kIOReturnNoMemory;
		}
		if (selector == 0 && args->structureInput != NULL && args->structureOutput != NULL) {
			uint32_t id = 0;
			IOReturn kr = pdSurfCreate((const uint8_t *)args->structureInput, args->structureInputSize, &id);

			if (kr != kIOReturnSuccess) {
				return kr;
			}
			return pdSurfReply(this, fTask, &fSurfRegion, &fMaps, id, (uint8_t *)args->structureOutput,
			    args->structureOutputSize);
		}
		if (selector == 10 && args->structureInput != NULL && args->structureInputSize >= 4 &&
		    args->structureOutputDescriptor != NULL) {
			uint32_t id = 0, n = 0;
			IOReturn kr;

			memcpy(&id, args->structureInput, 4);
			kr = pdSurfCopyValues(id, args->structureOutputDescriptor, &n);
			args->structureOutputDescriptorSize = n;
			return kr;
		}
		if (selector == 9 && args->structureInput != NULL && args->structureInputSize > 16) {
			uint32_t id = 0;
			OSString *err = NULL;
			OSObject *obj = OSUnserializeXML((const char *)args->structureInput + 12, args->structureInputSize - 12, &err);
			OSDictionary *d = OSDynamicCast(OSDictionary, obj);
			OSArray *a = OSDynamicCast(OSArray, obj);

			memcpy(&id, args->structureInput, 4);
			if (id < PD_SURF_MAX && sSurfaces[id].values != NULL) {
				if (d != NULL) {
					sSurfaces[id].values->merge(d);
				} else if (a != NULL) {
					for (unsigned i = 0; i + 1 < a->getCount(); i += 2) {
						OSString *key = OSDynamicCast(OSString, a->getObject(i + 1));

						if (key != NULL) {
							sSurfaces[id].values->setObject(key, a->getObject(i));
						}
					}
				}
			}
			OSSafeReleaseNULL(obj);
			OSSafeReleaseNULL(err);
			if (args->structureOutput != NULL && args->structureOutputSize >= 4) {
				bzero(args->structureOutput, 4);
			}
			return kIOReturnSuccess;
		}
		// sel 40 (async, once per process): where shared event listeners are answered
		if (selector == 40 && args->asyncWakePort != MACH_PORT_NULL) {
			bzero(fEventRef, sizeof(fEventRef));
			bcopy(args->asyncReference, fEventRef,
			    (args->asyncReferenceCount < kOSAsyncRef64Count ? args->asyncReferenceCount : kOSAsyncRef64Count) *
			    sizeof(io_user_reference_t));
			fEventRefSet = true;
			return kIOReturnSuccess;
		}
		if (selector == 39) {
			const uint64_t *in = args->scalarInput;
			uint32_t n = args->scalarInputCount;
			OSObject *obj = NULL;
			uint32_t index = kPDEventMax;
			uint64_t now = 0;

			if (n < 5 || !fEventRefSet || sEventPage == NULL) {
				return kIOReturnBadArgument;
			}
			if (IOUserClient::copyObjectForPortNameInTask(fTask, (mach_port_name_t)in[0], &obj) == kIOReturnSuccess) {
				OSNumber *num = OSDynamicCast(OSNumber, obj);

				if (num != NULL && (num->unsigned32BitValue() & kPDEventTag) != 0) {
					index = num->unsigned32BitValue() & ~kPDEventTag;
				}
				OSSafeReleaseNULL(obj);
			}
			if (index >= kPDEventMax) {
				return kIOReturnBadArgument;
			}
			if (sListenLock == NULL) {
				IOLock *lk = IOLockAlloc();

				if (lk != NULL && !OSCompareAndSwapPtr(NULL, lk, (void * volatile *)&sListenLock)) {
					IOLockFree(lk);
				}
			}
			if (sListenLock == NULL) {
				return kIOReturnNoMemory;
			}
			IOLockLock(sListenLock);
			if (sListenCount < kPDListenMax) {
				PDEventListener *l = &sListeners[sListenCount++];

				l->index = index;
				l->value = in[4];
				bcopy(fEventRef, l->ref, sizeof(l->ref));
				l->echo[0] = in[3];
				l->echo[1] = in[2];
				l->echo[2] = in[1];
				l->since = sFBFrames;
			}
			IOLockUnlock(sListenLock);
			now = ((volatile uint64_t *)sEventPage->getBytesNoCopy())[index];
			// Already reached: fire now
			if (now >= in[4]) {
				pdEventSet(index, now);
			}
			return kIOReturnSuccess;
		}
		// sel 36 {0} -> {port name, value address}: a new MTLSharedEvent. sel 38 {port name} ->
		// {0, value address, 1 << 56 | index, 0}: an event another process shared
		if ((selector == 36 || selector == 38) && args->scalarInputCount >= 1) {
			uint32_t index = kPDEventMax;
			mach_port_name_t name = 0;
			IOReturn kr = kIOReturnNoResources;

			if (sEventPage == NULL) {
				IOBufferMemoryDescriptor *pg = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
				    kIODirectionInOut | kIOMemoryKernelUserShared, kPDEventMax * 8, page_size);

				if (pg != NULL) {
					bzero(pg->getBytesNoCopy(), pg->getLength());
					if (!OSCompareAndSwapPtr(NULL, pg, (void * volatile *)&sEventPage)) {
						pg->release();
					}
				}
			}
			if (sEventAllocLock == NULL) {
				IOLock *lk = IOLockAlloc();

				if (lk != NULL && !OSCompareAndSwapPtr(NULL, lk, (void * volatile *)&sEventAllocLock)) {
					IOLockFree(lk);
				}
			}
			if (selector == 36 && sEventAllocLock != NULL) {
				OSNumber *num = NULL;

				IOLockLock(sEventAllocLock);
				for (uint32_t tries = 0; tries < kPDEventMax - 1; tries++) {
					uint32_t i = (sNextEvent + tries) % (kPDEventMax - 1) + 1;

					if (sEventObj[i] == NULL || sEventObj[i]->getRetainCount() == 1) {
						index = i;
						sNextEvent = i;
						break;
					}
				}
				if (index < kPDEventMax) {
					OSSafeReleaseNULL(sEventObj[index]);
					num = OSNumber::withNumber(kPDEventTag | index, 32);
					sEventObj[index] = num;
					if (sEventPage != NULL) {
						((volatile uint64_t *)sEventPage->getBytesNoCopy())[index] = 0;
					}
				}
				IOLockUnlock(sEventAllocLock);
				if (num != NULL) {
					kr = IOUserClient::copyPortNameForObjectInTask(fTask, num, &name);
				}
			} else if (selector == 38) {
				OSObject *obj = NULL;

				name = (mach_port_name_t)args->scalarInput[0];
				kr = IOUserClient::copyObjectForPortNameInTask(fTask, name, &obj);
				if (kr == kIOReturnSuccess) {
					OSNumber *num = OSDynamicCast(OSNumber, obj);

					if (num != NULL && (num->unsigned32BitValue() & kPDEventTag) != 0) {
						index = num->unsigned32BitValue() & ~kPDEventTag;
					} else {
						kr = kIOReturnBadArgument;
					}
					OSSafeReleaseNULL(obj);
				}
			}
			if (kr == kIOReturnSuccess && index < kPDEventMax) {
				uint64_t addr = eventAddress(index);

				if (selector == 36 && args->scalarOutputCount >= 2) {
					args->scalarOutput[0] = name;
					args->scalarOutput[1] = addr;
				} else if (selector == 38 && args->scalarOutputCount >= 4) {
					args->scalarOutput[0] = 0;
					args->scalarOutput[1] = addr;
					args->scalarOutput[2] = (1ULL << 56) | index;
					args->scalarOutput[3] = 0;
				}
			}
			return kr;
		}
		// sel 34 {port name} -> 3176 bytes: IOSurfaceLookupFromMachPort. The scalar is a port
		// name from sel 35, a plain surface id is still taken for callers that pass one
		if (selector == 34 && args->scalarInputCount >= 1 && args->structureOutput != NULL) {
			uint32_t id = (uint32_t)args->scalarInput[0];
			OSObject *obj = NULL;

			if (IOUserClient::copyObjectForPortNameInTask(fTask, (mach_port_name_t)args->scalarInput[0], &obj) ==
			    kIOReturnSuccess) {
				OSNumber *num = OSDynamicCast(OSNumber, obj);

				if (num != NULL) {
					id = num->unsigned32BitValue();
				}
				OSSafeReleaseNULL(obj);
			}
			// a lookup is one more holder, it releases on its own
			IOReturn kr = pdSurfReply(this, fTask, &fSurfRegion, &fMaps, id,
			    (uint8_t *)args->structureOutput, args->structureOutputSize);

			if (kr == kIOReturnSuccess && id < PD_SURF_MAX && sSurfaces[id].md != NULL)
				sSurfaces[id].refs++;
			return kr;
		}
		// sel 35 {id, 0} -> scalar port name: IOSurfaceCreateMachPort, how an app hands its
		// window's surface to WindowServer. The port names an OSNumber of the id
		if (selector == 35 && args->scalarInputCount >= 1 && args->scalarOutputCount >= 1) {
			uint32_t id = (uint32_t)args->scalarInput[0];
			mach_port_name_t name = 0;
			IOReturn kr = kIOReturnNotFound;

			if (id < PD_SURF_MAX && sSurfaces[id].values != NULL) {
				OSNumber *num = OSNumber::withNumber(id, 32);

				if (num != NULL) {
					kr = IOUserClient::copyPortNameForObjectInTask(fTask, num, &name);
					num->release();
				}
			}
			args->scalarOutput[0] = name;
			return kr;
		}
		// sel 13 is the framework's version handshake ("IOSurface.framework versus IOSurface.kext
		// version mismatch" on zeros). macOS 15.6.1 and 26.6.2 VMs both answer these
		if (selector == 13 && args->structureOutput != NULL && args->structureOutputSize >= 40) {
			uint32_t *w = (uint32_t *)args->structureOutput;
			uint64_t *q = (uint64_t *)(w + 2);

			w[0] = w[1] = 127;
			q[0] = 0x7fff; q[1] = 0x4000; q[2] = 0x4000; q[3] = 0x10000;
		}
		return kIOReturnSuccess;
	}
	// Type 6 selector 10 answers with a user address the client reads at once:
	// a kernel/user shared region. Map one and hand back its address
	uint64_t shmem_va = 0, shmem_len = 0;

	if (fType == 6 && selector == 10 && args->structureOutputSize >= 24) {
		IOBufferMemoryDescriptor *md = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
		    kIODirectionInOut | kIOMemoryKernelUserShared, 0x10000, page_size);
		IOMemoryMap *map = md ? md->createMappingInTask(fTask, 0, kIOMapAnywhere) : NULL;

		if (map != NULL) {
			uint8_t *shmem = (uint8_t *)md->getBytesNoCopy();

			bzero(shmem, md->getLength());
			if (fMaps == NULL) {
				fMaps = OSArray::withCapacity(4);
			}
			if (fMaps != NULL) {
				fMaps->setObject(map);
			}
			shmem_va = map->getAddress();
			shmem_len = map->getLength();
			map->release();
		}
		OSSafeReleaseNULL(md);
	}
	// Apple's paravirtual plugin drives this service like an IOGPU device. Answer with zeros,
	// except creation calls, which assert on a zero identifier
	if (args->structureOutput != NULL && args->structureOutputSize > 0) {
		uint8_t *out = (uint8_t *)args->structureOutput;

		bzero(out, args->structureOutputSize);
		// sel 265 is the capability table (224 bytes) then the feature flags (60 bytes). The table is
		// a macOS 26.6.2 host's reply, whose own features trail it in real[] unused
		if (selector == 265 && args->structureOutputSize == kAPVCapsSize + kAPVFeaturesSize) {
			static const uint8_t real[284] = { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xff, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00, 0x00, 0xff, 0x0c, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xef, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 };
			uint8_t *f = out + kAPVCapsSize;
			uint32_t v;

			// Only a subset of the host's features: with all of them WindowServer takes paths the kext
			// does not serve. Bit i turns on feature byte i, the u32 counts at 20 and 24 are set below
			memcpy(out, real, kAPVCapsSize);
			for (uint32_t i = 0; i < kAPVFeaturesSize; i++) {
				if (i >= kAPVMaxFIFOCount && i < kAPVMaxFIFOCount + 8) {
					continue;
				}
				if (kAPVFeatureMask & (1ULL << i)) {
					f[i] = 1;
				}
			}
			v = 1;
			memcpy(f + kAPVMaxFIFOCount, &v, sizeof(v));
			v = 0;
			memcpy(f + kAPVS8ByteCount, &v, sizeof(v));
		}
		// IOGPU's device keeps the regions sel 2 and 5 hand out, left zero -[AppleParavirtTexture
		// synchronize] gets NULL. Shapes from a 26.6.2 host (PROTO.md 15), sel 0 constants below
		if (strcmp(fTag, "AppleParavirtGPU") == 0 &&
		    ((selector == 2 && args->structureOutputSize == 536) || (selector == 5 && args->structureOutputSize == 32))) {
			uint64_t va[2] = { 0, 0 };
			uint32_t nreg = selector == 5 ? 2 : 1, rlen = selector == 5 ? 0x4000 : 0x10000;

			for (uint32_t r = 0; r < nreg; r++) {
				IOBufferMemoryDescriptor *rmd = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
				    kIODirectionInOut | kIOMemoryKernelUserShared, rlen, page_size);
				IOMemoryMap *rmap = rmd ? rmd->createMappingInTask(fTask, 0, kIOMapAnywhere) : NULL;

				if (rmap != NULL) {
					bzero(rmd->getBytesNoCopy(), rlen);
					// Region B starts with the host's GPU address allocator word. Resource id N's
					// record is its 0xc0 bytes at N * 0xc0 (a 26.6.2 host's sel 9 replies)
					if (selector == 5 && r == 1) {
						*(uint32_t *)rmd->getBytesNoCopy() = 0x100000;
						if (fRegionB == NULL) {
							fRegionB = rmd;
							fRegionB->retain();
							fRegionBVA = rmap->getAddress();
						}
					}
					// sel 2's region holds each queue's last completed stamp << 8 at 4 * index
					if (selector == 2 && fSel2MD == NULL) {
						uint32_t *w = (uint32_t *)rmd->getBytesNoCopy();

						for (uint32_t q = 0; q < 256; q++) {
							fQueueStamp[q] = 0x100;
							w[q] = 0x100 << 8;
						}
						fSel2MD = rmd;
						fSel2MD->retain();
					}
					va[r] = rmap->getAddress();
					if (fMaps == NULL) {
						fMaps = OSArray::withCapacity(16);
					}
					if (fMaps != NULL) {
						fMaps->setObject(rmap);
					}
					rmap->release();
				}
				OSSafeReleaseNULL(rmd);
			}
			if (selector == 2) {
				uint32_t four = 4, x20 = 0x20;

				memcpy(out + 0x00, &va[0], 8);
				memcpy(out + 0x08, &four, 4);
				memcpy(out + 0x14, &x20, 4);
				memcpy(out + 0x1c, &four, 4);
				for (uint32_t k = 0; k < 30; k++) {
					uint32_t o = 8 + 4 * k;
					memcpy(out + 0x20 + 4 * k, &o, 4);
				}
			} else {
				uint64_t trace = 0x1a04cb00, sz = 0x4000;

				memcpy(out + 0x00, &trace, 8);
				memcpy(out + 0x08, &va[0], 8);
				memcpy(out + 0x10, &va[1], 8);
				memcpy(out + 0x18, &sz, 8);
			}
		}
		if (strcmp(fTag, "AppleParavirtGPU") == 0 && selector == 0 && args->structureOutputSize == 64) {
			uint32_t one = 1;
			uint64_t base = 0x155558000ULL;

			memcpy(out + 0x18, &one, 4);
			memcpy(out + 0x20, &base, 8);
			memcpy(out + 0x2c, &one, 4);
		}
		// Creation calls hand back an identifier.
		// IOGPUCommandQueueCreate (sel 7) asserts on zero
		if ((selector == 7 || selector == 16) && args->structureOutputSize >= 8) {
			// Per process, from 1, as a host numbers them: the plugin indexes sel 2's completed
			// stamps and the usage records by command queue id - 1
			uint64_t id = selector == 7 ? ++fNextQueueID : ++fNextNotifyID;

			// The id's offset differs per call, so every word of these small replies carries it
			for (uint32_t w = 0; w + 8 <= args->structureOutputSize; w += 8) {
				memcpy(out + w, &id, sizeof(id));
			}
			// sel 16 makes a notification queue of scalar0 entries of scalar1 bytes, which is
			// IODataQueue::withEntries' signature: a PDNotifyQueue, {address, id} back
			if (selector == 16 && args->scalarInputCount >= 2 && args->structureOutputSize >= 16) {
				IOSharedDataQueue *q = OSTypeAlloc(PDNotifyQueue);

				if (q != NULL && !q->initWithEntries((UInt32)args->scalarInput[0], (UInt32)args->scalarInput[1]))
					OSSafeReleaseNULL(q);
				IOMemoryDescriptor *qmd = q ? q->getMemoryDescriptor() : NULL;
				IOMemoryMap *qmap = qmd ? qmd->createMappingInTask(fTask, 0, kIOMapAnywhere) : NULL;

				if (qmap != NULL) {
					uint64_t va = qmap->getAddress();
					char key[24];

					memcpy(out, &va, sizeof(va));
					memcpy(out + 8, &id, sizeof(id));
					snprintf(key, sizeof(key), "%llu", (unsigned long long)id);
					if (fQueues == NULL) {
						fQueues = OSDictionary::withCapacity(4);
					}
					if (fMaps == NULL) {
						fMaps = OSArray::withCapacity(16);
					}
					for (uint32_t i = 0; i < 4; i++) {
						if (fEarlyRef[i] == id) {
							q->setNotificationPort(fEarlyPort[i]);
							fEarlyRef[i] = 0;
						}
					}
					if (fQueues != NULL) {
						fQueues->setObject(key, q);
					}
					if (fMaps != NULL) {
						fMaps->setObject(qmap);
					}
				}
				OSSafeReleaseNULL(qmap);
				OSSafeReleaseNULL(qmd);
				OSSafeReleaseNULL(q);
			}
		}
	}
	// sel 9 creates a resource: the length is at +0x48 of the request, the reply maps it into
	// the client in the layout a 26.6.2 host answers
	if (selector == 9 && args->structureInputSize >= 0x50 && args->structureOutput != NULL &&
	    args->structureOutputSize >= 8) {
		uint64_t len = 0;
		IOBufferMemoryDescriptor *md;
		IOMemoryMap *map;

		bool onSurface = false;

		memcpy(&len, (const uint8_t *)args->structureInput + 0x48, sizeof(len));
		// A private texture (168 bytes, type byte +0x13 = 8) carries no length: the host sizes it
		// from width and height (u16 at +0x08/+0x0a) at 8 bytes a pixel, the widest format
		uint32_t sid168 = 0;

		if (args->structureInputSize == 168) {
			memcpy(&sid168, (const uint8_t *)args->structureInput + 0x38, 4);
		}
		// the 168 byte form can also name an IOSurface at +0x38 (WindowServer's display surface):
		// then it is that surface's memory, not a private allocation
		if (len == 0 && args->structureInputSize == 168 && sid168 != 0 && sid168 < PD_SURF_MAX &&
		    sSurfaces[sid168].md != NULL) {
			uint16_t tw = 0, th = 0;
			uint32_t objid = 0;

			memcpy(&tw, (const uint8_t *)args->structureInput + 0x08, 2);
			memcpy(&th, (const uint8_t *)args->structureInput + 0x0a, 2);
			memcpy(&objid, (const uint8_t *)args->structureInput + 0x5c, 4);
			uint32_t o = idSlotNew(kIdObj, objid);
			if (o != 0) {
				texInfo(o).w = tw;
				texInfo(o).h = th;
				texInfo(o).surf = (uint16_t)sid168;
				// the request's own descriptor, else RGBA16Float for RGhA and BGRA8Unorm otherwise
				texInfo(o).pf = pdDescFormat((const uint8_t *)args->structureInput, 168, tw, th);
				if (texInfo(o).pf == 0)
					texInfo(o).pf = sSurfaces[sid168].format == 0x52476841 ? 115 : 80;
				texInfo(o).bpr = 0;
				texInfo(o).alias = 0;
			}
			md = sSurfaces[sid168].md;
			md->retain();
			len = sSurfaces[sid168].allocSize;
			onSurface = true;
		} else if (len == 0 && args->structureInputSize == 168) {
			uint16_t tw = 0, th = 0;
			uint32_t objid = 0;

			memcpy(&tw, (const uint8_t *)args->structureInput + 0x08, 2);
			memcpy(&th, (const uint8_t *)args->structureInput + 0x0a, 2);
			memcpy(&objid, (const uint8_t *)args->structureInput + 0x5c, 4);
			len = ((uint64_t)tw * th * 8 + 0x3fff) & ~0x3fffULL;
			uint32_t o = idSlotNew(kIdObj, objid);
			if (o != 0) {
				texInfo(o).w = tw;
				texInfo(o).h = th;
				texInfo(o).surf = 0;
				// ids are reused: a pitch, alias or format left from the previous texture would stick
				texInfo(o).bpr = 0;
				texInfo(o).alias = 0;
				texInfo(o).pf = pdDescFormat((const uint8_t *)args->structureInput, 168, tw, th);
			}
		}
		// A 160 byte request is a texture on an IOSurface: the surface id at +0x38, the object id at
		// +0x5c, width and height at +0x80, and no length. The resource is the surface's own memory
		if (args->structureInputSize == 160) {
			uint32_t sid = 0, objid = 0;

			memcpy(&sid, (const uint8_t *)args->structureInput + 0x38, 4);
			memcpy(&objid, (const uint8_t *)args->structureInput + 0x5c, 4);
			// Size, format and surface even when the surface's memory is not known yet: a draw
			// that binds the texture then reads the surface directly
			uint32_t o = idSlotNew(kIdObj, objid);
			if (o != 0) {
				memcpy(&texInfo(o).w, (const uint8_t *)args->structureInput + 0x80, 4);
				memcpy(&texInfo(o).h, (const uint8_t *)args->structureInput + 0x84, 4);
				texInfo(o).pf = pdDescFormat((const uint8_t *)args->structureInput, 160, texInfo(o).w, texInfo(o).h);
				if (texInfo(o).pf == 0)
					memcpy(&texInfo(o).pf, (const uint8_t *)args->structureInput + 0x7e, 2);
				texInfo(o).surf = sid < PD_SURF_MAX ? (uint16_t)sid : 0;
				texInfo(o).bpr = 0;
				texInfo(o).alias = 0;
			}
			if (sid < PD_SURF_MAX && sSurfaces[sid].md != NULL) {
				md = sSurfaces[sid].md;
				md->retain();
				len = sSurfaces[sid].allocSize;
				onSurface = true;
			}
		}
		if (!onSurface) {
			md = (len == 0 || len > 0x10000000) ? NULL :
			    IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
			    kIODirectionInOut | kIOMemoryKernelUserShared, round_page(len), page_size);
		}
		map = md ? md->createMappingInTask(fTask, 0, kIOMapAnywhere) : NULL;
		if (map != NULL) {
			uint64_t va = map->getAddress();
			uint8_t *out = (uint8_t *)args->structureOutput;

			if (!onSurface) {
				bzero(md->getBytesNoCopy(), md->getLength());
			}
			// The layout the plugin reads: +0x00 gpuAddress, +0x08 virtualAddress, +0x10 clientSharedRO
			// pointer, +0x24 resourceID, +0x28 size in 16 KB pages (again at +0x50), +0x30 trace id, +0x38 unique id
			{
				static uint64_t sResN;
				uint64_t gpu, id, meta = 0, rlen = (len + 0x3fff) & ~0x3fffULL, trace, uniq;
				IOBufferMemoryDescriptor *rmd = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
				    kIODirectionInOut | kIOMemoryKernelUserShared, page_size, page_size);
				IOMemoryMap *rmap = rmd ? rmd->createMappingInTask(fTask, 0, kIOMapAnywhere) : NULL;

				sResN++;
				id = ++fResCount;
				{
					char rkey[16];

					if (fResources == NULL) {
						fResources = OSDictionary::withCapacity(32);
					}
					snprintf(rkey, sizeof(rkey), "%llu", (unsigned long long)id);
					if (fResources != NULL) {
						fResources->setObject(rkey, md);
					}
				}
				if (recordFor((uint32_t)id) != NULL) {
					uint32_t *rw = (uint32_t *)recordFor((uint32_t)id);

					for (uint32_t i = 0; i < 0xc0 / 4; i++) {
						rw[i] = (i % 16) == 0 ? 0 : 0xff;
					}
					meta = fRegionBVA + id * 0xc0;
				} else if (rmap != NULL) {
					// A 26.6.2 host's record: three 0x40 slots of {u32 used, u32 (stamp << 8) |
					// queue, ...}, every word but used starting at 0xff ("no queue")
					uint32_t *rw = (uint32_t *)rmd->getBytesNoCopy();
					char rk[16];

					for (uint32_t i = 0; i < rmd->getLength() / 4; i++) {
						rw[i] = (i % 16) == 0 ? 0 : 0xff;
					}
					meta = rmap->getAddress();
					if (fRecords == NULL) {
						fRecords = OSDictionary::withCapacity(32);
					}
					snprintf(rk, sizeof(rk), "%llu", (unsigned long long)id);
					if (fRecords != NULL) {
						fRecords->setObject(rk, rmd);
					}
					// the record lives as long as its resource: trap 1 unmaps it
					snprintf(rk, sizeof(rk), "r%llu", (unsigned long long)id);
					if (fResMaps == NULL) {
						fResMaps = OSDictionary::withCapacity(64);
					}
					if (fResMaps != NULL) {
						fResMaps->setObject(rk, rmap);
					}
					rmap->release();
				}
				OSSafeReleaseNULL(rmd);
				// a low gpu address and distinct trace and unique ids, +0x18 and +0x40 left zero
				gpu = 0x110000ULL + (sResN << 18);
				trace = 0x1a04cb00ULL + sResN;
				uniq = 0x4500 + 2 * sResN;
				bzero(out, args->structureOutputSize);
				memcpy(out + 0x00, &gpu, 8);
				memcpy(out + 0x08, &va, 8);
				memcpy(out + 0x10, &meta, 8);
				memcpy(out + 0x24, &id, 4);
				memcpy(out + 0x28, &rlen, 8);
				memcpy(out + 0x30, &trace, 8);
				memcpy(out + 0x38, &uniq, 8);
				memcpy(out + 0x50, &rlen, 8);
				// A private resource (kind +0x14 bit 0x2000) has no CPU address on a real host, nor
				// does one on the client's own memory (+0x40, heaps). Private textures get neither address
				if (args->structureInputSize >= 0x50) {
					uint32_t kind = 0;
					uint64_t uva = 0, zero = 0;

					memcpy(&kind, (const uint8_t *)args->structureInput + 0x14, 4);
					memcpy(&uva, (const uint8_t *)args->structureInput + 0x40, 8);
					if ((kind & 0x2000) || uva >= 0x100000000ULL) {
						memcpy(out + 0x08, &zero, 8);
					}
					if (args->structureInputSize == 168) {
						memcpy(out + 0x00, &zero, 8);
						memcpy(out + 0x08, &zero, 8);
					}
				}
				// The host's allocator word at the start of region B moves on by each size
				if (fRegionB != NULL) {
					*(volatile uint32_t *)fRegionB->getBytesNoCopy() += (uint32_t)rlen;
				}
			}
			// A resource's mapping lives as long as the resource: trap 1 or a reused id unmaps it.
			// Encoder storage names no object and stays for the connection
			{
				uint32_t mapId = 0;

				if (args->structureInputSize >= 0x60) {
					memcpy(&mapId, (const uint8_t *)args->structureInput + 0x5c, 4);
				}
				if (mapId != 0) {
					char mkey[16];

					if (fResMaps == NULL) {
						fResMaps = OSDictionary::withCapacity(64);
					}
					snprintf(mkey, sizeof(mkey), "%u", mapId);
					if (fResMaps != NULL) {
						fResMaps->setObject(mkey, map);
					}
				} else {
					if (fMaps == NULL) {
						fMaps = OSArray::withCapacity(16);
					}
					if (fMaps != NULL) {
						fMaps->setObject(map);
					}
				}
			}
			// The request names the object: id at +0x5c for a buffer, none for encoder storage
			if (args->structureInputSize >= 0x60) {
				uint32_t objid = 0;
				char key[16];

				memcpy(&objid, (const uint8_t *)args->structureInput + 0x5c, sizeof(objid));
				uint32_t o = idSlotNew(kIdObj, objid);
				// trap 1 releases by the resourceID the reply gave (+0x24), not by this object id
				{
					uint32_t rid = 0;

					if (args->structureOutput != NULL && args->structureOutputSize >= 0x28) {
						memcpy(&rid, (const uint8_t *)args->structureOutput + 0x24, sizeof(rid));
					}
					if (o != 0 && rid != 0) {
						char rkey[16];
						OSNumber *num = OSNumber::withNumber(objid, 32);

						texInfo(o).res = rid;
						if (fResObj == NULL) {
							fResObj = OSDictionary::withCapacity(64);
						}
						snprintf(rkey, sizeof(rkey), "%u", rid);
						if (fResObj != NULL && num != NULL) {
							fResObj->setObject(rkey, num);
						}
						OSSafeReleaseNULL(num);
					}
				}
				// a 168 byte private texture carries its size at +0x08 and was set above: +0xa0 is not it
				if (o != 0 && args->structureInputSize >= 0xa8 && args->structureInputSize != 168) {
					memcpy(&texInfo(o).w, (const uint8_t *)args->structureInput + 0xa0, 4);
					memcpy(&texInfo(o).h, (const uint8_t *)args->structureInput + 0xa4, 4);
					texInfo(o).pf = 0;
					texInfo(o).surf = 0;
					texInfo(o).bpr = 0;
					texInfo(o).alias = 0;
					// +0xb8 in the 224 byte form, +0x1b4 in the 476 byte one with mip levels
					texInfo(o).pf = pdDescFormat((const uint8_t *)args->structureInput, (uint32_t)args->structureInputSize,
					    texInfo(o).w, texInfo(o).h);
				} else if (o != 0 && args->structureInputSize != 160 &&
				    args->structureInputSize != 168) {
					// a plain buffer reusing a texture's id drops the texture's size and alias
					// (160 and 168 byte requests are surface and private textures, set above)
					texInfo(o).w = 0;
					texInfo(o).h = 0;
					texInfo(o).pf = 0;
					texInfo(o).surf = 0;
					texInfo(o).alias = 0;
				}
				if (objid != 0) {
					if (fBuffers == NULL) {
						fBuffers = OSDictionary::withCapacity(16);
					}
					snprintf(key, sizeof(key), "%u", objid);
					if (fBuffers != NULL) {
						fBuffers->setObject(key, md);
					}
					// No-copy buffer: the request names the client's own memory at +0x40, length +0x48
					if (fUserBuffers != NULL) {
						IOMemoryDescriptor *old = OSDynamicCast(IOMemoryDescriptor, fUserBuffers->getObject(key));

						if (old != NULL) {
							old->complete();
							fUserBuffers->removeObject(key);
						}
					}
					if (args->structureInputSize >= 0x50) {
						uint64_t uva = 0, ulen = 0;

						memcpy(&uva, (const uint8_t *)args->structureInput + 0x40, 8);
						memcpy(&ulen, (const uint8_t *)args->structureInput + 0x48, 8);
						if (uva >= 0x100000000ULL && uva < 0x800000000000ULL && ulen != 0 && ulen <= md->getLength()) {
							IOMemoryDescriptor *u = IOMemoryDescriptor::withAddressRange(uva, ulen,
							    kIODirectionInOut, fTask);

							if (u != NULL && u->prepare() == kIOReturnSuccess) {
								if (fUserBuffers == NULL) {
									fUserBuffers = OSDictionary::withCapacity(16);
								}
								if (fUserBuffers != NULL) {
									fUserBuffers->setObject(key, u);
								} else {
									u->complete();
								}
							}
							OSSafeReleaseNULL(u);
						}
					}
				}
			}
			// Encoder storage is the large allocation that names no object. A buffer or texture
			// with an id kept here forever, and a Language Chooser run leaked gigabytes of them
			uint32_t storageId = 0;

			if (args->structureInputSize >= 0x60) {
				memcpy(&storageId, (const uint8_t *)args->structureInput + 0x5c, sizeof(storageId));
			}
			if (len >= 0x20000 && storageId == 0) {
				if (fShmems == NULL) {
					fShmems = OSArray::withCapacity(8);
				}
				if (fShmems != NULL) {
					fShmems->setObject(md);
				}
				// The executor walks fEncoders for command streams
				if (fEncoders == NULL) {
					fEncoders = OSArray::withCapacity(8);
				}
				if (fEncoders != NULL) {
					fEncoders->setObject(md);
				}
			}
			map->release();
		}
		OSSafeReleaseNULL(md);
	}
	// sel 6 is IOGPU's getNextGIDGroup: 16 bytes out, and it asserts gid_group_min != 0.
	// Hand out ranges of 256 ids as {u64 min, u64 max}
	if (selector == 6 && args->structureOutput != NULL && args->structureOutputSize == 16 &&
	    strcmp(fTag, "AppleParavirtGPU") == 0) {
		static uint64_t sNextGroup;
		uint64_t lo = sNextGroup * 256 + 1, hi = (sNextGroup + 1) * 256;
		uint8_t *out = (uint8_t *)args->structureOutput;

		sNextGroup++;
		bzero(out, 16);
		memcpy(out, &lo, 8);
		memcpy(out + 8, &hi, 8);
	}
	// sel 14 is IOGPUMetalDeviceShmem init. Scalars (size, type), out {address, size u32, id u32},
	// and the address must not be NULL
	if (selector == 14 && args->scalarInputCount >= 2 && args->structureOutput != NULL &&
	    args->structureOutputSize == 16 && strcmp(fTag, "AppleParavirtGPU") == 0) {
		static uint32_t sNextShmem;
		uint32_t size = (uint32_t)args->scalarInput[0], id = ++sNextShmem;
		uint8_t *out = (uint8_t *)args->structureOutput;
		IOBufferMemoryDescriptor *md = (size == 0 || size > 0x4000000) ? NULL :
		    IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
		    kIODirectionInOut | kIOMemoryKernelUserShared, round_page(size), page_size);
		IOMemoryMap *map = md ? md->createMappingInTask(fTask, 0, kIOMapAnywhere) : NULL;

		if (map != NULL) {
			uint64_t va = map->getAddress();

			bzero(md->getBytesNoCopy(), md->getLength());
			bzero(out, 16);
			memcpy(out, &va, 8);
			memcpy(out + 8, &size, 4);
			memcpy(out + 12, &id, 4);
			if (fMaps == NULL) {
				fMaps = OSArray::withCapacity(16);
			}
			if (fShmems == NULL) {
				fShmems = OSArray::withCapacity(8);
			}
			if (fMaps != NULL) {
				fMaps->setObject(map);
			}
			if (fShmems != NULL) {
				fShmems->setObject(md);
			}
			if (id < kShmemIds) {
				OSSafeReleaseNULL(fShmemById[id]);
				fShmemById[id] = md;
				md->retain();
			}
			// The command buffer header (type 0) and its sideband list of event signals (type 1)
			if (args->scalarInput[1] < 2) {
				OSSafeReleaseNULL(fSideband[args->scalarInput[1]]);
				fSideband[args->scalarInput[1]] = md;
				md->retain();
				// A type 1 completes the pair its type 0 opened just before
				if (args->scalarInput[1] == 0 && fSbCount < kSbPairs) {
					fSbPair[fSbCount++][0] = md;
					md->retain();
				} else if (args->scalarInput[1] == 1 && fSbCount > 0 && fSbPair[fSbCount - 1][1] == NULL) {
					fSbPair[fSbCount - 1][1] = md;
					md->retain();
				}
			}
			map->release();
		}
		OSSafeReleaseNULL(md);
	}
	// sel 260 registers a shader function: {u32 id, u32 1, u64 offset, u64 size, ...}.
	// The bytes sit back to back in a 1 MB resource the plugin allocated first
	if (selector == 260 && args->structureInput != NULL && args->structureInputSize >= 0x18 &&
	    strcmp(fTag, "AppleParavirtGPU") == 0) {
		uint32_t id;
		uint64_t foff, fsize;

		memcpy(&id, args->structureInput, 4);
		memcpy(&foff, (const uint8_t *)args->structureInput + 8, 8);
		memcpy(&fsize, (const uint8_t *)args->structureInput + 16, 8);
		// ids are reused: a function that fails to register must not run the id's previous one
		uint32_t fs = idSlotNew(kIdFn, id);
		if (fs != 0)
			fFnMap.at(fs) = 0;
		// Word 1 names the storage resource: a client with several 1 MB resources
		// (WindowServer) got zeros from the wrong one when every candidate was scanned
		{
			uint32_t sid = 0;
			char key[16];
			IOBufferMemoryDescriptor *named;

			memcpy(&sid, (const uint8_t *)args->structureInput + 4, 4);
			snprintf(key, sizeof(key), "%u", sid);
			named = fResources ? OSDynamicCast(IOBufferMemoryDescriptor, fResources->getObject(key)) : NULL;
			if (named != NULL && foff <= named->getLength() && fsize <= named->getLength() - foff &&
			    fs != 0 && fsize <= PD_MAX_SHADER_BYTES) {
				fFnMap.at(fs) = pdShaderRegister((const uint8_t *)named->getBytesNoCopy() + foff, (uint32_t)fsize);
			}
		}
		for (uint32_t e = 0; fEncoders != NULL && e < fEncoders->getCount(); e++) {
			IOBufferMemoryDescriptor *md = (IOBufferMemoryDescriptor *)fEncoders->getObject(e);

			if (md->getLength() < 0x100000 || foff > md->getLength() || fsize > md->getLength() - foff) {
				continue;
			}
			const uint8_t *b = (const uint8_t *)md->getBytesNoCopy() + foff;

			// Only a Metal library: the named resource can be missing and this storage hold vertices
			if (fs != 0 && fsize <= PD_MAX_SHADER_BYTES && shader(id) == NULL &&
			    fsize >= 4 && memcmp(b, "MTLB", 4) == 0)
				fFnMap.at(fs) = pdShaderRegister(b, (uint32_t)fsize);
			break;
		}
	}
	// sel 261 makes a texture on existing memory: {?, new id, kind, bytes after this word, new id,
	// +0x14 source, ...}. Kind 0x1b is a view, 9 and 0x37 linear textures on a buffer, 0x38 a heap texture
	if (selector == 261 && strcmp(fTag, "AppleParavirtGPU") == 0 && args->structureInput != NULL &&
	    args->structureInputSize >= 0x1c) {
		const uint8_t *in = (const uint8_t *)args->structureInput;
		uint32_t kind, nid, src;

		memcpy(&kind, in + 8, 4);
		memcpy(&nid, in + 4, 4);
		memcpy(&src, in + 0x14, 4);
		uint32_t no = idSlotNew(kIdObj, nid), so = texSlot(src);

		if (no != 0) {
			if (kind == 0x1b) {
				// a view: +0x18 u16 pixel format
				texInfo(no).w = texInfo(so).w;
				texInfo(no).h = texInfo(so).h;
				memcpy(&texInfo(no).pf, in + 0x18, 2);
				texInfo(no).surf = texInfo(so).surf;
				texInfo(no).alias = src;
				texInfo(no).bpr = texInfo(so).bpr;
				texInfo(no).off = texInfo(so).alias != 0 ? texInfo(so).off : 0;
			} else if (kind == 9 && args->structureInputSize >= 0x34) {
				// WindowServer's glyph and gradient textures on buffers (72 bytes): +0x20 bytes
				// per row, +0x2a u8 pixel format (A8Unorm glyphs), +0x2c width, +0x30 height
				memcpy(&texInfo(no).bpr, in + 0x20, 4);
				texInfo(no).pf = in[0x2a];
				memcpy(&texInfo(no).w, in + 0x2c, 4);
				memcpy(&texInfo(no).h, in + 0x30, 4);
				texInfo(no).surf = 0;
				texInfo(no).alias = src;
			} else if (kind == 0x38 && args->structureInputSize >= 0x4c) {
				// a heap texture (76 bytes): +0x14 heap, +0x18 the 0x1d5 query's descriptor (u16 format
				// +0x19, u32 width +0x1f, height +0x23), +0x44 u64 offset into the heap
				uint64_t hoff = 0;
				uint16_t pf = 0;

				memcpy(&pf, in + 0x19, 2);
				memcpy(&texInfo(no).w, in + 0x1f, 4);
				memcpy(&texInfo(no).h, in + 0x23, 4);
				memcpy(&hoff, in + 0x44, 8);
				texInfo(no).pf = pf;
				texInfo(no).surf = 0;
				texInfo(no).alias = src;
				texInfo(no).off = hoff < 0x40000000ULL ? (uint32_t)hoff : 0;
				texInfo(no).bpr = texInfo(no).w * pdMtlBpp(pf);
			} else if (kind == 0x37 && args->structureInputSize >= 0x37) {
				// +0x20 bytes per row, +0x29 u8 pixel format, +0x2f width, +0x33 height
				memcpy(&texInfo(no).bpr, in + 0x20, 4);
				texInfo(no).pf = in[0x29];
				memcpy(&texInfo(no).w, in + 0x2f, 4);
				memcpy(&texInfo(no).h, in + 0x33, 4);
				texInfo(no).surf = 0;
				texInfo(no).alias = src;
			}
			// newTextureWithDescriptor:offset:bytesPerRow: keeps the offset before the pitch. A glyph
			// run's texture starts 0x100 into its buffer: read from 0 its tail wrapped to the next row
			if (kind == 9 || kind == 0x37) {
				uint64_t boff = 0;

				memcpy(&boff, in + 0x18, 8);
				texInfo(no).off = boff < 0x40000000ULL ? (uint32_t)boff : 0;
			}
			// a texture past the end of the memory it was made on: its request, to find the field misread
			{
				static uint32_t sOverLogs;
				uint64_t moff = 0;
				IOBufferMemoryDescriptor *mmd = texMemory(nid, &moff);
				uint32_t bpr = texInfo(no).bpr ? texInfo(no).bpr : texInfo(no).w * pdMtlBpp(texInfo(no).pf);

				if (mmd != NULL && moff + (uint64_t)bpr * texInfo(no).h > mmd->getLength() && sOverLogs < 32) {
					uint32_t w[20] = { 0 };

					sOverLogs++;
					memcpy(w, in, args->structureInputSize < sizeof(w) ? args->structureInputSize : sizeof(w));
					IOLog("PDIOSurface: sel 261 kind 0x%x texture %u (%ux%u pf %u bpr %u) on %u at 0x%llx past 0x%llx (%u bytes): "
					    "%08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
					    kind, nid, texInfo(no).w, texInfo(no).h, texInfo(no).pf, bpr, src, (unsigned long long)moff,
					    (unsigned long long)mmd->getLength(), (uint32_t)args->structureInputSize, w[0], w[1], w[2], w[3],
					    w[4], w[5], w[6], w[7], w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15], w[16], w[17], w[18],
					    w[19]);
				}
			}
		}
		return kIOReturnSuccess;
	}
	// sel 266 creates a compute pipeline: shader-storage id at +0x10, new pipeline id at +0x14
	if (selector == 266 && args->structureInput != NULL && args->structureInputSize >= 0x30 &&
	    strcmp(fTag, "AppleParavirtGPU") == 0) {
		uint32_t pipeline = 0, function = 0;

		memcpy(&pipeline, (const uint8_t *)args->structureInput + 0x14, 4);
		memcpy(&function, (const uint8_t *)args->structureInput + 0x10, 4);
		uint32_t ps = idSlotNew(kIdPipe, pipeline);

		if (ps != 0) {
			pipeInfo(ps).fn = function;
			// Smoke path from before the Apple encoder ran: the daemon runs the kernel over
			// buffer 1 at +2048 when the pipeline is registered
			if (sBridge != NULL && shader(function) != NULL &&
			    fBuffers != NULL) {
				PDBridgeGuard bridgeGuard;
				IOBufferMemoryDescriptor *buf = OSDynamicCast(IOBufferMemoryDescriptor,
				    fBuffers->getObject("1"));
				PDBridgeHeader *h = (PDBridgeHeader *)sBridge->getBytesNoCopy();
				uint32_t msize = (uint32_t)shader(function)->getLength();
				uint32_t dataoff = (0x100 + msize + 15) & ~15U;
				uint32_t dsize = 64 * sizeof(uint32_t);

				if (buf != NULL && buf->getLength() >= 2048 + dsize &&
				    h->status == 0x44525652 && h->state == 0 &&
				    dataoff <= PD_BRIDGE_BYTES && dsize <= PD_BRIDGE_BYTES - dataoff) {
					uint8_t *base = (uint8_t *)h;
					h->serial = ++sBridgeSerial; h->functionID = shaderKey(function); h->pipelineID = pipeline; h->kind = 0;
					h->metallibSize = msize; h->dataSize = dsize; h->dataOffset = dataoff;
					h->bufferID = 1; h->bufferOffset = 2048;
					h->grid[0] = 64; h->grid[1] = 1; h->grid[2] = 1;
					h->threadsPerGroup[0] = 8; h->threadsPerGroup[1] = 1; h->threadsPerGroup[2] = 1;
					memcpy(base + 0x100, shader(function)->getBytesNoCopy(), msize);
					memcpy(base + dataoff, (uint8_t *)buf->getBytesNoCopy() + 2048, dsize);
					pdBridgeRing(h);
					pdBridgeWait(h, 120000);
					__sync_synchronize();
					if (h->state == 2) {
						memcpy((uint8_t *)buf->getBytesNoCopy() + 2048, base + dataoff, dsize);
					} else {
						IOLog("PDIOSurface: pipeline smoke serial %u failed, state %u: %s\n",
						    h->serial, h->state, h->error);
					}
					pdBridgeRelease(h);
				}
			}
		}
	}
	// sel 28 binds a command queue to the notification queue that gets its completions
	if (selector == 28 && args->scalarInputCount >= 2 && strcmp(fTag, "AppleParavirtGPU") == 0) {
		for (uint32_t i = 0; i < 16; i++) {
			if (fBoundQueue[i] == 0 || fBoundQueue[i] == args->scalarInput[0]) {
				fBoundQueue[i] = args->scalarInput[0];
				fBoundNotify[i] = args->scalarInput[1];
				break;
			}
		}
	}
	// sel 17 destroys a notification queue
	if (selector == 17 && args->scalarInputCount >= 1 && fQueues != NULL) {
		char qkey[24];

		snprintf(qkey, sizeof(qkey), "%llu", (unsigned long long)args->scalarInput[0]);
		fQueues->removeObject(qkey);
	}
	for (uint32_t i = 0; i < args->scalarOutputCount; i++) {
		args->scalarOutput[i] = 0;
	}
	// WindowServer asks the framebuffer for its size in two scalars before enabling it
	if (selector == 8 && args->scalarOutputCount == 2 && strcmp(fTag, "IOMobileFramebuffer") == 0) {
		uint32_t w, h;
		pdDisplayGeometry(&w, &h);
		args->scalarOutput[0] = w;
		args->scalarOutput[1] = h;
	}
	// sel 83 {layer} -> port name of the layer's default surface: display sized RGhA, 8 bytes a
	// pixel (a 26.6.2 VM hands back one that WindowServer then opens with IOSurfaceRoot sel 34)
	if (selector == 83 && args->scalarInputCount >= 1 && args->scalarOutputCount >= 1 &&
	    args->scalarInput[0] == 0 && strcmp(fTag, "IOMobileFramebuffer") == 0) {
		static uint32_t sDefaultSurface;
		uint32_t w, h;
		pdDisplayGeometry(&w, &h);
		mach_port_name_t name = 0;
		IOReturn kr = kIOReturnSuccess;

		if (sDefaultSurface == 0) {
			char plist[512];
			uint32_t id = 0;

			snprintf(plist, sizeof(plist), "<dict><key>IOSurfaceWidth</key><integer>%u</integer>"
			    "<key>IOSurfaceHeight</key><integer>%u</integer><key>IOSurfaceBytesPerElement</key><integer>8</integer>"
			    "<key>IOSurfaceBytesPerRow</key><integer>%u</integer><key>IOSurfacePixelFormat</key><integer>%u</integer>"
			    "<key>IOSurfaceName</key><string>IOMFB default surface</string></dict>", w, h, w * 8, 0x52476841u);
			kr = pdSurfCreate((const uint8_t *)plist, (uint32_t)strlen(plist) + 1, &id);
			if (kr == kIOReturnSuccess) {
				sDefaultSurface = id;
			}
		}
		if (sDefaultSurface != 0) {
			OSNumber *num = OSNumber::withNumber(sDefaultSurface, 32);

			kr = kIOReturnNoMemory;
			if (num != NULL) {
				kr = IOUserClient::copyPortNameForObjectInTask(fTask, num, &name);
				num->release();
			}
		}
		args->scalarOutput[0] = name;
		return kr;
	}
	// sel 257 describes a resource and wants one scalar back. Zero reads as a failed allocation,
	// so answer an address-like handle
	if (selector == 257 && args->scalarOutputCount == 1) {
		static uint64_t sNextResource;

		sNextResource++;
		args->scalarOutput[0] = 0x100000000ULL + (sNextResource << 24);
	}
	// sel 72 registers the frame callback for a notification port type
	if (selector == 72 && args->scalarInputCount >= 3 && strcmp(fTag, "IOMobileFramebuffer") == 0 &&
	    args->scalarInput[2] < 8) {
		uint64_t deadline;

		// scalar 0 is the callout the frame notification carries, scalar 1 its refcon
		sFBCallback[args->scalarInput[2]] = (mach_vm_address_t)args->scalarInput[0];
		sFBRefcon[args->scalarInput[2]] = args->scalarInput[1];
		sFBTask = fTask;
		sFBOwner = this;
		// WindowServer registers ports 0 and 1 only, the vblank that paces swaps starts either way
		if (sFBVsync == NULL) {
			sFBVsync = thread_call_allocate(&vsyncFired, NULL);
		}
		if (sFBVsync != NULL) {
			clock_interval_to_deadline(1000000 / kPDVsyncHz, kMicrosecondScale, &deadline);
			thread_call_enter_delayed(sFBVsync, deadline);
		}
	}
	// enqueue presentation behind the work that produces this frame. Polling
	// observes completion only, it must never rescan a reused surface.
	if (selector == 5 && strcmp(fTag, "IOMobileFramebuffer") == 0 && args->structureInput != NULL) {
		return pdQueueSwap((const uint8_t *)args->structureInput, args->structureInputSize, sSwapID)
		    ? kIOReturnSuccess : kIOReturnNoMemory;
	}
	// poll only the last completed present, an unfinished frame is always busy.
	if (selector == 6 && strcmp(fTag, "IOMobileFramebuffer") == 0) {
		return args->scalarInputCount > 0 && args->scalarInput[0] <= sPresentedSwap
		    ? kIOReturnSuccess : kIOReturnBusy;
	}
	// sel 78 sets the ICC curve and matrix (656 and 64 byte payloads). A real
	// AppleParavirtDisplay answers unsupported and CoreAnimation carries on
	if (selector == 78 && strcmp(fTag, "IOMobileFramebuffer") == 0) {
		return kIOReturnUnsupported;
	}
	// The one-scalar queries and sel 29 answer as the real 26.6.2 display does: 25 -> 1, 28 -> 0x90,
	// 57 and 82 -> 0, 29 -> floats 13.18, 21.0. sel 27 reads back the gamma table sel 17 writes
	if (strcmp(fTag, "IOMobileFramebuffer") == 0) {
		if (args->scalarOutputCount >= 1 && (selector == 25 || selector == 28 ||
		    selector == 57 || selector == 82)) {
			args->scalarOutput[0] = selector == 25 ? 1 : selector == 28 ? 0x90 : 0;
		}
		if (selector == 29 && args->structureOutput != NULL && args->structureOutputSize == 8) {
			((uint32_t *)args->structureOutput)[0] = 0x4152e38e;
			((uint32_t *)args->structureOutput)[1] = 0x41a80000;
		}
		if (selector == 17 && args->structureInput != NULL && args->structureInputSize == 3084) {
			OSSafeReleaseNULL(sGammaTable);
			sGammaTable = OSData::withBytes(args->structureInput, 3084);
		}
		if (selector == 27 && args->structureOutput != NULL && args->structureOutputSize == 3084) {
			if (sGammaTable != NULL) {
				memcpy(args->structureOutput, sGammaTable->getBytesNoCopy(), 3084);
			} else {
				uint32_t *t = (uint32_t *)args->structureOutput;

				for (uint32_t i = 0; i < 3 * 257; i++) {
					t[i] = (i % 257) * 4;
				}
			}
		}
	}
	// sel 4 opens a swap and expects an id back. 5 describes it, 6 closes it
	if (selector == 4 && args->scalarOutputCount == 1 && strcmp(fTag, "IOMobileFramebuffer") == 0) {
		args->scalarOutput[0] = ++sSwapID;
	}
	if (shmem_va != 0 && args->structureOutput != NULL && args->structureOutputSize >= 24) {
		uint64_t fields[3] = { shmem_va, shmem_len, fMaps ? fMaps->getCount() : 1 };

		memcpy(args->structureOutput, fields, sizeof(fields));
	}
	return kIOReturnSuccess;
}
