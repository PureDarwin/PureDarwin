#pragma once
// IOSurfaceRoot for WindowServer, which otherwise spins on an IONameMatch lookup for it.
// The client interface is learned by observation, unknown external methods succeed

// Client object, function and pipeline ids keep growing over a session
#define PD_MAX_OBJ 1024



#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/graphics/IOAccelerator.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOSharedDataQueue.h>
#include <pexpert/pexpert.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <kern/clock.h>
#include <libkern/c++/OSSerialize.h>
#include <libkern/OSSerializeBinary.h>

uint32_t pdSurfFormat(uint32_t sid);
uint64_t pdSurfBpr(uint32_t sid);

class IOSurfaceRoot : public IOService
{
	OSDeclareDefaultStructors(IOSurfaceRoot);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

// WindowServer registers for IOAccelerator publication right before it aborts, so publish one
class PDAccelerator : public IOAccelerator
{
	OSDeclareDefaultStructors(PDAccelerator);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

// Apple's paravirtual GPU is one driver on an arm-io node named "paravirtualizedgraphics,gpu".
// Its display objects hang off it as children
class AppleARMIODevice : public IOService
{
	OSDeclareDefaultStructors(AppleARMIODevice);

public:
	static void publishGPU(IOService *owner);
	virtual bool compareName(OSString *name, OSString **matched = NULL) const override;
};

class AppleParavirtGPU : public PDAccelerator
{
	OSDeclareDefaultStructors(AppleParavirtGPU);
};

// PureDarwin-owned transport for the userspace shader executor.
// Keeping this separate means the daemon never joins the population of Metal GPU clients
class PDShaderBridge : public IOService
{
	OSDeclareDefaultStructors(PDShaderBridge);

public:
	static void publish(IOService *owner);
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

// The display node WindowServer finds under IODeviceTree:/arm-io by device_type display.
// Whatever opens the display comes through here, so it answers newUserClient
class PDDisplayNub : public IOService
{
	OSDeclareDefaultStructors(PDDisplayNub);

public:
	static void publish(IOService *owner);
	static void publishOne(IOService *owner, const char *name, const char *compatible,
	    const char *displayType);

	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

// The paravirtual display shape. Apple's own VM guests present these classes in place of
// a panel with an EDID, next to a display manager and a DisplayPort transport
class AppleParavirtDisplay : public IOService
{
	OSDeclareDefaultStructors(AppleParavirtDisplay);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

class AppleParavirtFramebuffer : public IOService
{
	OSDeclareDefaultStructors(AppleParavirtFramebuffer);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

class DCPAVCECInterfaceProxy : public IOService
{
	OSDeclareDefaultStructors(DCPAVCECInterfaceProxy);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

class AppleDisplayManager : public IOService
{
	OSDeclareDefaultStructors(AppleDisplayManager);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

// Apple's VMs publish this from AppleVPBootPolicy. mobileactivationd traps when the lookup
// finds nothing, and syspolicyd opens it too
class BootPolicy : public IOService
{
	OSDeclareDefaultStructors(BootPolicy);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

// Apple's VMs publish this on IOResources with no transport behind it. bluetoothd waits
// for it before serving CoreBluetooth, and Language Chooser waits on CoreBluetooth
class IOBluetoothHCIController : public IOService
{
	OSDeclareDefaultStructors(IOBluetoothHCIController);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

class IOPortTransportStateDisplayPort : public IOService
{
	OSDeclareDefaultStructors(IOPortTransportStateDisplayPort);

	thread_call_t fHotplug;
	static void hotplugFired(thread_call_param_t self, thread_call_param_t unused);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

// WindowServer looks up the display's framebuffer services by class
class IOMobileFramebuffer : public IOService
{
	OSDeclareDefaultStructors(IOMobileFramebuffer);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

class AppleCLCD : public IOService
{
	OSDeclareDefaultStructors(AppleCLCD);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};
// CoreDisplay matches the paravirt display's scaler drivers before its master service
// checks in, so publish both
#define PD_SCALER_CLASS(Name) \
class Name : public IOService \
{ \
	OSDeclareDefaultStructors(Name); \
public: \
	virtual bool start(IOService *provider) override; \
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type, \
	    IOUserClient **handler) override; \
};
PD_SCALER_CLASS(AppleM2ScalerParavirtDriver)
PD_SCALER_CLASS(AppleM2ScalerCSCDriver)
extern OSData *sShaders[256];
extern uint32_t sShaderN;

class IOSurfaceRootUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(IOSurfaceRootUserClient);

	const char *fTag;
	UInt32 fType;
	task_t fTask;
	OSArray *fMaps;                 // Shared regions mapped into the client task
	OSDictionary *fResMaps;         // sel 9 resources mapped into the client, by object id ("r<rid>": records)
	OSDictionary *fResObj;          // the object id each resourceID was created under
	uint32_t fObjRes[PD_MAX_OBJ];   // the resourceID an object id currently names
	// guards the resource dictionaries: the client creates (sel 9) and releases (trap 1) on
	// different threads, and a remove shifting entries while an insert regrew them panicked
	IORecursiveLock *fResLock;
	void resLock(void);
	void resUnlock(void);
	OSArray *fShmems;               // Device shmem regions, held for the connection
	OSDictionary *fQueues;          // IOSharedDataQueue per notification queue id
	uint64_t fSubmitBlocks[2];      // Callback blocks named by the last submission
	OSDictionary *fBuffers;         // client buffers by the plugin's object id
	OSDictionary *fResources;       // every resource by the id the sel 9 reply gave it
	OSDictionary *fRecords;         // each resource's usage record (sel 9 reply +0x10), by id
	IOBufferMemoryDescriptor *fRegionB;	// sel 5's region B: the resource record table, 0xc0 per id
	uint64_t fRegionBVA;
	uint32_t fResCount;		// resource ids, numbered from 1 per client like a real host
	uint8_t *recordFor(uint32_t id);
	uint32_t fSubmitQueue;          // command queue of the submission being executed
	IOBufferMemoryDescriptor *fSel2MD; // sel 2's region: each queue's last completed stamp << 8
	uint32_t fQueueStamp[256];      // per command queue index (queue id - 1): last stamp issued
	uint32_t fCurStamp;             // the stamp of the submission being executed
	uint32_t fNextQueueID, fNextNotifyID; // per-client command and notification queue ids
	// sel 14 shmems by id: a submission names its header and list by these
	enum { kShmemIds = 256 };
	IOBufferMemoryDescriptor *fShmemById[kShmemIds];
	uint32_t fSubList, fSubHdr;	// the pending submission's list and header ids, 0 unknown
	// pipeline ids are per client: a global table let one process run another's shaders and blending
	uint32_t fPipelineFunction[PD_MAX_OBJ];
	uint32_t fRenderVertexFn[PD_MAX_OBJ], fRenderFragmentFn[PD_MAX_OBJ];	// by sel 257 pipeline object id
	uint32_t fPipeBlend[PD_MAX_OBJ];	// colour attachment 0's packed blend state, see pdParseBlend
	uint32_t fPipeBlendRT[PD_MAX_OBJ][4];	// attachments 1-3 (RenderBox accumulates coverage and layers)
	uint32_t fSampFlags[PD_MAX_OBJ];	// sel 257 type 3: each sampler's packed filter / address / coordinate flags
	// memoryless attachments (RenderBox's coverage and layer live in tile memory): scratch per slot,
	// sized to the pass's target, cleared at pass begin
	IOBufferMemoryDescriptor *fAttScratch[4];
	IOBufferMemoryDescriptor *fSideband[2]; // sel 14 type 0 (command buffer header) and type 1
	enum { kSbPairs = 32 };
	IOBufferMemoryDescriptor *fSbPair[kSbPairs][2]; // every header/list pair: command buffers cycle through a pool
	uint32_t fSbStamp[kSbPairs], fSbCount;
	IOMemoryMap *fEventMap;         // this task's mapping of the shared event values
	uint64_t eventAddress(uint32_t index);
	void applySignals(void);
	void applySidebandPair(const uint8_t *hdr, const uint8_t *list, uint32_t len);
	// Submissions waiting for their completion entries, posted from fCompletionCall in order
enum { kPDTrapSlots = 24 };
struct PendingCompletion { uint64_t queue, block[2], start, end; };
	enum { kPendMax = 512 };
	PendingCompletion fPend[kPendMax];
	uint32_t fPendCount;
	IOLock *fPendLock;
	OSAsyncReference64 fEventRef;   // sel 40: shared event listener replies
	bool fEventRefSet;
	IOLock *fSubmitLock;            // submissions from several queue threads share fSubmit*
	thread_call_t fCompletionCall;
	static void completionFired(thread_call_param_t self, thread_call_param_t unused);
	void postCompletion(const PendingCompletion &pc);
	uint8_t fPurgeable[PD_MAX_OBJ]; // setPurgeableState per resource id, 0 = never set (2)
	void markUsed(uint32_t id, bool written);
	OSDictionary *fUserBuffers;     // client memory behind no-copy buffers, by object id
	uint32_t fShaderKey[PD_MAX_OBJ];        // this client's function id -> sShaders index + 1
	OSData *shader(uint32_t id) { return id < PD_MAX_OBJ && fShaderKey[id] ? sShaders[fShaderKey[id] - 1] : NULL; }
	uint32_t shaderKey(uint32_t id) { return id < PD_MAX_OBJ && fShaderKey[id] ? fShaderKey[id] - 1 : 0; }
	OSArray *fEncoders;             // encoder command storage
	IOMemoryMap *fSurfRegion;       // this client's mapping of the IOSurface shared region
	OSData *fSwapDesc;              // the last swap descriptor (sel 5), scanned out at sel 6
	uint32_t fExecuted[32];         // Signature of the stream last executed, per encoder storage
	uint32_t fTexW[PD_MAX_OBJ], fTexH[PD_MAX_OBJ];  // texture dimensions by object id
	uint16_t fTexPf[PD_MAX_OBJ];	// MTLPixelFormat from the texture request, 0 = not seen
	uint16_t fTexSurf[PD_MAX_OBJ];	// IOSurface behind a surface texture, 0 = none
	uint32_t fTexAlias[PD_MAX_OBJ];	// sel 261: the texture or buffer whose memory this one uses
	uint32_t fTexBpr[PD_MAX_OBJ];	// sel 261 buffer textures: their own row pitch
	uint32_t fTexOff[PD_MAX_OBJ];	// sel 261 buffer textures: byte offset into the buffer (+0x18)
	// daemon format for a texture: 0 BGRA8, 1 RGBA half, 2 A8, 3 R8, 4 RGBA8, 5 RG8, guessed
	// from the size when unknown
	uint32_t texFmt(uint32_t id, uint64_t len)
	{
		// a texture on an IOSurface has the surface's format: RGhA half, RGBA and BGRA 8 bit
		if (fTexSurf[id] != 0) {
			uint32_t pf = pdSurfFormat(fTexSurf[id]);

			if (pf == 0x52476841) {
				return 1;
			}
			if (pf == 0x52474241) {
				return 4;
			}
			if (pf == 0x42475241) {
				return 0;
			}
			// two 8-bit channels (LA08) sample as RG8
			if (pf == 0x4c413038) {
				return 5;
			}
			// otherwise the surface's pitch tells a one or two byte format from a four byte one
			uint64_t sbpr = pdSurfBpr(fTexSurf[id]);
			if (fTexW[id] != 0 && sbpr != 0 && sbpr < (uint64_t)fTexW[id] * 4) {
				return sbpr >= (uint64_t)fTexW[id] * 2 ? 5 : 3;
			}
		}
		if (fTexPf[id] != 0) {
			// MTLPixelFormat RGBA16Float 115, A8Unorm 1, R8Unorm 10, RG8Unorm 30, RGBA8Unorm 70
			switch (fTexPf[id]) {
			case 115: return 1;
			case 1: return 2;
			case 10: return 3;
			case 70: return 4;
			case 30: return 5;
			default: return 0;
			}
		}
		return len >= (uint64_t)fTexW[id] * fTexH[id] * 8 ? 1 : 0;
	}
	uint64_t fBoundQueue[16];       // sel 28 bindings: command queue id ...
	uint64_t fBoundNotify[16];      // ... to notification queue id
	UInt32 fTrapIndex;              // index of the trap being dispatched
	IOExternalTrap fTrap;
	IOExternalTrap fTraps[kPDTrapSlots]; // one per index: concurrent traps can't share fTrapIndex
	OSArray *fShared;               // buffers handed out by clientMemoryForType
	IOBufferMemoryDescriptor *fDirty; // managed buffer dirty ring handed out by trap 257
	IOMemoryMap *fDirtyMap;
	uint32_t fSharedType[16];       // The type each fShared entry answers
	uint32_t fVariant;              // Which reply shape this client gets

public:
	static IOReturn open(IOService *provider, task_t owningTask, void *securityID,
	    UInt32 type, IOUserClient **handler);

	virtual IOReturn clientClose(void) override;
	// The command stream lives in memory the client maps from us.
	// Hand out real buffers so what it writes can be read back
	virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
	    IOMemoryDescriptor **memory) override;
	// A notification queue is a ring plus a port. The plugin registers the port here
	virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type,
	    io_user_reference_t refCon) override;
	// IOConnectTrap is the other way in. Submissions may use it instead of a selector
	virtual IOExternalTrap *getTargetAndTrapForIndex(IOService **targetP, UInt32 index) override;
	IOReturn trap(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6);
	static IOReturn sendAsync(OSAsyncReference64 ref, io_user_reference_t *a, UInt32 n)
	{
		return sendAsyncResult64(ref, kIOReturnSuccess, a, n);
	}
	IOReturn trapIndexed(UInt32 trapIndex, void *p1, void *p2, void *p3, void *p4, void *p5, void *p6);
	template <UInt32 I> IOReturn trapN(void *p1, void *p2, void *p3, void *p4, void *p5, void *p6)
	{
		return trapDispatch(I < 16 ? I : I - 16 + 256, p1, p2, p3, p4, p5, p6);
	}
	IOReturn trapDispatch(UInt32 trapIndex, void *p1, void *p2, void *p3, void *p4, void *p5, void *p6);
	static IOReturn sendFrameNotification(uint32_t type, const io_user_reference_t *words, uint32_t count);
	static void completeSwap(uint64_t id);
	void executeStreams(IOBufferMemoryDescriptor **snap);
	void snapshotStreams(IOBufferMemoryDescriptor **snap);
	uint32_t sidebandOrder(uint32_t *order, uint32_t max);
	void queueCompletion(uint64_t queue, uint64_t block0, uint64_t block1, uint64_t start, uint64_t end);
	void runAsyncJob(struct PDAsyncJob *job);
	void submitAsync(uint32_t queue);
	void releaseResource(uint32_t id);
	void drainDirty(void);
	IOBufferMemoryDescriptor *lookupBuffer(const char *key);
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) override;
	IOReturn externalMethodImpl(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference);
	void runCompute(uint32_t pipeline, const uint32_t *cBuf, const uint64_t *cOff, const uint32_t *cTex,
	    const uint32_t *cTg, const uint64_t *grid, const uint64_t *tpg, bool threads);
};

// state and helpers shared by the user client and the accelerator

// Userspace shader executor bridge. The daemon maps this descriptor through
// clientMemoryForType(PD_BRIDGE_TYPE). Metal clients publish work synchronously
enum { PD_BRIDGE_TYPE = 0x50444750, PD_BRIDGE_BYTES = 0x8000000, PD_DRAW_REQ = 0x100, PD_DRAW_DATA = 0x800, PD_DRAW_BUFS = 16 };
struct PDBridgeHeader {
	volatile uint32_t state; // 0 idle, 1 request, 2 complete, 3 failed
	uint32_t serial, status, functionID, pipelineID;
	uint32_t metallibSize, dataSize, dataOffset, bufferID;
	uint64_t bufferOffset;
	uint64_t grid[3], threadsPerGroup[3];
	uint32_t kind;		// 0 compute dispatch, 1 introspect a function's interface
	char error[128];
};
// doorbells: the daemon sleeps in sel 100 until a job is queued and calls sel 101 when it
// finishes, so neither side polls. Without a ringing daemon the waits still re-check every 10 ms
enum { PD_BRIDGE_SEL_WAIT_JOB = 100, PD_BRIDGE_SEL_DONE = 101 };
enum { kPDEventTag = 0x40000000, kPDEventMax = 2048 };

// sel 39 listeners (MTLSharedEvent notifyListener:atValue:): an async reply once the event
// reaches the value
struct PDEventListener {
	uint32_t index;
	uint64_t value;
	OSAsyncReference64 ref;         // the process's sel 40 reference
	uint64_t echo[3];               // block, event, listener: sel 39 in[3], in[2], in[1]
	uint64_t since;                 // vblank count when registered
};
enum { kPDListenMax = 256 };
// Metal libraries run past 64 KB (Language Chooser's text shader is 0x17f10)
#define PD_MAX_SHADER_BYTES 0x80000U

// IOSurfaces, shared by every client. A create answers the data mapped into the client, its
// records in a per client shared region, the id, the geometry, the format and a creation count

struct PDSurface {
	IOBufferMemoryDescriptor *md;
	OSDictionary *props;		// the creation dictionary, handed back by sel 10
	OSDictionary *values;		// values set through sel 9, handed back beside it
	uint64_t width, height, bytesPerRow, bytesPerElement, allocSize, format;
};
#define PD_SURF_MAX      256
extern IOLock *sBridgeLock;
struct PDBridgeGuard {
	PDBridgeGuard()
	{
		if (sBridgeLock == NULL) {
			IOLock *l = IOLockAlloc();
			if (!OSCompareAndSwapPtr(NULL, l, (void * volatile *)&sBridgeLock)) IOLockFree(l);
		}
		IOLockLock(sBridgeLock);
	}
	~PDBridgeGuard() { IOLockUnlock(sBridgeLock); }
};
extern IOLock *sBridgeBell;
extern uint8_t sIntrospected[256];
extern IOBufferMemoryDescriptor *sBridge;
extern uint32_t sBridgeSerial;
extern IOBufferMemoryDescriptor *sEventPage;
extern uint32_t sNextEvent;
extern OSNumber *sEventObj[kPDEventMax];
extern IOLock *sEventAllocLock;
extern PDEventListener sListeners[kPDListenMax];
extern uint32_t sListenCount;
extern IOLock *sListenLock;
extern PDSurface sSurfaces[PD_SURF_MAX];

void pdBridgeRing(PDBridgeHeader *h);
void pdBridgeWait(PDBridgeHeader *h, uint32_t ms);
IOReturn pdBridgeDoorbell(uint32_t selector);
uint32_t pdParseBlend(const uint8_t *m, uint32_t len, uint32_t att = 0);
void pdEventSet(uint32_t index, uint64_t value);
void pdFireDueListeners(uint64_t frame);
void pdAsyncDrain(uint32_t ms);
